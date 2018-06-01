/*
 * pm.c
 */

/* This is the source code for the process and task manager. This module is responsible for the lifecycle of a process
 * and a task. It also contains all other functions which change the status of a task (like sem_down) and functions
 * to load and run an executable as well as the entire signal processing code
 *
 * The main data structures in this module are
 *
 * a table of processes
 * a table of tasks
 *
 * A process is essentially a virtual address space plus the attributes of a process specified by POSIX (session, process group,
 * parent, real and effective user and group ID, pending signals). A task is a thread of execution which is represented as a runnable
 * piece of work within the scheduler and is executing within exactly one process = virtual address space.
 *
 * Note that a process is also reflected by a matching data structure in the file system (fs.c) and the memory manager (mm.c), whereas
 * a task that is ready to run is represented by a runnable in the scheduler (sched.c)
 *
 * A task can be in any of the following states:
 *
 * TASK_STATUS_NEW             A new task which is still being initialized
 * TASK_STATUS_RUNNING         Ready to run and known to the scheduler
 * TASK_STATUS_BLOCKED         Blocked and waiting, for instance for a semaphore
 * TASK_STATUS_DONE            Completed, but not yet cleaned up
 * TASK_STATUS_STOPPED         Stopped due to a signal
 * TASK_STATUS_BLOCKED_INTR    Blocked, but ready to continue if a signal is received
 *
 * This module also handles the signal processing. Essentially, signals are processed when returning to user space. Delivering a
 * signal to a task amounts to setting a flag in the task structure which is evaluated when returning to user space.
 */

/*
 * A note on locking: The four most important locks in this module are:
 *
 * proc_table_lock: protect the process table, the parent-child relationships, process groups and sessions
 * task_table_lock: protect task table and reference counts of tasks
 * spinlock in struct proc_t: protect an individual process
 * spinlock in struct task_t: protect an individual task
 *
 * Especially in the signal handling code, there are situations where we need to walk either the process table or the task
 * table and then perform a specific action on the tasks or processes. This might involve getting more than one of these locks
 * within one thread of execution. To avoid deadlocks, only certain orders are allowed here, as indicated in the following diagram
 *
 *                                --  proc_table_lock  --
 *                                |          |          |
 *                                V          |          V
 *                             lock on a <---|------ task table
 *                              process      |         lock
 *                                |          V          |
 *                                --> lock on a task <---
 *
 *
 * Thus if you already hold the process table lock, you can get any of the other three locks. If you hold the task table lock, you can
 * still get the lock on a task or a process, but no other lock. If you hold the lock on a process, you can still get the lock on a task,
 * but no other lock. Finally, if you hold the lock on a task, you cannot get any other lock.
 */

#include "pm.h"
#include "wq.h"
#include "util.h"
#include "kprintf.h"
#include "debug.h"
#include "mm.h"
#include "gdt.h"
#include "lib/string.h"
#include "locks.h"
#include "irq.h"
#include "sched.h"
#include "kerrno.h"
#include "lists.h"
#include "fs.h"
#include "elf.h"
#include "rtc.h"
#include "lib/os/syscalls.h"
#include "lib/os/stat.h"
#include "lib/sys/stat.h"
#include "lib/os/times.h"
#include "lib/sys/resource.h"
#include "timer.h"
#include "smp.h"
#include "debug.h"
#include "timer.h"
#include "drivers.h"
#include "tty.h"

/*
 * References to sigreturn.o which contains the code executed in user
 * space to complete the processing of a signal
 */
extern u32 __sigreturn_start;
extern u32 __sigreturn_end;


/*
 * This table holds all tasks. It is protected by the spinlock task_table_lock, i.e. whenever entries are to be added to the
 * table or removed from the table, that lock needs to be taken first. This lock is also required if the reference count of
 * a task is modified.
 */
static task_t tasks[PM_MAX_TASK];
static spinlock_t task_table_lock;

/*
 * This table holds all processes and is protected by the lock proc_table_lock. Note that the process table lock is also
 * used to synchronize access to the relationships of processes given the process groups and sessions as well as parent-child
 * relationships and the controlling terminal
 */
static proc_t procs[PM_MAX_PROCESS];
static spinlock_t proc_table_lock;


/*
 * The currently active task and process as well as the
 * previously active task and process
 */
static int active_task[SMP_MAX_CPU];
static int active_proc[SMP_MAX_CPU];
static int previous_task[SMP_MAX_CPU];
static int previous_proc[SMP_MAX_CPU];


/*
 * This array holds all known signals and their default actions
 */

static sig_default_action_t sig_default_actions[] = {
        { __KSIGCONT, SIG_DFL_CONT },
        { __KSIGSTOP, SIG_DFL_STOP },
        { __KSIGKILL, SIG_DFL_TERM },
        { __KSIGUSR1, SIG_DFL_TERM },
        { __KSIGUSR2, SIG_DFL_TERM },
        { __KSIGCHLD, SIG_DFL_IGN},
        { __KSIGTTIN, SIG_DFL_STOP},
        { __KSIGTTOU, SIG_DFL_STOP},
        { __KSIGTSTP, SIG_DFL_STOP},
        { __KSIGABRT, SIG_DFL_TERM},
        { __KSIGALRM, SIG_DFL_TERM},
        { __KSIGBUS, SIG_DFL_TERM},
        { __KSIGFPE, SIG_DFL_TERM},
        { __KSIGHUP, SIG_DFL_TERM},
        { __KSIGILL, SIG_DFL_TERM},
        { __KSIGINT, SIG_DFL_TERM},
        { __KSIGPIPE, SIG_DFL_TERM},
        { __KSIGQUIT, SIG_DFL_TERM},
        { __KSIGSEGV, SIG_DFL_TERM},
        { __KSIGTERM, SIG_DFL_TERM},
        { __KSIGURG, SIG_DFL_IGN},
        { __KSIGTASK, SIG_DFL_STOP}};

/*
 * Some forward declarations
 */
static void signal_proc(proc_t* proc, int sig_no);
static void discard_signal(proc_t* proc, u32 sigmask, int threads);
static int promote_signals(task_t* task);
static int get_default_action(int sig_no);


/*****************************************************************************************
 * The following functions are used to work with tasks, i.e. to locate free slots in the *
 * task table, locate a task by ID and get a reference to it and release a reference to  *
 * a task again                                                                          *
 *****************************************************************************************/

/*
 * Locate a free slot in the task table and reserve it.
 * Return value:
 * pointer to task slot on success
 * 0 if no free slot could be found
 * Locks:
 * task_table_lock
 */
static task_t* reserve_task() {
    u32 eflags;
    int i;
    spinlock_get(&task_table_lock, &eflags);
    /*
     * Start scan at one, as task 0 will never be available
     */
    for (i = 1; i < PM_MAX_TASK; i++) {
        if (TASK_SLOT_FREE == tasks[i].slot_usage) {
            tasks[i].slot_usage = TASK_SLOT_RESERVED;
            tasks[i].id = i;
            spinlock_release(&task_table_lock, &eflags);
            return tasks + i;
        }
    }
    spinlock_release(&task_table_lock, &eflags);
    return 0;
}

/*
 * Unreserve a previously reserved slot in the task table
 * Parameter:
 * @task - the task to be unreserved
 * Locks:
 * task_table_lock
 */
static void unreserve_task(task_t* task) {
    u32 eflags;
    KASSERT(task);
    spinlock_get(&task_table_lock, &eflags);
    KASSERT(TASK_SLOT_RESERVED == task->status);
    task->slot_usage = TASK_SLOT_FREE;
    spinlock_release(&task_table_lock, &eflags);
}

/*
 * Activate a task, i.e. commit a previously reserved task as used
 * Parameter:
 * @task - the task to be committed
 * Locks:
 * task_table_lock
 */
static void activate_task(task_t* task) {
    u32 eflags;
    KASSERT(task);
    spinlock_get(&task_table_lock, &eflags);
    KASSERT(TASK_SLOT_RESERVED == task->slot_usage);
    task->slot_usage = TASK_SLOT_USED;
    spinlock_release(&task_table_lock, &eflags);
}

/*
 * Initialize the task table entry for a newly created task. It is assumed that the caller holds the
 * necessary locks.
 * Parameter:
 * @task - pointer to new task
 * @self - currently active task
 * @esp - value to use for the saved stack pointer of the task
 * Notes:
 * The fields in the task structure are initialized as follows.
 * user_id - set to task id
 * ref_count - set to zero
 * status - set to NEW
 * saved_esp - according to parameter esp
 * saved_cr3 - set to same value as active task
 * execution level - kernel thread
 * force_exit - zero
 * proc - taken over from currently active task
 * priority - taken over from currently active task
 * ticks - set to zero
 * sig_waiting - zero
 * sig_blocked - taken over from currently active task
 * sig_pending - zero
 * intr - zero
 * idle - zero
 * cpuid - -1
 * fpu - 0
 * fpu_save_area - 0
 */
static void init_task(task_t* task, task_t* self, reg_t esp) {
    KASSERT(task);
    task->user_id = task->id;
    task->ref_count = 0;
    task->status = TASK_STATUS_NEW;
    task->saved_esp = esp;
    task->saved_cr3 = get_cr3();
    KASSERT(task->saved_cr3);
    task->execution_level = EXECUTION_LEVEL_KTHREAD;
    task->force_exit = 0;
    spinlock_init(&task->spinlock);
    task->proc = self->proc;
    task->priority = self->priority;
    task->ticks = 0;
    task->sig_waiting = 0;
    task->sig_blocked = self->sig_blocked;
    task->sig_pending = 0;
    task->intr = 0;
    task->idle = 0;
    task->cpuid = -1;
    task->fpu = 0;
    task->fpu_save_area = 0;
}

/*
 * Utility function to clone a task. Only call this from within do_fork
 * to clone the currently active task in order to create the initial task
 * of the new process
 * Parameter:
 * @task_id - id of the new task
 * @proc_id - PID of the new process
 * @cr3 - value for saved cr3 register of new task
 * @ir_context - current interrupt context
 */
static void clone_task(task_t* task, int proc_id, u32 cr3,
        ir_context_t* ir_context) {
    task_t* self = tasks + pm_get_task_id();
    KASSERT(task);
    task->ref_count = 0;
    task->user_id = self->user_id;
    task->status = TASK_STATUS_NEW;
    task->saved_cr3 = cr3;
    /*
     * Use the stack pointer in the context as saved esp
     * so that our new task continues at this point
     */
    task->saved_esp = ir_context->esp;
    /*
     * The new task will start execution at the point where the old task
     * issued the fork system call, so that execution level of the new task
     * might be kernel thread level or user space, depending on the code
     * segment of the context
     */
    if (mm_is_kernel_code(ir_context->cs_old)) {
        task->execution_level = EXECUTION_LEVEL_KTHREAD;
    }
    else {
        task->execution_level = EXECUTION_LEVEL_USER;
    }
    /*
     * Set up remaining fields
     */
    task->force_exit = 0;
    spinlock_init(&task->spinlock);
    task->proc = &procs[proc_id];
    task->priority = self->priority;
    task->ticks = 0;
    task->sig_waiting = 0;
    task->sig_blocked = self->sig_blocked;
    task->sig_pending = 0;
    task->intr = 0;
    task->cpuid = -1;
    if (self->fpu_save_area) {
        task->fpu_save_area = kmalloc_aligned(FPU_STATE_BYTES, 16);
        if (0 == task->fpu_save_area) {
            PANIC("Not sufficient memory to clone FPU area for a task\n");
        }
        /*
         * If the task has not used the FPU since we have last saved the state, we can just copy
         * the saved state. Otherwise we need to save a new state
         */
        if (0 == self->fpu) {
            memcpy((void*) task->fpu_save_area, (void*) self->fpu_save_area, FPU_STATE_BYTES);
        }
        else {
            fpu_save((u32) task->fpu_save_area);
        }
    }
    else
        task->fpu_save_area = 0;
    /*
     * We set the FPU bit of the new task to zero as the task has not used the FPU since the last
     * saving operation
     */
    task->fpu = 0;
}

/*
 * Get a pointer to a task from the task table and increase the task's
 * reference count by one
 * Parameter:
 * @task_id - the id of the task we are looking for
 * Return value:
 * a pointer to the task if the task id is valid
 * 0 otherwise
 * Locks:
 * task_table_lock
 */
static task_t* get_task(int task_id) {
    u32 eflags;
    if ((task_id < 0) || (task_id >= PM_MAX_TASK))
        return 0;
    spinlock_get(&task_table_lock, &eflags);
    if (TASK_SLOT_USED == tasks[task_id].slot_usage) {
        tasks[task_id].ref_count++;
        if (0 == tasks[task_id].ref_count) {
            PANIC("Hmm...reference count of task %d is 0 even though I just incremented it - should not happen!\n", task_id);
        }
        spinlock_release(&task_table_lock, &eflags);
        return tasks + task_id;
    }
    spinlock_release(&task_table_lock, &eflags);
    return 0;
}

/*
 * Release a reference to a task again and decrease reference count accordingly.
 * If the reference count drops below 0, the task table slot is invalidated and
 * made available for future use.
 * Parameter:
 * @task - the task to be released
 * Locks:
 * task_table_lock
 */
static void release_task(task_t* task) {
    u32 eflags;
    task_t* self = tasks + pm_get_task_id();
    KASSERT(task);
    if (TASK_SLOT_USED != task->slot_usage) {
        PANIC("Could not release task %d, slot not in use, ref_count = %d, actual status = %d\n", task->id,
                task->ref_count, task->slot_usage);
    }
    spinlock_get(&task_table_lock, &eflags);
    task->ref_count--;
    if (task->ref_count < 0) {
        /*
         * We should never invalidate the currently active task
         */
        KASSERT(task->id != self->id);
        task->slot_usage = TASK_SLOT_FREE;
        if (task->fpu_save_area) {
            kfree(task->fpu_save_area);
            task->fpu_save_area = 0;
        }
    }
    spinlock_release(&task_table_lock, &eflags);
}

/***********************************************************************************************
 * The following functions are used to change the status of a task, i.e. make a task           *
 * runnable, stop a task or block a task. They should ONLY be called if the caller owns        *
 * the lock on the respective task                                                             *
 * Note that while a task can run any other tasks, a task can only block or stop itself        *
 ***********************************************************************************************/

/*
 * Utility function to make a task runnable again. This function will set the
 * status of a task to running and add it to the scheduler queue again with the
 * original priority plus 1. The caller of this function is assumed to hold the lock
 * on the task status
 * Parameter:
 * @task - the task
 */
static void run_task(task_t* task) {
    KASSERT(TASK_SLOT_USED == task->slot_usage);
    KASSERT(TASK_STATUS_RUNNING != task->status);
    task->status = TASK_STATUS_RUNNING;
    if (-1 == task->cpuid)
        sched_enqueue(task->id, MIN(task->priority+1, SCHED_MAX_PRIO));
    else
        sched_enqueue_cpu(task->id, MIN(task->priority+1, SCHED_MAX_PRIO), task->cpuid);
}


/*
 * Utility function to initially start a task after creation. This function will
 * activate the task in the task table so that it becomes visible for other
 * tasks, set its status to RUNNING and add it to the schedulers ready queues
 * Parameter:
 * @task - the task to be started
 * Locks:
 * lock on task structure
 */
static void start_task(task_t* task) {
    u32 eflags;
    activate_task(task);
    spinlock_get(&task->spinlock, &eflags);
    run_task(task);
    spinlock_release(&task->spinlock, &eflags);
}


/*
 * Block the currently active task, i.e. remove it from the scheduler
 * queues and set its status to BLOCKED. The caller of this function is
 * supposed to hold the lock on the task status
 */
static void block_task() {
    task_t* self = tasks + pm_get_task_id();
    KASSERT(TASK_STATUS_RUNNING == self->status);
    self->floating = 1;
    sched_dequeue();
    self->status = TASK_STATUS_BLOCKED;
}

/*
 * Block the currently active task until it is signaled, i.e. remove it from the scheduler
 * queues and set its status to BLOCKED_INTR. The caller
 * of this function is supposed to hold the lock on the task status.
 * This function returns immediately if there is any pending unblocked signal on task level
 */
static void block_task_intr() {
    task_t* self = tasks + pm_get_task_id();
    if (self->sig_pending & ~self->sig_blocked) {
        self->intr = 1;
        return;
    }
    KASSERT(TASK_STATUS_RUNNING == self->status);
    self->floating = 1;
    sched_dequeue();
    self->status = TASK_STATUS_BLOCKED_INTR;
}

/*
 * Stop the currently active task, i.e. remove it from the scheduler
 * queues and set its status to STOPPED.
 * Locks:
 * spinlock on task
 */
static void stop_task() {
    u32 eflags;
    task_t* self = tasks + pm_get_task_id();
    spinlock_get(&self->spinlock, &eflags);
    KASSERT(TASK_STATUS_RUNNING == self->status);
    self->floating = 1;
    sched_dequeue();
    self->status = TASK_STATUS_STOPPED;
    spinlock_release(&self->spinlock, &eflags);
}

/*
 * Reschedule, but only if the currently active task is not RUNNING. This
 * function can be used to force a reschedule after a task has been blocked. As it
 * is theoretically possible that the task has already been woken up when the
 * reschedule is executed (which is not a problem, just a waste of time), we check
 * for the status first
 * Locks:
 * spinlock on task
 */
void cond_reschedule() {
    u32 eflags;
    int status;
    task_t* self = tasks + pm_get_task_id();
    spinlock_get(&self->spinlock, &eflags);
    status = self->status;
    spinlock_release(&self->spinlock, &eflags);
    if ((TASK_STATUS_RUNNING != status) || (self->floating)) {
        reschedule();
    }
}


