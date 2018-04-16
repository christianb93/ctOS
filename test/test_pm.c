/*
 * test_pm.c
 */

#include "kunit.h"
#include "pm.h"
#include "vga.h"
#include "irq.h"
#include "drivers.h"
#include <stdio.h>
#include "gdt.h"
#include "locks.h"
#include "lib/pthread.h"
#include "pagetables.h"
#include <time.h>
#include "lib/os/errors.h"
#include "lib/os/stat.h"
#include "lib/sys/wait.h"

extern u32 __sigreturn_start;
extern u32 __sigreturn_end;


extern void pm_task_exit_handler();
/*
 * Stub for reschedule
 */
void reschedule() {

}

void timer_time_ecb(ecb_t* ecb, u32 timeout) {

}

void timer_cancel_ecb(ecb_t* ecb) {

}

int do_alarm(int seconds) {
    return 0;
}

void wq_do_tick(int cpuid) {

}

int smp_get_cpu_count() {
    return 1;
}

int debug_running() {
    return 0;
}

void udelay(u32 us) {

}

u32 timer_get_ticks(int cpuid) {
    return 0;
}

void timer_time_sem(semaphore_t* sem, u32 timeout) {

}

void timer_cancel_sem(semaphore_t* sem) {

}

void debug_getline(void* buffer, int n) {

}

void debug_lock_wait(u32 lock, int type, int rw, char* file, int line) {

}

void debug_lock_acquired(u32 lock, int type) {

}

void debug_lock_cancel(u32 lock, int type) {

}

void debug_lock_released(u32 lock, int type) {

}

u32 get_eflags() {
    return 0;
}

void clts() {

}

void setts() {

}

void fpu_restore(unsigned int x) {

}

void fpu_save(unsigned int x) {

}


int smp_get_cpu() {
    return 0;
}


u32 atomic_load(u32* ptr) {
    return *ptr;
}

void atomic_store(u32* ptr, u32 value) {
    *ptr = value;
}

/*
 * Stub for fs_on_exec
 */
static int fs_on_exec_called = 0;
void fs_on_exec(int proc) {
    fs_on_exec_called = 1;
}

/*
 * Stub for do_stat
 */
static mode_t st_mode = 0;
static uid_t st_uid = 0;
int do_stat(char* path, struct __ctOS_stat* mystat) {
    mystat->st_mode = st_mode;
    mystat->st_uid = st_uid;
    return 0;
}

/*
 * Stub for win_putchar
 */
static int do_print = 0;
void win_putchar(win_t* win, u8 c) {
    if (do_print)
        printf("%c", c);
}

/*
 * Stub for do_time
 */
time_t do_time(time_t* res) {
    return time(res);
}

void sti() {

}

/*
 * Stub for kmalloc/kfree
 */
u32 kmalloc(size_t size) {
    return malloc(size);
}
void kfree(u32 addr) {
    free((void*) addr);
}
u32 kmalloc_aligned(size_t size, u32 alignment) {
    return malloc(size);
}

/*
 * Stub for scheduler
 */
int last_enqueued_task = -1;
void sched_enqueue(int task, int prio) {
    last_enqueued_task = task;
}
void sched_enqueue_cpu(int task, int prio, int cpuid) {
    last_enqueued_task = task;
}
static int last_dequeued_task = -1;
void sched_dequeue() {
    last_dequeued_task = pm_get_task_id();
}

/*
 * Stub for mm_reserve_task_stack
 */
static int task_tos = 0;
static int mm_reserve_task_stack_called = 0;
u32 mm_reserve_task_stack(int task_id, int pid, int *pages) {
    *pages = 2;
    mm_reserve_task_stack_called = 1;
    return task_tos;
}
int mm_release_task_stack(u32 task_id) {
    return 0;
}
/*
 * Stub for mm_get_kernel_stack
 */
u32 mm_get_kernel_stack(u32 task_id) {
    return 0;
}

/*
 * Stub for gdt_update_tss
 */
void gdt_update_tss(u32 esp0, int cpuid) {

}

/*
 * Stub for elf_load_executable
 */
int elf_load_executable(char* path, u32* ep) {
    return 0;
}

/*
 * Stub for goto_ring3
 */
void goto_ring3(u32 ep, u32 esp) {

}

/*
 * Stub for halt
 */
void halt() {

}

int __ctOS_syscall(int sysno, int arg, ...) {
    return 0;
}

/*
 * Stub for mm_init_user_area and
 * mm_teardown_user_area
 */
static u32 user_space_stack = 0;
int mm_init_user_area() {
    return user_space_stack;
}
void mm_teardown_user_area() {

}
void mm_release_page_tables() {

}
/*
 * Stub for get_cr0
 */
static u32 pg_enabled = 0;
u32 get_cr0() {
    return pg_enabled << 31;
}

void fs_close_all() {

}

/*
 * Stub for get_cr3
 */
static u32 cr3 = 0;
u32 get_cr3() {
    return cr3;
}

/*
 * Stub for trap
 */
static int trapped = 0;
void trap() {
    trapped = 1;
}

/*
 * Stubs for FS operations
 */
void fs_clone (u32 source_pid, u32 target_pid) {

}

/*
 * Set process group
 */
static int __pgrp[256];
void tty_setpgrp(minor_dev_t minor, pid_t pgrp) {
    __pgrp[minor] = pgrp;
}

/*
 * Return true if the passed code segment selector matches
 * the kernel code segment
 */
int mm_is_kernel_code(u32 code_segment) {
    return (code_segment == SELECTOR_CODE_KERNEL);
}

/*
 * Stub for cli
 */
void cli() {
}
/*
 * Stub for restore_eflags
 * and save eflags
 */
void restore_eflags(u32* flags) {
}
static int eflags = 0;
void save_eflags(u32* flags) {
    *flags = eflags;
}
/*
 * Stub for xchg
 */
u32 xchg(u32 reg, u32* mem) {
    u32 tmp;
    tmp = *mem;
    *mem = reg;
    return tmp;
}
/*
 * Stub for exec function
 */
void* my_exec(void* parm) {
    return 0;
}

/*
 * Stub for mm_clone
 */
u32 mm_clone() {
    return 0xffff;
}

/*
 * Stubs for spinlocks
 */
static u32 ie = 1;
void spinlock_init(spinlock_t* lock) {
    *((u32*) lock) = 0;
}

void atomic_incr(reg_t* mem) {
    (*mem)++;
}

void atomic_decr(int* mem) {
    (*mem)--;
}

void spinlock_get(spinlock_t* lock, u32* flags) {
    if (*((u32*) lock) == 1) {
        printf(
                "----------- Spinlock requested which is not available! ----------------\n");
        _exit(1);
    }
    *((u32*) lock) = 1;
    *flags = ie;
}
void spinlock_release(spinlock_t* lock, u32* flags) {
    if (0 == *((u32*) lock)) {
        printf("------------- Trying to release spinlock which is not held!-------------\n");
        _exit(1);
    }
    *((u32*) lock) = 0;
    ie = *flags;
}

extern u32 pm_prepare_signal_stack(u32 stack, int sig_no, ir_context_t* context, u32 sigmask, sig_frame_t** sigframe);

/*
 * Dummy signal handler
 */
void my_handler(int x) {

}

/*
 * Validation function for signal stack as prepared by pm_prepare_signal_stack
 * Parameter:
 * @esp - the address of the lowest byte on the stack (where the return address of the handler is stored)
 * @sig_no - the number of the signal for which the stack is prepared
 */
static int validate_signal_stack(unsigned int esp, int sig_no, ir_context_t* context, u32 sigmask) {
    unsigned char* ptr_code;
    unsigned char* ptr_stack;
    sig_frame_t* sigframe;
    u32 eip;
    u32* args;
    int i;
    /*
     * Get return address from stack
     */
    eip = *((u32*)esp);
    /*
     * Get parameters from stack. First parameter is sig_no
     */
    args = (u32*)(esp+4);
    ASSERT(sig_no==args[0]);
    /*
     * "Forth" parameter is sigframe structure
     */
    sigframe = (sig_frame_t*) &args[3];
    /*
     * Verify that code starting at eip is what we want. We first proceed until
     * we do no longer have a NOP
     */
    ptr_stack = (unsigned char*) eip;
    while (*ptr_stack == 0x90)
        ptr_stack++;
    /*
     * Then compare byte by byte with the code in sigreturn.o
     */
    ptr_code = (unsigned char*) &__sigreturn_start;
    while  (((u32)((ptr_code +i))) <= ((u32) &__sigreturn_end)) {
        ASSERT(*ptr_code==*ptr_stack);
        ptr_code++;
        ptr_stack++;
    }
    /*
     * Verify sigframe structure
     */
    ASSERT(sigframe->sigmask==sigmask);
    ASSERT(sigframe->eax == context->eax);
    ASSERT(sigframe->ebp == context->ebp);
    ASSERT(sigframe->ebx == context->ebx);
    ASSERT(sigframe->ecx == context->ecx);
    ASSERT(sigframe->edi == context->edi);
    ASSERT(sigframe->edx == context->edx);
    ASSERT(sigframe->eflags == context->eflags);
    ASSERT(sigframe->eip == context->eip);
    ASSERT(sigframe->esi == context->esi);
    ASSERT(sigframe->esp == context->esp);
    return 0;
}
/*
 * Testcase 1
 * Tested function: pm_setup_stack
 * Testcase: invalid parameters - not enough space
 */
int testcase1() {
    ir_context_t ir_context;
    u32 esp;
    ir_context.cs_old = SELECTOR_CODE_KERNEL;
    trapped = 0;
    int rc;
    u32 arg;
    rc = pm_setup_stack(0x10000-1, 0x10000, &ir_context, my_exec, (void*) &arg, &esp);
    ASSERT(0==trapped);
    ASSERT(rc);
    return 0;
}

/*
 * Testcase 2
 * Tested function: pm_setup_stack
 * Testcase: invalid parameters - ir context not originating in kernel space
 */
int testcase2() {
    ir_context_t ir_context;
    ir_context.cs_old = SELECTOR_DATA_KERNEL;
    trapped = 0;
    int rc;
    u32 esp;
    u32 arg;
    rc = pm_setup_stack(0x11000-1, 0x10000, &ir_context, my_exec, (void*) &arg, &esp);
    ASSERT(rc);
    return 0;
}

/*
 * Testcase 3
 * Tested function: pm_setup_stack
 * Testcase: valid parameters, test correct layout of stack
 */
int testcase3() {
    ir_context_t ir_context;
    u32 __attribute__ ((aligned (4096))) my_stack[100];
    ir_context.cs_old = SELECTOR_CODE_KERNEL;
    trapped = 0;
    int rc;
    u32 esp;
    u32 arg;
    rc = pm_setup_stack((u32)(my_stack)+100*4-1, (u32) my_stack,  &ir_context, my_exec, (void*) &arg, &esp);
    ASSERT(rc==0);
    ASSERT(0==trapped);
    /* Now verify correct layout of stack:
     * - top dword contains argument
     * - second dword contains address of task exit function
     * - below this, we have the ir_context which has been passed unchanged
     * with the following exception:
     * - EIP points to my_exec
     */
    ASSERT(my_stack[99]==((u32) &arg));
    ASSERT(my_stack[98]==((u32) pm_task_exit_handler));
    ASSERT(my_stack[97]==ir_context.eflags);
    ASSERT(my_stack[96]==ir_context.cs_old);
    ASSERT(my_stack[95]==(u32) my_exec);
    ASSERT(my_stack[94]==ir_context.err_code);
    ASSERT(my_stack[93]==ir_context.vector);
    ASSERT(my_stack[92]==ir_context.eax);
    ASSERT(my_stack[91]==ir_context.ebx);
    ASSERT(my_stack[90]==ir_context.ecx);
    ASSERT(my_stack[89]==ir_context.edx);
    ASSERT(my_stack[88]==ir_context.esi);
    ASSERT(my_stack[87]==ir_context.edi);
    ASSERT(my_stack[86]==ir_context.ebp);
    ASSERT(my_stack[85]==ir_context.ds);
    ASSERT(my_stack[84]==ir_context.cr2);
    ASSERT(my_stack[82]==ir_context.cr3);
    /*
     * Test that the returned stack pointer is correct
     */
    ASSERT(esp==(u32)&my_stack[84]);
    return 0;
}

/*
 * Testcase 4
 * Tested function: do_pthread_create
 * Testcase: call from user space is rejected
 */
int testcase4() {
    ir_context_t ir_context;
    pthread_t thread;
    u32 __attribute__ ((aligned (4096))) my_stack[100];
    ir_context.cs_old = SELECTOR_CODE_KERNEL+8;
    int rc = do_pthread_create(&thread, 0, my_exec, 0, &ir_context);
    ASSERT(rc);
    return 0;
}

/*
 * Testcase 5
 * Tested function: do_pthread_create
 * Testcase: normal processing
 */
int testcase5() {
    ir_context_t ir_context;
    pthread_t thread;
    thread = 0;
    u32 __attribute__ ((aligned (4096))) my_stack[8192];
    task_tos = (u32)(my_stack+99)+3;
    ir_context.cs_old = SELECTOR_CODE_KERNEL;
    int rc = do_pthread_create(&thread, 0, my_exec, (void*) 0x100, &ir_context);
    ASSERT(0==rc);
    /* Now perform validations:
     * - a valid ID has been written into the threads structure
     * - stub for mm_reserve_task_stack has been called
     * - at top of stack, the argument has been place
     */
    ASSERT(thread);
    ASSERT(1==mm_reserve_task_stack_called);
    ASSERT(*((u32*)(task_tos-3)) == 0x100);
    return 0;
}

/*
 * Testcase 6
 * Tested function: pm_init
 * Testcase: after initialization, currently active task is 0
 */
int testcase6() {
    pg_enabled=1;
    pm_init();
    ASSERT(0==pm_get_task_id());
    return 0;
}


/*
 * Testcase 8
 * Tested function: do_exec
 * Testcase: call do_exec with one argument and verify correct stack layout
 */
