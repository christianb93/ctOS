/*
 * time.h
 *
 */

#ifndef _SYS_TIME_H_
#define _SYS_TIME_H_

#ifndef __TIMEVAL
#define __TIMEVAL

#ifndef __suseconds_t_defined
typedef long int suseconds_t;
#define __suseconds_t_defined
#endif

#ifndef __time_t_defined
typedef long int time_t;
#define __time_t_defined
#endif


struct timeval {
    time_t tv_sec;
    suseconds_t tv_usec;
};
#endif

#ifndef _TIMEZONE
#define _TIMEZONE
struct timezone {
    int tz_minuteswest;
    int zt_dsttime;
};
#endif

int gettimeofday(struct timeval* tv, void*);

#endif /* _TIME_H_ */