/*
 * Maintain the FPU flag of a task - this function is called by the interrupt
 * manager when an NF trap is raised, indicating that the FPU is used for the
 * first time during a time slice
 */
void pm_handle_nm_trap() {
    task_t* self = tasks + pm_get_task_id();
    /*
     * Is the fpu bit already set? This should never happen
     */
    if (self->fpu) {
        PANIC("NM exception raised even though FPU bit is set\n");
    }
    else {
        /*
         * Set FPU flag
         */
        self->fpu = 1;
        /*
         * and clear TS bit
         */
        clts();
        /*
         * If there is a saved FPU state, get it
         */
        if (self->fpu_save_area) {
            fpu_restore((u32) self->fpu_save_area);
        }
    }
}


/*
 * Execute a task switch
 * Note that the task switch only becomes active when the second part of the common handler
 * in gates.S is executed. This implies in particular that between executing this function
 * and the return to gates.S, active_task and active_proc are not correctly set. So the task switch
 * should be the last thing to be done in an interrupt context
 * Note: we do not check here that the new task is valid
 * Parameter:
 * @task - the task to switch to, -1 means that the scheduler could not determine a new valid task
 * @ir_context - the interrupt context on which we operate
 * Return value:
 * The function returns 1 if a task switch took place and 0 otherwise
 */
int pm_switch_task(int task, ir_context_t* ir_context) {
    int wait = 0;
    task_t* self = tasks + pm_get_task_id();
    task_t* target = tasks + task;
    int cpuid = smp_get_cpu();
    if ((task == self->id) || (-1 == task)) {
        return 0;
    }
    if (TASK_SLOT_USED != target->slot_usage) {
        PANIC("Invalid target task\n");
    }
    /*
     * If the "floating" flag of the target task is set, this means that the task
     * is already blocked, but the CPU on which it has been running has not yet processed
     * the next interrupt after running block_task or block_task_intr, i.e. has not yet switched
     * to another task. In particular, the saved_esp and saved_cr3 fields in the target task do not
     * yet point to a valid interrupt context and the kernel stack of the task is still in use.
     * Thus we need to wait until the flag is cleared before we can proceed and use the saved interrupt context
     * and kernel stack.  We time out after waiting for 10 ms. Note that the flag is cleared when the task
     * passes pm_cleanup_task, i.e. in the post-IRQ handler executed after every task switch. This is a
     * little bit of a hack, but this situation is a rare exception
     */
    if (target->floating) {
        for (wait = 0; wait < 1000; wait++) {
            if (0 == target->floating)
                break;
            udelay(10);
        }
    }
    /*
     * Complain if floating flag of target task is still not set
     */
    KASSERT(0 == tasks[task].floating);
    KASSERT(TASK_STATUS_RUNNING == tasks[task].status);
    KASSERT(0 == IRQ_ENABLED(get_eflags()));
    /*
     * Save current values of ESP and CR3
     */
    self->saved_esp = ir_context->esp;
    self->saved_cr3 = ir_context->cr3;
    /*
     * Patch IR context on stack. This will cause the actual task
     * switch when leaving the interrupt context again
     */
    ir_context->esp = tasks[task].saved_esp;
    ir_context->cr3 = tasks[task].saved_cr3;
    /*
     * Adapt active_task and active_proc
     */
    previous_task[cpuid] = active_task[cpuid];
    previous_proc[cpuid] = active_proc[cpuid];
    active_task[cpuid] = target->id;
    active_proc[cpuid] = target->proc->id;
    /*
     * Put address of kernel stack of new task into
     * the task status segment (TSS)
     */
    gdt_update_tss(mm_get_kernel_stack(active_task[cpuid]), cpuid);
    smp_mb();
    /*
     * If FPU bit was set, clear it and save FPU state
     */
    if (self->fpu) {
        self->fpu = 0;
        if (0 == self->fpu_save_area) {
            self->fpu_save_area = kmalloc_aligned(FPU_STATE_BYTES, 16);
            if (0 == self->fpu_save_area) {
                PANIC("Could not allocate memory for FPU save area\n");
            }
        }
        fpu_save((u32) self->fpu_save_area);
    }
    /*
     * Make sure that CR0.TS is set. We need to do this after saving the FPU
     * state as otherwise the saving will raise an exception
     */
    setts();
    return 1;
}

/****************************************************************************************
 * Synchronisation primitives, i.e.                                                     *
 * - condition variables                                                                *
 * - semaphores                                                                         *
 ****************************************************************************************/

/*
 * Wake up a task which is sleeping on an even control block (ECB)
 */
void wakeup_task(ecb_t* ecb) {
    u32 eflags;
    u32 task_id;
    task_id = ecb->waiting_task;
    spinlock_get(&(tasks[task_id].spinlock), &eflags);
    if ((TASK_STATUS_BLOCKED == tasks[task_id].status)
            || (TASK_STATUS_BLOCKED_INTR == tasks[task_id].status)) {
        run_task(tasks + task_id);
    }
    spinlock_release(&(tasks[task_id].spinlock), &eflags);
}

/*
 * Initialize a semaphore
 * Parameter:
 * @sem - the semaphore to be set up
 * @value - the initial value of the semaphore
 */
void sem_init(semaphore_t* sem, u32 value) {
    sem->value = value;
    spinlock_init(&sem->lock);
    sem->queue_head = 0;
    sem->queue_tail = 0;
    sem->next = 0;
    sem->prev = 0;
    sem->timed = 0;
    sem->timeout = 0;
    sem->timeout_val = 0;
}

/*
 * Perform the down operation on a semaphore
 * This function will also check whether interrupts are disabled
 * and PANIC if that is the case as doing a sleep while interrupts are disabled will block the CPU forever.
 * Parameter:
 * @sem - the semaphore to use
 * @intr - set this to 1 if the semaphore should use interruptible sleep
 * @timeout - timeout in ticks
 * @time - set this to 1 to time semaphore
 * Return value:
 * 0 - normal completion of down operation
 * -1 - down operation aborted due to a signal (only possible if intr = 1)
 * -2 - down operation aborted due to a timeout (only possible if time = 1)
 */
static int perform_sem_down(semaphore_t* sem, int intr, char* file, int line, u32 timeout, int time) {
    u32 eflags1;
    u32 eflags2;
    u32 task_id = pm_get_task_id();
    task_t* self = tasks + task_id;
    ecb_t* ecb;
    int rc = 0;
    /*
     * First get lock on semaphore
     */
    spinlock_get(&sem->lock, &eflags1);
    KASSERT (TASK_STATUS_DONE != self->status);
    KASSERT(TASK_SLOT_USED == self->slot_usage);
    while (0 == sem->value) {
        /*
         * Panic if interrupts are disabled
         */
        if (IRQ_ENABLED(eflags1) == 0)
            PANIC("sem_down invoked while interrupts are disabled and value is null - this will hang the CPU!");
        /*
         * Allocate event control block and add it to the queue. Note that we cannot allocate ecb on the stack
         * as it would then not be visible to another process
         */
        if (0 == (ecb = (ecb_t*) kmalloc(sizeof(ecb_t)))) {
            PANIC("Could not allocate memory for event control block!\n");
        }
        /*
         * If requested, time semaphore
         */
        if (time) {
            timer_time_ecb(ecb, timeout);
        }
        else {
            ecb->timer.is_active = 0;
        }
        /*
         * Now add entry to queue
         */
        LIST_ADD_END(sem->queue_head, sem->queue_tail, ecb);
        ecb->waiting_task = self->id;
        /*
         * Inform debugger that we are waiting for the lock
         */
        debug_lock_wait((u32) sem, 2, 0, file, line);
        /*
         * Get spinlock on task status
         */
        spinlock_get(&self->spinlock, &eflags2);
        /*
         * Dequeue task and update task status. Make sure to release all
         * locks before rescheduling
         */
        if (0 == intr)
            block_task();
        else
            block_task_intr();
        spinlock_release(&self->spinlock, &eflags2);
        spinlock_release(&sem->lock, &eflags1);
        reschedule();
        /*
         * Get lock on semaphore again and remove event control block
         * from queue
         */
        spinlock_get(&sem->lock, &eflags1);
        KASSERT(TASK_SLOT_USED == self->slot_usage);
        LIST_REMOVE(sem->queue_head, sem->queue_tail, ecb);
        /*
         * If this was a timed semaphore, cancel timer
         */
        if (ecb->timer.is_active) {
            timer_cancel_ecb(ecb);
        }
        /*
         * If this was a timeout, return
         */
        if ((ecb->timer.is_active) && (ecb->timer.timeout)) {
            spinlock_release(&sem->lock, &eflags1);
            kfree(ecb);
            return -2;
        }
        /*
         * Free event control block
         */
        kfree(ecb);
        /*
         * If we have been called with the parameter intr = 1, i.e. requesting an interruptible sleep,
         * look at flag in task structure to see whether we have been interrupted by a signal. If this is set,
         * clear it and return -1.
         * Before reading the flag, we also have to make sure that we get the spinlock on the task status
         * once more, to avoid race conditions with the wakeup code (in fact, we want to make sure that the
         * wakeup code completes both the rescheduling of this task and setting this flag to one before
         * we read from it)
         */
        if (1 == intr) {
            spinlock_get(&self->spinlock, &eflags2);
            if (1 == self->intr) {
                self->intr = 0;
                spinlock_release(&self->spinlock, &eflags2);
                spinlock_release(&sem->lock, &eflags1);
                /*
                 * Inform debugger that we have canceled the lock request
                 */
                debug_lock_cancel((u32) sem, 0);
                return -1;
            }
            spinlock_release(&self->spinlock, &eflags2);
        }
        /*
         * Inform debugger that we are now owning the lock
         */
        debug_lock_cancel((u32) sem, 0);
    }
    sem->value--;
    spinlock_release(&sem->lock, &eflags1);
    return rc;
}

/*
 * Perform the down operation on a semaphore
 * Parameter:
 * @sem - a pointer to the semaphore
 */
void __sem_down(semaphore_t* sem, char* file, int line) {
    perform_sem_down(sem, 0, file, line, 0, 0);
}

/*
 * Wrapper function for sem_down interruptible. If a sleep is necessary during execution of a down operation,
 * the task will be put in the status "sleep interruptible". It can be woken up by
 * a) setting its status back to RUNNING, and
 * b) setting the flag intr in the task structure to 1,
 * both to be performed in an atomic operation protected by task->spinlock. In this case, this operation will
 * return with return code -1 and will NOT perform the actual down operation, i.e. the value of the semaphore will
 * remain unchanged.
 * Parameter:
 * @sem - the semaphore
 * Return value:
 * 0 - operation was completed
 * -1 - operation was not completed and interrupted as described above
 */
int __sem_down_intr(semaphore_t* sem, char* file, int line) {
    return perform_sem_down(sem, 1, file, line, 0, 0);
}

/*
 * Wrapper function for sem_down timed. If a sleep is necessary during execution of a down operation,
 * the task will be put in the status "sleep interruptible". It can be woken up by
 * a) setting its status back to RUNNING, and
 * b) setting the flag intr in the task structure to 1,
 * both to be performed in an atomic operation protected by task->spinlock. In this case, this operation will
 * return with return code -1 and will NOT perform the actual down operation, i.e. the value of the semaphore will
 * remain unchanged.
 * In addition, a timer is set. If the timer expires while a thread is sleeping on the semaphore, it will be woken
 * up as well
 * Parameter:
 * @sem - the semaphore
 * @file - source code file used for debugging
 * @line - line in source code
 * @timeout - timeout in ticks
 * Return value:
 * 0 - operation was completed
 * -1 - operation was not completed and interrupted as described above
 */
int __sem_down_timed(semaphore_t* sem, char* file, int line, u32 timeout) {
    return perform_sem_down(sem, 1, file, line, timeout, 1);
}

/*
 * Perform a down operation on a semaphore if possible. If not, do not sleep, but return immediately
 * Parameters:
 * @sem - the semaphore
 * Return value:
 * 0 if the operation could be completed
 * -1 if no down could be done because the value of the semaphore was already 0
 */
int sem_down_nowait(semaphore_t* sem) {
    u32 eflags;
    spinlock_get(&sem->lock, &eflags);
    if (sem->value > 0) {
        sem->value--;
        spinlock_release(&sem->lock, &eflags);
        return 0;
    }
    spinlock_release(&sem->lock, &eflags);
    return -1;
}

/*
 * Perform the up operation on a semaphore
 * Parameters:
 * @sem - the semaphore to use
 * @max_value - the maximum value which the semaphore can have - 0 means n/a
 * @timeout - set this to 1 if the UP operation occured due to a timeout
 * Note that this function temporarily gets the lock on all tasks which are
 * waiting in the queue!
 */
static void perform_sem_up(semaphore_t* sem, u32 max_value, int timeout) {
    u32 eflags1;
    u32 eflags2;
    u32 task_id;
    ecb_t* ecb;
    int unblocked;
    spinlock_get(&sem->lock, &eflags1);
    if (timeout)
        sem->timeout = 1;
    if (0 == sem->value) {
        /*
         * It can happen that the task at the head of the queue is no longer blocked, as it has been woken
         * up by an interrupt. We therefore scan elements from the head of the queue until we find one which
         * is still sleeping or until the queue ends
         */
        unblocked = 0;
        LIST_FOREACH(sem->queue_head, ecb) {
            task_id = ecb->waiting_task;
            spinlock_get(&(tasks[task_id].spinlock), &eflags2);
            if ((TASK_STATUS_BLOCKED == tasks[task_id].status)
                    || (TASK_STATUS_BLOCKED_INTR == tasks[task_id].status)) {
                run_task(tasks + task_id);
                unblocked = 1;
            }
            spinlock_release(&(tasks[task_id].spinlock), &eflags2);
            if (unblocked) {
                break;
            }
        }
    }
    if ((sem->value < max_value) || (0 == max_value))
        sem->value++;
    spinlock_release(&sem->lock, &eflags1);
}

/*
 * Perform the up-operation on a counter semaphore
 * Parameter:
 * @sem - the semaphore to use
 */
void sem_up(semaphore_t* sem) {
    perform_sem_up(sem, 0, 0);
}

/*
 * Perform the up-operation on a counter semaphore
 * due to a timeout
 * Parameter:
 * @sem - the semaphore to use
 */
void sem_timeout(semaphore_t* sem) {
    perform_sem_up(sem, 0, 1);
}

/*
 * Perform the up-operation on a binary semaphore
 * Parameter:
 * @sem - the semaphore to use
 */
void mutex_up(semaphore_t* mutex) {
    perform_sem_up(mutex, 1, 0);
}

/*
 * Initialize a condition variable
 * Parameter:
 * @cond - the condition variable
 */
void cond_init(cond_t* cond) {
    cond->queue_head = 0;
    cond->queue_tail = 0;
    spinlock_init(&cond->lock);
}

/*
 * Wait on a condition variable until being woken up by signal or broadcast or until
 * the current task has received a signal
 * Parameter:
 * @cond - the condition variable
 * @lock - the associated lock
 * @lock_eflags - saved eflags which have been used to acquire the spinlock
 * @timeout - timeout in ticks
 * Return value:
 * 0 - normal completion, i.e. the task has been woken up by signal or broadcast
 * -1 - the task has been woken up by delivery of a signal
 * -2 - the condition variable has timed out
 */
int cond_wait_intr_timed(cond_t* cond, spinlock_t* lock, u32* lock_eflags, unsigned int timeout) {
    u32 eflags;
    ecb_t* ecb;
    task_t* self = tasks + pm_get_task_id();
    if (lock_eflags)
        if ((*lock_eflags & (1 << 9)) == 0)
            PANIC("cond_wait_intr invoked while interrupts are disabled - this will hang the CPU!");
    /*
     * Allocate ecb
     */
    if (0 == (ecb = (ecb_t*) kmalloc(sizeof(ecb_t)))) {
        PANIC("Could not allocate memory for ecb\n");
        return -1;
    }
    ecb->waiting_task = self->id;
    /*
      * If requested, time condition variable
      */
     if (timeout) {
         timer_time_ecb(ecb, timeout);
     }
     else
         ecb->timer.is_active = 0;
    /*
     * Add ecb to queue
     */
    spinlock_get(&cond->lock, &eflags);
    LIST_ADD_END(cond->queue_head, cond->queue_tail, ecb);
    spinlock_release(&cond->lock, &eflags);
    /*
     * Now put task to sleep, release lock and reschedule
     */
    spinlock_get(&self->spinlock, &eflags);
    block_task_intr();
    spinlock_release(&self->spinlock, &eflags);
    if (lock)
        spinlock_release(lock, lock_eflags);
    reschedule();
    /*
     * Remove ourselves from queue
     */
    spinlock_get(&cond->lock, &eflags);
    LIST_REMOVE(cond->queue_head, cond->queue_tail, ecb);
    spinlock_release(&cond->lock, &eflags);
    /*
     * If this was a timed condition variable, cancel timer
     */
    if (ecb->timer.is_active) {
        timer_cancel_ecb(ecb);
    }
    /*
     * If this was a timeout, return
     */
    if ((ecb->timer.is_active) && (ecb->timer.timeout)) {
        kfree(ecb);
        return -2;
    }
    /*
     * Free ecb
     */
    kfree(ecb);
    /*
     * Get lock on task status again to see whether we have
     * been woken up by a signal.
     */
    spinlock_get(&self->spinlock, &eflags);
    /*
     * Return immediately if we have been woken up by a signal - note that this also
     * applies if a signal was pending when we called block_task_intr above
     */
    if (1 == self->intr) {
        self->intr = 0;
        spinlock_release(&self->spinlock, &eflags);
        return -1;
    }
    spinlock_release(&self->spinlock, &eflags);
    if (lock)
        spinlock_get(lock, lock_eflags);
    return 0;
}

