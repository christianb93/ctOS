/*
 * kunit.c
 * Collection of various functions to support unit testing
 */

#include <stdio.h>
#include "lib/sys/types.h"



/* Run a test case
 */
int do_test_case(int x, int (*testcase)()) {
    int rc;
    printf("Running testcase %d...", x);
    fflush(stdout);
    rc = testcase();
    if (0==rc)
        printf("ok\n");
    else
        printf("failure\n");
    return rc;
}
