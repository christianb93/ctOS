/*
 * pm.h
 */

#ifndef _PM_H_
#define _PM_H_

#include "lib/sys/types.h"
#include "locks.h"
#include "lib/pthread.h"
#include "irq.h"
#include "lib/os/signals.h"
#include "lib/os/times.h"
#include "lib/sys/resource.h"

/*
 * Size of the FPU save area
 */
#define FPU_STATE_BYTES 512


/*
 * This structure contains a user space execution context and is used to
 * store the state which needs to be restored by a sigreturn call on the
 * stack while a signal is handled
 */
typedef struct _sig_frame_t {
    u8 fpu_save_area[FPU_STATE_BYTES + 16];
    reg_t eax;
    reg_t ebx;
    reg_t ecx;
    reg_t edx;
    reg_t esp;
    reg_t ebp;
    reg_t edi;
    reg_t esi;
    reg_t eip;
    reg_t eflags;
    u32 sigmask;
    u32 ring0_esp;
} sig_frame_t;

/*
 * Usage of a slot in the task table
 */
#define TASK_SLOT_FREE 0
#define TASK_SLOT_RESERVED 1
#define TASK_SLOT_USED 2

/*
 * This structure defines a task
 */
typedef struct _task_t {
    int slot_usage;                              // 0 = unused slot, 1 = reserved slot, 2 = used slot
    int ref_count;                               // reference count
    u32 id;                                      // id of the task
    u32 user_id;                                 // user-visible id, only unique within process, but inherited by fork
    int status;                                  // the status of the task
    reg_t saved_esp;                             // used to store esp during task switch
    reg_t saved_cr3;                             // saved CR3 of the process
    int execution_level;                         // Execution level
    int force_exit;                              // set this to force exit of task
    spinlock_t spinlock;                         // lock to protect the data structure
    struct _proc_t* proc;                        // process to which the task belongs
    int priority;                                // the static priority of the task
    u32 ticks;                                   // CPU ticks while the task was active
    u32 sig_waiting;                             // bitmask indicating for which signals this task is waiting
    u32 sig_blocked;                             // signals blocked for this task
    u32 sig_pending;                             // signals pending
    int intr;                                    // set if a sleep has been interrupted
    int idle;                                    // set if this task is the idle task for a CPU
    int floating;                                // task has been removed from scheduler queue but not yet switched away from
    int cpuid;                                   // ID of CPU to which we are bound or -1
    int fpu;                                     // this flag is set if the task has used the FPU since we have saved the FPU state
    u8* fpu_save_area;                           // a pointer to a 512 byte array in which the FPU state is saved
} task_t;


/*
 * Usage of a slot in the process table
 */
#define PROC_SLOT_FREE 0
#define PROC_SLOT_RESERVED 1
#define PROC_SLOT_USED 2

/*
 *
 * This structure defines a process
 */
typedef struct _proc_t {
    u32 slot_usage;                                // PROC_SLOT_FREE, PROC_SLOT_RESERVED or PROC_SLOT_USED
    u32 id;                                        // id of the process
    spinlock_t spinlock;                           // lock to protect the data structure
    u32 task_count;                                // number of tasks in this process with status != DONE, protected by task table lock
    int force_exit;                                // set this to force exit of a process
    u32 sid;                                       // Session id
    u32 pgid;                                      // process group id
    u32 ppid;                                      // parent process id
    int waitable;                                  // this flag will be set as soon as status information is available
    cond_t unwaited;                               // handle unwaited children
    int unwaited_children;                         // actual number of unwaited children
    int exit_status;                               // exit status
    __ksigaction_t sig_actions[__NR_OF_SIGNALS];   // signal actions
    u32 sig_pending;                               // pending signals on process level
    uid_t euid;                                    // effective user ID
    uid_t uid;                                     // real user ID
    uid_t suid;                                    // saved set-user-id
    gid_t egid;                                    // effective group id
    gid_t gid;                                     // real group id
    gid_t sgid;                                    // saved set-group-id
    int exec;                                      // set to one if any task within this process does an exec
    clock_t utime;                                 // user time in ticks
    clock_t stime;                                 // kernel time in ticks
    clock_t cutime;                                // children user time
    clock_t cstime;                                // children kernel time
    dev_t cterm;                                   // controlling terminal
} proc_t;



