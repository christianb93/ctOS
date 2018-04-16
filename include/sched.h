/*
 * sched.h
 */

#ifndef _SCHED_H_
#define _SCHED_H_

#include "lib/sys/types.h"
#include "locks.h"
#include "pit.h"

/*
 * This structure is the runnable structure which represents
 * a runnable task from the schedulers point of view
 */
typedef struct _runnable_t {
    u32 quantum;                   // CPU time left until task will be preempted
    int priority;                  // priority
    int reschedule;                // perform scheduling operation
    int valid;                     // is this a valid runnable?
    struct _runnable_t* next;      // next in ready queue
    struct _runnable_t* prev;      // previous in ready queue
} runnable_t;

/*
 * This structure describes a ready queue
 */
typedef struct {
    runnable_t* head;
    runnable_t* tail;
} sched_queue_t;

/*
 * Maximum value for priority
 */
#define SCHED_MAX_PRIO 15

/*
 * Initial quantum for each task (in ticks, i.e. 1/HZ seconds)
 * We set this to HZ / 10, so that we have 100 mseconds as initial CPU
 * time for each runnable
 */
#define SCHED_INIT_QUANTUM ((HZ / 10))

/*
 * IPI used to inform another CPU about a higher priority task
 * in its run queue
 */
#define SCHED_IPI 0x83


void sched_init();
void sched_add_idle_task(int task_id, int cpuid);
int sched_get_queue_length(int cpuid);
int sched_schedule();
void sched_enqueue(int task_id, int priority);
void sched_enqueue_cpu(int task_id, int priority, int cpuid);
void sched_dequeue();
void sched_do_tick();
void sched_yield();
void sched_print();
int sched_get_load(int cpuid);

#endif /* _SCHED_H_ */
