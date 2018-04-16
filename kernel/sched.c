/*
 * sched.c
 *
 * This module contains the scheduler code. The scheduler is responsible for determining the next task to run. It
 * does not perform the task switch - this is done by the process manager
 */

#include "pm.h"
#include "sched.h"
#include "locks.h"
#include "lists.h"
#include "debug.h"
#include "smp.h"
#include "util.h"
#include "timer.h"
#include "cpu.h"
#include "params.h"

/*
 * This table holds the runnables, one for each task
 * It is protected by the lock queue_lock declared below.
 * There is one copy of this table per CPU
 */
static runnable_t runnable[SMP_MAX_CPU][PM_MAX_TASK];

/*
 * This table holds the ready queues
 * Note that the lock protecting the ready queues is
 * also used to synchronize access to the runnable table
 * to keep those data structures in sync
 */
static sched_queue_t queue[SMP_MAX_CPU][SCHED_MAX_PRIO + 2];
static spinlock_t queue_lock[SMP_MAX_CPU];

/*
 * This always points to the currently active runnable
 * It is also protected by the lock queue_lock
 */
static runnable_t* active[SMP_MAX_CPU];

/*
 * Keep track of used CPUs and their queue length
 */
static int cpu_used[SMP_MAX_CPU];
static u32 cpu_queue_length[SMP_MAX_CPU];

/*
 * Some statistical data
 *
 * idle_task - task ID of idle task, needed for some computations
 * idle - number of time slices during which the CPU has been idle
 * busy - number of time slices during which the CPU has been busy
 * idle_last, busy_last - updated every second with the then current value
 */
static u32 idle_task[SMP_MAX_CPU];
static u32 idle[SMP_MAX_CPU];
static u32 busy[SMP_MAX_CPU];
static u32 idle_last[SMP_MAX_CPU];
static u32 busy_last[SMP_MAX_CPU];
static int load[SMP_MAX_CPU];


/****************************************************************************************
 * The following functions are used for initialization. Whereas sched_init() is being   *
 * called by the BSP at boot time, sched_add_idle_task() is used by an AP to set up     *
 * the scheduler for the AP as well and is called once per AP                           *
 ***************************************************************************************/

/*
 * Initialize scheduler and set up array of runnables and queue for
 * first task
 */
void sched_init() {
    int i;
    int cpu;
    for (cpu = 0; cpu < SMP_MAX_CPU; cpu++) {
        active[cpu] = 0;
        cpu_used[cpu] = 0;
        cpu_queue_length[cpu]=0;
        for (i = 0; i <= SCHED_MAX_PRIO; i++) {
            queue[cpu][i].head = 0;
            queue[cpu][i].tail = 0;
        }
        spinlock_init(&queue_lock[cpu]);
        for (i = 1; i < PM_MAX_TASK; i++) {
            runnable[cpu][i].valid = 0;
        }
    }
    /*
     * Set up task 0 on BSP as active task
     */
    cpu_used[SMP_BSP_ID] = 1;
    active[SMP_BSP_ID] = runnable[SMP_BSP_ID];
    runnable[SMP_BSP_ID][0].valid = 1;
    runnable[SMP_BSP_ID][0].priority = 0;
    runnable[SMP_BSP_ID][0].reschedule = 0;
    runnable[SMP_BSP_ID][0].quantum = SCHED_INIT_QUANTUM;
    /*
     * Task 0 is the idle task for the BSP
     */
    idle_task[SMP_BSP_ID]=0;
}

/*
 * Add an idle task to the scheduler for a specific CPU. This function should only
 * be called once per CPU at boot time. Note that the idle task for the BSP is added
 * by sched_init already. Calling this function marks the CPU as active as far as the
 * scheduler is concerned
 * Parameter:
 * @task_id - the task to be used as idle task
 * @cpuid - the cpu
 */
void sched_add_idle_task(int task_id, int cpuid) {
    u32 eflags;
    if ((task_id < 0)  || (task_id >= PM_MAX_TASK)) {
        ERROR("Invalid task id %x\n", task_id);
        return;
    }
    if ((cpuid < 0) || (cpuid >= SMP_MAX_CPU)) {
        ERROR("Invalid cpuid %d\n", cpuid);
        return;
    }
    spinlock_get(&queue_lock[cpuid], &eflags);
    if (active[cpuid]) {
        ERROR("There is already an active entry for CPU %d\n", cpuid);
    }
    else {
        cpu_used[cpuid] = 1;
        runnable[cpuid][task_id].valid = 1;
        runnable[cpuid][task_id].quantum = SCHED_INIT_QUANTUM;
        runnable[cpuid][task_id].reschedule = 0;
        runnable[cpuid][task_id].priority = 0;
        active[cpuid]=runnable[cpuid] + task_id;
    }
    spinlock_release(&queue_lock[cpuid], &eflags);
    idle_task[cpuid] = task_id;
}

