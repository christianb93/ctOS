/*
 * string.c
 */

#include "lib/string.h"
#include "lib/ctype.h"
#include "lib/errno.h"

/*
 * An array of known errors. This should be in line with 
 * include/lib/errors.h
 */
typedef struct {
    int err_no;
    char* err_msg;
} __ctOS_error_msg_t;

static __ctOS_error_msg_t __ctOS_error_msg[] = {
        {ENOMEM, "Not sufficient memory to complete operation"},
        {EPERM, "Operation not permitted"},
        {EAGAIN, "Required resource temporarily not available"},
        {EINVAL, "Invalid argument"},
        {ENOSYS, "Invalid or unsupported system call"},
        {EALREADY, "Resource already in use"},
        {ENODEV, "No such device"},
        {EIO, "I/O operation failed"},
        {EBUSY, "Device busy"},
        {ENOTDIR, "Not a directory"},
        {ENOEXEC, "No valid executable"},
        {EBADF, "Invalid file descriptor"},
        {ENOENT, "No such file"},
        {ENOSPC, "No space left on device"},
        {E2BIG, "Size of arguments exceeds upper limit"},
        {ERANGE, "Value out of range"},
        {ECHILD, "Not a child"},
        {ESRCH, "No such process"},
        {EINTR, "Operation interrupted by signal"},
        {EACCES, "Access denied"},
        {EMFILE, "Too many open files"},
        {EPIPE, "Broken pipe"},
        {ENFILE, "Too many open files"},
        {ESPIPE, "Seek not supported for pipes and FIFOS"},
        {EWOULDBLOCK, "Operation would block"},
        {EEXIST, "File exists"},
        {ENOTTY, "File does not refer to a TTY"},
        {EOVERFLOW, "Arithmetic overflow"},
        {EISDIR, "File descriptor is a directory"},
        {ENETUNREACH, "Network not reachable"},
        {EADDRINUSE, "Address already in use"},
        {ENOTCONN, "Socket not connected"},
        {ETIMEDOUT, "Connection timed out"},
        {ENOTSOCK, "Not a socket"},
        {EAFNOSUPPORT, "Address family not supported"},
        {EADDRNOTAVAIL, "Address not available"},
        {ECONNRESET, "Connection reset by peer"},
        {ECONNREFUSED, "Connection refused"},
        {EMSGSIZE, "Message too long"},
        {ENOBUFS, "No buffer space available"},
        {EISCONN, "Endpoint already connected"},
        {EDOM, "Argument out of domain"},
        {EILSEQ, "Invalid or incomplete multibyte character"},
        {EXDEV, "Invalid cross-device link"},
        {EMLINK, "Too many links"},
        {EFAULT, "Bad address"},
        {EOPNOTSUPP, "Operation not supported"},
        {ELOOP, "Too many levels of symbolic links"},
        {ENAMETOOLONG, "Name too long"}
        
};

/*
 * Convert a character to lower case or leave unchanged
 */
static unsigned char __xtolower(int c) {
    if ((c >= 'A') && (c<='Z'))
        return c-'A'+'a';
    return c;
}

/*
 * ANSI C function strlen
 * Parameter:
 * @s - a string
 * Return value:
 * length of string
 */
int strlen(const char* s) {
    if (0 == s)
        return 0;
    int i = 0;
    while (s[i] != 0)
        i++;
    return i;
}

/*
 * ANSI C strncpy
 * Parameter:
 * @s1 - string to be copied to
 * @s2 - source string
 * @max - number of bytes to copy at most
 * Return value:
 * pointer to string to be copied to
 */
char* strncpy(char* s1, const char* s2, int max) {
    int i;
    if (0==s1)
        return 0;
    for (i = 0; (i < max) && (i < strlen(s2)); i++)
        s1[i] = s2[i];
    if (strlen(s2) < max)
        for (i = strlen(s2); i < max; i++)
            s1[i] = 0;
    return s1;
}

/*
 * ANSI C strcpy
 */
char* strcpy(char* s1, const char* s2) {
    int i = 0;
    if (0==s1)
        return 0;
    while (1) {
        s1[i]=s2[i];
        if (0==s2[i])
            break;
        i++;
    }
    return s1;
}

/*
 * ANSI C strncmp
 * Parameters:
 * @s1 - first string
 * @s2 - second string
 * @max - maximum number of characters to compare
 * Return value:
 * if strings are equal, 0 is returned
 * if s1 is less than s2, -1 is returned
 * if s1 is greater than s2, 1 is returned
 */
int strncmp(const char* s1, const char* s2, int max) {
    int i = 0;
    int result = 0;
    if ((0==s1) || (0==s2))
        return 0;
    while ((s1[i] != 0) && (s2[i] != 0) && (0 == result) && (i < max)) {
        result = ((unsigned char) s1[i]) - ((unsigned char) s2[i]);
        i++;
    }
    if (i < max) {
        if ((s1[i] == 0) && (s2[i] != 0))
            result = -1;
        if ((s2[i] == 0) && (s1[i] != 0))
            result = 1;
    }
    return result;
}

