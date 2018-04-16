/*
 * times.h
 *
 */

#ifndef _OS_TIMES_H_
#define _OS_TIMES_H_

struct __ktms {
    clock_t tms_utime;
    clock_t tms_stime;
    clock_t tms_cutime;
    clock_t tms_cstime;
};

#endif /* _OS_TIMES_H_ */