/****************************************************************************************
 * The following functions are used by other parts of the kernel to add runnables to    *
 * the ready queues or remove them from the queue and to get statistical information    *
 ***************************************************************************************/

/*
 * This function returns the CPU on which a new runnable is placed.
 * We walk the list of CPUs and look for the CPU with the lowest
 * queue length. No locks are used to make this fast - thus it might
 * be that the information becomes outdated as we go. However, as the queue
 * length is only a rough approximation to the actual load anyway, this is not
 * a real problem
 */
static int get_next_cpu() {
    int cpu;
    int rc = SMP_BSP_ID;
    u32 queue_length = atomic_load(&cpu_queue_length[SMP_BSP_ID]);
    for (cpu = 1; cpu < SMP_MAX_CPU; cpu++) {
        if (cpu_used[cpu]) {
            if (cpu_queue_length[cpu] < queue_length) {
                queue_length = atomic_load(&cpu_queue_length[cpu]);
                rc = cpu;
            }
        }
    }
    return rc;
}

/*
 * Add a new task to the ready queues for a specific CPU.
 * Parameter:
 * @task_id -  the ID of the new runnable / task
 * @priority -  the priority with which we add the new runnable
 * @cpu - the target CPU (0 = BSP)
 * Locks:
 * queue_lock
 */
void sched_enqueue_cpu(int task_id, int priority, int cpuid) {
    u32 flags;
    if ((task_id < 0)  || (task_id >= PM_MAX_TASK)) {
        ERROR("Invalid task id %x\n", task_id);
        return;
    }
    if (priority > SCHED_MAX_PRIO) {
        ERROR("Invalid priority %x\n", priority);
        return;
    }
    if ((cpuid < 0) || (cpuid >= SMP_MAX_CPU)) {
        ERROR("Invalid cpuid %d\n", cpuid);
        return;
    }
    if (0 == cpu_used[cpuid]) {
        ERROR("Invalid cpuid %d\n", cpuid);
        return;
    }
    /*
     * Get lock on queue for the specified cpu
     */
    spinlock_get(&(queue_lock[cpuid]), &flags);
    /*
     * Mark runnable as valid and add entry to queue
     */
    runnable[cpuid][task_id].valid = 1;
    runnable[cpuid][task_id].quantum = SCHED_INIT_QUANTUM;
    runnable[cpuid][task_id].reschedule = 0;
    runnable[cpuid][task_id].priority = priority;
    if (priority > active[cpuid]->priority) {
        active[cpuid]->reschedule = 1;
        /*
         * If the CPU is not the current CPU, send special scheduler IPI 0x83 to the other CPU
         * so that the other CPU has a chance to switch to the higher priority task immediately
         * without having to wait for the timer interrupt
         */
        if (cpuid != smp_get_cpu()) {
            if (params_get_int("sched_ipi"))
                apic_send_ipi(cpu_get_apic_id(cpuid), 0, SCHED_IPI, 0);
        }
    }
    LIST_ADD_END(queue[cpuid][priority].head, queue[cpuid][priority].tail, runnable[cpuid] + task_id);
    /*
     * Increase queue length
     */
    cpu_queue_length[cpuid]++;
    spinlock_release(&queue_lock[cpuid], &flags);

}

/*
 * Add a new task to the ready queues. The CPU to be used will be selected
 * according to shortest processor queue length
 * Parameter:
 * @task_id: the ID of the new runnable / task
 * @priority: the priority with which we add the new runnable
 */
void sched_enqueue(int task_id, int priority) {
    int cpuid;
    /*
     * Get CPU on which we place the new runnable
     */
    cpuid = get_next_cpu();
    /*
     * and call delegate
     */
    sched_enqueue_cpu(task_id, priority, cpuid);
}


/*
 * Remove the currently active runnable for the current CPU from the queues. This function assumes that
 * only runnables on the same CPU are removed and that interrupts are disabled
 * Locks:
 * queue_lock
 */
