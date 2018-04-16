/*
 * test_time.c
 *
 * Test POSIX date and time routines
 */

#include "lib/time.h"
#include "kunit.h"
#include <stdio.h>
#include <string.h>

/*
 * Dummy for __ctOS_time
 */
time_t __ctOS_time(time_t* tloc) {
    if (tloc)
        *tloc = 1000;
    return 1000;
}

/*
 * Testcase 1
 * Tested function: mktime
 * Test case: correct adjustment of seconds
 */
int testcase1() {
    struct tm time;
    time_t res;
    /*
     * Set time to 12:13:60 on
     * 1.8.2011
     */
    time.tm_hour = 12;
    time.tm_min = 13;
    time.tm_sec = 60;
    time.tm_year = 2011 - 1900;
    time.tm_mday = 1;
    time.tm_mon = 7;
    res = mktime(&time);
    ASSERT(time.tm_year == 2011-1900);
    ASSERT(time.tm_mon==7);
    ASSERT(time.tm_mday==1);
    ASSERT(time.tm_hour==12);
    ASSERT(time.tm_min==14);
    ASSERT(time.tm_sec==0);
    return 0;
}

/*
 * Testcase 2
 * Tested function: mktime
 * Test case: no wrap around if seconds is 59
 */
int testcase2() {
    struct tm time;
    time_t res;
    /*
     * Set time to 12:13:59 on
     * 1.8.2011
     */
    time.tm_hour = 12;
    time.tm_min = 13;
    time.tm_sec = 59;
    time.tm_year = 2011 - 1900;
    time.tm_mday = 1;
    time.tm_mon = 7;
    res = mktime(&time);
    ASSERT(time.tm_year == 2011-1900);
    ASSERT(time.tm_mon==7);
    ASSERT(time.tm_mday==1);
    ASSERT(time.tm_hour==12);
    ASSERT(time.tm_min==13);
    ASSERT(time.tm_sec==59);
    return 0;
}

/*
 * Testcase 3
 * Tested function: mktime
 * Test case: wrap around minutes
 */
int testcase3() {
    struct tm time;
    time_t res;
    /*
     * Set time to 12:60:00 on
     * 1.8.2011
     */
    time.tm_hour = 12;
    time.tm_min = 60;
    time.tm_sec = 00;
    time.tm_year = 2011 - 1900;
    time.tm_mday = 1;
    time.tm_mon = 7;
    res = mktime(&time);
    ASSERT(time.tm_year == 2011-1900);
    ASSERT(time.tm_mon==7);
    ASSERT(time.tm_mday==1);
    ASSERT(time.tm_hour==13);
    ASSERT(time.tm_min==0);
    ASSERT(time.tm_sec==0);
    return 0;
}


/*
 * Testcase 4
 * Tested function: mktime
 * Test case: wrap around hours
 */
int testcase4() {
    struct tm time;
    time_t res;
    /*
     * Set time to 24:13:00 on
     * 1.8.2011
     */
    time.tm_hour = 24;
    time.tm_min = 13;
    time.tm_sec = 00;
    time.tm_year = 2011 - 1900;
    time.tm_mday = 1;
    time.tm_mon = 7;
    res = mktime(&time);
    ASSERT(time.tm_year == 2011-1900);
    ASSERT(time.tm_mon==7);
    ASSERT(time.tm_mday==2);
    ASSERT(time.tm_hour==00);
    ASSERT(time.tm_min==13);
    ASSERT(time.tm_sec==0);
    return 0;
}

/*
 * Testcase 5
 * Tested function: mktime
 * Test case: wrap around days
 */
int testcase5() {
    struct tm time;
    time_t res;
    /*
     * Set time to 12:13:00 on
     * 32.8.2011
     */
    time.tm_hour = 12;
    time.tm_min = 13;
    time.tm_sec = 00;
    time.tm_year = 2011 - 1900;
    time.tm_mday = 32;
    time.tm_mon = 7;
    res = mktime(&time);
    ASSERT(time.tm_year == 2011-1900);
    ASSERT(time.tm_mon==8);
    ASSERT(time.tm_mday==1);
    ASSERT(time.tm_hour==12);
    ASSERT(time.tm_min==13);
    ASSERT(time.tm_sec==0);
    return 0;
}

/*
 * Testcase 6
 * Tested function: mktime
 * Test case: wrap around months
 */
int testcase6() {
    struct tm time;
    time_t res;
    /*
     * Set time to 12:13:00 on
     * 1.13.2011
     */
    time.tm_hour = 12;
    time.tm_min = 13;
    time.tm_sec = 00;
    time.tm_year = 2011 - 1900;
    time.tm_mday = 1;
    time.tm_mon = 12;
    res = mktime(&time);
    ASSERT(time.tm_year == 2012-1900);
    ASSERT(time.tm_mon==0);
    ASSERT(time.tm_mday==1);
    ASSERT(time.tm_hour==12);
    ASSERT(time.tm_min==13);
    ASSERT(time.tm_sec==0);
    return 0;
}

/*
 * Testcase 7
 * Tested function: mktime
 * Test case: cascading wrap around
 */