/*
 * Wait on a condition variable until being woken up by signal or broadcast or until
 * the current task has received a signal
 * Parameter:
 * @cond - the condition variable
 * @lock - the associated lock
 * @lock_eflags - saved eflags which have been used to acquire the spinlock
 * Return value:
 * 0 - normal completion, i.e. the task has been woken up by signal or broadcast
 * -1 - the task has been woken up by delivery of a signal
 */
int cond_wait_intr(cond_t* cond, spinlock_t* lock, u32* lock_eflags) {
    return cond_wait_intr_timed(cond, lock, lock_eflags, 0);
}

/*
 * Wake up all tasks waiting on a condition variable
 * Parameter:
 * @cond - the condition variable
 * Note that this function will temporarily lock all
 * tasks which are currently waiting on the queue!
 */
void cond_broadcast(cond_t* cond) {
    ecb_t* ecb;
    u32 eflags;
    u32 eflags2;
    task_t* task;
    spinlock_get(&cond->lock, &eflags);
    LIST_FOREACH(cond->queue_head, ecb) {
        task = tasks + ecb->waiting_task;
        spinlock_get(&task->spinlock, &eflags2);
        if ((TASK_STATUS_BLOCKED == task->status) || (TASK_STATUS_BLOCKED_INTR
                == task->status)) {
            run_task(task);
        }
        spinlock_release(&task->spinlock, &eflags2);
    }
    spinlock_release(&cond->lock, &eflags);
}

/**********************************************************************************************************
 * The following functions are used to manage process table entries                                       *
 **********************************************************************************************************/

/*
 * Locate a free slot in the process table and reserve it.
 * Return value:
 * pointer to process slot on success
 * 0 if no free slot could be found
 * Locks:
 * proc_table_lock
 */
static proc_t* reserve_proc() {
    u32 eflags;
    int proc_id = 0;
    int i;
    int j;
    int found = 0;
    spinlock_get(&proc_table_lock, &eflags);
    for (i = 1; i < PM_MAX_PROCESS; i++) {
        if (PROC_SLOT_FREE == procs[i].slot_usage) {
            /*
             * Verify that there is no active process group with this
             * ID. Note that both setpgrp and setpgid use the process table lock,
             * so we can simply scan the table
             */
            found = 0;
            for (j = 1; j < PM_MAX_PROCESS; j++) {
                if (PROC_SLOT_USED == procs[j].slot_usage) {
                    if (procs[j].pgid == i) {
                        found = 1;
                        break;
                    }
                }
            }
            if (0 == found) {
                proc_id = i;
                procs[proc_id].slot_usage = PROC_SLOT_RESERVED;
                procs[proc_id].id = proc_id;
                break;
            }
        }
    }
    spinlock_release(&proc_table_lock, &eflags);
    if (proc_id)
        return procs + proc_id;
    return 0;
}

/*
 * Unreserve a previously reserved slot in the process table
 * Parameter:
 * @proc - the previously reserved process table slot
 * Locks:
 * proc_table_lock
 */
static void unreserve_proc(proc_t* proc) {
    u32 eflags;
    KASSERT(proc);
    spinlock_get(&proc_table_lock, &eflags);
    if (PROC_SLOT_RESERVED == proc->slot_usage) {
        proc->slot_usage = PROC_SLOT_FREE;
    }
    spinlock_release(&proc_table_lock, &eflags);
}

/*
 * Activate a process, i.e. commit a previously reserved process
 * Parameter:
 * @proc - the process
 * Locks:
 * proc_table_lock
 */
static void activate_proc(proc_t* proc) {
    u32 eflags;
    KASSERT(proc);
    spinlock_get(&proc_table_lock, &eflags);
    if (PROC_SLOT_RESERVED == proc->slot_usage) {
        proc->slot_usage = PROC_SLOT_USED;
    }
    spinlock_release(&proc_table_lock, &eflags);
}

/**************************************************************************************************
 * At any point in time, a running task is executing at one of the execution levels user space,   *
 * kernel level thread, system call level or hardware interrupt level. The following functions    *
 * are invoked from the interrupt manager and are used to keep track of the execution level       *
 **************************************************************************************************/

/*
 * Update the execution level, based on the old level as stored in the task structure
 * and the IRQ context.
 * Parameter:
 * @ir_context - current IRQ context
 * @old_level - here we store the old level
 * Return value:
 * the new level
 * Note: the following algorithm is used:
 * - if the interrupt vector is different from 0x80, the new execution level is always EXECUTION_LEVEL_IRQ
 * - if the interrupt vector is 0x80, a system call has been made, so the new execution level is EXECUTION_LEVEL_SYSCALL
 */
int pm_update_exec_level(ir_context_t* ir_context, int* old_level) {
    task_t* self = tasks + pm_get_task_id();
    if (TASK_SLOT_USED != self->slot_usage) {
        PANIC("Called for task %d with IRQ %d, but the task is no longer in use (slot_usage = %x)\n", self->id, ir_context->vector,
                self->slot_usage);
    }
    *old_level = self->execution_level;
    if (SYSCALL_IRQ == ir_context->vector) {
        self->execution_level = EXECUTION_LEVEL_SYSCALL;
    }
    else {
        self->execution_level = EXECUTION_LEVEL_IRQ;
    }
    /*
     * It is not permitted to invoke a system call from within an interrupt handler
     * or while another system call is executing
     */
    if ((EXECUTION_LEVEL_IRQ == *old_level) && (SYSCALL_IRQ
            == ir_context->vector)) {
        PANIC("Tried to do system call while being at interrupt level already\n");
    }
    if ((EXECUTION_LEVEL_SYSCALL == *old_level) && (SYSCALL_IRQ
            == ir_context->vector)) {
        PANIC("Tried to do system call while being at system call level already\n");
    }
    return self->execution_level;
}

/*
 * Restore the execution level of the currently active task
 * Parameters:
 * @ir_context - IR context
 * @old_exec_level - the old value to which we restore the tasks execution level
 */
void pm_restore_exec_level(ir_context_t* ir_context, int old_level) {
    task_t* self = tasks + pm_get_task_id();
    self->execution_level = old_level;
    /*
     * Verify that execution level matches IR context
     */
    if (mm_is_kernel_code(ir_context->cs_old) && (EXECUTION_LEVEL_USER
            == old_level)) {
        PANIC("Restored execution level USER, but code segment indicates kernel thread\n");
    }
    if (!mm_is_kernel_code(ir_context->cs_old) && (EXECUTION_LEVEL_USER
            != old_level)) {
        PANIC("Code segment indicates user space, but previous execution level is different\n");
    }
}

/****************************************************************************************
 * The functions in the following section are about task and process life cycle. This   *
 * includes the initialization routine of the process manager where the first task and  *
 * the first process are created as well as the implementation of the system calls      *
 * pthread_create and fork and the entire exit processing                               *
 ****************************************************************************************/

/*
 * Initialize process manager. This function will set up the data structures for the root
 * process and the root task and initialize all PM tables
 */
void pm_init() {
    int i;
    int cpu;
    /*
     * Init per-cpu data structures
     */
    for (cpu = 0; cpu < SMP_MAX_CPU; cpu++) {
        active_task[cpu] = 0;
        previous_task[cpu] = 0;
        active_proc[cpu] = 0;
        previous_proc[cpu] = 0;
    }
    /*
     * Clear task table entry and add entry for first task
     */
    memset(tasks, 0, sizeof(task_t) * PM_MAX_TASK);
    init_task(tasks, tasks, 0);
    tasks[0].slot_usage = TASK_SLOT_USED;
    tasks[0].status = TASK_STATUS_RUNNING;
    tasks[0].idle = 0;
    /*
     * Link new task to root process
     */
    tasks[0].proc = &procs[0];
    /*
     * Clear process table and create entry for first process
     */
    memset(procs, 0, sizeof(proc_t) * PM_MAX_PROCESS);
    for (i = 0; i < PM_MAX_PROCESS; i++) {
        procs[i].cterm = DEVICE_NONE;
    }
    procs[0].task_count = 1;
    spinlock_init(&procs[0].spinlock);
    cond_init(&procs[0].unwaited);
    /*
     * Initialize sigaction structures
     */
    for (i = 0; i < __NR_OF_SIGNALS; i++) {
        procs[0].sig_actions[i].sa_handler = __KSIG_DFL;
    }
    /*
     * Init lock on task table and process table
     */
    spinlock_init(&task_table_lock);
    spinlock_init(&proc_table_lock);
}

/*
 * Mark a process for exit processing.
 * Parameter:
 * @proc_id - the ID of the process
 * @exit_status - exit status to be set
 *
 * Notes:
 *
 * Each task in the process will be terminated the next time an interrupt
 * returns to kernel thread or user space execution level. In particular, the next system call
 * will never return for each of those threads
 *
 * This function does not acquire any locks - the caller needs to take care of getting the process table lock
 * to make sure that no process currently being re-initialized is flagged for exit
 *
 */
static void pm_schedule_exit(int proc_id, int exit_status) {
    if ((proc_id < 0) || (proc_id >= PM_MAX_PROCESS))
        return;
    if (PROC_SLOT_USED == procs[proc_id].slot_usage) {
        atomic_store((u32*)&procs[proc_id].exit_status, exit_status);
        atomic_store((u32*)&procs[proc_id].force_exit, 1);
    }
}

/*
 * This function is stored on the stack of a kernel thread as return address. It
 * simply invokes the quit system call to exit the currently running task.
 */
void pm_task_exit_handler() {
    __ctOS_syscall(__SYSNO_QUIT, 0);
    PANIC("Should never get here\n");
}

/*
 * Set the currently active task to DONE and remove it from the queue of runnable tasks
 * Parameter:
 * @task - the task
 * Locks:
 * lock on the task structure
 */
static void complete_task() {
    u32 eflags;
    task_t* self = tasks + pm_get_task_id();
    spinlock_get(&self->spinlock, &eflags);
    /*
     * If a task has just blocked itself but still owns the CPU
     * and the exit flag is set, we might be called for a blocked task.
     * Thus we only invoke sched_dequeue if the task is actually running
     */
    if (TASK_STATUS_RUNNING == self->status) {
        self->floating = 1;
        sched_dequeue();
    }
    self->status = TASK_STATUS_DONE;
    spinlock_release(&self->spinlock, &eflags);
}

/*
 * Perform any cleanup functions necessary after a task switch
 * Locks:
 * proc table lock
 * Cross-monitor function calls:
 * signal_proc
 * cond_broadcast
 */
void pm_cleanup_task() {
    u32 eflags;
    proc_t* previous;
    int cpuid = smp_get_cpu();
    /*
     * Make sure that interrupts are disabled
     */
    KASSERT (0 == IRQ_ENABLED(get_eflags()));
    /*
     * Set the floating flag of the previous task to zero
     */
    tasks[previous_task[cpuid]].floating = 0;
    smp_mb();
    /*
     * If the previous task has status done, clean up after it
     */
    if (TASK_STATUS_DONE == tasks[previous_task[cpuid]].status) {
        /*
         * Release stack of old task. We can now safely do this as we never return to it
         */
        mm_release_task_stack(previous_task[cpuid], previous_proc[cpuid]);
        /*
         * Get spinlock on process table - used for synchronization with waitpid
         */
        spinlock_get(&proc_table_lock, &eflags);
        /*
         * Decrease task count of previous process and release task table
         * slot. We need to do an atomic decrement here as we access the task
         * count at some points for reading without getting any locks
         */
        previous = procs + previous_proc[cpuid];
        atomic_decr((int*) &(previous->task_count));
        release_task(tasks + previous_task[cpuid]);
        /*
         * If this task has been the last task within the process, perform extended cleanup for the
         * process
         */
        if (0 == previous->task_count) {
            /*
             * Release all page tables
             */
            mm_release_page_tables(previous_proc[cpuid]);
            /*
             * Inform parent via signal and the condition variable unwaited in the process structure
             * that child has completed
             */
            signal_proc(procs + previous->ppid, __KSIGCHLD);
            if (0 == previous->waitable) {
                cond_broadcast(&procs[previous->ppid].unwaited);
                procs[previous->ppid].unwaited_children++;
                previous->waitable = 1;
            }
            /*
             * Clear any pending alarms we might have
             */
            do_alarm(0);
        }
        spinlock_release(&proc_table_lock, &eflags);
    }
}

/*
 * This function sets up the stack for a new task.
 * Parameter:
 * @top_of_stack: the top of the new stack
 * @base_of_stack: the base of the available stack area
 * @ir_context: an IR context which is used as blueprint for the new stack
 * @exec - a pointer to a function which is to be invoked when the task is initially run
 * @arg - an argument passed to this function
 * @esp - returns the new value of ESP which has to be set to switch to this task
 * Return value:
 * ENOMEM if there is not enough space left on the stack
 * EINVAL if the code segment is not a kernel code segment
 * 0 upon success
 * Notes:
 * When this function completes, the stack layout will be as follows
 * - at the top of the new stack, there is the argument to be passed to the function exec
 * and the address of the function pm_exit_handler()
 * - below this, a copy of the passed ir_context is placed
 * - within that ir_context, EIP is made to point to exec
 * The function will check that the space between top_of_stack and base_of_stack
 * is sufficient for this
 * Note that this function may only be used if the passed interrupt context
 * represents an interrupt generated in kernel space
 */
int pm_setup_stack(u32 top_of_stack, u32 base_of_stack,
        ir_context_t* ir_context, void* (*exec)(void*), void* arg, u32* esp) {
    ir_context_t* new_context;
    /*
     * Verify parameters
     */
    if (top_of_stack - base_of_stack + 1 < sizeof(ir_context_t) + 8) {
        ERROR("Not enough space on stack\n");
        return ENOMEM;
    }
    if (!mm_is_kernel_code(ir_context->cs_old)) {
        ERROR("Calling code is not kernel code\n");
        return EINVAL;
    }
    /*
     * Put argument and return address for exec onto stack
     */
    *((u32*) (top_of_stack - 3)) = (u32) arg;
    *((u32*) (top_of_stack - 7)) = (u32) pm_task_exit_handler;
    /*
     * Now copy ir context to stack area
     */
    new_context = (ir_context_t*) (top_of_stack - 7 - sizeof(ir_context_t));
    memcpy((void*) new_context, (void*) ir_context, sizeof(ir_context_t));
    /*
     * Finally modify EIP
     * and return esp
     */
    new_context->eip = (u32) exec;
    *esp = ((u32) (new_context)) + 8;
    return 0;
}

/*
 * Create a new task.
 * Parameter:
 * @thread - the memory location where we store the id of the new thread
 * @attr - currently unused except for attr->cpu which can be used to bind a thread to a CPU - EXPERIMENTAL
 * @start_function - the main function of the new task
 * @arg - the argument to the main function
 * @ir_context - the current ir context
 * Return value:
 * EAGAIN if there is no free task id
 * ENOMEM if there is not enough memory left for the stack of the new task
 * EPERM if the function is called from user space
 * EINVAL if one of the arguments is not valid
 * 0 upon success
 * Notes:
 * This function performs all steps necessary to create a new task, i.e.
 * - find a free task id
 * - locate a free area in the kernel stack for the new task
 * - allocate and create a new task structure
 * - set up the kernel stack
 * - store the id of the newly created thread in the buffer thread
 * - mark the new thread as ready
 */
int do_pthread_create(pthread_t* thread, pthread_attr_t* attr,
        void* (*start_function)(void*), void* arg, ir_context_t* ir_context) {
    u32 esp;
    u32 new_tos = 0;
    int pages;
    task_t* new_task;
    if (0 == start_function)
        return EINVAL;
    /*
     * Currently we only support kernel level threads, so check whether we were invoked
     * from user space
     */
    if (!mm_is_kernel_code(ir_context->cs_old))
        return EPERM;
    /*
     * Locate a free task ID
     */
    if (0 == (new_task = reserve_task())) {
        ERROR("No free task slot\n");
        return EAGAIN;
    }
    if (thread)
        *((u32*) thread) = new_task->id;
    /*
     * Reserve a free area on the kernel stack and build up a stack there
     */
    if (0 == (new_tos = mm_reserve_task_stack(new_task->id, pm_get_pid(), &pages))) {
        ERROR("No memory available for stack of task %d\n", new_task->id);
        unreserve_task(new_task);
        return ENOMEM;
    }
    if (pm_setup_stack(new_tos, new_tos + 1 - MM_PAGE_SIZE * pages, ir_context,
            start_function, arg, &esp)) {
        ERROR("Could not set up stack segment\n");
        mm_release_task_stack(new_task->id, pm_get_pid());
        unreserve_task(new_task);
        return ENOMEM;
    }
    /*
     * Fill in new task structure
     */
    init_task(new_task, tasks+pm_get_task_id(), esp);
    /*
     * Do an atomic increment of task count in process structure
     */
    atomic_incr(&(procs[pm_get_pid()].task_count));
    /*
     * Are we tied to a particular CPU?
     */
    if (attr) {
        if ((attr->cpuid < 0) || (new_task->cpuid > smp_get_cpu_count()))
            ERROR("Ignoring invalid cpuid %d\n", attr->cpuid);
        else
            new_task->cpuid = attr->cpuid;
        new_task->priority = MIN(attr->priority, SCHED_MAX_PRIO);
    }
    /*
     * Now the task is ready to run. As soon as we add it to the scheduler queue,
     * we can be preempted and the new task could be selected, so we do this only
     * at the end of this function when the task is fully initialized
     */
    start_task(new_task);
    return 0;
}

/*
 * This function is called by the SMP startup routines once for each AP. It
 * reserves a task ID for the current "flow of execution" which is going to
 * be the idle task for this CPU, and sets the task status to RUNNING
 * Parameter:
 * @cpuid - the ID of the CPU on which the new task is to be executed
 * Return value:
 * the ID of the new task
 * -EAGAIN if no free task slot could be found
 * Locks:
 * lock on newly created task
 */
