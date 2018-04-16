/*
 * lseek.c
 */

#include "lib/os/syscalls.h"
#include "lib/sys/types.h"

/*
 * Implementation of lseek system call
 * Parameters:
 * @fd - file descriptor
 * @offset - new offset
 * @whence - SEEK_SET, SEEK_CURR or SEEK_END
 * Return value:
 * new position if operation is successful
 * -1 if operation fails
 */
off_t __ctOS_lseek(int fd, off_t offset, int whence) {
    return __ctOS_syscall(__SYSNO_LSEEK, 3, fd, offset, whence);
}