int testcase7() {
    struct tm time;
    time_t res;
    /*
     * Set time to 23:59:60 on
     * 31.12.1999
     */
    time.tm_hour = 23;
    time.tm_min = 59;
    time.tm_sec = 60;
    time.tm_year = 1999 - 1900;
    time.tm_mday = 31;
    time.tm_mon = 11;
    res = mktime(&time);
    ASSERT(time.tm_year == 2000-1900);
    ASSERT(time.tm_mon==0);
    ASSERT(time.tm_mday==1);
    ASSERT(time.tm_hour==0);
    ASSERT(time.tm_min==0);
    ASSERT(time.tm_sec==0);
    return 0;
}

/*
 * Testcase 8
 * Tested function: mktime
 * Test case: compute Unix time for 1.1.1970
 */
int testcase8() {
    struct tm time;
    time_t res;
    /*
     * Set time to 00:00:00 on
     * 1.1.1970
     */
    time.tm_hour = 0;
    time.tm_min = 0;
    time.tm_sec = 0;
    time.tm_year = 70;
    time.tm_mday = 1;
    time.tm_mon = 0;
    res = mktime(&time);
    ASSERT(res==0);
    ASSERT(time.tm_wday==4);
    return 0;
}

/*
 * Testcase 9
 * Tested function: mktime
 * Test case: compute Unix time for 1.1.1970 at a later point of this day
 */
int testcase9() {
    struct tm time;
    time_t res;
    /*
     * Set time to 11:11:11 on
     * 1.1.1900
     */
    time.tm_hour = 11;
    time.tm_min = 11;
    time.tm_sec = 11;
    time.tm_year = 70;
    time.tm_mday = 1;
    time.tm_mon = 0;
    res = mktime(&time);
    ASSERT(res==11+11*60+11*60*60);
    ASSERT(time.tm_wday==4);
    return 0;
}

/*
 * Testcase 10
 * Tested function: mktime
 * Test case: compute Unix time for 29.2.1970
 * As 1970 is NOT a leap year, this should adjust
 * tm_mon to 2 - weekday should be a sunday
 */
int testcase10() {
    struct tm time;
    time_t res;
    /*
     * Set time to 00:00:00 on
     * 29.2.1900
     */
    time.tm_hour = 0;
    time.tm_min = 0;
    time.tm_sec = 0;
    time.tm_year = 70;
    time.tm_mday = 29;
    time.tm_mon = 1;
    res = mktime(&time);
    ASSERT(time.tm_mon==2);
    ASSERT(res==(28+31)*24*60*60);
    ASSERT(time.tm_wday==0);
    return 0;
}

/*
 * Testcase 11
 * Tested function: mktime
 * Test case: compute Unix time for 1.1.2011
 */
int testcase11() {
    struct tm time;
    time_t res;
    /*
     * Set time to 00:00:00 on
     * 1.1.2011
     */
    time.tm_hour = 0;
    time.tm_min = 0;
    time.tm_sec = 0;
    time.tm_year = 111;
    time.tm_mday = 1;
    time.tm_mon = 0;
    res = mktime(&time);
    ASSERT(time.tm_yday==0);
    ASSERT(res==1293840000);
    ASSERT(time.tm_wday==6);
    return 0;
}

/*
 * Testcase 12
 * Tested function: mktime
 * Test case: compute Unix time for 1.2.2011
 */
int testcase12() {
    struct tm time;
    time_t res;
    /*
     * Set time to 00:00:00 on
     * 1.2.2011
     */
    time.tm_hour = 0;
    time.tm_min = 0;
    time.tm_sec = 0;
    time.tm_year = 111;
    time.tm_mday = 1;
    time.tm_mon = 1;
    res = mktime(&time);
    ASSERT(time.tm_yday==31);
    ASSERT(res==1296518400);
    ASSERT(time.tm_wday==2);
    return 0;
}

/*
 * Testcase 13
 * Tested function: mktime
 * Test case: compute Unix time for 1.11.2011
 */
int testcase13() {
    struct tm time;
    time_t res;
    /*
     * Set time to 00:00:00 on
     * 1.11.2011
     */
    time.tm_hour = 0;
    time.tm_min = 0;
    time.tm_sec = 0;
    time.tm_year = 111;
    time.tm_mday = 1;
    time.tm_mon = 10;
    res = mktime(&time);
    ASSERT(time.tm_yday==304);
    ASSERT(res==1320105600);
    ASSERT(time.tm_wday==2);
    return 0;
}


/*
 * Testcase 14
 * Tested function: mktime
 * Test case: compute Unix time for 29.11.2011
 */
int testcase14() {
    struct tm time;
    time_t res;
    /*
     * Set time to 00:00:00 on
     * 29.11.2011
     */
    time.tm_hour = 0;
    time.tm_min = 0;
    time.tm_sec = 0;
    time.tm_year = 111;
    time.tm_mday = 29;
    time.tm_mon = 10;
    res = mktime(&time);
    ASSERT(time.tm_yday==332);
    ASSERT(res==1322524800);
    ASSERT(time.tm_wday==2);
    return 0;
}

/*
 * Testcase 15
 * Tested function: localtime
 * Test case: compute broken down time 1.2.2011 (=1296518400)
 */
int testcase15() {
    time_t mytime = 1296518400;
    struct tm* time_ptr = localtime(&mytime);
    ASSERT(time_ptr->tm_year==111);
    ASSERT(time_ptr->tm_mon==1);
    ASSERT(time_ptr->tm_mday==1);
    ASSERT(time_ptr->tm_hour==0);
    ASSERT(time_ptr->tm_min==0);
    ASSERT(time_ptr->tm_sec==0);
    return 0;
}

