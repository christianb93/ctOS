/*
 * timer.c
 *
 * This module contains timer services which can be used by other parts of the kernel and offers a common interface independent of the
 * actual physical timer used. At the moment, the module itself is still hardware dependent, in future releases, it is planned to add
 * an additional abstraction layer between this module and the actual hardware specific code. Each timer would then register itself
 * and its capabilities with this module at boot time, and the timer module could then decide which hardware timer is used for which
 * purpose (getting wall clock times, providing delay loops, sleeping, ...)
 *
 * At boot time, this module registers itself with the interrupt manager as interrupt handler for the interrupt vector returned
 * by irq_get_vector_timer. It assumes that the bootstrap code for the APs sets up the local APIC as periodic interrupt source. The
 * PIT is set up as periodic interrupt source for the BSP by the timer module itself.
 *
 * To measure time, essentially three different methods are used. Method one is used to determine the absolute time with respect
 * to a certain timezone (UTC is currently the only supported timezone in the kernel) which is also referred to as wall clock time.
 * For this purpose, ctOS uses the real-time clock (RTC) which is present in every x86 system and from which we can read the current
 * time in years, month, days, hours, minutes and seconds directly.
 *
 * To measure periods shorter than one second, "ticks" are used, i.e. counter driven by a periodic interrupt source.
 * For each CPU, ticks is a counter which measures the number of timer interrupts received since boot time. For the BSP, timer
 * interrupts are received from the programmable interrupt timer (PIT). The ticks measured by the BSP are also referred as
 * "global ticks". For the APs, the local APIC is initialized at boot time to deliver interrupts with approximately the same
 * frequency as the PIT (this is done in apic.c). To wait for a defined number of ticks, this module offers the following functions
 *
 * timer_wait_ticks          -  wait for a specified number of global ticks, i.e. ticks of the BSP
 * timer_wait_local_ticks    -  wait for a specified number of local ticks, i.e. ticks of the current CPU
 *
 *
 * For even shorter periods, the counter register of the PIT is read directly to be able to realize delays which are shorter than
 * the interval between two ticks. The two functions udelay and mdelay can be used to wait for a specified number of microseconds
 * and milliseconds respectively. Note however that these functions are both not really accurate. Future versions of ctOS will use
 * the TSC if available to offer more precise delays.
 *
 * Some operating systems used a write to port 0x80 which is used by the BIOS to send information during the power-on self test
 * (POST) to a remote machine. On an ISA bus, a write to 0x80 took approximately 1 microsecond and could therefore be used to
 * measure the time. However, this is no longer reliable on modern PCs using an LPC (for instance, on my PC - X58 chipset and Core I7 CPU- ,
 * a write to port 0x80 takes only about 100 ns) and there also seem to be some BIOSe around on which writing to this port hangs the
 * machine, so we do not use this method.
 *
 * The timer module is also the "owner" of the timer interrupt and calls the functions of the process manager and scheduler which
 * depend on being called periodically. It also calls the TCP tick processing periodically.
 *
 * Finally, this module manages a list of wakeup timers which can be set by other parts of the kernel to be woken up at a specified
 * time in the future. There are three different types of wakeup timers:
 *
 * 1) an event control block can have a timeout, i.e. a thread waiting for an event will wake up when the timer expires - this
 *    is handled by time_time_ecb and timer_cancel_ecb
 * 2) A sleep timer is an entry in the timer list with timer->type == 1. When a sleep timer expires, the associated task will
 *    be woken up
 * 3) An alarm timer is an entry in the timer list with timer->type == 2. When an alarm timer expires, the associated process will
 *    receive the signal SIGALRM
 */

#include "irq.h"
#include "pit.h"
#include "rtc.h"
#include "timer.h"
#include "debug.h"
#include "smp.h"
#include "sched.h"
#include "pm.h"
#include "util.h"
#include "lib/sys/time.h"
#include "lib/limits.h"
#include "lists.h"
#include "mm.h"
#include "locks.h"
#include "keyboard.h"
#include "vga.h"
#include "sysmon.h"
#include "tcp.h"
#include "ip.h"
#include "lib/stddef.h"

/*
 * Number of times the timer interrupt has been invoked per CPU
 */
static u32 ticks[SMP_MAX_CPU];

