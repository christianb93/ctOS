/*
 * fcntl.c
 *
 */


#include "lib/fcntl.h"
#include "lib/os/oscalls.h"
#include "lib/os/syscalls.h"

int __ctOS_fcntl(int fd, int cmd, int arg) {
    return __ctOS_syscall(__SYSNO_FCNTL, 3, fd, cmd, arg);
}

