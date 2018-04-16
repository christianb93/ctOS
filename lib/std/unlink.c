/*
 * unlink.c
 *
 */

#include "lib/os/oscalls.h"
#include "lib/errno.h"

static int __do_unlink(char* path) {
    int res;
    res = __ctOS_unlink(path);
    if (res >= 0)
        return 0;
    errno = -res;
    return -1;
}


/*
 * Unlink a file
 * Parameter:
 * @path - the file name
 * Return value:
 * 0 if the operation was successful
 * -1 if the operation failed - errno will be set in this case
 */
int unlink (char* path) {
    struct stat mystat;
    /*
     * First stat file and make sure that this is not a directory
     */
    if (stat(path, &mystat)) {
        errno = ENOENT;
        return -1;
    }
    if (S_ISDIR(mystat.st_mode)) {
        errno = EPERM;
        return -1;
    }
    return __do_unlink(path);
}


/*
 * Remove a directory
 *
 * This function checks that the given path refers to a directory and deletes the directory. It is not possible to remove
 * a non-empty directory, the root directory, a used mount point or a directory to which additional links refer.
 *
 * It the last component of the path is .. or ., the function will fail and set errno to EINVAL
 *
 * If a directory is removed while a process has a file descriptor referring to the directory open, the
 * reference continues to be valid, but no entries can be added to the directory any more. The directory will be removed from
 * disk once the last reference is dropped
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * none
 */
int rmdir(char* path) {
    struct stat mystat;
    /*
     * First stat the directory
     */
    if (stat(path, &mystat)) {
        errno = ENOENT;
        return -1;
    }
    if (!S_ISDIR(mystat.st_mode)) {
        errno = ENOTDIR;
        return -1;
    }
    return __do_unlink(path);
}
