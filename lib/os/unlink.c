/*
 * unlink.c
 */

#include "lib/os/syscalls.h"
#include "lib/os/errors.h"

/*
 * Unlink a file
 * Parameter:
 * @path - path name of the file to be removed
 * Return value:
 * 0 if the operation is successful
 * a negative error code if the operation failed
 */
int __ctOS_unlink(char* path) {
    if (0==path)
        return -EINVAL;
    return __ctOS_syscall(__SYSNO_UNLINK, 1, path);
}

/*
 * Rename a file
 */
int __ctOS_rename(char* old, char* new) {
    if (0 == old)
        return -EINVAL;
    if (0 == new)
        return -EINVAL;
    return __ctOS_syscall(__SYSNO_RENAME, 2, old, new);
}
