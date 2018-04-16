/*
 * close.c
 *
 */

#include "lib/os/oscalls.h"
#include "lib/errno.h"

/*
 * The close() function will deallocate the file descriptor indicated by fildes. To deallocate means to make the file descriptor
 * available for return by subsequent calls to open() or other functions that allocate file descriptors.
 *
 * When all file descriptors associated with a pipe are closed, any data remaining in the pipe will be discarded.
 *
 * When all file descriptors associated with an open file description have been closed, the open file description will be freed.
 *
 * If the link count of the file is 0, when all file descriptors associated with the file are closed, the space occupied by
 * the file will be freed and the file will no longer be accessible.
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * none
 *
 */
int close (int fildes) {
    int res;
    res = __ctOS_close(fildes);
    if (res>=0)
        return 0;
    errno = -res;
    return -1;
}