/*
 * Testcase 16: strftime - ordinary characters
 */
int testcase16() {
    struct tm mytime;
    unsigned char buffer[256];
    int i;
    int rc;
    memset((void*) buffer, 0x2f, 256);
    ASSERT(3 == strftime(buffer, 4, "abc", &mytime));
    ASSERT(0 == strcmp("abc", buffer));
    /*
     * Should not have touched any bytes in buffer after byte 3
     */
    for (i = 4; i < 256; i++)
        ASSERT(buffer[i] == 0x2f);
    return 0;
}

/*
 * Testcase 17: strftime - ordinary characters, overflow
 */
int testcase17() {
    int i;
    struct tm mytime;
    unsigned char buffer[256];
    memset((void*) buffer, 0x2f, 256);
    ASSERT(0 == strftime(buffer, 3, "abc", &mytime));
    /*
     * Should not have touched any bytes in buffer after byte 2
     */
    for (i = 3; i < 256; i++)
        ASSERT(buffer[i] == 0x2f);
    return 0;
}

/*
 * Testcase 18
 * Tested function: strftime - %a
 */
int testcase18() {
    int i;
    char buffer[256];
    struct tm mytime;
    /*
     * Init time structure to Tue, 1.2.2011
     */
    mytime.tm_year = 111;
    mytime.tm_mon = 1;
    mytime.tm_mday = 1;
    mytime.tm_hour = 0;
    mytime.tm_min = 0;
    mytime.tm_sec = 0;
    mytime.tm_wday = 2;
    /*
     * Call strftime
     */
    memset((void*) buffer, 0x2f, 256);
    ASSERT(strlen("Tue") == strftime(buffer, 4, "%a", &mytime));
    /*
     * Should not have touched any bytes in buffer after byte 3
     */
    for (i = 4; i < 256; i++)
        ASSERT(buffer[i] == 0x2f);
    ASSERT(0 == strcmp(buffer, "Tue"));
    return 0;
}

/*
 * Testcase 19
 * Tested function: strftime - %a, mix with ordinary characters
 */
int testcase19() {
    int i;
    char buffer[256];
    struct tm mytime;
    /*
     * Init time structure to Wed, 2.2.2011
     */
    mytime.tm_year = 111;
    mytime.tm_mon = 1;
    mytime.tm_mday = 2;
    mytime.tm_hour = 0;
    mytime.tm_min = 0;
    mytime.tm_sec = 0;
    mytime.tm_wday = 3;
    /*
     * Call strftime
     */
    memset((void*) buffer, 0x2f, 256);
    ASSERT(strlen("XWedY") == strftime(buffer, 6, "X%aY", &mytime));
    /*
     * Should not have touched any bytes in buffer after byte 5
     */
    for (i = 6; i < 256; i++)
        ASSERT(buffer[i] == 0x2f);
    ASSERT(0 == strcmp(buffer, "XWedY"));
    return 0;
}

/*
 * Testcase 20
 * Tested function: strftime - %A
 */
int testcase20() {
    int i;
    char buffer[256];
    struct tm mytime;
    /*
     * Init time structure to Tue, 1.2.2011
     */
    mytime.tm_year = 111;
    mytime.tm_mon = 1;
    mytime.tm_mday = 1;
    mytime.tm_hour = 0;
    mytime.tm_min = 0;
    mytime.tm_sec = 0;
    mytime.tm_wday = 2;
    /*
     * Call strftime
     */
    memset((void*) buffer, 0x2f, 256);
    ASSERT(strlen("Tuesday") == strftime(buffer, strlen("Tuesday") + 1, "%A", &mytime));
    /*
     * Should not have touched any bytes in buffer after byte 7
     */
    for (i = 8; i < 256; i++)
        ASSERT(buffer[i] == 0x2f);
    ASSERT(0 == strcmp(buffer, "Tuesday"));
    return 0;
}

/*
 * Testcase 21
 * Tested function: strftime - %A, overflow
 */
int testcase21() {
    int i;
    char buffer[256];
    struct tm mytime;
    /*
     * Init time structure to Tue, 1.2.2011
     */
    mytime.tm_year = 111;
    mytime.tm_mon = 1;
    mytime.tm_mday = 1;
    mytime.tm_hour = 0;
    mytime.tm_min = 0;
    mytime.tm_sec = 0;
    mytime.tm_wday = 2;
    /*
     * Call strftime
     */
    memset((void*) buffer, 0x2f, 256);
    ASSERT(0 == strftime(buffer, 7, "%A", &mytime));
    /*
     * Should not have touched any bytes in buffer after byte 6
     */
    for (i = 7; i < 256; i++)
        ASSERT(buffer[i] == 0x2f);
    return 0;
}


/*
 * Testcase 22
 * Tested function: strftime - %b
 */