/*
 * Here we store default actions for signals
 */
typedef struct {
    int sig_no;
    int default_action;
} sig_default_action_t ;

/*
 * How many tasks do we allow
 */
#define PM_MAX_TASK 1024

/*
 * How many procs do we allow
 */
#define PM_MAX_PROCESS 1024

/*
 * The interrupt number we use for system calls
 */
#define SYSCALL_IRQ 0x80

/*
 * Execution level
 */
#define EXECUTION_LEVEL_USER 0
#define EXECUTION_LEVEL_KTHREAD 1
#define EXECUTION_LEVEL_SYSCALL 2
#define EXECUTION_LEVEL_IRQ 3

/*
 * Task status
 */
#define TASK_STATUS_NEW 0
#define TASK_STATUS_RUNNING 1
#define TASK_STATUS_BLOCKED 2
#define TASK_STATUS_DONE 3
#define TASK_STATUS_STOPPED 4
#define TASK_STATUS_BLOCKED_INTR 5

/*
 * Default actions for signals
 */

#define SIG_DFL_TERM 1
#define SIG_DFL_IGN 2
#define SIG_DFL_STOP 3
#define SIG_DFL_CONT 4

/*
 * Constants for actions actually taken
 */
#define SIG_ACTION_NONE 0
#define SIG_ACTION_IGN 1
#define SIG_ACTION_STOPPED 2
#define SIG_ACTION_HANDLER 3
#define SIG_ACTION_TERM 4

/*
 * Exit reasons.
 */
#define EXIT_REASON_SUSPEND 0177

/*
 * Needs to match sys/wait.h
 */
#define __WIFEXITED(x)     (((x) & 0xff)==0)
#define __WIFSIGNALED(x)   (((x) & 0xff) && (((x) & 0xff) != 0177))
#define __WNOHANG 1
#define __WUNTRACED 2



void pm_init();
int pm_create_idle_task(int cpuid);
int pm_update_exec_level(ir_context_t* ir_context, int* old_level);
void pm_restore_exec_level(ir_context_t* ir_context, int old_level);
int do_pthread_create(pthread_t* thread, pthread_attr_t* attr, void* (*start_function)(void*), void* arg, ir_context_t* ir_context);
int pm_get_task_id();
int pm_switch_task(int task, ir_context_t* ir_context);
int pm_get_pid();
int do_fork(ir_context_t* ir_context);
void do_exit(int statusm);
int do_quit();
void pm_print_task_table();
void pm_do_tick(ir_context_t* ir_context, int cpuid);
int do_exec(char* path, char** argv, char** envp, ir_context_t* ir_context);
void pm_cleanup_task();
int do_sleep(time_t seconds);
pid_t do_waitpid(pid_t pid, int* stat_loc, int options, struct rusage* ru);
int do_kill(pid_t pid, int sig_no);
int pm_process_signals(ir_context_t* ir_context);
int do_sigaction(int sig_no, __ksigaction_t* act, __ksigaction_t* old);
int do_sigreturn(int sig_no, sig_frame_t* sigframe, ir_context_t* context);
int do_sigwait(u32 sig_set, int* sig);
int pm_handle_exit_requests();
int do_pause();
int do_sigpending(u32* sigmask);
int do_sigprocmask(int how, u32* set, u32* oset);
int do_sigsuspend(u32* set, u32* old_set);
int do_pthread_kill(u32 task_id, int sig_no);
int do_getpid();
void cond_reschedule();
int do_seteuid(uid_t euid);
uid_t do_geteuid();
int do_setuid(uid_t uid);
uid_t do_getuid();
uid_t do_getegid();
uid_t do_getgid();
int do_getppid();
int do_setpgid(pid_t pid, pid_t pgid);
pid_t do_getpgrp();
pid_t do_setpgrp();
int do_setsid();
void pm_attach_tty(dev_t tty);
dev_t pm_get_cterm();
pid_t do_getsid(pid_t pid);
int do_times(struct __ktms* times);
int pm_pgrp_in_session(int pid, int pgrp);
void pm_validate();
void wakeup_task(ecb_t* ecb);
void pm_handle_nm_trap();
#endif /* _PM_H_ */
