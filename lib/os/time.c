/*
 * time.c
 */

#include "lib/os/oscalls.h"
#include "lib/os/syscalls.h"
#include "lib/sys/time.h"
#include "lib/time.h"

/*
 * Get current time in seconds
 */
time_t __ctOS_time(time_t* tloc) {
    return __ctOS_syscall(__SYSNO_TIME,1, tloc);
}


/*
 * Get time of day
 *
 * The current time is obtained and stored in the structure timeval which is passed
 * as the first argument.
 *
 * The second argument is ignored
 *
 * BASED ON: POSIX 2004
 *
 */
int gettimeofday(struct timeval *tv, void* tz) {
    /*
     * Currently we only implement resolutions of a second
     */
    if (0 == tv) {
        return 0;
    }
    tv->tv_sec = time(0);
    tv->tv_usec = 0;
    return 0;
}