int testcase22() {
    int i;
    char buffer[256];
    struct tm mytime;
    /*
     * Init time structure to Tue, 1.2.2011
     */
    mytime.tm_year = 111;
    mytime.tm_mon = 1;
    mytime.tm_mday = 1;
    mytime.tm_hour = 0;
    mytime.tm_min = 0;
    mytime.tm_sec = 0;
    mytime.tm_wday = 2;
    /*
     * Call strftime
     */
    memset((void*) buffer, 0x2f, 256);
    ASSERT(strlen("Feb") == strftime(buffer, 4, "%b", &mytime));
    /*
     * Should not have touched any bytes in buffer after byte 3
     */
    for (i = 4; i < 256; i++)
        ASSERT(buffer[i] == 0x2f);
    ASSERT(0 == strcmp(buffer, "Feb"));
    return 0;
}

/*
 * Testcase 23
 * Tested function: strftime - %B
 */
int testcase23() {
    int i;
    char buffer[256];
    struct tm mytime;
    /*
     * Init time structure to Tue, 1.2.2011
     */
    mytime.tm_year = 111;
    mytime.tm_mon = 1;
    mytime.tm_mday = 1;
    mytime.tm_hour = 0;
    mytime.tm_min = 0;
    mytime.tm_sec = 0;
    mytime.tm_wday = 2;
    /*
     * Call strftime
     */
    memset((void*) buffer, 0x2f, 256);
    ASSERT(strlen("February") == strftime(buffer, strlen("February") + 1, "%B", &mytime));
    /*
     * Should not have touched any bytes in buffer after byte strlen("February")
     */
    for (i = strlen("February") + 1; i < 256; i++)
        ASSERT(buffer[i] == 0x2f);
    ASSERT(0 == strcmp(buffer, "February"));
    return 0;
}

/*
 * Testcase 24
 * Tested function: strftime - %c
 */
int testcase24() {
    int i;
    char buffer[256];
    struct tm mytime;
    /*
     * Init time structure to Tue, 1.2.2011, 20:31:21
     */
    mytime.tm_year = 111;
    mytime.tm_mon = 1;
    mytime.tm_mday = 1;
    mytime.tm_hour = 20;
    mytime.tm_min = 31;
    mytime.tm_sec = 21;
    mytime.tm_wday = 2;
    /*
     * Call strftime
     */
    memset((void*) buffer, 0x2f, 256);
    ASSERT(strftime(buffer, 256, "%c", &mytime));
    ASSERT(0 == strcmp(buffer, "Tue Feb  1 20:31:21 2011"));
    return 0;
}

/*
 * Testcase 25
 * Tested function: strftime - %c
 */
int testcase25() {
    int i;
    char buffer[256];
    struct tm mytime;
    /*
     * Init time structure to Tue, 15.2.2011, 20:31:21
     */
    mytime.tm_year = 111;
    mytime.tm_mon = 1;
    mytime.tm_mday = 15;
    mytime.tm_hour = 20;
    mytime.tm_min = 31;
    mytime.tm_sec = 21;
    mytime.tm_wday = 2;
    /*
     * Call strftime
     */
    memset((void*) buffer, 0x2f, 256);
    ASSERT(strftime(buffer, 256, "%c", &mytime));
    ASSERT(0 == strcmp(buffer, "Tue Feb 15 20:31:21 2011"));
    return 0;
}

/*
 * Testcase 26
 * Tested function: strftime - %C
 */
int testcase26() {
    int i;
    char buffer[256];
    struct tm mytime;
    /*
     * Init time structure to Tue, 15.2.2011, 20:31:21
     */
    mytime.tm_year = 111;
    mytime.tm_mon = 1;
    mytime.tm_mday = 15;
    mytime.tm_hour = 20;
    mytime.tm_min = 31;
    mytime.tm_sec = 21;
    mytime.tm_wday = 2;
    /*
     * Call strftime
     */
    memset((void*) buffer, 0x2f, 256);
    ASSERT(2 == strftime(buffer, 256, "%C", &mytime));
    ASSERT(0 == strcmp(buffer, "20"));
    return 0;
}

/*
 * Testcase 27
 * Tested function: strftime - %d
 */
int testcase27() {
    int i;
    char buffer[256];
    struct tm mytime;
    /*
     * Init time structure to Tue, 15.2.2011, 20:31:21
     */
    mytime.tm_year = 111;
    mytime.tm_mon = 1;
    mytime.tm_mday = 15;
    mytime.tm_hour = 20;
    mytime.tm_min = 31;
    mytime.tm_sec = 21;
    mytime.tm_wday = 2;
    /*
     * Call strftime
     */
    memset((void*) buffer, 0x2f, 256);
    ASSERT(2 == strftime(buffer, 256, "%d", &mytime));
    ASSERT(0 == strcmp(buffer, "15"));
    /*
     * Same with a one-digit day
     */
    mytime.tm_mday = 1;
    ASSERT(2 == strftime(buffer, 256, "%d", &mytime));
    ASSERT(0 == strcmp(buffer, "01"));
    return 0;
}

/*
 * Testcase 28
 * Tested function: strftime - %D
 */
int testcase28() {
    int i;
    char buffer[256];
    struct tm mytime;
    /*
     * Init time structure to Tue, 15.2.2011, 20:31:21
     */
    mytime.tm_year = 111;
    mytime.tm_mon = 1;
    mytime.tm_mday = 15;
    mytime.tm_hour = 20;
    mytime.tm_min = 31;
    mytime.tm_sec = 21;
    mytime.tm_wday = 2;
    /*
     * Call strftime
     */
    memset((void*) buffer, 0x2f, 256);
    ASSERT(strftime(buffer, 256, "%D", &mytime));
    ASSERT(0 == strcmp(buffer, "02/15/11"));
    return 0;
}