int pm_create_idle_task(int cpuid) {
    u32 eflags;
    task_t* task;
    /*
     * Locate a free task ID
     */
    if (0 == (task = reserve_task())) {
        ERROR("No free task slot\n");
        return -EAGAIN;
    }
    /*
     * Initialize task so that it belongs to process 0
     */
    task->sig_blocked = 0;
    task->proc = procs;
    task->priority = 0;
    init_task(task, task, 0);
    /*
     * and mark it as idle task
     */
    task->idle = 1;
    /*
     * do_pthread_create / init_task need this to be set
     */
    task->saved_cr3 = get_cr3();
    /*
     * Make this the active task on the respective CPU
     */
    active_task[cpuid]=task->id;
    /*
      * Do an atomic increment of task count in process structure
      */
     atomic_incr(&(procs[pm_get_pid()].task_count));
     /*
      * Activate task. We do not add an entry to the scheduler queues as
      * this is done by the SMP startup code itself
      */
     activate_task(task);
     spinlock_get(&task->spinlock, &eflags);
     task->status = TASK_STATUS_RUNNING;
     spinlock_release(&task->spinlock, &eflags);
     return task->id;
}

/*
 * Execute the fork system call
 * Parameters:
 * @ir_context - the interrupt context
 * Return value:
 * the pid of the new process if the processing was successful
 * -EAGAIN if there was no free process ID or task ID
 * -ENOMEM when cloning of the address space failed
 */
int do_fork(ir_context_t* ir_context) {
    proc_t* new_proc;
    proc_t* current_proc = procs + pm_get_pid();
    task_t* new_task;
    int i;
    u32 cr3;
    /*
     * Locate a free process ID
     */
    if (0 == (new_proc = reserve_proc())) {
        ERROR("No free process slot\n");
        return -EAGAIN;
    }
    /*
     * Get free task id
     */
    if (0 == (new_task = reserve_task())) {
        ERROR("Could not create task, no free slot in task table found\n");
        unreserve_proc(new_proc);
        return -EAGAIN;
    }
    /*
     * Set this to zero so that the cloned instance will see 0 as return code
     * We need to do this before calling mm_clone as mm_clone will clone the
     * kernel stack as well including the interrupt context
     */
    ir_context->eax = 0;
    /*
     * Clone address space
     */
    if (0 == (cr3 = mm_clone(new_proc->id, new_task->id))) {
        ERROR("mm_clone not successful, rc=%d\n", cr3);
        unreserve_task(new_task);
        unreserve_proc(new_proc);
        return -ENOMEM;
    }
    /*
     * At this point, we have a task and a process table entry and a new address
     * space, so we can now start to initialize the new process
     */
    spinlock_init(&new_proc->spinlock);
    new_proc->task_count = 1;
    new_proc->utime = 0;
    new_proc->stime = 0;
    new_proc->cutime = 0;
    new_proc->cstime = 0;
    if (1 == new_proc->id) {
        new_proc->sid = 1;
        new_proc->pgid = 1;
    }
    else {
        new_proc->sid = current_proc->sid;
        new_proc->pgid = current_proc->pgid;
    }
    for (i = 0; i < __NR_OF_SIGNALS; i++) {
        new_proc->sig_actions[i] = current_proc->sig_actions[i];
    }
    new_proc->sig_pending = 0;
    new_proc->ppid = current_proc->id;
    new_proc->exit_status = 0;
    new_proc->force_exit = 0;
    cond_init(&new_proc->unwaited);
    new_proc->waitable = 0;
    new_proc->unwaited_children = 0;
    new_proc->egid = current_proc->egid;
    new_proc->euid = current_proc->euid;
    new_proc->sgid = current_proc->sgid;
    new_proc->suid = current_proc->suid;
    new_proc->gid = current_proc->gid;
    new_proc->uid = current_proc->uid;
    new_proc->exec = 0;
    new_proc->cterm = current_proc->cterm;
    /*
     * Mark entry in process table as used
     */
    activate_proc(new_proc);
    /*
     * Clone file descriptors
     */
    fs_clone(current_proc->id, new_proc->id);
    /*
     * Now clone ONLY the currently active task
     * within the process
     */
    clone_task(new_task, new_proc->id, cr3, ir_context);
    /*
     * Set the task status to running and return
     */
    start_task(new_task);
    return new_proc->id;
}

/*
 * Exit the currently running task. This is the public interface function which is
 * used by the system call layer
 * Return value:
 * -EINVAL if the currently active task is 0
 * 0 upon success
 */
int do_quit() {
    task_t* self = tasks + pm_get_task_id();
    if (1 == self->idle) {
        ERROR("Cannot quit idle task\n");
        return -EINVAL;
    }
    /*
     * Set task exit flag
     */
    atomic_store((u32*) (&self->force_exit), 1);
    return 0;
}

/*
 * Handle exit requests. This functions terminates the currently active task and - if
 * this is the last task within the process - also the currently active process. It is
 * called from the interrupt handler before returning from an interrupt
 * Return value:
 * 0 if no action was taken
 * 1 if the task was terminated
 * Locks:
 * proc_table_lock
 */
int pm_handle_exit_requests() {
    u32 eflags;
    int i;
    int my_count;
    task_t* self = tasks + pm_get_task_id();
    proc_t* proc = self->proc;
    KASSERT(TASK_SLOT_USED == self->slot_usage);
    /*
     * Do nothing if force exit flag is not set
     */
    if ((0 == self->force_exit) && (0 == proc->force_exit))
        return 0;
    /*
     * Retrieve number of tasks in the process which are not yet completed
     * Note that this is an atomic operation as we read a dword from memory only
     */
    my_count = atomic_load(&(proc->task_count));
    /*
     * See whether we are the last task within the process that has not yet executed the
     * cleanup in pm_cleanup_task. If yes, we need to cleanup the process as well. In addition,
     * we will set the PPID to one for all child processes which are still not cleaned up and
     * - if the process is a session leader - disassociate all processes in the session from their
     * controlling terminal
     */
    if (1 == my_count) {
        mm_teardown_user_area();
        fs_close_all();
        spinlock_get(&proc_table_lock, &eflags);
        for (i = 2; i < PM_MAX_PROCESS; i++) {
            if ((PROC_SLOT_USED == procs[i].slot_usage)) {
                if (procs[i].ppid == proc->id) {
                    /*
                     * We do not need to lock the process, as we are the only one left
                     * who could change the ppid
                     */
                    procs[i].ppid = 1;
                }
                /*
                 * If the current process is a session lead and we have a process in the same
                 * session, clear its controlling terminal - we can safely do this as we hold
                 * the lock on the process table
                 */
                if ((procs[i].sid == proc->sid) && (proc->sid == proc->id)) {
                    procs[i].cterm = DEVICE_NONE;
                }
            }
        }
        spinlock_release(&proc_table_lock, &eflags);
    }
    complete_task();
    return 1;
}

/*
 * Exit the currently running process
 * Parameters:
 * @status - the exit status which will be set in the process table
 */
void do_exit(int status) {
    if (0 == pm_get_pid()) {
        PANIC("Cannot exit process 0\n");
        return;
    }
    /*
     * Set exit status and process exit flag. Note that only the least significant byte is taken over
     */
    pm_schedule_exit(pm_get_pid(), (status & 0xff) << 8);
}

/****************************************************************************************
 * The next group of functions implements some of the system calls which are used to    *
 * set and get standard attributes of a process like PID, PPID, group id as well as     *
 * functions to handle process groups and sessions.                                     *
 ****************************************************************************************/

/*
 * Return the ID of the currently active task on the current CPU
 * Return value:
 * id of running task
 */
int pm_get_task_id() {
    int cpuid;
    cpuid = smp_get_cpu();
    return active_task[cpuid];
}

/*
 * Return the ID of the currently active process
 * Return value:
 * PID of the currently running process
 */
int pm_get_pid() {
    int cpuid;
    cpuid = smp_get_cpu();
    return active_proc[cpuid];
}

/*
 * System call wrapper for getpid conforming to naming conventions
 * Return value:
 * process ID of the currently executing process
 */
int do_getpid() {
    return pm_get_pid();
}

/*
 * Get the real group ID of the currently running process
 * Return value:
 * real group ID of the currently active process
 */
uid_t do_getgid() {
    uid_t gid;
    u32 eflags;
    proc_t* self = procs + pm_get_pid();
    spinlock_get(&self->spinlock, &eflags);
    gid = self->gid;
    spinlock_release(&self->spinlock, &eflags);
    return gid;
}
/*
 * Get parent process id
 * Return value:
 * parent process ID of the currently executing process
 */
int do_getppid() {
    return procs[pm_get_pid()].ppid;
}

/*
 * Scan the session of the provided process and check whether a process
 * group exists in its session, i.e. whether the group ID is the process group of any process
 * in that session
 * Parameter:
 * @pid - a PID
 * @pgrp - a process group
 * Return value:
 * 1 if there is a process within the session of pid having process group pgrp
 * 0 otherwise
 * Locks:
 * proc_table_lock
 */
int pm_pgrp_in_session(int pid, int pgrp) {
    u32 eflags;
    proc_t* proc;
    int i;
    int found = 0;
    if ((pid < 0) || (pid >= PM_MAX_PROCESS))
        return 0;
    if (PROC_SLOT_USED != procs[pid].slot_usage)
        return 0;
    spinlock_get(&proc_table_lock, &eflags);
    for (i = 1; i < PM_MAX_PROCESS; i++) {
        proc = procs + i;
        if ((PROC_SLOT_USED == proc->slot_usage) && (proc->sid
                == procs[pid].sid) && (proc->pgid == pgrp)) {
            found = 1;
            break;
        }
    }
    spinlock_release(&proc_table_lock, &eflags);
    return found;
}

/*
 * Set the process group of a process.
 * Parameter:
 * @pid - pid of process, 0 = current process.
 * @pgid - process group id, 0 = same as pid
 * Return value:
 * ESRCH if the specified pid is not that of a child or the current process
 * EINVAL if the pid or pgid is not valid
 * EPERM if the process is a session leader
 * EPERM if the process is not in the same session as the active process
 * EACCES if the process has already done an exec
 * 0 upon success
 * Locks:
 * proc_table_lock
 *
 * Notes:
 *
 * 1) a process may only change the process group id of itself or a
 * child process
 * 2) the process group into which we move the process needs to be in
 * the same session as the calling process
 */
int do_setpgid(pid_t pid, pid_t pgid) {
    u32 eflags;
    proc_t* self = procs + pm_get_pid();
    proc_t* proc;
    int i;
    proc_t* proc_in_pgrp = 0;
    /*
     * As we depend on the parent - child relationship, we
     * need to get the proc table lock. This is also necessary to avoid
     * race conditions with system calls that scan processes in the process
     * group, like kill, and with fork which checks process groups when locating
     * usable process IDs
     */
    spinlock_get(&proc_table_lock, &eflags);
    /*
     * Make sure that pid is valid and refers to an existing process
     */
    if (pid != 0) {
        if ((pid < 0) || (pid >= PM_MAX_PROCESS)) {
            spinlock_release(&proc_table_lock, &eflags);
            return EINVAL;
        }
        if (PROC_SLOT_USED != procs[pid].slot_usage) {
            spinlock_release(&proc_table_lock, &eflags);
            return ESRCH;
        }
    }
    /*
     *  Verify that pid refers to the current process or a child
     */
    if ((0 != pid) && (self->id != pid)) {
        if (procs[pid].ppid != pm_get_pid()) {
            spinlock_release(&proc_table_lock, &eflags);
            return ESRCH;
        }
    }
    /*
     * When we reach this point, we know that the pid is either 0 or matches the pid of
     * the current process or any of its childs. Next eliminate the special case 0
     */
    if (0 == pid)
        pid = pm_get_pid();
    proc = procs + pid;
    /*
     * Next we check whether the process is a session leader, i.e. its session ID equals
     * its process id
     */
    if (proc->id == proc->sid) {
        spinlock_release(&proc_table_lock, &eflags);
        return EPERM;
    }
    /*
     * Return EACCES if the process has done an exec already
     */
    if (proc->exec) {
        spinlock_release(&proc_table_lock, &eflags);
        return EACCES;
    }
    /*
     * The child must be in the same session as the calling process
     */
    if (proc->sid != self->sid) {
        spinlock_release(&proc_table_lock, &eflags);
        return EPERM;
    }
    /*
     * Next we check whether pgid refers to an existing process group. Again we eliminate
     * the special case pgid = 0 first
     */
    if (0 == pgid)
        pgid = pid;
    /*
     * Validate the process group ID.
     */
    if ((pgid < 0) || (pgid >= PM_MAX_PROCESS)) {
        spinlock_release(&proc_table_lock, &eflags);
        return EINVAL;
    }
    /*
     * Now we need to check whether the process group already exists in this session, i.e. whether there is a process
     * which has this process group id and is in the same session as proc. Identify this process and make proc_in_pgrp point to it
     */
    for (i = 1; i < PM_MAX_PROCESS; i++) {
        if (PROC_SLOT_USED == procs[i].slot_usage) {
            if ((procs[i].pgid == pgid) && (procs[i].sid == proc->sid)) {
                proc_in_pgrp = procs + i;
                break;
            }
        }
    }
    /*
     * If the process group exists within the session, we can join it. If not, we need to create a new process group and
     * place proc in it as group leader. As the standard requires that the pgid of proc needs to match the supplied argument pgid, this is
     * only possible if pgid is the pid of the selected process
     */
    if (proc_in_pgrp) {
        proc->pgid = pgid;
    }
    else {
        if (pgid != pid) {
            spinlock_release(&proc_table_lock, &eflags);
            return EPERM;
        }
        proc->pgid = pgid;
    }
    spinlock_release(&proc_table_lock, &eflags);
    return 0;
}

/*
 * Create a new session with the current process as the session leader, and set the controlling terminal
 * of the process to NONE
 * Return value:
 * 0 upon success
 * -EPERM if there is another process which would be in the same process group as the current process
 *  after completing the operation
 * -EPERM if the process is already a process group leader
 * Locks:
 * process table lock
 */
int do_setsid() {
    u32 eflags;
    int i;
    int pgid_used = 0;
    proc_t* self = procs + pm_get_pid();
    /*
     * Get lock on proc table to avoid races with other functions which browse or manipulate the
     * process group structure
     */
    spinlock_get(&proc_table_lock, &eflags);
    /*
     * If the current process is not a process group lead, set session ID and process group ID,
     * otherwise return - this is a shortcut, as the check further below would also detect this case
     */
    if (self->pgid != self->id) {
        /*
         * We want to create a new process group by setting the pgid of self to its pid. For this
         * we need to make sure that there is no other process with that pgid
         */
        for (i = 1; i < PM_MAX_PROCESS; i++) {
            if (PROC_SLOT_USED == procs[i].slot_usage) {
                if (procs[i].pgid == self->id)  {
                    pgid_used = 1;
                    break;
                }
            }
        }
        if (pgid_used) {
            spinlock_release(&proc_table_lock, &eflags);
            return -EPERM;
        }
        /*
         * Set process group ID and session ID to PID which will create a new session
         * and a new process group
         */
        self->pgid = self->id;
        self->sid = self->id;
        /*
         * clear controlling terminal
         */
        self->cterm = DEVICE_NONE;
    }
    else {
        spinlock_release(&proc_table_lock, &eflags);
        return -EPERM;
    }
    spinlock_release(&proc_table_lock, &eflags);
    return 0;
}

/*
 * Get the session ID of the specified process. If the argument is 0, the session ID
 * of the caller will be returned
 * Return value:
 * session ID upon success
 * -EINVAL if the process ID pid is not valid
 * -ESRCH if there is no process with that pid
 */
pid_t do_getsid(pid_t pid) {
    u32 eflags;
    pid_t sid;
    proc_t* self = procs + pm_get_pid();
    /*
     * Get lock on proc table to avoid races with other functions which browse or manipulate the
     * process group structure
     */
    spinlock_get(&proc_table_lock, &eflags);
    /*
     * If pid is not 0, return SID of that process
     */
    if (pid) {
        if ((pid < 0) || (pid >= PM_MAX_PROCESS)) {
            spinlock_release(&proc_table_lock, &eflags);
            return -EINVAL;
        }
        if (PROC_SLOT_USED == procs[pid].slot_usage) {
            sid = procs[pid].sid;
        }
        else {
            spinlock_release(&proc_table_lock, &eflags);
            return -ESRCH;
        }
    }
    /*
     * otherwise return SID of calling process
     */
    else {
        sid = self->sid;
    }
    spinlock_release(&proc_table_lock, &eflags);
    return sid;
}

/*
 * Get the process group of the currently active process
 * Return value:
 * the current process group
 * Locks:
 * process table lock
 */
pid_t do_getpgrp() {
    pid_t pgid;
    u32 eflags;
    proc_t* self = procs + pm_get_pid();
    /*
     * Get process table lock
     */
    spinlock_get(&proc_table_lock, &eflags);
    pgid = self->pgid;
    spinlock_release(&proc_table_lock, &eflags);
    return pgid;
}



/*
 * Make the calling process a process group leader
 * Return value:
 * the new process group id
 * Locks:
 * proc_table_lock
 */
pid_t do_setpgrp() {
    u32 eflags;
    proc_t* self = procs + pm_get_pid();
    pid_t pgid;
    /*
     * Need to get proc table lock - see comments for setpgid
     */
    spinlock_get(&proc_table_lock, &eflags);
    /*
     * Are we a session leader? If yes, do nothing. If no,
     * adjust process group id
     */
    if (self->sid != self->id) {
        self->pgid = self->id;
    }
    pgid = self->pgid;
    spinlock_release(&proc_table_lock, &eflags);
    return pgid;
}

