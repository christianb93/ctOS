/*
 * tests.c
 *
 *  This file contains some of the built-in assembly tests which the kernel
 *  will execute at startup if the parameter do_test is set to 1 and the
 *  corresponding switches in tests.h are turned on
 */

#include "tests.h"
#include "params.h"
#include "drivers.h"
#include "dm.h"
#include "fs.h"
#include "pm.h"
#include "mm.h"
#include "debug.h"
#include "locks.h"
#include "util.h"
#include "lib/string.h"
#include "pata.h"
#include "ahci.h"
#include "pit.h"
#include "sched.h"
#include "lib/os/syscalls.h"
#include "lib/os/oscalls.h"
#include "smp.h"
#include "rtc.h"
#include "timer.h"
#include "io.h"
#include "eth.h"
#include "net_if.h"
#include "ip.h"


#ifdef DO_SMP_TEST
extern pte_t* (*mm_get_pt_address)(pte_t* ptd, int ptd_offset, int pg_enabled);
#endif

#ifdef  DO_THREAD_TEST
static ecb_t ecb;
static semaphore_t sem;
static rw_lock_t rw_lock1;
static rw_lock_t rw_lock2;
static rw_lock_t rw_lock3;
static u32 timer_kill = 0;
static void* task_main(void* arg) {
    int count = 0;
    while (1) {
        if (1==timer_kill) {
            /*
            * Execute system call to quit task
            */
            __ctOS_syscall(__SYSNO_QUIT, 0);
            PANIC("Should never get here\n");
        }
        count++;
        halt();
    }
    return 0;
}
#endif

#ifdef DO_SMP_TEST
static u32 tlb_test_page = 0;
static u32 tlb_page_reserved = 0;
static u32 tlb_page_read = 0;
static u32 tlb_page_write = 0;
#endif

#ifdef DO_SMP_TEST
/*
 * This thread of the TLB test is supposed to run on the BSP. It will allocate
 * an additional page on the kernel heap and write the address of it into a
 * variable
 */
static void do_smp_tlb_test_t0() {
    pte_t* pt;
    pte_t* ptd = (pte_t*) get_cr3();
    u32 value;
    tlb_test_page = (u32) kmalloc_aligned(10*4096, 4096) + 8*4096;
    /*
     * Get pointer to page table
     */
    pt = mm_get_pt_address(ptd, PTD_OFFSET(tlb_test_page), 1);
    smp_mb();
    /*
     * Now write
     */
    value = 0xabcdffff;
    *((u32*)tlb_test_page) = value;
    /*
     * and set page table entry to read-only
     */
    pt[PT_OFFSET(tlb_test_page)].rw = 0;
    /*
     * Inform thread 1. Thread 1 will then read from this address,
     * this will fill its TLB with the information that this is a read-only
     * page
     */
    smp_mb();
    tlb_page_reserved = 1;
    smp_mb();
    /*
     * Wait until thread has read
     */
    while (0 == tlb_page_read) {
        asm("hlt");
    }
    /*
     * Now map page back to read/write
     */
    pt[PT_OFFSET(tlb_test_page)].rw = 1;
    smp_mb();
    /*
     * and tell thread 1 to write to address
     */
    tlb_page_write = 1;
    smp_mb();
}

/*
 * This is the part of the TLB test running on the AP.
 */
static void do_smp_tlb_test_t1() {
    if (params_get_int("do_test") == 0)
        return;
    u32 value;
    while (0 == tlb_page_reserved) {
        asm("pause");
    }
    value = *((u32*)(tlb_test_page));
    tlb_page_read = 1;
    smp_mb();
    while (0 == tlb_page_write) {
        asm("pause");
    }
    /*
     * Now write - this should give us a page fault
     */
    *((u32*)(tlb_test_page)) = 0;
    PRINT("do_smp_tlb_test_t1: write successful, value is %x\n", value);
}
#endif


#ifdef DO_SMP_TEST
#define SMP_TEST2_ITERATIONS 50000000

