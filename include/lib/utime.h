/*
 * utime.h
 *
 */

#ifndef _UTIME_H_
#define _UTIME_H_


#ifndef __time_t_defined
typedef long int time_t;
#define __time_t_defined
#endif

struct utimbuf {
    time_t actime;      // Access time.
    time_t modtime;     // Modification time.
};

int utime(const char *path, const struct utimbuf *times);

#endif /* _UTIME_H_ */
