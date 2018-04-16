/*
 * rtc.c
 *
 * Functions to handle the real time clock (RTC)
 *
 * To read a register from the RTC, we first need to write the register number (index) to register 0x70 and then read
 * the value from register 0x71.
 *
 * Note that the MSB of register 0x70 also controls NMI. So we need to make sure to change the value back after being done. While
 * reading, we turn all interrupts of, including the NMI, so that we set the NMI disable bit for every read and set it back to
 * its original value
 *
 * The registers relevant for our purposes are:
 *
 * 0   -    seconds
 * 2   -    minutes
 * 4   -    hours
 * 7   -    day
 * 8   -    month
 * 9   -    year
 * a   -    status register A
 * b   -    status register B
 * d   -    status register C
 *
 * Status register A can be used to check whether an update is in progress - if yes, bit 7 is set. This flag is set by the RTC
 * approximately 200 us before the update actually begins, so after checking this flag, we have about 200 us left to complete the
 * read
 *
 * Status register B contains the format:
 *
 * Bit 1 - 24 hour format
 * Bit 2 - binary format if set, BCD if not set
 *
 * In binary format, all numbers are stored as ordinary binary numbers. In BCD, 59 minutes would for instance be stored as 0x59.
 * In 12 hour format, bit 7 of the hour byte is the PM/AM flag (1 = PM)
 *
 * Bit 7 of status register C is indicate if time and date are valid, i.e. if no loss of power occured
 *
 */

#include "locks.h"
#include "rtc.h"
#include "io.h"
#include "debug.h"
#include "pit.h"
#include "kerrno.h"
#include "lib/time.h"

#define BCD_TO_BIN(x)   ((x & 0xf) + ((x >> 4) & 0xf)*10)

/*
 * A spinlock to protect the RTC index register
 */
static spinlock_t rtc_lock;

/*
 * Avoid concurrent calls of rtc_get_time
 */
static spinlock_t get_time_lock;

/*
 * This is set to 1 if initialization was successful
 */
static int rtc_ok = 0;

/*
 * Time at which initialization took place
 */
static int init_time = 0;

static char* __module = "RTC   ";

/*
 * Read a value from the RTC/CMOS
 * Parameters:
 * @index - number of register to read
 * Return value:
 * content of register
 */
u8 rtc_read_register(u8 index) {
    u8 nmi_disable;
    u8 value;
    /*
     * Turn off interrupts and get spinlock
     */
    u32 eflags;
    spinlock_get(&rtc_lock, &eflags);
    /*
     * Get old value of NMI disable bit
     */
    nmi_disable = inb(RTC_INDEX_REGISTER) & NMI_DISABLED;
    /*
     * Write index register
     */
    outb(index + NMI_DISABLED, RTC_INDEX_REGISTER);
    /*
     * and read value
     */
    value = inb(RTC_DATA_REGISTER);
    /*
     * write old value of NMI disabled bit back
     */
    outb(nmi_disable, RTC_INDEX_REGISTER);
    spinlock_release(&rtc_lock, &eflags);
    return value;
}

/*
 * Write a value to the RTC/CMOS
 * Parameter:
 * @index - number of register to write to
 * @value - value to be written
 */
void rtc_write_register(u8 index, u8 value) {
    u8 nmi_disable;
    /*
     * Turn off interrupts and get spinlock
     */
    u32 eflags;
    spinlock_get(&rtc_lock, &eflags);
    /*
     * Get old value of NMI disable bit
     */
    nmi_disable = inb(RTC_INDEX_REGISTER) & NMI_DISABLED;
    /*
     * Write index register
     */
    outb(index + NMI_DISABLED, RTC_INDEX_REGISTER);
    /*
     * and write value
     */
    outb(value, RTC_DATA_REGISTER);
    /*
     * write old value of NMI disabled bit back
     */
    outb(nmi_disable, RTC_INDEX_REGISTER);
    spinlock_release(&rtc_lock, &eflags);
}

/*
 * Get time from the RTC clock
 * Parameters:
 * @year - used to store year in BCD-format (last two digits only)
 * @day - used to store day in BCD-format
 * @month - used to store month in BCD-format
 * @hour - used to store hour in BCD-format (24h)
 * @min - used to store min in BCD format
 * @second - used to store second
 * Return value:
 * 0 if operation was successful
 * ENODEV if RTC has not been properly initialized
 */