void do_smp_test2() {
    u32 time = do_time(0);
    int i;
    if (0 == params_get_int("do_test"))
            return;
    /*
     * Call pm_get_pid and pm_get_task_id
     */
    kprintf("Doing SMP test 2 part I (pm_get_pid)\n");
    for (i = 0; i< SMP_TEST2_ITERATIONS; i++) {
        pm_get_pid();
    }
    kprintf("SMP test 2 part I completed, this took %d seconds for %d iterations\n", do_time(0)-time, SMP_TEST2_ITERATIONS);
    kprintf("Doing SMP test 2 part II (pm_get_task_id)\n");
    time = do_time(0);
    for (i = 0; i< SMP_TEST2_ITERATIONS; i++) {
        pm_get_task_id();
    }
    kprintf("SMP test 2 part II completed, this took %d seconds for %d iterations\n", do_time(0)-time, SMP_TEST2_ITERATIONS);
}


#endif

/*
 * This is the main entry point for those tests which are executed
 * before the INIT process is started, i.e. in the context of task 0
 */
void do_pre_init_tests() {
    if (params_get_int("do_test") == 0)
        return;
#ifdef DO_TEST
    PRINT("Starting tests (to compile without tests, drop -DDO_TEST)\n");
#endif
#ifdef DO_EFLAGS_TEST
    u32 eflags;
    PRINT("Calling save_eflags...");
    save_eflags(&eflags);
    PRINT("done, EFLAGS=%x\n", eflags);
#endif
#ifdef DO_XCHG_TEST
    u32 test1;
    u32 test2;
    test1 = 1;
    test2 = 2;
    PRINT("Testing xchg...");
    test1 = xchg(test1, &test2);
    KASSERT(test1==2);
    KASSERT(test2==1);
    PRINT("done, test1=%d (was: 1), test2=%d (was: 2)\n", test1, test2);
#endif
#ifdef DO_PHYS_PAGES_TEST
    do_phys_pages_test();
#endif
#ifdef DO_PAGING_TEST
    /*
     * This test will map a physical page to two different
     * virtual pages and then read and write to it to see
     * that they both map to the same page
     */
    mm_do_paging_test();
#endif
#ifdef DO_KHEAP_TEST
    mm_do_kheap_test();
#endif
#ifdef DO_ATTACH_TEST
    mm_do_attach_test();
#endif
#ifdef DO_RAMDISK_TEST
    char buffer[1024];
    if (dm_get_blk_dev_ops(MAJOR_RAMDISK)) {
        KASSERT(dm_get_blk_dev_ops(MAJOR_RAMDISK)->read);
        KASSERT(dm_get_blk_dev_ops(MAJOR_RAMDISK)->write);
        dm_get_blk_dev_ops(MAJOR_RAMDISK)->read(0, 1, 1, (void*) buffer);
        PRINT("First dword in second block of ram disk: %x\n", *((u32*)buffer));
    }
    else
        PRINT("Skipping RAMDISK test, as no ramdisk present\n");
#endif
}

/*
 * These tests are executed after the INIT process has been
 * spawned, the file system has been established and STDIN
 * and STDOUT have been opened
 */
