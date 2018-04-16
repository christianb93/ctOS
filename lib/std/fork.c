/*
 * fork.c
 *
 */

#include "lib/os/oscalls.h"
#include "lib/errno.h"

/*
 * The fork() function will create a new process. The new process (child process) will be an exact copy of the
 * calling process (parent process) except as detailed below:
 *
 * The child process will have a unique process ID.
 * The child process ID also will not match any active process group ID
 * The child process will have a different parent process ID, which will be the process ID of the calling process.
 * The child process will have its own copy of the parent's file descriptors.
 * Each of the child's file descriptors will refer to the same open file description with the corresponding file descriptor of the parent.
 * The child process will have its own copy of the parent's open directory streams. Each open directory stream in the child process
 * will share directory stream positioning with the corresponding directory stream of the parent.
 * The set of signals pending for the child process will be initialized to the empty set.
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * none
 *
 *
 */
pid_t fork() {
    int res =  __ctOS_fork();
    if (res<0) {
        errno = -res;
        return -1;
    }
    return res;
}
