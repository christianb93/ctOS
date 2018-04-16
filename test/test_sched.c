/*
 * test_sched.c
 */

#include "kunit.h"
#include <stdio.h>
#include "sched.h"
#include "pm.h"
#include "locks.h"
#include "timer.h"
#include "vga.h"


void win_putchar(win_t* win, u8 c) {
    printf("%c", c);
}

void trap() {

}

void debug_getline(void* c, int n) {

}

unsigned int get_eflags() {
    return 0;
}

static int cpuid = 0;
int smp_get_cpu() {
    return cpuid;
}

u32 atomic_load(u32* ptr) {
    return *ptr;
}

void atomic_store(u32* ptr, u32 value) {
    *ptr = value;
}

void save_eflags(unsigned int* eflags) {

}

void restore_eflags(unsigned int* eflags) {

}

void cli() {

}

int apic_send_ipi(u8 apic_id, u8 ipi, u8 vector, int deassert) {
    return 0;
}
int cpu_get_apic_id(int cpuid) {
    return 0;
}

int params_get_int(char* param) {
    return 0;
}

/*
 * Stubs for spinlock functions
 */
void spinlock_init(spinlock_t* lock) {
}
void spinlock_get(spinlock_t* lock, u32* flags) {
}
void spinlock_release(spinlock_t* lock, u32* flags) {
}

/*
 * Stub for pm_get_active_task
 */
int pm_get_task_id() {
    return 0;
}


/*
 * Testcase 1
 * Tested function: sched_enqueue
 * Verify that if a new task is added which has a higher priority than
 * the currently active task, the next call to schedule yields this task
 */
int testcase1() {
    sched_init();
    ASSERT(0==sched_get_queue_length(0));
    sched_enqueue(1, 0);
    ASSERT(1==sched_get_queue_length(0));
    sched_yield();
    ASSERT(1==sched_schedule());
    ASSERT(1==sched_get_queue_length(0));
    /*
     * Now runnable 1 is active. Add a second task which has priority 0
     * and check that next call to scheduler returns this one
     */
    sched_enqueue(2, 1);
    ASSERT(2==sched_schedule());
    ASSERT(2==sched_get_queue_length(0));
    return 0;
}

/*
 * Testcase 2
 * Tested function: sched_schedule
 * Verify that no scheduling is done if the quantum of the current
 * process is not exceeded
 */
int testcase2() {
    int i;
    sched_init();
    sched_enqueue(1, SCHED_MAX_PRIO);
    ASSERT(1==sched_schedule());
    /*
     * Now runnable 1 is active. Verify that additional calls of the scheduler do not change this
     */
    for (i = 0; i < 100; i++)
        ASSERT(1==sched_schedule());
    return 0;
}

/*
 * Testcase 3
 * Tested function: sched_schedule
 * Verify that scheduling is done if the quantum of the current
 * process is zero
 */
int testcase3() {
    int i;
    sched_init();
    sched_enqueue(1, 1);
    ASSERT(1==sched_get_queue_length(0));
    ASSERT(1==sched_schedule());
    /*
     * Now runnable 1 is active. Call sched_do_tick sufficiently often to get quantum to zero
     * and then call sched_schedule. This should place the task at the end of the ready queue for prio 0
     */
    for (i = 0; i < SCHED_INIT_QUANTUM; i++)
        sched_do_tick();
    ASSERT(0==sched_schedule());
    ASSERT(1==sched_get_queue_length(0));
    return 0;
}

/*
 * Testcase 4
 * Tested function: sched_dequeue
 * Verify that scheduling if the active runnable is removed, it is not scheduled again
 */
int testcase4() {
    int i;
    sched_init();
    sched_enqueue(1, 1);
    ASSERT(1==sched_get_queue_length(0));
    ASSERT(1==sched_schedule());
    /*
     * Now runnable 1 is active. Call sched_dequeue on it and verify that the next call to sched_schedule
     * yields 0 again
     */
    sched_dequeue();
    /*
     * Queue length is still 1 at this point
     */
    ASSERT(1==sched_get_queue_length(0));
    ASSERT(0==sched_schedule());
    /*
     * This will have removed runnable 0 from the queue
     */
    ASSERT(0==sched_get_queue_length(0));
    return 0;
}

/*
 * Testcase 5
 * Tested function: sched_dequeue
 * Verify that removing a ready runnable prevents it from being scheduled
 */
int testcase5() {
    int i;
    sched_init();
    sched_enqueue(1, 1);
    ASSERT(1==sched_get_queue_length(0));
    /*
     * Schedule once to make runnable 1 the active runnable
     */
    ASSERT(1==sched_schedule());
    ASSERT(1==sched_get_queue_length(0));
    /*
     * and remove it
     */
    sched_dequeue();
    ASSERT(1==sched_get_queue_length(0));
    /*
     * If the dequeue did not work, the runnable will still be in the prio 1 queue
     * and would be scheduled - make sure that this does not happen
     */
    ASSERT(0==sched_schedule());
    ASSERT(0==sched_get_queue_length(0));
    return 0;
}

