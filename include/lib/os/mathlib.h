/*
 * mathlib.h
 *
 *  Created on: Oct 8, 2012
 *      Author: chr
 */

#ifndef _MATHLIB_H_
#define _MATHLIB_H_


/*
 * This is an IEEE 754 floating point value of size "double", represented as
 * - a 52 bit mantissa 
 * - an 11 bit exponent (signed)
 * - a sign bit
 * This obviously only works on little-endian targets and is sometimes called
 * a 64 bit double as it consumes 64 bit in memory
 * The value of this number is given by
 * 
 * (-1)^sign * (1.<migh><mlow>) * 2^(exp - bias)
 * 
 * where the bias is 2^10 - 1 =  0x3ff. Note the implicit one at the beginning. For instance, if
 * mhigh = 0x80000
 * mlow  = 0x00000000
 * exp = 0
 * this is binary 1.1, i.e. decimal 1.5.
 * 
 * Also note that the exponent can be extracted from the 64 bit number as
 * exp = (number >> 20)  & 0x7FF - 0x3FF.
 * 
 * Special values: 
 * - if exponent and mantissa are both zero (i.e. if the stored exponent is already zero before substracting the bias), the
 *   number is zero by definition (as otherwise there would be no way to distinguish 0.0 and 1.0). Thus zero can either be
 *   represented by the 64 bit value 0x0 ("+0.0") or by 0x80000000:00000000 ("-0.0")
 * - infinity is the number with mantissa zero and the maximum exponent (unbiased) 1024, i.e.
 *   0x7FF00000:00000
 * - any value where all exponent bits are set to one (like for infinity), but at least one mantissa bit is set,
 *   is considered "NaN"
 */
typedef struct {
    unsigned int mlow;                 // Low dword of mantissa (and full number)
    unsigned int mhigh:20;             // High dword of mantissa
    unsigned int exp:11;               // Exponent
    unsigned char sign:1;              // Sign
} __attribute__ ((packed)) __ieee754_double_t;

/*
 * Extract the mantissa from a double
 */
#define GET_MANTISSA(x)       (((__ieee754_double_t*)(&x))->mlow + (((unsigned long long)((__ieee754_double_t*)(&x))->mhigh) << 32))
#define GET_MANTISSA_LOW(x)   (((__ieee754_double_t*)(&x))->mlow)
#define GET_MANTISSA_HIGH(x)  (((__ieee754_double_t*)(&x))->mhigh)

/*
 * Extract the exponent from a double (as a signed integer)
 */
#define BIAS 0x3FF
#define GET_EXP(x)   (((int)(((__ieee754_double_t*)(&x))->exp)) - 0x3FF)
#define SET_EXP(x, e)   ((((__ieee754_double_t*)(&x))->exp) =  e + 0x3FF)

/*
 * Extract the sign 
 */
#define GET_SIGN(x) (((__ieee754_double_t*)(&x))->sign)

/*
 * Is the number zero?
 */
#define IS_ZERO(x) ((((__ieee754_double_t*)(&x))->exp == 0) && (((__ieee754_double_t*)(&x))->mlow == 0) && (((__ieee754_double_t*)(&x))->mhigh == 0))

/*
 * Value of ln(2)
 */
#define M_LN2 0.69314718056

/*
 * Value of pi
 */
#define M_PI  3.141592653589793238

int __ctOS_isinf(double value);
int __ctOS_isnan(double value);
int __ctOS_isneg(double value);

double __ctOS_inf();
double __ctOS_nan();

double __ctOS_ceil(double x);
double __ctOS_floor(double x);

double __ctOS_log2(double x);
double __ctOS_exp2_kernel(double x);
double __ctOS_exp2(double x);
double __ctOS_exp(double x);

double __ctOS_cos(double x);
double __ctOS_sin(double x);
double __ctOS_tan_kernel(double x);
double __ctOS_tan(double x);

#endif /* _MATHLIB_H_ */
