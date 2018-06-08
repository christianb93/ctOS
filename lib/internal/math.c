/*
 * math.c
 *
 * This module contains a few mathematic functions, in particular basic routines to work with floating point numbers
 * 
 * Note that the algorithms used in this module are far from being optimal. In each individual case, there is probably
 * an algorithm which is faster, consumes less memory and is more precise. However, the purpose of this library is not to
 * provide a floating point library for scientific or productive use, but to educate me and allow me to try out a few different
 * approaches, like
 * - argument reduction
 * - iterative approaches
 * - bit fiddling
 * - approximation by polynomials
 * - use of a floating point unit
* Correspondingly the algorithms are chosen to be simply, readable and covering all these various aspects.
 */

#include "lib/os/mathlib.h"

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


/*
 * Return NaN
 */
double __ctOS_nan() {
    double x = 0.0;
    __ieee754_double_t* __ieee = (__ieee754_double_t*) &x;
    __ieee->exp = 2047;
    __ieee->mlow = 0;
    __ieee->sign = 0;
    __ieee->mhigh = 0x80000;
    return x;
}

/*
 * Return infinity
 * 
 */
double __ctOS_inf() {
    double x = 0.0;
    __ieee754_double_t* __ieee = (__ieee754_double_t*) &x;
    __ieee->exp = 2047;
    __ieee->mlow = 0;
    __ieee->sign = 0;
    __ieee->mhigh = 0;
    return x;
}


/*
 * Determine ceil or floor, depending on the second argument:
 * 0 - ceil
 * 1 - floor
 */
static double round_int(double x, int ceil_floor) {
    __ieee754_double_t result;
    int exp = GET_EXP(x);
    /* TODO: raise inexact flag if ceil(x) != x */
    /*
     * Special case 0.0 (exponent = 0, i.e. GET_EXP = -BIAS)
     * 
     */
    if (IS_ZERO(x)) {
        return 0.0;
    }
    /*
     * Special case NaN or infinity
     */
    if (__ctOS_isnan(x)  || __ctOS_isinf(x)) {
        return x;
    }
    /*
     * If the exponent is negative, the number is smaller than one
     * For ceil: if it is negative, return 0, else return 1
     * For floor: if it is negative, return -1, else return 0
     */
    if (exp < 0) {
        if (GET_SIGN(x)) {
            return (ceil_floor == 0 ? 0.0 : -1.0);
        }
        return (ceil_floor == 0 ? 1.0 : 0.0);
    }
    /*
     * The exponent is positive. If the exponent is at least 52, 
     * the number is an integral number
     */
    if (exp > 51) {
        return x;
    }
    /*
     * We have 52 - exp fractional bits. We now mask them, i.e. 
     * set the fractional bits to zero. If the exponent is at least 20,
     * all bits of the high mantissa will make it into the result. If the exponent
     * is less than 20, the low part of the mantissa is zero and the hight part is masked
     */
    if (exp >= 20) {
        result.mhigh = GET_MANTISSA_HIGH(x);
        result.mlow = (GET_MANTISSA_LOW(x) >> (52 - exp)) << (52- exp);
        if (result.mlow == GET_MANTISSA_LOW(x)) {
            /*
             * Integral number, return it - otherwise we get a problem
             * because we add 1.0 below
             */
             return x;
        }
    }
    else {
        result.mhigh = (GET_MANTISSA_HIGH(x) >> (20 - exp)) << (20 - exp);
        if (result.mhigh == GET_MANTISSA_HIGH(x)) {
            return x;
        }
        result.mlow = 0;
    }
    result.exp = exp + BIAS;
    result.sign = GET_SIGN(x);
    /*
     * At this point we know that this is not an 
     * integer
     */
     if (result.sign) 
         return (ceil_floor == 0 ? *((double*) &result) : *((double*) &result) - 1);
     return (ceil_floor == 0 ? *((double*) &result) + 1 : *((double*) &result));     
}


/*
 * Determine the ceil of a double
 * 
 */
double __ctOS_ceil(double x) {
    return round_int(x, 0);
}

/*
 * Determine the floor of a double
 * 
 */
double __ctOS_floor(double x) {
    return round_int(x, 1);
}

/*
 * Compute the base 2 logarithm for a number between 1 and 2 
 * 
 * This algorithm is far from optimal, but simple and well known, the oldest reference I could find is
 * Majithia, Levan, A note on base-2 logarithm computation, Proceedings of the IEEE, 61 (10), pp 1519–1520 
 * The calculation is based on the iteration
 * x_0 = x
 * x_{n+1} = x_n^2 / 2^{a_n}
 * where a_n is chosen such that x_n remains between 1 and 2. Specifically, we choose a_n = 1 when x_n^2 >= 2.
 * The logarithms of x_{n+1} and x_n are related by
 * log_2(x_{n+1}) = 2 log_2(x_n) - a_n
 * i.e.
 * log_2(x_n) = 1/2(log(x_{n+1}) + a_n) = 1/2 log_2(x_{n+1}) + 1/2 a_n
 * Thus we obtain the relation
 * log_2(x) = 1/2 a_0 + 1/4 a_1 + 1/8 a_2 + ... + 1/2^n log_2(x_n) 
 * However as x_n is, by construction, always between 1 and 2, the last logarithm is always between 0 and 1 and therefore the last term 
 * converges to zero. We thus obtain the approximation
 * log_2(x) = 1/2 a_0 + 1/4 a_1 + 1/8 a_2 + ....
 *
 */
static double log2_kernel(double x) {
    double result = 0.0;
    double inc = 0.5;
    /*
     * We do fifty iterations, which gives
     * us an error below 1e-15 compared
     * to a standard implementation
     */
    for (int i  = 0 ; i < 50; i++) {
        x = x*x;
        if (x > 2.0){
            x = x / 2;
            result = result + inc;
        }
        inc = inc / 2;
    }
    return result;
}

/*
 * Implementation of log2. We first reduce the argument to a value between 1 and 2 and then
 * apply the function log2_kernel above
 * 
 * If our argument is represented as
 * x = 1.m * 2**exp
 * then 
 * log2(x) = log2(1.m) + exp
 * and 1.m is in the range that we want
 */

double __ctOS_log2(double x) {
    double y;
    int exp;
    /*
     * If the argument is zero, return -infinity
     */
    if (IS_ZERO(x)) {
        return -1.0 * __ctOS_inf();
    }
    /*
     * If the argument is negative, return nan
     */
    if (GET_SIGN(x)) {
        return __ctOS_nan();
    }
    /*
     * If the argument is nan or inf, return x
     */
    if (__ctOS_isnan(x) || __ctOS_isinf(x)) {
        return x;
    }
    /*
     * Extract mantissa part
     */
    y = x;
    exp = GET_EXP(x);
    SET_EXP(y, 0);
    /*
     * Now y is between 1 and 2 and we
     * can call our log2 implementation 
     * for that case
     */
    return log2_kernel(y) + exp;
}