void sched_dequeue() {
    u32 flags;
    int cpuid;
    /*
     * Make sure that we are not preempted and will therefore
     * continue to execute on the same CPU until this function completes
     */
    KASSERT(0 == IRQ_ENABLED(get_eflags()));
    /*
     * Get CPU on which we execute
     */
    cpuid = smp_get_cpu();
    spinlock_get(&queue_lock[cpuid], &flags);
    if (0 == active[cpuid]) {
        spinlock_release(&queue_lock[cpuid], &flags);
        PANIC("No active runnable on queues for cpu %d\n", cpuid);
        return;
    }
    /*
     * Do not allow us to remove the idle task of the CPU from the ready
     * queue
     */
    if (idle_task[cpuid] == (active[cpuid] - runnable[cpuid])) {
        spinlock_release(&queue_lock[cpuid], &flags);
        ERROR("Cannot remove idle task from ready queue\n");
        return;
    }
    active[cpuid]->valid = 0;
    active[cpuid] = 0;
    spinlock_release(&queue_lock[cpuid], &flags);
}

/*
 * Yield control to scheduler voluntarily
 */
void sched_yield() {
    int cpuid = 0;
    u32 flags;
    cpuid = smp_get_cpu();
    /*
     * Get lock on queue
     */
    spinlock_get(&queue_lock[cpuid], &flags);
    if (active[cpuid])
        active[cpuid]->reschedule = 1;
    spinlock_release(&queue_lock[cpuid], &flags);
}

/*
 * Get the processor queue length for a specific CPU. Note that
 * the queue length is the number of runnables waiting in the queue,
 * not including the currently active entry
 */
int sched_get_queue_length(int cpuid) {
    u32 flags;
    int rc;
    if ((cpuid < 0) || (cpuid >= SMP_MAX_CPU)) {
        ERROR("Invalid cpuid %d\n", cpuid);
        return 0;
    }
    if (0 == cpu_used[cpuid]) {
        ERROR("Invalid cpuid %d\n", cpuid);
        return 0;
    }
    spinlock_get(&queue_lock[cpuid], &flags);
    rc = cpu_queue_length[cpuid];
    spinlock_release(&queue_lock[cpuid], &flags);
    return rc;
}

/****************************************************************************************
 * The following functions are the main entry points for the kernels interrupt handler  *
 * and perform the actual scheduling operations                                         *
 ***************************************************************************************/

/*
 * Perform the actual scheduling operation, i.e. select a new task to be run
 * Return value:
 * the id of the next runnable to be executed or -1 if no valid runnable could
 * be determined
 * Locks:
 * queue_lock
 */
int sched_schedule() {
    int i;
    int rc;
    int cpuid;
    u32 flags;
    /*
     * Make sure that we are not preempted and will therefore
     * continue to execute on the same CPU until this function completes
     */
    KASSERT(0 == IRQ_ENABLED(get_eflags()));
    /*
     * Get CPU on which we execute and lock its queue
     */
    cpuid = smp_get_cpu();
    spinlock_get(&queue_lock[cpuid], &flags);
    /*
     * If the currently active task is not marked for being preempted as it has
     * used up its time slice, return immediately and select this task again
     */
    if (active[cpuid]) {
        if (0 == active[cpuid]->reschedule) {
            rc = active[cpuid] - runnable[cpuid];
            spinlock_release(&queue_lock[cpuid], &flags);
            return rc;
        }
    }
    if (active[cpuid]) {
        /*
         * Decrease current priority if the runnable
         * has exhausted its quantum
         */
        if ((0 == active[cpuid]->quantum) && (active[cpuid]->priority > 0))
            (active[cpuid]->priority)--;
        /*
         * Add current runnable to tail of ready queue
         * for its new priority and refresh quantum if it was zero
         */
        LIST_ADD_END(queue[cpuid][active[cpuid]->priority].head, queue[cpuid][active[cpuid]->priority].tail, active[cpuid]);
        cpu_queue_length[cpuid]++;
        if (active[cpuid]->quantum == 0)
            active[cpuid]->quantum = SCHED_INIT_QUANTUM;
    }
    /*
     * Now determine runnable to execute next. For this purpose, we locate the highest priority
     * queue which is not empty
     */
    for (i = SCHED_MAX_PRIO; i >= 0; i--)
        if (queue[cpuid][i].head)
            break;
    if (0 == queue[cpuid][i].head) {
        /*
         * This can actually happen if a CPU has not yet been fully initialized. In all other
         * cases, it is illegal
         */
        spinlock_release(&queue_lock[cpuid], &flags);
        if (1 == cpu_used[cpuid])
            PANIC("Head of priority 0 queue on CPU %d is empty - where has idle task gone?\n", cpuid);
        return -1;
    }
    /*
     * Remove runnable from the head of this queue and make it active
     */
    active[cpuid] = queue[cpuid][i].head;
    LIST_REMOVE_FRONT(queue[cpuid][i].head, queue[cpuid][i].tail);
    cpu_queue_length[cpuid]--;
    active[cpuid]->reschedule = 0;
    rc = active[cpuid] - runnable[cpuid];
    spinlock_release(&queue_lock[cpuid], &flags);
    return rc;
}

