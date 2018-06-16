/*
 * test_tty.c
 */

#include "tty.h"
#include "tty_ld.h"
#include "kunit.h"
#include "drivers.h"
#include "irq.h"
#include "pm.h"
#include "vga.h"
#include "lib/termios.h"
#include <stdio.h>
#include "lib/os/signals.h"
/*
 * Dummys
 */
void dm_register_char_dev(char_dev_ops_t* ops) {
}

int irq_add_handler(int vector, isr_t isr) {
    return 0;
}

int kbd_isr(ir_context_t* irc) {
    return 0;
}

int pm_get_task_id() {
    return 0;
}

void pm_validate() {

}



int irq_add_handler_isa(isr_t new_isr, int priority, int _irq, int lock) {
    return 0x21;
}

static int do_print = 0;
static int kputchar_called = 0;
static int last_char = 0;
void win_putchar(win_t* win, u8 c) {
    if (do_print)
        printf("%c", c);
    last_char = c;
}

void kputchar(u8 c) {
    if (do_print)
        printf("%c", c);
    last_char = c;
    kputchar_called = 1;
}


void spinlock_get(spinlock_t* lock, u32* eflags) {
}
void spinlock_release(spinlock_t* lock, u32* eflags) {
}
void spinlock_init(spinlock_t* lock) {
}
/*
 * Stubs for mutex and semaphore
 */
void mutex_up(semaphore_t* sem) {
    sem->value=1;
}

void __sem_down(semaphore_t* sem, char* file, int line) {
    sem->value--;
}


int __sem_down_intr(semaphore_t* sem, char* file, int line) {
    sem->value--;
    return 0;
}

static int do_timeout = 0;
int __sem_down_timed(semaphore_t* sem, char* file, int line, u32 timeout) {
    if (do_timeout)
        return -2;
    sem->value--;
    return 0;
}


int sem_down_nowait(semaphore_t* sem) {
    sem->value--;
    return 0;
}

void sem_up(semaphore_t* sem) {
    sem->value++;
}

void sem_init(semaphore_t* sem, u32 value) {
    sem->value = value;
}

void kbd_setattr(struct termios* settings) {
}

void kbd_getattr(struct termios* settings) {
}

int irq_get_vector(int vector) {
    return 0x21;
}


void trap() {

}

static int last_pid = 0;
static int last_sig_no = 0;
int do_kill(pid_t pid, int sig_no) {
    last_pid = pid;
    last_sig_no = sig_no;
    return 0;
}

/*
 * Stubs for process group and signal handling
 */
static pid_t pgrp = 1;
pid_t do_getpgrp() {
    return pgrp;
}

static void (*sa_handler)(int) = __KSIG_DFL;
int do_sigaction(int sig_no, __ksigaction_t* sa_new, __ksigaction_t* sa_old) {
    if (sa_old)
        sa_old->sa_handler = sa_handler;
    return 0;
}

static unsigned int procmask = 0;
int do_sigprocmask(int what, u32* set, u32* oset) {
    if (oset)
        *oset = procmask;
    return 0;
}

static dev_t cterm = DEVICE(MAJOR_TTY, 0);
dev_t pm_get_cterm(pid_t pid) {
    return cterm;
}

static int pid = 1;
pid_t pm_get_pid() {
    return 1;
}



/*
 * Testcase 1
 * Tested function: tty_ld_put
 * Testcase: call tty_ld_put to add one character to the empty line
 * Expected result: character is added, echoing is done
 */
int testcase1() {
    tty_t my_tty;
    tty_ld_init(&my_tty);
    last_char = 0;
    kputchar_called = 0;
    ASSERT(0==tty_ld_put(&my_tty, "a", 1));
    ASSERT(-1==my_tty.read_buffer_end);
    ASSERT('a'==my_tty.line_buffer[0]);
    ASSERT(0==my_tty.line_buffer_end);
    ASSERT(1==kputchar_called);
    ASSERT('a'==last_char);
    return 0;
}

/*
 * Testcase 2
 * Tested function: tty_ld_put
 * Testcase: call tty_ld_put twice and check that second character is correctly added
 */
int testcase2() {
    tty_t my_tty;
    tty_ld_init(&my_tty);
    ASSERT(0==tty_ld_put(&my_tty, "a", 1));
    ASSERT(0==tty_ld_put(&my_tty, "b", 1));
    ASSERT(-1==my_tty.read_buffer_end);
    ASSERT('a'==my_tty.line_buffer[0]);
    ASSERT('b'==my_tty.line_buffer[1]);
    ASSERT(1==my_tty.line_buffer_end);
    return 0;
}

/*
 * Testcase 3
 * Tested function: tty_ld_put
 * Testcase: call tty_ld_put MAX_INPUT+1 times
 * Expected result: last call does not change buffer any more
 */
