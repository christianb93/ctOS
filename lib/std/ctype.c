/*
 * ctype.c
 *
 *
 * This file implements the function required in ctype.h for the POSIX locate. Currently no other locales
 * are supported
 */

#include "lib/ctype.h"

/*
 * This is a list of punctuation characters in the POSIX locale
 */
static int __punct[] = {
        '!',
        '"',
        '#',
        '$',
        '%',
        '&',
        39,
        '(',
        ')',
        '*',
        '+',
        ',',
        '-',
        '.',
        '/',
        ':',
        ';',
        '<',
        '=',
        '>',
        '?',
        '@',
        ']',
        '\\',
        ']',
        '^',
        '_',
        '`',
        '{',
        '|',
        '}',
        '~'
};

#define __punct_chars (sizeof(__punct) / sizeof(int))

/*
 * Return true if the character is a decimal digit
 */
int isdigit(int x) {
    return (((x>='0') && (x<='9')) ? 1 : 0);
}

/*
 * Returns true if the character is a hexadecimal character, i.e.
 * a-f or A-F or a decimal digit
 */
int isxdigit(int x) {
    return ( (isdigit(x) || ((x<='F') && (x>='A'))  || ((x<='f') && (x>='a'))) ? 1 : 0 );
}

/*
 * Return true if the character is a whitespace character, i.e. either
 * - a space
 * - a form-feed
 * - a newline
 * - carriage return
 * - horizontal tab
 * - vertical tab
 */
int isspace(int x) {
    return (((x)==' ') || ((x)=='\f') || ((x)=='\n') || ((x)=='\r') || ((x)=='\t') || ((x)=='\v'));
}

/*
 * Return true if the character is a lower-case character. In the POSIX locate, these are
 * exactly the 26 characters a-z
 */
int islower(int x) {
    return (((x) >= 'a') && ((x)<='z'));
}

/*
 * Return true if the character is an upper-case character. In the POSIX locate, these are
 * exactly the 26 characters A-Z
 */
int isupper(int x) {
    return (((x) >= 'A') && ((x)<='Z'));
}

/*
 * Return true if the character is a letter. In the POSIX locate, these are exactly the letters
 * a-z and A-Z
 */
int isalpha(int x) {
    return (islower(x) || isupper(x));
}

/*
 * Returns true if the character is either a digit or a letter
 */
int isalnum(int x) {
    return (isalpha(x) || isdigit(x));
}

/*
 * Returns true if the character is a punctuation character, i.e. a printable character which
 * is not alphanumeric or a white space
 */
int ispunct(int x) {
    int i;
    for (i=0;i<__punct_chars; i++)
        if (__punct[i]==x)
            return 1;
    return 0;
}

/*
 * Returns true if the character is a graphics character or a space
 */
int isprint(int x) {
    return (isgraph(x)) || (x==' ');
}

/*
 * Returns true if the character is a space or a tab
 */
int isblank(int x) {
    return (x==' ') || (x=='\t');
}

/*
 * Returns true if the character is a printable character, i.e.
 * either a digit, a letter or a punctuation character
 */
int isgraph(int x) {
    return (isdigit(x) || isalpha(x) || ispunct(x));
}

/*
 * Returns true if the character is a control character
 * A control character is an ASCII character which is not a letter, a digit,
 * a punctuation character and not a space
 */
int iscntrl(int x) {
    if (x>127)
        return 0;
    if (x<0)
        return 0;
    if (!isalpha(x) && !isdigit(x) && !ispunct(x) && (x != ' '))
            return 1;
    return 0;
}

/*
 * Convert a character to an upper character if the character is a lower character 
 * (in the POSIX locale, these are * exactly the 26 characters a-z)
 * 
 * Return the original character otherwise
 */
int toupper(int x) {
    if (((x) >= 'a') && ((x)<='z')) {
        return (x - 'a' + 'A');
    }
    return x;
}

/*
 * Convert a character to a lower character if the character is an upper character 
 * (in the POSIX locale, these are * exactly the 26 characters A-Z)
 * 
 * Return the original character otherwise
 */
int tolower(int x) {
    if (((x) >= 'A') && ((x)<='Z')) {
        return (x - 'A' + 'a');
    }
    return x;
}