/*
 * ANSI C strncasecmp
 * Parameters:
 * @s1 - first string
 * @s2 - second string
 * @max - maximum number of characters to compare
 * Return value:
 * if strings are equal, 0 is returned
 * if s1 is less than s2, -1 is returned
 * if s1 is greater than s2, 1 is returned
 */
int strncasecmp(const char* s1, const char* s2, int max) {
    int i = 0;
    int result = 0;
    while ((s1[i] != 0) && (s2[i] != 0) && (0 == result) && (i < max)) {
        result = ((unsigned char) __xtolower(s1[i])) - ((unsigned char) __xtolower(s2[i]));
        i++;
    }
    if (i < max) {
        if ((s1[i] == 0) && (s2[i] != 0))
            result = -1;
        if ((s2[i] == 0) && (s1[i] != 0))
            result = 1;
    }
    return result;
}

/*
 * ANSI C strmcp
 * Parameters:
 * @s1 - first string
 * @s2 - second string
 * Return value:
 * if strings are equal, 0 is returned
 * if s1 is less than s2, -1 is returned
 * if s1 is greater than s2, 1 is returned
 */
int strcmp(const char* s1, const char* s2) {
    int n;
    n = strlen(s1);
    if (n < strlen(s2))
        n = strlen(s2);
    return strncmp(s1, s2, n);
}

/*
 * ANSI C strcasemcp
 * Parameters:
 * @s1 - first string
 * @s2 - second string
 * Return value:
 * if strings are equal, 0 is returned
 * if s1 is less than s2, -1 is returned
 * if s1 is greater than s2, 1 is returned
 */
int strcasecmp(const char* s1, const char* s2) {
    int n;
    n = strlen(s1);
    if (n < strlen(s2))
        n = strlen(s2);
    return strncasecmp(s1, s2, n);
}


/*
 * ANSI C memcpy
 * Parameters:
 * @to - target of copy operation
 * @from - source of copy operation
 * @n - number of bytes to copy
 * Return value:
 * pointer to target of copy operation
 */
void* memcpy(void* to, const void* from, size_t n) {
    size_t i;
    for (i = 0; i < n; i++)
        *((char*) (to + i)) = *((char*) (from + i));
    return to;
}

/*
 * ANSI C memmove
 */
void* memmove(void* to, const void* from, size_t n) {
    int i;
    if (to==from)
        return to;
    /*
     * If to < from, we can use memcpy
     */
    if (to <= from)
        return memcpy(to, from, n);
    /*
     * Source and target overlap. Copy backwards
     */
    for (i = n-1; i >=0; i--)
        *((char*) (to + i)) = *((char*) (from + i));
    return to;
}

/*
 * POSIX strspn
 * The strspn() function calculates the length of the initial segment
 * of @s which consists entirely of characters in @accept.
 * Parameters:
 * @s - string to check
 * @accept - all accepted characters
 */
size_t strspn(const char *s, const char *accept) {
    size_t ret;
    const char* ptr;
    ret = 0;
    while (s[ret] != 0) {
        ptr = accept;
        while (*ptr != 0) {
            if (*ptr == s[ret])
                break;
            ptr++;
        }
        if (0 == *ptr)
            return ret;
        ret++;
    }
    return ret;
}

/*
 * POSIX strcspn
 * The strcspn() function calculates the length of the initial segment
 * of @s which does not contain a character in @reject
 * Parameters:
 * @s - string to check
 * @reject - characters to reject
 */
size_t strcspn(const char *s, const char *reject) {
    size_t ret;
    const char* ptr;
    ret = 0;
    while (s[ret] != 0) {
        ptr = reject;
        while (*ptr != 0) {
            if (*ptr == s[ret])
                break;
            ptr++;
        }
        if (0 != *ptr)
            return ret;
        ret++;
    }
    return ret;
}

/*
 * POSIX strtok
 * A sequence of calls to strtok() breaks the string pointed to by @s1
 * into a  sequence  of  tokens, each of which is delimited by a byte from
 * the string pointed to by @s2. The first call in the sequence has @s1 as its
 * first  argument, and is followed by calls with a null pointer as their
 * first argument.  The separator string pointed to by @s2 may be different
 * from call to call.
 * The first call in the sequence searches the string pointed to by @s1 for
 * the first byte that is not contained in the  current  separator  string
 * pointed to by @s2. If no such byte is found, then there are no tokens in
 * the string pointed to by @s1 and strtok() shall return a  null  pointer.
 * If such a byte is found, it is the start of the first token.
 * The  strtok() function then searches from there for a byte that is
 * contained in the current separator string. If no such byte is  found,
 * the current  token  extends  to the end of the string pointed to by @s1,
 * and subsequent searches for a token shall return a null pointer.
 * If such a byte  is  found, it is overwritten by a null byte,
 * which terminates the current token. The strtok() function saves a
 * pointer to  the  following byte, from which the next search
 * for a token shall start. Each  subsequent  call,
 * with  a null pointer as the value of the first argument,
 * starts searching  from  the  saved  pointer  and  behaves
 * as described above.
 * Parameters:
 * @s1 - string to tokenize
 * @s2 - separator
 */