int testcase3() {
    int i;
    tty_t my_tty;
    tty_ld_init(&my_tty);
    for (i=0;i<MAX_INPUT; i++) {
        ASSERT(0==tty_ld_put(&my_tty, "a", 1));
        ASSERT(i==my_tty.line_buffer_end);
    }
    ASSERT(0==tty_ld_put(&my_tty, "b", 1));
    ASSERT(-1==my_tty.read_buffer_end);
    ASSERT('a'==my_tty.line_buffer[MAX_INPUT-1]);
    ASSERT(MAX_INPUT-1==my_tty.line_buffer_end);
    return 0;
}

/*
 * Testcase 4
 * Tested function: tty_ld_put
 * Testcase: call tty_ld_put twice with regular characters in canonical mode and
 * then once with NL.
 * Expected result: line buffer is empty, read buffer is filled
 */
int testcase4() {
    int i;
    tty_t my_tty;
    tty_ld_init(&my_tty);
    tty_ld_put(&my_tty, "a", 1);
    tty_ld_put(&my_tty, "b", 1);
    ASSERT(1==tty_ld_put(&my_tty, "\n", 1));
    ASSERT(-1==my_tty.line_buffer_end);
    ASSERT(2==my_tty.read_buffer_end);
    ASSERT('a'==my_tty.read_buffer[0]);
    ASSERT('b'==my_tty.read_buffer[1]);
    ASSERT('\n'==my_tty.read_buffer[2]);
    return 0;
}

/*
 * Testcase 5
 * Tested function: tty_ld_put
 * Testcase: call tty_ld_put twice with regular characters in canonical mode and
 * then once with EOL
 * Expected result: line buffer is empty, read buffer is filled
 */
int testcase5() {
    int i;
    unsigned char c;
    tty_t my_tty;
    tty_ld_init(&my_tty);
    tty_ld_put(&my_tty, "a", 1);
    tty_ld_put(&my_tty, "b", 1);
    c = 255;
    ASSERT(1==tty_ld_put(&my_tty, &c, 1));
    ASSERT(-1==my_tty.line_buffer_end);
    ASSERT(2==my_tty.read_buffer_end);
    ASSERT('a'==my_tty.read_buffer[0]);
    ASSERT('b'==my_tty.read_buffer[1]);
    ASSERT(c==my_tty.read_buffer[2]);
    return 0;
}

/*
 * Testcase 6
 * Tested function: tty_ld_put
 * Testcase: call tty_ld_put twice with regular characters in canonical mode and
 * then once with EOD
 * Expected result: line buffer is empty, read buffer is filled
 */
int testcase6() {
    int i;
    unsigned char c;
    tty_t my_tty;
    tty_ld_init(&my_tty);
    tty_ld_put(&my_tty, "a", 1);
    tty_ld_put(&my_tty, "b", 1);
    c = 4;
    ASSERT(1==tty_ld_put(&my_tty, &c, 1));
    ASSERT(-1==my_tty.line_buffer_end);
    ASSERT(2==my_tty.read_buffer_end);
    ASSERT('a'==my_tty.read_buffer[0]);
    ASSERT('b'==my_tty.read_buffer[1]);
    ASSERT(4==my_tty.read_buffer[2]);
    return 0;
}

/*
 * Testcase 7
 * Tested function: tty_ld_put
 * Testcase: call tty_ld_put MAX_INPUT times to build up a line with MAX_INPUT-1 characters and one NL.
 * This should fill up the read buffer entirely. Then call tty_ld_put once more. Verify that both buffers
 * remain unchanged
 */
int testcase7() {
    int i;
    tty_t my_tty;
    tty_ld_init(&my_tty);
    for (i=0;i<MAX_INPUT-1;i++)
        tty_ld_put(&my_tty, "a", 1);
    tty_ld_put(&my_tty, "\n", 1);
    ASSERT(-1==my_tty.line_buffer_end);
    ASSERT(MAX_INPUT-1==my_tty.read_buffer_end);
    ASSERT('\n'==my_tty.read_buffer[MAX_INPUT-1]);
    /*
     * Do one more call - should not change anything
     */
    ASSERT(0==tty_ld_put(&my_tty, "\n", 1));
    ASSERT(-1==my_tty.line_buffer_end);
    ASSERT(MAX_INPUT-1==my_tty.read_buffer_end);
    ASSERT('\n'==my_tty.read_buffer[MAX_INPUT-1]);
    return 0;
}


/*
 * Testcase 8
 * Tested function: tty_ld_put
 * Testcase: call tty_ld_put to add one character to the empty line when the ECHO flag is not set
 * Expected result: character is added, echoing is not done
 */
int testcase8() {
    tty_t my_tty;
    tty_ld_init(&my_tty);
    my_tty.settings.c_lflag = ICANON;
    last_char = 0;
    kputchar_called = 0;
    tty_ld_put(&my_tty, "a", 1);
    ASSERT(-1==my_tty.read_buffer_end);
    ASSERT('a'==my_tty.line_buffer[0]);
    ASSERT(0==my_tty.line_buffer_end);
    ASSERT(0==kputchar_called);
    return 0;
}


