/*
 * test_keyboard.c
 */

#include "keyboard.h"
#include "kunit.h"
#include "tty.h"
#include "vga.h"
#include "locks.h"

int target_cpu = 1;
int target_vector = 0x40;

/*
 * Stub for debug main
 */
static int debug_main_called = 0;
void debug_main() {
    debug_main_called = 1;
}

int cpu_get_apic_id(int target_cpu) {
    return 0x2;
}

int apic_send_ipi(u8 lapic_id, u8 ipi, u8 vector, int deassert) {
    return 0;
}

static int do_putchar = 0;
void win_putchar(win_t* win, u8 c) {
    if (do_putchar)
        printf("%c", c);
}



u32 get_eflags() {
    return 0;
}

void irq_set_debug_flag() {

}

/*
 * Stub for inb
 */
static u8 scancode;
u8 inb(u16 port) {
    if (port==KEYBOARD_STATUS_PORT)
        return 1;
    if (port==KEYBOARD_DATA_PORT)
        return scancode;
    return 0;
}


int do_kill(pid_t pid, int sig_no) {
    return 0;
}

/*
 * Stub for tty_put
 */
static int tty_put_called = 0;
void tty_put (int channel, unsigned char* input, size_t nbytes) {
    tty_put_called = 1;
}

void spinlock_get(spinlock_t* lock, u32* eflags) {
}
void spinlock_release(spinlock_t* lock, u32* eflags) {
}
void spinlock_init(spinlock_t* lock) {
}

/*
 * Testcases 1:
 * Tested function: kbd_isr
 * Simulate keypress of F1 and verify that we enter the debugger
 */
int testcase1() {
    kbd_init();
    debug_main_called = 0;
    scancode = 0x3b;
    ASSERT(1 == kbd_isr(0));
    return 0;
}

/*
 * Testcase 2:
 * Tested function: kbd_isr
 * Testcase: simulate a single key event and make sure that tty_put is called
 */
int testcase2() {
    kbd_init();
    tty_put_called = 0;
    scancode = 30; // 'a'
    ASSERT(0==kbd_isr(0));
    ASSERT(1==tty_put_called);
    return 0;
}



int main() {
    INIT;
    RUN_CASE(1);
    RUN_CASE(2);
    END;
}

