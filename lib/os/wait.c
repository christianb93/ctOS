/*
 * wait.c
 *
 */

#include "lib/os/syscalls.h"
#include "lib/sys/types.h"
#include "lib/os/oscalls.h"

/*
 * Wait for process completion
 */
pid_t __ctOS_waitpid(pid_t pid, int* stat_loc, int options) {
    return __ctOS_syscall(__SYSNO_WAITPID, 4, pid, stat_loc, options, 0);
}

pid_t __ctOS_wait3(int* status, int options, struct rusage* ru) {
    return __ctOS_syscall(__SYSNO_WAITPID, 4, -1, status, options, ru);
}
