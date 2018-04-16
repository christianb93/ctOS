/*
 * test_pipes.c
 *
 */


#include "kunit.h"
#include "fs_pipe.h"
#include "vga.h"
#include <stdio.h>

/*
 * Stub for kputchar
 */
static int do_print = 1;
void win_putchar(win_t* win, u8 c) {
    if (do_print)
        printf("%c", c);
}

/*
 * Stubs for condition variables and locks
 */
static u32 ie = 1;
void spinlock_init(spinlock_t* lock) {
    *((u32*) lock) = 0;
}

void spinlock_get(spinlock_t* lock, u32* flags) {
    if (*((u32*) lock) == 1) {
        printf(
                "----------- Spinlock requested which is not available! ----------------\n");
        _exit(1);
    }
    *((u32*) lock) = 1;
    *flags = ie;
}
void spinlock_release(spinlock_t* lock, u32* flags) {
    *((u32*) lock) = 0;
    ie = *flags;
}

void cond_init(cond_t* cond) {

}

static int cond_broadcast_called = 0;
void cond_broadcast(cond_t* cond) {
    cond_broadcast_called = 1;
}

/*
 * Dummy for cond_wait_intr. As we cannot really wait in a single-threaded
 * unit test, we always return -1 here, i.e. we simulate the case that we
 * were interrupted
 */
int cond_wait_intr(cond_t* cond, spinlock_t* lock, u32* eflags) {
    spinlock_release(lock, eflags);
    return -1;
}

/*
 * Stub for kmalloc/kfree
 */
u32 kmalloc(size_t size) {
    return malloc(size);
}
void kfree(u32 addr) {
    free((void*) addr);
}


/*
 * Testcase 1: create a pipe
 */
int testcase1() {
    ASSERT(fs_pipe_create());
    return 0;
}

/*
 * Testcase 2: connect to a pipe for reading
 */
int testcase2() {
    pipe_t* pipe = fs_pipe_create();
    ASSERT(pipe);
    ASSERT(0==fs_pipe_connect(pipe, PIPE_READ));
    return 0;
}

/*
 * Testcase 3: connect to a pipe for writing
 */
int testcase3() {
    pipe_t* pipe = fs_pipe_create();
    ASSERT(pipe);
    ASSERT(0==fs_pipe_connect(pipe, PIPE_WRITE));
    return 0;
}

/*
 * Testcase 4: connect to a pipe with an invalid mode
 */
int testcase4() {
    pipe_t* pipe = fs_pipe_create();
    ASSERT(pipe);
    ASSERT(fs_pipe_connect(pipe, 177));
    return 0;
}

/*
 * Testcase 5: disconnect the writing end of a pipe - no more files connected
 */
int testcase5() {
    pipe_t* pipe = fs_pipe_create();
    ASSERT(pipe);
    ASSERT(0==fs_pipe_connect(pipe, PIPE_WRITE));
    ASSERT(-1==fs_pipe_disconnect(pipe, PIPE_WRITE));
    return 0;
}

/*
 * Testcase 6: disconnect the writing end of a pipe - still other files connected
 * for writing
 */
int testcase6() {
    pipe_t* pipe = fs_pipe_create();
    ASSERT(pipe);
    ASSERT(0==fs_pipe_connect(pipe, PIPE_WRITE));
    ASSERT(0==fs_pipe_connect(pipe, PIPE_WRITE));
    ASSERT(0==fs_pipe_disconnect(pipe, PIPE_WRITE));
    return 0;
}

/*
 * Testcase 7: disconnect the writing end of a pipe - still other files connected
 * for reading
 */
int testcase7() {
    pipe_t* pipe = fs_pipe_create();
    ASSERT(pipe);
    ASSERT(0==fs_pipe_connect(pipe, PIPE_WRITE));
    ASSERT(0==fs_pipe_connect(pipe, PIPE_READ));
    ASSERT(0==fs_pipe_disconnect(pipe, PIPE_WRITE));
    return 0;
}


/*
 * Testcase 8: disconnect the reading end of a pipe - no more files connected
 */
int testcase8() {
    pipe_t* pipe = fs_pipe_create();
    ASSERT(pipe);
    ASSERT(0==fs_pipe_connect(pipe, PIPE_READ));
    ASSERT(-1==fs_pipe_disconnect(pipe, PIPE_READ));
    return 0;
}

/*
 * Testcase 9: disconnect the reading end of a pipe - still other files connected
 * for reading
 */
int testcase9() {
    pipe_t* pipe = fs_pipe_create();
    ASSERT(pipe);
    ASSERT(0==fs_pipe_connect(pipe, PIPE_READ));
    ASSERT(0==fs_pipe_connect(pipe, PIPE_READ));
    ASSERT(0==fs_pipe_disconnect(pipe, PIPE_READ));
    return 0;
}

/*
 * Testcase 10: disconnect the reading end of a pipe - still other files connected
 * for writing
 */