void do_post_init_tests() {
    int rc;
    if (params_get_int("do_test")) {
#ifdef DO_FS_TEST
        int fd;
        char hdata[6];
        fd = do_open("/hello", 0, 0);
        KASSERT(fd==2);
        do_read(fd, (void*) hdata, 5);
        do_close(fd);
        hdata[5] = 0;
        KASSERT(0==strcmp("Hello", hdata));
        PRINT("Ext2 FS test successful, content of file /hello is %s\n", hdata);
#endif
#ifdef DO_TTY_TEST
        char buffer[11];
        PRINT("Reading data from keyboard (max. 10 characters)\n");
        rc = do_read(0, buffer, 10);
        KASSERT(rc<=10);
        buffer[rc] = 0;
        PRINT("Read %d characters: %s\n", rc, buffer);
#endif
#ifdef  DO_THREAD_TEST
        /*
         * This variable is used to check that we really have separated address spaces
         */
        u32 check;
        PRINT("Doing thread test...");
        u32 thread;
        u32 thread_rc;
        int i;
        thread_rc = __ctOS_syscall(1, 4, &thread, 0, task_main, 0);
        if (0 == thread_rc) {
            PRINT("success, thread=%d\n", thread);
        }
        else
            PRINT("failure, thread_rc = %x\n", thread_rc);
        /* Fork off a new process to see what happens if
         * we fork with multiple threads
         */
        PRINT("Forking off new process\n");
        check = 0;
        ecb.waiting_task = 999;
        sem_init(&sem, 0);
        rc = __ctOS_fork();
        if (rc == 0) {
            /*
             * This is process 2, task 3
             */
            check = 100;
            PRINT("New process %d / task %d  doing down on semaphore\n", pm_get_pid(), pm_get_task_id());
            sem_down(&sem);
            KASSERT(check==100);
            __ctOS_syscall(__SYSNO_QUIT, 0);
            PANIC("Should never get here\n");
        }
        /*
         * This is process 1, task 1
         */
        PRINT("Process %d: waiting for 10 ticks\n", pm_get_pid());
        for (i = 0; i < 10; i++) {
            check = 0;
            sched_yield();
            reschedule();
        }
        PRINT("Process %d / task %d: calling up on semaphore\n", pm_get_pid(), pm_get_task_id());
        sem_up(&sem);
        /*
         * Now test read/write locks. We first verify that we can get a read lock twice
         */
        rw_lock_init(&rw_lock1);
        rw_lock_init(&rw_lock2);
        rw_lock_init(&rw_lock3);
        PRINT("Process %d / task %d: getting read lock once ...", pm_get_pid(), pm_get_task_id());
        rw_lock_get_read_lock(&rw_lock1);
        PRINT("and once more...");
        rw_lock_get_read_lock(&rw_lock1);
        PRINT("done\n");
        /*
         * Next we fork off a second process. This process will try to get a write lock
         */
        rc = __ctOS_fork();
        if (0==rc) {
            PRINT("Process %d / task %d: trying to get write lock\n", pm_get_pid(), pm_get_task_id());
            rw_lock_get_write_lock(&rw_lock1);
            PRINT("Process %d / task %d: got write lock\n", pm_get_pid(), pm_get_task_id());
            __ctOS_syscall(__SYSNO_QUIT, 0);
            PANIC("Should never get here!\n");
        }
        sched_yield();
        reschedule();
        /*
         * Wait for 5 ticks, then releasing first read lock
         */
        PRINT("Process %d: waiting for another 5 ticks...", pm_get_pid());
        for (i = 0; i < 5; i++) {
            sched_yield();
            reschedule();
        }
        PRINT("now releasing first read lock\n");
        rw_lock_release_read_lock(&rw_lock1);
        PRINT("Process %d: waiting for another 5 ticks...", pm_get_pid());
        for (i = 0; i < 5; i++) {
            sched_yield();
            reschedule();
        }
        PRINT("now releasing second read lock\n");
        rw_lock_release_read_lock(&rw_lock1);
        sched_yield();
        reschedule();
        /*
         * Now acquire a write lock on our second test lock
         */
        rw_lock_get_write_lock(&rw_lock2);
        PRINT("Process %d / task %d: got write lock on rw_lock2\n", pm_get_pid(), pm_get_task_id());
        PRINT("Process %d / task %d: forking off process which will try to get a read lock\n", pm_get_pid(), pm_get_task_id());
        rc = __ctOS_fork();
        if (0==rc) {
            PRINT("Process %d / task %d: trying to get read lock on rw_lock2\n", pm_get_pid(), pm_get_task_id());
            rw_lock_get_read_lock(&rw_lock2);
            PRINT("Process %d / task %d: got read lock on rw_lock2\n", pm_get_pid(), pm_get_task_id());
            __ctOS_syscall(__SYSNO_QUIT, 0);
            PANIC("Should never get here\n");
        }
        /*
         * Wait for 10 ticks, then release write lock
         */
        sched_yield();
        reschedule();
        PRINT("Process %d: waiting for another 10 ticks...", pm_get_pid());
        for (i = 0; i < 10; i++) {
            sched_yield();
            reschedule();
        }
        PRINT("now releasing write lock on rw_lock2\n");
        rw_lock_release_write_lock(&rw_lock2);
        sched_yield();
        reschedule();
        /*
         * Finally acquire write lock on our third test lock
         */
        rw_lock_get_write_lock(&rw_lock3);
        PRINT("Process %d / task %d: got write lock on rw_lock3\n", pm_get_pid(), pm_get_task_id());
        PRINT("Process %d / task %d: forking off process which will try to get a write lock\n", pm_get_pid(), pm_get_task_id());
        rc = __ctOS_fork();
        if (0==rc) {
            PRINT("Process %d / task %d: trying to get write lock on rw_lock3\n", pm_get_pid(), pm_get_task_id());
            rw_lock_get_write_lock(&rw_lock3);
            PRINT("Process %d / task %d: got write lock on rw_lock3\n", pm_get_pid(), pm_get_task_id());
            __ctOS_syscall(__SYSNO_QUIT, 0);
            PANIC("Should never get here\n");
        }
        /*
         * Wait for 10 ticks, then release write lock
         */
        sched_yield();
        reschedule();
        PRINT("Process %d: waiting for another 10 ticks...", pm_get_pid());
        for (i = 0; i < 10; i++) {
            sched_yield();
            reschedule();
        }
        PRINT("now releasing write lock on rw_lock3\n");
        rw_lock_release_write_lock(&rw_lock3);
        sched_yield();
        reschedule();
        PRINT("Process %d / task %d: killing timer task\n", pm_get_pid(), pm_get_task_id());
        timer_kill = 1;
#endif
#ifdef DO_TIMER_TEST
        PRINT("Waiting 100000 times for 10 timer ticks (8 us)\n");
        u32 timer_test;
        for (timer_test = 0; timer_test < 100000; timer_test++)
            timer_wait(10);
#endif
#ifdef DO_PATA_TEST
        pata_do_tests();
#endif
#ifdef DO_AHCI_TEST
        ahci_do_tests();
#endif
#ifdef DO_8139_TEST
        ip_test();
#endif
        rc = 1;
        KASSERT(rc);
    }
}

