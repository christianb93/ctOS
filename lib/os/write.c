/*
 * write.c
 */

#include "lib/os/syscalls.h"
#include "lib/sys/types.h"

/*
 * Write a string to an open file descriptor
 * Parameter:
 * @fd - the file descriptor to write to
 * @buffer - buffer containing the data
 * @bytes - number of bytes to write
 * Return value:
 * -EIO or -ENODEV if operation failed, number of bytes written otherwise
 */
ssize_t __ctOS_write(int fd, char* buffer, size_t bytes) {
    return __ctOS_syscall(__SYSNO_WRITE, 3, fd, buffer, bytes);
}
