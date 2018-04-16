/*
 * ioctl.c
 *
 */

#include "lib/sys/ioctl.h"
#include "lib/stdarg.h"
#include "lib/os/oscalls.h"
#include "lib/errno.h"

/*
 * IOCTL system call.
 *
 * Manipulate device settings for special files. The argument d is a file descriptor which
 * refers to a special file representing the device. The second parameter is the command.
 *
 * Currently the following commands are supported:
 *
 * TIOCGPGRP - get foreground process group.
 * TIOCSPGRP - set foreground process group
 * TIOCGETD - get line discipline
 *
 */
int ioctl(int d, unsigned int request, ...) {
    unsigned int arg;
    int rc;
    va_list ap;
    va_start(ap, request);
    arg = va_arg(ap, unsigned int);
    rc = __ctOS_ioctl(d, request, arg);
    if (rc < 0) {
        errno = rc;
        return -1;
    }
    return 0;
}
