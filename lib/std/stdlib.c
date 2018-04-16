/*
 * stdlib.c
 */

#include "lib/ctype.h"
#include "lib/string.h"
#include "lib/errno.h"
#include "lib/limits.h"
#include "lib/time.h"
#include "lib/stdlib.h"

#define ERROR(end_ptr, ptr) do { if ((end_ptr)) *(end_ptr) = (ptr); return 0; } while (0);

/*
 * Next random value
 */
static unsigned int nextrand = 0;

/*
 * Convert an individual digit
 */
static int convert_char(char c, int* error, int base) {
    *error = 0;
    /*
     * Ordinary digit
     */
    if (isdigit(c))
        return c-'0';
    /*
     * A character - convert to upper case first
     */
    if ((c>='a') && (c<='z')) {
        c = c-'a'+'A';
    }
    if ((c<'A') || (c>'Z')) {
        *error = 1;
        return 0;
    }
    if (c-'A' >= base-10) {
        *error = 1;
        return 0;
    }
    return c-'A'+10;
}

/*
 * Multiply two 64-bit integers and detect overflow
 */
static unsigned long long mult_overflow(unsigned long long a, unsigned long long b, int* overflow) {
    unsigned long long result = 0;
    *overflow = 0;
    while (b > 0) {
        if (b & 0x1) {
            /*
             * This might overflow
             */
            if (result > ~a)
                *overflow = 1;
            result = result + a;
        }
        b = b >> 1;
        /*
         * Next we need to shift a to the left by one bit, this might
         * overflow as well which is a problem if b is not yet zero.
         * Note that this happens if the number of bits
         * of a plus the number of bits of b minus 1 exceeds 64. As the product
         * of a and b has at least that many binary digits, this is a real overflow
         * and not just due to our algorithm
         */
        if ((a & (1LL << 63)) && (b))
            *overflow = 1;
        a = a << 1;
    }
    return result;
}

/*
 * Internal utility function to convert a string to a long long integer
 * Parameters:
 * @s - the string to be converted
 *
 *
 */
static unsigned long long do_conversion(const char* s, char** end_ptr, unsigned int base, int* sign) {
    char* ptr = (char*) s;
    unsigned long long value = 0;
    unsigned int actual_base = base;
    unsigned long long digit = 0;
    int error = 0;
    /*
     * Step 1: proceed until we hit upon the first non-whitespace character
     */
    while ((*ptr) && isspace(*ptr))
        ptr++;
    if (0==*ptr) {
        ERROR(end_ptr, (char*) s);
    }
    /*
     * Step 2: check whether the next character is a sign and consume it if necessary
     */
    *sign = 1;
    if ('+'==*ptr) {
        ptr++;
    }
    else if ('-'==*ptr) {
        ptr++;
        *sign = -1;
    }
    if (0 == *ptr) {
        ERROR(end_ptr, ptr);
    }
    /*
     * Step 3: derive actual base and consume leading 0x or 0X
     * for hexadecimal numbers
     */
    if ('0'==*ptr) {
        /*
         * This can be a hex or an octal number. Read next character to
         * check this
         */
        ptr++;
        if (0==*ptr) {
            ERROR(end_ptr, ptr);
        }
        if (('x'==*ptr) || ('X'==*ptr)) {
            /*
             * x or X is valid if the base is not specified or 16 or 32 and above
             */
            if ((0 == base) || (16 == base) || (base > 32)) {
                if (0==base)
                    actual_base=16;
                ptr++;
            }
            else {
                ERROR(end_ptr, ptr);
            }
        }
        else {
            /*
             * Assume octal if base is not specified
             */
            if (0==base)
                actual_base=8;
        }
    }
    else
        if (0 == base)
            actual_base = 10;
    /*
     * Step 4: Advance through string until we hit upon a null or a
     * non-digit character
     */
    while (*ptr) {
        digit = convert_char(*ptr, &error, actual_base);
        if (0==error) {
            /*
             * We now multiply value by base and add digit
             */
            value = mult_overflow(value, actual_base, &error);
            /*
             * Now the addition can still overflow
             */
            if ((value > ~digit) || (error)) {
                errno = ERANGE;
                return ULLONG_MAX;
            }
            value = value + digit;
        }
        else {
            break;
        }
        ptr++;
    }
    if (end_ptr)
        *end_ptr = ptr;
    return value;
}

/*
 * ANSI C function strtol
 * Parameter:
 * @s - string to parse
 * @end_ptr - will be set to first character which could not be parsed
 * @base - base to use
 * Return value:
 * long value of string
 */
long strtol(const char* s, char** end_ptr, int base) {
    unsigned long long unsigned_value = 0;
    long long signed_value = 0;
    int sign = 1;
    unsigned_value = do_conversion(s, end_ptr, base, &sign);
    signed_value = ((long long )sign) * unsigned_value;
    /*
     * Check for overflow
     */
    if (1==sign) {
        if ((signed_value>LONG_MAX) || (signed_value <0)) {
            errno = ERANGE;
            return LONG_MAX;
        }
        return (long) signed_value;
    }
    else {
        if ((signed_value < LONG_MIN) || (signed_value >0)) {
            errno = ERANGE;
            return LONG_MIN;
        }
        return ((long) signed_value);
    }
}

