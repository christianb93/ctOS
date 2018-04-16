/*
 * kunit.h
 *
 */

#ifndef _KUNIT_H_
#define _KUNIT_H_

#include "lib/sys/types.h"
/*
 * Macro for assertions in unit test cases
 */
#define ASSERT(x)  do { if (!(x)) { \
                          printf("Assertion %s failed at line %d in %s..", #x, __LINE__, __FILE__ ); \
                          return 1 ;   \
                        } \
                   } while (0)

/*
 * Set up statistics
 */
#define INIT  int __failed=0; int __passed=0; int __rc=0 ; \
              printf("------------------------------------------\n"); \
              printf("Starting unit test %s\n", __FILE__); \
              printf("------------------------------------------\n");

/*
 * Print statistic and return
 */
#define END printf("------------------------------------------\n"); \
            printf("Overall test results (%s):\n", __FILE__); \
            printf("------------------------------------------\n"); \
            printf("Failed: %d  Passed:  %d\n", __failed, __passed); \
            printf("------------------------------------------\n"); \
            return __failed;
/*
 * Execute a test case
 */
#define RUN_CASE(x) do { __rc= do_test_case(x, testcase##x);  \
                         if (__rc) __failed++; else __passed++;} while (0)

int do_test_case(int x, int (*testcase)());

#endif /* _KUNIT_H_ */
