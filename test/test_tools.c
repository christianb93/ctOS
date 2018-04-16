/*
 * test_tools.c
 *
 */


#include "kunit.h"
#include <stdio.h>
#include <limits.h>

/*
 * Add two unsigned integers and set an error flag if an overflow
 * occurs
 */
unsigned int add_overflow_uu(unsigned int a, unsigned int b, int* overflow) {
    *overflow = (a > ~b) ? 1 : 0;
    return a+b;
}

/*
 * Multiply two 64-bit integers and detect overflow
 */
unsigned long long mult_overflow(unsigned long long a, unsigned long long b, int* overflow) {
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
         * of a plus the number of bits of b exceeds 64, thus it is a "real" overflow
         * and not just due to the algorithm
         */
        if ((a & (1LL << 63)) && (b))
            *overflow = 1;
        a = a << 1;
    }
    return result;
}

/*
 * Testcase 1: add two unsigned integers without overflow
 */
int testcase1() {
    int overflow;
    ASSERT(3==add_overflow_uu(1, 2, &overflow));
    ASSERT(0==overflow);
    return 0;
}

/*
 * Testcase 2: add two unsigned integers with overflow
 */
int testcase2() {
    int overflow;
    add_overflow_uu(0xffffffff, 1, &overflow);
    ASSERT(1==overflow);
    return 0;
}

/*
 * Testcase 3: add two unsigned integers with overflow
 */
int testcase3() {
    int overflow;
    add_overflow_uu(1, 0xffffffff, &overflow);
    ASSERT(1==overflow);
    return 0;
}

/*
 * Testcase 4: add two unsigned integers with overflow
 */
int testcase4() {
    int overflow;
    add_overflow_uu(0xffffffff, 0xffffffff, &overflow);
    ASSERT(1==overflow);
    return 0;
}

/*
 * Testcase 5: add two unsigned integers without overflow
 */
int testcase5() {
    int overflow;
    add_overflow_uu(0xfffffffe, 1, &overflow);
    ASSERT(0==overflow);
    return 0;
}

/*
 * Testcase 6: multiply two integers without overflow
 */
int testcase6() {
    int overflow;
    ASSERT(14==mult_overflow(2, 7, &overflow));
    ASSERT(0==overflow);
    return 0;
}

/*
 * Testcase 7: multiply two integers with overflow
 */
int testcase7() {
    int overflow;
    mult_overflow(ULLONG_MAX, 2, &overflow);
    ASSERT(1==overflow);
    return 0;
}

int main() {
    INIT;
    RUN_CASE(1);
    RUN_CASE(2);
    RUN_CASE(3);
    RUN_CASE(4);
    RUN_CASE(5);
    RUN_CASE(6);
    RUN_CASE(7);
    END;
}
