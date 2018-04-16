/*
 * times.c
 *
 */

#include "lib/os/oscalls.h"
#include "lib/os/syscalls.h"

/*
 * Times system call
 */
clock_t __ctOS_times(struct tms* buffer) {
    return __ctOS_syscall(__SYSNO_TIMES, 1, buffer);
}
