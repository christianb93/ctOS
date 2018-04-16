/*
 * resource.h
 *
 */

#ifndef _RESOURCE_H_
#define _RESOURCE_H_

/*
 * This needs to match the definition in sys/time.h
 */
#ifndef __TIMEVAL
#define __TIMEVAL
struct timeval {
    time_t tv_sec;
    suseconds_t tv_usec;
};
#endif

struct rusage {
    struct timeval ru_utime;
    struct timeval ru_stime;
};

#endif /* _RESOURCE_H_ */
