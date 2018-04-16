/*
 * stdio.c
 */

#include "lib/os/streams.h"
#include "lib/os/oscalls.h"
#include "lib/unistd.h"
#include "lib/stdlib.h"
#include "lib/errno.h"
#include "lib/stdarg.h"
#include "lib/stdio.h"
#include "lib/string.h"
#include "lib/fcntl.h"
#include "lib/sys/stat.h"
#include "lib/ctype.h"
#include "lib/limits.h"
#include "lib/stdint.h"
#include "lib/os/mathlib.h"

int __scanf_loglevel = 0;
#define SCANF_DEBUG(...) do {if (__scanf_loglevel > 0 ) { printf("DEBUG at %s@%d (%s): ", __FILE__, __LINE__, __FUNCTION__); \
        printf(__VA_ARGS__); }} while (0)

/*
 * Pre-defined streams
 */
static FILE f_stdin;
static FILE f_stdout;
static FILE f_stderr;

FILE* stdin = &f_stdin;
FILE* stdout = &f_stdout;
FILE* stderr = &f_stderr;

/*
 * These bits are used for printf flags
 */
#define __PRINTF_FLAGS_PLUS 0x1
#define __PRINTF_FLAGS_MINUS 0x2
#define __PRINTF_FLAGS_SPACE 0x4
#define __PRINTF_FLAGS_HASH 0x8
#define __PRINTF_FLAGS_ZERO 0x10
#define __PRINTF_FLAGS_CAP 0x20
#define __PRINTF_FLAGS_DYN_WIDTH 0x40
#define __PRINTF_FLAGS_DYN_PREC 0x80


#define MAX(x,y)  ((x<y) ? y : x)

/*
 * Clear the error indicator and EOF marker for a file
 */
void clearerr(FILE* stream) {
    __ctOS_stream_clearerr(stream);
}

/*
 * Close a file
 * Parameter:
 * @stream - the file to be closed
 * Return value:
 * 0 upon success
 * EOF if an error occurs
 */
int fclose(FILE* stream) {
    int res = 0;
    /*
     * Do not close stderr, stdin or stdout
     */
    if ((stream == stdin) || (stream == stdout) || (stream == stderr))
        return 0;
    /*
     * Close stream - this will still leave the file open
     */
    int rc = __ctOS_stream_close(stream);
    if (rc) {
        errno = rc;
        res = EOF;
    }
    /*
     * Close file
     */
    rc = close(stream->fd);
    if (rc) {
        res = EOF;
    }
    free(stream);
    return res;
}

/*
 * Read EOF indicator of a stream
 * Parameter:
 * @stream - the stream
 * Return value:
 * End-of-file indicator of the stream
 */
int feof(FILE* stream) {
    return __ctOS_stream_geteof(stream);
}

/*
 * Read error indicator of a stream
 * Parameter:
 * @stream - the stream
 * Return value:
 * a non-zero value if the error indicator is set, 0 otherwise
 */
int ferror(FILE* stream) {
    return __ctOS_stream_geterror(stream);
}

/*
 * Get a character from a stream
 * Parameter:
 * @stream - the stream
 * Return value:
 * the character or EOF
 */
int fgetc(FILE* stream) {
    return __ctOS_stream_getc(stream);
}

/*
 * Utility function to determine the flags which need to be passed to the OPEN system
 * call depending on the mode string used for FOPEN
 * Parameter:
 * @mode - the fopen mode string
 * Return value:
 * -1 if the mode is not valid
 * flags to be used for open otherwise
 */
static int get_flags_for_mode(const char* mode) {
    int flags = -1;
    if (0 == strcmp(mode, "r"))
        flags = O_RDONLY;
    else if (0 == strcmp(mode, "rb"))
        flags = O_RDONLY;
    else if (0 == strcmp(mode, "w"))
        flags = O_WRONLY | O_CREAT | O_TRUNC;
    else if (0 == strcmp(mode, "wb"))
        flags = O_WRONLY | O_CREAT | O_TRUNC;
    else if (0 == strcmp(mode, "a"))
        flags = O_WRONLY | O_CREAT | O_APPEND;
    else if (0 == strcmp(mode, "ab"))
        flags = O_WRONLY | O_CREAT | O_APPEND;
    else if (0 == strcmp(mode, "r+"))
        flags = O_RDWR;
    else if (0 == strcmp(mode, "rb+"))
        flags = O_RDWR;
    else if (0 == strcmp(mode, "r+b"))
        flags = O_RDWR;
    else if (0 == strcmp(mode, "w+"))
        flags = O_RDWR | O_CREAT | O_TRUNC;
    else if (0 == strcmp(mode, "wb+"))
        flags = O_RDWR | O_CREAT | O_TRUNC;
    else if (0 == strcmp(mode, "w+b"))
        flags = O_RDWR | O_CREAT | O_TRUNC;
    else if (0 == strcmp(mode, "a+"))
        flags = O_RDWR | O_CREAT | O_APPEND;
    else if (0 == strcmp(mode, "ab+"))
        flags = O_RDWR | O_CREAT | O_APPEND;
    else if (0 == strcmp(mode, "a+b"))
        flags = O_RDWR | O_CREAT | O_APPEND;
    return flags;
}

/*
 * Open a FILE
 * Parameter:
 * @filename - the name of the file to be opened
 * @mode - the access mode
 * Return value:
 * a pointer to the newly opened file or NULL if an error occurred
 */
FILE* fopen(const char* filename, const char* mode) {
    int fd;
    int rc;
    FILE* file;
    int flags = 0;
    /*
     * Determine flags to apply
     */
    flags = get_flags_for_mode(mode);
    if (-1 == flags) {
        errno = EINVAL;
        return 0;
    }
    /*
     * Try to open file. If we create the file, add
     * file mode
     */
    if (flags | O_CREAT)
        fd = open((char*) filename, flags, S_IRWXU);
    else
        fd = open((char*) filename, flags);
    if (fd < 0) {
        errno = -fd;
        return 0;
    }
    /*
     * Create and initialize stream
     */
    file = malloc(sizeof(FILE));
    if (0 == file) {
        errno = ENOMEM;
        return 0;
    }
    rc = __ctOS_stream_open(file, fd);
    if (rc) {
        errno = rc;
        return 0;
    }
    /*
     * If the file descriptor refers to a TTY, set stream mode to line buffered
     */
    if (isatty(fd)) {
        __ctOS_stream_setvbuf(file, 0, _IOLBF, 0);
    }
    return file;
}

/*
 * Open a stream with a given file descriptor
 */
