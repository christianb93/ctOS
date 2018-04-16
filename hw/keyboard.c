/*
 * keyboard.c
 *
 * This module contains the low-level part of the keyboard TTY driver.
 */

#include "keyboard.h"
#include "io.h"
#include "debug.h"
#include "locks.h"
#include "tty.h"
#include "vga.h"
#include "lib/termios.h"
#include "lib/ctype.h"
#include "pm.h"
#include "lib/os/signals.h"
#include "lib/string.h"
#include "cpu.h"
#include "apic.h"
#include "util.h"

/*
 * Is idle wait possible?
 */
static int do_idle_wait = 0;


/*
 * Flag which saves whether shift key, control key or caps lock has been pressed
 */
static u32 shift_enabled = 0;
static u32 caps_lock = 0;
static int ctrl_enabled = 0;
static int right_ctrl_enabled = 0; // AltGr on a german keyboard

/*
 * Values returned by translate scancode for some special keys
 */
#define ARROW_UP    0x100
#define ARROW_DOWN  0x101
#define ARROW_LEFT  0x102
#define ARROW_RIGHT 0x103

/*
 * This is scancode set 1 for a german keyboard. Remember that even though most
 * keyboards operate with set 2 or three these days, the 8042 will translate these
 * sets to set 1, so this is what we see when we read from the keyboard controller
 */
static keyboard_map_entry_t kbd_map[] = {
        { 1, 27, 0, 0},
        { 2, '1', '!', 0},
        { 3, '2', '"', 0},
        { 4, '3', ' ', 0},
        { 5, '4', '$', 0},
        { 6, '5', '%', 0},
        { 7, '6', '&', 0},
        { 8, '7', '/', 0},
        { 9, '8', '(', 0},
        { 10, '9', ')', 0},
        { 11, '0', '=', 0},
        { 12, 0, '?', 0},
        { 14, 127, 127, 0 },
        { 16, 'q', 'Q', 0 },
        { 17, 'w', 'W', 0 },
        { 18, 'e' , 'E', 0},
        { 19, 'r', 'R', 0 },
        { 20, 't', 'T', 0 },
        { 21, 'z', 'Z', 26 },
        { 22, 'u', 'U', 21 },
        { 23, 'i', 'I', 0 },
        { 24, 'o', 'O', 0 },
        { 25, 'p', 'P', 0 },
        { 27, '+', '*', 0 },
        { 28, '\n', '\n', 0 },
        { 30, 'a', 'A', 0 },
        { 31, 's', 'S', 0 },
        { 32, 'd', 'D', 4 },
        { 33, 'f', 'F', 0 },
        { 34, 'g', 'G', 7 },
        { 35, 'h', 'H', 8 },
        { 36, 'j', 'J', 0 },
        { 37, 'k', 'K', 0 },
        { 38, 'l', 'L', 12 },
        { 44, 'y', 'Y', 0 },
        { 45, 'x', 'X', 0 },
        { 46, 'c', 'C', 3 },
        { 47, 'v', 'V', 0 },
        { 48, 'b', 'B', 0 },
        { 49, 'n', 'N', 0 },
        { 50, 'm', 'M', 0 },
        { 51, ',', ';', 0 },
        { 52, '.', ':', 0 },
        { 53, '-', '_', 0 },
        { 72, ARROW_UP, 0, 0},
        { 75, ARROW_LEFT, 0, 0},
        { 77, ARROW_RIGHT, 0, 0},
        { 80, ARROW_DOWN, 0, 0 },
        { 86, '<', '>', 0 },
        { 57, ' ', ' ', 0 },
        { 0, 0 } };

/*
 * Set idle wait flag. This is called by the timer module timer.c once
 * the PIT has been set up and we have a periodic interrupt source
 */
void keyboard_enable_idle_wait() {
    do_idle_wait = 1;
}

/*
 * Read a scancode directly from the keyboard
 * without waiting for an interrupt
 * Only use this function when you know
 * what you are doing!
 * Return value:
 * the next scancode from the keyboard
 */
static u32 read_scancode() {
    int ie = IRQ_ENABLED(get_eflags());
    u32 scancode = 0;
    while (!(inb(KEYBOARD_STATUS_PORT) & 1)) {
        asm("pause");
        /*
         * If interrupts are enabled and we have a periodic IRQ source
         * halt CPU
         */
        if ((ie) && (do_idle_wait)) {
            asm("hlt");
        }
    }
    scancode = inb(KEYBOARD_DATA_PORT);
    return scancode;
}

/*
 * Translate a given scancode into an
 * ASCII character
 * Parameters:
 * @scancode - the scancode to be translated
 * Return value:
 * 0 if the scancode does not describe an ASCII character
 * the ASCII character corresponding to the scancode if the key represents an ASCII character
 * a integer value with lowest byte zero if the character is a special key
 */
