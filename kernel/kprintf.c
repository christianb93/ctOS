/*
 * kprintf.c
 *
 * This module contains an implementation of the kprintf
 * function which offers formatting in kernel mode similar
 * to printf in user space
 */

#include "ktypes.h"
#include "vga.h"
#include "lib/stdarg.h"
#include "kprintf.h"

int __loglevel;

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

/*
 * Utility function. Uses kputchar to
 * print @value with @digit digits
 * where digits is the number of nibbles
 * to be included in the output
 * Parameter:
 * @value - value to be printed
 * @digits - hexadecimal digit
 */
static void printhex(win_t* win, u32 value, int digits) {
    int shift = (digits - 1) * 4;
    u32 mask = 0xf << shift;
    u32 c;
    while (shift >= 0) {
        c = ((value & mask) >> shift) + 48;
        if (c > 57)
            c += 39;
        win_putchar(win, (u8) c);
        if (16 == shift)
            win_putchar(win, ':');
        shift -= 4;
        mask = mask >> 4;
    }
}

/*
 * Print a string
 * Parameter:
 * @string - the string to be printed
 */
static void kputs(win_t* win, char* string) {
    u32 pos = 0;
    while (string[pos]) {
        win_putchar(win, string[pos++]);
    }
}


/*
 * POSIX strspn
 * The strspn() function calculates the length of the initial segment
 * of @s which consists entirely of characters in @accept.
 * Parameters:
 * @s - string to check
 * @accept - all accepted characters
 */
static int strspn(const char *s, const char *accept) {
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
 * Return true if the character is a decimal digit
 */
static int isdigit(int x) {
    return (((x>='0') && (x<='9')) ? 1 : 0);
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
        }
        else  if (**ptr=='*') {
            field_length = 1;
            *flags += __PRINTF_FLAGS_DYN_PREC;
        }
        (*ptr) += field_length;
    }
    return 0;
}


/*
 * Implementation of kprintf
 * Currently the following format specifiers are supported:
 * X,x,p - format number as 32 bit hex in format 0xFFFF:FFFF
 * h - format number as hex with two nibbles
 * w - format number as hex with four nibbles (16 bit word)
 * d - format number as decimal number
 * s - string
 * c - character
 * Parameter:
 * @template - a template string
 * @args - a variable argument list
 */
static void vkprintf(win_t* win, char* template, va_list args) {
    char* ptr;
    u32 x, c;
    char* s;
    u32 i;
    u32 nr_of_digits;
    char buffer[32];
    int flags;
    int width;
    int precision;
    /* Start to move through template
     * If character is different from %,
     * use putc to print it
     * If we hit upon a %-character,
     * get format specifier and next argument
     * from list of arguments and print
     * Stop as soon as we hit upon NULL
     */
    ptr = template;
    while (*ptr) {
        if ('%' == *ptr) {
            /* move to format specifier */
            precision = 0;
            flags = 0;
            width = 0;
            parse_conv_specs_printf(&ptr, &flags, &width, &precision);
            switch (*ptr) {
                case 'X':
                case 'p':
                case 'x':
                    x = va_arg(args, u32);
                    printhex(win, x, 8);
                    break;
                case 'h':
                    x = va_arg(args, u32);
                    printhex(win, x, 2);
                    break;
                case 'w':
                    x = va_arg(args, u32);
                    printhex(win, x, 4);
                    break;
                case 'd':
                    i = va_arg(args, u32);
                    if (i < 0) {
                        win_putchar(win, '-');
                        i = (-1) * i;
                    }
                    /* Now we have a positive integer being at most INT32_MAX
                     * The idea is to print starting with the least significant
                     * digit into a buffer
                     */
                    nr_of_digits = 0;
                    if (i == 0) {
                        for (i = 1; i < precision; i++)
                            if (flags & __PRINTF_FLAGS_ZERO)
                                win_putchar(win, '0');
                            else
                                win_putchar(win, ' ');
                        win_putchar(win, '0');
                    }
                    else {
                        while (i > 0) {
                            c = (i % 10);
                            i = i / 10;
                            c += 48;
                            buffer[nr_of_digits] = c;
                            nr_of_digits++;
                        }
                        /*
                         * If a precision is specified, fill up with blanks
                         */
                        for (i = nr_of_digits; i < precision; i++)
                            if (flags & __PRINTF_FLAGS_ZERO)
                                win_putchar(win, '0');
                            else
                                win_putchar(win, ' ');
                        for (i = nr_of_digits; i > 0; i--) {
                            win_putchar(win, buffer[i - 1]);
                        }
                    }
                    break;
                case 's':
                    s = va_arg(args, char*);
                    kputs(win, s);
                    break;
                case 'c':
                    x = va_arg(args, u32);
                    win_putchar(win, x);
                    break;
                default:
                    break;
            }
        }
        else {
            win_putchar(win, *ptr);
        }
        ptr++;
    }
}

/*
 * Very basic implementation of printf
 * using the functions defined in vga.c
 * Implemented format specifiers:
 * X,x,p: 32 bit hex
 * h: 8 bit hex
 * w: 16 bit hex
 * d: decimal
 * s: string
 * c: character
 * This function will only call vkprintf.
 */
void kprintf(char* template, ...) {
    va_list args;
    va_start(args, template);
    vkprintf(0, template, args);
}


/*
 * Similar to kprintf, but print to a window
 */
void wprintf(win_t* win, char* template, ...) {
    va_list args;
    va_start(args, template);
    vkprintf(win, template, args);
}


