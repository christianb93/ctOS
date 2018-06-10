/*
 * test_math.c
 *
 */

#include "kunit.h"
#include "lib/os/mathlib.h"
#include <stdio.h>
#include <math.h>
#include <fenv.h>

/*
 * Utility function to print the representation of a double
 * 
 */
void print_ieee(double x) {
    __ieee754_double_t* ieee = (__ieee754_double_t*) &x;
    printf("Double:            %f\n", x);
    printf("EXP:               %d\n", GET_EXP(x));
    printf("MANTISSA:          %llu\n", GET_MANTISSA(x));
    printf("ieee->mlow:        %d\n", ieee->mlow);
    printf("ieee->mhigh:       %d (%x)\n", ieee->mhigh, ieee->mhigh);    
    printf("ieee->sign:        %d\n", ieee->sign);
    printf("64 bit repr.       %llx\n", *((unsigned long long*) (&x)));
}

/*
 * Testcase 1: isinf
 */
int testcase1() {
    double value;
    value = 1.0 / 0.0;
    ASSERT(1 == __ctOS_isinf(value));
    value = 2.1;
    ASSERT(0 == __ctOS_isinf(value));
    ASSERT(__ctOS_isinf(__ctOS_inf()));
    return 0;
}

/*
 * Testcase 2: isnan
 */
int testcase2() {
    double value;
    value = sqrt(-1);
    ASSERT(1 == __ctOS_isnan(value));
    value = 2.1;
    ASSERT(0 == __ctOS_isnan(value));
    ASSERT(__ctOS_isnan(__ctOS_nan()));
    return 0;
}

/*
 * Testcase 3
 * Check exponent, mantissa and sign for an example
 */
int testcase3() {
    double x = 3.141;
    __ieee754_double_t* ieee = (__ieee754_double_t*) &x;
    unsigned long long* as64 = (unsigned long long *) &x;
    ASSERT(sizeof(unsigned long long) == 8);
    ASSERT(sizeof(unsigned long) == 4);
    ASSERT(sizeof(x) == 8);
    unsigned long int low_dword = (*as64) & 0xFFFFFFFF;
    unsigned long int high_dword = (*as64) >> 32;
    int exponent = ((high_dword >> 20) & 0x7ff) - 0x3FF;
    ASSERT(exponent == GET_EXP(x));
    return 0;
}

/*
 * Test ceil
 */
int testcase4() {
    double x;
    __ieee754_double_t* __ieee;
    /*
     * Try 0.5
     */
    ASSERT(__ctOS_ceil(0.5) == 1.0);
    /*
     * Try 1.0
     */
    ASSERT(__ctOS_ceil(1.0) == 1.0);
    /*
     * And 2.5, 1.5
     */
    ASSERT(__ctOS_ceil(1.5) == 2.0); 
    ASSERT(__ctOS_ceil(2.5) == 3.0);
    /*
     * Create a double where the high mantissa is completey filled, but the low mantissa is zero
     * This represents the decimal number 7.999996 (roughly) and binary 111.111111111111111111
     */
    __ieee = (__ieee754_double_t*) (&x);
    __ieee->sign = 0;
    __ieee->mlow = 0;
    __ieee->mhigh = 0xFFFFF;
    __ieee->exp = BIAS + 2;
    ASSERT(__ctOS_ceil(x) == 8.0);
    /*
     * Now do a double where the low mantissa has a bit set as well
     */
    __ieee->mlow = 0x80000000;
    ASSERT(__ctOS_ceil(x) == 8.0);
    /*
     * Finally do a negative number
     */
    __ieee->sign = 1;
    ASSERT(__ctOS_ceil(x) == -7.0);
    /*
     * and zero
     */
    ASSERT(0.0 == __ctOS_ceil(0.0));
    return 0;
}

/*
 * Testcase 5: test special cases for ceil:
 * 0.0
 * inf
 * nan
 */
