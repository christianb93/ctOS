/*
 * signal.h
 *
 */

#ifndef _SIGNAL_H_
#define _SIGNAL_H_


#include "os/signals.h"

/*
 * We only use 32 signals in ctOS. However, we define a sigset as Linux does
 * it to ease testing on the development platform
 */
typedef struct {
    unsigned  int __val[(1024 / (8 * sizeof (unsigned  int)))];
} sigset_t;

typedef __ksiginfo_t siginfo_t;


union sigval {
    int sival_int;
    void* sival_ptr;
};

struct sigaction {
    void (*sa_handler)(int);                          // signal handler
    sigset_t sa_mask;                                 // signal mask to be applied during execution of handler
    unsigned int sa_flags;                            // additional flags
    void (*sa_sigaction)(int, siginfo_t*, void*);     // not supported by ctOS
};

typedef void (*sighandler_t)(int);
sighandler_t signal(int signum, sighandler_t handler);

typedef unsigned int sig_atomic_t;

/*
 * BSD has a list of all signal names
 */
#ifdef BSD
extern const char * const sys_siglist[];
#endif

/*
 * Supported signals
 */
#define SIGHUP   __KSIGHUP
#define SIGINT   __KSIGINT
#define SIGQUIT  __KSIGQUIT
#define SIGILL   __KSIGILL
#define SIGABRT  __KSIGABRT
#define SIGBUS   __KSIGBUS
#define SIGFPE   __KSIGFPE
#define SIGKILL  __KSIGKILL
#define SIGUSR1  __KSIGUSR1
#define SIGSEGV  __KSIGSEGV
#define SIGUSR2  __KSIGUSR2
#define SIGPIPE  __KSIGPIPE
#define SIGALRM  __KSIGALRM
#define SIGTERM  __KSIGTERM
#define SIGCHLD  __KSIGCHLD
#define SIGCONT  __KSIGCONT
#define SIGSTOP  __KSIGSTOP
#define SIGTSTP  __KSIGTSTP
#define SIGTTIN  __KSIGTTIN
#define SIGTTOU  __KSIGTTOU
#define SIGURG   __KSIGURG


/*
 * Values for sigaction.sa_handler to indicate default or ignore action
 */
#define SIG_DFL __KSIG_DFL
#define SIG_IGN __KSIG_IGN
#define SIG_ERR __KSIG_ERR


/*
 * Parameter "how" for sigprocmask
 */
#define SIG_BLOCK __KSIG_BLOCK
#define SIG_SETMASK __KSIG_SETMASK
#define SIG_UNBLOCK __KSIG_UNBLOCK

/*
 * Flags for sigaction
 */
#define SA_NOCLDSTOP __KSA_NOCLDSTOP

/*
 * Function prototypes
 */
int sigemptyset(sigset_t* set);
int sigfillset(sigset_t* set);
int sigaddset(sigset_t* set, int sig_no);
int sigdelset(sigset_t* set, int sig_no);
int sigismember(const sigset_t* set, int sig_no);
int sigwait(const sigset_t* set, int* sig_no);
int sigprocmask(int how, sigset_t* set, sigset_t* oset);
int sigsuspend(const sigset_t* sigmask);
int sigaction(int sig_no, const struct sigaction* act,  struct sigaction* oldact);
int sigpending(sigset_t *set);
int kill(pid_t pid, int sig);
int raise(int sig_no);
int pause();
int killpg(pid_t pgrp, int sig);


#endif /* _SIGNAL_H_ */
