/*
 * read.c
 *
 * Wrapper for the read system call
 */

#include "lib/os/syscalls.h"
#include "lib/sys/types.h"


/*
 * Read a string from an open file descriptor
 * Parameter:
 * @fd - the file descriptor to read from
 * @buffer - buffer into which we put the data
 * @bytes - number of bytes to read
 * Return value:
 * -EIO or -ENODEV if operation failed, number of bytes read otherwise
 */
ssize_t __ctOS_read(int fd, char* buffer, size_t bytes) {
    return __ctOS_syscall(__SYSNO_READ, 3, fd, buffer, bytes);
}