int testcase10() {
    pipe_t* pipe = fs_pipe_create();
    ASSERT(pipe);
    ASSERT(0==fs_pipe_connect(pipe, PIPE_WRITE));
    ASSERT(0==fs_pipe_connect(pipe, PIPE_READ));
    ASSERT(0==fs_pipe_disconnect(pipe, PIPE_READ));
    return 0;
}

/*
 * Testcase 11: write to a pipe to which no readers are connected
 */
int testcase11() {
    char buffer[256];
    pipe_t* pipe = fs_pipe_create();
    ASSERT(pipe);
    ASSERT(0==fs_pipe_connect(pipe, PIPE_WRITE));
    ASSERT(-126==fs_pipe_write(pipe, 10, buffer, 0));
    return 0;
}

/*
 * Testcase 12: write to a pipe to which a reader is connected. Write less than
 * PIPE_BUF bytes while pipe is empty
 */
int testcase12() {
    char buffer[256];
    pipe_t* pipe = fs_pipe_create();
    ASSERT(pipe);
    ASSERT(0==fs_pipe_connect(pipe, PIPE_WRITE));
    ASSERT(0==fs_pipe_connect(pipe, PIPE_READ));
    cond_broadcast_called = 0;
    ASSERT(256==fs_pipe_write(pipe, 256, buffer, 0));
    ASSERT(1==cond_broadcast_called);
    return 0;
}

/*
 * Testcase 13: write to a pipe to which a reader is connected. Write less than
 * PIPE_BUF bytes when pipe is full
 */
int testcase13() {
    char buffer[256];
    pipe_t* pipe = fs_pipe_create();
    int i;
    ASSERT(pipe);
    ASSERT(0==fs_pipe_connect(pipe, PIPE_WRITE));
    ASSERT(0==fs_pipe_connect(pipe, PIPE_READ));
    /*
     * Fill up pipe
     */
    for (i=0;i<4;i++)
        ASSERT(256==fs_pipe_write(pipe, 256, buffer, 0));
    /*
     * Next write should return -EPAUSE as our wait stub always simulates
     * getting a signal
     */
    cond_broadcast_called = 0;
    ASSERT(-122==fs_pipe_write(pipe, 256, buffer, 0));
    ASSERT(0==cond_broadcast_called);
    return 0;
}

/*
 * Testcase 14: write to a pipe to which a reader is connected. Write more than
 * PIPE_BUF bytes when pipe is full
 */
int testcase14() {
    char buffer[2048];
    pipe_t* pipe = fs_pipe_create();
    int i;
    ASSERT(pipe);
    ASSERT(0==fs_pipe_connect(pipe, PIPE_WRITE));
    ASSERT(0==fs_pipe_connect(pipe, PIPE_READ));
    /*
     * Fill up pipe until only 256 bytes are left
     */
    for (i=0;i<3;i++)
        ASSERT(256==fs_pipe_write(pipe, 256, buffer, 0));
    /*
     * Next write should 256 as our wait stub always simulates
     * getting a signal. Thus we are interrupt by a signal, but as the
     * write is allowed to be non-atomically, we should have written 256 bytes
     * and notified readers
     */
    ASSERT(2048>PIPE_BUF);
    cond_broadcast_called = 0;
    ASSERT(256==fs_pipe_write(pipe, 2048, buffer, 0));
    ASSERT(cond_broadcast_called==1);
    return 0;
}

/*
 * Testcase 15: write two bytes from a pipe and read two bytes
 */
int testcase15() {
    char buffer;
    pipe_t* pipe = fs_pipe_create();
    ASSERT(pipe);
    ASSERT(0==fs_pipe_connect(pipe, PIPE_WRITE));
    ASSERT(0==fs_pipe_connect(pipe, PIPE_READ));
    buffer = 'a';
    ASSERT(1==fs_pipe_write(pipe, 1, &buffer, 0));
    buffer = 'b';
    ASSERT(1==fs_pipe_write(pipe, 1, &buffer, 0));
    ASSERT(1==fs_pipe_read(pipe, 1, &buffer, 0));
    ASSERT(buffer=='a');
    ASSERT(1==fs_pipe_read(pipe, 1, &buffer, 0));
    ASSERT(buffer=='b');
    return 0;
}

/*
 * Testcase 16: read from an empty pipe to which a writer is connected
 * As our cond_wait_intr stub simulates the case of being interrupted, this
 * should return -EPAUSE
 */
int testcase16() {
    char buffer;
    pipe_t* pipe = fs_pipe_create();
    ASSERT(pipe);
    ASSERT(0==fs_pipe_connect(pipe, PIPE_WRITE));
    ASSERT(0==fs_pipe_connect(pipe, PIPE_READ));
    ASSERT(-122==fs_pipe_read(pipe, 1, &buffer, 0));
    return 0;
}