FILE *fdopen(int fd, const char *mode) {
    FILE* file;
    int rc;
    int offset;
    /*
     * Determine offset and verify that fd is usable
     */
    offset = lseek(fd, 0, SEEK_CUR);
    if (-1 == offset) {
        errno = -EBADF;
        return 0;
    }
    file = malloc(sizeof(FILE));
    if (0 == file) {
        errno = ENOMEM;
        return 0;
    }
    rc = __ctOS_stream_open(file, fd);
    if (rc) {
        errno = rc;
        return 0;
    }
    /*
     * Set file position indicator to result of lseek
     */
    fseek(file, offset, SEEK_SET);
    /*
     * If the file descriptor refers to a TTY, set stream mode to line buffered
     */
    if (isatty(fd)) {
        __ctOS_stream_setvbuf(file, 0, _IOLBF, 0);
    }
    return file;
}

/*
 * Re-open a stream. When this function is called, the stream is flushed. If filename
 * is not NULL, the file descriptor currently associated with the stream will be closed and
 * a new file descriptor will be associated with the stream which represents filename, using
 * the new mode
 */
FILE *freopen(const char* filename, const char* mode, FILE* stream) {
    __ctOS_stream_flush(stream);
    int rc;
    int fd;
    int flags;
    /*
     * In any case flush stream
     */
    __ctOS_stream_flush(stream);
    /*
     * If a new filename is specified, close existing
     * file descriptor and allocate a new one
     */
    if (filename) {
        close(stream->fd);
        __ctOS_stream_seek(stream, 0);
        flags = get_flags_for_mode(mode);
        if (-1 == flags) {
            errno = EINVAL;
            return 0;
        }
        if (flags | O_CREAT)
            fd = open((char*) filename, flags, S_IRWXU);
        else
            fd = open((char*) filename, flags);
        if (fd < 0) {
            errno = -fd;
            return 0;
        }
        rc = __ctOS_stream_open(stream, fd);
        if (rc) {
            errno = rc;
            return 0;
        }
    }
    /*
     * Otherwise, we return EBADF - this implementation does not support
     * any mode changes without closing the file
     */
    else {
        errno = EBADF;
        return 0;
    }
    return stream;
}

/*
 * Flush a stream
 * Parameters:
 * @stream - the stream to be flushed (NULL -> flush all open streams)
 * Return value:
 * 0 if operation was successful
 * EOF otherwise
 */
int fflush(FILE* stream) {
    int rc;
    if (0 == stream) {
        rc = __ctOS_stream_flush_all();
    }
    else {
        rc = __ctOS_stream_flush(stream);
    }
    if (rc) {
        if (stream)
            errno = __ctOS_stream_geterror(stream);
        return EOF;
    }
    return 0;
}

/*
 * Put a character into a stream
 * Parameter:
 * @c - the character to be written
 * @stream - the file to which we write
 */
int fputc(int c, FILE* stream) {
    return __ctOS_stream_putc(stream, c);
}

/*
 * Position a stream
 */
int fseek(FILE* stream, long offset, int whence) {
    int rc;
    /*
     * Make sure that stream gets flushed if it is dirty
     */
    rc = __ctOS_stream_flush(stream);
    if (rc) {
        errno = rc;
        return -1;
    }
    /*
     * Position open file and adapt filpos attribute of stream
     */
    rc = lseek(stream->fd, offset, whence);
    if (rc < 0) {
        return -1;
    }
    /*
     * Prepare stream again
     */
    __ctOS_stream_seek(stream, rc);
    return 0;
}

int fseeko(FILE* stream, off_t offset, int whence) {
    return fseek(stream, offset, whence);
}

/*
 * Get file position for a stream. Note that calling fseek with the result of this
 * call will reposition the stream at the same point again
 */
long ftell(FILE* stream) {
    return __ctOS_stream_tell(stream);
}

/*
 * Read a string from a file until one of the following conditions occurs:
 * - n-1 bytes have been read
 * - a newline has been read
 * - an EOF condition has been encountered
 * The string shall be terminated with a 0
 * Return value:
 * A pointer to the string or NULL if the operation failed without reading at least one character
 */
char* fgets(char* s, int n, FILE* stream) {
    int bytes_read = 0;
    int rc;
    while (bytes_read < n - 1) {
        rc = fgetc(stream);
        if (EOF == rc) {
            errno = __ctOS_stream_geterror(stream);
            if (0 == bytes_read) {
                return 0;
            }
            else {
                break;
            }
        }
        s[bytes_read] = (char) rc;
        bytes_read++;
        if ('\n' == rc)
            break;
    }
    s[bytes_read] = 0;
    return s;
}

/*
 * Remove a file
 */
int remove(const char *path) {
    return unlink((char*) path);
}

/*
 * Write a null-terminated string to a stream. The trailing 0 will
 * not be written
 */
int fputs(const char* s, FILE* stream) {
    int i = 0;
    int rc;
    while (s[i]) {
        rc = fputc(s[i], stream);
        if (EOF == rc) {
            errno = __ctOS_stream_geterror(stream);
            return EOF;
        }
        i++;
    }
    return i;
}

/*
 * Write a string, followed by a newline, to stdout
 * Parameter:
 * @s - the string to be written
 */
int puts(char* s) {
    int rc;
    rc = fputs(s, stdout);
    if (rc) {
        fputc('\n', stdout);
        return rc + 1;
    }
    errno = __ctOS_stream_geterror(stdout);
    return EOF;
}

/*
 * Read a specified number of objects of a specified size from a stream
 * Parameters:
 * @ptr - the memory area where the result is stored
 * @size - size of an object
 * @nitems - number of objects to read
 * @stream - the stream from which we read
 */
size_t fread(void* ptr, size_t size, size_t nitems, FILE* stream) {
    unsigned char* target = (unsigned char*) ptr;
    size_t i = 0;
    size_t j = 0;
    int rc;
    if (0 == nitems)
        return 0;
    for (i = 0; i < nitems; i++) {
        for (j = 0; j < size; j++) {
            rc = fgetc(stream);
            if (EOF == rc) {
                errno = __ctOS_stream_geterror(stream);
                return i;
            }
            target[i * size + j] = rc;
        }
    }
    return i;
}

/*
 * Write a specified number of objects of a specified size to a stream
 * Parameters:
 * @ptr - the memory area where the data to be written is stored
 * @size - size of an object
 * @nitems - number of objects to write
 * @stream - the stream to which we write
 */
size_t fwrite(void* ptr, size_t size, size_t nitems, FILE* stream) {
    unsigned char* src = (unsigned char*) ptr;
    size_t i = 0;
    size_t j = 0;
    int rc;
    if (0 == nitems)
        return 0;
    for (i = 0; i < nitems; i++) {
        for (j = 0; j < size; j++) {
            rc = fputc(src[i * size + j], stream);
            if (EOF == rc) {
                errno = __ctOS_stream_geterror(stream);
                return i;
            }
        }
    }
    return i;
}

/*
 * Getc - this is the same as fgetc
 */
int getc(FILE* stream) {
    return fgetc(stream);
}

/*
 * Putc - this is the same as fputc
 */
