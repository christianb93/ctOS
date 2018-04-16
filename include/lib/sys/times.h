/*
 * times.h
 *
 */

#ifndef _TIMES_H_
#define _TIMES_H_

#include "types.h"

struct tms {
    clock_t tms_utime;
    clock_t tms_stime;
    clock_t tms_cutime;
    clock_t tms_cstime;
};

clock_t times(struct tms *buffer);

#endif /* _TIMES_H_ */