/*
 * A linked list of timers and a spinlock to protect it
 */
static pm_timer_t* timer_list_head = 0;
static pm_timer_t* timer_list_tail = 0;
static spinlock_t timer_list_lock;

/*
 * Timed event control blocks
 */
static struct __ecb_timer_t* timed_ecb_queue_head[SMP_MAX_CPU];
static struct __ecb_timer_t* timed_ecb_queue_tail[SMP_MAX_CPU];
static spinlock_t timed_ecb_queue_lock[SMP_MAX_CPU];


/*
 * This is the interrupt vector used for the timer. This value is set when the timer
 * module is initialized by the BSP and required for the setup of the APs
 */
static int timer_irq_vector = 0;

/****************************************************************************************
 * Initialization and interrupt handler                                                 *
 ***************************************************************************************/

/*
 * Interrupt handler. This is the interrupt handler for the periodic timer interrupt which
 * is connected to the PIT on the BSP and the local APIC on the APs.
 * Parameter:
 * @ir_context - interrupt context
 * Return value:
 * always 0
 */
static int timer_isr(ir_context_t* ir_context) {
    time_t current_time;
    pm_timer_t* timer;
    pm_timer_t* next;
    u32 eflags;
    struct __ecb_timer_t* ecb_timer;
    u32 cpuid = smp_get_cpu();
    /*
     * Process ticks for process manager and scheduler
     */
    pm_do_tick(ir_context, cpuid);
    sched_do_tick();
    /*
     * Increment ticks on current CPU
     */
    atomic_incr(&ticks[cpuid]);
    /*
     * If we are on the BSP, check for expired timers and update cursor state
     * Also call TCP timer if required
     */
    if (0 == cpuid) {
        if (0 == (ticks[0] % (HZ / 2))) {
            current_time = do_time(0);
            cons_cursor_tick();
            spinlock_get(&timer_list_lock, &eflags);
            timer = timer_list_head;
            while (timer) {
                next = timer->next;
                if (timer->time <= current_time) {
                    /*
                     * If this is a sleep timer, wake up task
                     */
                    if (TIMER_TYPE_SLEEP == timer->type) {
                        mutex_up(&timer->mutex);
                    }
                    /*
                     * otherwise send SIGALRM and remove entry
                     */
                    else {
                        do_kill(timer->owner, __KSIGALRM);
                        LIST_REMOVE(timer_list_head, timer_list_tail, timer);
                        kfree((void*) timer);
                    }
                }
                timer = next;
            }
            spinlock_release(&timer_list_lock, &eflags);
        }
        if (0 == ticks[0] % (HZ / TCP_HZ)) {
            tcp_do_tick();
        }
        if (0 == ticks[0] % HZ) {
            ip_do_tick();
        }
    }
    /*
     * Check if there are any expired timed event control blocks
     * on our queue
     */
    if (0 == (ticks[cpuid] % SEM_CHECK)) {
        spinlock_get(&timed_ecb_queue_lock[cpuid], &eflags);
        LIST_FOREACH(timed_ecb_queue_head[cpuid], ecb_timer) {
            if (ecb_timer->is_active) {
                if (ecb_timer->timeout_value >= SEM_CHECK) {
                    ecb_timer->timeout_value -= SEM_CHECK;
                }
                else {
                    ecb_timer->timeout_value = 0;
                }
                if (0 == ecb_timer->timeout_value) {
                    ecb_timer->timeout = 1;
                    /*
                     * Wakeup sleeping task
                     */
                    wakeup_task(TIMER2ECB(ecb_timer));
                }
            }
        }
        spinlock_release(&timed_ecb_queue_lock[cpuid], &eflags);

    }
    return 0;
}

/*
 * Initialize the timer and register its interrupt service handler with the interrupt manager
 */
void timer_init() {
    /*
     * Set up interrupt handler
     */
    timer_irq_vector = irq_add_handler_isa(timer_isr, 1, TIMER_IRQ, 1);
    /*
     * Set up PIT and RTC
     */
    pit_init();
    rtc_init();
    /*
     * Initialize timer list
     */
    timer_list_head = 0;
    timer_list_tail = 0;
    spinlock_init(&timer_list_lock);
    /*
     * Inform keyboard driver that it can do an idle wait in debugging mode
     */
    keyboard_enable_idle_wait();
}

