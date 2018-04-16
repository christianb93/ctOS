/*
 * sigtest.c
 *
 * This program executes a few test cases for signal handling
 *
 */


#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>

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

int last_signal = -1;
int signal_raised[33];
volatile int alarm_raised = 0;
void signal_handler(int sig_no) {
    last_signal = sig_no;
    if (SIGALRM == sig_no)
        alarm_raised = 1;
    signal_raised[sig_no]++;
}

int second_handler_called = 0;
void second_signal_handler(int sig_no) {
    second_handler_called = 1;
    raise(SIGUSR1);
}

/*
 * Testcase 1
 * In this testcase, we install a signal handler and raise a signal to see that the handler is executed
 */
int testcase1() {
    struct sigaction sa;
    sa.sa_flags = 0;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    ASSERT(0==sigaction(SIGUSR1, &sa, 0));
    last_signal = -1;
    ASSERT(0==raise(SIGUSR1));
    ASSERT(SIGUSR1==last_signal);
    return 0;
}

/*
 * Testcase 2
 * In this testcase, we install a signal handler and block the signal. We then verify that the handler is only
 * called once the signal is unblocked again
 */
int testcase2() {
    sigset_t set;
    ASSERT(SIG_ERR!=signal(SIGUSR1, signal_handler));
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    ASSERT(0==sigprocmask(SIG_BLOCK, &set, 0));
    last_signal = -1;
    ASSERT(0==raise(SIGUSR1));
    ASSERT(-1==last_signal);
    ASSERT(0==sigprocmask(SIG_UNBLOCK, &set, 0));
    ASSERT(SIGUSR1==last_signal);
    return 0;
}

/*
 * Testcase 3
 * In this testcase, we install a signal handler for two signals and block both signals. We then verify that when we unblock
 * both signals, both signal handlers are called before the sigprocmask call returns
 */
int testcase3() {
    struct sigaction sa;
    sigset_t set;
    sa.sa_flags = 0;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    ASSERT(0==sigaction(SIGUSR1, &sa, 0));
    ASSERT(0==sigaction(SIGUSR2, &sa, 0));
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    sigaddset(&set, SIGUSR2);
    ASSERT(0==sigprocmask(SIG_BLOCK, &set, 0));
    last_signal = -1;
    ASSERT(0==raise(SIGUSR1));
    ASSERT(0==raise(SIGUSR2));
    ASSERT(-1==last_signal);
    signal_raised[SIGUSR1]=0;
    signal_raised[SIGUSR2]=0;
    ASSERT(0==sigprocmask(SIG_UNBLOCK, &set, 0));
    ASSERT(1==signal_raised[SIGUSR1]);
    ASSERT(1==signal_raised[SIGUSR2]);
    return 0;
}

/*
 * Testcase 4
 * Test nested delivery of signals. Here we install a signal handler which will raise a second signal. We then verify that both
 * handlers have been called
 */
int testcase4() {
    struct sigaction sa;
    sigset_t set;
    sigemptyset(&set);
    ASSERT(0==sigprocmask(SIG_SETMASK, &set, 0));
    sa.sa_flags = 0;
    sa.sa_handler = second_signal_handler;
    sigemptyset(&sa.sa_mask);
    /*
     * We install the second handler for SIGUSR2 and then raise SIGUSR2. This should in turn raise SIGUSR1
     * which is caught by our standard handler
     */
    ASSERT(0==sigaction(SIGUSR2, &sa, 0));
    sa.sa_handler = signal_handler;
    ASSERT(0==sigaction(SIGUSR1, &sa, 0));
    last_signal = -1;
    signal_raised[SIGUSR1]=0;
    signal_raised[SIGUSR2]=0;
    second_handler_called = 0;
    ASSERT(0==raise(SIGUSR2));
    ASSERT(1==second_handler_called);
    ASSERT(1==signal_raised[SIGUSR1]);
    ASSERT(0==signal_raised[SIGUSR2]);
    ASSERT(SIGUSR1==last_signal);
    return 0;
}

/*
 * Testcase 5
 * Test executing sigwait for a blocked and pending signal
 */
int testcase5() {
    struct sigaction sa;
    sigset_t set;
    int sig_no;
    sigemptyset(&set);
    ASSERT(0==sigprocmask(SIG_SETMASK, &set, 0));
    sa.sa_flags = 0;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    /*
     * Install handler for SIGUSR1
     */
    ASSERT(0==sigaction(SIGUSR1, &sa, 0));
    signal_raised[SIGUSR1]=0;
    /*
     * Block signal SIGUSR1
     */
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    ASSERT(0==sigprocmask(SIG_SETMASK, &set, 0));
    /*
     * Raise it and verify that it is pending
     */
    ASSERT(0==raise(SIGUSR1));
    sigemptyset(&set);
    ASSERT(0==sigpending(&set));
    ASSERT(1==sigismember(&set, SIGUSR1));
    /*
     * Now do sigwait and verify that when sigwait returns, the signal is no longer pending, even though the handler
     * has not been called
     */
    sig_no = 0;
    ASSERT(0==sigwait(&set, &sig_no));
    ASSERT(SIGUSR1==sig_no);
    sigemptyset(&set);
    ASSERT(0==sigismember(&set, SIGUSR1));
    ASSERT(0==sigpending(&set));
    ASSERT(0==sigismember(&set, SIGUSR1));
    ASSERT(0==signal_raised[SIGUSR1]);
    return 0;
}

/*
 * Testcase 6
 * Test setting the action for a blocked and pending signal to ignore and verify that the signal is no longer pending
 * afterwards even though the handler has not been invoked
 */