static unsigned int translate_scancode(u32 scancode) {
    u32 i = 0;
    unsigned int ascii;
    /*
     * Handle combinations with right ctrl key (AltGr on a german keyboard)
     */
    if (right_ctrl_enabled && (86==scancode)) {
        return '|';
    }
    while (kbd_map[i].scancode != 0) {
        if (kbd_map[i].scancode == scancode) {
            ascii = kbd_map[i].ascii;
            if (1 == shift_enabled)
                ascii = kbd_map[i].shift;
            else if (1 == ctrl_enabled)
                ascii = kbd_map[i].ctrl;
            if ((isalpha(ascii)) && caps_lock)
                ascii -=32;
            return ascii;
        }
        i++;
    }
    /*
     * Handle shift key. Left shift key is 0x36, right shift key is 0x2a
     * */
    if ((0x36 == scancode) || (0x2a==scancode)) {
        shift_enabled = 1;
        return 0;
    }
    else if ((0xb6 == scancode) || (0xaa==scancode)) {
        shift_enabled = 0;
        return 0;
    }
    /*
     * Handle caps lock
     */
    if (0x3a==scancode) {
        if (1==caps_lock)
            caps_lock = 0;
        else
            caps_lock=1;
        return 0;
    }
    /*
     * Handle control keys
     */
    if (0x1d==scancode) {
        ctrl_enabled = 1;
        return 0;
    }
    if ((0x1d+0x80)==scancode) {
        ctrl_enabled = 0;
        return 0;
    }
    if (0x38==scancode) {
        right_ctrl_enabled = 1;
        return 0;
    }
    if ((0x38+0x80)==scancode) {
        right_ctrl_enabled = 0;
        return 0;
    }
    /*
     * If we get to this point, we have not found the scancode - unless
     * of course its a break code, i.e. bit 0x80 is set
     */
    if (0==(scancode & 0x80))
        DEBUG("Unknown scancode %d (%x)\n", scancode, scancode);
    return 0;
}

/*
 * Read a character from keyboard
 * Note that this will only return
 * once a key is pressed, not when a key is released. So calling this
 * from the interrupt handler will actually freeze
 * This function should only be used in situations where direct keyboard
 * input is required and proper reading from /dev/tty is not yet available,
 * for instance at boot time
 * Return value:
 * a character from the keyboard
 */
u8 early_getchar() {
    u32 scancode;
    u8 read = -1;
    int done = 0;
    while (!done) {
        scancode = read_scancode();
        /* Handle shift key */
        if (0x36 == scancode) {
            shift_enabled = 1;
        }
        else if (0xb6 == scancode)
            shift_enabled = 0;
        /* Processing of regular scancodes */
        else {
            read = translate_scancode(scancode);
            if (read > 0)
                done = 1;
        }
    }
    return read;
}

/*
 * Initialize keyboard driver
 */
void kbd_init() {
    shift_enabled = 0;
    ctrl_enabled = 0;
}


/*
 * ISR for keyboard interrupts
 * Parameter:
 * @ir_context - the IR context
 */
int kbd_isr(ir_context_t* ir_context) {
    unsigned char buffer [16];
    u8 scancode = inb(KEYBOARD_DATA_PORT);
    unsigned int c = 0;
    unsigned char ascii;
    /* Did we hit F1? */
    if (scancode == 0x3b) {
        return 1;
    }
    c = translate_scancode(scancode);
    /*
     * Transfer character to TTY driver if it is an
     * ordinary ASCII character
     */
    if (0==(c & 0xff00)) {
        ascii = c & 0xff;
        if (ascii) {
            tty_put(TTY_CHANNEL_CONS, &ascii, 1);
        }
    }
    else {
        switch (c) {
            case ARROW_UP:
                strcpy((char*) buffer+1, "[A");
                buffer[0]=27;
                tty_put(TTY_CHANNEL_CONS, buffer,  3);
                break;
            case ARROW_LEFT:
                strcpy((char*) buffer+1, "[D");
                buffer[0]=27;
                tty_put(TTY_CHANNEL_CONS, buffer,  3);
                break;
            case ARROW_RIGHT:
                strcpy((char*) buffer+1, "[C");
                buffer[0]=27;
                tty_put(TTY_CHANNEL_CONS, buffer,  3);
                break;
            case ARROW_DOWN:
                strcpy((char*) buffer+1, "[B");
                buffer[0]=27;
                tty_put(TTY_CHANNEL_CONS, buffer,  3);
                break;
            default:
                break;
        }
    }
    return 0;
}