int putc(int c, FILE* stream) {
    return fputc(c, stream);
}

/*
 * Putchar - this is the same as putc(., stdout)
 */
int putchar(int c) {
    return putc(c, stdout);
}

/*
 * Getchar - this is the same as getc(stdin)
 */
int getchar() {
    return getc(stdin);
}

/*
 * Rewind a stream
 */
void rewind(FILE* stream) {
    fseek(stream, 0L, SEEK_SET);
    clearerr(stream);
}

/*
 * Set buffering mode and buffer for a stream
 * Parameter:
 * @stream - the stream
 * @buf - the buffer to use (or NULL)
 * @type - buffering mode
 * @size - size of buffer
 *
 */
int setvbuf(FILE* stream, char* buf, int type, size_t size) {
    return __ctOS_stream_setvbuf(stream, buf, type, size);
}

/*
 * Set buffer for a stream
 * Parameter:
 * @stream - the stream
 * @buf - the buffer to use (NULL = turn off buffering)
 */
void setbuf(FILE* stream, char* buf) {
    if (buf)
        setvbuf(stream, buf, _IOFBF, BUFSIZ);
    else
        setvbuf(stream, 0, _IONBF, BUFSIZ);
}

/*
 * Push a byte back into the input stream
 * Parameter:
 * @c - the character to be pushed back
 * @stream - the stream
 */
int ungetc(int c, FILE *stream) {
    return __ctOS_stream_ungetc(stream, c);
}

/*
 * Convert a string to an unsigned integer value
 * (decimal notation is assumed)
 * Parameter:
 * @s - string to parse
 * @size - length of string
 * Return value:
 * integer value of string or -1 if the string is empty or not a number
 */
static int __strntoi(const char* s, int size) {
    int result = 0;
    int len = 0;
    int i;
    int my_base = 1;
    if (0 == size)
        return -1;
    /*
     * First go through the string until we hit upon the first
     * character which is not a number
     */
    while (len < size) {
        if (!isdigit(s[len]))
            break;
        len++;
    }
    if (len == 0) {
        return -1;
    }
    /*
     * Now assemble our number
     */
    for (i = len - 1; i >= 0; i--) {
        result = result + (s[i] - '0') * my_base;
        my_base = my_base * 10;
    }
    return result;
}

/*
 * Parse a conversion specification for the printf function
 * Parameter:
 * @ptr - address of a char* ptr which initially points to the % preceding the conversion
 * specification and is advanced until it points to the conversion specifier by this function
 * @flags - the flags contained in the conversion specification are stored here
 * @width - the width stored in the conversion specification is stored here
 * @precision - the precision contained in the conversion specification is stored here
 * Return value:
 * 0 if no parsing error occured
 * 1 if a parsing error occured
 */
static int parse_conv_specs_printf(char** ptr, int* flags, int* width,
        int* precision) {
    int field_length;
    int i;
    /*
     * Advance to first character after %
     */
    (*ptr)++;
    /*
     * First check for any flags at the beginning of the string
     */
    field_length = strspn(*ptr, "+- #0");
    for (i = 0; i < field_length; i++) {
        switch ((*ptr)[i]) {
            case '+':
                *flags |= __PRINTF_FLAGS_PLUS;
                break;
            case '-':
                *flags |= __PRINTF_FLAGS_MINUS;
                break;
            case '#':
                *flags |= __PRINTF_FLAGS_HASH;
                break;
            case ' ':
                *flags |= __PRINTF_FLAGS_SPACE;
                break;
            case '0':
                *flags |= __PRINTF_FLAGS_ZERO;
                break;
            case 0:
                return 1;
        }
    }
    (*ptr) += field_length;
    /*
     * Now parse field width
     */
    field_length = strspn(*ptr, "0123456789");
    if (field_length > 0) {
        *width = __strntoi(*ptr, field_length);
    }
    /*
     * Handle special case that * is specified
     */
    else if (**ptr=='*') {
        field_length = 1;
        *flags += __PRINTF_FLAGS_DYN_WIDTH;
    }
    (*ptr) += field_length;
    /*
     * Parse the precision
     */
    if (**ptr == '.') {
        (*ptr)++;
        field_length = strspn(*ptr, "0123456789");
        if (field_length > 0) {
            *precision = __strntoi(*ptr, field_length);
            if (precision < 0) {
                *precision = (-1) * (*precision);
                *flags |= __PRINTF_FLAGS_MINUS;
            }
        }
        else  if (**ptr=='*') {
            field_length = 1;
            *flags += __PRINTF_FLAGS_DYN_PREC;
        }
        (*ptr) += field_length;
    }
    /*
     * Finally parse length modifiers
     */
    field_length = strspn(*ptr, "hljztL");
    (*ptr) += field_length;
    return 0;
}

/*
 * Print a string
 * Parameter:
 * @stream - the stream to which we print
 * @s - the string
 * @precision: maximum number of characters to be printed, -1 if no precision is specified
 * @count - this number will be incremented by one for each character printed
 * @size - the size limit, we will only print as long as count < size, -1 means no limit
 * Return value:
 * the number of character written
 */
static int __do_print_string(__ctOS_stream_t* stream, char* s, int precision, int width, int flags, int*count, int size) {
    int mycount = 0;
    int i;
    int chars_tobeprinted = 0;
    /*
     * We will print MAX(strlen(s), precision) characters
     */
    for (i = 0; s[i] && ((i < precision) || (-1 == precision)); i++) {
        chars_tobeprinted++;
    }
    /*
     * Print trailing spaces if necessary and - flag not given
     */
    if ((0 == (flags & __PRINTF_FLAGS_MINUS)) && (-1 != width)) {
        for (i = 0; i < width - chars_tobeprinted; i++) {
            if ((-1 == size) || ((*count) < size)) {
                if (EOF == __ctOS_stream_putc(stream, ' '))
                    return -1;
            }
            mycount++;
            (*count)++;
        }
    }
    /*
     * Now print actual string
     */
    i = 0;
    while (s[i] && ((mycount < precision) || (-1 == precision))) {
        if ((-1 == size) || ((*count) < size))
            if (EOF == __ctOS_stream_putc(stream, s[i]))
                return -1;
        mycount++;
        i++;
        (*count)++;
    }
    /*
     * Print trailing spaces if required - note that this should only happen
     * if __PRINTF_FLAGS_MINUS was set as we should have printed leading spaces
     * otherwise above
     */
    while ((mycount < width) && (-1 != width)) {
        if ((-1 == size) || ((*count) < size)) {
            if (EOF == __ctOS_stream_putc(stream, ' '))
                return -1;
        }
        mycount++;
        (*count)++;
    }
    return mycount;
}

/*
 * Print a decimal unsigned integer in a "raw" format, i.e. without any leading zeroes or spaces. See the comments
 * in __do_print_int for a description of the parameters
 */