int testcase8() {
    ir_context_t ir_context;
    char* argv[2];
    u8 __attribute__  ((aligned (4))) test_stack[8192];
    u32 esp;
    u32* ptr;
    int stored_argc;
    char** stored_argv;
    argv[0]="ab";
    argv[1]=0;
    /*
     * Simulate case that we are called from user space
     */
    ir_context.cs_old = SELECTOR_CODE_USER;
    /*
     * Set the user space stack to our test stack
     */
    user_space_stack = (u32)(test_stack + 8192 - 4);
    pm_init();
    fs_on_exec_called = 0;
    ASSERT(0==do_exec("test", argv,  0, &ir_context));
    /*
     * Verify that stack pointer is valid
     */
    esp = *(&(ir_context.eflags) + 1);
    ASSERT(esp <= user_space_stack);
    ASSERT(esp>=(u32) test_stack);
    /*
     * Print stack
    for (ptr = (u32*)(user_space_stack);  ((u32) ptr) >= ((u32)esp) ; ptr--) {
        printf("%p:  %p\n", ptr, *ptr);
    }
    */
    /*
     * Argc should be located at ESP+4
     */
    stored_argc = *((int*)(esp+4));
    ASSERT(1==stored_argc);
    /*
     * Argv should be at ESP+8
     */
    stored_argv = (char**) (*((u32*)(esp+8)));
    ASSERT(stored_argv);
    /*
     * Argv[0] should be "ab"
     */
    ASSERT(stored_argv[0]);
    ASSERT(0==strncmp("ab", stored_argv[0],2 ));
    ASSERT(2==strlen(stored_argv[0]));
    /*
     * Argv[1] should be 0
     */
    ASSERT(stored_argv[1]==0);
    /*
     * We should have called fs_on_exec
     */
    ASSERT(1==fs_on_exec_called);
    return 0;
}

/*
 * Testcase 9
 * Tested function: do_exec
 * Testcase: call do_exec with two arguments and verify correct stack layout
 */
int testcase9() {
    ir_context_t ir_context;
    char* argv[3];
    u8 __attribute__  ((aligned (4))) test_stack[8192];
    u32 esp;
    u32* ptr;
    int stored_argc;
    char** stored_argv;
    argv[0]="ab";
    argv[1]="cde";
    argv[2]=0;
    /*
     * Simulate case that we are called from user space
     */
    ir_context.cs_old = SELECTOR_CODE_USER;
    /*
     * Set the user space stack to our test stack
     */
    user_space_stack = (u32)(test_stack + 8192 - 4);
    pm_init();
    ASSERT(0==do_exec("test", argv,  0, &ir_context));
    /*
     * Verify that stack pointer is valid
     */
    esp = *(&(ir_context.eflags) + 1);
    ASSERT(esp <= user_space_stack);
    ASSERT(esp>=(u32) test_stack);
    /*
     * Argc should be located at ESP+4
     */
    stored_argc = *((int*)(esp+4));
    ASSERT(2==stored_argc);
    /*
     * Argv should be at ESP+8
     */
    stored_argv = (char**) (*((u32*)(esp+8)));
    ASSERT(stored_argv);
    /*
     * Argv[0] should be "ab"
     */
    ASSERT(stored_argv[0]);
    ASSERT(0==strncmp("ab", stored_argv[0], 2));
    ASSERT(2==strlen(stored_argv[0]));
    /*
     * Argv[1] should be "cde"
     */
    ASSERT(stored_argv[1]);
    ASSERT(0==strncmp("cde", stored_argv[1], 3));
    ASSERT(3==strlen(stored_argv[1]));
    /*
     * Argv[2] should be 0
     */
    ASSERT(stored_argv[2]==0);
    return 0;
}

/*
 * Testcase 10
 * Tested function: do_exec
 * Testcase: call do_exec with empty argument list
 */
int testcase10() {
    ir_context_t ir_context;
    char* argv[3];
    u8 __attribute__  ((aligned (4))) test_stack[8192];
    u32 esp;
    u32* ptr;
    int stored_argc;
    char** stored_argv;
    argv[0]=0;
    /*
     * Simulate case that we are called from user space
     */
    ir_context.cs_old = SELECTOR_CODE_USER;
    /*
     * Set the user space stack to our test stack
     */
    user_space_stack = (u32)(test_stack + 8192 - 4);
    pm_init();
    ASSERT(0==do_exec("test", argv, 0,  &ir_context));
    /*
     * Verify that stack pointer is valid
     */
    esp = *(&(ir_context.eflags) + 1);
    ASSERT(esp <= user_space_stack);
    ASSERT(esp>=(u32) test_stack);
    /*
     * Argc should be located at ESP+4
     */
    stored_argc = *((int*)(esp+4));
    ASSERT(0==stored_argc);
    /*
     * Argv should be at ESP+8
     */
    stored_argv = (char**) (*((u32*)(esp+8)));
    ASSERT(stored_argv);
    /*
     * Argv[0] should be 0
     */
    ASSERT(0==stored_argv[0]);
    return 0;
}

/*
 * Tested function: pm_prepare_signal_stack
 */
int testcase11() {
    unsigned char stack[128];
    char* tos = &(stack[124]);
    sig_frame_t* sigframe;
    int i;
    memset(stack, 0, 128);
    ir_context_t ir_context;
    ir_context.eax = 1;
    ir_context.ebp = 2;
    ir_context.ebx = 3;
    ir_context.ecx = 4;
    ir_context.edi = 5;
    ir_context.edx = 6;
    ir_context.eflags = 7;
    ir_context.eip = 8;
    ir_context.esi = 9;
    ir_context.esp = 10;
    u32 esp = pm_prepare_signal_stack((u32) tos, 1, &ir_context, 5, &sigframe);
    /*
     * Print entire stack
     */
    if (0) {
        for (i=0;i<128;i++) {
            printf("%02x ", stack[i]);
            if (7==(i % 8))
                printf("\n");
        }
    }
    ASSERT(0==validate_signal_stack(esp, 1, &ir_context, 5));
    return 0;
}

/*
 * Testcase 12: do_kill
 * Send a signal to a process containing only one task which does not have any signals blocked. Verify that the
 * signals becomes pending
 */
int testcase12() {
    ir_context_t ir_context;
    u32 sigmask;
    int rc;
    pm_init();
    /*
     * As it is not allowed to deliver a signal to tasks 0 and 1,
     * we first have to call do_fork twice
     */
    ir_context.cs_old = SELECTOR_CODE_KERNEL;
    ASSERT(1==do_fork(&ir_context));
    ASSERT(2==do_fork(&ir_context));
    pm_switch_task(2, &ir_context);
    ASSERT(2==pm_get_task_id());
    ASSERT(2==pm_get_pid());
    rc=do_kill(0, 10);
    ASSERT(0==rc);
    ASSERT(0==do_sigpending(&sigmask));
    ASSERT((1<<10)==sigmask);
    return 0;
}

/*
 * Testcase 13: do_sigprocmask
 * Block a signal
 */
int testcase13() {
    unsigned int set;
    unsigned int oset;
    pm_init();
    /*
     * Block signal 10
     */
    set = (1 << 10);
    ASSERT(0==do_sigprocmask(__KSIG_BLOCK, &set, 0));
    ASSERT(0==do_sigprocmask(0, 0, &oset));
    ASSERT(oset==(1<<10));
    /*
     * Block signal 11
     */
    set = (1 << 11);
    ASSERT(0==do_sigprocmask(__KSIG_BLOCK, &set, 0));
    ASSERT(0==do_sigprocmask(0, 0, &oset));
    ASSERT(oset==((1<<10) | (1<<11)));
    return 0;
}

/*
 * Testcase 14: do_sigprocmask
 * Unblock a signal
 */
int testcase14() {
    unsigned int set;
    unsigned int oset;
    pm_init();
    /*
     * Block signal 10 and 11
     */
    set = ((1 << 10) | (1<<11));
    ASSERT(0==do_sigprocmask(__KSIG_BLOCK, &set, 0));
    ASSERT(0==do_sigprocmask(0, 0, &oset));
    ASSERT(oset==((1<<10) | (1<<11)));
    /*
     * Now unblock 11
     */
    set = (1 << 11);
    ASSERT(0==do_sigprocmask(__KSIG_UNBLOCK, &set, 0));
    ASSERT(0==do_sigprocmask(0, 0, &oset));
    ASSERT(oset==(1<<10));
    return 0;
}

/*
 * Testcase 15: do_sigprocmask
 * Set a signal mask
 */
int testcase15() {
    unsigned int set;
    unsigned int oset;
    pm_init();
    /*
     * Block signal 10 and 11
     */
    set = ((1 << 10) | (1<<11));
    ASSERT(0==do_sigprocmask(__KSIG_BLOCK, &set, 0));
    ASSERT(0==do_sigprocmask(0, 0, &oset));
    ASSERT(oset==((1<<10) | (1<<11)));
    /*
     * Now set signal mask to 1 << 12
     */
    set = (1 << 12);
    ASSERT(0==do_sigprocmask(__KSIG_SETMASK, &set, 0));
    ASSERT(0==do_sigprocmask(0, 0, &oset));
    ASSERT(oset==(1<<12));
    return 0;
}

/*
 * Testcase 16: do_kill
 * Send a signal to a process containing two tasks one of which has the signal blocked and verify that the signal
 * becomes pending for the other task
 */
int testcase16() {
    ir_context_t ir_context;
    pthread_t thread = 0;
    u32 __attribute__ ((aligned (4096))) my_stack[8192];
    u32 sigmask;
    int rc;
    pm_init();
    /*
     * As it is not allowed to deliver a signal to tasks 0 and 1,
     * we first have to call do_fork twice and switch to this process
     */
    task_tos = (u32)(my_stack+99)+3;
    ir_context.cs_old = SELECTOR_CODE_KERNEL;
    ASSERT(1==do_fork(&ir_context));
    ASSERT(2==do_fork(&ir_context));
    pm_switch_task(2, &ir_context);
    ASSERT(2==pm_get_task_id());
    ASSERT(2==pm_get_pid());
    /*
     * Now we create a new task within this process and switch to it
     */
    ir_context.cs_old = SELECTOR_CODE_KERNEL;
    rc = do_pthread_create(&thread, 0, my_exec, (void*) 0x100, &ir_context);
    ASSERT(0==rc);
    ASSERT(3==thread);
    pm_switch_task(3, &ir_context);
    ASSERT(3==pm_get_task_id());
    ASSERT(2==pm_get_pid());
    /*
     * Block signal 10 for this task
     */
    sigmask = (1 << 10);
    ASSERT(0==do_sigprocmask(__KSIG_SETMASK, &sigmask, 0));
    /*
     * Send kill
     */
    ASSERT(0==do_kill(2, 10));
    /*
     * and verify that it is not pending any more on process level or for this task
     */
    sigmask = 0;
    ASSERT(0==do_sigpending(&sigmask));
    ASSERT(0==sigmask);
    /*
     * but has been delivered to task 2
     */
    pm_switch_task(2, &ir_context);
    ASSERT(0==do_sigpending(&sigmask));
    ASSERT((1<<10)==sigmask);
    return 0;
}

/*
 * Testcase 17: do_kill
 * Send a signal to a process containing two tasks while both of them block the signal. Then unblock the signal for one of them
 */
int testcase17() {
    ir_context_t ir_context;
    pthread_t thread = 0;
    u32 __attribute__ ((aligned (4096))) my_stack[8192];
    u32 sigmask;
    int rc;
    pm_init();
    /*
     * As it is not allowed to deliver a signal to tasks 0 and 1,
     * we first have to call do_fork twice and switch to this process
     */
    task_tos = (u32)(my_stack+99)+3;
    ir_context.cs_old = SELECTOR_CODE_KERNEL;
    ASSERT(1==do_fork(&ir_context));
    ASSERT(2==do_fork(&ir_context));
    pm_switch_task(2, &ir_context);
    ASSERT(2==pm_get_task_id());
    ASSERT(2==pm_get_pid());
    /*
     * Block signal
     */
    sigmask = (1 << 10);
    ASSERT(0==do_sigprocmask(__KSIG_SETMASK, &sigmask, 0));
    /*
     * Now we create a new task within this process and switch to it
     */
    ir_context.cs_old = SELECTOR_CODE_KERNEL;
    rc = do_pthread_create(&thread, 0, my_exec, (void*) 0x100, &ir_context);
    ASSERT(0==rc);
    ASSERT(3==thread);
    pm_switch_task(3, &ir_context);
    ASSERT(3==pm_get_task_id());
    ASSERT(2==pm_get_pid());
    /*
     * Block signal 10 for this task as well
     */
    sigmask = (1 << 10);
    ASSERT(0==do_sigprocmask(__KSIG_SETMASK, &sigmask, 0));
    /*
     * Send kill
     */
    ASSERT(0==do_kill(2, 10));
    /*
     * and verify that it remains pending
     */
    sigmask = 0;
    ASSERT(0==do_sigpending(&sigmask));
    ASSERT((1<<10)==sigmask);
    /*
     * Now unblock signal again for task 3 and verify that it remains pending for task 2
     */
    sigmask = (1 << 10);
    ASSERT(0==do_sigprocmask(__KSIG_UNBLOCK, &sigmask, 0));
    sigmask = 0;
    ASSERT(0==do_sigpending(&sigmask));
    ASSERT((1<<10)==sigmask);
    /*
     * Switch to task 2 and verify that from its point of view, the signal is no longer pending
     */
    pm_switch_task(2, &ir_context);
    ASSERT(2==pm_get_task_id());
    sigmask = 0;
    ASSERT(0==do_sigpending(&sigmask));
    ASSERT(0==sigmask);
    return 0;
}

/*
 * Testcase 18: do_sigprocmask
 * Verify that signal SIGKILL cannot be blocked
 */
int testcase18() {
    unsigned int set;
    unsigned int oset;
    pm_init();
    /*
     * Block signal __KSIGKILL and 11
     */
    set = (1 << __KSIGKILL) | (1<<11);
    ASSERT(0==do_sigprocmask(__KSIG_SETMASK, &set, 0));
    /*
     * Verify that  __KSIGKILL cannot be blocked
     */
    ASSERT(0==do_sigprocmask(0, 0, &oset));
    ASSERT(oset==(1<<11));
    return 0;
}

/*
 * Testcase 19: do_sigprocmask
 * Verify that signal SIGSTOP cannot be blocked
 */
int testcase19() {
    unsigned int set;
    unsigned int oset;
    pm_init();
    /*
     * Block signal __KSIGSTOP and 11
     */
    set = (1 << __KSIGSTOP) | (1<<11);
    ASSERT(0==do_sigprocmask(__KSIG_BLOCK, &set, 0));
    /*
     * Verify that  __KSIGSTOP cannot be blocked
     */
    ASSERT(0==do_sigprocmask(0, 0, &oset));
    ASSERT(oset==(1<<11));
    return 0;
}

/*
 * Testcase 20: pm_process_signals / pthread_kill
 * Verify that a pending signal which is blocked remains pending
 */