static int rtc_get_time(int* year, u32* month, u32* day, u32* hour, u32* min, u32* second) {
    u8 rtc_busy;
    u32 eflags;
    if (0 == rtc_ok)
        return ENODEV;
    /*
     * The entire operation needs to be completed within a certain guaranteed time. Thus
     * we serialize do_time to make sure that no other threads jump in and we need to wait
     * on the spinlocks in rtc_read_register and rtc_write_register
     */
    spinlock_get(&get_time_lock, &eflags);
    /*
     * Make sure that time is stable - if yes, we have more than 200 us to complete
     * remaining read operations
     */
    rtc_busy = 1;
    while (rtc_busy) {
        rtc_busy = rtc_read_register(RTC_STS_REGISTER_A) & RTC_STS_A_UIP;
    }
    *second = rtc_read_register(RTC_INDEX_SECONDS);
    *hour = rtc_read_register(RTC_INDEX_HOURS);
    *day = rtc_read_register(RTC_INDEX_DAY);
    *month = rtc_read_register(RTC_INDEX_MONTH);
    *year = rtc_read_register(RTC_INDEX_YEAR);
    *min = rtc_read_register(RTC_INDEX_MINS);
    spinlock_release(&get_time_lock, &eflags);
    return 0;
}

/*
 * Get Unix time, i.e. number of seconds passed since 1.1.1970
 * Parameters:
 * @time - used to return time
 * Return value:
 * The current time if operation was successful or ((time_t)-1) if
 * an error occurred
 */
time_t rtc_do_time(time_t* time) {
    struct tm rtc_time;
    int year;
    u32 month;
    u32 day;
    u32 hours;
    u32 min;
    u32 seconds;
    int rc;
    time_t result;
    /*
     * First get time from RTC and convert the result (BCD)
     * to binary values
     */
    rc = rtc_get_time(&year, &month, &day, &hours, &min, &seconds);
    if (rc) {
        ERROR("RTC not initialized\n");
        return -1;
    }
    /*
     * Fill tm structure and convert BCD to binary
     */
    rtc_time.tm_hour = BCD_TO_BIN(hours);
    rtc_time.tm_isdst = 0;
    rtc_time.tm_mday = BCD_TO_BIN(day);
    rtc_time.tm_min = BCD_TO_BIN(min);
    rtc_time.tm_mon = BCD_TO_BIN(month) - 1;
    rtc_time.tm_year = BCD_TO_BIN(year)+100;
    rtc_time.tm_sec = BCD_TO_BIN(seconds);
    result = mktime(&rtc_time);
    /*
     * Some consistency checks
     */
    if (init_time) {
        if (result < init_time) {
            PANIC("Result of do_time (%d) is less than initialization time (%d)\n", result, init_time);
        }
        if (result > init_time + 60*60*24*365) {
            PANIC("Result of do_time (%d) is more than 1 year after initialization time (%d)\n", result, init_time);
        }
    }
    if (time)
        *time = result;
    return result;
}



/*
 * Initialize RTC driver
 */
void rtc_init() {
    spinlock_init(&rtc_lock);
    spinlock_init(&get_time_lock);
    int year;
    u32 day;
    u32 month;
    u32 min;
    u32 second;
    u32 hour;
    u8 status_reg_b = rtc_read_register(RTC_STS_REGISTER_B);
    int binary_mode = (status_reg_b & RTC_STS_B_BINARY) / RTC_STS_B_BINARY;
    int twentyfourhour_mode = (status_reg_b & RTC_STS_B_24H) / RTC_STS_B_24H;
    rtc_ok = 1;
    if ((0 == twentyfourhour_mode)) {
        ERROR("Could not properly initialize RTC as 24h mode is not enabled\n");
        rtc_ok = 0;
    }
    if (binary_mode) {
        ERROR("Could not properly initialize RTC as BCD mode is not enabled\n");
        rtc_ok = 0;
    }
    if (rtc_ok) {
        rtc_get_time(&year, &month, &day, &hour, &min, &second);
        init_time = rtc_do_time(0);
        MSG("Current time: %h:%h:%h %h.%h.%h\n", hour, min, second, day, month, year);
    }
}
