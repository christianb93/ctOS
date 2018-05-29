/*
 * locale.c
 *
 */

#include "lib/stddef.h"
#include "lib/locale.h"
#include "lib/string.h"

/*
 * The current locale
 */
static struct lconv current_lconv;
static char* current_locale;

/*
 * Convert a wide character of type wchar_t into a multibyte character. If the first argument is not a null
 * pointer, the resulting multibyte character is stored there, writing at most MB_CUR_MAX bytes
 *
 * It returns the number of bytes in the multibyte character or -1. If the first argument is NULL, 0 will
 * be returned
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * Only the C / POSIX locale is supported, i.e. trivial conversion of ASCII codes 0 - 127 to their
 * corresponding wide char and multibyte characters
 *
 */
int wctomb(char *s, wchar_t wchar) {
    if (0 == s)
        return 0;
    if ((0 <= wchar) && (wchar <= 127)) {
        *s = wchar;
        return 1;
    }
    return -1;
}


/*
 * If s is not a null pointer, mbtowc() shall determine the number of bytes that constitute the character pointed to by s. 
 * It shall then determine the wide-character code for the value of type wchar_t that corresponds to that character. 
 * (The value of the wide-character code corresponding to the null byte is 0.) 
 * If the character is valid and pwc is not a null pointer, mbtowc() shall store the wide-character code in the object pointed to by pwc.
 * 
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * Only the C / POSIX locale is supported, i.e. trivial conversion of ASCII codes 0 - 127 to their
 * corresponding wide char and multibyte characters
 *
 */
int mbtowc(wchar_t * pwc, const char * s, size_t n) {
    if (0 == s)
        return 0;
    if ((0 <= *s) && (*s <= 127)) {
        if (pwc)
            *pwc = *s;
        return 1;
    }
    return -1;
}

/*
 * Get the current locale
 */
struct lconv* localeconv() {
    return &current_lconv;
}

/*
 * Set the current locale in all or a specific category.
 *
 * The category is specified by the first parameter which is supposed to be one of the constants LC_*
 * defined in locale.h. The second parameter specifies the locale to be set and can be NULL to only
 * query the locale.
 *
 * The function returns the new (if locale is not NULL) or current locale as a string
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * - if the locale is an empty string, the locale POSIX is set, regardless of the values of the
 *   environment variables LC_* and LANG
 * - POSIX is the only supported locale at the moment
 *
 */
char* setlocale(int category, const char* locale) {
    /*
     * If locale is NULL, return current locale
     */
    if (0 == locale) {
        current_locale = "POSIX";
    }
    /*
     * If locale is not an empty string, validate it and set new locale
     */
    else if (strcmp("", locale)) {
        if (strcmp(locale, "C") && strcmp(locale, "POSIX")) {
            return 0;
        }
        if (0 == strcmp("C", locale)) {
            current_locale = "C";
        }
        if (0 == strcmp("POSIX", locale)) {
            current_locale = "POSIX";
        }
    }
    /*
     * Update all fields of the locale if required. Note that only the categories LC_MONETARY, LC_ALL
     * and LC_NUMERIC are reflected in the lconv structure
     */
    if ((LC_ALL == category) || (LC_NUMERIC == category)) {
        current_lconv.decimal_point = ".";
        current_lconv.thousands_sep = "";
        current_lconv.grouping = "0";

    }
    if ((LC_ALL == category) || (LC_MONETARY == category)) {
        current_lconv.int_curr_symbol = "";
        current_lconv.currency_symbol = "";
        current_lconv.mon_decimal_point = "";
        current_lconv.mon_thousands_sep = "";
        current_lconv.mon_grouping = "\0";
        current_lconv.positive_sign = "";
        current_lconv.negative_sign = "";
        current_lconv.int_frac_digits = 127;
        current_lconv.frac_digits = 127;
        current_lconv.p_cs_precedes = 127;
        current_lconv.int_p_cs_precedes = 127;
        current_lconv.p_sep_by_space = 127;
        current_lconv.int_p_sep_by_space = 127;
        current_lconv.n_cs_precedes = 127;
        current_lconv.int_n_cs_precedes = 127;
        current_lconv.n_sep_by_space = 127;
        current_lconv.int_n_sep_by_space = 127;
        current_lconv.p_sign_posn = 127;
        current_lconv.n_sign_posn = 127;
        current_lconv.int_p_sign_posn = 127;
        current_lconv.int_n_sign_posn = 127;
    }
    return current_locale;
}