int testcase20() {
    ir_context_t ir_context;
    u32 __attribute__ ((aligned (4096))) my_stack[8192];
    u32 sigmask;
    int rc;
    pm_init();
    /*
     * As it is not allowed to deliver a signal to tasks 0 and 1,
     * we first have to call do_fork twice and switch to this process
     */
    task_tos = (u32)(my_stack+99)+3;
    ir_context.cs_old = SELECTOR_CODE_USER;
    ASSERT(1==do_fork(&ir_context));
    ASSERT(2==do_fork(&ir_context));
    pm_switch_task(2, &ir_context);
    ASSERT(2==pm_get_task_id());
    ASSERT(2==pm_get_pid());
    /*
     * Block signal
     */
    sigmask = (1 << 10);
    ASSERT(0==do_sigprocmask(__KSIG_SETMASK, &sigmask, 0));
    /*
     * Now send signal to task and verify that it is pending
     */
    ASSERT(0==do_pthread_kill(2, 10));
    sigmask = 0;
    ASSERT(0==do_sigpending(&sigmask));
    ASSERT((1<<10)==sigmask);
    /*
     * Call pm_process_signal and verify that the signal remains pending
     */
    ASSERT(0==pm_process_signals(&ir_context));
    sigmask = 0;
    ASSERT(0==do_sigpending(&sigmask));
    ASSERT((1<<10)==sigmask);
    return 0;
}

/*
 * Testcase 21: pm_process_signals
 * Verify that a pending signal which is not blocked is processed - default action "termination"
 */
int testcase21() {
    ir_context_t ir_context;
    u32 __attribute__ ((aligned (4096))) my_stack[8192];
    u32 sigmask;
    int rc;
    pm_init();
    /*
     * As it is not allowed to deliver a signal to tasks 0 and 1,
     * we first have to call do_fork twice and switch to this process
     */
    task_tos = (u32)(my_stack+99)+3;
    ir_context.cs_old = SELECTOR_CODE_USER;
    ASSERT(1==do_fork(&ir_context));
    ASSERT(2==do_fork(&ir_context));
    pm_switch_task(2, &ir_context);
    ASSERT(2==pm_get_task_id());
    ASSERT(2==pm_get_pid());
    /*
     * Now send signal to task and verify that it is pending
     */
    ASSERT(0==do_pthread_kill(2, 10));
    sigmask = 0;
    ASSERT(0==do_sigpending(&sigmask));
    ASSERT((1<<10)==sigmask);
    /*
     * Call pm_process_signal and verify that the signal is no longer pending
     * and that the task has been marked for exit (use pm_handle_exit for that purpose)
     */
    ASSERT(0==pm_process_signals(&ir_context));
    sigmask = 0;
    ASSERT(0==do_sigpending(&sigmask));
    ASSERT(0==sigmask);
    ASSERT(1==pm_handle_exit_requests());
    return 0;
}

/*
 * Testcase 22: pm_process_signals
 * Verify that a pending signal which is blocked but ignored is processed
 */
int testcase22() {
    __ksigaction_t sa;
    ir_context_t ir_context;
    u32 __attribute__ ((aligned (4096))) my_stack[8192];
    u32 sigmask;
    int rc;
    pm_init();
    /*
     * As it is not allowed to deliver a signal to tasks 0 and 1,
     * we first have to call do_fork twice and switch to this process
     */
    task_tos = (u32)(my_stack+99)+3;
    ir_context.cs_old = SELECTOR_CODE_USER;
    ASSERT(1==do_fork(&ir_context));
    ASSERT(2==do_fork(&ir_context));
    pm_switch_task(2, &ir_context);
    ASSERT(2==pm_get_task_id());
    ASSERT(2==pm_get_pid());
    /*
     * Set sigaction to ignore
     */
    sa.sa_handler = __KSIG_IGN;
    sa.sa_flags = 0;
    ASSERT(0==do_sigaction(10, &sa, 0));
    /*
     * and block signal
     */
    sigmask = (1 << 10);
    ASSERT(0==do_sigprocmask(__KSIG_SETMASK, &sigmask, 0));
    /*
     * Now send signal to task and verify that it is pending
     */
    ASSERT(0==do_pthread_kill(2, 10));
    sigmask = 0;
    ASSERT(0==do_sigpending(&sigmask));
    ASSERT((1<<10)==sigmask);
    /*
     * Call pm_process_signal and verify that the signal is no longer pending
     * and that the task has not been marked for exit (use pm_handle_exit for that purpose)
     * and not been dequeued
     */
    last_dequeued_task = -1;
    ASSERT(0==pm_process_signals(&ir_context));
    sigmask = 0;
    ASSERT(0==do_sigpending(&sigmask));
    ASSERT(0==sigmask);
    ASSERT(0==pm_handle_exit_requests());
    ASSERT(-1==last_dequeued_task);
    return 0;
}

/*
 * Testcase 23: pm_process_signals
 * Verify that a pending SIGSTOP signal is processed
 */
int testcase23() {
    __ksigaction_t sa;
    ir_context_t ir_context;
    u32 __attribute__ ((aligned (4096))) my_stack[8192];
    u32 sigmask;
    int rc;
    pm_init();
    /*
     * As it is not allowed to deliver a signal to tasks 0 and 1,
     * we first have to call do_fork twice and switch to this process
     */
    task_tos = (u32)(my_stack+99)+3;
    ir_context.cs_old = SELECTOR_CODE_USER;
    ASSERT(1==do_fork(&ir_context));
    ASSERT(2==do_fork(&ir_context));
    pm_switch_task(2, &ir_context);
    ASSERT(2==pm_get_task_id());
    ASSERT(2==pm_get_pid());
    /*
     * Now send signal to task and verify that it is pending
     */
    ASSERT(0==do_pthread_kill(2, __KSIGSTOP));
    sigmask = 0;
    ASSERT(0==do_sigpending(&sigmask));
    ASSERT((1<<__KSIGSTOP)==sigmask);
    /*
     * Call pm_process_signal and verify that the signal is no longer pending
     * and that the task has not been marked for exit (use pm_handle_exit for that purpose)
     * and has been dequeued
     */
    last_dequeued_task = -1;
    ASSERT(0==pm_process_signals(&ir_context));
    sigmask = 0;
    ASSERT(0==do_sigpending(&sigmask));
    ASSERT(0==sigmask);
    ASSERT(0==pm_handle_exit_requests());
    ASSERT(2==last_dequeued_task);
    return 0;
}

/*
 * Testcase 24: pm_process_signals
 * Verify that a pending signal which has a handler installed is delivered
 */
int testcase24() {
    __ksigaction_t sa;
    /*
     * We need those two here as we need to simulate the full stack, even the part
     * above the IR context
     */
    unsigned int space1;
    unsigned int space2;
    ir_context_t ir_context;
    u32 old_eip;
    u32 __attribute__ ((aligned (4096))) my_stack[8192];
    u32 __attribute__ ((aligned (4096))) my_user_stack[8192];
    u32 sigmask;
    unsigned int* esp_ptr;
    unsigned int new_esp;
    int rc;
    pm_init();
    /*
     * As it is not allowed to deliver a signal to tasks 0 and 1,
     * we first have to call do_fork twice and switch to this process
     */
    task_tos = (u32)(my_stack+99)+3;
    ir_context.cs_old = SELECTOR_CODE_USER;
    ASSERT(1==do_fork(&ir_context));
    ASSERT(2==do_fork(&ir_context));
    pm_switch_task(2, &ir_context);
    ASSERT(2==pm_get_task_id());
    ASSERT(2==pm_get_pid());
    /*
     * Set sigaction
     */
    sa.sa_handler = my_handler;
    sa.sa_flags = 0;
    ASSERT(0==do_sigaction(10, &sa, 0));
    /*
     * Now send signal to task and verify that it is pending
     */
    ASSERT(0==do_pthread_kill(2, 10));
    sigmask = 0;
    ASSERT(0==do_sigpending(&sigmask));
    ASSERT((1<<10)==sigmask);
    /*
     * Call pm_process_signal and verify that the signal is no longer pending
     * and that the task has not been marked for exit (use pm_handle_exit for that purpose)
     * and has not been dequeued. Before we do that, set old value of ESP as that is used to determine
     * the address where the user space stack is built
     */
    esp_ptr = &(ir_context.eflags) + 1;
    *esp_ptr = (u32) (my_user_stack + 1024);
    last_dequeued_task = -1;
    old_eip = ir_context.eip;
    ASSERT(0==pm_process_signals(&ir_context));
    sigmask = 0;
    ASSERT(0==do_sigpending(&sigmask));
    ASSERT(0==sigmask);
    ASSERT(0==pm_handle_exit_requests());
    ASSERT(-1==last_dequeued_task);
    /*
     * Verify that the IR context has been patched as needed:
     * - eip points to address of handler
     * - the new value of the stack pointer stored above the IR context
     * points to a stack as in the validation of pm_prepare_stack_handler
     */
    ASSERT(ir_context.eip == (unsigned int) my_handler);
    new_esp = *esp_ptr;
    /*
     * Put old value of eip back into ir_context as otherwise the comparison
     * in validate_signal_stack will fail
     */
    ir_context.eip = old_eip;
    ASSERT(0==validate_signal_stack(new_esp, 10, &ir_context, 0));
    return 0;
}

/*
 * Testcase 25: pm_process_signals
 * Verify that all pending signals for which are either ignored or have their default action and the default is to ignore
 * it are processed by one call of the signal processing code
 */
int testcase25() {
    __ksigaction_t sa;
    ir_context_t ir_context;
    u32 __attribute__ ((aligned (4096))) my_stack[8192];
    u32 sigmask;
    int rc;
    pm_init();
    /*
     * As it is not allowed to deliver a signal to tasks 0 and 1,
     * we first have to call do_fork twice and switch to this process
     */
    task_tos = (u32)(my_stack+99)+3;
    ir_context.cs_old = SELECTOR_CODE_USER;
    ASSERT(1==do_fork(&ir_context));
    ASSERT(2==do_fork(&ir_context));
    pm_switch_task(2, &ir_context);
    ASSERT(2==pm_get_task_id());
    ASSERT(2==pm_get_pid());
    /*
     * Set sigaction to ignore for signal 10
     */
    sa.sa_handler = __KSIG_IGN;
    sa.sa_flags = 0;
    ASSERT(0==do_sigaction(10, &sa, 0));
    /*
     * and set signaction to default for 17
     */
    sa.sa_handler = __KSIG_DFL;
    sa.sa_flags = 0;
    ASSERT(0==do_sigaction(17, &sa, 0));
    /*
     * and block signals 10 and 17
     */
    sigmask = ((1 << 10)  | (1<<17));
    ASSERT(0==do_sigprocmask(__KSIG_SETMASK, &sigmask, 0));
    /*
     * Now send signals 10 and 17 to task and verify that it is pending
     */
    ASSERT(0==do_pthread_kill(2, 10));
    ASSERT(0==do_pthread_kill(2, 17));
    sigmask = 0;
    ASSERT(0==do_sigpending(&sigmask));
    ASSERT(((1<<10) | (1<<17))==sigmask);
    /*
     * Call pm_process_signal and verify that the signals are no longer pending
     * and that the task has not been marked for exit (use pm_handle_exit for that purpose)
     * and not been dequeued
     */
    last_dequeued_task = -1;
    ASSERT(0==pm_process_signals(&ir_context));
    sigmask = 0;
    ASSERT(0==do_sigpending(&sigmask));
    ASSERT(0==sigmask);
    ASSERT(0==pm_handle_exit_requests());
    ASSERT(-1==last_dequeued_task);
    return 0;
}

/*
 * Testcase 26: do_sigaction
 * Verify that a blocked pending signal is removed from the pending signal bitmask on process level if its action is set to
 * SIG_IGN
 */
int testcase26() {
    __ksigaction_t sa;
    ir_context_t ir_context;
    u32 __attribute__ ((aligned (4096))) my_stack[8192];
    u32 sigmask;
    int rc;
    pm_init();
    /*
     * As it is not allowed to deliver a signal to tasks 0 and 1,
     * we first have to call do_fork twice and switch to this process
     */
    task_tos = (u32)(my_stack+99)+3;
    ir_context.cs_old = SELECTOR_CODE_USER;
    ASSERT(1==do_fork(&ir_context));
    ASSERT(2==do_fork(&ir_context));
    pm_switch_task(2, &ir_context);
    ASSERT(2==pm_get_task_id());
    ASSERT(2==pm_get_pid());
    /*
     * block signal 10
     */
    sigmask = (1 << 10);
    ASSERT(0==do_sigprocmask(__KSIG_SETMASK, &sigmask, 0));
    /*
     * Now send signal 10 to process and verify that it is pending
     */
    ASSERT(0==do_kill(2, 10));
    sigmask = 0;
    ASSERT(0==do_sigpending(&sigmask));
    ASSERT((1<<10)==sigmask);
    /*
     * Set action to IGNORE
     */
    sa.sa_flags = 0;
    sa.sa_handler = __KSIG_IGN;
    ASSERT(0==do_sigaction(10, &sa, 0));
    /*
     * and verify that entry has been cleared
     */
    sigmask = 0;
    ASSERT(0==do_sigpending(&sigmask));
    ASSERT(0==sigmask);
    return 0;
}

/*
 * Testcase 27: do_sigaction
 * Verify that a blocked pending signal is removed from the pending signal bitmask on process level if its action is set to
 * SIG_DFL and the default is to ignore the signal
 */
int testcase27() {
    __ksigaction_t sa;
    ir_context_t ir_context;
    u32 __attribute__ ((aligned (4096))) my_stack[8192];
    u32 sigmask;
    int rc;
    pm_init();
    /*
     * As it is not allowed to deliver a signal to tasks 0 and 1,
     * we first have to call do_fork twice and switch to this process
     */
    task_tos = (u32)(my_stack+99)+3;
    ir_context.cs_old = SELECTOR_CODE_USER;
    ASSERT(1==do_fork(&ir_context));
    ASSERT(2==do_fork(&ir_context));
    pm_switch_task(2, &ir_context);
    ASSERT(2==pm_get_task_id());
    ASSERT(2==pm_get_pid());
    /*
     * block signal 17
     */
    sigmask = (1 << 17);
    ASSERT(0==do_sigprocmask(__KSIG_SETMASK, &sigmask, 0));
    /*
     * Now send signal 17 to process and verify that it is pending
     */
    ASSERT(0==do_kill(2, 17));
    sigmask = 0;
    ASSERT(0==do_sigpending(&sigmask));
    ASSERT((1<<17)==sigmask);
    /*
     * Set action to DEFAULT
     */
    sa.sa_flags = 0;
    sa.sa_handler = __KSIG_DFL;
    ASSERT(0==do_sigaction(17, &sa, 0));
    /*
     * and verify that entry has been cleared
     */
    sigmask = 0;
    ASSERT(0==do_sigpending(&sigmask));
    ASSERT(0==sigmask);
    return 0;
}

