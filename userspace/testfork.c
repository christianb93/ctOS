/*
 * testfork.c
 *
 * This module contains a few tests which fork off several processes which are then run in parallel. The idea is to create
 * as much parallel load as possible, starting with simple tasks and then moving on to more complex tasks, to test for race
 * conditions and deadlocks on SMP systems
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

/*
 * These constants determine the number of parallel processes used for
 * the individual test cases
 */
#define TC1_PROCS 100
#define TC2_PROCS 600
#define TC3_PROCS 100
#define TC4_PROCS 100
#define TC5_PROCS 100
#define TC6_PROCS 16
#define TC7_PROCS 100
#define TC8_PROCS 16
#define TC9_PROCS 128

static int signal_received = 0;


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
 * Testcase 1: fork off a given number of processes and wait for them. Each individual process only executes
 * a few integer arithmetic instructions to consume some CPU time
 */
int testcase1() {
    int pid[TC1_PROCS];
    int i;
    int j;
    int a;
    int stat;
    int rc;
    for (i = 0;i < TC1_PROCS; i++) {
        pid[i] = fork();
        if (0 == pid[i]) {
            /*
             * Child. Execute a few instructions and exit
             */
            for (j=0 ; j < 10000000; j++) {
                a = j / 31415141;
                a = a * 4983098745;
            }
            _exit(0);
        }
        if (pid[i] < 0) {
            printf("Error during fork\n");
            _exit(1);
        }
    }
    /*
     * The code below this line is only run for the parent
     */
    for (i = 0;i < TC1_PROCS; i++) {
        rc = waitpid(pid[i], &stat, 0);
        if (-1 == rc) {
            printf("Error while waiting for process %d\n", pid[i]);
            ASSERT(0);
        }
        ASSERT(0==stat);
    }
    return 0;
}

/*
 * Testcase 2: fork off a given number of processes and wait for them. Each individual process exits immediately
 */
int testcase2() {
    int pid[TC2_PROCS];
    int i;
    int stat;
    int rc;
    for (i = 0;i < TC2_PROCS; i++) {
        pid[i] = fork();
        if (0 == pid[i]) {
            /*
             * Child. Exit
             */
            _exit(0);
        }
        if (pid[i] < 0) {
            printf("Error during fork\n");
            _exit(1);
        }
    }
    /*
     * The code below this line is only run for the parent
     */
    for (i = 0;i < TC2_PROCS; i++) {
        rc = waitpid(pid[i], &stat, 0);
        if (-1 == rc) {
            printf("Error while waiting for process %d\n", pid[i]);
            ASSERT(0);
        }
        ASSERT(0 == stat);
    }
    return 0;
}


/*
 * Testcase 3: fork off a given number of processes and wait for them. Each individual process
 * reads from the file /hello and compares the bytes read to the result of a previous read done
 * by the parent
 */
int testcase3() {
    int pid[TC3_PROCS];
    int i;
    int j;
    int stat;
    int rc;
    int fd;
    char read_buffer[100];
    char comp_buffer[100];
    /*
     * Read from file and set up comp_buffer as basis for later
     * comparison
     */
    fd = open("/hello", 0);
    if (fd <=0) {
        printf("Error, could not open file /hello\n");
        _exit(1);
    }
    memset(comp_buffer, 0, 100);
    rc = read(fd, comp_buffer, 100);
    if (rc<=0) {
        printf("Error, could not open file /hello\n");
        _exit(1);
    }
    for (i = 0;i < TC3_PROCS; i++) {
        pid[i] = fork();
        if (0 == pid[i]) {
            /*
             * Child. Read 100 bytes from /hello and exit
             */
            fd = open("/hello", 0);
            if (fd<0) {
                printf("Error: process %d could not open file /hello\n", getpid());
                _exit(1);
            }
            memset(read_buffer, 0, 100);
            rc = read(fd, read_buffer, 100);
            if (rc<=0) {
                printf("Error: process %d could not read from file /hello\n", getpid());
                _exit(1);
            }
            /*
             * Compare
             */
            for (j = 0; j < 100; j++) {
                ASSERT(comp_buffer[0] == read_buffer[0]);
            }
            close(fd);
            _exit(0);
        }
        if (pid[i] < 0) {
            printf("Error during fork\n");
            _exit(1);
        }
    }
    /*
     * The code below this line is only run for the parent
     */
    for (i = 0;i < TC3_PROCS; i++) {
        rc = waitpid(pid[i], &stat, 0);
        if (-1 == rc) {
            printf("Error while waiting for process %d\n", pid[i]);
            ASSERT(0);
        }
        ASSERT(0==stat);
    }
    return 0;
}

/*
 * Testcase 4: fork off a given number of processes and wait for them. Each individual process
 * again forks off a child and waits for it
 */
