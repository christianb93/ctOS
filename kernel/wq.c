/*
 * wq.c
 *
 *  This module implements work queues. Work queues are a mechanism which can be used to process things later, for instance outside
 *  of an interrupt handler. An entry in a work queue basically specifies a handler function to be called with a specific argument.
 *  When the handler fails with return code EAGAIN, it is rescheduled. If it fails with a different return code not zero, it is discarded.
 *
 *  Work queues are processed by one decicated worker thread per CPU. These worker threads can be actively triggered, otherwise they
 *  will be triggered periodically by the timer module.
 *
 *  A handler function accepts two arguments:
 *  - a void* pointer to the actual argument
 *  - an integer argument which specifies whether the message has timed out. If this argument is set, the handler should free
 *    the argument and return as soon as possible
 */

#include "wq.h"
#include "smp.h"
#include "lib/string.h"
#include "lib/os/errors.h"
#include "lib/pthread.h"
#include "debug.h"
#include "timer.h"
#include "lib/os/syscalls.h"
#include "sched.h"

static int __wq_loglevel = 0;
#define WQ_DEBUG(...) do {if (__wq_loglevel > 0 ) { kprintf("DEBUG at %s@%d (%s): ", __FILE__, __LINE__, __FUNCTION__); \
        kprintf(__VA_ARGS__); }} while (0)

/*
 * Are we already fully initialized?
 */
static int initialized = 0;

/*
 * The work queues
 */
static wq_t work_queue[WQ_COUNT];

/*
 * Semaphores to control execution of worker threads
 */
static semaphore_t wq_mutex[SMP_MAX_CPU];

/*
 * Our own ticks
 */
static int ticks[SMP_MAX_CPU];

/****************************************************************************************
 * Utility functions to operate on work queues                                          *
 ***************************************************************************************/

/*
 * Add an entry to a work queue. If the work queue is full, the entry will be discarded
 * Parameter:
 * @wq - the work queue to which the element is added
 * @wq_entry - the entry to be added
 * Return value:
 * 0 - operation successful
 * -1 - queue full
 */
static int add_entry(wq_t* wq, wq_entry_t* entry) {
    u32 eflags;
    spinlock_get(&wq->queue_lock, &eflags);
    if (wq->tail - wq->head == WQ_MAX_ENTRIES) {
        /*
         * Queue full
         */
        spinlock_release(&wq->queue_lock, &eflags);
        ERROR("Work queue full!\n");
        return -1;
    }
    /*
     * Add entry to queue
     */
    wq->wq_entries[wq->tail % WQ_MAX_ENTRIES] = *entry;
    wq->tail++;
    WQ_DEBUG("Added entry to queue, tail = %d, head = %d\n", wq->tail, wq->head);
    spinlock_release(&wq->queue_lock, &eflags);
    return 0;
}


/*
 * Remove an entry from the head of the queue
 * Parameter:
 * @wq - the queue
 * @entry - will be filled with a copy of the element just removed
 * Return value:
 * 0 - successful
 * -1 - queue empty
 */
static int get_entry(wq_t* wq, wq_entry_t* entry) {
    u32 eflags;
    spinlock_get(&wq->queue_lock, &eflags);
    if (wq->tail == wq->head) {
        /*
         * Queue empty
         */
        spinlock_release(&wq->queue_lock, &eflags);
        return -1;
    }
    /*
     * Get entry from queue
     */
    *entry = wq->wq_entries[wq->head % WQ_MAX_ENTRIES];
    wq->head++;
    WQ_DEBUG("Removed entry from queue, tail = %d, head = %d\n", wq->tail, wq->head);
    spinlock_release(&wq->queue_lock, &eflags);
    return 0;
}

/****************************************************************************************
 * These are the functions invoked by other parts of the kernel to schedule entries     *
 * and to trigger processing of the queue                                               *
 ***************************************************************************************/

/*
 * Schedule an operation for later execution by adding it to a work queue
 * Parameter:
 * @wq_id - ID of work queue to be used
 * @handler - handler to be called
 * @arg - argument to be passed to the handler
 * @opt - WQ_RUN_NOW to process entry as soon as possible, WQ_RUN_LATER to wait for next regular iteration
 * Return value:
 * 0 - operation successful
 * -1 - queue full or ID invalid
 */
int wq_schedule(int wq_id, int (*handler)(void*, int), void* arg, int opt) {
    wq_entry_t entry;
    wq_t* wq;
    int cpuid;
    cpuid = wq_id % smp_get_cpu_count();
    /*
     * Avoid usage of queues which are not yet fully initialized
     */
    if (!initialized) {
        ERROR("Work queues not yet initialized\n");
        return -1;
    }
    /*
     * Validate work queue ID
     */
    if ((wq_id < 0) || (wq_id >= WQ_COUNT)) {
        ERROR("Invalid work queue ID %d\n", wq_id);
        return -1;
    }
    wq = work_queue + wq_id;
    /*
     * Prepare entry
     */
    entry.arg = arg;
    entry.handler = handler;
    entry.expires = timer_get_ticks(cpuid) + WQ_TIMEOUT;
    entry.iteration = 0;
    /*
     * Add entry to queue
     */
    if (-1 == add_entry(wq, &entry)) {
        return -1;
    }
    /*
     * Trigger processing if needed
     */
    if (WQ_RUN_NOW == opt) {
        WQ_DEBUG("Waking up worker thread on CPU %d\n", cpuid);
        mutex_up(&wq_mutex[cpuid]);
    }
    return 0;
}

