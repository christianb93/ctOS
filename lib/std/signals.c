/*
 * signals.c
 */

#include "lib/signal.h"
#include "lib/errno.h"
#include "lib/os/oscalls.h"
#include "lib/string.h"


const char * const sys_siglist[] = {
        "No signal",
        "Hang up process",
        "Keyboard interrupt",
        "Quit",
        "Illegal instruction",
        "Unused signal 5",
        "Abort",
        "Bus error",
        "Floating point exception",
        "Kill",
        "User defined signal 1",
        "Segmentation fault",
        "User defined signal 2",
        "Broken pipe",
        "Alarm",
        "Terminate",
        "Unused signal 16",
        "Child status change",
        "Continue",
        "Stop",
        "Terminal stop",
        "Terminal input",
        "Terminal output",
        "Urgent"
};

/*
 * The kill() function will send a signal to a process or a group of processes specified by pid. The signal to be sent is
 * specified by sig and is either one from the list given in <signal.h> or 0.
 *
 * If pid is greater than 0, sig will be sent to the process whose process ID is equal to pid.
 *
 * If pid is 0, sig will be sent to all processes (excluding process 0 and 1) whose process group ID is equal
 * to the process group ID of the sender, and for which the process has permission to send a signal.
 *
 * If pid is -1, sig will be sent to all processes (excluding process 0 and 1) for which the process has permission to send that signal.
 *
 * If pid is negative, but not -1, sig will be sent to all processes (excluding process 0 and 1) whose process group ID is equal to
 * the absolute value of pid, and for which the process has permission to send a signal.
 *
 *
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * 1) the special handling of signal 0 (do not do anything, just check) is not supported yet
 * 2) permissions restricting the ability to signal an arbitrary process are not yet supported
 *
 */
int kill(pid_t pid, int sig_no) {
    int rc;
    rc = __ctOS_kill(pid, sig_no);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    return 0;
}

/*
 * The sigaction() function allows the calling process to examine and/or specify the action to be associated with a specific signal.
 * The argument sig specifies the signal; acceptable values are defined in <signal.h>.
 *
 * If the argument act is not a null pointer, it points to a structure specifying the action to be associated with the specified signal. I
 * f the argument oact is not a null pointer, the action previously associated with the signal is stored in the location pointed to
 * by the argument oact.
 *
 * If the argument act is a null pointer, signal handling is unchanged; thus, the call can be used to enquire about the current
 * handling of a given signal.
 *
 * The SIGKILL and SIGSTOP signals cannot not be added to the signal mask using the field sa_mask in the provided sigaction structure,
 * if the corresponding bits are set in the mask, they are silenty ignored.
 *
 * The sa_handler field in the provided sigaction structure identifies the action to be associated with the specified signal. It
 * can either point to an application-provided function or be one of the values SIG_DFL and SIG_IGN defined in signal.h
 *
 * The sa_flags field can be used to modify the behavior of the specified signal.
 *
 * The following flags, defined in the <signal.h> header, can be set in sa_flags:
 *
 * SA_NOCLDSTOP
 *     Do not generate SIGCHLD when children stop. If sig is SIGCHLD and the SA_NOCLDSTOP flag is not set in sa_flags,
 *     then a SIGCHLD signal will be generated for the calling process whenever any of its child processes stop
 *     If sig is SIGCHLD and the SA_NOCLDSTOP flag is set in sa_flags, then no SIGCHLD signal is generated in this way.
 *
 *
 * When a signal is caught by a signal-catching function installed by sigaction(), a new signal mask is calculated and installed
 * for the duration of the signal-catching function (or until a call to either sigprocmask() or sigsuspend() is made).
 * This mask is formed by taking the union of the current signal mask and the value of the sa_mask for the signal being delivered
 * and then including the signal being delivered.
 *
 * If and when the user's signal handler returns normally, the original signal mask is restored.
 *
 * Once an action is installed for a specific signal, it will remain installed until another action is explicitly requested
 * (by another call to sigaction()), or until one of the exec functions is called.
 *
 *
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * 1) POSIX specifies that a member of an orphaned process group shall not be stopped by any of the signals SIGTSTP, SIGTTI and
 * SIGTTOU. This is not implemented in ctOS.
 * 2) POSIX also specifies that if a process which was stopped is continued after receiving a SIGCONT signal, all pending signals
 * should be delivered before the process resumes execution. The implementation in ctOS is different. After a process has been continued,
 * execution resumes immediately and pending signals are only processed upon the next return from kernel space, i.e. after the next system
 * call or timer interrupt
 *
 */