/*
 * Attach the calling process to a terminal so that the terminal becomes
 * the controlling terminal of the process, and set the foreground process
 * group of the terminal to the process group of the process.
 * No action is performed if the calling process is not a session leader or
 * if the terminal is already the controlling terminal of another session or if the
 * process has already a controlling terminal
 * Parameter:
 * @tty - the TTY
 * Locks:
 * lock on process table
 */
void pm_attach_tty(dev_t tty) {
    u32 eflags;
    int i;
    proc_t* self = procs + pm_get_pid();
    /*
     * The controlling terminal is protected by the
     * proc_table_lock
     */
    spinlock_get(&proc_table_lock, &eflags);
    /*
     * If there is no controlling terminal yet and the process is a session
     * leader, set controlling terminal - but ignore request if the terminal is
     * already the controlling terminal of a different session
     */
    if ((DEVICE_NONE == self->cterm) && (self->sid == self->id)) {
        for (i = 0; i < PM_MAX_PROCESS; i++) {
            if ((procs[i].cterm == tty) && (procs[i].sid != self->sid)
                    && (procs[i].sid == i) && (PROC_SLOT_USED == procs[i].slot_usage)) {
                /*
                 * There is a process in a different session which owns the terminal - return
                 */
                spinlock_release(&proc_table_lock, &eflags);
                return;
            }
        }
        self->cterm = tty;
        /*
         * Also set the process group of the terminal to the process group of
         * the process
         */
        tty_setpgrp(MINOR(tty), self->pgid);
    }
    spinlock_release(&proc_table_lock, &eflags);
}

/*
 * Return the controlling terminal of a process
 */
dev_t pm_get_cterm() {
    dev_t res;
    u32 eflags;
    spinlock_get(&proc_table_lock, &eflags);
    res = procs[pm_get_pid()].cterm;
    spinlock_release(&proc_table_lock, &eflags);
    return res;
}

/*
 * Set the effective user ID of a process
 * Parameter:
 * @euid - new effective user ID
 * Return value:
 * 0 upon successful completion
 * EPERM - not running as super user and the euid does not match the real uid or the saved set-user-id
 * Locks:
 * lock on process structure
 *
 * Notes:
 *
 * If the process is running with effective user ID 0, it will be able to set the effective
 * user ID to any value
 *
 * All other processes can only set the effective user ID to the real user id or the saved set-user-id
 *
 */
int do_seteuid(uid_t euid) {
    u32 eflags;
    int rc = 0;
    proc_t* self = procs + pm_get_pid();
    spinlock_get(&self->spinlock, &eflags);
    if (0 == self->euid) {
        self->euid = euid;
    }
    else {
        if ((euid == self->uid) || (euid == self->suid)) {
            self->euid = euid;
        }
        else {
            rc = EPERM;
        }
    }
    spinlock_release(&self->spinlock, &eflags);
    return rc;
}

/*
 * Get the effective user ID of the currently running process
 * Return value:
 * the effective user ID
 * Locks:
 * lock on process structure
 */
uid_t do_geteuid() {
    u32 eflags;
    uid_t euid;
    proc_t* self = procs + pm_get_pid();
    spinlock_get(&self->spinlock, &eflags);
    euid = self->euid;
    spinlock_release(&self->spinlock, &eflags);
    return euid;
}

/*
 * Set the user IDs of the currently running process.
 * Parameter:
 * @uid - the new user ID
 * Return value:
 * 0 upon success
 * EPERM if the process is not running with effective user ID 0 and the argument does not match euid or suid
 * Locks:
 * lock on process structure
 *
 * Notes:
 * A call of this function will set the real user ID, the effective user ID and the saved set-user-ID
 * to the provided value, given that the effective user ID at the time of invocation is zero.
 *
 * Otherwise, the effective user ID is set to the provided value if it matches the
 * real user ID or the saved set-user-id
 *
 */
int do_setuid(uid_t uid) {
    u32 eflags;
    int rc = 0;
    proc_t* self = procs + pm_get_pid();
    spinlock_get(&self->spinlock, &eflags);
    if (0 == self->euid) {
        self->euid = uid;
        self->uid = uid;
        self->suid = uid;
    }
    else {
        if ((uid == self->uid) || (uid == self->suid)) {
            self->euid = uid;
        }
        else {
            rc = EPERM;
        }
    }
    spinlock_release(&self->spinlock, &eflags);
    return rc;
}

/*
 * Get the user ID of the currently running process
 * Return value:
 * real user ID of the currently active process
 * Locks:
 * lock on process structure
 */
uid_t do_getuid() {
    u32 eflags;
    uid_t uid;
    proc_t* self = procs + pm_get_pid();
    spinlock_get(&self->spinlock, &eflags);
    uid = self->uid;
    spinlock_release(&self->spinlock, &eflags);
    return uid;
}

/*
 * Get the effective group ID of the currently running process
 * Return value:
 * effective group ID of the currently active process
 * Locks:
 * lock on process structure
 */
uid_t do_getegid() {
    u32 eflags;
    uid_t egid;
    proc_t* self = procs + pm_get_pid();
    spinlock_get(&self->spinlock, &eflags);
    egid = self->egid;
    spinlock_release(&self->spinlock, &eflags);
    return egid;
}

/****************************************************************************************
 * The following functions implement program execution                                  *
 ****************************************************************************************/

/*
 * This function sets up a user space stack for program execution.
 *
 * Parameters:
 * @user_space_stack - initial stack pointer
 * @argv - array holding the arguments, last element should be 0 (argv may be zero)
 * @env - array holding the environment strings, last element should be 0 (env may be zero)
 * @stack_size - available space on stack
 * Return value:
 * E2BIG - the total size of all arguments exceeds stack size
 * 0 upon success
 *
 * After this function has been executed, the stack area designated by the parameter @user_space_stack
 * will look as follows:
 *
 *  --------->       First argument + trailing zero
 *  |                Second argument + trailing zero
 *  |                .
 *  |                .
 *  |                .
 *  |                Last argument + trailing zero   <-----
 *  |                First environment string + zero <--- | ---|
 *  |                .                                    |    |
 *  |                .                                    |    |
 *  |                .                                    |    |
 *  |                Last env. string + trailing zero     |    |
 *  |                0                                    |    |
 *  |                argv[argc-1]   -----------------------    |
 *  |                .                                         |
 *  |                .                                         |
 *  -----------      argv[0]         <-------------------      |
 *                   0                                  |      |
 *                   env[envc-1]                        |      |
 *                   .                                  |      |
 *                   .                                  |      |
 *                   .                                  |      |
 *            -----> env[0]  -----------------------------------
 *            |                                         |
 *            ---- - env                                |
 *                   argv             -------------------
 *                   argc
 *                   Return address
 *

 */
static int setup_user_stack(u32* user_space_stack, char** argv, char** env,
        u32 stack_size) {
    int argc = 0;
    int envc = 0;
    u32* arg_pointers;
    u32* env_pointers;
    char* last_arg;
    char* last_env_string;
    int arg_len = 0;
    int i;
    u32 lowest_byte = *user_space_stack - stack_size + 1;
    /*
     * We walk through the list of arguments first and copy all arguments
     * to the stack, followed by a trailing zero. We start with the first
     * argument at the highest address, then decrease the stack pointer,
     * copy the second argument to the stack and so forth
     */
    if (argv) {
        while (argv[argc]) {
            arg_len = strlen(argv[argc]);
            (*user_space_stack) -= (arg_len + 1);
            if ((*user_space_stack) < lowest_byte) {
                ERROR("Stack size exhausted\n");    
                return E2BIG;
            }
            memset((void*) (*user_space_stack), 0, arg_len+1);
            memcpy((void*) (*user_space_stack), (void*) argv[argc], arg_len);
            argc++;
        }
    }
    /*
     * Save pointer to last string which we have saved
     */
    last_arg = (char*) (*user_space_stack);
    /*
     * Repeat the same procedure with the environment strings
     */
    if (env) {
        while (env[envc]) {
            arg_len = strlen(env[envc]);
            (*user_space_stack) -= (arg_len + 1);
            if (*user_space_stack < lowest_byte)
                return E2BIG;
            memset((void*) (*user_space_stack), 0, arg_len+1);
            memcpy((void*) (*user_space_stack), (void*) env[envc], arg_len);
            envc++;
        }
    }
    /*
     * Save pointer to last environment string which we have saved
     */
    last_env_string = (char*) (*user_space_stack);
    /*
     * Re-align to a dword boundary
     */
    (*user_space_stack) = ((*user_space_stack) / 4) * 4;
    if (*user_space_stack < lowest_byte)
        return E2BIG;
    /*
     * Next we place a zero on the stack - this is going to be argv[argc] from the point
     * of view of the main(int argc, char** argv) function which we call later
     */
    (*user_space_stack) -= 4;
    if (*user_space_stack < lowest_byte)
        return E2BIG;
    *((u32*) (*user_space_stack)) = 0;
    /*
     * We need space for the argv array, i.e. for
     * argc double words, each pointing to an argument
     */
    (*user_space_stack) -= sizeof(u32) * argc;
    if (*user_space_stack < lowest_byte)
        return E2BIG;
    /*
     * Now we place argv[0] .. argv[argc-1] on the stack.
     * We first put argv[argc-1] on the stack which points
     * to last_arg, then increase last_arg so that it points
     * to the next argument on the stack and so forth
     */
    arg_pointers = (u32*) (*user_space_stack);
    for (i = argc - 1; i >= 0; i--) {
        arg_pointers[i] = (u32) last_arg;
        last_arg += strlen(last_arg);
        last_arg++;
    }
    /*
     * Place another zero on the stack - this terminates the env array
     */
    (*user_space_stack) -= 4;
    if (*user_space_stack < lowest_byte)
        return E2BIG;
    *((u32*) (*user_space_stack)) = 0;
    /*
     * We need space for the env array, i.e. for
     * envc double words, each pointing to an argument
     */
    (*user_space_stack) -= sizeof(u32) * envc;
    if (*user_space_stack < lowest_byte)
        return E2BIG;
    /*
     * We now put env[envc-1] on the stack which points
     * to last_env_string, then increase last_env_string so that it points
     * to the next argument on the stack and so forth
     */
    env_pointers = (u32*) (*user_space_stack);
    for (i = envc - 1; i >= 0; i--) {
        env_pointers[i] = (u32) last_env_string;
        last_env_string += strlen(last_env_string);
        last_env_string++;
    }
    /*
     * Place envp
     */
    (*user_space_stack) -= 4;
    if (*user_space_stack < lowest_byte)
        return E2BIG;
    *((u32*) (*user_space_stack)) = (u32) env_pointers;
    /*
     * Place argv
     */
    (*user_space_stack) -= 4;
    if (*user_space_stack < lowest_byte)
        return E2BIG;
    *((u32*) (*user_space_stack)) = (u32) arg_pointers;
    /*
     * Place argc
     */
    (*user_space_stack) -= 4;
    if (*user_space_stack < lowest_byte)
        return E2BIG;
    *((u32*) (*user_space_stack)) = argc;
    /*
     * Finally place return address on stack
     */
    (*user_space_stack) -= 4;
    if (*user_space_stack < lowest_byte)
        return E2BIG;
    *((u32*) (*user_space_stack)) = 0;
    return 0;
}

/*
 * This utility function will clone an array of strings,
 * i.e. a null-terminated array of char-pointers. The needed
 * memory will be allocated using kmalloc
 */
static char** clone_string_list(char** list) {
    int entries;
    char** new_list;
    if (0 == list)
        return 0;
    /*
     * First determine number of entries,
     * excluding the terminating zero
     */
    entries = 0;
    while (list[entries]) {
        entries++;
    }
    /*
     * Allocate memory needed for that, including one slot
     * for the trailing zero
     */
    new_list = (char**) kmalloc((entries+1)*sizeof(char*));
    if (0 == new_list) {
        ERROR("Could not allocate memory for table copy\n");
        return 0;
    }
    /*
     * Now allocate and copy the individual strings
     */
    for (int i = 0; i < entries; i++) {
        new_list[i] = kmalloc(strlen(list[i])+1);
        if (0 == new_list[i]) {
            for (int j=0; j < i; j++) {
                kfree(new_list[j]);
            }
            kfree(new_list);
            ERROR("Could not allocate memory for table copy\n");
            return 0;
        }
        strcpy(new_list[i], list[i]);
    }
    /*
     * Finally do the trailing zero
     */
    new_list[entries] = 0;
    return new_list;
}

/*
 * Free all entries in a string list
 * and the list itself
 */
static void free_string_list(char** list) {
    if (0 == list)
        return;
    int entries = 0;
    while (list[entries]) {
        kfree(list[entries]);
        entries++;
    }
    kfree(list);
}

/*
 * Load a program and execute it.
 * Parameter:
 * @path - name of ELF executable to run
 * @argv - array of argument strings, last element in array should be 0. If argv==0, no arguments are assumed
 * @ir_context - current IR context or zero if function is executed outside of an IR context
 * Return value:
 * 0 upon success
 * ENOEXEC if the validation of the executable failed
 * E2BIG if the total size of all arguments exceeds the stack size
 * ENOMEM if there is not enough memory available
 *
 */
int do_exec(char* path, char** argv, char** envp, ir_context_t* ir_context) {
    u32 user_space_stack;
    u32 entry_point;
    task_t* self = tasks + pm_get_task_id();
    proc_t* proc = procs + pm_get_pid();
    int i;
    struct __ctOS_stat mystat;
    char mypath[PATH_MAX+2];
    char** myenv = 0;
    char** myargv = 0;
    if (0 == path)
        return EINVAL;
    /*
     * Stat file
     */
    if (do_stat(path, &mystat)) {
        return ENOEXEC;
    }
    /*
     * If the path name is exceeding the maximum return an error
     */
    if (strlen(path) > PATH_MAX) {
        ERROR("Path name too long\n");
        return E2BIG;
    }
    /*
     * Validate executable to do some basic checks before
     * changing anything in the memory space of our process
     */
    if (elf_load_executable(path, &entry_point, 1)) {
        return ENOEXEC;
    }
    /*
     * Kill all other tasks in the current process - this is only done if an IR context is provided
     */
    if (ir_context) {
        proc->force_exit = 1;
        /*
         * Wait until all other tasks have completed their exit processing, including clean-up
         * of their part of the kernel stack, then reset flag again to avoid that this task is
         * terminated as well when we complete the system call. Note that when we are interrupted here,
         * no exit processing will be invoked for this task as we are already on the system call level
         */
        while (atomic_load(&(proc->task_count)) > 1) {
            sched_yield();
            reschedule();
        }
        proc->force_exit = 0;
    }
    /*
     * Close all file descriptors for which FD_CLOEXEC is specified
     */
    fs_on_exec(proc->id);
    /*
     * Set up user space memory layout. Note that this will return the top
     * of the new user space stack. As we will modify this a few lines below, we
     * must, starting at this point, no longer trust any data that is located in
     * the user space - this implies in particular to our path variables, the environment
     * and the arguments as we do not know where the userspace program calling us
     * has placed this data. 
     */
    strcpy(mypath, path);
    if (envp) {
        if (0 == (myenv = clone_string_list(envp))) {
            ERROR("No memory for environment copy\n");
            return ENOMEM;
        }
    }
    if (argv) {
        if (0 == (myargv = clone_string_list(argv))) {
            ERROR("No memory for argv copy\n");
            free_string_list(myenv);
            return ENOMEM;
        }
    }
    if (0 == (user_space_stack = mm_init_user_area())) {
        ERROR("Could not prepare user space for program execution\n");
        proc->force_exit = 1;
        return ENOMEM;
    }
    /*
     * Fill user space stack with arguments. We allow only one page for
     * all arguments in total.
     * NOTE: this only works because mm_init_user_area only maps new pages,
     * but does not modify any pages or remove pages. Otherwise there would be
     * the danger that the arguments which are located somewhere in the user pages
     * have already been overwritten when we call setup_user_stack or - even worse - removed
     * so that we would produce a page fault
     */
    if (setup_user_stack(&user_space_stack, myargv, myenv, MM_PAGE_SIZE)) {
        ERROR("Stack area not sufficient for arguments\n");
        return E2BIG;
    }
    free_string_list(myargv);
    free_string_list(myenv);
    /*
     * Set all user supplied signal handler back to default. We do not need a lock
     * here as all other tasks are gone already
     */
    for (i = 0; i < __NR_OF_SIGNALS; i++) {
        if ((proc->sig_actions[i].sa_handler != __KSIG_DFL)
                && (proc->sig_actions[i].sa_handler != __KSIG_IGN)) {
            proc->sig_actions[i].sa_handler = __KSIG_DFL;
        }
    }
    /*
     * Load executable. If that fails, return but set exit flag
     * so that we will never return to user space
     */
    if (elf_load_executable(mypath, &entry_point, 0)) {
        ERROR("Could not load executable %s\n", path);
        proc->force_exit = 1;
        return ENOEXEC;
    }
    /*
     * If SUID bit is set, set effective user ID of process
     */
    if (S_ISUID & mystat.st_mode) {
        proc->euid = mystat.st_uid;
    }
    /*
     * If SGID bit is set, set effective group ID of process
     */
    if (S_ISGID & mystat.st_mode) {
        proc->egid = mystat.st_gid;
    }
    if (0 == ir_context) {
        self->execution_level = EXECUTION_LEVEL_USER;
        goto_ring3(entry_point, user_space_stack);
    }
    else {
        /*
         * We manipulate the IR context such that when we return to user space,
         * we continue execution at the entry point with the correct stack pointer
         */
        if (mm_is_kernel_code(ir_context->cs_old)) {
            ERROR("Cannot run execve from interrupt context with kernel CS\n");
            procs->force_exit = 1;
            return ENOEXEC;
        }
        ir_context->cs_old = (SELECTOR_CODE_USER / 8) * 8 + 3;
        ir_context->ds = (SELECTOR_DATA_USER / 8) * 8 + 3;
        ir_context->eip = entry_point;
        /*
         * Above ir_context, we have stored the old ESP and SS
         * as two dwords
         */
        *(&(ir_context->eflags) + 1) = user_space_stack;
        *(&(ir_context->eflags) + 2) = (SELECTOR_STACK_USER / 8) * 8 + 3;
    }
    /*
     * Register the fact that a task has successfully executed exec in the process table,
     * this is needed by setpgid
     */
    proc->exec = 1;
    return 0;
}