/*
 * Testcase 29
 * Tested function: strftime - %e
 */
int testcase29() {
    int i;
    char buffer[256];
    struct tm mytime;
    /*
     * Init time structure to Tue, 15.2.2011, 20:31:21
     */
    mytime.tm_year = 111;
    mytime.tm_mon = 1;
    mytime.tm_mday = 15;
    mytime.tm_hour = 20;
    mytime.tm_min = 31;
    mytime.tm_sec = 21;
    mytime.tm_wday = 2;
    /*
     * Call strftime
     */
    memset((void*) buffer, 0x2f, 256);
    ASSERT(2 == strftime(buffer, 256, "%e", &mytime));
    ASSERT(0 == strcmp(buffer, "15"));
    /*
     * Try again with one digit
     */
    mytime.tm_mday = 1;
    memset((void*) buffer, 0x2f, 256);
    ASSERT(2 == strftime(buffer, 256, "%e", &mytime));
    ASSERT(0 == strcmp(buffer, " 1"));
    return 0;
}

/*
 * Testcase 30
 * Tested function: strftime - %F
 */
int testcase30() {
    int i;
    char buffer[256];
    struct tm mytime;
    /*
     * Init time structure to Tue, 15.2.2011, 20:31:21
     */
    mytime.tm_year = 111;
    mytime.tm_mon = 1;
    mytime.tm_mday = 15;
    mytime.tm_hour = 20;
    mytime.tm_min = 31;
    mytime.tm_sec = 21;
    mytime.tm_wday = 2;
    /*
     * Call strftime
     */
    memset((void*) buffer, 0x2f, 256);
    ASSERT(strftime(buffer, 256, "%F", &mytime));
    ASSERT(0 == strcmp(buffer, "2011-02-15"));
    return 0;
}

/*
 * Testcase 31
 * Tested function: strftime - %H
 */
int testcase31() {
    int i;
    char buffer[256];
    struct tm mytime;
    /*
     * Init time structure to Tue, 15.2.2011, 22:31:21
     */
    mytime.tm_year = 111;
    mytime.tm_mon = 1;
    mytime.tm_mday = 15;
    mytime.tm_hour = 22;
    mytime.tm_min = 31;
    mytime.tm_sec = 21;
    mytime.tm_wday = 2;
    /*
     * Call strftime
     */
    memset((void*) buffer, 0x2f, 256);
    ASSERT(strftime(buffer, 256, "%H", &mytime));
    ASSERT(0 == strcmp(buffer, "22"));
    return 0;
}

/*
 * Testcase 32
 * Tested function: strftime - %I
 */
int testcase32() {
    int i;
    char buffer[256];
    struct tm mytime;
    /*
     * Init time structure to Tue, 15.2.2011, 22:31:21
     */
    mytime.tm_year = 111;
    mytime.tm_mon = 1;
    mytime.tm_mday = 15;
    mytime.tm_hour = 22;
    mytime.tm_min = 31;
    mytime.tm_sec = 21;
    mytime.tm_wday = 2;
    /*
     * Call strftime
     */
    memset((void*) buffer, 0x2f, 256);
    ASSERT(strftime(buffer, 256, "%I", &mytime));
    ASSERT(0 == strcmp(buffer, "10"));
    /*
     * Try more values for the hour
     */
    mytime.tm_hour = 0;
    memset((void*) buffer, 0x2f, 256);
    ASSERT(strftime(buffer, 256, "%I", &mytime));
    ASSERT(0 == strcmp(buffer, "12"));
    mytime.tm_hour = 1;
    memset((void*) buffer, 0x2f, 256);
    ASSERT(strftime(buffer, 256, "%I", &mytime));
    ASSERT(0 == strcmp(buffer, "01"));
    mytime.tm_hour = 23;
    memset((void*) buffer, 0x2f, 256);
    ASSERT(strftime(buffer, 256, "%I", &mytime));
    ASSERT(0 == strcmp(buffer, "11"));
    mytime.tm_hour = 24;
    memset((void*) buffer, 0x2f, 256);
    ASSERT(strftime(buffer, 256, "%I", &mytime));
    ASSERT(0 == strcmp(buffer, "12"));
    mytime.tm_hour = 12;
    memset((void*) buffer, 0x2f, 256);
    ASSERT(strftime(buffer, 256, "%I", &mytime));
    ASSERT(0 == strcmp(buffer, "12"));
    mytime.tm_hour = 13;
    memset((void*) buffer, 0x2f, 256);
    ASSERT(strftime(buffer, 256, "%I", &mytime));
    ASSERT(0 == strcmp(buffer, "01"));
    return 0;
}

/*
 * Testcase 33
 * Tested function: strftime - %j
 */
int testcase33() {
    int i;
    char buffer[256];
    struct tm mytime;
    /*
     * Init time structure to Sat, 1.1.2011, 22:31:21
     */
    mytime.tm_year = 111;
    mytime.tm_mon = 0;
    mytime.tm_mday = 1;
    mytime.tm_hour = 22;
    mytime.tm_min = 31;
    mytime.tm_sec = 21;
    mytime.tm_wday = 6;
    mytime.tm_yday = 0;
    /*
     * Call strftime
     */
    memset((void*) buffer, 0x2f, 256);
    ASSERT(3 == strftime(buffer, 256, "%j", &mytime));
    ASSERT(0 == strcmp(buffer, "001"));
    return 0;
}

