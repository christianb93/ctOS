/*
 * wait.c
 *
 */

#include "lib/sys/types.h"
#include "lib/errno.h"
#include "lib/os/oscalls.h"
#include "lib/sys/wait.h"
#include "lib/sys/resource.h"

/*
 * Wait for process completion
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * No known limitations
 *
 * NOTES:
 *
 * POSIX states that when an application has installed a handler for SIGCHLD and SIGCHLD is not blocked, it is unspecified whether
 * waitpid returns the status or returns -1 with errno set to EINTR. This implementation will return the status in this case if the
 * child which has generated the signal is within the set specified by the PID argument and EINTR otherwise.
 *
 */
pid_t waitpid(pid_t pid, int* stat_loc, int options) {
    pid_t res = __ctOS_waitpid(pid, stat_loc, options);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

/*
 * Wait for process completion
 */
pid_t wait(int* stat_loc) {
    return waitpid(-1, stat_loc, 0);
}

/*
 * Wait3
 *
 * This function has the same functionality as waitpid(-1, status, options), but in addition
 * returns ressource usage information for the child
 */
pid_t wait3(int *status, int options, struct rusage *rusage) {
    return __ctOS_wait3(status, options, rusage);
}
