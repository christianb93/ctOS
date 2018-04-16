/*
 * fork.c
 *
 */

#include "lib/os/syscalls.h"

/*
 * Fork
 * Return value:
 * 0 for the child process
 * the pid of the newly generated process for the parent
 * -EAGAIN or -ENOMEM if the operation failed
 */
int __ctOS_fork() {
    return __ctOS_syscall(__SYSNO_FORK, 0);
}