/*
 * Testcase 9: read from a controlling terminal when we are in a background process group and SIGTTIN is not ignored or
 * blocked
 * tty_read should return -EPAUSE in this case
 */
int testcase9() {
    char buffer[16];
    tty_init();
    tty_setpgrp(0, 1);
    /*
     * Simulate the case that we are process 2 and in process group 2
     */
    pgrp = 2;
    pid = 2;
    ASSERT(tty_read(0, 16, buffer, 0) == -122);
    /*
     * Restore old values of pid and pgrp
     */
    pid = 1;
    pgrp = 1;
    return 0;
}

/*
 * Testcase 10: read from a controlling terminal when we are in a background process group and SIGTTIN is ignored
 * tty_read should return -EIO in this case
 */
int testcase10() {
    char buffer[16];
    tty_init();
    tty_setpgrp(0, 1);
    /*
     * Simulate the case that we are process 2 and in process group 2,
     * but SIGTTIN is ignored
     */
    pgrp = 2;
    pid = 2;
    sa_handler = __KSIG_IGN;
    ASSERT(tty_read(0, 16, buffer, 0)==-111);
    /*
     * Restore old values of pid and pgrp and signal handler
     */
    pid = 1;
    pgrp = 1;
    sa_handler = __KSIG_DFL;
    return 0;
}

/*
 * Testcase 11: read from a controlling terminal when we are in a background process group and SIGTTIN is blocked
 * tty_read should return -EIO in this case
 */
int testcase11() {
    char buffer[16];
    tty_init();
    tty_setpgrp(0, 1);
    /*
     * Simulate the case that we are process 2 and in process group 2,
     * but SIGTTIN is blocked
     */
    pgrp = 2;
    pid = 2;
    procmask = (1 << __KSIGTTIN);
    ASSERT(tty_read(0, 16, buffer, 0)==-111);
    /*
     * Restore old values of pid and pgrp as well as procmask
     */
    pid = 1;
    pgrp = 1;
    procmask = 0;
    return 0;
}

/*
 * Testcase 12
 * Tested function: tty_ld_read
 * Testcase: call tty_ld_put to put 3 characters into buffer, then call
 * tty_ld_read requesting two characters
 * Expected result: two characters are returned
 */
int testcase12() {
    unsigned char buffer[16];
    tty_t my_tty;
    tty_ld_init(&my_tty);
    tty_ld_put(&my_tty, "a", 1);
    tty_ld_put(&my_tty, "b", 1);
    tty_ld_put(&my_tty, "\n", 1);
    ASSERT(2==tty_ld_read(&my_tty, buffer, 2));
    ASSERT('a'==buffer[0]);
    ASSERT('b'==buffer[1]);
    return 0;
}

/*
 * Testcase 13
 * Tested function: tty_ld_read
 * Testcase: call tty_ld_put to put 3 characters into buffer, then call
 * tty_read requesting three characters
 * Expected result: three characters are returned
 */
int testcase13() {
    unsigned char buffer[16];
    tty_t my_tty;
    tty_ld_init(&my_tty);
    tty_ld_put(&my_tty, "a", 1);
    tty_ld_put(&my_tty, "b", 1);
    tty_ld_put(&my_tty, "\n", 1);
    ASSERT(3==tty_ld_read(&my_tty, buffer, 3));
    ASSERT('a'==buffer[0]);
    ASSERT('b'==buffer[1]);
    ASSERT('\n'==buffer[2]);
    return 0;
}

/*
 * Testcase 14
 * Tested function: tty_ld_read
 * Testcase: call tty_ld_put to put 3 characters into buffer, then call
 * tty_read requesting four characters
 * Expected result: only three characters are returned
 */
int testcase14() {
    unsigned char buffer[16];
    tty_t my_tty;
    tty_ld_init(&my_tty);
    tty_ld_put(&my_tty, "a", 1);
    tty_ld_put(&my_tty, "b", 1);
    tty_ld_put(&my_tty, "\n", 1);
    ASSERT(3==tty_ld_read(&my_tty, buffer, 4));
    ASSERT('a'==buffer[0]);
    ASSERT('b'==buffer[1]);
    ASSERT('\n'==buffer[2]);
    return 0;
}

/*
 * Testcase 15
 * Tested function: tty_ld_read
 * Testcase: call tty_ld_put to put 3 characters into buffer, then call
 * tty_ld_read requesting two characters and perform a subsequent read to
 * get one additional characters
 * Expected result: two characters are returned upon first read, one
 * upon second read
 */