/****************************************************************************************
 * These functions deal with CPU time accounting information                            *
 ****************************************************************************************/

/*
 * Update accounting information for currently active task
 * and trigger work queue processing
 * Parameters:
 * @exec_level - the previous execution level
 */
void pm_do_tick(ir_context_t* ir_context, int cpuid) {
    task_t* current_task = tasks + active_task[cpuid];
    proc_t* self = current_task->proc;
    atomic_incr(&current_task->ticks);
    /*
     * Update accounting information of current process. We use
     * the code segment stored in the interrupt context, i.e. the execution level from which
     * the timer interrupt was generated, to distinguish between kernel time
     * and user time
     */
    if (mm_is_kernel_code(ir_context->cs_old)) {
        atomic_incr((reg_t*) &self->stime);
    }
    else {
        atomic_incr((reg_t*) &self->utime);
    }
    /*
     * Trigger work queue processing
     */
    wq_do_tick(cpuid);
}

/*
 * Add accounting information from a child to the accounting information of a process. This function is used by the
 * waitpid system call to carry over CPU usage from a child which is completed to the parent
 * Parameter:
 * @self - the parent
 * @child - the child
 * Locks:
 * lock on parent process
 */
static void add_child_times(proc_t* self, proc_t* child) {
    u32 eflags;
    spinlock_get(&self->spinlock, &eflags);
    self->cstime += child->cstime + child->stime;
    self->cutime += child->cutime + child->utime;
    spinlock_release(&self->spinlock, &eflags);
}


/*
 * Get CPU accounting information of current process
 * Parameter:
 * @times - this is where we store the accounting information
 * Return value:
 * number of ticks passed since system boot
 * Locks:
 * spinlock on process
 */
int do_times(struct __ktms* times) {
    u32 eflags;
    proc_t* self = procs + pm_get_pid();
    spinlock_get(&self->spinlock, &eflags);
    if (times) {
        times->tms_cstime = self->cstime;
        times->tms_cutime = self->cutime;
        times->tms_stime = self->stime;
        times->tms_utime = self->utime;
    }
    spinlock_release(&self->spinlock, &eflags);
    return timer_get_ticks();
}

/****************************************************************************************
 * Wait for completion of a process (waitpid)                                           *
 ****************************************************************************************/

/*
 * Utility function for waitpid. This function checks whether a provided PID matches
 * the filter passed as first argument to the waitpid call, following the semantic of the
 * waitpid system call, i.e.
 * filter = positive integer x matches the process with pid x given that it is a child of self
 * filter = 0 matches all child processes in the same process group
 * filter = -1 matches all childs
 * filter = any other negative integer x matches all childs in process group -x
 * Parameter:
 * @filter - filter
 * @self - PID of own process, only children of that process are considered
 * @pid - pid of process to check
 * Return value:
 * 1 - match
 * 0 - no match
 */
static int is_match(int filter, int self, int pid) {
    if (filter > 0) {
        if ((PROC_SLOT_USED == procs[filter].slot_usage) && (procs[filter].ppid
                == self)) {
            return (pid == filter);
        }
    }
    else {
        if ((PROC_SLOT_USED == procs[pid].slot_usage) && (procs[pid].ppid
                == self)) {
            if (((pid_t) -1) == filter) {
                return 1;
            }
            if ((0 == filter) && (procs[pid].pgid == procs[self].pgid)) {
                return 1;
            }
            if ((filter < 0) && (procs[pid].pgid == ((-1) * filter))) {
                return 1;
            }
        }
    }
    return 0;
}

/*
 * Wait for one or more processes and retrieve the exit status.
 * Parameters:
 * @pid - specifies the child for which we request status information.
 * - ((pid_t) -1) --> request status information for any child
 * - a positive value --> request status information for the process with that pid
 * - 0 --> request status information for any process in the same process group as the currently running process
 * - a negative value -pgid --> request status for all processes in the process group pgid
 * @stat_loc - the status of the process will be stored here
 * @options - options (any combination of WUNTRACED and WNOHANG)
 * @ru - a pointer to an rusage structure as defined in resources.h into which the
 * CPU time consumed by the process is written
 * Return value:
 * the pid of the child for which status is reported
 * 0 if no child has been waiting but WNOHANG was specified
 * -EINVAL if the arguments are not valid
 * -ECHILD if no matching child could be found
 * -EPAUSE if the operation was interrupted by a signal
 * Locks:
 * proc_table_lock
 * Cross-monitor function calls:
 * add_child_times
 * discard_signal
 *
 */
pid_t do_waitpid(pid_t pid, int* stat_loc, int options, struct rusage* ru) {
    int child_pid = -1;
    proc_t* self = procs + pm_get_pid();
    proc_t* child;
    int i;
    int rc;
    int wuntraced = options & __WUNTRACED;
    int wnohang = options & __WNOHANG;
    u32 eflags;
    int match;
    int intr = 0;
    /*
     * Get lock on process table
     */
    spinlock_get(&proc_table_lock, &eflags);
    /*
     * Start to loop
     * until information is there or we exit because WNOHANG has been given
     */
    while (-1 == child_pid) {
        /*
         * Loop through all specified child processes. We need to check whether the process we are looking for
         * exists again because it might have been removed while we were sleeping. When an exact child has been specified,
         * i.e. if pid > 0, do only one iteration of the loop targeting this PID
         */
        match = 0;
        for (i = ((pid > 0) ? pid : 0); i < ((pid > 0) ? (pid + 1): PM_MAX_PROCESS); i++) {
            if (is_match(pid, self->id, i)) {
                match = 1;
                child = procs + i;
                /*
                 * If the child has the waitable flag set and has either exited normally or exited due to a signal
                 * or has been stopped and WUNTRACED is set, get status and mark child as processed. Also update accounting
                 * information in own process and fill rusage argument
                 */
                if (1 == child->waitable) {
                    if (__WIFEXITED(child->exit_status)
                            || __WIFSIGNALED(child->exit_status) || (wuntraced)) {
                        *stat_loc = child->exit_status;
                        child->waitable = 0;
                        self->unwaited_children--;
                        child_pid = i;
                        add_child_times(self, child);
                        if (ru) {
                            ru->ru_utime.tv_sec = child->utime / HZ;
                            ru->ru_utime.tv_usec = 0;
                            ru->ru_stime.tv_sec = child->stime / HZ;
                            ru->ru_stime.tv_usec = 0;
                        }
                    }
                }
                if (-1 != child_pid)
                    break;
            }
        }
        /*
         * Remove SIGCHLD from pending signal mask of the currently active process and all its threads
         * if we have found a child (i.e. will return now) and no other childs have pending status information
         */
        if ((-1 != child_pid) && (0 == self->unwaited_children)) {
            discard_signal(self, 1 << __KSIGCHLD, 0);
        }
        /*
         * No process matching the filter seems to exist any more
         */
        if (0 == match) {
            spinlock_release(&proc_table_lock, &eflags);
            return -ECHILD;
        }
        /*
         * If we have not found a child which has waitable information, wait until condition variable unwaited fires
         * or return if WNOHANG has been specified
         * When we return from wait with return code zero, we also hold the proc table lock again
         */
        if (-1 == child_pid) {
            /*
             * If intr is set from previous loop iteration, return
             */
            if (1 == intr) {
                spinlock_release(&proc_table_lock, &eflags);
                return -EPAUSE;
            }
            if (wnohang) {
                spinlock_release(&proc_table_lock, &eflags);
                return 0;
            }
            rc = cond_wait_intr(&self->unwaited, &proc_table_lock, &eflags);
            /*
             * If rc == -1, we have been interrupted by a signal. We need to get the proc table lock
             * again in this case as cond_wait did not acquire it
             */
            if (-1 == rc) {
                intr = 1;
                spinlock_get(&proc_table_lock, &eflags);
            }
        }
    }
    /*
     * If we got to this point, we have retrieved status information from child child_pid. We
     * now invalidate the process table entry.
     */
    if (0 == atomic_load(&procs[child_pid].task_count)) {
        procs[child_pid].id = 0;
        procs[child_pid].slot_usage = PROC_SLOT_FREE;
    }
    spinlock_release(&proc_table_lock, &eflags);
    return child_pid;
}

/****************************************************************************************
 * The following functions deal with signal processing on the level of an individual    *
 * task                                                                                 *
 ****************************************************************************************/

/*
 * Remove a signal from the pending signal bitmask of a task
 * Parameter:
 * @task - the task
 * @sigmask - bit mask of all signals which are to be cleared
 * Locks:
 * task lock
 */
static void discard_signal_task(task_t* task, u32 sigmask) {
    u32 eflags;
    spinlock_get(&task->spinlock, &eflags);
    task->sig_pending &= ~sigmask;
    spinlock_release(&task->spinlock, &eflags);
}

/*
 * Get the pending signal bitmask for a specific task
 * Parameter:
 * @task - the task
 * Return value:
 * the bitmask of signals pending on task level
 * Locks:
 * spinlock on task
 */
static u32 get_signals_task(task_t* task) {
    u32 eflags;
    u32 bitmask;
    /*
     * Get lock even though this is an atomic read to make sure that we get a consistent state even
     * if another function changes the bitmask bit-by-bit
     */
    spinlock_get(&task->spinlock, &eflags);
    bitmask = task->sig_pending;
    spinlock_release(&task->spinlock, &eflags);
    return bitmask;
}

/*
 * Send a signal to an individual task within a process. Unless overridden using the
 * parameter @force, this function will not mark the signal as pending
 * on thread level if it is blocked and the task is not waiting for it
 * Parameter:
 * @task - the task
 * @sig_no - signal number
 * @force - set this to one to force delivery
 * Return value:
 * 0 if signal could be send because the signal is not blocked or the task is waiting for it using sigwait
 * 1 if signal could not be send
 * Locks:
 * task->spinlock for target task
 */
static int signal_task(task_t* task, int sig_no, int force) {
    int ign_sign;
    u32 eflags;
    u32 handler;
    u32 sig_mask = (1 << sig_no);
    /*
     * Check whether the signal is currently ignored. Note that we do NOT get the process lock
     * while reading from the sigaction structure, as this would avoid our locking strategy
     * and lead to deadlocks. However, the read of the pointer to the handler is atomic. Thus if another
     * thread sets a signal to IGNORE and the target task is sleeping in an interruptible sleep, we will
     * mistakenly wake it up further below. Strictly speaking, this is a violation of the POSIX standard
     * which says in section 2.4.4 of the chapter on signal concepts:
     * "Signals that are ignored shall not affect the behavior of any function", but this specification is
     * inherently not thread-safe anyway
     */
    ign_sign = 0;
    handler = atomic_load((u32*) &task->proc->sig_actions[sig_no].sa_handler);
    if ((u32) __KSIG_IGN == handler) {
        ign_sign = 1;
    }
    if ((u32) __KSIG_DFL == handler) {
        if ( SIG_DFL_IGN == get_default_action(sig_no))
            ign_sign = 1;
    }
    /*
     * Now get lock on task
     */
    spinlock_get(&task->spinlock, &eflags);
    /*
     * If the task is already completed return immediately
     */
    if (TASK_STATUS_DONE == task->status) {
        spinlock_release(&task->spinlock, &eflags);
        return 0;
    }
    /*
     * It is not possible to signal an idle task
     */
    if (task->idle) {
        spinlock_release(&task->spinlock, &eflags);
        return 0;
    }
    /*
     * If the signal is blocked and the task is not in a sigwait
     * for the signal to be delivered, return immediately unless the force flag is set
     */
    if ((sig_mask & task->sig_blocked) && (!(sig_mask & task->sig_waiting))) {
        if (!force) {
            spinlock_release(&task->spinlock, &eflags);
            return 0;
        }
    }
    if (!(sig_mask & task->sig_pending)) {
        /*
         * Mark signal as pending
         */
        task->sig_pending |= sig_mask;
        /*
         * If we are in an interruptible sleep and the signal is not blocked
         * or we are waiting for the signal and the task is blocked
         * then wakeup task.
         * Make an exception when the signal is ignored
         */
        if (((TASK_STATUS_BLOCKED_INTR == task->status) && (!(sig_mask
                & task->sig_blocked)))
                || ((TASK_STATUS_BLOCKED == task->status) && (sig_mask
                        & task->sig_waiting))) {
            if (0 == ign_sign) {
                if (TASK_STATUS_BLOCKED_INTR == task->status)
                    task->intr = 1;
                run_task(task);
            }
        }
    }
    spinlock_release(&task->spinlock, &eflags);
    return 1;
}

/*
 * Send a signal to a thread. It is not possible to signal task 0
 * Parameter:
 * @task_id - id of the task
 * @sig_no - signal number
 * Return value:
 * 0 upon success
 * -ESRCH if the task id is not valid
 */
int do_pthread_kill(u32 task_id, int sig_no) {
    task_t* task;
    task = get_task(task_id);
    if (0 == task) {
        return -ESRCH;
    }
    /*
     * Send signal
     */
    signal_task(task, sig_no, 1);
    /*
     * Drop reference on task again
     */
    release_task(task);
    return 0;
}

/*
 * Continue a task that has received SIGCONT or SIGKILL
 * Parameters:
 * @task - the task to be continued
 * @sig_no - number of the signal which is the reason for the call
 * Locks:
 * spinlock on task
 */
static void continue_task(task_t* task, int sig_no) {
    u32 eflags;
    u32 stop_signals = (1 << __KSIGSTOP) | (1 << __KSIGTTIN)
            | (1 << __KSIGTTOU) | (1 << __KSIGTSTP);
    spinlock_get(&task->spinlock, &eflags);
    if (__KSIGCONT == sig_no) {
        task->sig_pending &= ~stop_signals;
    }
    if (TASK_STATUS_STOPPED == task->status) {
        run_task(task);
    }
    spinlock_release(&task->spinlock, &eflags);
}

/*
 * Complete a signal handler. This function implements the sigreturn system call
 * which each signal handler needs to execute at completion. It will restore the original
 * stack and signal mask as well as the old interrupt context and FPU state
 * Parameters:
 * @sig_no - signal number
 * @sig_frame - saved registers
 * @ir_context - current IR context which will be overwritten with the interrupt
 * context which was saved before the signal handler was entered
 * Return value:
 * EAX of old interrupt context
 * Locks:
 * spinlock on task
 */
int do_sigreturn(int sig_no, sig_frame_t* sigframe, ir_context_t* ir_context) {
    task_t* task = tasks + pm_get_task_id();
    u32 fpu_save_area;
    u32 eflags;
    u32 sv_only_bits;
    spinlock_get(&task->spinlock, &eflags);
    /*
     * Restore old userland stack pointer
     */
    u32 new_esp = sigframe->ring0_esp;
    *((&(ir_context->eflags)) + 1) = new_esp;
    /*
     * Restore signal mask
     */
    task->sig_blocked = sigframe->sigmask;
    /*
     * Restore old IR context from sigframe structure on user space stack
     */
    ir_context->eax = sigframe->eax;
    ir_context->ebx = sigframe->ebx;
    ir_context->ecx = sigframe->ecx;
    ir_context->edx = sigframe->edx;
    ir_context->ebp = sigframe->ebp;
    ir_context->esp = sigframe->esp;
    ir_context->esi = sigframe->esi;
    ir_context->edi = sigframe->edi;
    ir_context->eip = sigframe->eip;
    /*
     * Restore FPU state. Again we have to clear TS and set the FPU bit in the
     * current task to be able to do this. Recall that we have saved the FPU
     * state at the first byte within the reserved area in the sigframe structure
     * which is 16 byte aligned
     */
    task->fpu = 1;
    clts();
    fpu_save_area = (u32) sigframe->fpu_save_area;
    while (fpu_save_area % 16)
        fpu_save_area++;
    fpu_restore(fpu_save_area);
    /*
     * To avoid that malicious user code manipulates system bits in the
     * EFLAGS register by changing the stack content during an interrupt handler,
     * the following bits are not taken over from the sigframe structure:
     * NT - bit 14
     * VM - bit 16
     * RF - bit 17
     * AC - bit 18
     * VIF - bit 19
     * VIP - bit 20
     * I/O privilege level - bits 12 and 13
     * IF - bit 9
     * TF - bit 8
     */
    sv_only_bits = (1 << 8) + (1 << 9) + (1 << 12) + (1 << 13)
            + (1 << 14) + (1 << 16) + (1 << 17) + (1 << 18) + (1 << 19) + (1 << 20);
    ir_context->eflags = (sigframe->eflags & ~sv_only_bits) + (ir_context->eflags & sv_only_bits);
    spinlock_release(&task->spinlock, &eflags);
    return ir_context->eax;
}

/****************************************************************************************
 * The following functions deal with signal processing on the level of all tasks which  *
 * belong to an individual process. They have in common that they walk the task table   *
 * and execute a certain signal processing related action on all tasks which belong     *
 * to a given process                                                                   *
 ****************************************************************************************/

/*
 * Remove a signal from the pending signal bitmask of all
 * threads of a process
 * Parameters:
 * @proc - the process
 * @sigmask - bit mask of all signals which are to be cleared
 * Locks:
 * task table lock
 * Cross-monitor function calls:
 * discard_signal_task
 */
