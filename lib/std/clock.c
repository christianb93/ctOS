#include "lib/time.h"
#include "lib/sys/times.h"
#include "lib/os/oscalls.h"


/*
 * Return an approximation to the processor time used by the process. This will
 * return the number of ticks consumed by the process in user space or kernel space
 *
 */
clock_t clock() {
    struct tms buffer;
    __ctOS_times(&buffer);
    return buffer.tms_stime + buffer.tms_utime;
}