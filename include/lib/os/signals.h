/*
 * signals.h
 */

#ifndef __OS_SIGNALS_H_
#define __OS_SIGNALS_H_

#include "types.h"

typedef void (*__ksighandler_t)(int);

union __ksigval {
    int sival_int;
    void* sival_ptr;
};

typedef struct __ksiginfo {
    int si_signo;
    int si_code;
    pid_t si_pid;
    uid_t si_uid;
    void* si_addr;
    int si_status;
    union __ksigval si_value;
} __ksiginfo_t;

/*
 * This is the sigaction structure used internally by the kernel
 */
typedef struct __ksigaction {
    void (*sa_handler)(int);                          // signal handler
    unsigned int sa_mask;                             // signal mask to be applied during execution of handler
    unsigned int sa_flags;                            // additional flags
} __ksigaction_t;

/*
 * Pseudo-handlers for sigaction
 */
#define __KSIG_DFL ((__ksighandler_t) 1)
#define __KSIG_IGN ((__ksighandler_t) 0)
/*
 * Return value for signal if an error occurs. We use -1 here,
 * as there seem to be some older programs which assume this
 */
#define __KSIG_ERR ((__ksighandler_t) -1)

/*
 * Number of signals allowed by the data model
 */
#define __NR_OF_SIGNALS 32

/*
 * Signals
 */
#define __KSIGHUP 1
#define __KSIGINT 2
#define __KSIGQUIT 3
#define __KSIGILL 4
#define __KSIGABRT 6
#define __KSIGBUS 7
#define __KSIGFPE 8
#define __KSIGKILL 9
#define __KSIGUSR1 10
#define __KSIGSEGV 11
#define __KSIGUSR2 12
#define __KSIGPIPE 13
#define __KSIGALRM 14
#define __KSIGTERM 15
#define __KSIGCHLD 17
#define __KSIGCONT 18
#define __KSIGSTOP 19
#define __KSIGTSTP 20
#define __KSIGTTIN 21
#define __KSIGTTOU 22
#define __KSIGURG 23

#define NSIG 23
/*
 * Kernel internal signals to stop a task
 * without signaling the parent
 */
#define __KSIGTASK 24

/*
 * Bitmask containing all valid signals
 */
#define __SIGALL   (((1 << 24) -1))

/*
 * Argument "how" to sigprocmask
 */
#define __KSIG_BLOCK 1
#define __KSIG_SETMASK 2
#define __KSIG_UNBLOCK 3

/*
 * Flags for sigaction
 */
#define __KSA_NOCLDSTOP 1

#endif /* __OS_SIGNALS_H_ */