#ifdef DO_SMP_TEST
/*
 * SMP Test 1
 *
 * Test spinlocks on multiple CPUs. This test case consists of two threads, t0 and t1, which
 * are supposed to run on different CPUs. Initially, the CPUs synchronize execution as follows.
 *
 * t0 sets the flag t0_ready and waits until t1_ready is one
 * t1 sets the flag t1_ready and waits until t0_ready is one
 *
 * Then both threads enter a loop and within the loop increment a counter by one. When they
 * exit the loop, t1 sets a flag t1_done. t0 waits for this flag and then evaluates
 * the test results
 *
 */
static int smp_test1_t0_ready = 0;
static int smp_test1_t1_ready = 0;
static int smp_test1_t1_done = 0;
static int smp_test1_counter = 0;
static spinlock_t smp_test1_lock;

#define SMP_TEST1_ITERATIONS 8000000

static void do_smp_test1_t0 () {
    int i;
    int tmp;
    u32 eflags;
    if (params_get_int("do_test") == 0)
            return;
    cli();
    spinlock_init(&smp_test1_lock);
    kprintf("do_smp_test1_t0: waiting for thread 1\n");
    smp_test1_t0_ready = 1;
    smp_mb();
    while (0 == smp_test1_t1_ready);
    kprintf("do_smp_test1_t0: starting loop\n");
    for (i = 0; i < SMP_TEST1_ITERATIONS; i++) {
        spinlock_get(&smp_test1_lock, &eflags);
        /*
         * We make the loop deliberately slow
         * to make sure that without the lock, the loop is broken
         * and concurrent updates happen
         */
        tmp = smp_test1_counter;
        tmp++;
        smp_test1_counter = tmp;
        spinlock_release(&smp_test1_lock, &eflags);
    }
    while (0 == smp_test1_t1_done);
    sti();
    kprintf("smp_test1 done, counter is now %d, expected %d, difference is %d\n", smp_test1_counter, SMP_TEST1_ITERATIONS*2,
            SMP_TEST1_ITERATIONS*2 - smp_test1_counter );
    if (smp_test1_counter != SMP_TEST1_ITERATIONS*2) {
        PANIC("Difference between actual and expected result");
    }
}

