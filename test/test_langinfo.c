#include "kunit.h"
#include "lib/langinfo.h"


/*
 * Test data and time formats
 */
int testcase1() {
    ASSERT(0 == strcmp(nl_langinfo(D_FMT), "%m/%d/%y"));
    ASSERT(0 == strcmp(nl_langinfo(T_FMT), "%H:%M:%S"));
    ASSERT(0 == strcmp(nl_langinfo(D_T_FMT), "%a %b %e %H:%M:%S %Y"));
    ASSERT(0 == strcmp(nl_langinfo(T_FMT_AMPM), "%I:%M:%S %p"));
    ASSERT(0 == strcmp(nl_langinfo(AM_STR), "AM"));
    ASSERT(0 == strcmp(nl_langinfo(PM_STR), "PM"));
    return 0;
}

/*
 * Test codeset 
 */
int testcase2() {
    ASSERT(0 == strcmp("ANSI_X3.4-1968", nl_langinfo(CODESET)));
    return 0;
}

/*
 * Test days of the week - long form
 */
int testcase3() {
    int i;
    char* days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
    for (i = 0; i < 7; i++) {
        ASSERT(0 == strcmp(days[i], nl_langinfo(DAY_1 + i)));
    }
}

/*
 * Test days of the week - abbreviations
 */
int testcase4() {
    int i;
    char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    for (i = 0; i < 7; i++) {
        ASSERT(0 == strcmp(days[i], nl_langinfo(ABDAY_1 + i)));
    }
}

/*
 * Test months of the year
 */
int testcase5() {
    int i;
    char* months[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };
    for (i = 0; i < 12; i++) {
        ASSERT(0 == strcmp(months[i], nl_langinfo(MON_1 + i)));
    }
}

/*
 * Test months of the year (abbreviated)
 */
int testcase6() {
    int i;
    char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    for (i = 0; i < 12; i++) {
        ASSERT(0 == strcmp(months[i], nl_langinfo(ABMON_1 + i)));
    }
}

/*
 * Some other special characters
 */
int testcase7() {
    ASSERT(0 == strcmp(".",nl_langinfo(RADIXCHAR)));
    ASSERT(0 == strcmp("^[yY]", nl_langinfo(YESEXPR)));
    ASSERT(0 == strcmp("^[nN]", nl_langinfo(NOEXPR)));
    ASSERT(0 == strcmp("-", nl_langinfo(CRNCYSTR)));
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
    END;
}