/*
 * Testcase 28: do_sigaction
 * Verify that a blocked pending signal is NOT removed from the pending signal bitmask on process level if its action is set to
 * SIG_DFL and the default is NOT to ignore the signal
 */
int testcase28() {
    __ksigaction_t sa;
    ir_context_t ir_context;
    u32 __attribute__ ((aligned (4096))) my_stack[8192];
    u32 sigmask;
    int rc;
    pm_init();
    /*
     * As it is not allowed to deliver a signal to tasks 0 and 1,
     * we first have to call do_fork twice and switch to this process
     */
    task_tos = (u32)(my_stack+99)+3;
    ir_context.cs_old = SELECTOR_CODE_USER;
    ASSERT(1==do_fork(&ir_context));
    ASSERT(2==do_fork(&ir_context));
    pm_switch_task(2, &ir_context);
    ASSERT(2==pm_get_task_id());
    ASSERT(2==pm_get_pid());
    /*
     * block signal 10
     */
    sigmask = (1 << 10);
    ASSERT(0==do_sigprocmask(__KSIG_SETMASK, &sigmask, 0));
    /*
     * Now send signal 10 to process and verify that it is pending
     */
    ASSERT(0==do_kill(2, 10));
    sigmask = 0;
    ASSERT(0==do_sigpending(&sigmask));
    ASSERT((1<<10)==sigmask);
    /*
     * Set action to DEFAULT
     */
    sa.sa_flags = 0;
    sa.sa_handler = __KSIG_DFL;
    ASSERT(0==do_sigaction(10, &sa, 0));
    /*
     * and verify that entry has not been cleared
     */
    sigmask = 0;
    ASSERT(0==do_sigpending(&sigmask));
    ASSERT((1<<10)==sigmask);
    return 0;
}

/*
 * Testcase 29: do_kill
 * Generate a SIGCONT for a stopped process which has blocked the signal and verify that it remains pending,
 * even though the process continues
 */
int testcase29() {
    __ksigaction_t sa;
    ir_context_t ir_context;
    u32 __attribute__ ((aligned (4096))) my_stack[8192];
    u32 sigmask;
    int rc;
    pm_init();
    /*
     * As it is not allowed to deliver a signal to tasks 0 and 1,
     * we first have to call do_fork twice and switch to this process
     */
    task_tos = (u32)(my_stack+99)+3;
    ir_context.cs_old = SELECTOR_CODE_USER;
    ASSERT(1==do_fork(&ir_context));
    ASSERT(2==do_fork(&ir_context));
    pm_switch_task(2, &ir_context);
    ASSERT(2==pm_get_task_id());
    ASSERT(2==pm_get_pid());
    /*
     * Block SIGCONT
     */
    sigmask = (1 << __KSIGCONT);
    ASSERT(0==do_sigprocmask(__KSIG_SETMASK, &sigmask, 0));
    /*
     * Stop the process
     */
    ASSERT(0==do_kill(2, __KSIGSTOP));
    ASSERT(0==pm_process_signals(&ir_context));
    /*
     * Now send signal SIGCONT and verify that it is pending, but the task has been scheduled again
     */
    last_enqueued_task = -1;
    ASSERT(0==do_kill(2, __KSIGCONT));
    sigmask = 0;
    ASSERT(0==do_sigpending(&sigmask));
    ASSERT((1<<__KSIGCONT)==sigmask);
    ASSERT(2==last_enqueued_task);
    /*
     * Simulate signal processing and verify that the signal remains still pending
     */
    ASSERT(0==pm_process_signals(&ir_context));
    sigmask = 0;
    ASSERT(0==do_sigpending(&sigmask));
    ASSERT((1<<__KSIGCONT)==sigmask);
    return 0;
}

/*
 * Testcase 30: stop a process using SIGSTOP
 */
int testcase30() {
    __ksigaction_t sa;
    ir_context_t ir_context;
    u32 __attribute__ ((aligned (4096))) my_stack[8192];
    u32 sigmask;
    int rc;
    pm_init();
    /*
     * As it is not allowed to deliver a signal to tasks 0 and 1,
     * we first have to call do_fork twice and switch to this process
     */
    task_tos = (u32)(my_stack+99)+3;
    ir_context.cs_old = SELECTOR_CODE_USER;
    ASSERT(1==do_fork(&ir_context));
    ASSERT(2==do_fork(&ir_context));
    pm_switch_task(2, &ir_context);
    ASSERT(2==pm_get_task_id());
    ASSERT(2==pm_get_pid());
    /*
     * Stop the process
     */
    ASSERT(0==do_kill(2, __KSIGSTOP));
    last_dequeued_task = -1;
    ASSERT(0==pm_process_signals(&ir_context));
    /*
     * Verify that the process has been descheduled
     */
    ASSERT(2==last_dequeued_task);
    return 0;
}

/*
 * Testcase 31: do_kill
 * Generate a SIGCONT for a  process which has blocked the signal and verify that it remains pending. Then
 * send SIGSTOP to the process and verify that the pending SIGCONT is cancelled
 */
int testcase31() {
    __ksigaction_t sa;
    ir_context_t ir_context;
    u32 __attribute__ ((aligned (4096))) my_stack[8192];
    u32 sigmask;
    int rc;
    pm_init();
    /*
     * As it is not allowed to deliver a signal to tasks 0 and 1,
     * we first have to call do_fork twice and switch to this process
     */
    task_tos = (u32)(my_stack+99)+3;
    ir_context.cs_old = SELECTOR_CODE_USER;
    ASSERT(1==do_fork(&ir_context));
    ASSERT(2==do_fork(&ir_context));
    pm_switch_task(2, &ir_context);
    ASSERT(2==pm_get_task_id());
    ASSERT(2==pm_get_pid());
    /*
     * Block SIGCONT
     */
    sigmask = (1 << __KSIGCONT);
    ASSERT(0==do_sigprocmask(__KSIG_SETMASK, &sigmask, 0));
    /*
     * Now send signal SIGCONT and verify that it is pending
     */
    last_enqueued_task = -1;
    ASSERT(0==do_kill(2, __KSIGCONT));
    sigmask = 0;
    ASSERT(0==do_sigpending(&sigmask));
    ASSERT((1<<__KSIGCONT)==sigmask);
    /*
     * Stop the process
     */
    ASSERT(0==do_kill(2, __KSIGSTOP));
    ASSERT(0==pm_process_signals(&ir_context));
    /*
     * Simulate signal processing and verify that SIGCONT is no longer pending
     */
    ASSERT(0==pm_process_signals(&ir_context));
    sigmask = 0;
    ASSERT(0==do_sigpending(&sigmask));
    ASSERT(0==sigmask);
    return 0;
}

/*
 * Testcase 32: do_kill
 * Generate a SIGCONT for a  process which has blocked the signal and verify that it remains pending. Then
 * send SIGTTIN to the process and verify that the pending SIGCONT is cancelled
 */
int testcase32() {
    __ksigaction_t sa;
    ir_context_t ir_context;
    u32 __attribute__ ((aligned (4096))) my_stack[8192];
    u32 sigmask;
    int rc;
    pm_init();
    /*
     * As it is not allowed to deliver a signal to tasks 0 and 1,
     * we first have to call do_fork twice and switch to this process
     */
    task_tos = (u32)(my_stack+99)+3;
    ir_context.cs_old = SELECTOR_CODE_USER;
    ASSERT(1==do_fork(&ir_context));
    ASSERT(2==do_fork(&ir_context));
    pm_switch_task(2, &ir_context);
    ASSERT(2==pm_get_task_id());
    ASSERT(2==pm_get_pid());
    /*
     * Block SIGCONT
     */
    sigmask = (1 << __KSIGCONT);
    ASSERT(0==do_sigprocmask(__KSIG_SETMASK, &sigmask, 0));
    /*
     * Now send signal SIGCONT and verify that it is pending
     */
    last_enqueued_task = -1;
    ASSERT(0==do_kill(2, __KSIGCONT));
    sigmask = 0;
    ASSERT(0==do_sigpending(&sigmask));
    ASSERT((1<<__KSIGCONT)==sigmask);
    /*
     * Stop the process
     */
    ASSERT(0==do_kill(2, __KSIGTTIN));
    ASSERT(0==pm_process_signals(&ir_context));
    /*
     * Simulate signal processing and verify that SIGCONT is no longer pending
     */
    ASSERT(0==pm_process_signals(&ir_context));
    sigmask = 0;
    ASSERT(0==do_sigpending(&sigmask));
    ASSERT(0==sigmask);
    return 0;
}

/*
 * Testcase 33: do_kill
 * Generate a pending STOP signal SIGSTOP and verify that it is cancelled if SIGCONT is sent
 */
int testcase33() {
    __ksigaction_t sa;
     ir_context_t ir_context;
     u32 __attribute__ ((aligned (4096))) my_stack[8192];
     u32 sigmask;
     int rc;
     pm_init();
     /*
      * As it is not allowed to deliver a signal to tasks 0 and 1,
      * we first have to call do_fork twice and switch to this process
      */
     task_tos = (u32)(my_stack+99)+3;
     ir_context.cs_old = SELECTOR_CODE_USER;
     ASSERT(1==do_fork(&ir_context));
     ASSERT(2==do_fork(&ir_context));
     pm_switch_task(2, &ir_context);
     ASSERT(2==pm_get_task_id());
     ASSERT(2==pm_get_pid());
     /*
      * Send SIGSTOP and verify that it is pending
      */
     ASSERT(0==do_kill(2, __KSIGSTOP));
     sigmask = 0;
     ASSERT(0==do_sigpending(&sigmask));
     ASSERT((1<<__KSIGSTOP)==sigmask);
     /*
      * Send SIGCONT and verify that SIGSTOP is no longer pending
      */
     ASSERT(0==do_kill(2, __KSIGCONT));
     sigmask = 0;
     ASSERT(0==do_sigpending(&sigmask));
     ASSERT(0==(sigmask & (1<<__KSIGSTOP)));
     return 0;
}

/*
 * Testcase 34: do_kill
 * Generate a pending STOP signal SIGTTIN and verify that it is cancelled if SIGCONT is sent
 */
int testcase34() {
    __ksigaction_t sa;
     ir_context_t ir_context;
     u32 __attribute__ ((aligned (4096))) my_stack[8192];
     u32 sigmask;
     int rc;
     pm_init();
     /*
      * As it is not allowed to deliver a signal to tasks 0 and 1,
      * we first have to call do_fork twice and switch to this process
      */
     task_tos = (u32)(my_stack+99)+3;
     ir_context.cs_old = SELECTOR_CODE_USER;
     ASSERT(1==do_fork(&ir_context));
     ASSERT(2==do_fork(&ir_context));
     pm_switch_task(2, &ir_context);
     ASSERT(2==pm_get_task_id());
     ASSERT(2==pm_get_pid());
     /*
      * Send SIGTTIN and verify that it is pending
      */
     ASSERT(0==do_kill(2, __KSIGTTIN));
     sigmask = 0;
     ASSERT(0==do_sigpending(&sigmask));
     ASSERT((1<<__KSIGTTIN)==sigmask);
     /*
      * Send SIGCONT and verify that SIGTTIN is no longer pending
      */
     ASSERT(0==do_kill(2, __KSIGCONT));
     sigmask = 0;
     ASSERT(0==do_sigpending(&sigmask));
     ASSERT(0==((1<<__KSIGTTIN) &sigmask));
     return 0;
}

/*
 * Testcase 35: pm_process_signals
 * Stop a process via SIGSTOP and verify that SIGCHLD is generated for the parent process
 */
int testcase35() {
    __ksigaction_t sa;
     ir_context_t ir_context;
     u32 __attribute__ ((aligned (4096))) my_stack[8192];
     u32 sigmask;
     int rc;
     pm_init();
     /*
      * As it is not allowed to deliver a signal to tasks 0 and 1,
      * we first have to call do_fork three times to create processes 2 and 3
      */
     task_tos = (u32)(my_stack+99)+3;
     ir_context.cs_old = SELECTOR_CODE_USER;
     ASSERT(1==do_fork(&ir_context));
     ASSERT(2==do_fork(&ir_context));
     ASSERT(1==pm_switch_task(2, &ir_context));
     ASSERT(2==do_getpid());
     ASSERT(3==do_fork(&ir_context));
     /*
      * Now we switch to process 3 and simulate that this process gets a stop signal
      */
     ASSERT(1==pm_switch_task(3, &ir_context));
     ASSERT(3==do_getpid());
     ASSERT(0==do_kill(3, __KSIGSTOP));
     last_dequeued_task=-1;
     ASSERT(0==pm_process_signals(&ir_context));
     ASSERT(3==last_dequeued_task);
     /*
      * Now switch back to parent (process 2) and verify that it has a pending SIGCHLD
      */
     ASSERT(1==pm_switch_task(2, &ir_context));
     ASSERT(2==do_getpid());
     sigmask = 0;
     ASSERT(0==do_sigpending(&sigmask));
     ASSERT((1<<__KSIGCHLD)==sigmask);
     return 0;
}

/*
 * Testcase 36: pm_process_signals
 * Stop a process via SIGSTOP and verify that SIGCHLD is NOT generated for the parent process if the parent
 * has set sa_mask to SA_NOCLDSTOP for signal SIGCHLD
 */
int testcase36() {
    __ksigaction_t sa;
     ir_context_t ir_context;
     u32 __attribute__ ((aligned (4096))) my_stack[8192];
     u32 sigmask;
     int rc;
     pm_init();
     /*
      * As it is not allowed to deliver a signal to tasks 0 and 1,
      * we first have to call do_fork three times to create processes 2 and 3
      */
     task_tos = (u32)(my_stack+99)+3;
     ir_context.cs_old = SELECTOR_CODE_USER;
     ASSERT(1==do_fork(&ir_context));
     ASSERT(2==do_fork(&ir_context));
     ASSERT(1==pm_switch_task(2, &ir_context));
     ASSERT(2==do_getpid());
     ASSERT(3==do_fork(&ir_context));
     /*
      * Use sigaction to set sa_flags to SA_NOCLDSTOP for signal SIGCHLD
      */
     sa.sa_flags = __KSA_NOCLDSTOP;
     sa.sa_handler = __KSIG_DFL;
     ASSERT(0==do_sigaction(__KSIGCHLD, &sa, 0));
     /*
      * Now we switch to process 3 and simulate that this process gets a stop signal
      */
     ASSERT(1==pm_switch_task(3, &ir_context));
     ASSERT(3==do_getpid());
     ASSERT(0==do_kill(3, __KSIGSTOP));
     last_dequeued_task=-1;
     ASSERT(0==pm_process_signals(&ir_context));
     ASSERT(3==last_dequeued_task);
     /*
      * Now switch back to parent (process 2) and verify that it does not have a pending SIGCHLD
      */
     ASSERT(1==pm_switch_task(2, &ir_context));
     ASSERT(2==do_getpid());
     sigmask = 0;
     ASSERT(0==do_sigpending(&sigmask));
     ASSERT(0==sigmask);
     return 0;
}