void do_smp_test1_t1 () {
    int i;
    int tmp;
    u32 eflags;
    if (params_get_int("do_test") == 0)
            return;
    smp_test1_t1_ready = 1;
    smp_mb();
    while (0 == smp_test1_t0_ready);
    for (i = 0; i < SMP_TEST1_ITERATIONS; i++) {
        spinlock_get(&smp_test1_lock, &eflags);
        tmp = smp_test1_counter;
        tmp++;
        smp_test1_counter = tmp;
        spinlock_release(&smp_test1_lock, &eflags);
    }
    smp_test1_t1_done = 1;
}


static void* task_smp(void* arg) {
    while (1) {
        asm("sti ; hlt");
    }
    return 0;
}

static  void do_smp_test3() {
    u32 thread;
    pthread_attr_t attr;
    if (params_get_int("do_test") == 0)
            return;
    attr.cpuid = 1;
    __ctOS_syscall(1, 4, &thread, &attr, task_smp, 0);
}

#define SMP_TEST4_TICKS 100
/*
 * Determine correct clock settings
 */
static void do_smp_test4() {
    u32 time0;
    u32 time1;
    if (0 == params_get_int("do_test"))
        return;
    /*
     * Get time
     */
    time0 = do_time(0);
    PRINT("Starting tick measurement, time is now: %d\n", time0);
    /*
     * Wait for the specified number of local ticks
     */
    timer_wait_local_ticks(SMP_TEST4_TICKS);
    time1 = do_time(0);
    PRINT("%d local ticks done in %d seconds, i.e. one tick is %d ms\n", SMP_TEST4_TICKS, time1-time0, (time1-time0)*(1000/SMP_TEST4_TICKS));
    /*
     * Now do the same thing for global ticks
     */
    time0 = do_time(0);
    timer_wait_ticks(SMP_TEST4_TICKS);
    time1 = do_time(0);
    PRINT("%d global ticks done in %d seconds, i.e. one tick is %d ms\n", SMP_TEST4_TICKS, time1-time0, (time1-time0)*(1000/SMP_TEST4_TICKS));
}

#endif



/*
 * This is the main entry points for all tests which are run on
 * the AP after reaching protected mode
 */
void do_smp_tests_boot_ap() {
    if (params_get_int("do_test") == 0)
        return;
#ifdef DO_DELAY_TEST
        int delay_loop = 0;
        int ticks_at_start = timer_get_ticks();
        /*
         * We do this on the AP as we are not being interrupted or preempted
         * yet on the AP, but can still use the timer interrupt of the BSP
         * to measure elapsed time
         */
        for (delay_loop = 0; delay_loop < 1000000; delay_loop++) {
            outb(0x0, 0x80);
        }
        PRINT("Ticks passed for 1 Million writes to 0x80: %d\n", timer_get_ticks() - ticks_at_start);
#endif
#ifdef DO_SMP_TEST
    do_smp_test1_t1();
    PRINT("AP: doing tlb_test_t1\n");
    do_smp_tlb_test_t1();
    PRINT("AP: boot-time tests completed\n");
#endif
}


/*
 * This is the main entry points for all tests which are run on
 * the BSP after the AP has been brought up
 */
void do_smp_tests_boot_bsp() {
    if (params_get_int("do_test") == 0)
        return;
#ifdef DO_SMP_TEST
    do_smp_test1_t0();
    do_smp_tlb_test_t0();
#endif
}

/*
 * Tests run on AP before entering idle loop
 */
void do_pre_init_tests_ap() {
    if (params_get_int("do_test") == 0)
        return;
#ifdef DO_SMP_TEST
    PRINT("AP: doing pre_init_tests_ap()\n");
    do_smp_test3();
    do_smp_test2();
    do_smp_test4();
#endif
}