int sigaction(int sig_no, const struct sigaction* act,  struct sigaction* oldact) {
    __ksigaction_t k_old;
    __ksigaction_t k_act;
    int rc;
    if (act) {
        k_act.sa_handler = act->sa_handler;
        k_act.sa_mask = act->sa_mask.__val[0];
        k_act.sa_flags = act->sa_flags;
    }
    rc = __ctOS_sigaction(sig_no, (act) ? &k_act : 0, &k_old);
    if (rc<0) {
        errno = -rc;
        return -1;
    }
    if (oldact) {
        oldact->sa_flags = k_old.sa_flags;
        oldact->sa_handler = k_old.sa_handler;
        sigemptyset(&(oldact->sa_mask));
        oldact->sa_mask.__val[0]=k_old.sa_mask;
        oldact->sa_sigaction = 0;
    }
    return 0;
}

/*
 * The sigemptyset() function initializes the signal set pointed to by set, such that all signals are excluded.
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * none
 */
int sigemptyset(sigset_t* set) {
    int i;
    for (i=0;i<32;i++)
        set->__val[i]=0;
    return 0;
}

/*
 * The sigfillset() function will initialize the signal set pointed to by set, such that all signals  are included.
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * none
 *
 */
int sigfillset(sigset_t* set) {
    set->__val[0]=__SIGALL;
    return 0;
}

/*
 * The sigaddset() function adds the individual signal specified by the signo to the signal set pointed to by set.
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * none
 *
 */
int sigaddset(sigset_t* set, int sig_no) {
    if (sig_no>31) {
        errno = EINVAL;
        return -1;
    }
    set->__val[0] |= (1 << sig_no);
    return 0;
}

/*
 * The sigdelset() function deletes the individual signal specified by signo from the signal set pointed to by set.
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * none
 *
 */
int sigdelset(sigset_t* set, int sig_no) {
    if (sig_no>31) {
        errno = EINVAL;
        return -1;
    }
    set->__val[0] &= (~(1 << sig_no));
    return 0;
}

/*
 * The sigismember() function will test whether the signal specified by signo is a member of the set pointed to by set and return
 * 1 in this case. Otherwise, 0 is returned unless an error occurs.
 *
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * none
 *
 */
int sigismember(const sigset_t *set, int sig_no) {
    if (sig_no>31) {
        errno = EINVAL;
        return -1;
    }
    return ((set->__val[0] & (1<<sig_no)) ? 1 : 0);
}

/*
 * The sigwait() function will select a pending signal from set, atomically clear it from the system's set of
 * pending signals, and return that signal number in the location referenced by sig.
 *
 * If no signal in set is pending at the time of the call, the thread will be suspended until one or more becomes pending.
 * The signals defined by set shall have been blocked at the time of the call to sigwait(); otherwise, the behavior is undefined.
 * The effect of sigwait() on the signal actions for the signals in set is unspecified.
 *
 * If more than one thread is using sigwait() to wait for the same signal, no more than one of these threads will return from sigwait()
 * with the signal number.
 *
 * If more than a single thread is blocked in sigwait() for a signal when that signal is generated for the process, it is
 * unspecified which of the waiting threads returns from sigwait(). If the signal is generated for a specific thread,
 * as by pthread_kill(), only that thread will return.
 *
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * none
 *
 */
int sigwait(const sigset_t* set, int* sig) {
    int rc;
    rc = __ctOS_sigwait(set->__val[0], sig);
    if (rc < 0) {
        return EINVAL;
    }
    return 0;
}


/*
 * The pause() function will suspend the calling thread until delivery of a signal whose action is either to execute a
 * signal-catching function or to terminate the process.
 *
 * If the action is to terminate the process, pause() will not return.
 *
 * If the action is to execute a signal-catching function, pause() will return with return value -1 after the signal-catching function
 * returns, and errno will be set to EINTR.
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * none
 *
 */
int pause() {
    int rc = __ctOS_pause();
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    return 0;
}