static int __do_print_uint(__ctOS_stream_t* stream, unsigned int  x, int flags, int base, int* count, int size) {
    int mycount = 0;
    int digit = -1;
    unsigned int unsigned_value = x;
    int shift;
    int last_digit = -1;
    unsigned int tmp;
    int i;
    unsigned char c;
    /*
     * We do this using the following algorithm:
     * 1) divide value by base until result becomes smaller than base, count number of divisions
     * 2) the number of divisions is the number of digits at the right of the digit we are about to print. If that
     * if that is smaller than the same number from the previous loop iteration minus 1, print a corresponding number of zeroes
     * 3) the result of the last division was the first digit - print it
     * 4) multiply result of last division by base^(number of divisions done) and subtract that from value
     * 5) if the value is now zero, print as many zeros as divisions have been done
     * 6) repeat that operation until the value becomes zero
     */
    while (unsigned_value) {
        shift = 1;
        last_digit = digit;
        digit = 0;
        tmp = unsigned_value;
        while (tmp >= base) {
            tmp = tmp / base;
            shift = shift * base;
            digit++;
        }
        /*
         * Now digit is the number of digits on the right
         * of the number we now print, i.e. the number of digits still to go after
         * printing this digit.
         * We expect that digit has only changed by one since the last loop - if
         * not, we have skipped a few zeros which we need to print now first
         */
        if (last_digit != -1) {
            for (i = 0; i < last_digit - digit - 1; i++) {
                if ((-1 == size) || ((*count) < size)) {
                   if (EOF== __ctOS_stream_putc(stream, '0'))
                       return -1;
                }
                mycount++;
                (*count)++;
            }
        }
        /*
         * Now determine representation of the digit in ASCII and print it
         */
        if (16==base) {
            if (flags & __PRINTF_FLAGS_CAP) {
                c = ((tmp < 10) ? '0'+tmp : 'A' + tmp - 10);
            }
            else {
                c = ((tmp < 10) ? '0'+tmp : 'a' + tmp - 10);
            }
        }
        else {
            c = '0' + tmp;
        }
        if ((-1==size) || ((*count)<size)) {
            if (EOF==__ctOS_stream_putc(stream, c))
                return -1;
        }
        mycount++;
        (*count)++;
        /*
         * What's left to be printed
         */
        unsigned_value = unsigned_value - tmp*shift;
        /*
         * If the value is now zero, it was tmp*shift before,
         * i.e. there are still digit = log(shift) zeroes to be printed
         */
        if (0 == unsigned_value) {
            for (i = 0; i < digit; i++) {
                if ((-1 == size) || ((*count) < size)) {
                    if (EOF == __ctOS_stream_putc(stream, '0'))
                        return -1;
                }
                mycount++;
                (*count)++;
            }
        }
    }
    return mycount;
}

/*
 * Print a formatted decimal integer, including trailing spaces, sign and trailing zeroes
 * Parameter:
 * @stream - the stream to which we print
 * @value - the integer to be printed
 * @precision - the precision
 * @width - the width
 * @signed_int - 1 if the argument is signed, 0 if it is unsigned
 * @flags - flags
 * @base - number base to use
 * @count - counter which will be increased for each character printed
 * @size - only if -1==size or count < size, an actual print is done
 */
static int __do_print_int(__ctOS_stream_t* stream, int value, int precision, int width, int signed_int, int flags,
        unsigned int base, int* count, int size) {
    unsigned int mycount = 0;
    int digits = 0;
    int sign_chars = 0;
    int i;
    unsigned int tmp;
    unsigned int unsigned_value;
    int rc;
    /*
     * Handle sign
     */
    if ((value <0) && (signed_int)) {
        sign_chars = 1;
        unsigned_value = value * (-1);
    }
    else {
        unsigned_value = value;
    }
    /*
     * First determine the number of digits which we will print
     */
    tmp = unsigned_value;
    while (tmp) {
        tmp = tmp / base;
        digits++;
    }
    if (0 == unsigned_value)
        digits = 1;
    /*
     * We will later print max(precision, digits) digits, preceded by sign_chars characters for the sign.
     * If this is still smaller than the width and the flag __PRINTF_FLAGS_MINUS is not set, print trailing spaces first
     */
    if (width != -1) {
        for (i = 0; i < (width - MAX(precision, digits) + sign_chars); i++) {
            if (0 == (__PRINTF_FLAGS_MINUS & flags)) {
                if ((-1 == size) || ((*count) < size)) {
                    if (EOF==__ctOS_stream_putc(stream, ' '))
                        return -1;
                }
                mycount++;
                (*count)++;
            }
        }
    }
    /*
     * Print sign if any
     */
    if (sign_chars) {
        if ((-1 == size) || ((*count) < size)) {
            if (EOF == __ctOS_stream_putc(stream, '-'))
                return -1;
        }
        mycount++;
        (*count)++;
    }
    /*
     * If precision is greater than digits, print precision - digits zeroes first
     */
    for (i = 0;i < (precision - digits); i++) {
        if ((-1 == size) || ((*count) < size)) {
            if (EOF==__ctOS_stream_putc(stream, '0'))
                return -1;
        }
        mycount++;
        (*count)++;
    }
    /*
     * Handle special case that value is zero
     */
    if ((0 == unsigned_value) && (0 != precision)) {
        if ((-1 == size) || ((*count) < size)) {
            if (EOF == __ctOS_stream_putc(stream, '0'))
                return -1;
        }
        mycount++;
        (*count)++;
        /*
         * Print trailing spaces if required - note that this should only happen
         * if __PRINTF_FLAGS_MINUS was set as we should have printed leading spaces
         * otherwise above
         */
        while ((mycount < width) && (-1 != width)) {
            if ((-1 == size) || ((*count) < size)) {
                if (EOF == __ctOS_stream_putc(stream, ' '))
                    return -1;
            }
            mycount++;
            (*count)++;
        }
        return mycount;
    }
    /*
     * Standard case
     */
    if (-1 == (rc = __do_print_uint(stream, unsigned_value, flags, base, count, size))) {
        return -1;
    }
    mycount += rc;
    /*
     * Print trailing spaces if required - note that this should only happen
     * if __PRINTF_FLAGS_MINUS was set as we should have printed leading spaces
     * otherwise above
     */
    while ((mycount < width) && (-1 != width)) {
        if ((-1 == size) || ((*count) < size)) {
            if (EOF == __ctOS_stream_putc(stream, ' '))
                return -1;
        }
        mycount++;
        (*count)++;
    }
    return mycount;
}

/*
 * Print a double
 * Parameter:
 * @stream - the stream to which we print
 * @value - the integer to be printed
 * @precision - the precision
 * @width - the width
 * @signed_int - 1 if the argument is signed, 0 if it is unsigned
 * @flags - flags
 * @count - counter which will be increased for each character printed
 * @size - only if -1==size or count < size, an actual print is done
 * @cap - set this to 1 to capitalize nan and inf
 */
