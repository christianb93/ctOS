/*
 * reboot.c
 *
 * This module contains low-level code to reboot the machine
 */

#include "reboot.h"
#include "io.h"
#include "rtc.h"
#include "keyboard.h"
#include "util.h"
#include "debug.h"

/*
 * Reset PC
 * We use two different methods to reset the PC
 * 1) first we try to pulse the CPU reset line connected
 * to the keyboard controller low. For this purpose, we wait
 * until bit 1 in the keyboard controller status register (IBF) is cleared,
 * indicating that all data sent from the CPU to the controller has been processed
 * We then sent the command 0xfe which will drive the reset line low. We repeat this
 * ten times if needed
 * 2) If that does not work, we use the reset control register located at port 0xcf9
 * on some PCs. When bit 2 of this register goes from 0 to 1, a reset is initiated
 * Before doing that, bit 1 can be set to request either a soft reset (bit set to 0)
 * or a hard reset (bit set to 1). To be on the safe side, we do a hard reset
 */
void reboot() {
    int i;
    int try;
    u8 temp;
    cli();
    /*
     * First write 0x0 to BIOS shutdown status in CMOS
     */
    rtc_write_register(RTC_SHUTDOWN_STS, 0x0);
    for (try = 0; try < 10; try++) {
        /* Wait for keyboard */
        for (i = 0; i < 10000; i++) {
            if ((inb(KEYBOARD_STATUS_PORT) & 0x02) == 0)
                break;
        }
        /* Pulse reset line */
        outb(0xfe, KEYBOARD_STATUS_PORT);
        for (i = 0; i < 10000; i++);
    }
    temp = inb(0xcf9);
    /* Set bit 2 to 0 and bit 1 to 1
     * to prepare hard reset
     */
    temp = (temp & ~0x4) | 0x2;
    outb(temp, 0xcf9);
    for (i = 0; i < 10000; i++);
    /* Now set bit 2 to 1 to do reset */
    temp = temp | 0x4;
    outb(temp, 0xcf9); /* Actually do the reset */
    for (i = 0; i < 10000; i++);
    PRINT("Reboot initiated, but machine still alive - please power down manually\n");
    halt();
}
