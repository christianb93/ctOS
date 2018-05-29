
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

/*
 * A global counter
 */
static int global_counter = 0;

/*
 * Test statistics
 */
static int __failed=0; 
static int __passed=0; 
static int __rc=0;

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
#define INIT  __failed=0; __passed=0; __rc=0 ; \
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
        printf("------------------------------------------\n"); return __rc;

/*
 * Execute a test case
 */
#define RUN_CASE(x) do { __rc= do_test_case(x, testcase##x);  \
        if (__rc) __failed++; else __passed++;} while (0)
            

/*
 * Forward declaration - this is in kunit.o
 */
int do_test_case(int x, int (*testcase)());


/*
 * Forward declarations to exit handlers
 */
void handler1();
void handler2();

/*
 * Testcase 1: register the first exit handler
 */
int testcase1() {
    ASSERT(0 == atexit(handler1));
    return 0;
}

/*
 * Testcase 2: register the second exit handler
 */
int testcase2() {
    ASSERT(0 == atexit(handler2));
    return 0;
}


/*
 * Testcase 1: this is the last exit handler. We simply verify that the
 * global counter is 1 
 */
void handler1() {
    int rc;
    printf("Running testcase 4...");
    rc = 0;
    if (1 != global_counter) {
        __failed++;
        rc = 1;
    }
    else {
        __passed++;
    }
    if (0 == rc)
        printf("ok\n");
    else
        printf("failure\n");
}

/*
 * Testcase 2: this is the first exit handler. We simply verify that the
 * global counter is 0 and increment it
 */
void handler2() {
    int rc = 0;
    printf("Running testcase 3...");    
    if (0 != global_counter) {
        __failed++;
        rc = 1;
    }
    else {
        __passed++;
    }
    global_counter++;
    if (0 == rc)
        printf("ok\n");
    else
        printf("failure\n");    
}


/*
 * The last handler being called - this is therefore the first handler
 * that we register
 */
void final_handler() {
    printf("------------------------------------------\n"); \
    printf("Overall test results (%s):\n", __FILE__); \
    printf("------------------------------------------\n"); \
    printf("Failed: %d  Passed:  %d\n", __failed, __passed); \
    printf("------------------------------------------\n");    
    if (__failed > 0)
        _exit(1);
}

int main() {
    INIT;
    atexit(final_handler);
    /*
     * Run the first test case that will register the exit handler as well
     */
    RUN_CASE(1);
    /*
     * Run the second case that will register the second exit handler
     */
    RUN_CASE(2);
    /*
     * Now call exit(0). This will invoke the first handler.
     * The last handler is doing the checks using END and
     * calls _exit(1) if any of the cases has failed
     */
}