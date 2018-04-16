/*
 * times.c
 *
 */

#include "lib/sys/times.h"
#include "lib/os/oscalls.h"

/*
 * Get CPU accounting information for current process. This call returns the number of ticks passed since
 * boot time ("uptime"). If the pointer buffer is different from NULL, it also fills the fields of the buffer
 * as follows.
 *
 * buffer->tms_utime:  user space time used by current process
 * buffer->tms_stime:  kernel space time used by current process
 * buffer->tms_cutime: user space time used by waited for children of current process
 * buffer->tms_cstime: kernel space time used by waited for children of current process
 */
clock_t times(struct tms *buffer) {
    return __ctOS_times(buffer);
}
