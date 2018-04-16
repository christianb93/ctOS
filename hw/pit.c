/*
 * pit.c
 *
 * Functions to set up the programmable interrupt Timer 8254
 */

#include "lib/sys/types.h"
#include "io.h"
#include "pit.h"
#include "debug.h"
#include "locks.h"
#include "timer.h"

/*
 * Spinlock used to protect PIT
 */
static spinlock_t pit_lock;

/*
 * Initialization of programmable Timer 8254
 * The timer has three different counters. In mode 3,
 * each counter is decremented by one with each tick of the timer
 * i.e. each 838 ns. (with 1193180 Hz)
 * When 0 is reached, the IRQ fires and the counter
 * starts anew.
 * To program the PIT, we have to send three bytes:
 * - control byte to port 0x043
 * - LSB of initial counter value
 * - MSB of initial counter value
 * The bits of the control word:
 * - 0: use BCD (1) or binary (0) format
 * - 1-3: mode
 * - 4-5: how to transfer counter value, 11b means transfer LSB, then MSB
 * - 6-7: counter to use (0-2)
 * We will set the counter to 1193180 / HZ
 * Thus the IRQ will fire HZ times per second
 */
void pit_init() {
    u32 latch = PIT_TIMER_FREQ / HZ;
    outb(0x36, PIT_CMD_PORT); /* binary, mode 2, LSB/MSB, counter 0 */
    outb(latch & 0xff, PIT_DATA_PORT); /* LSB */
    outb(latch >> 8, PIT_DATA_PORT); /* MSB */
    spinlock_init(&pit_lock);
}

/*
 * This function waits in a loop until a given number of
 * PIT ticks (one each 838 ns) have passed
 * Parameters:
 * @ticks - ticks to wait
 */
void pit_short_delay(u16 ticks) {
    u16 initial_count;
    u16 current_count;
    u16 diff;
    u32 eflags;
    /*
     * Latch current value by writing 0x0 to latch register 0x43
     * and then read twice from data register. We need to protect this
     * by a lock!
     */
    spinlock_get(&pit_lock, &eflags);
    outb(0x0, PIT_CMD_PORT);
    initial_count = inb(PIT_DATA_PORT);
    initial_count += inb(PIT_DATA_PORT) << 8;
    spinlock_release(&pit_lock, &eflags);
    current_count = initial_count;
    /*
     * Now enter a loop and read again until
     * current_count and initial_count differ by
     * at least ticks
     */
    while (1) {
        spinlock_get(&pit_lock, &eflags);
        outb(0x0, PIT_CMD_PORT);
        current_count = inb(PIT_DATA_PORT);
        current_count += inb(PIT_DATA_PORT) << 8;
        spinlock_release(&pit_lock, &eflags);
        if (initial_count > current_count) {
            diff = initial_count - current_count;
        }
        else {
            diff = initial_count + (65535 - current_count);
        }
        if (diff > ticks)
            break;
    }
}