int testcase15() {
    unsigned char buffer[16];
    tty_t my_tty;
    tty_ld_init(&my_tty);
    tty_ld_put(&my_tty, "a", 1);
    tty_ld_put(&my_tty, "b", 1);
    tty_ld_put(&my_tty, "\n", 1);
    ASSERT(2==tty_ld_read(&my_tty, buffer, 2));
    ASSERT('a'==buffer[0]);
    ASSERT('b'==buffer[1]);
    ASSERT(1==tty_ld_read(&my_tty, buffer, 1));
    ASSERT('\n'==buffer[0]);
    return 0;
}


/*
 * Testcase 16
 * Tested function: tty_ld_read
 * Testcase: call tty_ld_put to put 3 characters into buffer, then call
 * tty_read requesting two characters and perform a subsequent read to
 * get two additional characters
 * Expected result: two characters are returned upon first read, one
 * upon second read
 */
int testcase16() {
    unsigned char buffer[16];
    tty_t my_tty;
    tty_ld_init(&my_tty);
    tty_ld_put(&my_tty, "a", 1);
    tty_ld_put(&my_tty, "b", 1);
    tty_ld_put(&my_tty, "\n", 1);
    ASSERT(2==tty_ld_read(&my_tty, buffer, 2));
    ASSERT('a'==buffer[0]);
    ASSERT('b'==buffer[1]);
    ASSERT(1==tty_ld_read(&my_tty, buffer, 2));
    ASSERT('\n'==buffer[0]);
    return 0;
}

/*
 * Testcase 17
 * Tested function: tty_ld_read
 * Testcase: in canonical mode, call tty_ld_put to store  'a' and NL,
 * followed by bcd and <NL>
 * Then we read three characters.
 * Expected result: we get one a and NL back
 */
int testcase17() {
    unsigned char buffer[16];
    tty_t my_tty;
    tty_ld_init(&my_tty);
    tty_ld_put(&my_tty, "a", 1);
    tty_ld_put(&my_tty, "\n", 1);
    tty_ld_put(&my_tty, "bcd", 3);
    tty_ld_put(&my_tty, "\n", 1);
    ASSERT(2==tty_ld_read(&my_tty, buffer, 3));
    ASSERT('a'==buffer[0]);
    ASSERT('\n'==buffer[1]);
    return 0;
}

/*
 * Testcase 18
 * Tested function: tty_ld_read
 * Testcase: in canonical mode, call tty_ld_put to store  EOF
 * Then we read three characters.
 * Expected result: 0 is returned
 */
int testcase18() {
    unsigned char buffer[16];
    unsigned char c = 4;
    tty_t my_tty;
    tty_ld_init(&my_tty);
    tty_ld_put(&my_tty, &c, 1);
    ASSERT(0==tty_ld_read(&my_tty, buffer, 3));
    return 0;
}

/*
 * Testcase 19
 * Tested function: tty_ld_read
 * Testcase: in canonical mode, call tty_ld_put to store  a and EOF
 * Then we read three characters.
 * Expected result: 1 is returned, a is read
 */
int testcase19() {
    unsigned char buffer[16];
    unsigned char c = 4;
    tty_t my_tty;
    tty_ld_init(&my_tty);
    tty_ld_put(&my_tty, "a", 1);
    tty_ld_put(&my_tty, &c, 1);
    ASSERT(1==tty_ld_read(&my_tty, buffer, 3));
    ASSERT('a'==buffer[0]);
    return 0;
}

/*
 * Testcase 20
 * Tested function: tty_ld_put / ERASE
 * Testcase: call tty_ld_put to add two characters to the input line, then add the DEL character and verify that the last
 * character is removed again and that DEL is not echoed
 */
int testcase20() {
    tty_t my_tty;
    unsigned char c;
    tty_ld_init(&my_tty);
    my_tty.settings.c_lflag = ICANON + ECHO;
    ASSERT(0==tty_ld_put(&my_tty, "a", 1));
    ASSERT(-1==my_tty.read_buffer_end);
    ASSERT('a'==my_tty.line_buffer[0]);
    ASSERT(0==my_tty.line_buffer_end);
    ASSERT(0==tty_ld_put(&my_tty, "b", 1));
    ASSERT(-1==my_tty.read_buffer_end);
    ASSERT('b'==my_tty.line_buffer[1]);
    ASSERT(1==my_tty.line_buffer_end);
    c = 127;
    kputchar_called = 0;
    ASSERT(0==tty_ld_put(&my_tty, &c, 1));
    ASSERT(-1==my_tty.read_buffer_end);
    ASSERT('a'==my_tty.line_buffer[0]);
    ASSERT(0==my_tty.line_buffer_end);
    ASSERT(0==kputchar_called);
    return 0;
}

/*
 * Testcase 21
 * Tested function: tty_ld_put
 * Testcase: apply ERASE to an empty line
 */