int testcase5() {
    double inf = 1.0 / 0.0;
    double nan = sqrt(-1.0);
    ASSERT(__ctOS_isinf(inf));
    ASSERT(__ctOS_isinf(__ctOS_ceil(inf)));
    ASSERT(0.0 == __ctOS_ceil(0.0));
    ASSERT(__ctOS_isnan(__ctOS_ceil(nan)));
    return 0;
}

/*
 * Test floor
 */
int testcase6() {
    double x;
    __ieee754_double_t* __ieee;
    /*
     * Try 0.5
     */
    ASSERT(__ctOS_floor(0.5) == 0.0);
    /*
     * Try 1.0
     */
    ASSERT(__ctOS_floor(1.0) == 1.0);
    /*
     * And 2.5, 1.5
     */
    ASSERT(__ctOS_floor(1.5) == 1.0); 
    ASSERT(__ctOS_floor(2.5) == 2.0);
    /*
     * Create a double where the high mantissa is completey filled, but the low mantissa is zero
     * This represents the decimal number 7.999996 (roughly) and binary 111.111111111111111111
     */
    __ieee = (__ieee754_double_t*) (&x);
    __ieee->sign = 0;
    __ieee->mlow = 0;
    __ieee->mhigh = 0xFFFFF;
    __ieee->exp = BIAS + 2;
    ASSERT(__ctOS_floor(x) == 7.0);
    /*
     * Now do a double where the low mantissa has a bit set as well
     */
    __ieee->mlow = 0x80000000;
    ASSERT(__ctOS_floor(x) == 7.0);
    /*
     * Finally do a negative number
     */
    __ieee->sign = 1;
    ASSERT(__ctOS_floor(x) == -8.0);
    /*
     * and zero
     */
    ASSERT(0.0 == __ctOS_floor(0.0));
    return 0;
}

/*
 * Test binary logarithm - no reduction, i.e. the argument is
 * already in the range 1...2
 */
int testcase7() {
    double x = 1.0;
    double y1, y2;
    double error = 0.0;
    double errorat = 0;
    for (int i = 0; i < 999; i++) {
        y1 = __ctOS_log2(x);
        y2 = log2(x);
        if (fabs(y1 - y2) > error) {
            error = fabs(y1-y2);
            errorat = x;
        }
        x = x + 0.001;
    }
    ASSERT(error < 1e-15);
    return 0;
}

/*
 * Test binary logarithm - reduction
 */
int testcase8() {
    double x = 5.0;
    double y1, y2;
    double error = 0.0;
    double errorat = 0;
    for (int i = 0; i < 999; i++) {
        y1 = __ctOS_log2(x);
        y2 = log2(x);
        if (fabs(y1 - y2) > error) {
            error = fabs(y1-y2);
            errorat = x;
        }
        x = x + 0.001;
    }
    ASSERT(error < 1e-15);
    return 0;
}

/*
 * Test binary logarithm - special cases
 */
int testcase9() {
    ASSERT(0 == __ctOS_log2(1.0));
    ASSERT(__ctOS_inf(__ctOS_log2(0.0)));
    ASSERT(__ctOS_inf(log2(0.0)));
    ASSERT(__ctOS_isnan(__ctOS_log2(-1.0)));
    ASSERT(__ctOS_isnan(__ctOS_log2(__ctOS_nan())));
    ASSERT(__ctOS_isinf(__ctOS_log2(__ctOS_inf())));
    ASSERT(__ctOS_isnan(__ctOS_log2(-1.0*__ctOS_inf())));
    return 0;
}

/*
 * Testcase 10
 * exp2 - no range reduction
 */