static int __do_print_double(__ctOS_stream_t* stream, double value, int precision, int width, int signed_int, int flags,
       int* count, int size, int cap) {
    double unsigned_value;
    double tmp;
    int base;
    int digits = 0;
    int digit;
    int len;
    int sign_chars = 0;
    int i;
    int rc;
    int mycount = 0;
    double delta = 0;
    /*
     * Handle sign
     */
    if ((value < 0) && (signed_int)) {
        unsigned_value = value * (-1.0);
        sign_chars = 1;
    }
    else {
        unsigned_value = value;
    }
    /*
     * Watch out for special case INF and NAN. As for NaN, the above comparison to determine the sign
     * fails, use isneg to derive actual sign
     */
    if (__ctOS_isinf(value) || __ctOS_isnan(value)) {
        if (__ctOS_isneg(value)) {
            if (EOF == __ctOS_stream_putc(stream, '-'))
                return -1;
            mycount++;
            (*count)++;
        }
        if (__ctOS_isinf(value)) {
            if (cap)
                rc = __do_print_string(stream, "INF", 3, 3, flags, count, size);
            else
                rc = __do_print_string(stream, "inf", 3, 3, flags, count, size);
        }
        else {
            if (cap)
                rc = __do_print_string(stream, "NAN", 3, 3, flags, count, size);
            else
                rc = __do_print_string(stream, "nan", 3, 3, flags, count, size);
        }
        if (EOF == rc)
            return -1;
        mycount+=3;
        return mycount;
    }
    /*
     * To achieve rounding, add  .5 ^ precision
     */
    delta = (double) .5;
    for (i = 0; i < precision; i++) {
        delta = delta / 10;
    }
    unsigned_value = unsigned_value + delta;
    /*
     * Determine number of non-zero digits to the left of the decimal point
     */
    tmp = unsigned_value;
    while (tmp > 1)
    {
        digits++;
        tmp = tmp / 10;
    }
    /*
     * Handle special case of numbers less than 1 - we want to
     * print the leading 0 in this case
     */
    if (unsigned_value < 1)
        digits = 1;
    /*
     * We will print the leading digits, a decimal digit and precision digits to the right of the
     * decimal point, plus maybe a sign - but skip radix if precision is zero
     */
    len = digits + precision + sign_chars + ((0 == precision) ? 0 : 1);
    /*
     * If this is still than the width and the flag __PRINTF_FLAGS_MINUS is not set, print trailing spaces first
     */
    if (width != -1) {
        for (i = 0; i < (width - len); i++) {
            if (0 == (__PRINTF_FLAGS_MINUS & flags)) {
                if ((-1 == size) || ((*count) < size)) {
                    if (EOF==__ctOS_stream_putc(stream, ' '))
                        return -1;
                }
                mycount++;
                (*count)++;
            }
        }
    }
    /*
     * Print sign if any
     */
    if (sign_chars) {
        if ((-1 == size) || ((*count) < size)) {
            if (EOF == __ctOS_stream_putc(stream, '-'))
                return -1;
        }
        mycount++;
        (*count)++;
    }
    /*
     * Now print the digits to the left of the decimal point
     */
    base = 1;
    for (i = 0; i < digits - 1; i++) {
        base = base * 10;
    }
    for (i = 0; i< digits; i++) {
        digit = (int) (unsigned_value / base);
        if (-1 == __ctOS_stream_putc(stream, digit + '0'))
            return -1;
        (*count)++;
        mycount++;
        unsigned_value = unsigned_value - digit*base;
        base = base / 10;
    }
    /*
     * Followed by the decimal point - unless precision is zero
     */
    if (precision) {
        if (-1 == __ctOS_stream_putc(stream, '.'))
            return -1;
        (*count)++;
        mycount++;
    }
    /*
     * At this point, unsigned value consists of the fractional part only. Print precision digits of that
     * parts
     */
    for (i = 0; i < precision; i++) {
        unsigned_value = unsigned_value * 10;
        digit = (int) (unsigned_value);
        unsigned_value = unsigned_value - digit;
        if (-1 == __ctOS_stream_putc(stream, digit + '0'))
            return -1;
        (*count)++;
        mycount++;
    }
    /*
     * Print trailing spaces if required - note that this should only happen
     * if __PRINTF_FLAGS_MINUS was set as we should have printed leading spaces
     * otherwise above
     */
    while ((mycount < width) && (-1 != width)) {
        if ((-1 == size) || ((*count) < size)) {
            if (EOF == __ctOS_stream_putc(stream, ' '))
                return -1;
        }
        mycount++;
        (*count)++;
    }
    return mycount;
}


/*
 * Internal utility function which serves as common "backend" for the different
 * functions in the printf family
 * Parameters:
 * @stream - the stream into which we print
 * @size - the maximum number of characters which we are allowed to print, 0 means as many as necessary
 * @template - the format string
 * @args - the arguments
 * Return value:
 * number of characters printed or -1 if an error occurred. Note that if a size is specified, the return value
 * upon success is the number of characters that would have been printed
 *
 * LIMITATIONS:
 * - flags are correctly parsed, but not interpreted, with the exception of the minus sign
 * - length modifiers are correctly parsed, but not interpreted
 * - only the f and F floating point conversion specifiers are supported at the moment, and accuracy of printing is low for
 *   floating point numbers
 *
 */