/*
 * Testcase 37: pm_process_signals
 * Verify that upon delivery of a pending signal for which a handler has been installed, the signal mask
 * is set to the union of the current signal mask, sa_mask for the signal to be delivered and the signal being delivered
 */
int testcase37() {
    __ksigaction_t sa;
    /*
     * We need those two here as we need to simulate the full stack, even the part
     * above the IR context
     */
    unsigned int space1;
    unsigned int space2;
    ir_context_t ir_context;
    u32 __attribute__ ((aligned (4096))) my_stack[8192];
    u32 __attribute__ ((aligned (4096))) my_user_stack[8192];
    u32 sigmask;
    unsigned int* esp_ptr;
    unsigned int new_esp;
    int rc;
    pm_init();
    /*
     * As it is not allowed to deliver a signal to tasks 0 and 1,
     * we first have to call do_fork twice and switch to this process
     */
    task_tos = (u32)(my_stack+99)+3;
    ir_context.cs_old = SELECTOR_CODE_USER;
    ASSERT(1==do_fork(&ir_context));
    ASSERT(2==do_fork(&ir_context));
    pm_switch_task(2, &ir_context);
    ASSERT(2==pm_get_task_id());
    ASSERT(2==pm_get_pid());
    /*
     * Set sigaction and include signal 17 in sa_mask
     */
    sa.sa_handler = my_handler;
    sa.sa_flags = 0;
    sa.sa_mask = (1 << 17);
    ASSERT(0==do_sigaction(10, &sa, 0));
    /*
     * Now send signal to task and verify that it is pending
     */
    ASSERT(0==do_pthread_kill(2, 10));
    sigmask = 0;
    ASSERT(0==do_sigpending(&sigmask));
    ASSERT((1<<10)==sigmask);
    /*
     * Call pm_process_signal and verify that the signal is no longer pending
     * and that the signal mask is set to
     * (1 << 10) | (1<<17)
     */
    esp_ptr = &(ir_context.eflags) + 1;
    *esp_ptr = (u32) (my_user_stack + 1024);
    ASSERT(0==pm_process_signals(&ir_context));
    sigmask = 0;
    ASSERT(0==do_sigpending(&sigmask));
    ASSERT(0==sigmask);
    sigmask = 0;
    ASSERT(0==do_sigprocmask(0, 0, &sigmask));
    ASSERT((((1<<10) | (1<<17)) == sigmask));
    return 0;
}

/*
 * Testcase 38: do_kill
 * Stop a process via SIGSTOP and verify that if a SIGKILL is sent to the process, it resumes execution
 */
int testcase38() {
    __ksigaction_t sa;
     ir_context_t ir_context;
     u32 __attribute__ ((aligned (4096))) my_stack[8192];
     u32 sigmask;
     int rc;
     pm_init();
     /*
      * As it is not allowed to deliver a signal to tasks 0 and 1,
      * we first have to call do_fork two times
      */
     task_tos = (u32)(my_stack+99)+3;
     ir_context.cs_old = SELECTOR_CODE_USER;
     ASSERT(1==do_fork(&ir_context));
     ASSERT(2==do_fork(&ir_context));
     ASSERT(1==pm_switch_task(2, &ir_context));
     ASSERT(2==do_getpid());
     /*
      * Stop process
      */
     ASSERT(0==do_kill(2, __KSIGSTOP));
     last_dequeued_task=-1;
     ASSERT(0==pm_process_signals(&ir_context));
     ASSERT(2==last_dequeued_task);
     /*
      * Now send SIGKILL and verify that task is enqueued again
      */
     last_enqueued_task = -1;
     ASSERT(0==do_kill(2, __KSIGKILL));
     ASSERT(2==last_enqueued_task);
     return 0;
}

/*
 * Testcase 39: pm_cleanup task / generation of SIGCHLD
 * Verify that if a process is terminated, pm_cleanup_task will send a SIGCHLD to the parent
 */
int testcase39() {
    __ksigaction_t sa;
     ir_context_t ir_context;
     u32 __attribute__ ((aligned (4096))) my_stack[8192];
     u32 sigmask;
     int rc;
     pm_init();
     /*
      * As it is not allowed to deliver a signal to tasks 0 and 1,
      * we first have to call do_fork three
      */
     task_tos = (u32)(my_stack+99)+3;
     ir_context.cs_old = SELECTOR_CODE_USER;
     ASSERT(1==do_fork(&ir_context));
     ASSERT(2==do_fork(&ir_context));
     /*
      * Switch to task 2 now so that ppid of the next process
      * will be 2
      */
     ASSERT(1==pm_switch_task(2, &ir_context));
     ASSERT(2==do_getpid());
     ASSERT(3==do_fork(&ir_context));
     /*
      * Next we switch to task 3 and exit it
      */
     ASSERT(1==pm_switch_task(3, &ir_context));
     ASSERT(3==do_getpid());
     do_exit(0);
     /*
      * So far, we have only set the exit flag. We now have to simulate the following sequence as it would occur
      * in a real interrupt handler for the do_exit system call:
      * - do_exit returns
      * - pm_handle_exit_requests is called
      * - a task switch is requested
      * - the cleanup handler is called
      * - execution of another process, say process 2, continues
      */
     ASSERT(1==pm_handle_exit_requests());
     ASSERT(1==pm_switch_task(2, &ir_context));
     pm_cleanup_task();
     sigmask = 0;
     /*
      * Note that the signal will be pending even though it is ignored as we do not
      * disregard ignored signals upon generation, but only upon delivery
      */
     ASSERT(0==do_sigpending(&sigmask));
     ASSERT((1<<__KSIGCHLD)==sigmask);
     return 0;
}

/*
 * Testcase 40: do_waitpid
 * Verify that after a process has been terminated by pm_cleanup_task, do_waitpid returns if
 * status information for that process has been requested
 */
int testcase40() {
     ir_context_t ir_context;
     u32 __attribute__ ((aligned (4096))) my_stack[8192];
     int status;
     pm_init();
     /*
      * First we build our process tree. We generate processes 1 and 2 as children of 0
      * then we switch to process 2 and do another fork to create process 3 as child of 2
      */
     task_tos = (u32)(my_stack+99)+3;
     ir_context.cs_old = SELECTOR_CODE_USER;
     ASSERT(1==do_fork(&ir_context));
     ASSERT(2==do_fork(&ir_context));
     ASSERT(1==pm_switch_task(2, &ir_context));
     ASSERT(2==do_getpid());
     ASSERT(3==do_fork(&ir_context));
     /*
      * Next we switch to task 3 and exit it with status 0x11 - could be any
      */
     ASSERT(1==pm_switch_task(3, &ir_context));
     ASSERT(3==do_getpid());
     do_exit(0x11);
     /*
      * So far, we have only set the exit flag. We now have to simulate the following sequence as it would occur
      * in a real interrupt handler for the do_exit system call:
      * - do_exit returns
      * - pm_handle_exit_requests is called
      * - a task switch is requested
      * - the cleanup handler is called
      * - execution of another process, say process 2, continues
      */
     ASSERT(1==pm_handle_exit_requests());
     ASSERT(1==pm_switch_task(2, &ir_context));
     pm_cleanup_task();
     /*
      * Now we call wait from within process 2 and verify that it returns PID 3 and the correct status
      */
     ASSERT(3==do_waitpid(3, &status, 0, 0));
     ASSERT(WIFEXITED(status));
     ASSERT(0x11==WEXITSTATUS(status));
     return 0;
}

/*
 * Testcase 41: do_waitpid
 * Verify that when do_waitpid is called for a non-existing process, it returns with error code -ECHILD
 */
int testcase41() {
     ir_context_t ir_context;
     u32 __attribute__ ((aligned (4096))) my_stack[8192];
     int status;
     pm_init();
     /*
      * First we build our process tree. We generate processes 1 and 2 as children of 0
      * then we switch to process 2
      */
     task_tos = (u32)(my_stack+99)+3;
     ir_context.cs_old = SELECTOR_CODE_USER;
     ASSERT(1==do_fork(&ir_context));
     ASSERT(2==do_fork(&ir_context));
     ASSERT(1==pm_switch_task(2, &ir_context));
     ASSERT(2==do_getpid());
     /*
      * Now we call wait from within process 2 for a non-existing process id
      */
     ASSERT(-ECHILD==do_waitpid(3, &status, 0, 0));
     return 0;
}

/*
 * Testcase 42: do_waitpid
 * Verify that when do_waitpid is called for an existing process which is not a child - expect -ECHILD as well
 */
int testcase42() {
     ir_context_t ir_context;
     u32 __attribute__ ((aligned (4096))) my_stack[8192];
     int status;
     pm_init();
     /*
      * First we build our process tree. We generate processes 1 and 2 as children of 0
      * then we switch to process 2
      */
     task_tos = (u32)(my_stack+99)+3;
     ir_context.cs_old = SELECTOR_CODE_USER;
     ASSERT(1==do_fork(&ir_context));
     ASSERT(2==do_fork(&ir_context));
     ASSERT(1==pm_switch_task(2, &ir_context));
     ASSERT(2==do_getpid());
     /*
      * Now we call wait from within process 2 for a existing process which however is not a child of 2
      */
     ASSERT(-ECHILD==do_waitpid(1, &status, 0, 0));
     return 0;
}

/*
 * Testcase 43: do_waitpid
 * Verify that when do_waitpid is called for all children but does not have any children at all, it returns -ECHILD
 */
int testcase43() {
     ir_context_t ir_context;
     u32 __attribute__ ((aligned (4096))) my_stack[8192];
     int status;
     pm_init();
     /*
      * First we build our process tree. We generate processes 1 and 2 as children of 0
      * then we switch to process 2
      */
     task_tos = (u32)(my_stack+99)+3;
     ir_context.cs_old = SELECTOR_CODE_USER;
     ASSERT(1==do_fork(&ir_context));
     ASSERT(2==do_fork(&ir_context));
     ASSERT(1==pm_switch_task(2, &ir_context));
     ASSERT(2==do_getpid());
     /*
      * Now we call wait from within process 2 for all children - should give -ECHILD, as 2 does not have any children
      */
     ASSERT(-ECHILD==do_waitpid(-1, &status, 0, 0));
     return 0;
}


/*
 * Testcase 44: do_waitpid
 * Verify that after a process has been terminated by pm_cleanup_task, do_waitpid returns if
 * status information for that process has been requested - call waitpid with pid -1 (any child)
 */
int testcase44() {
     ir_context_t ir_context;
     u32 __attribute__ ((aligned (4096))) my_stack[8192];
     int status;
     pm_init();
     /*
      * First we build our process tree. We generate processes 1 and 2 as children of 0
      * then we switch to process 2 and do another fork to create process 3 as child of 2
      */
     task_tos = (u32)(my_stack+99)+3;
     ir_context.cs_old = SELECTOR_CODE_USER;
     ASSERT(1==do_fork(&ir_context));
     ASSERT(2==do_fork(&ir_context));
     ASSERT(1==pm_switch_task(2, &ir_context));
     ASSERT(2==do_getpid());
     ASSERT(3==do_fork(&ir_context));
     /*
      * Next we switch to task 3 and exit it with status 0x11 - could be any
      */
     ASSERT(1==pm_switch_task(3, &ir_context));
     ASSERT(3==do_getpid());
     do_exit(0x11);
     /*
      * So far, we have only set the exit flag. We now have to simulate the following sequence as it would occur
      * in a real interrupt handler for the do_exit system call:
      * - do_exit returns
      * - pm_handle_exit_requests is called
      * - a task switch is requested
      * - the cleanup handler is called
      * - execution of another process, say process 2, continues
      */
     ASSERT(1==pm_handle_exit_requests());
     ASSERT(1==pm_switch_task(2, &ir_context));
     pm_cleanup_task();
     /*
      * Now we call wait from within process 2 and verify that it returns PID 3 and the correct status
      */
     ASSERT(3==do_waitpid(-1, &status, 0, 0));
     ASSERT(WIFEXITED(status));
     ASSERT(0x11==WEXITSTATUS(status));
     return 0;
}

/*
 * Testcase 45: do_waitpid
 * Verify that after two child processes has been terminated by pm_cleanup_task, subsequent calls of do_waitpid return
 * status information for those processes - call waitpid with pid -1 (any child)
 */
int testcase45() {
     ir_context_t ir_context;
     u32 __attribute__ ((aligned (4096))) my_stack[8192];
     int status;
     int pid;
     int done[2];
     pm_init();
     /*
      * First we build our process tree. We generate processes 1 and 2 as children of 0
      * then we switch to process 2 and do another fork to create process 3 and 4 as children of 2
      */
     task_tos = (u32)(my_stack+99)+3;
     ir_context.cs_old = SELECTOR_CODE_USER;
     ASSERT(1==do_fork(&ir_context));
     ASSERT(2==do_fork(&ir_context));
     ASSERT(1==pm_switch_task(2, &ir_context));
     ASSERT(2==do_getpid());
     ASSERT(3==do_fork(&ir_context));
     ASSERT(4==do_fork(&ir_context));
     /*
      * Next we switch to task 3 and exit it with status 0x11 - could be any
      */
     ASSERT(1==pm_switch_task(3, &ir_context));
     ASSERT(3==do_getpid());
     do_exit(0x11);
     /*
      * So far, we have only set the exit flag. We now have to simulate the following sequence as it would occur
      * in a real interrupt handler for the do_exit system call:
      * - do_exit returns
      * - pm_handle_exit_requests is called
      * - a task switch is requested
      * - the cleanup handler is called
      * - execution of another process, say process 4, continues
      */
     ASSERT(1==pm_handle_exit_requests());
     ASSERT(1==pm_switch_task(4, &ir_context));
     pm_cleanup_task();
     /*
      * Repeat the entire procedure for process 4 and switch back to process 2
      */
     do_exit(0x12);
     ASSERT(1==pm_handle_exit_requests());
     ASSERT(1==pm_switch_task(2, &ir_context));
     pm_cleanup_task();
     /*
      * Now we call waitpid from within process 2 two times
      */
     done[0]=0;
     done[1]=0;
     pid = do_waitpid(-1, &status, 0, 0);
     ASSERT(WIFEXITED(status));
     ASSERT(((pid==3) || (pid==4)));
     done[pid-3]=0;
     if (3==pid)
         ASSERT(0x11==WEXITSTATUS(status));
     if (4==pid)
         ASSERT(0x12==status);
     pid = do_waitpid(-1, &status, 0, 0);
     ASSERT(WIFEXITED(status));
     ASSERT(((pid==3) || (pid==4)));
     done[pid-3]=0;
     if (3==pid)
         ASSERT(0x11==WEXITSTATUS(status));
     if (4==pid)
         ASSERT(0x12==WEXITSTATUS(status));
     /*
      * By now both pids should have been processed
      */
     ASSERT(done[0]==0);
     ASSERT(done[1]==0);
     /*
      * Next call should return -ECHILD
      */
     ASSERT(-ECHILD==do_waitpid(-1, &status, 0, 0));
     return 0;
}

