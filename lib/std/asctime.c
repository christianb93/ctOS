/*
 * asctime.c
 *
 * This has been moved into a separate module to avoid dependencies from time.c to
 * sprintf as time.c is used in the kernel as well
 */

#include "lib/stdio.h"
#include "lib/time.h"

static char __date_string[23];

/*
 * Day of week
 */
static char* day_of_week_name[7]={"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

/*
 * Month name
 */
static char* month_name[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

/*
 * Create an ASCII representation of a broken-down time
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS: none
 */
char* asctime(const struct tm* timeptr) {
    sprintf(__date_string, "%.3s %.3s%3d %.2d:%.2d:%.2d %d\n",
            day_of_week_name[timeptr->tm_wday],
            month_name[timeptr->tm_mon],
            timeptr->tm_mday, timeptr->tm_hour,
            timeptr->tm_min, timeptr->tm_sec,
            1900 + timeptr->tm_year);
    return __date_string;
}


/*
 * Return an ASCII representation of a UNIX time, i.e. the number
 * of seconds since the 1.1.1970
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS: none
 */
char* ctime(const time_t *clock) {
    return asctime(localtime(clock));
}
