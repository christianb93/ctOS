/*
 * signals.c
 *
 */

#include "lib/os/syscalls.h"
#include "lib/os/oscalls.h"

/*
 * Send a signal to a process
 */
int __ctOS_kill(pid_t pid, int sig_no) {
    return __ctOS_syscall(__SYSNO_KILL, 2, pid, sig_no);
}


/*
 * Read or set action to take when a signal is delivered
 */
int __ctOS_sigaction(int sig_no, __ksigaction_t* act,  __ksigaction_t* oldact) {
    return __ctOS_syscall(__SYSNO_SIGACTION, 3, sig_no, act, oldact);
}

/*
 * Wait for a signal
 */
int __ctOS_sigwait(unsigned int set, int* sig) {
    return __ctOS_syscall(__SYSNO_SIGWAIT, 2, set, sig);
}


/*
 * Pause until a signal handler is executed or the task terminates
 */
int __ctOS_pause() {
    return __ctOS_syscall(__SYSNO_PAUSE, 0);
}

/*
 * Sigsuspend
 */
int __ctOS_sigsuspend(unsigned int* set, unsigned int* old_set) {
    return __ctOS_syscall(__SYSNO_SIGSUSPEND, 2, set, old_set);
}

/*
 * Change the signal mask of a task
 */
int __ctOS_sigprocmask(int how, unsigned int* set, unsigned int* oset) {
    return __ctOS_syscall(__SYSNO_SIGPROCMASK, 3, how, set, oset);
}

/*
 * Get bitmask of pending signals
 */
int __ctOS_sigpending(unsigned int* bitmask) {
    return __ctOS_syscall(__SYSNO_SIGPENDING, 1, bitmask);
}
