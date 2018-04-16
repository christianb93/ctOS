/*
 * testmisc.c
 *
 *  Miscellanous testcases
 */


#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>
#include <stdlib.h>
#include <setjmp.h>


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
 * Testcase 1: test task switch after using the FPU
 */
int testcase1() {
    double a;
    double b;
    double c;
    a = 2.5;
    b = 2.0;
    int pid;
    int stat;
    int i;
    /*
     * Start a calculation
     */
    c = a*b;
    /*
     * Fork off a new process which will also use the FPU
     */
    pid = fork();
    if (0 == pid) {
        /*
         * Child. Use FPU and exit
         */
        for (i = 0; i < 10000; i++) {
            c = 2.0;
            c = c*a;
            ASSERT(c == 5.0);
        }
        _exit(0);
    }
    /*
     * Sleep to force task switch
     */
    sleep(1);
    /*
     * Resume calculation
     */
    c = c * a;
    ASSERT(12.5 == c);
    /*
     * and wait for task
     */
    waitpid(pid, &stat, 0);
    ASSERT(0 == stat);
    return 0;
}

/*
 * Testcase 2: test execution of a signal handler during an FPU operation
 */
static int volatile __handler_called = 0;
static int volatile __handler_completed = 0;
void myhandler(int signo) {
    double a;
    double b;
    double c;
    __handler_called = 1;
    a = 4.5;
    b = 4.0;
    c = a*b;
    if (18.0 == c)
        __handler_completed = 1;
    return;
}
int testcase2() {
    double a;
    double b;
    double c;
    a = 2.5;
    b = 2.0;
    struct sigaction sa;
    sigset_t sigmask;
    /*
     * Install signal handler
     */
    sa.sa_flags = 0;
    sa.sa_handler = myhandler;
    sigemptyset(&sa.sa_mask);
    ASSERT(0 == sigaction(SIGUSR1, &sa, 0));
    /*
     * Make sure that SIGUSR1 is not blocked
     */
    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGUSR1);
    ASSERT(0 == sigprocmask(SIG_UNBLOCK, &sigmask, 0));
    __handler_called = 0;
    /*
     * Start a calculation
     */
    c = a*b;
    /*
     * Raise signal to execute signal handler
     */
    raise(SIGUSR1);
    ASSERT(__handler_called);
    ASSERT(__handler_completed);
    /*
     * Resume calculation
     */
    c = c * a;
    ASSERT(12.5 == c);
    return 0;
}

/*
 * Testcase 3: combine test cases 2 with a task executing concurrently
 */
int testcase3() {
    double a;
    double b;
    double c;
    a = 2.5;
    b = 2.0;
    struct sigaction sa;
    sigset_t sigmask;
    int pid;
    int i;
    int stat;
    /*
     * Install signal handler
     */
    sa.sa_flags = 0;
    sa.sa_handler = myhandler;
    sigemptyset(&sa.sa_mask);
    ASSERT(0 == sigaction(SIGUSR1, &sa, 0));
    /*
     * Make sure that SIGUSR1 is not blocked
     */
    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGUSR1);
    ASSERT(0 == sigprocmask(SIG_UNBLOCK, &sigmask, 0));
    __handler_called = 0;
    /*
     * Start a calculation
     */
    c = a*b;
    /*
     * Fork off a new process which will also use the FPU
     */
    pid = fork();
    if (0 == pid) {
        /*
         * Child. Use FPU and exit
         */
        for (i = 0; i < 1000000; i++) {
            c = 2.0;
            c = c*a;
            ASSERT(c == 5.0);
        }
        _exit(0);
    }
    /*
     * Raise signal to execute signal handler
     */
    raise(SIGUSR1);
    ASSERT(__handler_called);
    ASSERT(__handler_completed);
    /*
     * Resume calculation
     */
    c = c * a;
    ASSERT(12.5 == c);
    /*
     * and wait for task
     */
    waitpid(pid, &stat, 0);
    ASSERT(0 == stat);
    return 0;
}

/* Testcase 4
 * Do a longjmp
 */
int testcase4() {
    int rc;
    int flag = 0;
    double value;
    jmp_buf __jmp_buf;
    memset((void*) __jmp_buf, 0, sizeof(__jmp_buf));
    /*
     * Do some floating point arithmetic to put the FPU into a non-trivial state
     */
    value = 2.5;
    value = value * value;
    rc = setjmp(__jmp_buf);
    if (0 == rc) {
        /*
         * This is the path which we take first
         */
        ASSERT(0 == flag);
        flag++;
        longjmp(__jmp_buf, 1);
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

/*
 * Main
 */
int main() {
    INIT;
    RUN_CASE(1);
    RUN_CASE(2);
    RUN_CASE(3);
    RUN_CASE(4);
    END;
}
