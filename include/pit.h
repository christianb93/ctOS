/*
 * pit.h
 */

#ifndef _PIT_H_
#define _PIT_H_

#define PIT_TIMER_FREQ 1193180


#define PIT_CMD_PORT 0x43
#define PIT_DATA_PORT 0x40

void pit_init();
void pit_short_delay(u16);

#endif /* _PIT_H_ */