/*
 * Testcase 34
 * Tested function: strftime - %m
 */
int testcase34() {
    int i;
    char buffer[256];
    struct tm mytime;
    /*
     * Init time structure to Sat, 1.1.2011, 22:31:21
     */
    mytime.tm_year = 111;
    mytime.tm_mon = 0;
    mytime.tm_mday = 1;
    mytime.tm_hour = 22;
    mytime.tm_min = 31;
    mytime.tm_sec = 21;
    mytime.tm_wday = 6;
    mytime.tm_yday = 0;
    /*
     * Call strftime
     */
    memset((void*) buffer, 0x2f, 256);
    ASSERT(2 == strftime(buffer, 256, "%m", &mytime));
    ASSERT(0 == strcmp(buffer, "01"));
    return 0;
}

/*
 * Testcase 35
 * Tested function: strftime - %M
 */
int testcase35() {
    int i;
    char buffer[256];
    struct tm mytime;
    /*
     * Init time structure to Sat, 1.1.2011, 22:31:21
     */
    mytime.tm_year = 111;
    mytime.tm_mon = 0;
    mytime.tm_mday = 1;
    mytime.tm_hour = 22;
    mytime.tm_min = 31;
    mytime.tm_sec = 21;
    mytime.tm_wday = 6;
    mytime.tm_yday = 0;
    /*
     * Call strftime
     */
    memset((void*) buffer, 0x2f, 256);
    ASSERT(2 == strftime(buffer, 256, "%M", &mytime));
    ASSERT(0 == strcmp(buffer, "31"));
    return 0;
}

/*
 * Testcase 36
 * Tested function: strftime - %n
 */
int testcase36() {
    int i;
    char buffer[256];
    struct tm mytime;
    /*
     * Init time structure to Sat, 1.1.2011, 22:31:21
     */
    mytime.tm_year = 111;
    mytime.tm_mon = 0;
    mytime.tm_mday = 1;
    mytime.tm_hour = 22;
    mytime.tm_min = 31;
    mytime.tm_sec = 21;
    mytime.tm_wday = 6;
    mytime.tm_yday = 0;
    /*
     * Call strftime
     */
    memset((void*) buffer, 0x2f, 256);
    ASSERT(1 == strftime(buffer, 256, "%n", &mytime));
    ASSERT(0 == strcmp(buffer, "\n"));
    return 0;
}

/*
 * Testcase 37
 * Tested function: strftime - %p
 */
int testcase37() {
    int i;
    char buffer[256];
    struct tm mytime;
    /*
     * Init time structure to Sat, 1.1.2011, 22:31:21
     */
    mytime.tm_year = 111;
    mytime.tm_mon = 0;
    mytime.tm_mday = 1;
    mytime.tm_hour = 22;
    mytime.tm_min = 31;
    mytime.tm_sec = 21;
    mytime.tm_wday = 6;
    mytime.tm_yday = 0;
    /*
     * Call strftime
     */
    memset((void*) buffer, 0x2f, 256);
    ASSERT(2 == strftime(buffer, 256, "%p", &mytime));
    ASSERT(0 == strcmp(buffer, "PM"));
    /*
     * Try some other values
     */
    mytime.tm_hour = 0;
    mytime.tm_min = 0;
    memset((void*) buffer, 0x2f, 256);
    ASSERT(2 == strftime(buffer, 256, "%p", &mytime));
    ASSERT(0 == strcmp(buffer, "AM"));
    mytime.tm_hour = 0;
    mytime.tm_min = 1;
    memset((void*) buffer, 0x2f, 256);
    ASSERT(2 == strftime(buffer, 256, "%p", &mytime));
    ASSERT(0 == strcmp(buffer, "AM"));
    mytime.tm_hour = 11;
    mytime.tm_min = 59;
    memset((void*) buffer, 0x2f, 256);
    ASSERT(2 == strftime(buffer, 256, "%p", &mytime));
    ASSERT(0 == strcmp(buffer, "AM"));
    mytime.tm_hour = 12;
    mytime.tm_min = 00;
    memset((void*) buffer, 0x2f, 256);
    ASSERT(2 == strftime(buffer, 256, "%p", &mytime));
    ASSERT(0 == strcmp(buffer, "PM"));
    mytime.tm_hour = 23;
    mytime.tm_min = 59;
    memset((void*) buffer, 0x2f, 256);
    ASSERT(2 == strftime(buffer, 256, "%p", &mytime));
    ASSERT(0 == strcmp(buffer, "PM"));
    return 0;
}

/*
 * Testcase 38
 * Tested function: strftime - %S
 */
int testcase38() {
    int i;
    char buffer[256];
    struct tm mytime;
    /*
     * Init time structure to Sat, 1.1.2011, 22:31:21
     */
    mytime.tm_year = 111;
    mytime.tm_mon = 0;
    mytime.tm_mday = 1;
    mytime.tm_hour = 22;
    mytime.tm_min = 31;
    mytime.tm_sec = 21;
    mytime.tm_wday = 6;
    mytime.tm_yday = 0;
    /*
     * Call strftime
     */
    memset((void*) buffer, 0x2f, 256);
    ASSERT(2 == strftime(buffer, 256, "%S", &mytime));
    ASSERT(0 == strcmp(buffer, "21"));
    return 0;
}