/*
 * Trigger processing of a queue
 * Parameter:
 * @wq_id - the work queue id
 */
void wq_trigger(int wq_id) {
    int cpuid;
    /*
      * Validate work queue ID
      */
     if ((wq_id < 0) || (wq_id >= WQ_COUNT)) {
         ERROR("Invalid work queue ID %d\n", wq_id);
         return;
     }
     /*
      * Determine CPU on which queue is located
      */
     cpuid = wq_id % smp_get_cpu_count();
     /*
      * and do broadcast on condition variable
      */
     if (initialized) {
         WQ_DEBUG("Waking up worker thread on CPU %d\n", cpuid);
         mutex_up(&wq_mutex[cpuid]);
     }
}

/****************************************************************************************
 * The worker thread related functions and initialization                               *
 ***************************************************************************************/

/*
 * This is the main loop of the worker thread
 */
static void worker_thread(void* thread_arg) {
    int cpuid = smp_get_cpu();
    int wq_id;
    wq_t* wq;
    wq_entry_t entry;
    u32 local_ticks;
    while (1) {
        /*
         * Walk all queues associated with this CPU.
         */
        for (wq_id = 0; wq_id < WQ_COUNT; wq_id++) {
            if (cpuid == wq_id % smp_get_cpu_count()) {
                /*
                 * Update iteration count
                 */
                WQ_DEBUG("Processing queue %d on CPU%d\n", wq_id, smp_get_cpu());
                wq = work_queue + wq_id;
                wq->iteration++;
                /*
                 * Now process this queue until we either hit upon an entry which has already be visited in this
                 * iteration or the queue is empty
                 */
                while (1) {
                    /*
                     * Get next element from queue
                     */
                    if (-1 == get_entry(wq, &entry))  {
                        /*
                         * Queue empty, exit inner while loop
                         */
                        WQ_DEBUG("Queue empty\n");
                        break;
                    }
                    /*
                     * If we have seen this entry before requeue and exit loop
                     */
                    if (entry.iteration == wq->iteration) {
                        if (add_entry(wq, &entry)) {
                            ERROR("Queue %d is full\n", wq_id);
                        }
                        WQ_DEBUG("Completed iteration %d\n", wq->iteration);
                        break;
                    }
                    /*
                     * Got entry - try to submit it unless it has expired
                     */
                    local_ticks = timer_get_ticks(cpuid);
                    if (entry.expires > local_ticks) {
                        if (EAGAIN == entry.handler(entry.arg, 0)) {
                            WQ_DEBUG("Requeuing entry on queue %d\n", wq_id);
                            entry.iteration = wq->iteration;
                            if (add_entry(wq, &entry)) {
                                ERROR("Queue %d is full\n", wq_id);
                            }
                        }
                    }
                    else {
                        WQ_DEBUG("Entry has expired (expired at %d, local ticks is %d)\n", entry.expires, local_ticks);
                        entry.handler(entry.arg, 1);
                    }
                } // while(1)
            }
        }
        /*
         * Done with all queues, test semaphore
         */
        WQ_DEBUG("Done with all queues, testing semaphore\n");
        sem_down(&wq_mutex[cpuid]);
    }
    PANIC("Should never get here\n");
}

/*
 * Initialize all work queues
 */
void wq_init() {
    int i;
    u32 thread;
    pthread_attr_t attr;
    /*
     * Set up work queues
     */
    for (i = 0; i < WQ_COUNT; i++) {
        work_queue[i].head = 0;
        work_queue[i].tail = 0;
        work_queue[i].iteration = 0;
        spinlock_init(&work_queue[i].queue_lock);
        work_queue[i].wq_id = i;
    }
    /*
     * and initialize condition variables
     */
    for (i = 0; i< SMP_MAX_CPU; i++)
        sem_init(&wq_mutex[i], 0);
    /*
     * Bring up worker threads
     */
    for (i = 0; i < smp_get_cpu_count(); i++) {
        attr.cpuid = i;
        attr.priority = SCHED_MAX_PRIO;
        if (__ctOS_syscall(__SYSNO_PTHREAD_CREATE, 4, &thread, &attr, &worker_thread, 0))
             ERROR("Error while launching worker thread for CPU %d\n", i);
    }
    initialized = 1;
}


/*
 * This function is called periodically by the programm manager main module pm.c on each CPU
 * Parameter:
 * @cpuid - the current cpu
 */
void wq_do_tick(int cpuid) {
    ticks[cpuid]++;
    if (0 == (ticks[cpuid] % WQ_TICKS))
        mutex_up(&wq_mutex[cpuid]);
}
