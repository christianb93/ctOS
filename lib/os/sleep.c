/*
 * sleep.c
 *
 */

#include "lib/os/syscalls.h"

/*
 * Sleep for the specified number of seconds
 */
int __ctOS_sleep(int seconds) {
    return __ctOS_syscall(__SYSNO_SLEEP, 1, seconds);
}

/*
 * Set alarm
 */
unsigned int __ctOS_alarm(unsigned int seconds) {
    return __ctOS_syscall(__SYSNO_ALARM, 1, seconds);
}
