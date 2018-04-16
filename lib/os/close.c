/*
 * close.c
 */


#include "lib/os/syscalls.h"
#include "lib/os/errors.h"

/*
 * Close a file or directory
 * Parameter:
 * @fd - file descriptor
 * Return value:
 * 0 if the operation is successful
 * a negative error code if the operation failed
 */
int __ctOS_close(int fd) {
    return __ctOS_syscall(__SYSNO_CLOSE, 1, fd);
}
