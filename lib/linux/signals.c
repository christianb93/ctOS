/*
 * signals.c
 */

#include "lib/os/oscalls.h"
#include "lib/os/errors.h"

#define __NR_kill        37
#define __NR_sigaction   67
#define __NR_pause       29

#define _syscall0(name) \
{ \
__asm__ volatile ("int $0x80" \
    : "=a" (__res) \
    : "a" (__NR_##name)); \
}

#define _syscall2(name,a,b) \
{ \
__asm__ volatile ("int $0x80" \
    : "=a" (__res) \
    : "a" (__NR_##name),"b" (a),"c" (b)); \
}


#define _syscall3(name,a,b,c) \
{ \
__asm__ volatile ("int $0x80" \
    : "=a" (__res) \
    : "a" (__NR_##name),"b" (a),"c" (b),  "d"(c)); \
}

int __ctOS_kill(pid_t pid, int sig_no) {
    int __res;
    _syscall2(kill, pid, sig_no);
    return __res;
}

int __ctOS_sigaction(int sig_no, __ksigaction_t* act,  __ksigaction_t* oldact) {
    int __res;
    _syscall3(sigaction, sig_no, act, oldact);
    return __res;
}

int __ctOS_sigwait(unsigned int set, int* sig) {
    return -ENOSYS;
}

int __ctOS_pause() {
    int __res;
    _syscall0(pause);
    return __res;
}

int __ctOS_sigprocmask(int how, unsigned int* set, unsigned int* oset) {
    return -ENOSYS;
}

int __ctOS_sigpending(unsigned int* mask) {
    return 0;
}