int testcase21() {
    tty_t my_tty;
    unsigned char c;
    tty_ld_init(&my_tty);
    my_tty.settings.c_lflag = ICANON + ECHO;
    c = 127;
    ASSERT(0==tty_ld_put(&my_tty, &c, 1));
    ASSERT(-1==my_tty.read_buffer_end);
    ASSERT(-1==my_tty.line_buffer_end);
    return 0;
}

/*
 * Testcase 22
 * Tested function: tty_ld_put / INTR
 * Testcase: verify that Ctrl-C creates SIGINT
 */
int testcase22() {
    tty_t my_tty;
    unsigned char c;
    tty_ld_init(&my_tty);
    my_tty.settings.c_lflag = ICANON + ECHO + ISIG;
    c = 3;
    last_sig_no = 0;
    ASSERT(0==tty_ld_put(&my_tty, &c, 1));
    ASSERT(-1==my_tty.read_buffer_end);
    ASSERT(-1==my_tty.line_buffer_end);
    ASSERT(__KSIGINT==last_sig_no);
    ASSERT(last_pid==-1);
    return 0;
}

/*
 * Testcase 23
 * Tested function: tty_ld_put / SUSP
 * Testcase: verify that Ctrl-Z creates SIGTSTP
 */
int testcase23() {
    tty_t my_tty;
    unsigned char c;
    tty_ld_init(&my_tty);
    my_tty.settings.c_lflag = ICANON + ECHO + ISIG;
    c = 26;
    last_sig_no = 0;
    ASSERT(0==tty_ld_put(&my_tty, &c, 1));
    ASSERT(-1==my_tty.read_buffer_end);
    ASSERT(-1==my_tty.line_buffer_end);
    ASSERT(__KSIGTSTP==last_sig_no);
    ASSERT(last_pid==-1);
    return 0;
}

/*
 * Testcase 24
 * Tested function: tty_ld_put
 * Testcase: verify that in non-canonical mode, tty_ld_put copies a character
 * to the read buffer right away
 */
int testcase24() {
    tty_t my_tty;
    unsigned char c;
    tty_ld_init(&my_tty);
    my_tty.settings.c_lflag = 0;
    ASSERT(1==tty_ld_put(&my_tty, "x", 1));
    ASSERT(0==my_tty.read_buffer_end);
    ASSERT('x'==my_tty.read_buffer[0]);
    ASSERT(-1==my_tty.line_buffer_end);
    return 0;
}

/*
 * Testcase 25
 * Tested function: tty_ld_put
 * Testcase: test non-canonical mode with VMIN=2
 */
int testcase25() {
    tty_t my_tty;
    unsigned char c;
    tty_ld_init(&my_tty);
    my_tty.settings.c_lflag = 0;
    my_tty.settings.c_cc[VMIN]=2;
    ASSERT(0==tty_ld_put(&my_tty, "x", 1));
    ASSERT(-1==my_tty.read_buffer_end);
    ASSERT(0==my_tty.line_buffer_end);
    ASSERT(1==tty_ld_put(&my_tty, "y", 1));
    ASSERT(1==my_tty.read_buffer_end);
    ASSERT(-1==my_tty.line_buffer_end);
    ASSERT('x'==my_tty.read_buffer[0]);
    ASSERT('y'==my_tty.read_buffer[1]);
    return 0;
}

/*
 * Testcase 26
 * Tested function: tty_ld_put / ECHO
 * Testcase: call tty_ld_put to add one character to the empty line, but disable ECHO
 * Expected result: character is added, echoing is done
 */
int testcase26() {
    tty_t my_tty;
    tty_ld_init(&my_tty);
    last_char = 0;
    kputchar_called = 0;
    my_tty.settings.c_lflag &= ~ECHO;
    ASSERT(0==tty_ld_put(&my_tty, "a", 1));
    ASSERT(-1==my_tty.read_buffer_end);
    ASSERT('a'==my_tty.line_buffer[0]);
    ASSERT(0==my_tty.line_buffer_end);
    ASSERT(0==kputchar_called);
    return 0;
}

/*
 * Testcase 27
 * Tested function: tty_ld_put / KILL
 * Testcase: call tty_ld_put to add two characters to the empty line, then simulate the KILL character
 * Expected result: line is empty, KILL has been echoed
 */
int testcase27() {
    tty_t my_tty;
    unsigned char input;
    tty_ld_init(&my_tty);
    my_tty.settings.c_lflag &= ~ECHOCTL;
    ASSERT(my_tty.settings.c_lflag & ECHOK);
    last_char = 0;
    kputchar_called = 0;
    ASSERT(0==tty_ld_put(&my_tty, "abc", 3));
    ASSERT(-1==my_tty.read_buffer_end);
    ASSERT('c'==my_tty.line_buffer[2]);
    input=my_tty.settings.c_cc[VKILL];
    ASSERT(21==input);
    kputchar_called = 0;
    ASSERT(0==tty_ld_put(&my_tty, &input, 1));
    ASSERT(-1==my_tty.line_buffer_end);
    ASSERT(1==kputchar_called);
    ASSERT(127==last_char);
    return 0;
}