/*
 * Perform initialization on the AP
 */
void timer_init_ap() {
    int cpu = smp_get_cpu();
    apic_init_timer(timer_irq_vector);
    timed_ecb_queue_head[cpu] = 0;
    timed_ecb_queue_tail[cpu] = 0;
    spinlock_init(&timed_ecb_queue_lock[cpu]);
}

/****************************************************************************************
 * Implementation of the sleep and alarm system calls                                   *
 ***************************************************************************************/

/*
 * Put a task to sleep for the specified number of seconds
 * Parameter:
 * @seconds - number of seconds to sleep
 * Return value:
 * 0 if operation was successful
 * number of seconds left if an error occurred
 * Locks:
 * lock timer_list_lock on list of timers
 */
int do_sleep(time_t seconds) {
    u32 eflags;
    pm_timer_t* timer;
    int rc;
    /*
     * Allocate memory for timer. We need to do this in the kernel
     * heap, as we might want to access it from a different process later on
     */
    if (0 == (timer = (pm_timer_t*) kmalloc(sizeof(pm_timer_t)))) {
        ERROR("Could not get memory for timer, returning immediately\n");
        return seconds;
    }
    timer->time = do_time(0) + seconds;
    timer->type = TIMER_TYPE_SLEEP;
    sem_init(&timer->mutex, 0);
    timer->owner = pm_get_task_id();
    spinlock_get(&timer_list_lock, &eflags);
    LIST_ADD_END(timer_list_head, timer_list_tail, timer);
    spinlock_release(&timer_list_lock, &eflags);
    /*
     * Now sleep on the semaphore until we are woken up by the
     * mutex_up call in timer_isr
     */
    rc = sem_down_intr(&timer->mutex);
    /*
     * Finally clean up timer again
     */
    spinlock_get(&timer_list_lock, &eflags);
    LIST_REMOVE(timer_list_head, timer_list_tail, timer);
    spinlock_release(&timer_list_lock, &eflags);
    if (0 == rc) {
        kfree(timer);
        return 0;
    }
    rc = timer->time - do_time(0);
    if (rc < 0)
        rc = 0;
    kfree((void*) timer);
    return rc;
}

/*
 * Set the alarm for the current process. If there is already a pending alarm, cancel it.
 * If the parameter is zero, the alarm is canceled
 * Parameter:
 * @seconds - number of seconds to sleep
 * Return value:
 * 0 if operation was successful
 * number of seconds left if there is already a pending alarm for the process
 * Locks:
 * lock timer_list_lock on list of timers
 */
int do_alarm(time_t seconds) {
    u32 eflags;
    pm_timer_t* timer;
    pm_timer_t* next;
    unsigned int current_time;
    int pid;
    int rc = 0;
    pid = pm_get_pid();
    /*
     * Get lock on timer list
     */
    spinlock_get(&timer_list_lock, &eflags);
    /*
     * Read current time
     */
    current_time = do_time(0);
    /*
     * Now walk list to see if we have an entry for the current process
     */
    timer = timer_list_head;
    while(timer) {
        next = timer->next;
        if ((pid == timer->owner) && (TIMER_TYPE_ALARM == timer->type)) {
            /*
             * There is already an entry for this process. Update it and return
             * number of seconds left. Only do update if seconds is not 0, otherwise
             * cancel timer
             */
            if (timer->time > current_time)
                rc = timer->time - current_time;
            else
                rc = 0;
            if (seconds > 0) {
                timer->time = current_time + seconds;
            }
            else {
                LIST_REMOVE(timer_list_head, timer_list_tail, timer);
                kfree ((void*) timer);
            }
            spinlock_release(&timer_list_lock, &eflags);
            return rc;
        }
        timer = next;
    }
    /*
     * If seconds is 0, we have been asked to cancel an alarm. As we get to this point,
     * there is no alarm - return
     */
    if (0 == seconds) {
        spinlock_release(&timer_list_lock, &eflags);
        return 0;
    }
    /*
     * Allocate memory for timer. We need to do this in the kernel
     * heap, as we might want to access it from a different process later on
     */
    if (0 == (timer = (pm_timer_t*) kmalloc(sizeof(pm_timer_t)))) {
        ERROR("Could not get memory for timer, returning immediately\n");
        return seconds;
    }
    timer->time = current_time + seconds;
    timer->type = TIMER_TYPE_ALARM;
    sem_init(&timer->mutex, 0);
    timer->owner = pid;
    LIST_ADD_END(timer_list_head, timer_list_tail, timer);
    /*
     * Release lock and return
     */
    spinlock_release(&timer_list_lock, &eflags);
    return rc;
}