int testcase6() {
    struct sigaction sa;
    sigset_t set;
    sigemptyset(&set);
    ASSERT(0==sigprocmask(SIG_SETMASK, &set, 0));
    sa.sa_flags = 0;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    /*
     * Install handler for SIGUSR1
     */
    ASSERT(0==sigaction(SIGUSR1, &sa, 0));
    signal_raised[SIGUSR1]=0;
    /*
     * Block signal SIGUSR1
     */
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    ASSERT(0==sigprocmask(SIG_SETMASK, &set, 0));
    /*
     * Raise it and verify that it is pending
     */
    ASSERT(0==raise(SIGUSR1));
    sigemptyset(&set);
    ASSERT(0==sigpending(&set));
    ASSERT(1==sigismember(&set, SIGUSR1));
    /*
     * Now set sigaction to ignore and verify that when sigaction returns, the signal is no longer pending, even though the handler
     * has not been called
     */
    sa.sa_handler = SIG_IGN;
    ASSERT(0==sigaction(SIGUSR1, &sa, 0));
    sigemptyset(&set);
    ASSERT(0==sigismember(&set, SIGUSR1));
    ASSERT(0==sigpending(&set));
    ASSERT(0==sigismember(&set, SIGUSR1));
    ASSERT(0==signal_raised[SIGUSR1]);
    return 0;
}

/*
 * Testcase 7
 * Set an alarm for a process and then wait in a loop until the alarm goes off and the signal handler is invoked
 */
int testcase7() {
    struct sigaction sa;
    sigset_t set;
    sigemptyset(&set);
    ASSERT(0==sigprocmask(SIG_SETMASK, &set, 0));
    sa.sa_flags = 0;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    /*
     * Install handler for SIGALRM
     */
    ASSERT(0==sigaction(SIGALRM, &sa, 0));
    signal_raised[SIGUSR1]=0;
    /*
     * Unblock signal SIGALRM
     */
    sigemptyset(&set);
    ASSERT(0 == sigprocmask(SIG_SETMASK, &set, 0));
    /*
     * Reset counter
     */
    alarm_raised = 0;
    /*
     * Set alarm
     */
    alarm(1);
    /*
     * and enter loop until alarm goes off
     */
    while (1) {
        if (alarm_raised)
            break;
    }
    return 0;
}

/*
 * Testcase 8
 * Set an alarm for a process and then wait in a sleep call until the alarm goes off and the signal handler is invoked
 */
int testcase8() {
    struct sigaction sa;
    sigset_t set;
    int rc;
    sigemptyset(&set);
    ASSERT(0==sigprocmask(SIG_SETMASK, &set, 0));
    sa.sa_flags = 0;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    /*
     * Install handler for SIGALRM
     */
    ASSERT(0==sigaction(SIGALRM, &sa, 0));
    signal_raised[SIGUSR1]=0;
    /*
     * Unblock signal SIGALRM
     */
    sigemptyset(&set);
    ASSERT(0 == sigprocmask(SIG_SETMASK, &set, 0));
    /*
     * Reset counter
     */
    alarm_raised = 0;
    /*
     * Set alarm
     */
    alarm(1);
    /*
     * and call sleep
     */
    rc = sleep(100);
    ASSERT(rc > 0);
    ASSERT(1 == alarm_raised);
    return 0;
}

/*
 * Testcase 9: wait in a read operation on a socket until a timeout occurs
 */
int testcase9() {
    int fd;
    struct sockaddr_in addr;
    struct timeval timeout;
    unsigned char buffer[512];
    /*
     * Make sure all alarms are canceled
     */
    alarm(0);
    /*
     * Open UDP socket and bind it
     */
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT(fd >= 0);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = 0;
    addr.sin_port = 0;
    ASSERT(0 == bind(fd, (struct sockaddr*) &addr, sizeof(struct sockaddr_in)));
    /*
     * Now set timeout to 1 second
     */
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    ASSERT(0 == setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (void*) &timeout, sizeof(struct timeval)));
    /*
     * and wait
     */
    ASSERT(-1 == recv(fd, buffer, 512, 0));
    ASSERT(EAGAIN == errno);
    /*
     * Close file descriptor again
     */
    close(fd);
    return 0;
}

/*
 * Testcase 10
 * Set an alarm for a process and then use sigsuspend to wait for the signal
 */
int testcase10() {
    struct sigaction sa;
    sigset_t set;
    /*
     * First block SIGALRM and SIGUSR1
     */
    sigemptyset(&set);
    sigaddset(&set, SIGALRM);
    sigaddset(&set, SIGUSR1);
    ASSERT(0 ==sigprocmask(SIG_SETMASK, &set, 0));
    /*
     * Install handler for SIGALRM
     */
    sa.sa_flags = 0;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    ASSERT(0 == sigaction(SIGALRM, &sa, 0));
    signal_raised[SIGALRM]=0;
    /*
     * Reset counter
     */
    alarm_raised = 0;
    /*
     * Set alarm
     */
    alarm(1);
    /*
     * Unblock signal SIGALRM and wait for it atomically
     */
    sigemptyset(&set);
    ASSERT(-1 == sigsuspend(&set));
    ASSERT(EINTR == errno);
    ASSERT(1 == alarm_raised);
    /*
     * Check that SIGUSR1 is blocked again, i.e the old signal mask has been
     * restored
     */
    ASSERT(0 == sigprocmask(SIG_SETMASK, 0, &set));
    ASSERT(sigismember(&set, SIGALRM));
    ASSERT(sigismember(&set, SIGUSR1));
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
    END;
}