/*
 * Testcase 28
 * Tested function: tty_ld_put / ICRNL
 * Testcase: set ICRNL. Call tty_ld_put to add two characters to the empty line, then simulate CR
 * Expected result: CR is treated as NL, i.e. the line buffer is transferred to the read buffer
 */
int testcase28() {
    tty_t my_tty;
    unsigned char input;
    tty_ld_init(&my_tty);
    my_tty.settings.c_iflag |= ICRNL;
    last_char = 0;
    kputchar_called = 0;
    ASSERT(0==tty_ld_put(&my_tty, "ab", 2));
    ASSERT(-1==my_tty.read_buffer_end);
    ASSERT('b'==my_tty.line_buffer[1]);
    input = '\r';
    ASSERT(1==tty_ld_put(&my_tty, &input, 1));
    ASSERT(-1==my_tty.line_buffer_end);
    return 0;
}

/*
 * Testcase 29
 * Tested function: tty_ld_put / INLCR
 * Testcase: set INLCR. Call tty_ld_put to add two characters to the empty line, then simulate NL
 * Expected result: NL is treated as CR, i.e. the line buffer is not transferred to the read buffer
 * and CR is appended
 */
int testcase29() {
    tty_t my_tty;
    unsigned char input;
    tty_ld_init(&my_tty);
    my_tty.settings.c_iflag |= INLCR;
    last_char = 0;
    kputchar_called = 0;
    ASSERT(0==tty_ld_put(&my_tty, "ab", 2));
    ASSERT(-1==my_tty.read_buffer_end);
    ASSERT('b'==my_tty.line_buffer[1]);
    input = '\n';
    ASSERT(0==tty_ld_put(&my_tty, &input, 1));
    ASSERT('\r'==my_tty.line_buffer[2]);
    return 0;
}

/*
 * Testcase 30
 * Tested function: tty_ld_put / IGNCR
 * Testcase: set IGNCR. Call tty_ld_put to add two characters to the empty line, then simulate CR
 * Expected result: CR is ignored, i.e. the read buffer end is not changed
 */
int testcase30() {
    tty_t my_tty;
    unsigned char input;
    tty_ld_init(&my_tty);
    my_tty.settings.c_iflag |= IGNCR;
    last_char = 0;
    kputchar_called = 0;
    ASSERT(0==tty_ld_put(&my_tty, "ab", 2));
    ASSERT(-1==my_tty.read_buffer_end);
    ASSERT(1==my_tty.line_buffer_end);
    ASSERT('b'==my_tty.line_buffer[1]);
    input = '\r';
    ASSERT(0==tty_ld_put(&my_tty, &input, 1));
    ASSERT(1==my_tty.line_buffer_end);;
    return 0;
}

/*
 * Testcase 31
 * Tested function: tty_ld_put / ERASE
 * Testcase: Set ECHOE. Call tty_ld_put to add two characters to the input line, then add the DEL character and verify that the last
 * character is removed again and that DEL is not echoed
 */
int testcase31() {
    tty_t my_tty;
    unsigned char c;
    tty_ld_init(&my_tty);
    my_tty.settings.c_lflag = ICANON + ECHO + ECHOE;
    ASSERT(0==tty_ld_put(&my_tty, "a", 1));
    ASSERT(-1==my_tty.read_buffer_end);
    ASSERT('a'==my_tty.line_buffer[0]);
    ASSERT(0==my_tty.line_buffer_end);
    ASSERT(0==tty_ld_put(&my_tty, "b", 1));
    ASSERT(-1==my_tty.read_buffer_end);
    ASSERT('b'==my_tty.line_buffer[1]);
    ASSERT(1==my_tty.line_buffer_end);
    c = 127;
    kputchar_called = 0;
    ASSERT(0==tty_ld_put(&my_tty, &c, 1));
    ASSERT(-1==my_tty.read_buffer_end);
    ASSERT('a'==my_tty.line_buffer[0]);
    ASSERT(0==my_tty.line_buffer_end);
    ASSERT(1==kputchar_called);
    return 0;
}

/*
 * Testcase 32
 * Tested function: tty_ld_put / KILL
 * Testcase: clear ECHOK. Call tty_ld_put to add two characters to the empty line, then simulate the KILL character
 * Expected result: line is empty, KILL has not been echoed
 */