/*
 * ANSI C function strtoull
 * Parameter:
 * @s - string to parse
 * @end_ptr - will be set to first character which could not be parsed
 * @base - base to use
 * Return value:
 * unsigned long long value of string
 */
unsigned long long strtoull(const char* s, char** end_ptr, int base) {
    int sign;
    return  do_conversion(s, end_ptr, base, &sign);
}

/*
 * ANSI C function strtoll
 * Parameter:
 * @s - string to parse
 * @end_ptr - will be set to first character which could not be parsed
 * @base - base to use
 * Return value:
 * long long value of string
 */
long long strtoll(const char* s, char** end_ptr, int base) {
    unsigned long long unsigned_value = 0;
    long long signed_value = 0;
    int sign = 1;
    unsigned_value = do_conversion(s, end_ptr, base, &sign);
    signed_value = ((long long) sign) * unsigned_value;
    /*
     * Check for overflow
     */
    if (1==sign) {
        if (signed_value <0) {
            errno = ERANGE;
            return LLONG_MAX;
        }
        return signed_value;
    }
    else {
        if (signed_value >0) {
            errno = ERANGE;
            return LLONG_MIN;
        }
        return signed_value;
    }
}

/*
 * Atoi
 *
 * The call atoi(str) shall be equivalent to:
 *
 * (int) strtol(str, (char **)NULL, 10)
 *
 * except that the handling of errors may differ. If the value cannot be represented, the behavior is undefined.
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS: none
 */
int atoi(const char *str) {
    return (int) strtol(str, 0, 10);
}

/*
 * Atol
 *
 * The call atol(str) shall be equivalent to:
 *
 * strtol(str, (char **)NULL, 10)
 *
 * except that the handling of errors may differ. If the value cannot be represented, the behavior is undefined.
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS: none
 *
 */
long atol(const char *str) {
    return strtol(str, 0, 10);
}

/*
 * Utility function to switch two elements in an array
 * Parameters:
 * @x1 - first element
 * @x2 - second element
 * @width - size of elements in byte
 */
static void _switch(void* x1, void* x2, size_t width) {
    unsigned char tmp;
    size_t i;
    for (i=0;i<width;i++) {
        tmp = *((unsigned char*)(x1+i));
        *((unsigned char*)(x1+i)) = *((unsigned char*)(x2+i));
        *((unsigned char*)(x2+i)) = tmp;
    }
}

/*
 * Utility function to split an array during quick sort
 * Parameters:
 * @base - the array to be splitted
 * @nel - number of elements in array
 * @width - size of each element in array
 * @compare - comparison function
 * Returns index (starting with 0) of pivot element
 */
static int _split(void* base, size_t nel, size_t width, int (*compar)(const void *, const void *)) {
    int i;
    int j;
    void* pivot;
    /*
     * If there is only one element, return its position
     */
    if (nel==1)
        return 0;
    /*
     * Start scan at first element from the left and chose pivot to be the last element
     */
    i=0;
    /*
     * Set pivot element
     */
    pivot = base+(nel-1)*width;
    j = nel-2;
    do {
        /*
         * Locate first element from the left which is greater than pivot
         */
        while ((i<nel-1) && (compar(base+i*width, pivot)<=0) ) {
            i++;
        }
        /*
         * Locate first element from the right which is smaller than pivot
         */
        while((j>0) && (compar(base+j*width, pivot)>=0)) {
            j--;
        }
        /*
         * If i < j switch array elements i and j
         */
        if (i<j)
            _switch(base+i*width, base+j*width, width);
    }
    while (i<j);
    /*
     * Bring pivot to its final position
     */
    if (compar(base+i*width, pivot)>0)
        _switch(base+i*width, pivot, width);
    return i;
}

/*
 * qsort
 * Parameters:
 * @base - array to be sorted
 * @nel - number of elements in array
 * @width - size of each element in bytes
 * @compare - comparison function
 */
void qsort(void *base, size_t nel, size_t width, int (*compar)(const void *, const void *)) {
    int pivot_index;
    /*
     * Return if array is empty
     */
    if (0==nel)
        return;
    pivot_index = _split(base, nel, width, compar);
    qsort(base, pivot_index, width, compar );
    qsort(base + (pivot_index+1)*width, nel - pivot_index -1, width, compar );
}

/*
 * Initialize random number generator
 */
void srand(unsigned int seed) {
    nextrand = seed;
}

/*
 * Return a randum number in the range 0..RAND_MAX
 *
 * This is a classical linear congruential random number generator. The
 * parameters are those used in early versions of FreeBSD as well. This is
 * definitely not secure enough for cryptographical applications
 */
int rand(void) {
    nextrand = nextrand * 1103515245 + 12345;
    return nextrand % (((unsigned int) RAND_MAX)+1);
}
