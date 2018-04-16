/*
 * getdent.c
 */

#include "lib/os/dirstreams.h"
#include "lib/os/syscalls.h"

/*
 * Get a directory entry from a directory inode
 * Parameters:
 * @fd - the file descriptor representing the directory
 * @direntry - this is where the result will be stored
 * Return value:
 * 0 upon success
 * a negative error code if the operation failed
 */
int __ctOS_getdent(int fd,__ctOS_direntry_t* direntry) {
    return -2;
}