int testcase4() {
    int pid[TC4_PROCS];
    int __pid;
    int i;
    int stat;
    int rc;
    for (i = 0;i < TC4_PROCS; i++) {
        pid[i] = fork();
        if (0 == pid[i]) {
            /*
             * Child. Fork off another child and wait
             */
            __pid = fork();
            if (__pid) {
                rc = waitpid(__pid, &stat, 0);
                if (-1 == rc) {
                    printf("Error while waiting for process %d\n", pid[i]);
                    ASSERT(0);
                }
                ASSERT(0 == stat);
            }
            if (0 == __pid) {
                _exit(0);
            }
            if (__pid < 0) {
                printf("Error during fork\n");
                _exit(1);
            }
            /*
             * exit
             */
            _exit(0);
        }
        if (pid[i] < 0) {
            printf("Error during fork\n");
            _exit(1);
        }
    }
    /*
     * The code below this line is only run for the parent
     */
    for (i = 0;i < TC4_PROCS; i++) {
        rc = waitpid(pid[i], &stat, 0);
        if (-1 == rc) {
            printf("Error while waiting for process %d\n", pid[i]);
            ASSERT(0);
        }
        ASSERT(0==stat);
    }
    return 0;
}

/*
 * Testcase 5: fork off a given number of processes and wait for them. Each individual process will then use lseek
 * on a shared file descriptor to advance the cursor of the file by one.
 */
int testcase5() {
    int pid[TC5_PROCS];
    int i;
    int stat;
    int rc;
    int fd;
    /*
     * Open test file
     */
    fd = open("/hello", 0);
    ASSERT(fd>=0);
    for (i = 0;i < TC5_PROCS; i++) {
        pid[i] = fork();
        if (0 == pid[i]) {
            /*
             * Child. Advance file position by one
             */
            ASSERT(lseek(fd, 1, SEEK_CUR) >=0);
            _exit(0);

        }
        if (pid[i] < 0) {
            printf("Error during fork\n");
            _exit(1);
        }
    }
    /*
     * The code below this line is only run for the parent
     */
    for (i = 0;i < TC5_PROCS; i++) {
        rc = waitpid(pid[i], &stat, 0);
        if (-1 == rc) {
            printf("Error while waiting for process %d\n", pid[i]);
            ASSERT(0);
        }
        ASSERT(0==stat);
    }
    /*
     * Verify that file position is correctly set, i.e. is the number of spawned
     * processes
     */
    ASSERT(TC5_PROCS == lseek(fd, 0, SEEK_CUR));
    /*
     * Close test file again
     */
    close(fd);
    return 0;
}

/*
 * Testcase 6: fork off a given number of processes and wait for them. Each individual process N will then open a shared
 * test file, position itself at byte N and write N at this position
 */
int testcase6() {
    int pid[TC6_PROCS];
    unsigned short i;
    int stat;
    int rc;
    int fd;
    int j;
    unsigned int byte;
    /*
     * Create test file
     */
    fd = open("/tmp_tc6", O_RDWR+O_TRUNC+O_CREAT, S_IRWXU);
    ASSERT(fd>=0);
    close(fd);
    for (i = 0;i < TC6_PROCS; i++) {
        pid[i] = fork();
        if (0 == pid[i]) {
            /*
             * Child. Open test file
             */
            fd = open("/tmp_tc6", O_RDWR);
            ASSERT(fd>=0);
            for (j = 0; j < 100; j++) {
            /*
             * advance to position j*TC6_PROCS+i
             */
            ASSERT(j*TC6_PROCS+i == lseek(fd, j*TC6_PROCS+i, SEEK_SET));
            /*
             * and write i
             */
            ASSERT(1 == write(fd, (void*) &i, 1));
            }
            /*
             * close file again
             */
            close(fd);
            _exit(0);

        }
        if (pid[i] < 0) {
            printf("Error during fork\n");
            _exit(1);
        }
    }
    /*
     * The code below this line is only run for the parent
     */
    for (i = 0;i < TC6_PROCS; i++) {
        rc = waitpid(pid[i], &stat, 0);
        if (-1 == rc) {
            printf("Error while waiting for process %d\n", pid[i]);
            ASSERT(0);
        }
        ASSERT(0==stat);
    }
    /*
     * Now open the file again and check that the content is correct
     */
    fd = open("/tmp_tc6", O_RDONLY);
    ASSERT(fd>=0);
    i = 0;
    byte = 0;
    while (1 == read(fd, &byte, 1)) {
        ASSERT(byte == (i % TC6_PROCS));
        i++;
        byte = 0;
    }
    close(fd);
    return 0;
}

/*
 * Testcase 7: fork off a given number of processes and wait for them. Each individual process only executes
 * the time system call in a loop several hundred times
 */
int testcase7() {
    int pid[TC7_PROCS];
    int i;
    int j;
    int stat;
    int rc;
    for (i = 0;i < TC7_PROCS; i++) {
        pid[i] = fork();
        if (0 == pid[i]) {
            /*
             * Child. Execute a time system call
             */
            for (j=0 ; j < 500; j++) {
                time(0);
            }
            _exit(0);
        }
        if (pid[i] < 0) {
            printf("Error during fork\n");
            _exit(1);
        }
    }
    /*
     * The code below this line is only run for the parent
     */
    for (i = 0;i < TC7_PROCS; i++) {
        rc = waitpid(pid[i], &stat, 0);
        if (-1 == rc) {
            printf("Error while waiting for process %d\n", pid[i]);
            ASSERT(0);
        }
        ASSERT(0==stat);
    }
    return 0;
}