int testcase10() {
    double epsilon = 1e-10;
    /*
     * exp2(0.5)
     */
    double x = 0.5;
    double y = __ctOS_exp2(x);
    ASSERT(fabs(y - exp2(x)) < epsilon);
    /*
     * exp2(-0.5)
     */
    x = -0.5;
    y = __ctOS_exp2(x);
    ASSERT(fabs(y - exp2(x)) < epsilon);
    /*
     * exp2(1.0)
     */
    x = 1.0;
    y = __ctOS_exp2(x);
    ASSERT(y == 2.0);
    /*
     * exp2(-1.0)
     */
    x = -1.0;
    y = __ctOS_exp2(x);
    ASSERT(y == 0.5); 
    return 0;
}

/*
 * Testcase 11
 * exp2 - outside of kernel range
 */
int testcase11() {
    double epsilon = 1e-22;
    double x = 1.5;
    double y = __ctOS_exp2(x);
    ASSERT(fabs(y - exp2(x)) < epsilon);
    x = 15.5;
    y = __ctOS_exp2(x);
    ASSERT(fabs(y - exp2(x)) < epsilon);
    x = 55.5;
    y = __ctOS_exp2(x);
    ASSERT(fabs(y - exp2(x)) < epsilon);
    x = 100.5;
    y = __ctOS_exp2(x);
    ASSERT(fabs(y - exp2(x)) < epsilon);
    x = 2000.5;
    y = __ctOS_exp2(x);
    ASSERT(__ctOS_inf(y));
    return 0;
}


/*
 * Testcase 12
 * exp
 */
int testcase12() {
    double epsilon = 1e-12;
    double x = 1.5;
    double y = __ctOS_exp(x);
    ASSERT(fabs(y - exp(x)) < epsilon);
    return 0;
}

/*
 * Testcase 13
 * cos - no reduction
 */
int testcase13() {
    double epsilon = 1e-7;
    double x = 0.0;
    double y = __ctOS_cos(x);
    ASSERT(fabs(y - cos(x)) < epsilon);
    x = 0.1;
    y = __ctOS_cos(x);
    ASSERT(fabs(y - cos(x)) < epsilon);
    x = 0.2;
    y = __ctOS_cos(x);
    ASSERT(fabs(y - cos(x)) < epsilon);
    x = 1.5;
    y = __ctOS_cos(x);
    ASSERT(fabs(y - cos(x)) < epsilon);
    return 0;
}

/*
 * Testcase 14
 * cos - reduction
 */
int testcase14() {
    double x = 0.0;
    double epsilon = 1e-6;
    double y;
    double error;
    for (int i = 0; i < 100; i++) {
        y = __ctOS_cos(x);
        error = fabs(y - cos(x));
        ASSERT(error < epsilon);
        x = x + 0.1;
    }
    return 0;
}

/*
 * Testcase 15
 * sin
 */
int testcase15() {
    double x = 0.0;
    double epsilon = 1e-6;
    double y;
    double error;
    for (int i = 0; i < 100; i++) {
        y = __ctOS_sin(x);
        error = fabs(y - sin(x));
        ASSERT(error < epsilon);
        x = x + 0.1;
    }
    return 0;
}

/*
 * Testcase 16
 * tan - no reduction, i.e. argument in range
 */
int testcase16() {
    double x = 0.01;
    double epsilon = 1e-6;
    double y;
    double error;
    for (int i = 0; i < 100; i++) {
        y = __ctOS_tan_kernel(x);
        error = fabs(y - tan(x));
        ASSERT(error < epsilon);
        x = x + 0.1;
    }
    return 0;
}

/*
 * Testcase 17
 * tan - reduction
 */
int testcase17() {
    double x = 0.0;
    double epsilon = 1e-5;
    double y;
    double error;
    for (int i = 0; i < 100; i++) {
        y = __ctOS_tan(x);
        error = fabs(y - tan(x));
        ASSERT(error < epsilon);
        x = x + 0.1;
    }
    return 0;
}

/*
 * Testcase 18
 * cosh 
 */