static int __do_print(__ctOS_stream_t* stream, size_t size, const char* template, va_list args) {
    int flags = 0;
    int width = -1;
    int precision = -1;
    int count = 0;
    int signed_int;
    char* s;
    char c;
    int i;
    unsigned int u;
    int* int_ptr;
    int cap;
    double double_val;
    /*
     * Start to move through template. If character is different from %, use putc to print it
     * If we hit upon a %-character, get conversion specification and next argument
     * from list of arguments and print. Stop as soon as we hit upon NULL
     */
    char* ptr = (char*) template;
    while (*ptr) {
        if ('%' == *ptr) {
            /*
             * A conversion specification consists of
             * - zero or more flags in any order (-, +, space, #, 0)
             * - an optional field width, which might be *
             * - optionally a precision, separated from the field width by a semicolon
             * - an optional length modifier (h, hh, l, ll, j, z, t, L)
             * - the conversion specifier (d, i, o, u, x, X, f, F, e, E, g, G, a, A, c, s, p, n, %)
             *
             * First reset results before parsing
             */
            flags = 0;
            precision = -1;
            width = -1;
            signed_int = 1;
            if(parse_conv_specs_printf(&ptr, &flags, &width, &precision))
                return -1;
            /*
             * If * was specified as width, get width from variable
             * argument list - same for precision
             */
            if (flags & __PRINTF_FLAGS_DYN_WIDTH)
                width = va_arg(args, int);
            if (flags & __PRINTF_FLAGS_DYN_PREC)
                precision = va_arg(args, int);
            switch (*ptr) {
                case 's':
                    s = va_arg(args, char*);
                    if (-1==__do_print_string(stream, s, precision, width, flags, &count, size))
                        return -1;
                    break;
                case 'u':
                    signed_int = 0;
                case 'd':
                case 'i':
                    i = va_arg(args, int);
                    /*
                     * Default precision for integers is one
                     */
                    if (-1==precision)
                        precision = 1;
                    if (-1==__do_print_int(stream, i, precision, width, signed_int, flags, 10, &count, size))
                        return -1;
                    break;
                case 'o':
                    signed_int = 0;
                    i = va_arg(args, int);
                    /*
                     * Default precision for integers is one
                     */
                    if (-1==precision)
                        precision = 1;
                    if (-1 == __do_print_int(stream, i, precision, width, signed_int, flags, 8, &count, size))
                        return -1;
                    break;
                case 'X':
                    flags |= __PRINTF_FLAGS_CAP;
                case 'x':
                    signed_int = 0;
                    i = va_arg(args, int);
                    /*
                     * Default precision for integers is one
                     */
                    if (-1==precision)
                        precision = 1;
                    if (-1 == __do_print_int(stream, i, precision, width, signed_int, flags, 16, &count, size))
                        return -1;
                    break;
                case 'c':
                    /*
                     * To keep the stack aligned, get argument as unsigned int and convert
                     * later
                     */
                    c = (unsigned char) va_arg(args, unsigned int);
                    if (EOF==__ctOS_stream_putc(stream, c))
                        return -1;
                    count++;
                    break;
                case 'p':
                    /*
                     * Print leading 0x
                     */
                    if (count < size)
                        if (EOF==__ctOS_stream_putc(stream, '0'))
                            return -1;
                    count++;
                    if (count < size)
                        if (EOF==__ctOS_stream_putc(stream, 'x'))
                            return -1;
                    count++;
                    u = va_arg(args, unsigned int);
                    /*
                     * Print with precision -1
                     */
                    if (-1 == __do_print_int(stream, u, -1, -1, 0, 0, 16, &count, size))
                        return -1;
                    break;
                case 'n':
                    int_ptr = va_arg(args, int*);
                    *int_ptr = count;
                    break;
                case '%':
                    if (count < size)
                        if (EOF==__ctOS_stream_putc(stream, '%'))
                            return -1;
                    count++;
                    break;
                case 'f':
                case 'F':
                    cap = (*ptr == 'f') ? 0 : 1;
                    double_val = va_arg(args, double);
                    /*
                     * Default precision for doubles is 6
                     */
                    if (-1 == precision)
                        precision = 6;
                    if (-1==__do_print_double(stream, double_val, precision, width, signed_int, flags,  &count, size, cap))
                        return -1;
                    break;
                case 'E':
                case 'e':
                case 'g':
                case 'G':
                case 'a':
                case 'A':
                    /*
                     * Not yet supported, but get argument from stack
                     */
                    double_val = va_arg(args, double);
                    break;
                default:
                    break;
            }
        }
        else {
            if (count < size)
                if (EOF==__ctOS_stream_putc(stream, *ptr))
                    return -1;
            count++;
        }
        ptr++;
    }
    return count;
}

/*
 * Implementation of printf
 * Parameters:
 * @format - the format template
 * Return value:
 * see __do_print
 */
int printf(const char *format, ...) {
    va_list args;
    int rc;
    va_start(args, format);
    rc = __do_print(stdout, -1, format, args);
    va_end(args);
    return rc;
}

/*
 * Implementation of snprintf
 * Parameters:
 * @s - the string into which we print
 * @n - maximum number of characters to print, including trailing 0
 * @format - the format template
 * Return value:
 * see __do_print
 */
int snprintf(char* s, size_t n, const char* format, ...) {
    va_list args;
    int rc;
    va_start(args, format);
    rc = vsnprintf(s, n, format, args);
    va_end(args);
    return rc;
}

/*
 * Implementation of vsnprintf
 * Parameters:
 * @s - the string into which we print
 * @n - maximum number of characters to print, including trailing 0
 * @format - the format template
 * @ap - the variable argument list
 * Return value:
 * see __do_print
 */
int vsnprintf(char* s, size_t n, const char* format, va_list ap) {
    __ctOS_stream_t tmp_stream;
    int rc = 0;
    __ctOS_stream_open(&tmp_stream, -1);
    /*
     * Initialize a temporary stream and
     * use s as buffer
     */
    __ctOS_stream_setvbuf(&tmp_stream, s, _IOFBF, n);
    /*
     * Call common backend and print at most n-1 characters
     */
    if (n>0)
        rc = __do_print(&tmp_stream, n - 1, format, ap);
    else
        rc = __do_print(&tmp_stream, 0, format, ap);
    /*
     * Finally add trailing zero and close stream
     */
    if (n>0)
        __ctOS_stream_putc(&tmp_stream, 0);
    __ctOS_stream_close(&tmp_stream);
    return rc;
}

/*
 * Implementation of sprintf
 * Parameters:
 * @s - the string into which we print
 * @format - the format template
 * Return value:
 * see __do_print
 */
int sprintf(char* s, const char* format, ...) {
    va_list args;
    int rc;
    va_start(args, format);
    rc = vsprintf(s, format, args);
    va_end(args);
    return rc;
}

/*
 * Implementation of vfprintf
 * Parameters:
 * @stream - the stream into which we print
 * @format - the format string
 * @ap - the variable argument list
 * Return value:
 * see __do_print
 */
int vfprintf(FILE* stream, const char* format, va_list ap) {
    /*
     * Call common backend
     */
    return __do_print(stream, -1, format, ap);
}

/*
 * Implementation of vprintf
 * Parameters:
 * @format - the format string
 * @ap - the variable argument list
 * Return value:
 * see __do_print

 */
int vprintf(const char* format, va_list ap) {
    return vfprintf(stdin, format, ap);
}

/*
 * Implementation of vsprintf
 * Parameters:
 * @s - the string into which we print
 * @format - the format string
 * @ap - the variable argument list
 * Return value:
 * see __do_print
 */
int vsprintf(char* s, const char* format, va_list ap) {
    int rc = 0;
    __ctOS_stream_t tmp_stream;
    /*
     * Initialize a temporary stream and
     * use s as buffer
     */
    __ctOS_stream_open(&tmp_stream, -10);
    __ctOS_stream_setvbuf(&tmp_stream, s, _IOFBF, INT32_MAX);
    /*
     * Call common backend
     */
    rc = __do_print(&tmp_stream, -1, format, ap);
    /*
     * Finally add trailing zero and close stream
     */
    __ctOS_stream_putc(&tmp_stream, 0);
    __ctOS_stream_close(&tmp_stream);
    return rc;
}

