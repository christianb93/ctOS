/*
 * test_math.c
 *
 */

#include "kunit.h"
#include "lib/os/mathlib.h"
#include <stdio.h>
#include <math.h>

/*
 * Testcase 1: isinf
 */
int testcase1() {
    double value;
    value = 1.0 / 0.0;
    ASSERT(1 == __ctOS_isinf(value));
    value = 2.1;
    ASSERT(0 == __ctOS_isinf(value));
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
    return 0;
}

int main() {
    INIT;
    RUN_CASE(1);
    RUN_CASE(2);
    END;
}

