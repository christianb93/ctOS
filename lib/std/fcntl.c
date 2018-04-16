/*
 * fcntl.c
 *
 */

#include "lib/fcntl.h"
#include "lib/os/oscalls.h"
#include "lib/stdarg.h"
#include "lib/errno.h"

/*
 * Fcntl - low-level file operations
 *
 * The fcntl() function shall perform the operations described below on open files. The fildes argument is a file descriptor.
 *
 * The available values for cmd are defined in <fcntl.h> and are as follows:
 *
 * F_GETFD: get the file descriptor flags defined in <fcntl.h> that are associated with the file descriptor fildes.
 * File descriptor flags are associated with a single file descriptor and do not affect other file descriptors that refer to the same file.
 * F_SETFD: set the file descriptor flags defined in <fcntl.h>, that are associated with fildes, to the third argument, arg,
 * taken as type int. If the FD_CLOEXEC flag in the third argument is 0, the file shall remain open across the exec functions;
 * otherwise, the file shall be closed upon successful execution of one of the exec functions.
 *
 * Upon successful completion, the value returned shall depend on cmd as follows:
 * GETFD: value of flags defined in <fcntl.h>. The return value shall not be negative.
 * F_SETFD: value other than -1.
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * 1) currently, F_GETFD, F_SETFD, F_GETFL and F_SETFL and F_DUPFD are the only supported commands
 * 2) In particular, file locking is not yet supported
 */
int fcntl(int fildes, int cmd, ...) {
    int rc = 0;
    int arg;
    va_list ap;
    va_start(ap, cmd);
    switch (cmd) {
        case F_SETFD:
        case F_SETFL:
        case F_DUPFD:
            arg = va_arg(ap, int);
            rc = __ctOS_fcntl(fildes, cmd, arg);
            if (rc<0) {
                errno = -rc;
                rc = -1;
            }
            break;
        case F_GETFD:
        case F_GETFL:
            rc = __ctOS_fcntl(fildes, cmd, 0);
            if (rc<0) {
                errno = -rc;
                rc = -1;
            }
            break;
        default:
            rc = -1;
            errno = EINVAL;
            break;
    }
    va_end(ap);
    return rc;
}