/*
 * Testcase 46: do_waitpid
 * Verify that if do_waitpid is called with WNOHANG and no status information is available, 0 is returned
 */
int testcase46() {
     ir_context_t ir_context;
     u32 __attribute__ ((aligned (4096))) my_stack[8192];
     int status;
     int pid;
     int done[2];
     pm_init();
     /*
      * First we build our process tree. We generate processes 1 and 2 as children of 0
      * then we switch to process 2 and create process 3
      */
     task_tos = (u32)(my_stack+99)+3;
     ir_context.cs_old = SELECTOR_CODE_USER;
     ASSERT(1==do_fork(&ir_context));
     ASSERT(2==do_fork(&ir_context));
     ASSERT(1==pm_switch_task(2, &ir_context));
     ASSERT(2==do_getpid());
     ASSERT(3==do_fork(&ir_context));
     /*
      * Call waitpid
      */
     ASSERT(0==do_waitpid(3, &status, __WNOHANG, 0));
     return 0;
}

/*
 * Testcase 47: do_waitpid
 * Verify that after two child processes has been terminated by pm_cleanup_task,subsequent calls of do_waitpid return
 * status information for those processes and after calling wait two times, any blocked and pending SIGCHLD signals have been
 * discarded
 */
int testcase47() {
     ir_context_t ir_context;
     u32 sigmask;
     u32 __attribute__ ((aligned (4096))) my_stack[8192];
     int status;
     int pid;
     int done[2];
     pm_init();
     /*
      * First we build our process tree. We generate processes 1 and 2 as children of 0
      * then we switch to process 2 and do another fork to create process 3 and 4 as children of 2
      */
     task_tos = (u32)(my_stack+99)+3;
     ir_context.cs_old = SELECTOR_CODE_USER;
     ASSERT(1==do_fork(&ir_context));
     ASSERT(2==do_fork(&ir_context));
     ASSERT(1==pm_switch_task(2, &ir_context));
     ASSERT(2==do_getpid());
     ASSERT(3==do_fork(&ir_context));
     ASSERT(4==do_fork(&ir_context));
     /*
      * Block sigchld
      */
     sigmask = (1 << __KSIGCHLD);
     ASSERT(0==do_sigprocmask(__KSIG_BLOCK, &sigmask, 0));
     /*
      * Next we switch to task 3 and exit it with status 0x11 - could be any
      */
     ASSERT(1==pm_switch_task(3, &ir_context));
     ASSERT(3==do_getpid());
     do_exit(0x11);
     /*
      * So far, we have only set the exit flag. We now have to simulate the following sequence as it would occur
      * in a real interrupt handler for the do_exit system call:
      * - do_exit returns
      * - pm_handle_exit_requests is called
      * - a task switch is requested
      * - the cleanup handler is called
      * - execution of another process, say process 4, continues
      */
     ASSERT(1==pm_handle_exit_requests());
     ASSERT(1==pm_switch_task(4, &ir_context));
     pm_cleanup_task();
     /*
      * Repeat the entire procedure for process 4 and switch back to process 2
      */
     do_exit(0x12);
     ASSERT(1==pm_handle_exit_requests());
     ASSERT(1==pm_switch_task(2, &ir_context));
     pm_cleanup_task();
     /*
      * Verify that there is a SIGCHLD pending
      */
     sigmask = 0;
     ASSERT(0==do_sigpending(&sigmask));
     ASSERT((1<<__KSIGCHLD)==sigmask);
     /*
      * Now we call waitpid from within process 2 two times
      */
     done[0]=0;
     done[1]=0;
     pid = do_waitpid(-1, &status, 0, 0);
     ASSERT(((pid==3) || (pid==4)));
     done[pid-3]=0;
     ASSERT(WIFEXITED(status));
     if (3==pid)
         ASSERT(0x11==WEXITSTATUS(status));
     if (4==pid)
         ASSERT(0x12==WEXITSTATUS(status));
     /*
      * SIGCHLD should still be pending at this point
      */
     sigmask = 0;
     ASSERT(0==do_sigpending(&sigmask));
     ASSERT((1<<__KSIGCHLD)==sigmask);
     pid = do_waitpid(-1, &status, 0, 0);
     ASSERT(WIFEXITED(status));
     ASSERT(((pid==3) || (pid==4)));
     done[pid-3]=0;
     if (3==pid)
         ASSERT(0x11==WEXITSTATUS(status));
     if (4==pid)
         ASSERT(0x12==WEXITSTATUS(status));
     /*
      * By now both pids should have been processed and SIGCHLD should be clear
      */
     ASSERT(done[0]==0);
     ASSERT(done[1]==0);
     sigmask = 0;
     ASSERT(0==do_sigpending(&sigmask));
     ASSERT(0==sigmask);

     /*
      * Next call should return -ECHILD
      */
     ASSERT(-ECHILD==do_waitpid(-1, &status, 0, 0));
     return 0;
}


/*
 * Testcase 48: geteuid
 * Verify that a newly created process has euid 0
 */
int testcase48() {
    pg_enabled = 1;
    pm_init();
    ASSERT(0==do_geteuid());
    return 0;
}

/*
 * Testcase 49: seteuid
 * Verify that a process with euid 0 can set the effective user ID to any other value
 */
int testcase49() {
    pg_enabled = 1;
    pm_init();
    ASSERT(0==do_geteuid());
    ASSERT(0==do_seteuid(1));
    ASSERT(1==do_geteuid());
    return 0;
}

/*
 * Testcase 50: seteuid
 * Verify that a process with euid 1 can not set the effective user ID to an arbitrary value
 */
int testcase50() {
    pg_enabled = 1;
    pm_init();
    ASSERT(0==do_geteuid());
    ASSERT(0==do_seteuid(1));
    ASSERT(1==do_geteuid());
    ASSERT(do_seteuid(2));
    ASSERT(1==do_geteuid());
    return 0;
}

/*
 * Testcase 51: seteuid
 * Verify that a process with euid 1 can set the effective user ID back to the real user ID
 */
int testcase51() {
    pg_enabled = 1;
    pm_init();
    ASSERT(0==do_geteuid());
    ASSERT(0==do_seteuid(1));
    ASSERT(1==do_geteuid());
    ASSERT(0==do_seteuid(0));
    ASSERT(0==do_geteuid());
    return 0;
}

/*
 * Testcase 52: setuid
 * Verify that a process with euid 0 can set its user ID to any value
 */
int testcase52() {
    pg_enabled = 1;
    pm_init();
    ASSERT(0==do_geteuid());
    ASSERT(0==do_setuid(1));
    ASSERT(1==do_geteuid());
    ASSERT(1==do_getuid());
    return 0;
}

/*
 * Testcase 53: setuid
 * Verify that a process with euid 1 cannot set its user ID to any value
 */
int testcase53() {
    pg_enabled = 1;
    pm_init();
    ASSERT(0==do_geteuid());
    ASSERT(0==do_setuid(1));
    ASSERT(1==do_geteuid());
    ASSERT(1==do_getuid());
    /*
     * Now we are no longer privileged - call should fail
     */
    ASSERT(do_setuid(2));
    ASSERT(1==do_geteuid());
    ASSERT(1==do_getuid());
    return 0;
}

/*
 * Testcase 54: setuid
 * Verify that a process with euid 1 can set its uid back to the real user ID
 */
int testcase54() {
    pg_enabled = 1;
    pm_init();
    ASSERT(0==do_geteuid());
    ASSERT(0==do_seteuid(1));
    ASSERT(1==do_geteuid());
    /*
     * Now we are running with effective user id 1
     */
    ASSERT(0==do_setuid(0));
    ASSERT(0==do_geteuid());
    ASSERT(0==do_getuid());
    return 0;
}

/*
 * Testcase 55
 * Tested function: do_exec
 * Testcase: call do_exec with one environment string and verify correct stack layout
 */
int testcase55() {
    ir_context_t ir_context;
    char* argv[2];
    char* env[2];
    u8 __attribute__  ((aligned (4))) test_stack[8192];
    u32 esp;
    u32* ptr;
    int stored_argc;
    char** stored_argv;
    char** stored_env;
    argv[0]="ab";
    argv[1]=0;
    env[0]="HOME=x";
    env[1]=0;
    /*
     * Simulate case that we are called from user space
     */
    ir_context.cs_old = SELECTOR_CODE_USER;
    /*
     * Set the user space stack to our test stack
     */
    user_space_stack = (u32)(test_stack + 8192 - 4);
    pm_init();
    fs_on_exec_called = 0;
    ASSERT(0==do_exec("test", argv,  env, &ir_context));
    /*
     * Verify that stack pointer is valid
     */
    esp = *(&(ir_context.eflags) + 1);
    ASSERT(esp <= user_space_stack);
    ASSERT(esp>=(u32) test_stack);
    /*
     * Print stack
    for (ptr = (u32*)(user_space_stack);  ((u32) ptr) >= ((u32)esp) ; ptr--) {
        printf("%p:  %p\n", ptr, *ptr);
    }
    */
    /*
     * Argc should be located at ESP+4
     */
    stored_argc = *((int*)(esp+4));
    ASSERT(1==stored_argc);
    /*
     * Argv should be at ESP+8
     */
    stored_argv = (char**) (*((u32*)(esp+8)));
    ASSERT(stored_argv);
    /*
     * Env should be at ESP+12
     */
    stored_env = (char**) (*((u32*)(esp+12)));
    ASSERT(stored_env);
    /*
     * Env[0] should be "HOME=x"
     */
    ASSERT(stored_env[0]);
    ASSERT(0==strcmp("HOME=x", stored_env[0]));
    /*
     * Env[1] should be 0
     */
    ASSERT(0==stored_env[1]);
    return 0;
}

/*
 * Testcase 56
 * Tested function: do_exec
 * Testcase: call do_exec with an empty environment (i.e. env[0] is null) and verify correct stack layout
 */
int testcase56() {
    ir_context_t ir_context;
    char* argv[2];
    char* env[1];
    u8 __attribute__  ((aligned (4))) test_stack[8192];
    u32 esp;
    u32* ptr;
    int stored_argc;
    char** stored_argv;
    char** stored_env;
    argv[0]="ab";
    argv[1]=0;
    env[0]=0;
    /*
     * Simulate case that we are called from user space
     */
    ir_context.cs_old = SELECTOR_CODE_USER;
    /*
     * Set the user space stack to our test stack
     */
    user_space_stack = (u32)(test_stack + 8192 - 4);
    pm_init();
    fs_on_exec_called = 0;
    ASSERT(0==do_exec("test", argv,  env, &ir_context));
    /*
     * Verify that stack pointer is valid
     */
    esp = *(&(ir_context.eflags) + 1);
    ASSERT(esp <= user_space_stack);
    ASSERT(esp>=(u32) test_stack);
    /*
     * Print stack
    for (ptr = (u32*)(user_space_stack);  ((u32) ptr) >= ((u32)esp) ; ptr--) {
        printf("%p:  %p\n", ptr, *ptr);
    }
    */
    /*
     * Argc should be located at ESP+4
     */
    stored_argc = *((int*)(esp+4));
    ASSERT(1==stored_argc);
    /*
     * Argv should be at ESP+8
     */
    stored_argv = (char**) (*((u32*)(esp+8)));
    ASSERT(stored_argv);
    /*
     * Env should be at ESP+12
     */
    stored_env = (char**) (*((u32*)(esp+12)));
    ASSERT(stored_env);
    /*
     * Env[0] should be 0
     */
    ASSERT(0==stored_env[0]);
    return 0;
}

/*
 * Testcase 57
 * Tested function: do_exec
 * Testcase: call do_exec for a file for which the SUID bit is not set
 * and verify that the effective user ID of the process has not changed
 */
int testcase57() {
    ir_context_t ir_context;
    char* argv[2];
    u8 __attribute__  ((aligned (4))) test_stack[8192];
    u32 esp;
    u32* ptr;
    int stored_argc;
    char** stored_argv;
    argv[0]="ab";
    argv[1]=0;
    /*
     * Simulate case that we are called from user space
     */
    ir_context.cs_old = SELECTOR_CODE_USER;
    /*
     * Set the user space stack to our test stack
     */
    user_space_stack = (u32)(test_stack + 8192 - 4);
    pm_init();
    fs_on_exec_called = 0;
    /*
     * Simulate the case that the file has no SUID bit set,
     * and the owner has UID 1
     */
    st_mode = 0;
    st_uid = 0;
    ASSERT(0==do_exec("test", argv,  0, &ir_context));
    ASSERT(0==do_geteuid());
    return 0;
}

/*
 * Testcase 58
 * Tested function: do_exec
 * Testcase: call do_exec for a file for which the SUID bit is  set
 * and verify that the effective user ID of the process has  changed
 */
int testcase58() {
    ir_context_t ir_context;
    char* argv[2];
    u8 __attribute__  ((aligned (4))) test_stack[8192];
    u32 esp;
    u32* ptr;
    int stored_argc;
    char** stored_argv;
    argv[0]="ab";
    argv[1]=0;
    /*
     * Simulate case that we are called from user space
     */
    ir_context.cs_old = SELECTOR_CODE_USER;
    /*
     * Set the user space stack to our test stack
     */
    user_space_stack = (u32)(test_stack + 8192 - 4);
    pm_init();
    fs_on_exec_called = 0;
    /*
     * Simulate the case that the file has no SUID bit set,
     * and the owner has UID 1
     */
    st_mode = 04000;
    st_uid = 1;
    ASSERT(0==do_exec("test", argv,  0, &ir_context));
    ASSERT(1==do_geteuid());
    return 0;
}