static void discard_signals_threads(proc_t* proc, u32 sigmask) {
    u32 eflags;
    int i;
    spinlock_get(&task_table_lock, &eflags);
    for (i = 2; i < PM_MAX_TASK; i++) {
        if ((TASK_SLOT_USED == tasks[i].slot_usage) && (tasks[i].proc == proc)) {
            discard_signal_task(tasks + i, sigmask);
        }
    }
    spinlock_release(&task_table_lock, &eflags);
}

/*
 * Internal utility function to distribute all pending signals on process level
 * to all threads of a process.
 * Parameters:
 * @pid - the pid
 * Locks:
 * task table lock
 * Cross-monitor function calls:
 * promote_signals
 */
static void distribute_signals_threads(proc_t* proc) {
    u32 eflags;
    int i;
    /*
     *
     * Walk through all tasks and try to move pending signals
     * from the process to this task. We start with task 2 as
     * tasks 0 and 1 cannot be signaled
     */
    spinlock_get(&task_table_lock, &eflags);
    for (i = 2; i < PM_MAX_TASK; i++) {
        if ((TASK_SLOT_USED == tasks[i].slot_usage) && (tasks[i].proc == proc)) {
            if (0 == promote_signals(tasks + i)) {
                break;
            }
        }
    }
    spinlock_release(&task_table_lock, &eflags);
}

/*
 * Continue all threads of a process that has received SIGCONT or SIGKILL
 * Parameters:
 * @proc - the process
 * @sig_no - the signal which has been received
 * Locks:
 * task table locks
 * Cross-monitor function calls:
 * continue_task
 */
static void continue_threads(proc_t* proc, int sig_no) {
    u32 eflags;
    int i;
    spinlock_get(&task_table_lock, &eflags);
    for (i = 2; i < PM_MAX_TASK; i++) {
        if ((TASK_SLOT_USED == tasks[i].slot_usage) && (tasks[i].proc == proc)) {
            continue_task(tasks + i, sig_no);
        }
    }
    spinlock_release(&task_table_lock, &eflags);
}

/*
 * Utility function to send a signal to all tasks within a process
 * except the currently active task
 * Parameters:
 * @proc - the process
 * @sig_no - the signal number
 * Locks:
 * task table lock
 * Cross-monitor function calls:
 * signal_task
 */
static void signal_other_threads(proc_t* proc, int sig_no) {
    u32 eflags;
    int i;
    task_t* self = tasks + pm_get_task_id();
    spinlock_get(&task_table_lock, &eflags);
    for (i = 2; i < PM_MAX_TASK; i++) {
        if ((TASK_SLOT_USED == tasks[i].slot_usage) && (tasks[i].proc == proc)
                && (i != self->id)) {
            signal_task(tasks + i, sig_no, 1);
        }
    }
    spinlock_release(&task_table_lock, &eflags);
}

/****************************************************************************************
 * The following functions deal with the signal handling on the level of an individual  *
 * process                                                                              *
 ****************************************************************************************/

/*
 * Promote pending signals on process level to pending signals on
 * task level for a specific task
 * @task - the task
 * Return value:
 * 0 if all pending signals on process level have been moved over to the task
 * 1 if there are still pending signals on process level
 * Locks:
 * lock on the process to which the task belongs
 * Cross-monitor function calls:
 * signal_task
 */
static int promote_signals(task_t* task) {
    u32 eflags;
    int i;
    int rc = 0;
    proc_t* proc = task->proc;
    spinlock_get(&proc->spinlock, &eflags);
    for (i = 0; i < __NR_OF_SIGNALS; i++) {
        if ((1 << i) & proc->sig_pending) {
            if (1 == signal_task(task, i, 0)) {
                proc->sig_pending &= ~(1 << i);
            }
        }
    }
    /*
     * Clear special signal __KSIGTASK immediately again as its delivery is forced
     * anyway
     */
    proc->sig_pending &= ~(1 << __KSIGTASK);
    if (proc->sig_pending)
        rc = 1;
    spinlock_release(&proc->spinlock, &eflags);
    return rc;
}

/*
 * Remove a signal from the pending signal bitmask of a process
 * and all its threads
 * Parameters:
 * @proc - the process
 * @sigmask - bit mask of all signals which are to be cleared
 * @threads - if this flag is set, remove signal from pending bitmasks of
 * threads as well
 * Locks:
 * lock on process @proc
 */
static void discard_signal(proc_t* proc, u32 sigmask, int threads) {
    u32 eflags;
    /*
     * Update process level signal mask
     */
    spinlock_get(&proc->spinlock, &eflags);
    proc->sig_pending &= ~sigmask;
    spinlock_release(&proc->spinlock, &eflags);
    /*
     * Now update pending signal bitmasks for all threads
     */
    if (threads)
        discard_signals_threads(proc, sigmask);
}

/*
 * Internal utility function to generate a signal for a specific process
 * It is assumed that the caller holds the lock on the process table structure
 * Parameters:
 * @proc - the process
 * @sig_no - the signal number
 * Locks:
 * lock on current process
 */
static void signal_proc(proc_t* proc, int sig_no) {
    u32 eflags;
    u32 stop_signals = (1 << __KSIGSTOP) | (1 << __KSIGTTIN)
            | (1 << __KSIGTTOU) | (1 << __KSIGTSTP);
    /*
     * "Ordinary signal delivery": add signal to pending bitmask and then try
     * to push it down to the task level by calling distribute_signals
     */
    spinlock_get(&proc->spinlock, &eflags);
    proc->sig_pending |= (1 << sig_no);
    spinlock_release(&proc->spinlock, &eflags);
    /*
     * For SIGCONT, clear all pending stop signals. We only need to do this
     * on process level, as we will call continue_task later for each task
     * which will do this on task level
     */
    if (__KSIGCONT == sig_no) {
        discard_signal(proc, stop_signals, 0);
    }
    /*
     * For a stop signal, clear all pending SIGCONTS
     */
    if ((1 << sig_no) & stop_signals) {
        discard_signal(proc, 1 << __KSIGCONT, 1);
    }
    distribute_signals_threads(proc);
    if ((__KSIGCONT == sig_no) || (__KSIGKILL == sig_no)) {
        /*
         * If the signal is SIGKILL, make sure that we set the exit flag now to avoid
         * the potential race condition that we continue the process before the KILL
         * is being processed
         */
        if (__KSIGKILL == sig_no)
            pm_schedule_exit(proc->id, sig_no);
        continue_threads(proc, sig_no);
    }
}

/****************************************************************************************
 * The following section contains the core signal handling routines:                    *
 * - send a signal to one or more processes                                             *
 * - prepare a task for execution of a signal handler                                   *
 * - process pending signals                                                            *
 * - miscellanous utility functions                                                     *
 ****************************************************************************************/

/*
 * Get the default action for a specified signal number
 * Parameter:
 * @sig_no - the signal number
 * Return value:
 * 0 - signal number could not be resolved
 * SIG_DFL_TERM - terminate process abnormally
 * SIG_DFL_IGN - ignore signal
 * SIG_DFL_STOP - stop process
 * SIG_DFL_CONT - continue if process is stopped
 */
static int get_default_action(int sig_no) {
    int i;
    int rc = 0;
    for (i = 0;
            i < (sizeof(sig_default_actions) / sizeof(sig_default_action_t)); i++) {
        if (sig_default_actions[i].sig_no == sig_no) {
            rc = sig_default_actions[i].default_action;
        }
    }
    return rc;
}

/*
 * Generate a signal for one or more processes
 * Parameter:
 * @pid - the process for which we generate the signal
 * @sig_no - the signal
 * Return value:
 * 0 upon success
 * -ESRCH - the specified process does not exist
 * -EINVAL - the signal is not valid
 * Locks:
 * proc_table_lock - protect process table
 * Cross-monitor function calls:
 * signal_proc
 *
 */
int do_kill(pid_t pid, int sig_no) {
    int rc = -ESRCH;
    int i;
    u32 eflags;
    /*
     * Validate signal
     */
    if (0 == get_default_action(sig_no))
        return -EINVAL;
    /*
     * If a specific PID is specified, deliver to this one only
     * and return
     */
    if (pid > 1) {
        spinlock_get(&proc_table_lock, &eflags);
        if ((PROC_SLOT_USED == procs[pid].slot_usage)) {
            signal_proc(procs + pid, sig_no);
            rc = 0;
        }
        spinlock_release(&proc_table_lock, &eflags);
        return rc;
    }
    /*
     * Get all PIDs matching the specification
     * We do not allow a signal to be sent to process 1
     */
    for (i = 2; i < PM_MAX_PROCESS; i++) {
        spinlock_get(&proc_table_lock, &eflags);
        /*
         * Signal needs to be delivered to more than one process
         */
        if (PROC_SLOT_USED == procs[i].slot_usage) {
            /*
             * If pid is 0, deliver to all processes in the same process group
             */
            if ((0 == pid) && (procs[i].pgid == procs[pm_get_pid()].pgid)) {
                signal_proc(procs + i, sig_no);
                rc = 0;
            }
            /*
             * If pid is -1, deliver to all processes
             */
            else if (-1 == pid) {
                signal_proc(procs + i, sig_no);
                rc = 0;
            }
            /*
             * Deliver to all processes in process group -pid
             */
            else if ((pid < -1) && (((-1) * pid) == procs[i].pgid)) {
                signal_proc(procs + i, sig_no);
                rc = 0;
            }
        }
        spinlock_release(&proc_table_lock, &eflags);
    }
    return rc;
}

/*
 * Prepare a stack for use by a signal handler and return the lowest used address on that stack -
 * this is where ESP of the signal handler needs to point to
 * Parameter:
 * @stack - the top of the stack area to use
 * @sig_no - the signal number
 * @context - the IR context which is to be used to fill up the sigframe structure
 * @sigmask - sigmask to be put into the sigframe structure
 * @__sigframe - pointer to a sigframe structure in which we return the generated sigframe
 * Return value:
 * lowest used address on the stack
 *
 * Note: after executing this function, the user space stack will look as follows:
 *
 * code to execute sigreturn <-- ends at address @tos
 * NOPS                      <----------------------------------|    <-- used to align dword boundary
 * sig_frame_t structure                                        |
 * 0                                                            |
 * 0                                                            |
 * sig_no                                                       |
 * address of code to execute sigreturn -------------------------    <-- this address is returned
 *
 */
u32 pm_prepare_signal_stack(u32 stack, int sig_no, ir_context_t* context,
        u32 sigmask, sig_frame_t** __sigframe) {
    u8* tos = (u8*) stack;
    task_t* self = tasks + pm_get_task_id();
    u8* ptr;
    u32* c;
    u32* int_ptr;
    u32 fpu_save_area;
    sig_frame_t* sigframe;
    /*
     * First we place the code to execute sigreturn on the stack
     */
    for (ptr = (u8*) &__sigreturn_end; ptr >= (u8*) &__sigreturn_start; ptr--) {
        *(tos--) = *ptr;
    }
    /*
     * Re-align tos if needed and fill up remaining area with NOPs
     */
    while (((u32) tos) % sizeof(u32)) {
        *(tos--) = 0x90;
    }
    /*
     * Remember address of first byte of code
     */
    c = (u32*) (tos + 1);
    /*
     * Now fill the sigframe structure
     */
    sigframe = (sig_frame_t*) (tos + 1);
    sigframe--;
    sigframe->sigmask = sigmask;
    sigframe->eax = context->eax;
    sigframe->ebp = context->ebp;
    sigframe->ebx = context->ebx;
    sigframe->ecx = context->ecx;
    sigframe->edi = context->edi;
    sigframe->edx = context->edx;
    sigframe->eflags = context->eflags;
    sigframe->eip = context->eip;
    sigframe->esp = context->esp;
    sigframe->esi = context->esi;
    *__sigframe = sigframe;
    /*
     * Place the FPU state in the sigframe structure. As we need to clear TS for that purpose,
     * we also set the fpu bit in the current task. We place the FPU state at the first byte
     * within the fpu save area in the sigframe structure which is 16 byte aligned
     */
    clts();
    fpu_save_area = (u32) sigframe->fpu_save_area;
    while (fpu_save_area % 16)
        fpu_save_area++;
    self->fpu = 1;
    fpu_save(fpu_save_area);
    /*
     * We assume that the @stack argument is the first free dword on the user space stack,
     * so we can get the old value of the user space stack pointer by adding sizeof(dword)
     */
    sigframe->ring0_esp = stack + 4;
    /*
     * and make int_ptr point to the first free byte below it
     */
    int_ptr = (u32*) (sigframe) - 1;
    /*
     * Now push three parameters. As we do not yet support SA_SIGINFO, two of them are zero, the
     * third one is the signal number
     */
    *(int_ptr--) = 0;
    *(int_ptr--) = 0;
    *(int_ptr--) = sig_no;
    /*
     * Push c itself - this will be the return address of the signal handler
     */
    *int_ptr = (u32) c;
    return (u32) int_ptr;
}

/*
 * Stop all running tasks within a process. This is simply done by
 * a) sending the SIGTASK signal to all tasks within the process
 * b) update its own status to STOPPED
 * In addition, SIGCHLD will be sent to the parent process assuming
 * that SA_NOCLDSTOP is not set, and the semaphore unwaited as well
 * as the counter unwaited_children will be increased for the parent
 * to inform it about the event
 * Parameters:
 * @sig_no - signal number which caused the stop
 * Locks:
 * proc_table_lock
 * Cross-monitor function calls:
 * signal_other_threads
 * signal_proc
 * cond_broadcast
 * stop_task
 */
static void stop_process(int sig_no) {
    u32 eflags;
    int pid = pm_get_pid();
    proc_t* parent;
    proc_t* self;
    spinlock_get(&proc_table_lock, &eflags);
    self = procs + pid;
    parent = procs + self->ppid;
    /*
     * If we receive SIGTASK, another task in the same process has executed this function, therefore
     * we only handle the currently active task.
     */
    if (__KSIGTASK != sig_no) {
        /*
         * Send the internal signal SIGTASK to all other tasks within this process
         */
        signal_other_threads(self, __KSIGTASK);
        /*
         * Send SIGCHLD to parent, but only if parent has not SA_NOCLDSTOP set in sa_flags
         * for the SIGCHLD signal. Note that we do not get a spinlock on the sigaction structure of the parent
         * here as the read from the flags field is atomic
         */
        u32 ppid = tasks[pm_get_task_id()].proc->ppid;
        u32 sa_flags = atomic_load(&procs[ppid].sig_actions[__KSIGCHLD].sa_flags);
        if (0 == (sa_flags & __KSA_NOCLDSTOP)) {
            signal_proc(procs + ppid, __KSIGCHLD);
        }
        /*
         * Inform parent process about a waitable child and set exit status
         */
        self->exit_status = EXIT_REASON_SUSPEND + (sig_no << 8);
        if (0 == self->waitable) {
            cond_broadcast(&parent->unwaited);
            parent->unwaited_children++;
            self->waitable = 1;
        }
    }
    /*
     * Set active task to stopped
     */
    stop_task();
    spinlock_release(&proc_table_lock, &eflags);
}

/*
 * Arrange for invocation of a user space signal handler. This function should only be called
 * if the caller has the lock on the task structure already
 * Parameter:
 * @ir_context - current IR context
 * @sig_no - signal number
 * @task - pointer to currently active task
 * @sig_action - pointer to sigaction structure
 * Return value:
 * a pointer to a sigframe structure
 */
static sig_frame_t* invoke_signal_handler(ir_context_t* ir_context, int sig_no,
        task_t* task, __ksigaction_t* sig_action) {
    sig_frame_t* sigframe;
    u32 saved_esp;
    u32 new_tos;
    /*
     * First save old value of esp, signal mask and IR context. Note that the EAX register in the saved
     * context already contains the return value of a syscall executed previously
     */
    saved_esp = *((&(ir_context->eflags)) + 1);
    /*
     * Set new signal mask, EIP and ESP. The new signal mask is formed by taking the union of the old signal mask and the
     * value of sa_mask in the sigaction structure and marking the currently delivered signal as blocked. We also make sure that
     * even if contained in sa_mask, SIGSTOP and SIGKILL cannot be blocked
     * Note that the call to pm_prepare_signal_mask will also copy the signal mask and the current IR context
     * onto the user space stack
     */
    new_tos = pm_prepare_signal_stack(saved_esp - 4, sig_no, ir_context,
            task->sig_blocked, &sigframe);
    *((&(ir_context->eflags)) + 1) = new_tos;
    ir_context->eip = (u32) sig_action->sa_handler;
    task->sig_blocked |= ((1 << sig_no) | sig_action->sa_mask);
    task->sig_blocked &= ~((1 << __KSIGSTOP) | (1<<__KSIGKILL));
    return sigframe;
}

/*
 * Utility function to determine if a restart of the currently executing system call is required
 * If no restart is required, this function also takes care of updating the interrupt context to
 * make sure that the system call returns the correct value
 * Parameter:
 * @ir_context - current IR context
 * @action - actual action taken by signal processing code (SIG_ACTION_*)
 * @task - pointer to currently active task
 * @sig_no - signal number
 * @sigframe - in case the action is SIG_ACTION_HANDLER, this is a pointer to the sigframe structure
 * generated on the user space stack
 */