int testcase32() {
    tty_t my_tty;
    unsigned char input;
    tty_ld_init(&my_tty);
    my_tty.settings.c_lflag &= ~ECHOK;
    last_char = 0;
    kputchar_called = 0;
    ASSERT(0==tty_ld_put(&my_tty, "abc", 3));
    ASSERT(-1==my_tty.read_buffer_end);
    ASSERT('c'==my_tty.line_buffer[2]);
    input=my_tty.settings.c_cc[VKILL];
    ASSERT(21==input);
    kputchar_called = 0;
    ASSERT(0==tty_ld_put(&my_tty, &input, 1));
    ASSERT(-1==my_tty.line_buffer_end);
    ASSERT(0==kputchar_called);
    return 0;
}

/*
 * Testcase 33
 * Tested function: tty_ld_put / ECHONL
 * Testcase: clear ECHO but set ECHONL. Call tty_ld_put to add two characters to the empty line, then simulate a NL
 * Expected result: NL is echoed
 */
int testcase33() {
    tty_t my_tty;
    unsigned char input;
    tty_ld_init(&my_tty);
    my_tty.settings.c_lflag &= ~ECHO;
    my_tty.settings.c_lflag |= ECHONL;
    last_char = 0;
    kputchar_called = 0;
    ASSERT(0==tty_ld_put(&my_tty, "abc", 3));
    ASSERT(-1==my_tty.read_buffer_end);
    ASSERT('c'==my_tty.line_buffer[2]);
    input='\n';
    kputchar_called = 0;
    ASSERT(1==tty_ld_put(&my_tty, &input, 1));
    ASSERT(-1==my_tty.line_buffer_end);
    ASSERT(1==kputchar_called);
    ASSERT('\n'==last_char);
    return 0;
}


/*
 * Testcase 34
 * Tested function: tty_ld_put / ECHONL
 * Testcase: clear ECHO, do not set ECHONL. Call tty_ld_put to add two characters to the empty line, then simulate a NL
 * Expected result: NL is not echoed
 */
int testcase34() {
    tty_t my_tty;
    unsigned char input;
    tty_ld_init(&my_tty);
    my_tty.settings.c_lflag &= ~(ECHO+ECHONL);
    last_char = 0;
    kputchar_called = 0;
    ASSERT(0==tty_ld_put(&my_tty, "abc", 3));
    ASSERT(-1==my_tty.read_buffer_end);
    ASSERT('c'==my_tty.line_buffer[2]);
    input='\n';
    kputchar_called = 0;
    ASSERT(1==tty_ld_put(&my_tty, &input, 1));
    ASSERT(-1==my_tty.line_buffer_end);
    ASSERT(0==kputchar_called);
    return 0;
}

/*
 * Testcase 35
 * Tested function: tty_ld_put / INTR
 * Testcase: verify that Ctrl-C does not create SIGINT if ISIG is cleared
 */
int testcase35() {
    tty_t my_tty;
    unsigned char c;
    tty_ld_init(&my_tty);
    my_tty.settings.c_lflag = ICANON + ECHO;
    c = 3;
    last_sig_no = 0;
    ASSERT(0==tty_ld_put(&my_tty, &c, 1));
    ASSERT(-1==my_tty.read_buffer_end);
    ASSERT(0==my_tty.line_buffer_end);
    ASSERT(0==last_sig_no);
    return 0;
}

/*
 * Testcase 36
 * Tested function: tty_ld_put / SUSP
 * Testcase: verify that Ctrl-Z does not create SIGSUSP if ISIG is cleared
 */
int testcase36() {
    tty_t my_tty;
    unsigned char c;
    tty_ld_init(&my_tty);
    my_tty.settings.c_lflag = ICANON + ECHO;
    c = my_tty.settings.c_cc[VSUSP];
    last_sig_no = 0;
    ASSERT(0==tty_ld_put(&my_tty, &c, 1));
    ASSERT(-1==my_tty.read_buffer_end);
    ASSERT(0==my_tty.line_buffer_end);
    ASSERT(0==last_sig_no);
    return 0;
}

/*
 * Testcase 37
 * Tested function: tty_ld_put / INTR
 * Testcase: verify that Ctrl-C flushes the buffer if ISIG is set and NOFLSH is clear
 */
int testcase37() {
    tty_t my_tty;
    unsigned char c;
    tty_ld_init(&my_tty);
    my_tty.settings.c_lflag = ICANON + ECHO + ISIG;
    c = 3;
    last_sig_no = 0;
    ASSERT(0==tty_ld_put(&my_tty, "ab", 2));
    ASSERT(0==tty_ld_put(&my_tty, &c, 1));
    ASSERT(-1==my_tty.read_buffer_end);
    ASSERT(-1==my_tty.line_buffer_end);
    ASSERT(__KSIGINT==last_sig_no);
    return 0;
}


/*
 * Testcase 38
 * Tested function: tty_ld_put / INTR
 * Testcase: verify that Ctrl-C does not flush the buffer if ISIG is set and NOFLSH is set
 */
