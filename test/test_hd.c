#include "kunit.h"
#include "hd.h"
#include "vga.h"
#include <stdio.h>

/*
 * The request queue which we use
 */
static hd_request_queue_t queue;

/*
 * Stubs for function which a HD driver needs to implement
 */
char data_buffer[1024];

void prepare_request(hd_request_queue_t* queue, hd_request_t* request) {

}

static int submit_request_called = 0;
void submit_request(hd_request_queue_t* queue, hd_request_t* request) {
    submit_request_called = 1;
}

/*
 * Other stubs
 */
void trap() {

}

int pm_get_task_id() {
    return 0;
}

void timer_wait(int x) {

}
static int do_putchar = 1;
void win_putchar(win_t* win, u8 c) {
    if (do_putchar)
        printf("%c", c);
}
void spinlock_get(spinlock_t* lock, u32* eflags) {
}
void spinlock_release(spinlock_t* lock, u32* eflags) {
}
void spinlock_init(spinlock_t* lock) {
}
void sem_init(semaphore_t* sem, u32 value) {

}
void sem_up(semaphore_t* sem) {

}
void __sem_down(semaphore_t* sem, char* file, int line) {

}
/*
 * Stub for kmalloc/kfree
 */
u32 kmalloc(size_t size) {
    return malloc(size);
}

u32 kmalloc_aligned(size_t size, u32 alignment) {
    return 0;
}

void kfree(u32 addr) {
    free((void*) addr);
}


/*
 * Utility function to initialize the queue
 */
static void init_queue() {
    queue.block_size = 512;
    queue.chunk_size = 65536;
    queue.device_busy = 0;
    spinlock_init(&queue.device_lock);
    queue.prepare_request = prepare_request;
    queue.head = 0;
    queue.tail = 0;
    sem_init(&queue.slots_available, HD_QUEUE_SIZE);
    queue.submit_request = submit_request;
}

/*
 * Testcase 1
 * Tested function: hd_rw
 * Testcase: add a request to the queue when the device_busy flag is not set
 * Verify that the submit_request function is called and that the head of the queue
 * contains a request with the correct content
 */
int testcase1() {
    hd_request_t* request;
    init_queue();
    hd_rw(&queue, 2, 3, 1, data_buffer, 16);
    ASSERT(submit_request_called==1);
    request = queue.queue;
    ASSERT(request->blocks == 2);
    ASSERT(request->buffer == (u32) data_buffer);
    ASSERT(request->first_block==3);
    ASSERT(request->minor_device==16);
    ASSERT(request->rw==1);
    ASSERT(1==queue.tail);
    ASSERT(0==queue.head);
    ASSERT(1==queue.device_busy);
    return 0;
}


/*
 * Testcase 2
 * Tested function: hd_put_request
 * Testcase: add a request to the queue when the device_busy flag is set
 * Verify that the submit_request function is not called and that the head of the queue
 * contains a request with the correct content
 */
int testcase2() {
    hd_request_t* request;
    init_queue();
    submit_request_called = 0;
    queue.device_busy = 1;
    hd_rw(&queue, 2, 3, 1, data_buffer, 16);
    ASSERT(submit_request_called==0);
    request = queue.queue;
    ASSERT(request->blocks == 2);
    ASSERT(request->buffer == (u32) data_buffer);
    ASSERT(request->first_block==3);
    ASSERT(request->minor_device==16);
    ASSERT(request->rw==1);
    ASSERT(1==queue.tail);
    ASSERT(0==queue.head);
    return 0;
}

/*
 * Testcase 3
 * Tested function: hd_handle_irq
 * Testcase: add a request to the queue when the device_busy flag is not set and call hd_handle_irq
 * Verify that the submit_request_function is not called again and that the tail of the queue is updated
 */
int testcase3() {
    hd_request_t* request;
    init_queue();
    submit_request_called = 0;
    queue.device_busy = 0;
    hd_rw(&queue, 2, 3, 1, data_buffer, 16);
    ASSERT(submit_request_called==1);
    request = queue.queue;
    ASSERT(request->blocks == 2);
    ASSERT(request->buffer == (u32) data_buffer);
    ASSERT(request->first_block==3);
    ASSERT(request->minor_device==16);
    ASSERT(request->rw==1);
    ASSERT(1==queue.tail);
    ASSERT(0==queue.head);
    /*
     * Now simulate interrupt
     */
    submit_request_called = 0;
    hd_handle_irq(&queue, 0);
    ASSERT(submit_request_called==0);
    ASSERT(1==queue.tail);
    ASSERT(1==queue.head);
    return 0;
}

/*
 * Testcase 4
 * Tested function: hd_handle_irq
 * Testcase: add two request to queue, then simulate interrupt
 * Verify that the submit_request_function is called again
 */
int testcase4() {
    hd_request_t* request;
    init_queue();
    submit_request_called = 0;
    queue.device_busy = 0;
    hd_rw(&queue, 2, 3, 1, data_buffer, 16);
    ASSERT(submit_request_called==1);
    hd_rw(&queue, 2, 1, 1, data_buffer, 16);
    request = queue.queue+1;
    ASSERT(request->blocks == 2);
    ASSERT(request->buffer == (u32) data_buffer);
    ASSERT(request->first_block==1);
    ASSERT(request->minor_device==16);
    ASSERT(request->rw==1);
    ASSERT(2==queue.tail);
    ASSERT(0==queue.head);
    /*
     * Now simulate interrupt
     */
    submit_request_called = 0;
    hd_handle_irq(&queue, 0);
    ASSERT(submit_request_called==1);
    ASSERT(2==queue.tail);
    ASSERT(1==queue.head);
    return 0;
}

int main() {
    INIT;
    RUN_CASE(1);
    RUN_CASE(2);
    RUN_CASE(3);
    RUN_CASE(4);
    END;
}
