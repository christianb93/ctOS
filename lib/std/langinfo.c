/*
 * langinfo.c
 */

#include "lib/langinfo.h"

/*
 * This static structure contains the values returned by nl_langinfo
 */
struct __langinfo_t {
    char* codeset;
    char* d_t_fmt;
    char* d_fmt;
    char* t_fmt;
    char* t_fmt_ampm;
    char* am_str;
    char* pm_str;
    const char** days;
    const char** abb_days;
    const char** months;
    const char** abb_months;
    char* radixchar;
    char* yesexpr;
    char* noexpr;
    char* crncystr;
};

static const char* days[] =  {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
static const char* abb_days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static const char* months[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };
static const char* abb_months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

static struct __langinfo_t __langinfo = {
    "ANSI_X3.4-1968",
    "%a %b %e %H:%M:%S %Y",
    "%m/%d/%y",
    "%H:%M:%S",
    "%I:%M:%S %p",
    "AM", 
    "PM",
    days,
    abb_days,
    months, 
    abb_months,
    ".",
    "^[yY]",
    "^[nN]",
    "-"
};

/*
 * The nl_langinfo() function returns a pointer to a string containing information relevant to the particular language or cultural area 
 * defined in the program's locale. The valid constant names and values of item are defined in <langinfo.h>. 
 * 
 * Currently ctOS only supports the C / POSIX locale, therefore values from this locale are returned. If the item is not valid,
 * a pointer to an empty string is returned
 * 
 * Note that the returned pointer might point to static data and must not be manipulated or freed by the callee
 *
 */
char *nl_langinfo(nl_item item) {
    switch(item) {
        case CODESET:
            return "ANSI_X3.4-1968";
        case D_T_FMT: 
            return __langinfo.d_t_fmt;
        case D_FMT:
            return __langinfo.d_fmt;
        case T_FMT:
            return __langinfo.t_fmt;
        case T_FMT_AMPM:
            return __langinfo.t_fmt_ampm;
        case AM_STR:
            return __langinfo.am_str;
        case PM_STR:
            return __langinfo.pm_str;
        case RADIXCHAR:
            return __langinfo.radixchar;
        case YESEXPR:
            return __langinfo.yesexpr;
        case NOEXPR:
            return __langinfo.noexpr;
        case CRNCYSTR:
            return __langinfo.crncystr;
        default:
            break;
    }
    /*
     * Days of the week
     */
    if ((DAY_1 <= item) && (item <= DAY_7))
        return (char*) __langinfo.days[item - DAY_1];
    if ((ABDAY_1 <= item) && (item <= ABDAY_7))
        return (char*) __langinfo.abb_days[item - ABDAY_1];    
    /*
     * Months
     */
    if ((MON_1 <= item) && (item <= MON_12))
        return (char*) __langinfo.months[item - MON_1];
    if ((ABMON_1 <= item) && (item <= ABMON_12))
        return (char*) __langinfo.abb_months[item - ABMON_1];    
    return "";
    
}