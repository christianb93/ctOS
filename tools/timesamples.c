/*
 * timesamples.c
 *
 * Create a few sample Unix time stamps for unit testing of time and date handling routines
 *
 * Invoke with TZ="UTC" ./tools/timesamples
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>

int main() {
    struct tm mytime;
    struct tm* timeptr;
    struct timeval tv;
    time_t res;
    char buffer[256];
    /*
     * 29.11.1911
     */
    mytime.tm_year = 111;
    mytime.tm_mon = 10;
    mytime.tm_mday = 29;
    mytime.tm_hour = 0;
    mytime.tm_min = 0;
    mytime.tm_sec = 0;
    mytime.tm_isdst = 0;
    mytime.tm_yday = 0;
    res = mktime(&mytime);
    printf("Unix time for 29.11.2011: %d, time->tm_yday=%d\n", (int) res, mytime.tm_yday);
    mytime.tm_mday = 1;
    mytime.tm_mon = 1;
    mytime.tm_isdst = 0;
    res = mktime(&mytime);
    printf("Unix time for 1.2.2011: %d, time->tm_yday=%d\n", (int) res, mytime.tm_yday);
    strftime(buffer, 256, "C locale representation: %c\n", &mytime);
    printf("%s", buffer);
    mytime.tm_mday = 1;
    mytime.tm_mon = 10;
    mytime.tm_isdst = 0;
    res = mktime(&mytime);
    printf("Unix time for 1.11.2011: %d, time->tm_yday=%d\n", (int) res, mytime.tm_yday);
    mytime.tm_mday = 1;
    mytime.tm_mon = 0;
    mytime.tm_isdst = 0;
    res = mktime(&mytime);
    printf("Unix time for 1.1.2011: %d, time->tm_yday=%d\n", (int) res, mytime.tm_yday);
    gettimeofday(&tv, 0);
    printf("Seconds since epoch: %ld\n", tv.tv_sec);
    printf("Microseconds: %ld\n", tv.tv_usec);
    time(&res);
    timeptr = localtime(&res);
    strftime(buffer, 256, "Current time: %c\n", timeptr);
    printf("%s", buffer);
    strftime(buffer, 256, "%C\n", timeptr);
    printf("%s", buffer);
    strftime(buffer, 256, "%D\n", timeptr);
    printf("%s", buffer);
    strftime(buffer, 256, "%F\n", timeptr);
    printf("%s", buffer);
}