/*
 * Testcase 17: write one byte from a pipe and read two bytes. This
 * should return 1 byte only (as our cond_wait_intr always simulates a signal)
 */
int testcase17() {
    char buffer;
    pipe_t* pipe = fs_pipe_create();
    ASSERT(pipe);
    ASSERT(0==fs_pipe_connect(pipe, PIPE_WRITE));
    ASSERT(0==fs_pipe_connect(pipe, PIPE_READ));
    buffer = 'a';
    ASSERT(1==fs_pipe_write(pipe, 1, &buffer, 0));
    ASSERT(1==fs_pipe_read(pipe, 2, &buffer, 0));
    ASSERT(buffer=='a');
    ASSERT(-122==fs_pipe_read(pipe, 1, &buffer, 0));
    return 0;
}

/*
 * Testcase 18: write PIPE_BUF to a pipe and read it again
 */
int testcase18() {
    char in_buffer[PIPE_BUF];
    char out_buffer[PIPE_BUF];
    memset(in_buffer, 0x1, PIPE_BUF);
    memset(out_buffer, 0x0, PIPE_BUF);
    pipe_t* pipe = fs_pipe_create();
    int i;
    ASSERT(pipe);
    ASSERT(0==fs_pipe_connect(pipe, PIPE_WRITE));
    ASSERT(0==fs_pipe_connect(pipe, PIPE_READ));
    ASSERT(PIPE_BUF == fs_pipe_write(pipe, PIPE_BUF, in_buffer, 0));
    ASSERT(PIPE_BUF == fs_pipe_read(pipe, PIPE_BUF, out_buffer, 0));
    for (i=0;i<PIPE_BUF;i++)
        ASSERT(in_buffer[i]==out_buffer[i]);
    return 0;
}

/*
 * Testcase 19: write two bytes from a pipe and read three bytes
 */
int testcase19() {
    char buffer;
    char result[3];
    pipe_t* pipe = fs_pipe_create();
    ASSERT(pipe);
    ASSERT(0==fs_pipe_connect(pipe, PIPE_WRITE));
    ASSERT(0==fs_pipe_connect(pipe, PIPE_READ));
    buffer = 'a';
    ASSERT(1==fs_pipe_write(pipe, 1, &buffer, 0));
    buffer = 'b';
    ASSERT(1==fs_pipe_write(pipe, 1, &buffer, 0));
    /*
     * Now read three bytes - should return 2
     */
    ASSERT(2==fs_pipe_read(pipe, 3, result, 0));
    ASSERT(result[0]=='a');
    ASSERT(result[1]=='b');
    return 0;
}

/*
 * Testcase 20: read with nowait from an empty pipe to which a writer is connected
 * This should return -EAGAIN
 */
int testcase20() {
    char buffer;
    pipe_t* pipe = fs_pipe_create();
    ASSERT(pipe);
    ASSERT(0==fs_pipe_connect(pipe, PIPE_WRITE));
    ASSERT(0==fs_pipe_connect(pipe, PIPE_READ));
    ASSERT(-106==fs_pipe_read(pipe, 1, &buffer, 1));
    return 0;
}

/*
 * Testcase 21: write with O_NONBLOCK to a pipe to which a reader is connected. Write less than
 * PIPE_BUF bytes when pipe is full
 */
int testcase21() {
    char buffer[256];
    pipe_t* pipe = fs_pipe_create();
    int i;
    ASSERT(pipe);
    ASSERT(0==fs_pipe_connect(pipe, PIPE_WRITE));
    ASSERT(0==fs_pipe_connect(pipe, PIPE_READ));
    /*
     * Fill up pipe
     */
    for (i=0;i<4;i++)
        ASSERT(256==fs_pipe_write(pipe, 256, buffer, 0));
    /*
     * Next write with nowait=1 - this should return -EAGAIN
     */
    cond_broadcast_called = 0;
    ASSERT(-106==fs_pipe_write(pipe, 256, buffer, 1));
    ASSERT(0==cond_broadcast_called);
    return 0;
}

/*
 * Testcase 22: write with O_NONBLOCK to a pipe to which a reader is connected. Write more than
 * PIPE_BUF bytes when pipe is full
 */
int testcase22() {
    char buffer[2048];
    pipe_t* pipe = fs_pipe_create();
    int i;
    ASSERT(pipe);
    ASSERT(0==fs_pipe_connect(pipe, PIPE_WRITE));
    ASSERT(0==fs_pipe_connect(pipe, PIPE_READ));
    /*
     * Fill up pipe until only 256 bytes are left
     */
    for (i=0;i<3;i++)
        ASSERT(256==fs_pipe_write(pipe, 256, buffer, 0));
    /*
     * Next write should return 256
     */
    ASSERT(2048>PIPE_BUF);
    cond_broadcast_called = 0;
    ASSERT(256==fs_pipe_write(pipe, 2048, buffer, 1));
    ASSERT(cond_broadcast_called==1);
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
    END;
}