/*
 * In a single-threaded process, the sigprocmask() function will examine or change (or both) the signal mask of the calling thread.
 *
 * If the argument set is not a null pointer, it points to a set of signals to be used to change the currently blocked set.
 *
 * The argument how indicates the way in which the set is changed, and the application shall ensure it consists of one of the following values:
 *
 *   SIG_BLOCK
 *   The resulting set will be the union of the current set and the signal set pointed to by set.
 *
 *   SIG_SETMASK
 *   The resulting set will be the signal set pointed to by set.
 *
 *   SIG_UNBLOCK
 *   The resulting set will be the intersection of the current set and the complement of the signal set pointed to by set.
 *
 * If the argument oset is not a null pointer, the previous mask shall be stored in the location pointed to by oset.
 * If set is a null pointer, the value of the argument how is not significant and the thread's signal mask will be unchanged;
 * thus the call can be used to enquire about currently blocked signals.
 *
 * It is not possible to block those signals which cannot be ignored. This will be enforced by the system without causing an
 * error to be indicated.
 *
 * If any of the SIGFPE, SIGILL, SIGSEGV, or SIGBUS signals are generated while they are blocked, the result is undefined,
 * unless the signal was generated by the kill() function or the raise() function.
 *
 * The use of the sigprocmask() function is unspecified in a multi-threaded process.
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * none
 */
int sigprocmask(int how, sigset_t* set, sigset_t* oset) {
    int rc;
    unsigned int mask;
    unsigned int omask;
    /*
     * Clean up oset if it is specified
     */
    if (oset) {
        memset((void*) oset, 0, sizeof(sigset_t));
    }
    if (set) {
        mask = set->__val[0];
        rc = __ctOS_sigprocmask(how, &mask, &omask);
    }
    else {
        rc = __ctOS_sigprocmask(0, 0, &omask);
    }
    if (0==rc) {
        if (oset) {
            oset->__val[0]=omask;
        }
    }
    else {
        errno = -rc;
        return -1;
    }
    return 0;
}


/*
 * Raise a signal to the own process
 *
 * The effect of the raise() function is be equivalent to calling:
 *
 * kill(getpid(), sig);
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * none
 */
int raise(int sig) {
    return kill(getpid(), sig);
}

/*
 * The sigpending() function will store, in the location referenced by the set argument, the set of signals that
 * are pending on the process or the calling thread.
 *
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * 1) POSIX specifies that only blocked signals are returned by this call. This implementation returns all pending signals,
 * regardless of whether they are blocked or not. As non-blocked signals are processed immediately when a system call returns,
 * the result will be the same unless a signal is sent to the calling process concurrently while it is executing sigpending
 *
 */
int sigpending(sigset_t *set) {
    if (0==set)
        return 0;
    sigemptyset(set);
    __ctOS_sigpending(&(set->__val[0]));
    return 0;
}

/*
 * Traditional signal library function
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * none
 *
 * NOTES:
 *
 * POSIX 2004 does not specify which signals will be blocked while a
 * signal handler installed with signal is executed. As this implementation
 * of signal uses sigaction with sa_mask set to zero, the signal mask which is
 * in effect during the execution of a signal handler is formed by adding the currently
 * executed signal to the signal mask of the task
 *
 */
sighandler_t signal(int signum, sighandler_t handler) {
    struct sigaction sa;
    sighandler_t old_handler;
    /*
     * Get old signal handler first
     */
    if(sigaction(signum, 0, &sa)) {
        return SIG_ERR;
    }
    old_handler = sa.sa_handler;
    /*
     * Install new handler
     */
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = handler;
    if(sigaction(signum, &sa, 0)) {
        return SIG_ERR;
    }
    return old_handler;
}

/*
 * The killpg() function shall send the signal specified by sig to the process group specified by pgrp.
 *
 * If pgrp is greater than 1, killpg(pgrp, sig) is equivalent to kill(-pgrp, sig). If pgrp is less than or equal to 1,
 * the behavior of killpg() is undefined by POSIX. This implementation sets errno to EINVAL and returns -1 in this case
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * see kill
 *
 */
int killpg(pid_t pgrp, int sig) {
    if (pgrp>1) {
        return kill(-pgrp, sig);
    }
    else {
        errno = EINVAL;
        return -1;
    }
}

/*
 * Sigsuspend
 *
 * Replace the current signal mask of a thread with the value pointed to by the argument and suspend the thread
 * until a signal causes execution of a signal handler or process termination. When the function returns, the old
 * value of the signal mask will be restored
 */
int sigsuspend(const sigset_t *set) {
    sigset_t old_set;
    if (0 == set)
        return 0;
    /*
     * Execute system call sigsuspend which will save the old signal mask,
     * apply the new signal mask and wait
     */
    __ctOS_sigsuspend(&(((sigset_t*) set)->__val[0]), &(old_set.__val[0]));
    /*
     * Restore old signal mask
     */
    __ctOS_sigprocmask(SIG_SETMASK, &(old_set.__val[0]), 0);
    errno = EINTR;
    return -1;
}

