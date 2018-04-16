/*
 * timer.h
 *
 */

#ifndef _TIMER_H_
#define _TIMER_H_

#include "locks.h"
#include "lib/sys/time.h"

/*
 * A timer
 */
typedef struct _pm_timer_t {
    time_t time;          // expiration time
    semaphore_t mutex;    // mutex used to wake up a process when the timer has expired
    int owner;            // id of the owner of the timer (task_id)
    int type;             // type of the timer
    struct _pm_timer_t* next;
    struct _pm_timer_t* prev;
} pm_timer_t;

/*
 * Timer types
 */
#define TIMER_TYPE_SLEEP 1
#define TIMER_TYPE_ALARM 2

/*
 * The number of local and global ticks per second. Note that this constant defines the heartbeat of the entire
 * operating system. It used to be 20 in earlier versions and has recently been adapted to 100, i.e. we process
 * one timer interrupt every 10 ms
 */
#define HZ 100

/*
 * Number of TCP ticks per second
 */
#define TCP_HZ 4

/*
 * Number of ticks which we let pass before we check timed semaphore. Setting this to 1 gives maximum precision,
 * but - if a lot of semaphores are timed - might slow down processing of timer ticks
 */
#define SEM_CHECK 10

/*
 * The legacy ISA timer interrupt
 */
#define TIMER_IRQ 0x0

void timer_init();
void timer_init_ap();
u32 timer_get_ticks();
void timer_print_timers();
void timer_print_cpu_ticks();
void timer_wait_ticks(u32 _ticks);
void timer_wait_local_ticks(u32 ticks);
time_t do_time(time_t* time);
void udelay(u32);
void mdelay(u32);
int do_gettimeofday(u32*, u32*);
void timer_time_ecb(ecb_t* ecb, u32 timeout);
void timer_cancel_ecb(ecb_t* ecb);
int do_alarm(time_t seconds);
int do_sleep(time_t seconds);
unsigned int timer_convert_timeval(struct timeval* time);

#endif /* _TIMER_H_ */