static int restart_needed(ir_context_t* ir_context, int action, task_t* task,
        int sig_no, sig_frame_t* sigframe) {
    KASSERT(TASK_SLOT_USED == task->slot_usage);
    /*
     * If the last system call returned -EPAUSE, and the action is not termination of the process or
     * execution of a signal handler, return 1 to signal restart to the interrupt manager
     * Make sure that if the task or process is flagged for exit, we do not restart
     */
    if ((SYSCALL_IRQ == ir_context->vector) && (-EPAUSE == ir_context->eax)) {
        if ((SIG_ACTION_HANDLER != action) && (0 == task->proc->force_exit)
                && (0 == task->force_exit)) {
            return 1;
        }
        else {
            /*
             * We need to overwrite the return value with -EINTR - do this in
             * saved signal handler used by sigreturn as well if there is one
             * (which is not necessarily the case as we might enter this branch
             * as the task is marked for exit)
             */
            ir_context->eax = -EINTR;
            if (sigframe)
                sigframe->eax = -EINTR;
        }
    }
    return 0;
}

/*
 * Build a bitmap of ignored signals. A signal is considered ignored if
 * 1) the signal action is SIG_IGN, or
 * 2) the signal action is SIG_DFL, and the default is to ignore the signal
 * Parameter:
 * @proc - the process which we examine
 * Return value:
 * a 32 bit bitmask where a bit (1<<x) is set if and only
 * if signal x is ignored
 * Locks:
 * spinlock on process
 */
static unsigned int build_ignore_mask(proc_t* proc) {
    int sig_no;
    u32 eflags;
    u32 mask = 0;
    spinlock_get(&proc->spinlock, &eflags);
    for (sig_no = 0; sig_no < 32; sig_no++) {
        if ((__KSIG_IGN == proc->sig_actions[sig_no].sa_handler)
                || ((__KSIG_DFL == proc->sig_actions[sig_no].sa_handler)
                        && (SIG_DFL_IGN == get_default_action(sig_no)))) {
            mask |= (1 << sig_no);
        }
    }
    spinlock_release(&proc->spinlock, &eflags);
    return mask;
}

/*
 * This is the main handler which is invoked by the interrupt manager in order to handle
 * pending signals for the currently running task
 * Parameter:
 * @ir_context - IR context
 * Return value:
 * 1 if a restart of the last system call is requested
 * 0 if no restart is required
 * Locks:
 * task->spinlock for currently active task
 *
 */
int pm_process_signals(ir_context_t* ir_context) {
    u32 eflags;
    u32 signals;
    task_t* task = tasks + pm_get_task_id();
    __ksigaction_t sig_action;
    int default_action;
    unsigned int ignore_mask = 0;
    int sig_no = 0;
    sig_frame_t* sigframe = 0;
    KASSERT(TASK_SLOT_USED == task->slot_usage);
    /*
     * Log the action we have actually taken (SIG_ACTION_*)
     */
    int action = SIG_ACTION_NONE;
    int rc = 0;
    /*
     * Do nothing if the code segment indicates that the IRQ was not
     * raised in user mode, but in kernel mode
     */
    if (mm_is_kernel_code(ir_context->cs_old)) {
        return 0;
    }
    /*
     * Do nothing if a task has a pending exit request. Otherwise, we could produce a situation in which
     * we stop a task which is already marked for exit
     */
    if ((1 == task->force_exit) || (1 == task->proc->force_exit))
        return 0;
    ignore_mask = build_ignore_mask(task->proc);
    spinlock_get(&task->spinlock, &eflags);
    /*
     * Remove all signals which are blocked and ignored from the mask of pending signals.
     */
    task->sig_pending &= ~(ignore_mask & task->sig_blocked);
    /*
     * Determine signals which are to be processed. We process all signals which are
     * pending and not blocked. SIGKILL has highest priority
     */
    signals = task->sig_pending & (~(task->sig_blocked));
    spinlock_release(&task->spinlock, &eflags);
    if (signals) {
        while (!((1 << sig_no) & signals))
            sig_no++;
        if (signals & (1 << __KSIGKILL))
            sig_no = __KSIGKILL;
        /*
         * Get sigaction. Note that this will fail for __KSIGTASK
         */
        if (do_sigaction(sig_no, 0, &sig_action)) {
            if (sig_no != __KSIGTASK) {
                ERROR("Could not retrieve sigaction for signal %d, task %d\n", sig_no, pm_get_task_id());
                return 0;
            }
            else {
                sig_action.sa_handler = __KSIG_DFL;
            }
        }
        /*
         * Remove sig_no from pending signal mask
         */
        spinlock_get(&task->spinlock, &eflags);
        task->sig_pending &= (~(1 << sig_no));
        spinlock_release(&task->spinlock, &eflags);
        /*
         * If default action is to be taken, determine and execute it. SIGKILL and SIGSTOP cannot be
         * overwritten, thus we also execute the default action for those two signals regardless of the value
         * of the sigaction handler
         */
        if ((__KSIG_DFL == sig_action.sa_handler) || (__KSIGKILL == sig_no)
                || (__KSIGSTOP == sig_no)) {
            default_action = get_default_action(sig_no);
            switch (default_action) {
                case SIG_DFL_TERM:
                    pm_schedule_exit(task->proc->id, sig_no);
                    action = SIG_ACTION_TERM;
                    break;
                case SIG_DFL_STOP:
                    stop_process(sig_no);
                    action = SIG_ACTION_STOPPED;
                    break;
                default:
                    break;
            }
        }
        /*
         * Ignore signal - do nothing.
         */
        else if (__KSIG_IGN == sig_action.sa_handler) {
            action = SIG_ACTION_IGN;
        }
        /*
         * Action should be to invoke a user-specified signal handler. Call utility function to set
         * up stack and overwrite the saved user land stack pointer above the IR context with its return value
         */
        else {
            action = SIG_ACTION_HANDLER;
            sigframe = invoke_signal_handler(ir_context, sig_no, task,
                    &sig_action);
        }
        /*
         * Set return code to indicate restart if needed
         */
        rc = restart_needed(ir_context, action, task, sig_no, sigframe);
    }
    return rc;
}

/****************************************************************************************
 * The following functions implement the system calls used by an application program    *
 * to retrieve or change signal handlers and the bitmask of blocked signals             *
 ****************************************************************************************/

/*
 * Set a new sigaction structure for the currently active process and store the previous structure.
 * If a signal is set to ignore, the corresponding bit in the pending signal bitmask of the process
 * is cleared
 * Parameter:
 * @sig_no - the signal number
 * @act - the action to be set
 * @old - here we store the old action if requested
 * Return value:
 * 0 if operation was successful
 * -EINVAL if signal number is not valid or if it is tried to change the action for KILL or STOP
 * Locks:
 * process spinlock
 *
 */
int do_sigaction(int sig_no, __ksigaction_t* act, __ksigaction_t* old) {
    proc_t* proc = procs + pm_get_pid();
    u32 eflags;
    __ksigaction_t* current_action = proc->sig_actions+sig_no;
    /*
     * Validate signal number
     */
    if (0 == get_default_action(sig_no)) {
        return -EINVAL;
    }
    if (__KSIGTASK == sig_no) {
        return -EINVAL;
    }
    spinlock_get(&(proc->spinlock), &eflags);
    /*
     * Save old action if requested
     */
    if (old) {
        old->sa_flags = current_action->sa_flags;
        old->sa_handler = current_action->sa_handler;
        old->sa_mask = current_action->sa_mask;
    }
    /*
     * Install new action if requested
     */
    if (act) {
        if ((__KSIGKILL == sig_no) || (__KSIGSTOP == sig_no)) {
            spinlock_release(&(proc->spinlock), &eflags);
            return -EINVAL;
        }
        current_action->sa_handler = act->sa_handler;
        current_action->sa_mask = act->sa_mask;
        current_action->sa_flags = act->sa_flags;
        /*
         * If the action is to ignore the signal, remove it from pending signal
         * bitmask on process level
         */
        if ((__KSIG_IGN == act->sa_handler) || ((__KSIG_DFL == act->sa_handler) && (SIG_DFL_IGN == get_default_action(sig_no)))) {
            proc->sig_pending &= (~(1<<sig_no));
        }
    }
    spinlock_release(&(proc->spinlock), &eflags);
    return 0;
}

/*
 * Return the set of signals pending for the calling thread. Note that this is the union of the set
 * of signals pending for the task with the set of signals pending for the process
 * Parameter:
 * @sigmask - the result is stored here
 * Return value:
 * always 0
 * Locks:
 * spinlock on process
 * Cross-monitor function calls:
 * get_signals_task
 */
int do_sigpending(u32* sigmask) {
    u32 eflags;
    task_t* self = tasks + pm_get_task_id();
    proc_t* proc = self->proc;
    /*
     * Get lock on process
     */
    spinlock_get(&proc->spinlock, &eflags);
    /*
     * Merge bitmasks and remove internal signal SIGTASK
     */
    *sigmask = (proc->sig_pending | get_signals_task(self))
            & ~(1 << __KSIGTASK);
    /*
     * Release lock again
     */
    spinlock_release(&proc->spinlock, &eflags);
    return 0;
}

/*
 * Inquire or modify the signal mask for the currently active task.
 * Parameters:
 * @how:
 * - __KSIG_SETMASK - set the signal mask to @set
 * - __KSIG_BLOCK - block all signals in @set
 * - __KSIG_UNBLOCK - unblock all signals in @set
 * @set - the signal mask to be applied resp. merged depending on  @how
 * @oset - if this is set, copy the old signal mask to this location
 * Return value:
 * 0 if the operation is successful
 * -EINVAL - invalid argument for @how
 * Locks:
 * lock on current task
 */
int do_sigprocmask(int how, u32* set, u32* oset) {
    u32 eflags;
    int rc = 0;
    task_t* self = tasks + pm_get_task_id();
    spinlock_get(&self->spinlock, &eflags);
    if (oset) {
        *oset = self->sig_blocked;
    }
    if (set) {
        switch (how) {
            case __KSIG_SETMASK:
                self->sig_blocked = *set;
                break;
            case __KSIG_BLOCK:
                self->sig_blocked |= *set;
                break;
            case __KSIG_UNBLOCK:
                self->sig_blocked &= (~(*set));
                break;
            default:
                rc = -EINVAL;
                break;
        }
        /*
         * Make sure that __KSIGKILL and __KISGSTOP as well as the internal signal __KSIGTASK cannot be blocked
         */
        self->sig_blocked &= (~((1 << __KSIGKILL) | (1 << __KSIGSTOP) | (1
                << __KSIGTASK)));
    }
    spinlock_release(&self->spinlock, &eflags);
    /*
     * Re-distribute signals for this process if signal mask has potentially been changed
     */
    if (set)
        promote_signals(self);
    return rc;
}

/****************************************************************************************
 * Actively wait for a signal, i.e.                                                     *
 * - pause until a signal is received, or                                               *
 * - wait for a specific signal                                                         *
 ****************************************************************************************/

/*
 * Wait for a signal to occur
 * Parameter:
 * @sig_set - a mask of up to 32 possible signals for which we wait
 * @sig - here we store the signal which we have actually received
 * Return value:
 * 0 if no error occured
 * -EINVAL if the signal number is not valid
 * Locks:
 * spinlock on task
 */
int do_sigwait(u32 sig_set, int* sig) {
    u32 eflags;
    int sig_no;
    task_t* task = tasks + pm_get_task_id();
    for (sig_no = 0; sig_no < __NR_OF_SIGNALS; sig_no++) {
        if ((1 << sig_no) & sig_set) {
            if ((0 == get_default_action(sig_no)) || (__KSIGTASK == sig_no))
                return -EINVAL;
        }
    }
    spinlock_get(&task->spinlock, &eflags);
    task->sig_waiting = sig_set;
    while (0 == (task->sig_waiting & task->sig_pending)) {
        /*
         * Note that we need to release the spinlock on the
         * task structure temporarily here as promote_signals
         * calls signal_task which tries to get this lock as well
         */
        spinlock_release(&task->spinlock, &eflags);
        promote_signals(task);
        spinlock_get(&task->spinlock, &eflags);
        /*
         * Promote signals might have promoted the signal to the task level
         * or we might have been signaled while we did not have the lock, so
         * check condition again. If we still did not receive the signal,
         * go to sleep
         */
        if (0 == (task->sig_waiting & task->sig_pending)) {
            block_task();
            spinlock_release(&task->spinlock, &eflags);
            reschedule();
            spinlock_get(&task->spinlock, &eflags);
        }
        /*
         * If we get to this point, we have been woken up by a signal
         * and hold the lock on the task
         */
    }
    for (sig_no = 0; sig_no < __NR_OF_SIGNALS; sig_no++) {
        if ((1 << sig_no) & (task->sig_pending) & (task->sig_waiting)) {
            break;
        }
    }
    if ((1 << sig_no) & (task->sig_pending) & (task->sig_waiting)) {
        task->sig_pending &= (~(1 << sig_no));
        task->sig_waiting = 0;
    }
    else {
        PANIC("There should be at least one signal in sig_waiting and sig_pending, but I could not find one\n");
    }
    *sig = sig_no;
    spinlock_release(&task->spinlock, &eflags);
    return 0;
}

/*
 * Pause a task until a signal is sent to it which will invoke a signal handler or terminate the process.
 * Return value:
 * -EINTR
 * Locks:
 * task spinlock
 */
int do_pause() {
    u32 eflags;
    task_t* self = tasks + pm_get_task_id();
    /*
     * This system call will simply go to TASK_STATUS_BLOCKED_INTR and then return
     * the special error code -EPAUSE.
     *
     * When this error code is detected by the signal handling code, it will initiate a restart of the system call if it has
     * not delivered an interrupt and if the task is not flagged for termination
     *
     */
    spinlock_get(&self->spinlock, &eflags);
    block_task_intr();
    spinlock_release(&self->spinlock, &eflags);
    reschedule();
    return -EPAUSE;
}

/*
 * Adapt the signal mask of a task and pause the task until a signal handler is executed
 * or the process terminates. Note that to implement the full functionality of the sigsuspend
 * POSIX function, the user space part needs to restore the signal mask when the system call returns
 * Parameter:
 * @set - new signal mask which will be applied
 * @old_set - the old signal mask will be stored here
 * Return value:
 * -EPAUSE
 * Locks:
 * lock on task status
 */
int do_sigsuspend(u32* set, u32* old_set) {
    u32 eflags;
    task_t* self = tasks + pm_get_task_id();
    if (0 == set)
        return -EINTR;
    /*
     * Adapt signal mask, block task and reschedule
     */
    spinlock_get(&self->spinlock, &eflags);
    if (old_set)
        *old_set = self->sig_blocked;
    self->sig_blocked = *set;
    block_task_intr();
    spinlock_release(&self->spinlock, &eflags);
    reschedule();
    /*
     * Return -EPAUSE to invoke restart logic in interrupt manager
     */
    return -EPAUSE;
}

/***************************************************************
 * Everything below this line is for debugging only            *
 **************************************************************/

extern void (*debug_getline)(char* line, int max);

/*
 * Print the task table for use by the internal debugger
 */
void pm_print_task_table() {
    int task;
    char c[2];
    int count = 0;
    int proc;
    PRINT("Active task: %x\n", pm_get_task_id());
    PRINT("Task ID   PID     Ticks        Saved ESP    Level   Status        CPU\n");
    PRINT("---------------------------------------------------- -------------------  \n");
    for (task = 0; task < PM_MAX_TASK; task++) {
        if ((tasks[task].slot_usage == TASK_SLOT_USED) || (task == 0)) {
            PRINT("%w      %w    %x    %x    %d      ", tasks[task].id, tasks[task].proc->id,
                    tasks[task].ticks, tasks[task].saved_esp, tasks[task].execution_level);
            if (pm_get_task_id() == task)
                PRINT("*");
            else
                PRINT(" ");
            switch (tasks[task].status) {
                case TASK_STATUS_RUNNING:
                    PRINT("RUNNING     ");
                    break;
                case TASK_STATUS_DONE:
                    PRINT("DONE        ");
                    break;
                case TASK_STATUS_BLOCKED:
                    PRINT("BLOCKED     ");
                    break;
                case TASK_STATUS_BLOCKED_INTR:
                    PRINT("BLOCKED_INTR");
                    break;
                case TASK_STATUS_STOPPED:
                    PRINT("STOPPED     ");
                    break;
                default:
                    PRINT("UNKNOWN     ");
                    break;
            }
            if (-1 == tasks[task].cpuid)
                PRINT("   \n");
            else
                PRINT(" %.2d\n", tasks[task].cpuid);
            count++;
            if (0 == (count % 10)) {
                count = 0;
                PRINT("Hit ENTER to continue\n");
                debug_getline(c, 1);
                PRINT("Active task: %x\n", pm_get_task_id());
                PRINT("Task ID   PID     Ticks        Saved ESP    Level   Status        CPU\n");
                PRINT("---------------------------------------------------- -------------------  \n");
            }
        }
    }
    PRINT("Hit ENTER to continue\n");
    debug_getline(c, 1);
    count = 0;
    PRINT("PID     Task count\n");
    PRINT("-------------------\n");
    for (proc = 0; proc < PM_MAX_PROCESS; proc++) {
        if ((procs[proc].slot_usage == PROC_SLOT_USED)) {
            count++;
            PRINT("%w    %d\n", proc, procs[proc].task_count);
            if (0 == (count % 10)) {
                count = 0;
                PRINT("Hit ENTER to continue\n");
                debug_getline(c, 1);
                PRINT("PID     Task count\n");
                PRINT("-------------------\n");
            }
        }
    }
}

/*
 * Perform a few consistency checks on our data structures
 */
void pm_validate() {
    int i;
    task_t* task;
    for (i = 0; i < PM_MAX_TASK; i++) {
        task = tasks + i;
        switch(task->slot_usage) {
            case TASK_SLOT_FREE:
                break;
            case TASK_SLOT_RESERVED:
                break;
            case TASK_SLOT_USED:
                KASSERT(task->id == task - tasks);
                KASSERT(task->proc);
                break;
            default:
                PANIC("Task %d has unknown task status %x\n");
        }
    }
}