int testcase18() {
    double x = 0.0;
    double epsilon = 1e-5;
    double y;
    double error;
    for (int i = 0; i < 100; i++) {
        y = __ctOS_cosh(x);
        error = fabs(y - cosh(x));
        ASSERT(error < epsilon);
        x = x + 0.1;
    }
    return 0;
}

/*
 * Testcase 19
 * sinh 
 */
int testcase19() {
    double x = 0.0;
    double epsilon = 1e-5;
    double y;
    double error;
    for (int i = 0; i < 100; i++) {
        y = __ctOS_sinh(x);
        error = fabs(y - sinh(x));
        ASSERT(error < epsilon);
        x = x + 0.1;
    }
    return 0;
}

/*
 * Testcase 20
 * tanh 
 */
int testcase20() {
    double x = 0.0;
    double epsilon = 1e-5;
    double y;
    double error;
    for (int i = 0; i < 100; i++) {
        y = __ctOS_tanh(x);
        error = fabs(y - tanh(x));
        ASSERT(error < epsilon);
        x = x + 0.1;
    }
    return 0;
}

/*
 * Testcase 21
 * sqrt kernel
 */
int testcase21() {
    double epsilon = 1e-50;
    double x = 0.5;
    double y;
    double error;
    for (int i = 0; i < 100; i++) {
        y = __ctOS_sqrt_kernel(x);
        error = fabs(y - sqrt(x));
        ASSERT(error < epsilon);
        x = x + 0.005;
    }
    return 0;
}


/*
 * Testcase 22
 * sqrt 
 */
int testcase22() {
    double epsilon = 1e-15;
    double x ;
    double y;
    double error;
    /*
     * First we take a look at the range between 2 and 12
     */
    x = 2.0;
    for (int i = 0; i < 200; i++) {
        y = __ctOS_sqrt(x);
        error = fabs(y - sqrt(x));
        ASSERT(error < epsilon);
        x = x + 0.05;
    }
    /*
     * Now we do the more difficult part close to zero
     */
    x = 0.0;
    for (int i = 0; i < 200; i++) {
        y = __ctOS_sqrt(x);
        error = fabs(y - sqrt(x));
        ASSERT(error < epsilon);
        x = x + 0.000001;
    } 
    /*
     * Now do a few special cases
     */
    ASSERT(isnan(__ctOS_sqrt(-1.0)));
    ASSERT(isnan(__ctOS_sqrt(__ctOS_nan())));
    ASSERT(isinf(__ctOS_sqrt(1.0 / 0.0)));
    return 0;
}


/*
 * Testcase 23
 * atan2
 */
int testcase23() {
    double epsilon = 1e-15;
    double x = 0.0;
    double y;
    double error;
    for (int i = 0; i < 1000; i++) {
        y = __ctOS_atan2(x, 1.0);
        error = fabs(y - atan(x));
        // printf("x = %f      y = %f        error = %e\n", x, y, error);
        ASSERT(error < epsilon);
        y = __ctOS_atan2(x, 3.0);
        error = fabs(y - atan2(x, 3.0));
        ASSERT(error < epsilon);
        x = x + 0.5;
    }
    return 0;
}


int main() {
    // print_ieee(1.5);
    // print_ieee(sqrt(-1.0));
    INIT;
    RUN_CASE(1);
    RUN_CASE(2);
    RUN_CASE(3);
    RUN_CASE(4);
    RUN_CASE(5);
    RUN_CASE(6);
    RUN_CASE(7);
    RUN_CASE(8);
    RUN_CASE(9);
    RUN_CASE(10);
    RUN_CASE(11);
    RUN_CASE(12);
    RUN_CASE(13);
    RUN_CASE(14);
    RUN_CASE(15);
    RUN_CASE(16);
    RUN_CASE(17);
    RUN_CASE(18);
    RUN_CASE(19);
    RUN_CASE(20);
    RUN_CASE(21);
    RUN_CASE(22);
    RUN_CASE(23);
    END;
}

