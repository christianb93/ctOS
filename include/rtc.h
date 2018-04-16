/*
 * rtc.h
 *
 */

#ifndef _RTC_H_
#define _RTC_H_

#define RTC_INDEX_REGISTER 0x70
#define RTC_DATA_REGISTER 0x71

#define RTC_STS_REGISTER_A 0xa
#define RTC_STS_REGISTER_B 0xb
#define RTC_STS_REGISTER_D 0xd

#define RTC_SHUTDOWN_STS 0xf

#define RTC_INDEX_SECONDS 0
#define RTC_INDEX_MINS 2
#define RTC_INDEX_HOURS 4
#define RTC_INDEX_DAY 7
#define RTC_INDEX_MONTH 8
#define RTC_INDEX_YEAR 9

#define NMI_DISABLED (1 << 7)

#define RTC_STS_B_BINARY (1 << 2)
#define RTC_STS_B_24H (1 << 1)

#define RTC_STS_A_UIP (1 << 7)

void rtc_init();
void rtc_write_register(u8 index, u8 value);
u8 rtc_read_register(u8 index);
time_t rtc_do_time(time_t* time);

#endif /* _RTC_H_ */
