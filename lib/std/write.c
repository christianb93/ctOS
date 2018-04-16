/*
 * write.c
 */

#include "lib/os/oscalls.h"
#include "lib/sys/types.h"
#include "lib/errno.h"

/*
 * Write a string to an open file descriptor
 * Parameter:
 * @fd - the file descriptor to write to
 * @buffer - buffer into which we have placed the data
 * @bytes - number of bytes to write
 * Return value:
 * the number of bytes written if the operation was succesful
 * -1 if the operation failed
 * In this case errno will be set to EIO or ENODEV
 *
 *
 * NOTE:
 *
 * POSIX does not cleary specify what the correct result is if a thread is waiting in a blocking write
 * to a pipe and the other end of the pipe is closed. We stop the system call and return the number of bytes
 * written so far in this case. If no data has been written yet, we return EPIPE and raise SIGPIPE.
 */
ssize_t write(int fd, const char* buffer, size_t bytes) {
    int res = __ctOS_write(fd, (char*) buffer, bytes);
    if (res>=0)
        return res;
    errno = -res;
    return -1;
}
