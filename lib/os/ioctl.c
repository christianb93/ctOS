/*
 * ioctl.c
 *
 */

#include "lib/os/syscalls.h"

int __ctOS_ioctl(int fd, unsigned int request, unsigned int arg) {
    return __ctOS_syscall(__SYSNO_IOCTL, 3, fd, request,arg);
}