/*
 * Reduce quantum of currently active process by one. This function should be called
 * periodically by the interrupt handler while interrupts are disabled
 */
void sched_do_tick() {
    u32 busy_ticks;
    u32 idle_ticks;
    int cpuid;
    u32 flags;
    /*
     * Make sure that we are not preempted and will therefore
     * continue to execute on the same CPU until this function completes
     */
    KASSERT(IRQ_ENABLED(get_eflags())==0);
    /*
     * Get CPU on which we execute
     */
    cpuid = smp_get_cpu();
    /*
     * Get lock
     */
    spinlock_get(&queue_lock[cpuid], &flags);
    /*
     * Reduce quantum of currently active runnable
     */
    if (active[cpuid]) {
        if (active[cpuid]->quantum) {
            active[cpuid]->quantum--;
            if (0 == active[cpuid]->quantum) {
                active[cpuid]->reschedule = 1;
            }
        }
        /*
         * Update statistics on idle and busy ticks
         */
        if (active[cpuid] == runnable[cpuid] + idle_task[cpuid]) {
            idle[cpuid]++;
        }
        else
            busy[cpuid]++;
    }
    else
        idle[cpuid]++;
    /*
     * If idle + busy is a multiple of HZ , update average load and
     * store busy and idle
     */
    if (0 == ((idle[cpuid] + busy[cpuid]) % HZ)) {
        busy_ticks = busy[cpuid] - busy_last[cpuid];
        idle_ticks = idle[cpuid] - idle_last[cpuid];
        if ((busy_ticks + idle_ticks))
            load[cpuid] = (busy_ticks * 100 / (busy_ticks + idle_ticks));
        else
            load[cpuid] = 0;
        busy_last[cpuid] = busy[cpuid];
        idle_last[cpuid] = idle[cpuid];
    }
    spinlock_release(&queue_lock[cpuid], &flags);
}



/***************************************************************
 * Everything below this line is for debugging only            *
 **************************************************************/

extern void (*debug_getline)(char* line, int max);


/*
 * Print out debugging information for each runnable
 * and the ready queue
 */
void sched_print() {
    int i;
    int cpu;
    int count;
    int lines;
    char c[2];
    runnable_t* item = 0;
    PRINT("Runnables:\n");
    PRINT("ID            Priority  CPU\n");
    PRINT("---------------------------\n");
    lines = 0;
    for (cpu = 0; cpu < SMP_MAX_CPU; cpu++) {
        for (i = 0; i < PM_MAX_TASK; i++)
            if (runnable[cpu][i].valid == 1) {
                PRINT("%x     %h        %d", i,   runnable[cpu][i].priority, cpu);
                if (active[cpu] == (runnable[cpu] + i))
                    PRINT("*");
                PRINT("\n");
                lines++;
                if (0 == (lines % 10)) {
                    lines = 0;
                    PRINT("Hit ENTER to see next page\n");
                    debug_getline(c,1);
                    PRINT("ID            Priority  CPU\n");
                    PRINT("---------------------------\n");
                }
            }
    }
    PRINT("\n");
    PRINT("Hit ENTER to print scheduler queues\n");
    debug_getline(c,1);
    lines = 0;
    PRINT("Queues:\n");
    PRINT("Priority  Count\n");
    PRINT("---------------\n");
    for (cpu = 0; cpu < SMP_MAX_CPU; cpu++) {
        for (i = 0; i <= SCHED_MAX_PRIO; i++) {
            count = 0;
            LIST_FOREACH(queue[cpu][i].head, item) {
                count++;
            }
            if (count) {
                PRINT("%h        %d\n", i, count);
                lines++;
                if (0 == (lines % 10)) {
                    lines = 0;
                    PRINT("Hit ENTER to see next page\n");
                    debug_getline(c,1);
                    PRINT("Priority  Count\n");
                    PRINT("---------------\n");
                }
            }
        }
    }
    PRINT("Hit ENTER to print CPU list\n");
    debug_getline(c,1);
    PRINT("\nCPUs:\n");
    PRINT("ID  Queue length    Load\n");
    PRINT("-------------------------\n");
    for (cpu = 0; cpu < SMP_MAX_CPU; cpu++) {
        if (1 == cpu_used[cpu]) {
            PRINT("%h  %h              %d\n", cpu, cpu_queue_length[cpu], load[cpu]);
        }
    }
}

/*
 * Return current load on CPU cpuid
 */
int sched_get_load(int cpuid) {
    if ((cpuid < 0) || (cpuid >= SMP_MAX_CPU))
        return 0;
    if (1 == cpu_used[cpuid]) {
        return load[cpuid];
    }
    return 0;
}
