/*
 * fork.c
 *
 */

#define __NR_fork         2

#define _syscall0(name) \
{ \
__asm__ volatile ("int $0x80" \
    : "=a" (__res) \
    : "a" (__NR_##name)); \
}

/*
 * Fork
 * Return value:
 * 0 for the child process
 * the pid of the newly generated process for the parent
 * -EAGAIN or -ENOMEM if the operation failed
 */
int __ctOS_fork() {
    int __res;
    _syscall0(fork);
    return __res;
}

