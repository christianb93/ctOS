/*
 * open.c
 *
 */

#include "lib/os/syscalls.h"

/*
 * Open a file or directory
 * Parameter:
 * @path - path name
 * @flags - flags
 * Return value:
 * file descriptor
 */
int __ctOS_open(char* path, int flags, int mode) {
    return __ctOS_syscall(__SYSNO_OPEN, 3, path, flags, mode);
}

/*
 * Open a file or directory relative to a directory
 * described by the file descriptor dirfd
 * Parameter:
 * @dirfd - a file descriptor describing a directory
 * @path - path name
 * @flags - flags
 * Return value:
 * file descriptor
 */
int __ctOS_openat(int dirfd, char* path, int flags, int mode) {
    return __ctOS_syscall(__SYSNO_OPENAT, 4, dirfd, path, flags, mode);
}

/*
 * Create a directory
 */
int __ctOS_mkdir(char* path, int mode) {
    return __ctOS_syscall(__SYSNO_MKDIR, 2, path, mode);
}