/*
 * Testcase 6
 * Tested function: sched_dequeue
 * Test round-robin within one priority
 */
int testcase6() {
    int i;
    sched_init();
    /* We now add two additional runnables to the queue */
    sched_enqueue(1, 0);
    sched_enqueue(2, 0);
    ASSERT(2==sched_get_queue_length(0));
    /*
     * Now simulate SCHED_INIT_QUANTUM clock ticks and then do reschedule. This should
     * schedule task 1
     */
    for (i = 0; i < SCHED_INIT_QUANTUM; i++) {
        ASSERT(0==sched_schedule());
        sched_do_tick();
    }
    ASSERT(1==sched_schedule());
    ASSERT(2==sched_get_queue_length(0));
    /*
     * Repeat this to schedule task 2
     */
    for (i = 0; i < SCHED_INIT_QUANTUM; i++) {
        ASSERT(1==sched_schedule());
        sched_do_tick();
    }
    ASSERT(2==sched_schedule());
    ASSERT(2==sched_get_queue_length(0));
    /*
     * Now we should get back to 0 again
     */
    for (i = 0; i < SCHED_INIT_QUANTUM; i++) {
        ASSERT(2==sched_schedule());
        sched_do_tick();
    }
    ASSERT(0==sched_schedule());
    /*
     * Finally verify that the quantum has been fully refreshed, i.e.
     * we can again call sched_do_tick SCHED_INIT_QUANTUM times before
     * a task switch happens
     */
    for (i = 0; i < SCHED_INIT_QUANTUM; i++) {
        ASSERT(0==sched_schedule());
        sched_do_tick();
    }
    ASSERT(1==sched_schedule());
    for (i = 0; i < SCHED_INIT_QUANTUM; i++) {
        ASSERT(1==sched_schedule());
        sched_do_tick();
    }
    ASSERT(2==sched_schedule());
    ASSERT(2==sched_get_queue_length(0));
    return 0;
}

/*
 * Testcase 7
 * Tested function: sched_yield
 * Test that after a sched_yield, the runnable is added to the end of the queue with its
 * old priority and its left-over quantum
 */
int testcase7() {
    int i;
    sched_init();
    /*
     * We add two tasks with priority 1 first
     */
    sched_enqueue(1, 1);
    sched_enqueue(2, 1);
    ASSERT(2==sched_get_queue_length(0));
    /*
     * Now task 1 should become active
     */
    ASSERT(1==sched_schedule());
    ASSERT(2==sched_get_queue_length(0));
    /*
     * Use up 1 tick
     */
    sched_do_tick();
    /*
     * and yield - this should result in
     * task 2 being active
     */
    sched_yield();
    ASSERT(2==sched_schedule());
    ASSERT(2==sched_get_queue_length(0));
    /*
     * Now verify that task 1 is still in priority 1 queue and
     * has only SCHED_INIT_QUANTUM-1 ticks left
     * First bring it back to the front by consuming task 2
     */
    for (i = 0; i < SCHED_INIT_QUANTUM; i++) {
        ASSERT(2==sched_schedule());
        sched_do_tick();
    }
    /*
     * Now make sure that 1 is selected next - this would not happen
     * if 1 would have been moved to the priority 0 queue as 0 is at the head of this queue
     */
    ASSERT(1==sched_schedule());
    /*
     * Finally verify that this time, SCHED_INIT_QUANTUM -1 ticks suffice to
     * make 1 inactive
     */
    for (i = 0; i < SCHED_INIT_QUANTUM - 1; i++) {
        ASSERT(1==sched_schedule());
        sched_do_tick();
    }
    ASSERT(0==sched_schedule());
    ASSERT(2==sched_get_queue_length(0));
    return 0;
}

/*
 * Testcase 8
 * Tested function: sched_add_idle_task
 */
int testcase8() {
    int i;
    sched_init();
    /*
     * Set up idle task for CPU 1
     */
    sched_add_idle_task(1, 1);
    /*
     * Simulate the case that CPU 1 schedules
     */
    cpuid = 1;
    ASSERT(1==sched_schedule());
    ASSERT(0==sched_get_queue_length(1));
    cpuid = 0;
    return 0;
}

/*
 * Testcase 9
 * Tested function: sched_enqueue
 * Bring up to CPUs and test that the second task is added
 * to the second CPU
 */
int testcase9() {
    int i;
    sched_init();
    /*
     * Set up idle task for CPU 1
     * which will activate the CPU
     */
    sched_add_idle_task(1, 1);
    ASSERT(0==sched_get_queue_length(1));
    /*
     * Add another task to the queue - this should go to the BSP
     */
    sched_enqueue(1, 0);
    ASSERT(1==sched_get_queue_length(0));
    ASSERT(0==sched_get_queue_length(1));
    /*
     * Next one should go to CPU 1
     */
    sched_enqueue(2, 0);
    ASSERT(1==sched_get_queue_length(0));
    ASSERT(1==sched_get_queue_length(1));
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
    END;
}

