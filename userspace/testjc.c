/*
 * testjc.c
 *
 * Test job control features
 */

#include <stdio.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

static int sigttin = 0;
void sighandler(int signo) {
    if (SIGTTIN==signo)
        sigttin=1;
}

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

/*
 * Forward declaration - this is in kunit.o
 */
int do_test_case(int x, int (*testcase)());

/*
 * Testcase 1: create a background process which tries to read from stdin and verify that it is stopped by a SIGTTIN signal
 */
int testcase1() {
    pid_t pid;
    int status;
    char c;
    struct sigaction sa;
    sigset_t procmask;
    struct stat mystat;
    /*
     * Check settings for SIGTTIN
     */
    sigaction(SIGTTIN, 0, &sa);
    ASSERT(sa.sa_handler==SIG_DFL);
    sigprocmask(0, 0, &procmask);
    ASSERT(!sigismember(&procmask, SIGTTIN));
    /*
     * Create child
     */
    pid = fork();
    if (pid < 0) {
        printf("Fork failed\n");
        _exit(1);
    }
    if (0==pid) {
        /*
         * Child. Verify settings again
         */
        sigaction(SIGTTIN, 0, &sa);
        ASSERT(sa.sa_handler==SIG_DFL);
        sigprocmask(0, 0, &procmask);
        ASSERT(!sigismember(&procmask, SIGTTIN));
        /*
         * We first put ourselves into the background, then we read from stdin.
         * This should stop the process
         */
        ASSERT(0==setpgid(0,0));
        ASSERT(getpid()==getpgrp());
        /*
         * Verify that we are not owning the terminal and that our input is
         * not a regular file or a pipe
         */
        ASSERT(getpgrp()!=tcgetpgrp(STDIN_FILENO));
        fstat(STDIN_FILENO, &mystat);
        ASSERT(!S_ISREG(mystat.st_mode));
        ASSERT(!S_ISFIFO(mystat.st_mode));
        read(STDIN_FILENO, &c, 1);
        /*
         * When the read returns, exit - this should actually never happen in this testcase
         */
        _exit(1);
    }
    else {
        /*
         * Parent. Wait until child has been stopped
         */
        ASSERT(pid==waitpid(pid, &status, WUNTRACED));
        ASSERT(WIFSTOPPED(status));
        ASSERT(SIGTTIN==WSTOPSIG(status));
        /*
         * Now kill child
         */
        ASSERT(0==kill(pid, SIGKILL));
        ASSERT(pid==waitpid(pid, &status, 0));
        ASSERT(WIFSIGNALED(status));
        ASSERT(WTERMSIG(status)==SIGKILL);
    }
    return 0;
}


/*
 * Testcase 2: create a background process which tries to read from stdin while SIGTTIN is blocked and verify that
 * the read return EIO
 */
int testcase2() {
    pid_t pid;
    int status;
    sigset_t procmask;
    char c;
    /*
     * Create child
     */
    pid = fork();
    if (pid < 0) {
        printf("Fork failed\n");
        _exit(1);
    }
    if (0==pid) {
        /*
         * Child. Block SIGTTIN
         */
        sigemptyset(&procmask);
        sigaddset(&procmask, SIGTTIN);
        ASSERT(0==sigprocmask(SIG_BLOCK, &procmask, 0));
        /*
         * We first put ourselves into the background, then we read from stdin.
         * This should return -1 and errno should be set to EIO afterwards
         */
        ASSERT(0==setpgid(0,0));
        ASSERT(-1==read(STDIN_FILENO, &c, 1));
        ASSERT(EIO==errno);
        _exit(0);
    }
    else {
        /*
         * Parent. Wait until child has exited
         */
        ASSERT(pid==waitpid(pid, &status, 0));
        ASSERT(WIFEXITED(status));
        ASSERT(0==WEXITSTATUS(status));
    }
    return 0;
}

/*
 * Testcase 3: create a background process which tries to read from stdin while a signal handler for SIGTTIN is installed. Verify
 * that the read never returns
 */
int testcase3() {
    pid_t pid;
    int status;
    char c;
    struct sigaction sa;
    /*
     * Create child
     */
    pid = fork();
    if (pid < 0) {
        printf("Fork failed\n");
        _exit(1);
    }
    if (0==pid) {
        /*
         * Child. Install signal handler for SIGTTIN
         */
        sigemptyset(&sa.sa_mask);
        sa.sa_handler = sighandler;
        ASSERT(0==sigaction(SIGTTIN, &sa, 0));
        /*
         * We first put ourselves into the background, then we read from stdin.
         * This should raise a SIGTTIN which is handled by our handler, thus read should
         * return with -EINTR
         */
        ASSERT(0==setpgid(0,0));
        sigttin = 0;
        ASSERT(-1==read(STDIN_FILENO, &c, 1));
        ASSERT(errno==EINTR);
        ASSERT(1==sigttin);
        _exit(0);
    }
    else {
        /*
         * Parent. Wait until child is done
         */
        ASSERT(pid==waitpid(pid, &status, WUNTRACED));
        ASSERT(WIFEXITED(status));
        ASSERT(0==status);
    }
    return 0;
}

/*
 * Testcase 4: create a new process which then establishes a new session.
 */
int testcase4() {
    pid_t pid;
    int status;
    /*
     * Create child
     */
    pid = fork();
    if (pid < 0) {
        printf("Fork failed\n");
        _exit(1);
    }
    if (0 == pid) {
        ASSERT(0 == setsid());
        ASSERT(getpid() == getsid(0));
        _exit(0);
    }
    else {
        /*
         * Parent. Wait until child is done
         */
        ASSERT(pid == waitpid(pid, &status, WUNTRACED));
        ASSERT(WIFEXITED(status));
        ASSERT(0==status);
    }
    return 0;
}

int main() {
    INIT;
    RUN_CASE(1);
    RUN_CASE(2);
    RUN_CASE(3);
    RUN_CASE(4);
    END;
}

