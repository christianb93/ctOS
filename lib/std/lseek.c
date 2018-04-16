/*
 * lseek.c
 *
 */


#include "lib/os/oscalls.h"
#include "lib/sys/types.h"
#include "lib/errno.h"

/*
 * The lseek() function will set the file offset for the open file description associated with the file descriptor fildes, as follows:
 *
 * If whence is SEEK_SET, the file offset will be set to offset bytes.
 *
 * If whence is SEEK_CUR, the file offset will be set to its current location plus offset.
 *
 * If whence is SEEK_END, the file offset will be set to the size of the file plus offset.
 *
 * The symbolic constants SEEK_SET, SEEK_CUR, and SEEK_END are defined in <unistd.h>.
 *
 * The lseek() function will allow the file offset to be set beyond the end of the existing data in the file.
 * If data is later written at this point, subsequent reads of data in the gap will return bytes with the value 0
 * until data is actually written into the gap.
 *
 * The lseek() function will not, by itself, extend the size of a file.
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * none
 *
 *
 */
off_t lseek(int fd, off_t offset, int whence) {
    int res;
    res =  __ctOS_lseek(fd, offset, whence);
    if (res<0) {
        errno = -res;
        return -1;
    }
    return res;
}