/****************************************************************************************
 * The following functions can be used by other parts of the kernel to retrieve ticks,  *
 * wait for a specified number of ticks or shorter periods and read the wall clock time *
 ***************************************************************************************/

/*
 * Get the number of global ticks passed since startup
 */
u32 timer_get_ticks() {
    return atomic_load(&ticks[SMP_BSP_ID]);
}

/*
 * Wait for a given number of global ticks. If interrupts are disabled
 * on the current CPU, we do a busy wait, otherwise we do an idle wait
 * Parameter:
 * @_ticks - number of ticks to wait (note that one second has HZ ticks)
 */
void timer_wait_ticks(u32 _ticks) {
    int wait_idle = 0;
    u32 current_ticks;
    u32 eflags = get_eflags();
    if (IRQ_ENABLED(eflags))
        wait_idle = 1;
    current_ticks = ticks[SMP_BSP_ID];
    while (ticks[SMP_BSP_ID] < current_ticks + _ticks) {
            if (wait_idle)
                asm("hlt");
            else
                asm("pause");
    }
}

/*
 * Wait for a specified number of ticks, but use the local
 * timer instead of the global timer. This function will panic
 * if interrupts are disabled on the local CPU
 * Parameter:
 * @_ticks - ticks to wait
 */
void timer_wait_local_ticks(u32 _ticks) {
    u32 eflags = get_eflags();
    int cpuid = smp_get_cpu();
    KASSERT(1 == IRQ_ENABLED(eflags));
    u32 current_ticks = ticks[cpuid];
    while (ticks[cpuid] < current_ticks + _ticks) {
        asm("hlt");
    }
}

/*
 * Get Unix time, i.e. number of seconds passed since 1.1.1970
 * Parameters:
 * @time - used to return time
 * Return value:
 * The current time if operation was successful or ((time_t)-1) if
 * an error occurred
 */
time_t do_time(time_t* time) {
    return rtc_do_time(time);
}

/*
 * Get time of day, i.e. seconds and microseconds within the current second
 */
int do_gettimeofday(u32* seconds, u32* useconds) {
    /*
     * First get second from RTC
     */
    *seconds = do_time(0);
    /*
     * Now get microseconds. Currently we use the ticks on CPU 0 which is only a rough approximation.
     * As the ticks field increases HZ times each second, we get an approximation for the microseconds within the
     * second by taking the ticks % HZ times the number of microseconds per ticks
     */
    *useconds = *seconds*1000 + (ticks[0] % HZ) * (1000000 / HZ);
    return 0;
}

/*
 * Define __ctOS_time so that we can link time.o from the standard library
 * into the kernel
 */
time_t __ctOS_time(time_t* tloc) {
    return do_time(tloc);
}

/*
 * Common utility functions for udelay and mdelay. Wait for N micro / milliseconds
 * Parameter:
 * @n - number of units to wait
 * @units - 1000 for milliseconds, 10000000 for microseconds
 */
static void delay(u32 n, u32 units) {
    if (n > UINT_MAX / PIT_TIMER_FREQ)
        PANIC("delay called with invalid parameter %x, units = %d\n", n, units);
    /*
     * The PIT decrements its counter PIT_TIMER_FREQ times per second, i.e. if
     * we wanted to wait once second, we would have to wait for PIT_TIMER_FREQ
     * ticks
     */
    u32 ticks = (n * 2000000) / units;
    /*
     * As the PIT counter is 16 bit, we can wait at most 65535 ticks
     */
    if (ticks > 65535)
        PANIC("delay called with invalid parameter %x, units = %d\n", n, units);
    pit_short_delay(ticks);
}

/*
 * Wait for N microseconds.
 * Parameter:
 * @us - number of microseconds to wait
 *
 * As it is unsafe on modern CPUs  to use a loop due to pipelining, we use the PIT for that purpose.
 * Note that in practice, this will probably take longer than one microsecond when N = 1 on older machines
 * due to ISA bus latency - on a real ISA bus, one read takes about 1 us
 */
