/*
 * time.h
 */

#ifndef _TIME_H_
#define _TIME_H_

#include "sys/types.h"

struct tm {
    int    tm_sec;   // Seconds [0,60].
    int    tm_min;   // Minutes [0,59].
    int    tm_hour;  // Hour [0,23].
    int    tm_mday;  // Day of month [1,31].
    int    tm_mon;   // Month of year [0,11].
    int    tm_year;  // Years since 1900.
    int    tm_wday;  // Day of week [0,6] (Sunday =0).
    int    tm_yday;  // Day of year [0,365].
    int    tm_isdst; // Daylight Savings flag.
};

/*
 * Number of OS clocks per second - need to match the value in timer.h in ctOS/include
 */
#define CLOCKS_PER_SEC 100


time_t mktime(struct tm*);
time_t time(time_t* tloc);
struct tm* localtime(const time_t *timer);
char* asctime(const struct tm*);
char* ctime(const time_t*);
struct tm *gmtime(const time_t *timep);
size_t strftime(char* s, size_t maxsize, const char* format, const struct tm* timeptr);
void tzset();

#endif /* _TIME_H_ */