/*
 * Parse a conversion specification for the scanf function
 * Parameter:
 * @ptr - address of a char* ptr which initially points to the % preceding the conversion
 * specification and is advanced until it points to the conversion specifier by this function
 * @suppress_assignment - set to 1 if the specs contain an assignment suppress operator *
 * Return value:
 * 0 if no parsing error occurred
 * 1 if a parsing error occurred
 */
static int parse_conv_specs_scanf(char** ptr, int* suppress_assignment,
        int* width) {
    int field_length;
    /*
     * Advance to first character after %
     */
    (*ptr)++;
    /*
     * First check for assignment suppress character
     */
    if (**ptr == '*') {
        *suppress_assignment = 1;
        (*ptr)++;
    }
    else {
        *suppress_assignment = 0;
    }
    /*
     * Now parse field width
     */
    field_length = strspn(*ptr, "0123456789");
    if (field_length > 0)
        *width = __strntoi(*ptr, field_length);
    (*ptr) += field_length;
    /*
     * Finally parse length modifiers
     */
    field_length = strspn(*ptr, "hljztL");
    (*ptr) += field_length;
    return 0;
}

/*
 * Consume leading whitespace from a stream
 * Parameters:
 * @stream - the stream
 * Return value:
 * 0 if operation was successful
 * EOF if the end of the stream was encountered
 */
static int __consume_whitespace(__ctOS_stream_t* stream) {
    int c;
    while (1) {
        c = __ctOS_stream_getc(stream);
        if (c==EOF)
            return EOF;
        if (!(isspace((unsigned char)c)))
            break;
    }
    __ctOS_stream_ungetc(stream, c);
    return 0;
}

/*
 * Utility function to convert a single digit to an integer
 * Parameters:
 * @c - the digit to be converted as a character
 * @base - the base
 * Return value:
 * the digit upon success
 * -1 if no conversion was possible
 */
static int __convert_digit(int c, int base) {
    if (10 == base) {
        if (isdigit(c))
            return c - '0';
        return -1;
    }
    if (16 == base) {
        if (isdigit(c))
            return c - '0';
        if ((c >= 'a') && (c <= 'f'))
            return c - 'a' + 10;
        if ((c >= 'A') && (c <= 'F'))
            return c - 'A' + 10;
        return -1;
    }
    if (8 == base) {
        if ((c >= '0') && (c <= '7'))
            return c - '0';
        return -1;
    }
    return -1;
}

/*
 * Consume an unsigned integer from a stream and convert it to an unsigned integer value
 * Parameters:
 * @stream - the stream
 * @base - the number base to use (8, 10 or 16)
 * @uint_ptr - result of the operation will be stored here
 * @width - maximum field width, -1 if none
 * Return value:
 * number of digits read
 * EOF if the end of the stream was encountered before any digit could be read
 */
static int __consume_uint(__ctOS_stream_t* stream, unsigned int base, unsigned int* uint_ptr,int width) {
    int read_characters = 0;
    *uint_ptr = 0;
    int digit = 0;
    int c;
    while ((-1==width) || (read_characters < width)) {
        c = __ctOS_stream_getc(stream);
        if (EOF==c) {
            if (0==read_characters) {
                return EOF;
            }
            else {
                break;
            }
        }
        digit = __convert_digit(c, base);
        if (-1==digit) {
            __ctOS_stream_ungetc(stream, c);
            break;
        }
        *uint_ptr = (*uint_ptr)*base + digit;
        read_characters++;
    }
    return read_characters;
}

/*
 * Consume an integer from a stream and convert it to a signed integer value
 * Parameters:
 * @stream - the stream
 * @base - the number base to use (8, 10 or 16)
 * @int_ptr - result of the operation will be stored here
 * @width - maximum field width, -1 if none
 * Return value:
 * number of digits read
 * EOF if the end of the stream was encountered before any digit could be read
 */
static int __consume_int(__ctOS_stream_t* stream, int base, int* int_ptr, int width) {
    int read_characters = 0;
    *int_ptr = 0;
    int c;
    int sign = 1;
    int sign_read = 0;
    if (0==width)
        return 0;
    /*
     * Check whether first character is a sign
     */
    c = __ctOS_stream_getc(stream);
    if (EOF==c) {
        return EOF;
    }
    if ('-'==c) {
        sign = -1;
        sign_read = 1;
    }
    else if ('+'==c) {
        sign_read = 1;
        sign = 1;
    }
    else {
        __ctOS_stream_ungetc(stream, c);
    }
    read_characters = __consume_uint(stream, base, (unsigned int*) int_ptr, ((width==-1) ? -1 : (width-sign_read)));
    (*int_ptr) = (*int_ptr) * sign;
    return read_characters;
}

/*
 * Utility function to consume a character array from a stream
 * Parameters:
 * @stream - the stream to use
 * @char_ptr - a pointer to the array to which the input is copied
 * @width - maximum number of characters to read, -1 = no limit
 * @append_trailing_zero - if this is set, an additional trailing zero will be added to the array
 * @stop_at_whitespace - stop if a character is a white space and put the character back into the stream
 * Return value:
 * number of characters read or EOF
 */
static int __consume_string(__ctOS_stream_t* stream, char* char_ptr, int width, int append_trailing_zero, int stop_at_whitespace) {
    int count = 0;
    int c;
    while ((-1==width) || (count < width)) {
        c = __ctOS_stream_getc(stream);
        if (EOF==c) {
            if (append_trailing_zero) {
                char_ptr[count]=0;
            }
            return EOF;
        }
        if (stop_at_whitespace && isspace(c)) {
            if (append_trailing_zero) {
                char_ptr[count]=0;
            }
            return count;
        }
        char_ptr[count]=(char) c;
        count++;
    }
    if (append_trailing_zero) {
        char_ptr[count]=0;
    }
    return count;
}

/*
 * Internal utility function which serves as common "backend" for the different
 * functions in the scanf family
 * Parameters:
 * @stream - the stream from which we read
 * @template - the format string
 * @args - the arguments
 * Return value:
 * number of input items assigned or EOF if an error occurred
 *
 * Limitations:
 * - the implementation does not distinguish between the d and i conversion specifier
 * - assignment suppress characters is not implemented
 * - length modifiers are not supported (but correctly parsed)
 * - no floating point conversion specifiers
 * - no scansets
 * - no p conversion specifier supported
 *
 */
