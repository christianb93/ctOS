/*
 * read.c
 *
 */

#include "lib/os/oscalls.h"
#include "lib/sys/types.h"
#include "lib/errno.h"

/*
 * The read() function will attempt to read nbyte bytes from the file associated with the open file descriptor, fildes,
 * into the buffer pointed to by buf.
 *
 * If nbytes is zero, the read function will return immediately with return value zero.
 *
 * On files that support seeking (for example, a regular file), the read() will start at a position in the file
 * given by the file offset associated with fildes. The file offset will be incremented by the number of bytes actually read.
 *
 * Files that do not support seeking-for example, terminals, always read from the current position. The value of a file
 * offset associated with such a file is undefined.
 *
 * No data transfer will occur past the current end-of-file. If the starting position is at or after the end-of-file, 0 will be returned.
 *
 * When attempting to read from an empty pipe:
 *
 *   If no process has the pipe open for writing, read() will return 0 to indicate end-of-file.
 *
 *   If some process has the pipe open for writing, read() will block the calling thread until some data is
 *   written or the pipe is closed by all processes that had the pipe open for writing.
 *
 * The read() function reads data previously written to a file. If any portion of a regular file prior to the end-of-file
 * has not been written, read() will return bytes with value 0. For example, lseek() allows the file offset to be set beyond
 * the end of existing data in the file. If data is later written at this point, subsequent reads in the gap between the
 * previous end of data and the newly written data will return bytes with value 0 until data is written into the gap.
 *
 * Upon successful completion, where nbyte is greater than 0, read() will return the number of bytes read. This number will never be
 * greater than nbyte.
 *
 * The value returned may be less than nbyte if the number of bytes left in the file is less than nbyte, if the read() request was
 * interrupted by a signal, or if the file is a pipe, terminal or special file and has fewer than nbyte bytes immediately
 * available for reading. For example, a read() from a file associated with a terminal may return one typed line of data.
 *
 * If a read() is interrupted by a signal before it reads any data, it will return -1 with errno set to EINTR.
 *
 * When the process is a member of a background process attempting to read from its controlling terminal and the process is ignoring or
 * blocking the SIGTTIN signal, read will return with return value -1 and errno will be set to EIO.
 *
 * When a process is a member of a background process group attempting to read from its controlling terminal, and SIGTTIN is not
 * ignored or blocked, a SIGTTIN signal will be sent to all processes of the process' process group, including the process itself. If
 * the process catches SIGTTIN, read will return with return value -1 and errno will be set to EINTR.
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * 1) the flag O_NONBLOCK is not supported (see also open)
 * 2) the access timestamp of the file is not updated
 * 3) when a process is a member of a background process group and attempts to read from its controlling terminal and the process group
 *    is orphaned, SIGTTIN will be generated even though the POSIX standard specifies that EIO should be returned in this case
 *
 *
 */
ssize_t read(int fildes, char* buf, size_t nbytes) {
    if (0==nbytes)
        return 0;
    int res;
    res =  __ctOS_read(fildes, buf, nbytes);
    if (res<0) {
        errno = -res;
        return -1;
    }
    return res;
}