/*
 * Testcase 59: do_getpgrp
 * Verify that the INIT process has process group 1
 */
int testcase59() {
     ir_context_t ir_context;
     u32 __attribute__ ((aligned (4096))) my_stack[8192];
     pm_init();
     /*
      * Fork off process 1 and switch to it
      */
     task_tos = (u32)(my_stack+99)+3;
     ir_context.cs_old = SELECTOR_CODE_USER;
     ASSERT(1==do_fork(&ir_context));
     ASSERT(1==pm_switch_task(1, &ir_context));
     ASSERT(1==do_getpid());
     /*
      * Then call getpgrp
      */
     ASSERT(1==do_getpgrp());
     return 0;
}

/*
 * Testcase 60: setpgid
 * Set up a new process group 2 within the session of the INIT process, specify pid and pgid
 * explicitly
 */
int testcase60() {
     ir_context_t ir_context;
     u32 __attribute__ ((aligned (4096))) my_stack[8192];
     pm_init();
     /*
      * Fork off process 1 and switch to it
      */
     task_tos = (u32)(my_stack+99)+3;
     ir_context.cs_old = SELECTOR_CODE_USER;
     ASSERT(1==do_fork(&ir_context));
     ASSERT(1==pm_switch_task(1, &ir_context));
     ASSERT(1==do_getpid());
     /*
      * Now create process 2
      */
     ASSERT(2==do_fork(&ir_context));
     ASSERT(1==pm_switch_task(2, &ir_context));
     ASSERT(2==do_getpid());
     /*
      * Then call setpgid
      */
     ASSERT(0==do_setpgid(2, 2));
     ASSERT(2==do_getpgrp());
     return 0;
}

/*
 * Testcase 61: setpgid
 * Set up a new process group 2 within the session of the INIT process, use pid 0
 */
int testcase61() {
     ir_context_t ir_context;
     u32 __attribute__ ((aligned (4096))) my_stack[8192];
     pm_init();
     /*
      * Fork off process 1 and switch to it
      */
     task_tos = (u32)(my_stack+99)+3;
     ir_context.cs_old = SELECTOR_CODE_USER;
     ASSERT(1==do_fork(&ir_context));
     ASSERT(1==pm_switch_task(1, &ir_context));
     ASSERT(1==do_getpid());
     /*
      * Now create process 2
      */
     ASSERT(2==do_fork(&ir_context));
     ASSERT(1==pm_switch_task(2, &ir_context));
     ASSERT(2==do_getpid());
     /*
      * Then call setpgid
      */
     ASSERT(0==do_setpgid(0, 2));
     ASSERT(2==do_getpgrp());
     return 0;
}

/*
 * Testcase 62: setpgid
 * Set up a new process group 2 within the session of the INIT process, use pgid 0
 */
int testcase62() {
     ir_context_t ir_context;
     u32 __attribute__ ((aligned (4096))) my_stack[8192];
     pm_init();
     /*
      * Fork off process 1 and switch to it
      */
     task_tos = (u32)(my_stack+99)+3;
     ir_context.cs_old = SELECTOR_CODE_USER;
     ASSERT(1==do_fork(&ir_context));
     ASSERT(1==pm_switch_task(1, &ir_context));
     ASSERT(1==do_getpid());
     /*
      * Now create process 2
      */
     ASSERT(2==do_fork(&ir_context));
     ASSERT(1==pm_switch_task(2, &ir_context));
     ASSERT(2==do_getpid());
     /*
      * Then call setpgid
      */
     ASSERT(0==do_setpgid(2, 0));
     ASSERT(2==do_getpgrp());
     return 0;
}

/*
 * Testcase 63: setpgid
 * Set up a new process group 2 within the session of the INIT process, use pid 0 and pgid 0
 */
int testcase63() {
     ir_context_t ir_context;
     u32 __attribute__ ((aligned (4096))) my_stack[8192];
     pm_init();
     /*
      * Fork off process 1 and switch to it
      */
     task_tos = (u32)(my_stack+99)+3;
     ir_context.cs_old = SELECTOR_CODE_USER;
     ASSERT(1==do_fork(&ir_context));
     ASSERT(1==pm_switch_task(1, &ir_context));
     ASSERT(1==do_getpid());
     /*
      * Now create process 2
      */
     ASSERT(2==do_fork(&ir_context));
     ASSERT(1==pm_switch_task(2, &ir_context));
     ASSERT(2==do_getpid());
     /*
      * Then call setpgid
      */
     ASSERT(0==do_setpgid(0, 0));
     ASSERT(2==do_getpgrp());
     return 0;
}

/*
 * Testcase 64: setpgid
 * Try to change the process group of a process which is not a child
 */
int testcase64() {
     ir_context_t ir_context;
     u32 __attribute__ ((aligned (4096))) my_stack[8192];
     pm_init();
     /*
      * Fork off process 1 and switch to it
      */
     task_tos = (u32)(my_stack+99)+3;
     ir_context.cs_old = SELECTOR_CODE_USER;
     ASSERT(1==do_fork(&ir_context));
     ASSERT(1==pm_switch_task(1, &ir_context));
     ASSERT(1==do_getpid());
     /*
      * Now create process 2
      */
     ASSERT(2==do_fork(&ir_context));
     ASSERT(1==pm_switch_task(2, &ir_context));
     ASSERT(2==do_getpid());
     /*
      * Then call setpgid
      */
     ASSERT(do_setpgid(1, 0));
     ASSERT(1==do_getpgrp());
     return 0;
}

/*
 * Testcase 65: setpgid
 * Try to change the process group to an invalid process group
 */
int testcase65() {
     ir_context_t ir_context;
     u32 __attribute__ ((aligned (4096))) my_stack[8192];
     pm_init();
     /*
      * Fork off process 1 and switch to it
      */
     task_tos = (u32)(my_stack+99)+3;
     ir_context.cs_old = SELECTOR_CODE_USER;
     ASSERT(1==do_fork(&ir_context));
     ASSERT(1==pm_switch_task(1, &ir_context));
     ASSERT(1==do_getpid());
     /*
      * Now create process 2
      */
     ASSERT(2==do_fork(&ir_context));
     ASSERT(1==pm_switch_task(2, &ir_context));
     ASSERT(2==do_getpid());
     /*
      * Then call setpgid
      */
     ASSERT(do_setpgid(0, 3));
     ASSERT(1==do_getpgrp());
     return 0;
}

/*
 * Testcase 66: setpgid
 * Try to change the process group for an invalid process
 */
int testcase66() {
     ir_context_t ir_context;
     u32 __attribute__ ((aligned (4096))) my_stack[8192];
     pm_init();
     /*
      * Fork off process 1 and switch to it
      */
     task_tos = (u32)(my_stack+99)+3;
     ir_context.cs_old = SELECTOR_CODE_USER;
     ASSERT(1==do_fork(&ir_context));
     ASSERT(1==pm_switch_task(1, &ir_context));
     ASSERT(1==do_getpid());
     /*
      * Now create process 2
      */
     ASSERT(2==do_fork(&ir_context));
     ASSERT(1==pm_switch_task(2, &ir_context));
     ASSERT(2==do_getpid());
     /*
      * Then call setpgrp
      */
     ASSERT(do_setpgid(2048, 0));
     ASSERT(1==do_getpgrp());
     return 0;
}

/*
 * Testcase 67: setpgrp
 * Set the process group for a child of the init process
 */
int testcase67() {
     ir_context_t ir_context;
     u32 __attribute__ ((aligned (4096))) my_stack[8192];
     pm_init();
     /*
      * Fork off process 1 and switch to it
      */
     task_tos = (u32)(my_stack+99)+3;
     ir_context.cs_old = SELECTOR_CODE_USER;
     ASSERT(1==do_fork(&ir_context));
     ASSERT(1==pm_switch_task(1, &ir_context));
     ASSERT(1==do_getpid());
     /*
      * Now create process 2
      */
     ASSERT(2==do_fork(&ir_context));
     ASSERT(1==pm_switch_task(2, &ir_context));
     ASSERT(2==do_getpid());
     /*
      * Then call setpgrp
      */
     ASSERT(2==do_setpgrp());
     ASSERT(2==do_getpgrp());
     return 0;
}

/*
 * Testcase 68: do_kill
 * Send a stop signal to a process which has three threads and verify that all threads are stopped
 */
int testcase68() {
    ir_context_t ir_context;
    pthread_t thread2 = 0;
    pthread_t thread3 = 0;
    u32 __attribute__ ((aligned (4096))) my_stack[8192];
    u32 sigmask;
    int rc;
    pm_init();
    /*
     * As it is not allowed to deliver a signal to tasks 0 and 1,
     * we first have to call do_fork twice and switch to this process
     */
    task_tos = (u32)(my_stack+99)+3;
    ir_context.cs_old = SELECTOR_CODE_KERNEL;
    ASSERT(1==do_fork(&ir_context));
    ASSERT(2==do_fork(&ir_context));
    pm_switch_task(2, &ir_context);
    ASSERT(2==pm_get_task_id());
    ASSERT(2==pm_get_pid());
    /*
     * Now we create two new tasks within that process
     */
    ir_context.cs_old = SELECTOR_CODE_KERNEL;
    rc = do_pthread_create(&thread2, 0, my_exec, (void*) 0x100, &ir_context);
    ASSERT(0==rc);
    ASSERT(3==thread2);
    rc = do_pthread_create(&thread3, 0, my_exec, (void*) 0x100, &ir_context);
    ASSERT(0==rc);
    ASSERT(4==thread3);
    /*
     * Switch to task 3
     */
    pm_switch_task(3, &ir_context);
    ASSERT(3==pm_get_task_id());
    ASSERT(2==pm_get_pid());
    /*
     * Send signal SIGSTOP to process 2
     */
    ASSERT(0==do_kill(2, __KSIGSTOP));
    /*
     * and verify that it is not pending in task 3 because it has been
     * delivered to task 2
     */
    sigmask = 0;
    ASSERT(0==do_sigpending(&sigmask));
    ASSERT(0==sigmask);
    /*
     * Switch to task 2 and verify that from its point of view, the signal is pending
     */
    pm_switch_task(2, &ir_context);
    ASSERT(2==pm_get_task_id());
    sigmask = 0;
    ASSERT(0==do_sigpending(&sigmask));
    ASSERT((1 << __KSIGSTOP)==sigmask);
    /*
     * Make kernel believe that we were called from user space to
     * force signal processing to take place
     */
    ir_context.cs_old = SELECTOR_CODE_USER;
    /*
     * Now call pm_process_signals from within task 2. This will:
     * - send __KSIGTASK to task 3 and task 4
     * - dequeue the currently active task, i.e. task 2
     */
    last_dequeued_task = 0;
    ASSERT(0==pm_process_signals(&ir_context));
    ASSERT(last_dequeued_task==2);
    /*
     * Switch to tasks 3 and 4 and verify that when pm_process_signals is called,
     * they are dequeued
     */
    pm_switch_task(3, &ir_context);
    ASSERT(3==pm_get_task_id());
    last_dequeued_task = 0;
    ASSERT(0==pm_process_signals(&ir_context));
    ASSERT(last_dequeued_task==3);
    pm_switch_task(4, &ir_context);
    ASSERT(4==pm_get_task_id());
    last_dequeued_task = 0;
    ASSERT(0==pm_process_signals(&ir_context));
    ASSERT(last_dequeued_task==4);
    return 0;
}

/*
 * Testcase 69: setsid
 * Create a child of the INIT process and spawn a new session.
 */
int testcase69() {
     ir_context_t ir_context;
     u32 __attribute__ ((aligned (4096))) my_stack[8192];
     pm_init();
     /*
      * Fork off process 1 (INIT) and switch to it
      */
     task_tos = (u32)(my_stack+99)+3;
     ir_context.cs_old = SELECTOR_CODE_USER;
     ASSERT(1 == do_fork(&ir_context));
     ASSERT(1 == pm_switch_task(1, &ir_context));
     ASSERT(1 == do_getpid());
     /*
      * Now create process 2
      */
     ASSERT(2 == do_fork(&ir_context));
     ASSERT(1 == pm_switch_task(2, &ir_context));
     ASSERT(2 == do_getpid());
     ASSERT(1 == do_getsid(0));
     /*
      * Then call setsid
      */
     ASSERT(0 == do_setsid());
     /*
      * Verify new session ID
      */
     ASSERT(2 == do_getsid(0));
     ASSERT(2 == do_getsid(2));
     return 0;
}

/*
 * Testcase 70: setsid
 * Verify that a process group lead cannot execute setsid
 */
int testcase70() {
     ir_context_t ir_context;
     u32 __attribute__ ((aligned (4096))) my_stack[8192];
     pm_init();
     /*
      * Fork off process 1 (INIT) and switch to it
      */
     task_tos = (u32)(my_stack+99)+3;
     ir_context.cs_old = SELECTOR_CODE_USER;
     ASSERT(1 == do_fork(&ir_context));
     ASSERT(1 == pm_switch_task(1, &ir_context));
     ASSERT(1 == do_getpid());
     /*
      * Now create process 2
      */
     ASSERT(2 == do_fork(&ir_context));
     ASSERT(1 == pm_switch_task(2, &ir_context));
     ASSERT(2 == do_getpid());
     ASSERT(1 == do_getsid(0));
     /*
      * Then call setsid
      */
     ASSERT(0 == do_setsid());
     /*
      * Verify new session ID
      */
     ASSERT(2 == do_getsid(0));
     ASSERT(2 == do_getsid(2));
     /*
      * Now make sure that we cannot call setsid again - should return -EPERM
      */
     ASSERT(-105 == do_setsid());
     ASSERT(2 == do_getsid(0));
     return 0;
}

/*
 * Testcase 71: do_sigsuspend
 */
int testcase71() {
    unsigned int set;
    unsigned int oset;
    pm_init();
    /*
     * Block signal 10 and 11
     */
    set = ((1 << 10) | (1<<11));
    ASSERT(0 == do_sigprocmask(__KSIG_BLOCK, &set, 0));
    ASSERT(0 == do_sigprocmask(0, 0, &oset));
    ASSERT(oset == ((1<<10) | (1<<11)));
    /*
     * Now call sigsuspend, thereby unblock signal 11
     */
    set = (1 << 10);
    oset = 0;
    ASSERT(-EPAUSE == do_sigsuspend(&set, &oset));
    /*
     * Old set should be as before
     */
    ASSERT(oset == ((1<<10) | (1<<11)));
    /*
     * Current signal mask should now be 1 << 10
     */
    ASSERT(0 == do_sigprocmask(0, 0, &oset));
    ASSERT(oset == (1<<10));
    return 0;
}