static int __do_scan(__ctOS_stream_t* stream, const char* template, va_list args) {
    /*
     * Start to move through template. If character is different from %, use getc to compare
     * against value in stream.
     * If we hit upon a %-character, get conversion specification and next argument
     * from list of arguments and print.
     *
     * If we hit upon a white space, this is a white-space directive - proceed by reading
     * from the stream until we hit upon the next character which is not a white space
     *
     * Stop as soon as we hit upon NULL
     */
    int suppress_assignment;
    int width;
    int c;
    int rc;
    int count = 0;
    int* int_ptr;
    char* char_ptr;
    char* ptr = (char*) template;
    unsigned int initial_filpos = __ctOS_stream_tell(stream);
    while (*ptr) {
        if ('%' == *ptr) {
            /*
             * Reset parameters and parse conversion specification
             */
            width = -1;
            suppress_assignment = 0;
            if(parse_conv_specs_scanf(&ptr, &suppress_assignment, &width)) {
                return (0==count) ? EOF : count;
            }
            /*
             * Now convert input depending on conversion type
             */
            switch (*ptr) {
                case 'd':
                case 'i':
                    SCANF_DEBUG("Have d/i conversion specifier\n");
                    if (EOF==__consume_whitespace(stream)) {
                        return (0==count) ? EOF : count;
                    }
                    int_ptr = (int*) va_arg(args, int*);
                    rc = __consume_int(stream, 10, int_ptr, width);
                    SCANF_DEBUG("rc is %d, count is %d\n", rc, count);
                    if (EOF==rc) {
                        return (0==count) ? EOF : count;
                    }
                    else {
                        count++;
                    }
                    break;
                case 'o':
                    if (EOF==__consume_whitespace(stream)) {
                        return (0==count) ? EOF : count;
                    }
                    int_ptr = (int*) va_arg(args, int*);
                    rc = __consume_int(stream, 8, int_ptr, width);
                    if (EOF==rc) {
                        return (0==count) ? EOF : count;                    }
                    else {
                        count++;
                    }
                    break;
                case 'X':
                case 'x':
                    if (EOF==__consume_whitespace(stream)) {
                        return (0==count) ? EOF : count;
                    }
                    int_ptr = (int*) va_arg(args, int*);
                    rc = __consume_int(stream, 16, int_ptr, width);
                    if (EOF==rc) {
                        return (0==count) ? EOF : count;
                    }
                    else {
                        count++;
                    }
                    break;
                case 'c':
                    char_ptr = (char*) va_arg(args, char*);
                    rc = __consume_string(stream, char_ptr, (-1==width) ? 1 : width, 0, 0);
                    if (EOF==rc) {
                        return (0==count) ? EOF : count;
                    }
                    else {
                        count++;
                    }
                    break;
                case 's':
                    if (EOF==__consume_whitespace(stream)) {
                        return (0==count) ? EOF : count;
                    }
                    char_ptr = (char*) va_arg(args, char*);
                    rc = __consume_string(stream, char_ptr, -1, 1, 1);
                    if (EOF==rc) {
                        if (0==count)
                            return EOF;
                    }
                    else {
                        count++;
                    }
                    break;
                case '%':
                    c = __ctOS_stream_getc(stream);
                    if (EOF==c)
                        return (0==count) ? EOF : count;
                    break;
                case 'n':
                    int_ptr = (int*) va_arg(args, int*);
                    *int_ptr = __ctOS_stream_tell(stream)-initial_filpos;
                    break;
                default:
                    break;
            }
        }
        else if (isspace(*ptr)) {
            SCANF_DEBUG("Have whitespace in format string\n");
            if (EOF==__consume_whitespace(stream)) {
                return (0==count) ? EOF : count;
            }
        }
        else {
            SCANF_DEBUG("Have ordinary character in format string\n");
            c = __ctOS_stream_getc(stream);
            if (((unsigned char)c) != *ptr) {
                return EOF;
            }
        }
        ptr++;
    }
    return count;
}

/*
 * Implementation of the vsscanf function
 */
int vsscanf(const char* s, const char* format, va_list arg) {
    __ctOS_stream_t tmp_stream;
    int rc = 0;
    __ctOS_stream_open(&tmp_stream, -1);
    __ctOS_stream_setvbuf(&tmp_stream, (char*) s, _IOFBF, INT32_MAX);
    /*
     * Call common backend
     */
    rc = __do_scan(&tmp_stream, format, arg);
    /*
     * Finally close stream
     */
    __ctOS_stream_close(&tmp_stream);
    return rc;
}

/*
 * Implementation of the sscanf function
 */
int sscanf(const char* s, const char* template, ...) {
    va_list args;
    int rc;
    va_start(args, template);
    rc = vsscanf(s, template, args);
    va_end(args);
    return rc;
}

/*
 * Implementation of the vfscanf function
 */
int vfscanf(FILE* stream, const char* format, va_list arg) {
    return __do_scan(stream, format, arg);
}

/*
 * Implementation of the fscanf function
 */
int fscanf(FILE* stream, const char* format, ...) {
    va_list args;
    int rc;
    va_start(args, format);
    rc = vfscanf(stream, format, args);
    va_end(args);
    return rc;
}

/*
 * Implementation of the fprintf function
 */
int fprintf(FILE* stream, const char* format, ...) {
    va_list args;
    int rc;
    va_start(args, format);
    rc = vfprintf(stream, format, args);
    va_end(args);
    return rc;
}

/*
 * Implementation of scanf function
 */
int scanf(const char* format, ...) {
    va_list args;
    int rc;
    va_start(args, format);
    rc = vfscanf(stdin, format, args);
    va_end(args);
    return rc;
}

/*
 * Implementation of vscanf function
 */
int vscanf(const char* format, va_list args) {
    return vfscanf(stdin, format, args);
}

/*
 * Return the file descriptor associated with a FILE
 */
int fileno(FILE *stream) {
    if (stream)
        return stream->fd;
    errno = EBADF;
    return -1;
}

/*
 * Write an error message describing perror to stderr
 */
void perror(const char *s) {
    if (s)
        fprintf(stderr, "%s: ", s);
    fprintf(stderr, "%s\n", strerror(errno));
}

/*
 * Rename a file (which may be a regular file or a directory)
 *
 * When the operation completes, the file previously accessed by the oldpath will be accessible by newpath and no
 * longer by oldpath. If both oldpath and newpath refer to the same file, the function returns successfully without any
 * further action. If newpath points to an existing file, it will be removed.
 *
 * The following validations are done and lead to an error being returned if one of them fails:
 * 1) if oldpath is a regular file, newpath must not refer to an existing directory
 * 2) if oldpath is a directory, newpath must not refer to an existing file which is not a directory
 * 3) if newpath names an existing directory, it must be empty
 * 4) newpath must not contain a path prefix which names old, i.e. directories cannot be moved "down the tree"
 * 5) oldpath and newpath must be located on the same mounted filesystem
 * 6) oldpath and newpath must not refer to a special file
 * 7) the last path component of oldpath must not be .
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * the st_ctime field of the parent directories of old and new file are not updated
 */
int rename(const char* oldpath, const char* newpath) {
    int res;
    res = __ctOS_rename((char*) oldpath, (char*) newpath);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return 0;
}
