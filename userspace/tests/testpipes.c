/*
 * test_pipes.c
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <limits.h>
#include <stdlib.h>
#include <fcntl.h>

/*
 * Signal handler for SIGPIPE
 */
static int sigpipe_caught = 0;
void sigpipe_handler(int signo) {
    if (SIGPIPE==signo)
        sigpipe_caught = 1;
}

/*
 * Signal handler for SIGUSR1
 */
static int sigusr1_caught = 0;
void sigusr1_handler(int signo) {
    if (SIGUSR1==signo)
        sigusr1_caught = 1;
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
 * Testcase 1: create a pipe and close all file descriptors again
 */
int testcase1() {
    int fd[2];
    ASSERT(0==pipe(fd));
    ASSERT(fd[0]);
    ASSERT(fd[1]);
    close(fd[0]);
    close(fd[1]);
    return 0;
}

/*
 * Testcase 2: write to a pipe and read from it within the same thread
 */
int testcase2() {
    int fd[2];
    char buffer = 'a';
    ASSERT(0==pipe(fd));
    ASSERT(fd[0]);
    ASSERT(fd[1]);
    ASSERT(1==write(fd[1], &buffer, 1));
    buffer = '0';
    ASSERT(1==read(fd[0], &buffer, 1));
    ASSERT('a'==buffer);
    close(fd[0]);
    close(fd[1]);
    return 0;
}

/*
 * Testcase 3: write to a pipe and read from it within different processes
 * Test the case that we have written before we read, i.e. we do not have to wait
 */
int testcase3() {
    int fd[2];
    int pid;
    int status;
    char buffer = 'a';
    ASSERT(0==pipe(fd));
    ASSERT(fd[0]);
    ASSERT(fd[1]);
    /*
     * Now fork off another process
     */
    pid = fork();
    if (0==pid) {
        /*
         * This is the child
         */
        ASSERT(1==write(fd[1], &buffer, 1));
        while(1);
    }
    else {
        /*
         * This is the parent. Give child some time to write
         */
         sleep(1);
         buffer = '0';
         ASSERT(1==read(fd[0], &buffer, 1));
         ASSERT('a'==buffer);
         close(fd[0]);
         close(fd[1]);
         kill(pid, SIGKILL);
         waitpid(pid, &status, 0);
    }
    return 0;
}

/*
 * Testcase 4: write to a pipe and read from it within different processes
 * Test the case that we try to read before we have written, i.e. we have to wait
 */
int testcase4() {
    int fd[2];
    int pid;
    int status;
    char buffer = 'a';
    ASSERT(0==pipe(fd));
    ASSERT(fd[0]);
    ASSERT(fd[1]);
    /*
     * Now fork off another process
     */
    pid = fork();
    if (0==pid) {
        /*
         * This is the child. Give parent some time to read,
         * then write to pipe
         */
        sleep(1);
        ASSERT(1==write(fd[1], &buffer, 1));
        while(1) {
            sleep(1);
        }
    }
    else {
        /*
         * This is the parent.
         */
         buffer = '0';
         ASSERT(1==read(fd[0], &buffer, 1));
         ASSERT('a'==buffer);
         close(fd[0]);
         close(fd[1]);
         kill(pid, SIGKILL);
         waitpid(pid, &status, 0);
    }
    return 0;
}

/*
 * Testcase 5: write to a pipe and read from it within different processes
 * Test the case that we try to write to a broken pipe
 */
int testcase5() {
    int fd[2];
    int pid;
    int status;
    char buffer = 'a';
    ASSERT(0==pipe(fd));
    ASSERT(fd[0]);
    ASSERT(fd[1]);
    /*
     * Now fork off another process
     */
    pid = fork();
    if (0==pid) {
        /*
         * This is the child. Close both ends of pipe immediately
         */
        close(fd[1]);
        close(fd[0]);
        while(1) {
            sleep(1);
        }
    }
    else {
        /*
         * This is the parent. Install signal handler for SIGPIPE
         */
        if (SIG_ERR==signal(SIGPIPE, sigpipe_handler)) {
            printf("Could not install signal handler for signal %d\n", SIGPIPE);
        }
         /*
         * Close reading end as well,
         * then wait until child has closed reading end and try to write
         */
        close(fd[0]);
        buffer = '0';
        sleep(1);
        sigpipe_caught = 0;
        ASSERT(-1==write(fd[1], &buffer, 1));
        ASSERT(errno==EPIPE);
        ASSERT(1==sigpipe_caught);
        /*
         * Now kill child
         */
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
    }
    return 0;
}

/*
 * Testcase 6: write to a pipe and read from it within different processes
 * Test the case that we try to read from a broken pipe
 */
int testcase6() {
    int fd[2];
    int pid;
    int status;
    char buffer = 'a';
    ASSERT(0==pipe(fd));
    ASSERT(fd[0]);
    ASSERT(fd[1]);
    /*
     * Now fork off another process
     */
    pid = fork();
    if (0==pid) {
        /*
         * This is the child. Close both ends of pipe immediately
         */
        close(fd[1]);
        close(fd[0]);
        while(1) {
            sleep(1);
        }
    }
    else {
        /*
         * This is the parent. Install signal handler for SIGPIPE
         */
        if (SIG_ERR==signal(SIGPIPE, sigpipe_handler)) {
            printf("Could not install signal handler for signal %d\n", SIGPIPE);
        }
         /*
         * Close writing end as well,
         * then wait until child has closed reading end and try to read
         */
        close(fd[1]);
        buffer = '0';
        sleep(1);
        sigpipe_caught = 0;
        ASSERT(0==read(fd[0], &buffer, 1));
        ASSERT(0==sigpipe_caught);
        /*
         * Now kill child
         */
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
    }
    return 0;
}

/*
 * Testcase 7: write to a pipe and read from it within different processes
 * Test the case that we try to read from a broken pipe, but there is still data in the pipe
 */
int testcase7() {
    int fd[2];
    int pid;
    int status;
    char buffer = 'a';
    ASSERT(0==pipe(fd));
    ASSERT(fd[0]);
    ASSERT(fd[1]);
    /*
     * Now fork off another process
     */
    pid = fork();
    if (0==pid) {
        /*
         * This is the child. Write some data to the pipe,
         * then close writing end
         */
        close(fd[0]);
        ASSERT(1==write(fd[1], &buffer, 1));
        close(fd[1]);
        while(1) {
            sleep(1);
        }
    }
    else {
        /*
         * This is the parent. Install signal handler for SIGPIPE
         */
        if (SIG_ERR==signal(SIGPIPE, sigpipe_handler)) {
            printf("Could not install signal handler for signal %d\n", SIGPIPE);
        }
         /*
         * Close writing end as well,
         * then wait until child has closed reading end and try to read. First read should
         * return a byte, next read should return 0
         */
        close(fd[1]);
        buffer = '0';
        sleep(1);
        sigpipe_caught = 0;
        ASSERT(1==read(fd[0], &buffer, 1));
        ASSERT(0==read(fd[0], &buffer, 1));
        ASSERT(0==sigpipe_caught);
        /*
         * Now kill child
         */
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
    }
    return 0;
}

/*
 * Testcase 8: test redirection. We fork create a pipe and fork off two childs. Child A closes the reading end of the pipe
 * and redirects its stdout to the writing end. Child B closes the writing end and connects its stdin to the reading end.
 * It then reads from the pipe and exits with a status indicating whether it has read the expected value
 */
int testcase8() {
    int fd[2];
    int pidA;
    int pidB;
    int status;
    int rc;
    char buffer = 'a';
    ASSERT(0==pipe(fd));
    ASSERT(fd[0]);
    ASSERT(fd[1]);
    /*
     * Now fork off another process - this will be child A
     */
    pidA = fork();
    if (0==pidA) {
        /*
         * This is child A. Close reading end of pipe
         */
        close(fd[0]);
        /*
         * Next close stdout and do a dup to connect the
         * writing end of the pipe to stdout
         */
        close(STDOUT_FILENO);
        dup(fd[1]);
        close(fd[1]);
        buffer = 'x';
        write(STDOUT_FILENO, &buffer, 1);
        _exit(0);
    }
    else {
        /*
         * This is the parent. Fork off another child
         */
        pidB = fork();
        if (0==pidB) {
            /*
             * This code is executed by the second child. Close writing end of pipe
             */
            close(fd[1]);
            /*
             * Connect stdin to reading end of pipe
             */
            close(STDIN_FILENO);
            dup(fd[0]);
            close(fd[0]);
            buffer = 'y';
            read(STDIN_FILENO, &buffer, 1);
            if (buffer=='x') {
                _exit(0);
            }
            _exit(1);
        }
        else {
            /*
             * Parent. Close both ends of pipe
             */
            close(fd[0]);
            close(fd[1]);
            /*
             * Child A is only writing and should therefore exit first - wait for it
             */
            rc = waitpid(pidA, &status, 0);
            ASSERT(pidA==rc);
            ASSERT(0==status);
            /*
             * Child B should read from pipe and exit as well
             */
            rc = waitpid(pidB, &status, 0);
            ASSERT(pidB==rc);
            ASSERT(0==status);
        }
    }
    return 0;
}

/*
 * Testcase 9: write to a pipe and read from it within different processes
 * Test the case that a pipe breaks while we wait to be able to write and we have
 * already written some data
 *
 */
int testcase9() {
    int fd[2];
    int pid;
    int status;
    int rc;
    char* buffer;
    /*
     * Should be good enough to make a write block also for Linux
     */
    int pipe_capacity = 65536;
    buffer = (char*) malloc(pipe_capacity+1);
    ASSERT(buffer);
    ASSERT(0==pipe(fd));
    ASSERT(fd[0]);
    ASSERT(fd[1]);
    /*
     * Now fork off another process
     */
    pid = fork();
    if (0==pid) {
        /*
         * This is the child. Wait 1 second, then close both ends of pipe
         */
        sleep(1);
        close(fd[1]);
        close(fd[0]);
        while(1) {
            sleep(1);
        }
    }
    else {
        /*
         * This is the parent. Install signal handler for SIGPIPE
         */
        if (SIG_ERR==signal(SIGPIPE, sigpipe_handler)) {
            printf("Could not install signal handler for signal %d\n", SIGPIPE);
        }
         /*
         * Close reading end as well,
         */
        close(fd[0]);
        sigpipe_caught = 0;
        /*
         * This write should block and return with a partial write
         *
         * NOTE: on Linux, SIGPIPE is delivered in this case even though
         * the return code is not -1. I do not know whether this is correct - ctOS
         * does not generate a SIGPIPE here
         */
        rc = write(fd[1], buffer, pipe_capacity+1);
        free(buffer);
        ASSERT(rc>0);
        /*
         * Now kill child
         */
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
    }
    return 0;
}

/*
 * Testcase 10: write to a pipe and read from it within different processes
 * Test the case that the last writer disconnects while we are waiting in a read call
 */
int testcase10() {
    int fd[2];
    int pid;
    int status;
    char buffer = 'a';
    ASSERT(0==pipe(fd));
    ASSERT(fd[0]);
    ASSERT(fd[1]);
    /*
     * Now fork off another process
     */
    pid = fork();
    if (0==pid) {
        /*
         * This is the child. Give parent some time to start the read, then
         * close both ends of pipe
         */
        sleep(1);
        close(fd[1]);
        close(fd[0]);
        while(1) {
            sleep(1);
        }
    }
    else {
        /*
         * This is the parent. Install signal handler for SIGPIPE
         */
        if (SIG_ERR==signal(SIGPIPE, sigpipe_handler)) {
            printf("Could not install signal handler for signal %d\n", SIGPIPE);
        }
         /*
         * Close writing end as well,
         * then try to read. This read should return when the pipe closes
         */
        close(fd[1]);
        buffer = '0';
        sigpipe_caught = 0;
        ASSERT(0==read(fd[0], &buffer, 1));
        ASSERT(0==sigpipe_caught);
        /*
         * Now kill child
         */
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
    }
    return 0;
}

/*
 * Testcase 11: Test that a read is interrupted by a signal and a handler has been installed for this signal.
 * This should return EINTR.
 */
int testcase11() {
    int fd[2];
    struct sigaction sa;
    int pid;
    int ppid;
    int rc;
    char buffer[16];
    int status;
    struct sigaction sa_old;
    /*
     * Create pipe
     */
    ASSERT(0==pipe(fd));
    ASSERT(fd[0]);
    ASSERT(fd[1]);
    /*
     * Install signal handler for SIGUSR1
     */
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = sigusr1_handler;
    ASSERT(0==sigaction(SIGUSR1, &sa, &sa_old));
    /*
     * Now start a second process
     */
    ppid = getpid();
    pid = fork();
    if (0==pid) {
        /*
         * Child. Wait for a second until the parent had time to enter the
         * read systemcall, then send it SIGUSR1
         */
        sleep(1);
        kill(ppid, SIGUSR1);
        /*
         * and exit
         */
        _exit(0);
    }
    /*
     * Parent.
     */
    errno = 0;
    sigusr1_caught = 0;
    rc = read(fd[0], buffer, 16);
    ASSERT(-1==rc);
    ASSERT(1==sigusr1_caught);
    ASSERT(EINTR==errno);
    /*
     * Wait for child
     */
    waitpid(pid, &status, 0);
    /*
     * and restore old signal handler
     */
    ASSERT(0==sigaction(SIGUSR1, &sa_old, 0));
    return 0;
}

/*
 * Testcase 12: Test that a read is interrupted by a signal which is ignored
 * This should not lead to EINTR
 */
int testcase12() {
    int fd[2];
    struct sigaction sa;
    int pid;
    int ppid;
    int rc;
    char write_buffer;
    char read_buffer;
    int status;
    struct sigaction sa_old;
    /*
     * Create pipe
     */
    ASSERT(0==pipe(fd));
    ASSERT(fd[0]);
    ASSERT(fd[1]);
    /*
     * Ignore SIGUSR1
     */
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = SIG_IGN;
    ASSERT(0==sigaction(SIGUSR1, &sa, &sa_old));
    /*
     * Now start a second process
     */
    ppid = getpid();
    pid = fork();
    if (0==pid) {
        /*
         * Child. Wait for a second until the parent had time to enter the
         * read systemcall, then send it SIGUSR1
         */
        sleep(1);
        kill(ppid, SIGUSR1);
        /*
         * Wait for another second, then write 1 byte
         */
        write_buffer = 'a';
        ASSERT(1==write(fd[1], &write_buffer,1));
        /*
         * and exit
         */
        _exit(0);
    }
    /*
     * Parent.
     */
    errno = 0;
    sigusr1_caught = 0;
    read_buffer = 'x';
    rc = read(fd[0], &read_buffer, 16);
    ASSERT(1==rc);
    ASSERT(0==errno);
    ASSERT('a'==read_buffer);
    /*
     * Wait for child
     */
    waitpid(pid, &status, 0);
    /*
     * and restore old signal handler
     */
    ASSERT(0==sigaction(SIGUSR1, &sa_old, 0));
    return 0;
}

/*
 * Testcase 13: write to a pipe and read from it within different processes
 * Test the case that a pipe breaks while we wait to be able to write and we have
 * not yet written any data
 *
 */
int testcase13() {
    int fd[2];
    int pid;
    int status;
    int rc;
    char* buffer;
    /*
     * Should be good enough to make a write block also for Linux
     */
    int pipe_capacity = 65536;
    buffer = (char*) malloc(pipe_capacity+1);
    ASSERT(buffer);
    ASSERT(0==pipe(fd));
    ASSERT(fd[0]);
    ASSERT(fd[1]);
    /*
     * Now fork off another process
     */
    pid = fork();
    if (0==pid) {
        /*
         * This is the child. Wait 1 second, then close both ends of pipe
         */
        sleep(1);
        close(fd[1]);
        close(fd[0]);
        while(1) {
            sleep(1);
        }
    }
    else {
        /*
         * This is the parent. Install signal handler for SIGPIPE
         */
        if (SIG_ERR==signal(SIGPIPE, sigpipe_handler)) {
            printf("Could not install signal handler for signal %d\n", SIGPIPE);
        }
         /*
         * Close reading end as well,
         */
        close(fd[0]);
        sigpipe_caught = 0;
        /*
         * Fill up pipe
         */
        write(fd[1], buffer, pipe_capacity);
        /*
         * This write should block and return with EPIPE
         */
        rc = write(fd[1], buffer, 1);
        free(buffer);
        ASSERT(-1==rc);
        ASSERT(EPIPE==errno);
        ASSERT(1==sigpipe_caught);
        /*
         * Now kill child
         */
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
    }
    return 0;
}

/*
 * Testcase 14: write to a pipe and read from it within different processes
 * Test the case that we try to read before we have written, but do a non-blocking read
 */
int testcase14() {
    int fd[2];
    int pid;
    int status;
    char buffer = 'a';
    ASSERT(0==pipe(fd));
    ASSERT(fd[0]);
    ASSERT(fd[1]);
    /*
     * Now fork off another process
     */
    pid = fork();
    if (0==pid) {
        /*
         * This is the child. Give parent some time to read,
         * then write to pipe
         */
        sleep(1);
        ASSERT(1==write(fd[1], &buffer, 1));
        while(1) {
            sleep(1);
        }
    }
    else {
        /*
         * This is the parent.
         */
         buffer = '0';
         /*
          * Set fd[0] to O_NONBLOCK
          */
         ASSERT(0==fcntl(fd[0], F_SETFL, O_NONBLOCK));
         ASSERT(-1==read(fd[0], &buffer, 1));
         ASSERT(EAGAIN==errno);
         close(fd[0]);
         close(fd[1]);
         kill(pid, SIGKILL);
         waitpid(pid, &status, 0);
    }
    return 0;
}

/*
 * Testcase 15: write to a pipe
 * Test the case that we try to write to a pipe which is full, but do a non-blocking write
 */
int testcase15() {
    int fd[2];
    char buffer = 'a';
    ASSERT(0==pipe(fd));
    ASSERT(fd[0]);
    ASSERT(fd[1]);
    int i;
    int pipe_capacity = PIPE_BUF;
    buffer = '0';
    /*
     * Set fd[1] to O_NONBLOCK
     */
    ASSERT(0==fcntl(fd[1], F_SETFL, O_NONBLOCK));
    /*
     * Fill up pipe by writing pipe_capacity times one byte to it
     */
    for (i=0;i<pipe_capacity;i++)
        write(fd[1], &buffer, 1);
    /*
     * Do a non-blocking write
     */
    ASSERT(-1==write(fd[1], &buffer, 1));
    ASSERT(EAGAIN==errno);
    close(fd[0]);
    close(fd[1]);
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
    RUN_CASE(8);
    RUN_CASE(9);
    RUN_CASE(10);
    RUN_CASE(11);
    RUN_CASE(12);
    RUN_CASE(13);
    RUN_CASE(14);
    RUN_CASE(15);
    END;
}
