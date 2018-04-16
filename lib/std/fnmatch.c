/*
 * fnmatch.c
 *
 */

#include "lib/fnmatch.h"

/*
 * Check whether a given string matches a filename pattern.
 *
 * The following flags defined in fnmatch.h impact the matching rules
 *
 * - if FNM_PATHNAME is set, a slash ('/') needs to match explicitly and is not matched by
 *   an asterisk or a question mark
 * - if FNM_NOESCAPE is not set, a backslash followed by a character in the pattern matches that
 *   second character in the string
 *
 * If the argument string matches the pattern in the first argument, 0 is returned. If the
 * string does not match, FNM_NOMATCH is returned. In case of an error, -1 is returned.
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * - FNM_PERIOD is not supported
 * - brackets are not supported
 *
 */

int fnmatch(const char *pattern, const char *string, int flags) {
    int ordinary_char;
    /*
     * The following pointers are advanced through the pattern and the string
     * as we go
     */
    char* pattern_pos = (char*) pattern;
    char* string_pos = (char*) string;
    char* next_pattern;
    /*
     * Walk the pattern and for each character in the pattern, consume matching
     * characters in the string or return FNM_NOMATCH is the character in the string
     * does not match the current character in the pattern
     */
    while (*pattern_pos) {
        ordinary_char = 1;
        /*
         * Case 1: asterisk
         */
        if ('*' == *pattern_pos) {
            ordinary_char = 0;
            /*
             * Locate next character in pattern which is not a wildcard
             */
            next_pattern = pattern_pos;
            while (((*next_pattern == '*') || (*next_pattern == '?')) && (*next_pattern)) {
                next_pattern++;
            }
            pattern_pos = next_pattern - 1;
            /*
             * Consume characters from the string until we reach the end of the string
             * or the first non-matching character (a slash when FNM_PATHNAME is set) or
             * the character preceding the * in the pattern if any
             */
            while(*string_pos) {
                if ((flags & FNM_PATHNAME) && ('/' == *string_pos)) {
                    break;
                }
                if (*next_pattern == *string_pos) {
                    break;
                }
                string_pos++;
            }
        }
        /*
         * Case 2: '?'
         */
        else if ('?' == *pattern_pos) {
            ordinary_char = 0;
            /*
             * If FNM_PATHNAME is set, ? matches everything
             * except /, otherwise it matches everything
             */
            if (((flags & FNM_PATHNAME) && (*string_pos) && ('/' != *string_pos))  ||
                    ((*string_pos) && (0 == (flags && FNM_PATHNAME)))) {
                string_pos++;
            }
            else {
                return FNM_NOMATCH;
            }
        }
        /*
         * Case 3: backslash
         */
        else if ('\\' == *pattern_pos) {
            /*
             * If FNM_NOESCAPE is not set, skip the escaping \
             */
            if (0 == (flags & FNM_NOESCAPE)) {
                pattern_pos++;
            }
        }
        /*
         * Ordinary character
         */
        if (ordinary_char) {
            if (*pattern_pos == *string_pos) {
                string_pos++;
            }
            else {
                return FNM_NOMATCH;
            }
        }
        pattern_pos++;
        /*
         * If we have reached the end of the string, we should have reached the
         * end of the pattern as well and vice versa
         */
        if (((0 == *string_pos) && (*pattern_pos)) ||
                ((0 == *pattern_pos) && (*string_pos))) {
            return FNM_NOMATCH;
        }
    }
    return 0;
}