/*
 * Testcase 72: pm_attach_tty
 * Attach a terminal as controlling terminal to process 1
 */
int testcase72() {
    ir_context_t ir_context;
    u32 __attribute__ ((aligned (4096))) my_stack[8192];
    pm_init();
    /*
     * Fork off process 1 (INIT) and switch to it
     */
    task_tos = (u32)(my_stack + 99) + 3;
    ir_context.cs_old = SELECTOR_CODE_USER;
    ASSERT(1 == do_fork(&ir_context));
    ASSERT(1 == pm_switch_task(1, &ir_context));
    ASSERT(1 == do_getpid());
    /*
     * Validate that process 1 is a session leader
     */
    ASSERT(1 == do_getsid(do_getpid()));
    /*
     * and not yet associated with a terminal
     */
    ASSERT(DEVICE_NONE == pm_get_cterm());
    /*
     * Now connect process to device (MAJOR_TTY, 0)
     */
    pm_attach_tty(DEVICE(MAJOR_TTY, 0));
    /*
     * and validate result
     */
    ASSERT(DEVICE(MAJOR_TTY, 0) == pm_get_cterm());
    return 0;
}

/*
 * Testcase 73: pm_attach_tty
 * Attach a terminal as controlling terminal to a process which is not a session leader
 */
int testcase73() {
    ir_context_t ir_context;
    u32 __attribute__ ((aligned (4096))) my_stack[8192];
    pm_init();
    /*
     * Fork off process 1 (INIT) and switch to it
     */
    task_tos = (u32)(my_stack + 99) + 3;
    ir_context.cs_old = SELECTOR_CODE_USER;
    ASSERT(1 == do_fork(&ir_context));
    ASSERT(1 == pm_switch_task(1, &ir_context));
    ASSERT(1 == do_getpid());
    /*
     * Validate that process 1 is a session leader
     */
    ASSERT(1 == do_getsid(do_getpid()));
    /*
     * and not yet associated with a terminal
     */
    ASSERT(DEVICE_NONE == pm_get_cterm());
    /*
     * Now create process 2 and switch to it
     */
    ASSERT(2 == do_fork(&ir_context));
    ASSERT(1 == pm_switch_task(2, &ir_context));
    ASSERT(2 == do_getpid());
    ASSERT(1 == do_getsid(0));
    /*
     * validate that process 2 is not yet associated with a terminal
     */
    ASSERT(DEVICE_NONE == pm_get_cterm());
    /*
     * Now connect process to device (MAJOR_TTY, 0)
     */
    pm_attach_tty(DEVICE(MAJOR_TTY, 0));
    /*
     * and validate result
     */
    ASSERT(DEVICE_NONE == pm_get_cterm());
    return 0;
}

/*
 * Testcase 74: pm_attach_tty
 * Attach a terminal as controlling terminal to a session leader different from the
 * INIT process
 */
int testcase74() {
    int rc;
    ir_context_t ir_context;
    u32 __attribute__ ((aligned (4096))) my_stack[8192];
    pm_init();
    /*
     * Fork off process 1 (INIT) and switch to it
     */
    task_tos = (u32)(my_stack + 99) + 3;
    ir_context.cs_old = SELECTOR_CODE_USER;
    ASSERT(1 == do_fork(&ir_context));
    ASSERT(1 == pm_switch_task(1, &ir_context));
    ASSERT(1 == do_getpid());
    /*
     * Validate that process 1 is a session leader
     */
    ASSERT(1 == do_getsid(1));
    /*
     * and not yet associated with a terminal
     */
    ASSERT(DEVICE_NONE == pm_get_cterm());
    /*
     * Now create process 2 and switch to it
     */
    ASSERT(2 == do_fork(&ir_context));
    ASSERT(1 == pm_switch_task(2, &ir_context));
    ASSERT(2 == do_getpid());
    /*
     * Initially, this process will be in session 1 as well - we now move
     * this process into its own session
     */
    ASSERT(1 == do_getsid(2));
    ASSERT(0 == do_setsid());
    ASSERT(2 == do_getsid(2));
    /*
     * validate that process 2 is not yet associated with a terminal
     */
    ASSERT(DEVICE_NONE == pm_get_cterm());
    /*
     * Now connect process to device (MAJOR_TTY, 0)
     */
    pm_attach_tty(DEVICE(MAJOR_TTY, 0));
    /*
     * and validate result
     */
    ASSERT(DEVICE(MAJOR_TTY, 0) == pm_get_cterm());
    return 0;
}

/*
 * Testcase 75: pm_attach_tty
 * Attach a terminal as controlling terminal to a session leader different from the
 * INIT process, but use a terminal which is already the controlling terminal of a session
 * - operation should fail
 */
int testcase75() {
    int rc;
    ir_context_t ir_context;
    u32 __attribute__ ((aligned (4096))) my_stack[8192];
    pm_init();
    /*
     * Fork off process 1 (INIT) and switch to it
     */
    task_tos = (u32)(my_stack + 99) + 3;
    ir_context.cs_old = SELECTOR_CODE_USER;
    ASSERT(1 == do_fork(&ir_context));
    ASSERT(1 == pm_switch_task(1, &ir_context));
    ASSERT(1 == do_getpid());
    /*
     * Validate that process 1 is a session leader
     */
    ASSERT(1 == do_getsid(1));
    /*
     * and not yet associated with a terminal
     */
    ASSERT(DEVICE_NONE == pm_get_cterm());
    /*
     * Now create process 2 and switch to it. After the fork and before
     * the task switch, attach (MAJOR_TTY, 0) to process 1
     */
    ASSERT(2 == do_fork(&ir_context));
    pm_attach_tty(DEVICE(MAJOR_TTY, 0));
    ASSERT(1 == do_getpid());
    ASSERT(DEVICE(MAJOR_TTY, 0) == pm_get_cterm());
    ASSERT(1 == pm_switch_task(2, &ir_context));
    ASSERT(2 == do_getpid());
    /*
     * Initially, this process will be in session 1 as well - we now move
     * this process into its own session
     */
    ASSERT(1 == do_getsid(2));
    ASSERT(0 == do_setsid());
    ASSERT(2 == do_getsid(2));
    /*
     * validate that process 2 is not yet associated with a terminal
     */
    ASSERT(DEVICE_NONE == pm_get_cterm());
    /*
     * Now connect process to device (MAJOR_TTY, 0)
     */
    pm_attach_tty(DEVICE(MAJOR_TTY, 0));
    /*
     * and validate result - we should not be attached to the terminal
     */
    ASSERT(DEVICE_NONE == pm_get_cterm());
    return 0;
}

/*
 * Testcase 76: pm_attach_tty
 * Attach a terminal as controlling terminal to a session leader different from the
 * INIT process and validate that the process group of the terminal has been set to
 * the process group of process 2
 */
int testcase76() {
    int rc;
    ir_context_t ir_context;
    u32 __attribute__ ((aligned (4096))) my_stack[8192];
    pm_init();
    /*
     * Fork off process 1 (INIT) and switch to it
     */
    task_tos = (u32)(my_stack + 99) + 3;
    ir_context.cs_old = SELECTOR_CODE_USER;
    ASSERT(1 == do_fork(&ir_context));
    ASSERT(1 == pm_switch_task(1, &ir_context));
    ASSERT(1 == do_getpid());
    /*
     * Validate that process 1 is a session leader
     */
    ASSERT(1 == do_getsid(1));
    /*
     * and not yet associated with a terminal
     */
    ASSERT(DEVICE_NONE == pm_get_cterm());
    /*
     * Now create process 2 and switch to it
     */
    ASSERT(2 == do_fork(&ir_context));
    ASSERT(1 == pm_switch_task(2, &ir_context));
    ASSERT(2 == do_getpid());
    /*
     * Initially, this process will be in session 1 as well - we now move
     * this process into its own session
     */
    ASSERT(1 == do_getsid(2));
    ASSERT(0 == do_setsid());
    ASSERT(2 == do_getsid(2));
    /*
     * validate that process 2 is not yet associated with a terminal
     */
    ASSERT(DEVICE_NONE == pm_get_cterm());
    /*
     * Now connect process to device (MAJOR_TTY, 0)
     */
    pm_attach_tty(DEVICE(MAJOR_TTY, 0));
    /*
     * and validate result
     */
    ASSERT(DEVICE(MAJOR_TTY, 0) == pm_get_cterm());
    /*
     * The process group of this terminal should now be 2
     */
    ASSERT(__pgrp[0] == 2);
    return 0;
}

/*
 * Testcase 77: setsid / controlling terminal / fork
 * Create a child of the INIT process and spawn a new session. Verify that the process
 * does not have a controlling terminal any more
 */
int testcase77() {
     ir_context_t ir_context;
     u32 __attribute__ ((aligned (4096))) my_stack[8192];
     pm_init();
     /*
      * Fork off process 1 (INIT) and switch to it
      */
     task_tos = (u32)(my_stack+99)+3;
     ir_context.cs_old = SELECTOR_CODE_USER;
     ASSERT(1 == do_fork(&ir_context));
     ASSERT(1 == pm_switch_task(1, &ir_context));
     ASSERT(1 == do_getpid());
     /*
      * Associate this process with a controlling terminal
      */
     pm_attach_tty(DEVICE(MAJOR_TTY, 0));
     /*
      * Now create process 2
      */
     ASSERT(2 == do_fork(&ir_context));
     ASSERT(1 == pm_switch_task(2, &ir_context));
     ASSERT(2 == do_getpid());
     ASSERT(1 == do_getsid(0));
     /*
      * should have same controlling terminal
      */
     ASSERT(DEVICE(MAJOR_TTY, 0) == pm_get_cterm());
     /*
      * Then call setsid
      */
     ASSERT(0 == do_setsid());
     /*
      * Verify new session ID
      */
     ASSERT(2 == do_getsid(0));
     ASSERT(2 == do_getsid(2));
     /*
      * and check that process is no longer attached to a controlling terminal
      */
     ASSERT(DEVICE_NONE == pm_get_cterm());
     return 0;
}

/*
 * Testcase 78: pm_handle_exit_requests / controlling terminal
 * Attach a terminal as controlling terminal to the INIT process, then fork a child
 * which thus inherits the controlling terminal. Exit the INIT process and verify that the
 * child loses its controlling terminal as well
 */
int testcase78() {
    int rc;
    ir_context_t ir_context;
    u32 __attribute__ ((aligned (4096))) my_stack[8192];
    pm_init();
    /*
     * Fork off process 1 (INIT) and switch to it
     */
    task_tos = (u32)(my_stack + 99) + 3;
    ir_context.cs_old = SELECTOR_CODE_USER;
    ASSERT(1 == do_fork(&ir_context));
    ASSERT(1 == pm_switch_task(1, &ir_context));
    ASSERT(1 == do_getpid());
    /*
     * Validate that process 1 is a session leader
     */
    ASSERT(1 == do_getsid(1));
    /*
     * and not yet associated with a terminal
     */
    ASSERT(DEVICE_NONE == pm_get_cterm());
    /*
     * Make /dev/tty0 the controlling terminal of INIT
     */
    pm_attach_tty(DEVICE(MAJOR_TTY, 0));
    ASSERT(DEVICE(MAJOR_TTY, 0) == pm_get_cterm());
    /*
     * Now create process 2 and switch to it.
     */
    ASSERT(2 == do_fork(&ir_context));
    ASSERT(1 == do_getpid());
    ASSERT(1 == pm_switch_task(2, &ir_context));
    ASSERT(2 == do_getpid());
    ASSERT(DEVICE(MAJOR_TTY, 0) == pm_get_cterm());
    /*
     * Now switch back to task 1 and exit
     */
    ASSERT(1 == pm_switch_task(1, &ir_context));
    ASSERT(1 == do_getpid());
    do_exit(0);
    /*
     * So far, we have only set the exit flag. We now have to simulate the following sequence as it would occur
     * in a real interrupt handler for the do_exit system call:
     * - do_exit returns
     * - pm_handle_exit_requests is called
     * - a task switch is requested
     * - the cleanup handler is called
     * - execution of another process, say process 2, continues
     */
    ASSERT(1 == pm_handle_exit_requests());
    ASSERT(1 == pm_switch_task(2, &ir_context));
    pm_cleanup_task();
    ASSERT(2 == do_getpid());
    /*
     * Now process 2 should no longer be associated to the terminal as well
     */
    ASSERT(DEVICE_NONE == pm_get_cterm());
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
    RUN_CASE(8);
    RUN_CASE(9);
    RUN_CASE(10);
    RUN_CASE(11);
    RUN_CASE(12);
    RUN_CASE(13);
    RUN_CASE(14);
    RUN_CASE(15);
    RUN_CASE(16);
    RUN_CASE(17);
    RUN_CASE(18);
    RUN_CASE(19);
    RUN_CASE(20);
    RUN_CASE(21);
    RUN_CASE(22);
    RUN_CASE(23);
    RUN_CASE(24);
    RUN_CASE(25);
    RUN_CASE(26);
    RUN_CASE(27);
    RUN_CASE(28);
    RUN_CASE(29);
    RUN_CASE(30);
    RUN_CASE(31);
    RUN_CASE(32);
    RUN_CASE(33);
    RUN_CASE(34);
    RUN_CASE(35);
    RUN_CASE(36);
    RUN_CASE(37);
    RUN_CASE(38);
    RUN_CASE(39);
    RUN_CASE(40);
    RUN_CASE(41);
    RUN_CASE(42);
    RUN_CASE(43);
    RUN_CASE(44);
    RUN_CASE(45);
    RUN_CASE(46);
    RUN_CASE(47);
    RUN_CASE(48);
    RUN_CASE(49);
    RUN_CASE(50);
    RUN_CASE(51);
    RUN_CASE(52);
    RUN_CASE(53);
    RUN_CASE(54);
    RUN_CASE(55);
    RUN_CASE(56);
    RUN_CASE(57);
    RUN_CASE(58);
    RUN_CASE(59);
    RUN_CASE(60);
    RUN_CASE(61);
    RUN_CASE(62);
    RUN_CASE(63);
    RUN_CASE(64);
    RUN_CASE(65);
    RUN_CASE(66);
    RUN_CASE(67);
    RUN_CASE(68);
    RUN_CASE(69);
    RUN_CASE(70);
    RUN_CASE(71);
    RUN_CASE(72);
    RUN_CASE(73);
    RUN_CASE(74);
    RUN_CASE(75);
    RUN_CASE(76);
    RUN_CASE(77);
    RUN_CASE(78);
    END;
}