/*
 * Testcase 39
 * Tested function: strftime - %r
 */
int testcase39() {
    int i;
    char buffer[256];
    struct tm mytime;
    /*
     * Init time structure to Sat, 1.1.2011, 22:31:21
     */
    mytime.tm_year = 111;
    mytime.tm_mon = 0;
    mytime.tm_mday = 1;
    mytime.tm_hour = 22;
    mytime.tm_min = 31;
    mytime.tm_sec = 21;
    mytime.tm_wday = 6;
    mytime.tm_yday = 0;
    /*
     * Call strftime
     */
    memset((void*) buffer, 0x2f, 256);
    ASSERT(strftime(buffer, 256, "%r", &mytime));
    ASSERT(0 == strcmp(buffer, "10:31:21 PM"));
    return 0;
}

/*
 * Testcase 40
 * Tested function: strftime - %R
 */
int testcase40() {
    int i;
    char buffer[256];
    struct tm mytime;
    /*
     * Init time structure to Sat, 1.1.2011, 22:31:21
     */
    mytime.tm_year = 111;
    mytime.tm_mon = 0;
    mytime.tm_mday = 1;
    mytime.tm_hour = 22;
    mytime.tm_min = 31;
    mytime.tm_sec = 21;
    mytime.tm_wday = 6;
    mytime.tm_yday = 0;
    /*
     * Call strftime
     */
    memset((void*) buffer, 0x2f, 256);
    ASSERT(strftime(buffer, 256, "%R", &mytime));
    ASSERT(0 == strcmp(buffer, "22:31"));
    return 0;
}

/*
 * Testcase 41
 * Tested function: strftime - %T
 */
int testcase41() {
    int i;
    char buffer[256];
    struct tm mytime;
    /*
     * Init time structure to Sat, 1.1.2011, 22:31:21
     */
    mytime.tm_year = 111;
    mytime.tm_mon = 0;
    mytime.tm_mday = 1;
    mytime.tm_hour = 22;
    mytime.tm_min = 31;
    mytime.tm_sec = 21;
    mytime.tm_wday = 6;
    mytime.tm_yday = 0;
    /*
     * Call strftime
     */
    memset((void*) buffer, 0x2f, 256);
    ASSERT(strftime(buffer, 256, "%T", &mytime));
    ASSERT(0 == strcmp(buffer, "22:31:21"));
    return 0;
}

/*
 * Testcase 42
 * Tested function: strftime - %u
 */
int testcase42() {
    int i;
    char buffer[256];
    struct tm mytime;
    /*
     * Init time structure to Sat, 1.1.2011, 22:31:21
     */
    mytime.tm_year = 111;
    mytime.tm_mon = 0;
    mytime.tm_mday = 1;
    mytime.tm_hour = 22;
    mytime.tm_min = 31;
    mytime.tm_sec = 21;
    mytime.tm_wday = 6;
    mytime.tm_yday = 0;
    /*
     * Call strftime
     */
    memset((void*) buffer, 0x2f, 256);
    ASSERT(strftime(buffer, 256, "%u", &mytime));
    ASSERT(0 == strcmp(buffer, "6"));
    /*
     * Try Sunday and Monday as well
     */
    mytime.tm_wday = 0;
    mytime.tm_mday = 2;
    memset((void*) buffer, 0x2f, 256);
    ASSERT(strftime(buffer, 256, "%u", &mytime));
    ASSERT(0 == strcmp(buffer, "7"));
    mytime.tm_wday = 1;
    mytime.tm_mday = 3;
    memset((void*) buffer, 0x2f, 256);
    ASSERT(strftime(buffer, 256, "%u", &mytime));
    ASSERT(0 == strcmp(buffer, "1"));
    return 0;
}

/*
 * Testcase 43
 * Tested function: strftime - %w
 */
int testcase43() {
    int i;
    char buffer[256];
    struct tm mytime;
    /*
     * Init time structure to Sat, 1.1.2011, 22:31:21
     */
    mytime.tm_year = 111;
    mytime.tm_mon = 0;
    mytime.tm_mday = 1;
    mytime.tm_hour = 22;
    mytime.tm_min = 31;
    mytime.tm_sec = 21;
    mytime.tm_wday = 6;
    mytime.tm_yday = 0;
    /*
     * Call strftime
     */
    memset((void*) buffer, 0x2f, 256);
    ASSERT(strftime(buffer, 256, "%w", &mytime));
    ASSERT(0 == strcmp(buffer, "6"));
    /*
     * Try Sunday and Monday as well
     */
    mytime.tm_wday = 0;
    mytime.tm_mday = 2;
    memset((void*) buffer, 0x2f, 256);
    ASSERT(strftime(buffer, 256, "%w", &mytime));
    ASSERT(0 == strcmp(buffer, "0"));
    mytime.tm_wday = 1;
    mytime.tm_mday = 3;
    memset((void*) buffer, 0x2f, 256);
    ASSERT(strftime(buffer, 256, "%w", &mytime));
    ASSERT(0 == strcmp(buffer, "1"));
    return 0;
}

/*
 * Testcase 44
 * Tested function: strftime - %y
 */
