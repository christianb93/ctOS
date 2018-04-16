/*
 * math.c
 *
 * This module contains a few mathematic functions, in particular basic routines to work with floating point numbers
 */


/*
 * This is an IEEE 754 floating point value of size "double", represented as
 * - a 52 bit mantissa (plus 1 implicit bit)
 * - an 11 bit exponent
 * - a sign bit
 * This obviously only works on little-endian targets
 */
typedef struct {
    unsigned int mlow;                 // Low dword of mantissa
    unsigned int mhigh:20;             // High dword of mantissa
    unsigned int exp:11;               // Exponent
    unsigned char sign:1;              // Sign
} __attribute__ ((packed)) __ieee754_double_t;

/*
 * Return 1 if a double value is infinity. Note that by the standard, a number is infinity if and only if the mantissa
 * bits are all zero and the exponent bits are all 1
 */
int __ctOS_isinf(double value) {
    __ieee754_double_t* repr = (__ieee754_double_t*) &value;
    if ((0 == repr->mhigh) && (0 == repr->mlow) && (2047 == repr->exp))
        return 1;
    return 0;
}

/*
 * Return 1 if a double value is not a number. Note that by the standard, a number is NaN if and only if the mantissa
 * bits are not all zero and the exponent bits are all 1
 */
int __ctOS_isnan(double value) {
    __ieee754_double_t* repr = (__ieee754_double_t*) &value;
    if (((0 != repr->mhigh) || (0 != repr->mlow)) && (2047 == repr->exp))
        return 1;
    return 0;
}

/*
 * Returns true if a double is negative - also works for inf and nan
 */
int __ctOS_isneg(double value) {
    __ieee754_double_t* repr = (__ieee754_double_t*) &value;
    return repr->sign;
}