char *strtok(char *s1, const char *s2) {
    static char* last;
    int start_token;
    int sep;
    if (0 == s1)
        s1 = last;
    if (0 == s1)
        return 0;
    /* Get index of first character which is not a separator */
    start_token = strspn(s1, s2);
    if (start_token == strlen(s1))
        return 0;
    /* Determine index of next separator */
    sep = start_token + strcspn(s1 + start_token, s2);
    if (sep == strlen(s1))
        last = 0;
    else
        last = s1 + sep + 1;
    s1[sep] = 0;
    return s1 + start_token;
}

/*
 * POSIX memset
 * The memset() function shall copy @c (converted to an unsigned char) into
 * each of the first n bytes of the object pointed to by @s.
 * Parameters:
 * @s - memory area to be filled
 * @c - character used to fill up area
 * @n - number of bytes to be written
 * Return value:
 * memory area filled
 */
void *memset(void *s, int c, size_t n) {
    void* ptr = s;
    while ((ptr - s) < n) {
        *((unsigned char*) ptr) = (unsigned char) c;
        ptr++;
    }
    return s;
}

/*
 * POSIX strchr
 * The strchr() function shall locate the first occurrence of @c
 * (converted to a char) in the string pointed to by @c.
 * The terminating null byte is considered to be part of the string.
 * Upon completion, strchr() shall return a pointer to the byte,
 * or a null pointer if the byte was not found.
 * Parameter:
 * @s - string to search
 * @c - character to search for
 */
char *strchr(const char *s, int c) {
    char* ret = (char*) s;
    if (0 == c)
        return (char*) (s + strlen(s));
    while ((*ret != ((char) c)) && (*ret != 0))
        ret++;
    if (*ret == 0)
        return 0;
    return ret;
}

/*
 * POSIX strrchr
 * The strrchr() function locates the last occurence of the
 * character c in the string
 * Parameter:
 * @s - string to search
 * @c - character to search for
 */
char *strrchr(const char *s, int c) {
    char* ret = (char*) s + strlen(s);
    while ((ret >= s) && (*ret != ((char) c)))
        ret--;
    if (ret < s)
        return 0;
    return ret;
}

/*
 * ANSI C strcat
 */
char *strcat(char* s1, const char* s2) {
    strcpy(s1+strlen(s1), s2);
    return s1;
}

/*
 * ANSI C strerror
 */
char *strerror(int errnum) {
    int i;
    for (i=0;i< sizeof(__ctOS_error_msg) / sizeof(__ctOS_error_msg_t);i++) {
        if (__ctOS_error_msg[i].err_no==errnum) {
            return __ctOS_error_msg[i].err_msg;
        }
    }
    return "Unspecified error";
}

/*
 * ANSI C memcmp
 */
int memcmp(const void *s1, const void *s2, size_t n) {
    int i;
    int result = 0;
    unsigned char* ptr1 = (unsigned char*) s1;
    unsigned char* ptr2 = (unsigned char*) s2;
    for (i=0;i<n;i++) {
        if (ptr1[i]!=ptr2[i]) {
            result = (ptr1[i] > ptr2[i]) ? 1 : -1;
            break;
        }
    }
    return result;
}

/*
 * ANSI C strstr - locate s2 within s1
 */
char* strstr(const char* s1, const char* s2) {
    int s2_len = strlen(s2);
    int s1_len = strlen(s1);
    int i;
    if (0==s2_len)
        return (char*) s1;
    for (i=0;i<=s1_len - s2_len;i++)
        if (0==strncmp(s1+i, s2, s2_len))
            return (char*) s1+i;
    return 0;
}



/*
 * ANSI C strpbrk - locate any byte from s2 within s1
 */
char* strpbrk(const char* s1, const char* s2) {
    int i;
    int n = strlen(s1);
    for (i=0;i<n;i++) {
        if (strchr(s2, s1[i]))
            return (char*) s1+i;
    }
    return 0;
}

/*
 * Strncat
 *
 * Append at most n bytes from s2 to s1 and add a trailing zero
 */
char *strncat(char* s1, const char* s2, size_t n) {
    int i;
    int offset = strlen(s1);
    for (i=0;i<n;i++) {
        if (0==s2[i])
            break;
        s1[i+offset] = s2[i];
    }
    s1[i+offset]=0;
    return s1;
}


/*
 * Implementation of POSIX strcoll. 
 * 
 * As we currently only support the POSIX / C locale, this
 * is the same as strcmp
 */
int strcoll(const char *s1, const char *s2) {
    return strcmp(s1, s2);
}