/*
 * Testcase 8: fork off a given number of processes and wait for them. Each individual process N will then open a shared
 * test file and read from it
 */
int testcase8() {
    int pid[TC8_PROCS];
    unsigned short i;
    int stat;
    int rc;
    int fd;
    int j;
    unsigned int byte;
    /*
     * Create test file
     */
    fd = open("/tmp_tc8", O_RDWR+O_TRUNC+O_CREAT, S_IRWXU);
    ASSERT(fd>=0);
    byte = 0;
    for (i = 0; i < 100 * TC8_PROCS; i++) {
        byte = (i % TC8_PROCS);
        ASSERT(1 == write(fd, (void*) &byte, 1));
    }
    close(fd);
    /*
     * Start children
     */
    for (i = 0;i < TC8_PROCS; i++) {
        pid[i] = fork();
        if (0 == pid[i]) {
            /*
             * Child. Open test file
             */
            fd = open("/tmp_tc8", O_RDWR);
            ASSERT(fd>=0);
            for (j = 0; j < 100; j++) {
                /*
                 * advance to position j*TC8_PROCS+i
                 */
                ASSERT(j*TC8_PROCS+i == lseek(fd, j*TC8_PROCS+i, SEEK_SET));
                /*
                 * and read
                 */
                byte = 0;
                ASSERT(1 == read(fd, &byte, 1));
                ASSERT(byte == i);
            }
            /*
             * close file again
             */
            close(fd);
            _exit(0);

        }
        if (pid[i] < 0) {
            printf("Error during fork\n");
            _exit(1);
        }
    }
    /*
     * The code below this line is only run for the parent
     */
    for (i = 0;i < TC8_PROCS; i++) {
        rc = waitpid(pid[i], &stat, 0);
        if (-1 == rc) {
            printf("Error while waiting for process %d\n", pid[i]);
            ASSERT(0);
        }
        ASSERT(0==stat);
    }
    return 0;
}

/*
 * Signal handler for testcase 9
 */
static void tc9_signal_handler(int sig_no) {
    if (SIGUSR1 == sig_no)
        signal_received = 1;
}

/*
 * Testcase 9: fork off a given number of processes and wait for them. Each individual process forks off another child and
 * waits until a signal is received by this child
 */
int testcase9() {
    int pid[TC9_PROCS];
    int child_pid;
    int i;
    int j;
    int a;
    pid_t parent_pid;
    sigset_t sigmask;
    int stat;
    int rc;
    for (i = 0;i < TC9_PROCS; i++) {
        pid[i] = fork();
        if (0 == pid[i]) {
            /*
             * Child. Install signal handler, then fork off another child
             */
            ASSERT(SIG_ERR != signal(SIGUSR1, tc9_signal_handler));
            sigemptyset(&sigmask);
            sigaddset(&sigmask, SIGUSR1);
            sigprocmask(SIG_UNBLOCK, &sigmask, 0);
            signal_received = 0;
            parent_pid = getpid();
            child_pid = fork();
            if (child_pid < 0)
                ASSERT(0);
            if (0 == child_pid) {
                /*
                 * Child of child. Use some CPU time, send signal and exit
                 */
                for (j=0 ; j < 10000000; j++) {
                    a = j / 31415141;
                    a = a * 4983098745;
                }
                kill(parent_pid, SIGUSR1);
                _exit(0);
            }
            else {
                /*
                 * Parent process. Wait until signal flag is set
                 */
                while (0 == signal_received);
                /*
                 * Then collect child
                 */
                rc = waitpid(child_pid, &stat, 0);
                ASSERT(child_pid == rc);
                ASSERT(0 == stat);
                _exit(0);
            }
            _exit(0);
        }
        if (pid[i] < 0) {
            printf("Error during fork\n");
            _exit(1);
        }
    }
    /*
     * The code below this line is only run for the parent
     */
    for (i = 0;i < TC9_PROCS; i++) {
        rc = waitpid(pid[i], &stat, 0);
        if (-1 == rc) {
            printf("Error while waiting for process %d\n", pid[i]);
            ASSERT(0);
        }
        ASSERT(0 == stat);
    }
    return 0;
}

int main(int argc, char** argv) {
    int iterations;
    int count;
    if (2 == argc) {
        iterations = strtol(argv[1], 0, 10);
    }
    else
        iterations = 1;
    INIT;
    for (count = 0; count < iterations; count++) {
        if (count > 0) {
            printf("\n---------------------------------------------\n");
            printf("Starting pass %d\n", count + 1);
            printf("---------------------------------------------\n\n");
        }
        RUN_CASE(1);
        RUN_CASE(2);
        RUN_CASE(3);
        RUN_CASE(4);
        RUN_CASE(5);
        RUN_CASE(6);
        RUN_CASE(7);
        RUN_CASE(8);
        RUN_CASE(9);
    }
    END;
}