void udelay(u32 us) {
    delay(us, 1000000);
}

/*
 * Wait for the specified number of milliseconds. Consider using timer_wait_ticks for waits longer
 * than 10 ms
 * Parameter:
 * @ms - number of milliseconds to wait
 */
void mdelay(u32 ms) {
    delay(ms, 1000);
}

/*
 * Given a timeval structure, convert its value into ticks or return the maximum in case
 * of an overflow
 */
unsigned int timer_convert_timeval(struct timeval* time) {
    unsigned int ticks;
    unsigned int ticks_usec;
    /*
      * First compute contribution of tv_sev field
      */
     if (time->tv_sec > (UINT_MAX / HZ)) {
         ticks = UINT_MAX;
     }
     else {
         ticks = time->tv_sec * HZ;
     }
     /*
      * then add contribution of tv_usec field
      */
     ticks_usec = time->tv_usec / (1000000 / HZ);
     if (ticks_usec >  ~ticks) {
         ticks = UINT_MAX / HZ;
     }
     else {
         ticks += ticks_usec;
     }
     return ticks;
}

/****************************************************************************************
 * Timing services for semaphores and condition variables                               *
 ***************************************************************************************/


/*
 * Add a timer for an event control block (ECB). When timeout ticks have passed, a wakeup operation will be
 * performed on the ECB, and the ECBs timeout flag will be set
 * Parameter:
 * @ecb - the ECB
 * @timeout - timeout in ticks
 * Locks:
 * lock on timed ECB queue
 */
void timer_time_ecb(ecb_t* ecb, u32 timeout) {
    u32 eflags;
    int cpu = smp_get_cpu();
    /*
     * Get lock on queue of timed ECBs for this CPU
     */
    spinlock_get(&timed_ecb_queue_lock[cpu], &eflags);
    /*
     * Add ECB to list and place CPU on which the ECB is queued
     * in the semaphore itself for later reference
     */
    ecb->timer.is_active = 1;
    ecb->timer.timeout = 0;
    ecb->timer.timeout_value = timeout;
    ecb->timer.cpuid = cpu;
    LIST_ADD_END(timed_ecb_queue_head[cpu], timed_ecb_queue_tail[cpu], ECB2TIMER(ecb));
    /*
     * Release lock again
     */
    spinlock_release(&timed_ecb_queue_lock[cpu], &eflags);
}

/*
 * Cancel a timer for an ECB variable
 */
void timer_cancel_ecb(ecb_t* ecb) {
    u32 eflags;
    /*
     * Get CPU on which the ECB has been timed and validate
     */
    int cpu = ecb->timer.cpuid;
    if ((cpu < 0) || (cpu >= SMP_MAX_CPU)) {
        ERROR("Invalid cpu %d stored in event control block %x\n", cpu, ecb);
        return;
    }
    /*
     * Get lock on queue of timed event control blocks for this CPU
     */
    spinlock_get(&timed_ecb_queue_lock[cpu], &eflags);
    /*
     * Remove ECB from list
     */
    LIST_REMOVE(timed_ecb_queue_head[cpu], timed_ecb_queue_tail[cpu], ECB2TIMER(ecb));
    /*
     * Release lock again
     */
    spinlock_release(&timed_ecb_queue_lock[cpu], &eflags);
}

/****************************************************************************************
 * Everything below this line is for debugging purposes only                            *
 ***************************************************************************************/

/*
 * Print all timers
 */
void timer_print_timers() {
    pm_timer_t* timer;
    PRINT("Owner  Mutex  Expiration time\n");
    PRINT("-------------------------\n");
    LIST_FOREACH(timer_list_head, timer) {
        PRINT("%w   %x:%d   %d\n", timer->owner, &(timer->mutex), timer->mutex.value, timer->time);
    }
    PRINT("Current time: %d\n", do_time(0));
}



/*
 * Print ticks per CPU
 */
void timer_print_cpu_ticks() {
    int cpu;
    PRINT("CPU     Ticks\n");
    PRINT("----------------\n");
    for (cpu = 0; cpu < SMP_MAX_CPU; cpu++) {
        PRINT("%x   %d\n", cpu, ticks[cpu]);
    }
}