int testcase44() {
    int i;
    char buffer[256];
    struct tm mytime;
    /*
     * Init time structure to Sat, 1.1.2011, 22:31:21
     */
    mytime.tm_year = 111;
    mytime.tm_mon = 0;
    mytime.tm_mday = 1;
    mytime.tm_hour = 22;
    mytime.tm_min = 31;
    mytime.tm_sec = 21;
    mytime.tm_wday = 6;
    mytime.tm_yday = 0;
    /*
     * Call strftime
     */
    memset((void*) buffer, 0x2f, 256);
    ASSERT(strftime(buffer, 256, "%y", &mytime));
    ASSERT(0 == strcmp(buffer, "11"));
    return 0;
}

/*
 * Testcase 45
 * Tested function: strftime - %x
 */
int testcase45() {
    int i;
    char buffer[256];
    struct tm mytime;
    /*
     * Init time structure to Sun, 2.1.2011, 22:31:21
     */
    mytime.tm_year = 111;
    mytime.tm_mon = 0;
    mytime.tm_mday = 2;
    mytime.tm_hour = 22;
    mytime.tm_min = 31;
    mytime.tm_sec = 21;
    mytime.tm_wday = 0;
    mytime.tm_yday = 1;
    /*
     * Call strftime
     */
    memset((void*) buffer, 0x2f, 256);
    ASSERT(strftime(buffer, 256, "%x", &mytime));
    ASSERT(0 == strcmp(buffer, "01/02/11"));
    return 0;
}

/*
 * Testcase 46
 * Tested function: strftime - %Y
 */
int testcase46() {
    int i;
    char buffer[256];
    struct tm mytime;
    /*
     * Init time structure to Sun, 2.1.2011, 22:31:21
     */
    mytime.tm_year = 111;
    mytime.tm_mon = 0;
    mytime.tm_mday = 2;
    mytime.tm_hour = 22;
    mytime.tm_min = 31;
    mytime.tm_sec = 21;
    mytime.tm_wday = 0;
    mytime.tm_yday = 1;
    /*
     * Call strftime
     */
    memset((void*) buffer, 0x2f, 256);
    ASSERT(strftime(buffer, 256, "%Y", &mytime));
    ASSERT(0 == strcmp(buffer, "2011"));
    return 0;
}

/*
 * Testcase 47
 * Tested function: strftime - %Ec
 */
int testcase47() {
    int i;
    char buffer[256];
    struct tm mytime;
    /*
     * Init time structure to Tue, 1.2.2011, 20:31:21
     */
    mytime.tm_year = 111;
    mytime.tm_mon = 1;
    mytime.tm_mday = 1;
    mytime.tm_hour = 20;
    mytime.tm_min = 31;
    mytime.tm_sec = 21;
    mytime.tm_wday = 2;
    /*
     * Call strftime
     */
    memset((void*) buffer, 0x2f, 256);
    ASSERT(strftime(buffer, 256, "%Ec", &mytime));
    ASSERT(0 == strcmp(buffer, "Tue Feb  1 20:31:21 2011"));
    return 0;
}

/*
 * Testcase 48
 * Tested function: strftime - %0c
 */
int testcase48() {
    int i;
    char buffer[256];
    struct tm mytime;
    /*
     * Init time structure to Tue, 1.2.2011, 20:31:21
     */
    mytime.tm_year = 111;
    mytime.tm_mon = 1;
    mytime.tm_mday = 1;
    mytime.tm_hour = 20;
    mytime.tm_min = 31;
    mytime.tm_sec = 21;
    mytime.tm_wday = 2;
    /*
     * Call strftime
     */
    memset((void*) buffer, 0x2f, 256);
    ASSERT(strftime(buffer, 256, "%0c", &mytime));
    ASSERT(0 == strcmp(buffer, "Tue Feb  1 20:31:21 2011"));
    return 0;
}

int main() {
    INIT;
    RUN_CASE(1);
    RUN_CASE(2);
    RUN_CASE(3);
    RUN_CASE(4);
    RUN_CASE(5);
    RUN_CASE(6);
    RUN_CASE(7);
    RUN_CASE(8);
    RUN_CASE(9);
    RUN_CASE(10);
    RUN_CASE(11);
    RUN_CASE(12);
    RUN_CASE(13);
    RUN_CASE(14);
    RUN_CASE(15);
    RUN_CASE(16);
    RUN_CASE(17);
    RUN_CASE(18);
    RUN_CASE(19);
    RUN_CASE(20);
    RUN_CASE(21);
    RUN_CASE(22);
    RUN_CASE(23);
    RUN_CASE(24);
    RUN_CASE(25);
    RUN_CASE(26);
    RUN_CASE(27);
    RUN_CASE(28);
    RUN_CASE(29);
    RUN_CASE(30);
    RUN_CASE(31);
    RUN_CASE(32);
    RUN_CASE(33);
    RUN_CASE(34);
    RUN_CASE(35);
    RUN_CASE(36);
    RUN_CASE(37);
    RUN_CASE(38);
    RUN_CASE(39);
    RUN_CASE(40);
    RUN_CASE(41);
    RUN_CASE(42);
    RUN_CASE(43);
    RUN_CASE(44);
    RUN_CASE(45);
    RUN_CASE(46);
    RUN_CASE(47);
    RUN_CASE(48);
    END;
}
