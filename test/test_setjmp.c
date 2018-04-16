/*
 * test_setjmp.c
 *
 *  Created on: Jan 20, 2012
 *      Author: chr
 */

#include <string.h>
#include <stdio.h>
#include "kunit.h"

typedef int __ctOS_jmp_buf[141];
extern int __ctOS_setjmp(__ctOS_jmp_buf);
extern void __ctOS_longjmp(__ctOS_jmp_buf, int);

/* Testcase 1
 * Tested function: setjmp
 */
int testcase1() {
    __ctOS_jmp_buf jmp_buf;
    ASSERT(0 == __ctOS_setjmp(jmp_buf));
    return 0;
}


/* Testcase 2
 * Tested function: longjmp
 */
int testcase2() {
    int rc;
    int i;
    int flag = 0;
    double value;
    __ctOS_jmp_buf __attribute__ ((aligned(256))) jmp_buf;
    memset((void*) jmp_buf, 0, sizeof(jmp_buf));
    /*
     * Do some floating point arithmetic to put the FPU into a non-trivial state
     */
    value = 2.5;
    value = value * value;
    rc = __ctOS_setjmp(jmp_buf);
#if 0
    if (0 == rc) {
        for (i = 0; i < sizeof(jmp_buf) / sizeof(unsigned int); i++) {
            if (0 == (i % 8))
                printf("\n%04x   ", i);
            printf("%08x  ", jmp_buf[i]);
        }
    }
#endif
    if (0 == rc) {
        /*
         * This is the path which we take first
         */
        ASSERT(0 == flag);
        flag++;
        __ctOS_longjmp(jmp_buf, 1);
        /*
         * We should never get to this point
         */
        ASSERT(0);
    }
    ASSERT(1 == rc);
    ASSERT(1 == flag);
    /*
     * Is value still correct?
     */
    ASSERT((double) 6.25 == value);
    return 0;
}

/* Testcase 3
 * Tested function: longjmp
 * Test that if 0 is passed as second parameter to longjmp, this becomes 1
 */
int testcase3() {
    int rc;
    int flag = 0;
    __ctOS_jmp_buf jmp_buf;
    rc = __ctOS_setjmp(jmp_buf);
    if (0==rc) {
        ASSERT(0==flag);
        flag = 1;
        __ctOS_longjmp(jmp_buf, 0);
        /*
         * We should never get to this point
         */
        ASSERT(0);
    }
    ASSERT(1==rc);
    ASSERT(1==flag);
    return 0;
}


int main() {
    INIT;
    RUN_CASE(1);
    RUN_CASE(2);
    RUN_CASE(3);
    END;
}
