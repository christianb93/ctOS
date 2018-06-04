/*
 * time.c
 *
 * Some of the POSIX date handling routines
 *
 */

#include "lib/time.h"
#include "lib/os/oscalls.h"
#include "lib/stdio.h"

/*
 * Return value of localtime
 */
static struct tm __localtime;

/*
 * Array with days per month. Recall that month = 0 is January!
 */
static int days_per_month[12]={31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

/*
 * Abbreviated weekday names
 */
static char* short_weekday_names[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

/*
 * Full weekday names
 */
static char* long_weekday_names[7] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };

/*
 * Abbreviated month names
 */
static char* short_month_names[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug",  "Sep", "Oct", "Nov", "Dec"};

/*
 * Full month names
 */
static char* long_month_names[12] = { "January", "February", "March", "April", "May", "June", "July",
        "August",  "September", "October", "November", "December"};

/*
 * Utility function to determine whether a year is a leap year
 * Parameters:
 * @year - the year where 0 is 1900
 * Return value:
 * 1 if the year is a leap year, 0 if it is not a leap year
 */
static int is_leap_year(int year) {
    int act_year = year+1900;
    /*
     * a year is a leap year if it is
     * a multiple of four
     */
    int is_leap_year = (0==(act_year % 4));
    /*
     * unless it is also a multiple of 100 but not of 400
     */
    if (0==(act_year % 100) && (act_year % 400))
        is_leap_year = 0;
    return is_leap_year;
}

/*
 * Utility function - return the days per month
 */
static int days_in_month(int month, int year) {
    int days_in_month = days_per_month[month];
    if (is_leap_year(year) && (1==month))
        days_in_month++;
    return days_in_month;
}

/*
 * Implementation of mktime
 * This function will convert the time passed as an instance of the
 * tm structure into seconds since the epoch. In addition, it will adapt
 * the values of the tm structure so that they are within the defined
 * range and will set the fields tm_wday and tm_yday in the tm structure
 * Parameter:
 * @tm - the time structure ("broken down time")
 * Return value:
 * the number of seconds since the epoch to the point represented by the argument
 * Limitations:
 * - overflows are not handled
 * - negative values in the tm structure are not properly handled
 * - timezone is not properly handled, including daylight savings time
 */
time_t mktime(struct tm* time) {
    unsigned int days_since_epoch = 0;
    int i;
    /*
     * First make sure that the values in the tm structure
     * are within the defined range. Start with seconds, minute, day
     * and month
     */
    time->tm_min += time->tm_sec / 60;
    time->tm_sec = time->tm_sec % 60;
    time->tm_hour += time->tm_min / 60;
    time->tm_min = time->tm_min % 60;
    time->tm_mday += time->tm_hour / 24;
    time->tm_hour = time->tm_hour % 24;
    time->tm_year += time->tm_mon / 12 ;
    time->tm_mon = time->tm_mon % 12 ;
    /*
     * Now fix mday (day within month)
     */
    while (time->tm_mday > days_in_month(time->tm_mon, time->tm_year)) {
        time->tm_mday = time->tm_mday - days_in_month(time->tm_mon, time->tm_year);
        time->tm_mon++;
        /*
         * Increasing the month might again cause an overflow within the date
         * Fix this
         */
        if (time->tm_mon>11) {
            time->tm_mon = 0;
            time->tm_year++;
        }
    }
    /*
     * Now compute  number of days since epoch. We first compute the number
     * of days passed since the 1st of January in the same year
     */
    days_since_epoch = (time->tm_year-70)*365;
    /*
     * Correct leap years. We do this in three steps:
     * - first assume one leap year for each year divisible by four
     * between 1970 and tm_year - this is (tm_year-71)/4+1
     * - then correct by the number of years divisible by 100 which is (tm_year-71) / 100 +1
     * - finally correct by the number of years divisible by 400 ((tm_year+229)/400)
     */
    if (time->tm_year > 0) {
        days_since_epoch += (time->tm_year-71)/4+1;
        days_since_epoch -= (time->tm_year-71)/100+1;
        days_since_epoch += (time->tm_year+229)/400;
    }
    /*
     * Now compute contribution of the current year
     */
    time->tm_yday = 0;
    for (i=0;i<time->tm_mon;i++)
        time->tm_yday+=days_in_month(i, time->tm_year);
    time->tm_yday+=(time->tm_mday-1);
    days_since_epoch+=time->tm_yday;
    /*
     * Finally compute day of week. The 1st of January 1970 was a thursday (day = 4).
     */
    time->tm_wday = ((days_since_epoch % 7) + 4) % 7;
    /*
     * Return number of seconds since epoch
     */
    return days_since_epoch*24*60*60 + time->tm_hour*60*60 + time->tm_min*60 + time->tm_sec;
}

/*
 * Return current time in seconds since the epoch
 *
 * If the argument tloc is different from NULL, the result will also be stored there
 */
time_t time(time_t* tloc) {
    return __ctOS_time(tloc);
}



/*
 * Convert a UNIX time (i.e. seconds since 1.1.1970) to a broken-down time
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * 1) currently, the only supported timezone is UTC
 * 2) no checks are done for overflows
 */
struct tm* localtime(const time_t *timer) {
    __localtime.tm_sec = *timer;
    __localtime.tm_min = 0;
    __localtime.tm_hour = 0;
    __localtime.tm_mday = 1;
    __localtime.tm_mon = 0;
    __localtime.tm_year = 70;
    mktime(&__localtime);
    return &__localtime;
}

struct tm *gmtime(const time_t *timep) {
    return localtime(timep);
}

/*
 * Utility function to place a single character in a string
 * Append char achar to string s at offset bytes_written
 * Make sure that at most maxsize - 1 bytes of s are used, if that limit is exceeded return -1
 * Bytes_written is increased by the number of bytes actually appended
 */
static int append_char(char* s, size_t* bytes_written, int maxsize, char achar) {
    if (*bytes_written < maxsize - 1) {
        s[*bytes_written] = achar;
       (*bytes_written)++;
    }
    else {
        return -1;
    }
    return 0;
}


/*
 * Utility function to place characters in a string
 * Append string astr to string s at offset bytes_written, not copying the trailing
 * zero. Make sure that at most maxsize - 1 bytes of s are used, if that limit is exceeded
 * return -1
 * Bytes_written is increased by the number of bytes actually appended
 */
static int append_string(char* s, size_t* bytes_written, int maxsize, char* astr) {
    char* achar = astr;
    while (*achar) {
        if (*bytes_written < maxsize - 1) {
            s[*bytes_written] = *achar;
            (*bytes_written)++;
        }
        else {
            return -1;
        }
        achar++;
    }
    return 0;
}

/*
 * Append a decimal to a string, printing the last @width digits of the number. If @space is set, leading
 * 0s are converted into spaces
 */
static int append_dec(char* s, size_t *bytes_written, int maxsize, unsigned int number, int width, int space) {
    unsigned int mask = 1;
    int digit = 0;
    int significant = 0;
    char c;
    int i;
    for (i = 1; i < width; i++)
        mask = mask * 10;
    for (i = 0; i < width; i++) {
        digit = (number / mask) % 10;
        if (digit)
            significant = 1;
        if ((0 == significant) && (space))
            c = ' ';
        else
            c = digit + '0';
        mask = mask / 10;
        if (*bytes_written < maxsize - 1) {
            s[*bytes_written] = c;
            (*bytes_written)++;
        }
        else {
            return -1;
        }
    }
    return 0;
}

/*
 * Append a string representation of a time to a string
 * Parameter:
 * @s - the string
 * @bytes_written - counter for the bytes written, will be updated
 * @maxsize - maximum number of bytes which we can append to the string, including a trailing 0
 * @format - format as specified for strftime
 * @timeptr - the time
 * Return value:
 * 0 upon success or -1 if an error occurred
 */
static int do_strftime(char* s, size_t maxsize, size_t* bytes_written, const char* format, const struct tm* timeptr) {
    char* format_ptr = (char*) format;
    if (0 == maxsize)
        return -1;
    if (0 == timeptr)
        return -1;
    while(*format_ptr) {
        /*
         * If that is a conversion specification, advance to next char
         * and evaluate based on expression
         */
        if ('%' == *format_ptr) {
            format_ptr++;
            if (0 == *format_ptr)
                return -1;
            /*
             * Skip E and 0
             */
            if (('0' == *format_ptr) || ('E' == *format_ptr)) {
                format_ptr++;
                if (0 == *format_ptr)
                    return -1;
            }
            switch(*format_ptr) {
                case 'a':
                    if (-1 == append_string(s, bytes_written, maxsize, short_weekday_names[timeptr->tm_wday % 7]))
                        return -1;
                    break;
                case 'A':
                    if (-1 == append_string(s, bytes_written, maxsize, long_weekday_names[timeptr->tm_wday % 7]))
                        return -1;
                    break;
                case 'b':
                case 'h':
                    if (-1 == append_string(s, bytes_written, maxsize, short_month_names[timeptr->tm_mon % 12]))
                        return -1;
                    break;
                case 'B':
                    if (-1 == append_string(s, bytes_written, maxsize, long_month_names[timeptr->tm_mon % 12]))
                        return -1;
                    break;
                case 'c':
                    /*
                     * In the C locale, this is %a %b %e %T %Y
                     */
                    if (-1 == do_strftime(s, maxsize, bytes_written, "%a %b %e %T %Y", timeptr))
                        return -1;
                    break;
                case 'C':
                    if (-1 == append_dec(s, bytes_written, maxsize, (timeptr->tm_year + 1900) / 100, 2, 0))
                        return -1;
                    break;
                case 'd':
                    if (-1 == append_dec(s, bytes_written, maxsize, timeptr->tm_mday, 2, 0))
                        return -1;
                    break;
                case 'D':
                    /*
                     * This is %m/%d/%y
                     */
                    if (-1 == do_strftime(s, maxsize, bytes_written, "%m/%d/%y", timeptr))
                        return -1;
                    break;
                case 'e':
                    if (-1 == append_dec(s, bytes_written, maxsize, timeptr->tm_mday, 2, 1))
                        return -1;
                    break;
                case 'F':
                    /*
                     * This is %Y-%m-%d
                     */
                    if (-1 == do_strftime(s, maxsize, bytes_written, "%Y-%m-%d", timeptr))
                        return -1;
                    break;
                case 'H':
                    if (-1 == append_dec(s, bytes_written, maxsize, timeptr->tm_hour, 2, 0))
                        return -1;
                    break;
                case 'I':
                    if (-1 == append_dec(s, bytes_written, maxsize, ((timeptr->tm_hour + 11) % 12) + 1 , 2, 0))
                        return -1;
                    break;
                case 'j':
                    if (-1 == append_dec(s, bytes_written, maxsize, timeptr->tm_yday + 1 , 3, 0))
                        return -1;
                    break;
                case 'm':
                    if (-1 == append_dec(s, bytes_written, maxsize, timeptr->tm_mon + 1 , 2, 0))
                        return -1;
                    break;
                case 'M':
                    if (-1 == append_dec(s, bytes_written, maxsize, timeptr->tm_min , 2, 0))
                        return -1;
                    break;
                case 'n':
                    if (-1 == append_char(s, bytes_written, maxsize, '\n'))
                        return -1;
                    break;
                case 'p':
                    if (-1 == append_string(s, bytes_written, maxsize, ((timeptr->tm_hour % 24) < 12) ? "AM" : "PM"))
                        return -1;
                    break;
                case 'S':
                    if (-1 == append_dec(s, bytes_written, maxsize, timeptr->tm_sec , 2, 0))
                        return -1;
                    break;
                case 'r':
                    /*
                     * In the POSIX locale, this is %I:%M:%S %p
                     */
                    if (-1 == do_strftime(s, maxsize, bytes_written, "%I:%M:%S %p" , timeptr))
                        return -1;
                    break;
                case 'R':
                    /*
                     * In the POSIX locale, this is %H:%M
                     */
                    if (-1 == do_strftime(s, maxsize, bytes_written, "%H:%M" , timeptr))
                        return -1;
                    break;
                case 't':
                    if (-1 == append_char(s, bytes_written, maxsize, '\t'))
                        return -1;
                    break;
                case 'T':
                case 'X':
                    if (-1 == do_strftime(s, maxsize, bytes_written, "%H:%M:%S" , timeptr))
                        return -1;
                    break;
                case 'u':
                    if (-1 == append_dec(s, bytes_written, maxsize, (0 == timeptr->tm_wday) ? 7 : timeptr->tm_wday, 1, 0))
                        return -1;
                    break;
                case 'w':
                    if (-1 == append_dec(s, bytes_written, maxsize, timeptr->tm_wday, 1, 0))
                        return -1;
                    break;
                case 'x':
                    if (-1 == do_strftime(s, maxsize, bytes_written, "%m/%d/%y" , timeptr))
                        return -1;
                    break;
                case 'y':
                    if (-1 == append_dec(s, bytes_written, maxsize, timeptr->tm_year, 2, 0))
                        return -1;
                    break;
                case 'Y':
                    if (-1 == append_dec(s, bytes_written, maxsize, timeptr->tm_year + 1900, 4, 0))
                        return -1;
                    break;
                case 'z':
                    break;
                case 'Z':
                    break;
                case '%':
                    if (-1 == append_char(s, bytes_written, maxsize, '%'))
                        return -1;
                    break;
                default:
                    return -1;
            }
        }
        else {
            /*
             * Ordinary character - just copy
             */
            if (-1 == append_char(s, bytes_written, maxsize, *format_ptr))
                return -1;
        }
        format_ptr++;
    }
    return 0;
}

/*
 * Format a time as a string
 *
 * This function appends bytes to the array pointed to by the first argument as controlled by the third argument format. This
 * format is a string containing zero or more conversion specification  - i.e a % character followed by an optional modifier
 * and a conversion specifier as listed below - and ordinary characters
 *
 * All ordinary characters are copied unchanged to the array, this includes the terminating null byte. In any case, no more than
 * maxsize bytes are placed in the array. Conversion specifications are replaced according to the following rules, using the time
 * values provided in the last parameter
 *
 * %a - replace by the abbreviated weekday name
 * %A - replace by the full weekday name
 * %b - replace with abbreviated month name
 * %B - replace with full month name
 * %c - replace with preferred string representation of current locale
 * %C - replace with century, i.e. year divided by 100 and truncated to an integer
 * %d - replace with day of month as two-digit decimal
 * %D - replace by US string representation MM/DD/YY
 * %e - replace by the day of month as a decimal, leading zeroes are converted to spaces
 * %F - replace by the ISO date format YYYY-MM-DD
 * %h - equivalent to %b
 * %H - replace by the hour as 2-digit decimal number
 * %I - replace by the hour as a 2-digit decimal number 01 - 12
 * %j - replace by the day of the year as a decimal number 001 - 366
 * %m - replace by the month as a two-digit decimal number
 * %M - replace by the minute as a decimal number
 * %n - replace by a newline
 * %p - replace by either AM or PM
 * %r - replace by the time in AM and PM notation
 * %S - replace by the second as a 2-digit decimal number
 * %r - replace by the time in am / pm notation
 * %R - replace by the time in 24 hour notation
 * %t - replaced by a tab
 * %T - replaced by the time (%H:%M:%S)
 * %u - replace by the day in the week as a decimal number, 1 is Monday, 7 is Sunday
 * %w - replace by the weekday as a decimal number, 0 is Sunday
 * %x - equivalent to %m/%d/%y in the POSIX locale
 * %X - equivalent to %T in the POSIX locale
 * %y - replace by the last two digits of the year
 * %Y - replace by the year as a four-digit decimal number
 *
 * The modifiers E and 0 can be used to enforce the usage of an alternative representation if that alternative representation
 * exists in the current locale
 *
 * If the operation can be completed writing at most maxsize bytes, the number of bytes written will be returned, not including
 * the terminating null byte. Otherwise, 0 is returned and the contents of the array are unspecified.
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * - no locale support, the C locale is used throughout
 * - the E and 0 modifiers are correctly parsed but ignored
 * - the conversion specifier %g, %G, %U, %V, %W (week-based years) are not supported
 * - the conversion specifiers %z and %Z related to timezone information are implemented as empty string
 */
size_t strftime(char* s, size_t maxsize, const char* format, const struct tm* timeptr) {
    size_t bytes_written = 0;
    if (-1 == do_strftime(s, maxsize, &bytes_written, format, timeptr))
        return 0;
    s[bytes_written] = 0;
    return bytes_written;
}

/*
 * Set the current timezone based on the value of the environment variable TZ
 * As only UTC is currently supported by ctOS, this function does nothing
 */
void tzset() {

}