int testcase38() {
    tty_t my_tty;
    unsigned char c;
    tty_ld_init(&my_tty);
    my_tty.settings.c_lflag = ICANON + ECHO + ISIG + NOFLSH;
    c = 3;
    last_sig_no = 0;
    ASSERT(0==tty_ld_put(&my_tty, "ab", 2));
    ASSERT(0==tty_ld_put(&my_tty, &c, 1));
    ASSERT(-1==my_tty.read_buffer_end);
    ASSERT(1==my_tty.line_buffer_end);
    ASSERT(__KSIGINT==last_sig_no);
    return 0;
}

/*
 * Testcase 39
 * Tested function: tty_ld_put / SUSP
 * Testcase: verify that Ctrl-Z flushes the buffer if ISIG is set and NOFLSH is clear
 */
int testcase39() {
    tty_t my_tty;
    unsigned char c;
    tty_ld_init(&my_tty);
    my_tty.settings.c_lflag = ICANON + ECHO + ISIG;
    c = my_tty.settings.c_cc[VSUSP];
    last_sig_no = 0;
    ASSERT(0==tty_ld_put(&my_tty, "ab", 2));
    ASSERT(0==tty_ld_put(&my_tty, &c, 1));
    ASSERT(-1==my_tty.read_buffer_end);
    ASSERT(-1==my_tty.line_buffer_end);
    ASSERT(__KSIGTSTP==last_sig_no);
    return 0;
}


/*
 * Testcase 40
 * Tested function: tty_ld_put / SUSP
 * Testcase: verify that Ctrl-C does not flush the buffer if ISIG is set and NOFLSH is set
 */
int testcase40() {
    tty_t my_tty;
    unsigned char c;
    tty_ld_init(&my_tty);
    my_tty.settings.c_lflag = ICANON + ECHO + ISIG + NOFLSH;
    c = my_tty.settings.c_cc[VSUSP];
    last_sig_no = 0;
    ASSERT(0==tty_ld_put(&my_tty, "ab", 2));
    ASSERT(0==tty_ld_put(&my_tty, &c, 1));
    ASSERT(-1==my_tty.read_buffer_end);
    ASSERT(1==my_tty.line_buffer_end);
    ASSERT(__KSIGTSTP==last_sig_no);
    return 0;
}

/*
 * Testcase 41
 * Tested function: cfgetospeed, cfsetospeed
 * Testcase: set and read output baud rate with valid values
 */
int testcase41() {
    struct termios term;
    ASSERT(0 == cfsetospeed(&term, B38400));
    ASSERT(B38400 == cfgetospeed(&term));
    return 0;
}

/*
 * Testcase 42
 * Tested function: cfgetispeed, cfsetispeed
 * Testcase: set and read output baud rate with a valid value
 */
int testcase42() {
    struct termios term;
    ASSERT(0 == cfsetispeed(&term, B38400));
    ASSERT(0 == cfsetospeed(&term, B19200));
    ASSERT(B38400 == cfgetispeed(&term));
    ASSERT(B19200 == cfgetospeed(&term));
    return 0;
}

/*
 * Testcase 43
 * Tested function: cfsetispeed, cfsetospeed
 * Testcase: try to set baud rates to invalid values
 */
int testcase43() {
    struct termios term;
    ASSERT(EINVAL == cfsetispeed(&term, 134567));
    ASSERT(EINVAL == cfsetospeed(&term, 134567));
    return 0;
}


/*
 * Testcase 44: do a timed read which results in a timeout
 * tty_read should return 0 in this case
 */
int testcase44() {
    char buffer[16];
    struct termios tt;
    tty_init();
    /*
     * Set the VTIME field
     */
    tty_tcgetattr(0, &tt);
    tt.c_cc[5] = 10;
    do_timeout = 1;
    ASSERT(tty_read(0, 16, buffer, 0) == 0);
    return 0;
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
    RUN_CASE(8);
    RUN_CASE(9);
    RUN_CASE(10);
    RUN_CASE(11);
    RUN_CASE(12);
    RUN_CASE(13);
    RUN_CASE(14);
    RUN_CASE(15);
    RUN_CASE(16);
    RUN_CASE(17);
    RUN_CASE(18);
    RUN_CASE(19);
    RUN_CASE(20);
    RUN_CASE(21);
    RUN_CASE(22);
    RUN_CASE(23);
    RUN_CASE(24);
    RUN_CASE(25);
    RUN_CASE(26);
    RUN_CASE(27);
    RUN_CASE(28);
    RUN_CASE(29);
    RUN_CASE(30);
    RUN_CASE(31);
    RUN_CASE(32);
    RUN_CASE(33);
    RUN_CASE(34);
    RUN_CASE(35);
    RUN_CASE(36);
    RUN_CASE(37);
    RUN_CASE(38);
    RUN_CASE(39);
    RUN_CASE(40);
    RUN_CASE(41);
    RUN_CASE(42);
    RUN_CASE(43);
    RUN_CASE(44);
    END;
}

