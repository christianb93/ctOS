/*
 * debug.c
 * This module contains functions for logging
 * and debugging
 *
 * The bulk of this module implements the internal debugger of ctOS which
 * is invoked when either int 3 is raised or the F1 key is pressed. To print out
 * information which is stored in static tables within the individual modules (pm, mm,...)
 * the debugger invokes specific functions in these modules
 *
 *
 * To support debugging on SMP machines, a debug event raised on one CPU will send an IPI to all other
 * CPUs which causes them to dump their current state and then enter an idle loop. For this purpose, the debug module
 * has a static variable debugger_running. If a debugger is invoked via calling debug_main, it first checks this
 * variable.
 *
 * If the variable is not set, it sets it and send sends an INT 0x82 to all other CPUs via an IPI. It then enters the normal
 * debugger code.
 *
 * If the variable is set, the debugger assumes that it is running on a CPU different from the one which caused the debug
 * event. In this case, it will write the interrupt context into an array, indexed by CPUID, and enter an idle loop.
 *
 * At boot time, the SMP startup code informs the debugger about available CPUs.
 *
 * Interrupt handling in debugging mode deserves some attention. Earlier versions of the debugger turned off interrupts
 * entirely upon entering debugging mode and used busy loops to wait for user input. This implies heavy CPU load - on QEMU,
 * for instance, I managed to bring CPU temperature to 70 C by running the debugger for about 30 minutes. As the debugger can
 * kick in due to a kernel panic at any time and the machine might remain in this state for some while, this is a problem. T
 *
 * Therefore the debugger now enables interrupts again after initialization and uses "hlt"-loops to wait for keyboard and timer events.
 * To avoid that the state of the system is changed by hardware interrupts, the interrupt manager calls the public function debug_running
 * when a hardware interrupt is received to find out whether the debugger is already running. If it is running, the hardware interrupt is
 * acknowledged, but no interrupt handler is called.
 *
 * The debugger also offers debugging support for locks. For this purpose, the following interface is available.
 *
 * debug_lock_wait       this function is called by the program manager when a task is waiting for a lock or a semaphore. It will add
 *                       the lock together with information on the location of the call (module and line number) to a list
 * debug_lock_acquired   when a task has acquired a read/write lock it calls this function which will update the lock status accordingly
 * debug_lock_released   this function is called when a read/write lock has been released and removes the entry
 * debug_lock_cancel     this is called when a sem_down operation completes and also removes the lock from the list maintained by the
 *                       debugger
 *
 * Using the "locks" command in the debugger, it is thus possible to display a list of

 * 1) all tasks waiting in a sem_down / sem_down_intr operation
 * 2) all tasks holding a read/write lock
 * 3) all tasks waiting for a read/write lock
 *
 */

#include "debug.h"
#include "gdt.h"
#include "keyboard.h"
#include "lib/string.h"
#include "lib/stdlib.h"
#include "util.h"
#include "mm.h"
#include "pm.h"
#include "pagetables.h"
#include "sched.h"
#include "reboot.h"
#include "pci.h"
#include "pata.h"
#include "irq.h"
#include "mptables.h"
#include "ahci.h"
#include "fs.h"
#include "vga.h"
#include "rtc.h"
#include "cpu.h"
#include "smp.h"
#include "apic.h"
#include "util.h"
#include "timer.h"
#include "eth.h"
#include "net_if.h"
#include "ip.h"
#include "multiboot.h"
#include "acpi.h"

extern int (*mm_page_mapped)(u32);

/*
 * Array in which we store the information that a process is waiting for a lock
 */
#define LOCK_TYPE_RW 1
#define LOCK_TYPE_SEM 2
#define LOCK_TYPE_NONE 0

#define LOCK_STS_WAITING 1
#define LOCK_STS_ACQUIRED 2
#define LOCK_STS_RELEASED 3

typedef struct {
    int lock_type;
    u32 lock_addr;
    int rw;
    int task_waiting;
    int task_acquired;
    int line;
    int lock_status;
    char file[256];
} lock_info_t;

static lock_info_t lock_info[1024];
static spinlock_t lock_info_lock;



/*
 * Which CPUs are active? To set this, the SMP startup code needs
 * to call debug_add_cpu() at startup time
 */
static int cpu_active[SMP_MAX_CPU];

/*
 * State of a CPU
 * 0 = not running
 * 1 = running
 * 2 = stopped
 */
static int cpu_state[SMP_MAX_CPU];

/*
 * Stored context per CPU
 */
static ir_context_t cpu_context[SMP_MAX_CPU];

/*
 * Static variable which stores the information whether the debugger is already running
 * and spinlock to protect it
 */
static int debugger_running = 0;
static spinlock_t debugger_lock;

/*
 * Linker symbol for end of kernel bss section
 */
extern unsigned int _end;

/*
 * Exception names
 *
 */
static char* exceptions[18] = {
        "Division by zero",
        "Debug trap",
        "NMI",
        "Breakpoint",
        "Overflow",
        "Out of bounds",
        "Undefined op-code",
        "Processor extension not present",
        "Double fault",
        "FPU protection fault",
        "Invalid task status segment",
        "Segment not present",
        "Stack overflow",
        "General protection fault",
        "Page fault",
        "Not used",
        "FPU fault",
        "Aligment check failed" };


/*
 * Debugger running?
 */
int debug_running() {
    return debugger_running;
}

/*
 * Register a CPU with the debugger
 * Parameter:
 * @cpuid - the logical CPUID
 */
void debug_add_cpu(int cpuid) {
    cpu_active[cpuid] = 1;
    cpu_state[cpuid] = 1;
}

/*
 * Store a context
 * Parameter:
 * @ir_context - the context to be stored
 */
static void store_context(ir_context_t* context) {
    int cpuid = smp_get_cpu();
    cpu_context[cpuid] = *context;
    cpu_state[cpuid] = 2;
}


/*
 * Print all known CPUs
 */
static void print_cpus() {
    int i;
    PRINT("CPU   State\n");
    PRINT("------------------------\n");
    for (i = 0; i < SMP_MAX_CPU; i++) {
        if ((1 == cpu_active[i]) || (0 == i)) {
            PRINT("%d     ", i);
            switch (cpu_state[i]) {
                case 0:
                    PRINT("NOT STARTED\n");
                    break;
                case 1:
                    PRINT("RUNNING\n");
                    break;
                case 2:
                    PRINT("STOPPED\n");
                    break;
                default:
                    PRINT("UNKNOWN\n");
                    break;
            }
        }
    }
}

/*
 * Utility function to translate a virtual into a physical address
 * Sets errno if no mapping was possible
 * Parameters:
 * @ptd - pointer to page table directory which we use for the translation
 * @virtual - virtual address to be translated
 * @errno - a pointer to an integer variable in which an error number is stored
 * Return value:
 * the physical address or 0 if an error occured
 */
static u32 virt_to_phys(pte_t* ptd, u32 virtual, int* errno) {
    u32 ptd_offset;
    u32 pt_offset;
    pte_t* pt;
    ptd_offset = virtual >> 22;
    pt_offset = (virtual >> 12) & 1023;
    *errno = 0;
    if (0 == ptd[ptd_offset].p) {
        *errno = 1;
        return 0;
    }
    pt = (pte_t*) MM_VIRTUAL_PT_ENTRY(ptd_offset,0);
    if (0 == pt[pt_offset].p) {
        *errno = 2;
        return 0;
    }
    return (virtual % 4096) + (pt[pt_offset].page_base * MM_PAGE_SIZE);
}

/*
 * Utility function to enter internal debugger
 */
void trap() {
    asm("int $3");
}

/*
 * Read one line from keyboard using early_getchar
 * Parameters:
 * @buffer - buffer into which the read data is written
 * @max - number of bytes to read
 */
static void debug_getline_impl(char* buffer, int max) {
    int i;
    char input;
    i = 0;
    input = 0;
    /*
     * Enable interrupts temporarily to avoid busy loop in ealry_getchar.
     * Note that the interrupt manager will call use debug_running to ignore
     * all hardware interrupts anyway - they are just used to wake up a halted CPU
     */
    while ((i < max) && (input != '\n')) {
        sti();
        input = (char) early_getchar();
        cli();
        kprintf("%c", input);
        if (input == (char) 127) {
            if (i > 0) {
                i--;
                buffer[i] = 0;
            }
        }
        else {
            buffer[i] = input;
            i++;
        }
    }
    buffer[i] = 0;
}

/*
 * This is a pointer to a function which we use to read a line
 * We use dependency injection here to make testing easier
 * Function is expected to take as argument a string buffer
 * and the maximum numbers of characters to be read
 * The buffer must be large enough for max characters plus
 * a trailing 0
 */
void (*debug_getline)(char* line, int max) = &debug_getline_impl;

/*
 * Print an IR context to the screen
 * Parameters:
 * @context - the IR context to be printed
 */
static void print_ir_context(ir_context_t* context) {
    PRINT("CS= %p  DS= %p  EFLAGS=%p  (IF=%h)\n", context->cs_old, context->ds, context->eflags, (context->eflags >> 9) & 0x1);
    PRINT("EIP=%p  ERR=%x  CR3=%x  CR2=%x\n",
            context->eip, context->err_code, context->cr3,
            context->cr2);
    PRINT("EBP=%x  ESP=%x  ESI=%x  EDI=%x\n",
            context->ebp, context->esp, context->esi, context->edi);
    PRINT("EAX=%x  EBX=%x  ECX=%x  EDX=%x\n",
            context->eax, context->ebx, context->ecx, context->edx);
}

/*
 * Print out all virtual pages above the common area and verify that all mappings within the common area
 * are correct
 */
static void print_pagetables() {
    u32 page;
    u32 first_page;
    u32 last_page;
    u32 first_phys;
    u32 last_phys;
    u32 phys;
    u32 prev_phys;
    u32 last_id_page;
    int errno;
    int print = 0;
    pte_t* ptd;
    int count = 0;
    char buf[2];
    ptd = (pte_t*) get_cr3();
    PRINT("Using page table directory at %p\n", ptd);
    PRINT("Virtual Memory           Physical Memory            \n");
    PRINT("Start       End          Start       End        Pages\n");
    PRINT("------------------------------------------------------\n");
    /*
     * First go through all pages within the common area below the end of the kernel and check
     * that the mapping is one-to-one
     */
    last_id_page = MM_PAGE_START(MM_PAGE(((u32) &_end)-1));
    for (page = 0; page < ((u32) &_end); page += 4096) {
        phys = virt_to_phys(ptd, page, &errno);
        if (phys != page) {
            PRINT("Error - page %p is within common area but not mapped one-to-one (physical address = %p)\n", phys);
            early_getchar();
        }
        if (errno) {
            PRINT("Error - page %p is within common area but not mapped into virtual address space\n");
        }
    }
    /*
     * Print one summary line for that area
     */
    PRINT("%p - %p    %p - %p  %d\n", 0, last_id_page+4095, 0, last_id_page+4095, last_id_page/4096+1);
    /*
     * Now go through all pages which are mapped and below the upper end of the kernel stack
     * and print their mappings.
     * In one line which is printed, we summarize a mapping consisting of several pages. We only
     * print a line and start a new area if the physical page "jumps" by an amount different
     * from the page size
     */

    first_page = last_id_page + 4096;
    last_page = first_page;
    first_phys = virt_to_phys(ptd, page, &errno);
    last_phys = first_phys;
    prev_phys = last_phys;
    for (page = last_id_page + 2 * 4096; page <= MM_VIRTUAL_TOS; page += 4096) {
        phys = virt_to_phys(ptd, page, &errno);
        if (0 == errno) {
            if ((phys - prev_phys != MM_PAGE_SIZE) || (phys < prev_phys)) {
                /* Print mapped area
                 * and restart
                 * */
                PRINT("%p - %p    %p - %p  %d\n", first_page, last_page+4095, first_phys, last_phys+4095, (last_page-first_page)/4096+1);
                first_page = page;
                last_page = page;
                first_phys = phys;
                last_phys = phys;
                print = 0;
                count++;
                if (0 == (count % 15)) {
                    count = 0;
                    PRINT("Hit ENTER to continue\n");
                    debug_getline(buf, 1);
                    PRINT("Virtual Memory           Physical Memory            \n");
                    PRINT("Start       End          Start       End        Pages\n");
                    PRINT("------------------------------------------------------\n");
                }
            }
            else {
                /* Extend mapped area */
                last_page = page;
                last_phys = phys;
                print = 1;
            }
            prev_phys = phys;
        }
    }
    /*
     * Is there an area left which is not yet printed?
     */
    if (0 == print)
        PRINT("%p - %p    %p - %p  %d\n", first_page, last_page+4095, first_phys, last_phys+4095, (last_page-first_page)/4096+1);
}

/*
 * Print usage information
 */
static void print_usage() {
    char c[2];
    PRINT("ctOS internal debugger\n");
    PRINT("------------------------------------\n");
    PRINT("Available commands: \n");
    PRINT("help - print this screen\n");
    PRINT("regs - print register contents\n");
    PRINT("cpus - print CPU state\n");
    PRINT("x /n address - print n (decimal) bytes starting at memory address address (hex)\n");
    PRINT("pt - print information on page tables\n");
    PRINT("tasks - print task table\n");
    PRINT("sched - print runnables\n");
    PRINT("sa - print stack allocators\n");
    PRINT("lsof - print open files\n");
    PRINT("vmem - print information on virtual memory\n");
    PRINT("pmem - print information on physical memory\n");
    PRINT("pci - start PCI device browser\n");
    PRINT("pata - list PATA devices\n");
    PRINT("buslist - list all known busses\n");
    PRINT("cpulist - list all known CPUs\n");
    PRINT("Hit ENTER to see next page\n");
    debug_getline(c, 1);
    PRINT("irqr - print IRQ routing MP table\n");
    PRINT("mpapics - print I/O APICs from MP tables\n");
    PRINT("apicc - print configuration of first I/O APIC\n");
    PRINT("irqstat - print IRQ statistic\n");
    PRINT("ahci - print AHCI ports\n");
    PRINT("reboot - reboot machine\n");
    PRINT("rtc - print RTC / CMOS info\n");
    PRINT("vga - print VGA controller information\n");
    PRINT("timer - print not yet expired timer\n");
    PRINT("locks - print locks (blocking semaphores and rw_locks only)\n");
    PRINT("trace - print stacktrace\n");
    PRINT("lsof - list open files\n");
    PRINT("lapic - print configuration of local APIC\n");
    PRINT("mmval - validate memory layout\n");
    PRINT("if - print network interfaces\n");
    PRINT("route - print routing table\n");
    PRINT("pir - print PIR BIOS table\n");
    PRINT("multiboot - print multiboot information\n");
    PRINT("acpi - print basic ACPI information\n");
    PRINT("madt - print the MADT ACPI table\n");
}

/*
 * Print register content on screen
 */
static void print_regs() {
    char* arg;
    int cpuid = 0;
    char* end_ptr;
    /*
     * Next token is CPU
     */
    arg = strtok(0, " \n");
    if (0 == arg) {
        cpuid = smp_get_cpu();
    }
    else {
        cpuid = strtol(arg, &end_ptr, 10);
        if ((*end_ptr != 0) && (*end_ptr != 0xa))  {
            PRINT("Invalid argument: %s\n", arg);
            return;
        }
    }
    if ((cpuid < 0) || (cpuid >= SMP_MAX_CPU)) {
        PRINT("Invalid CPU ID: %d\n", cpuid);
        return;
    }
    if (cpu_state[cpuid] != 2) {
        PRINT("No context available for CPU %d\n", cpuid);
        return;
    }
    PRINT("Register contents for CPU %d: \n", cpuid);
    print_ir_context(cpu_context+cpuid);
    if (smp_get_cpu() == cpuid)
        PRINT("CR0=%x (PG=%d, PE=%d, CD=%d, NW=%d, TS=%d, MP=%d)\n", get_cr0(), (get_cr0() >> 31), get_cr0() & 0x1,(get_cr0() >> 30) & 0x1,
                (get_cr0() >> 29) & 0x1, (get_cr0() >> 3) & 0x1, (get_cr0() >> 1) & 0x1);
}

/*
 * Dump memory content on screen
 * Note that we assume that the main routine has already
 * done one strtok on the command so that we can access the parameters
 * by calling strtok again
 */
static void dump_memory() {
    char* arg;
    int count;
    u32 base;
    char* end_ptr;
    int i;
    /*
     * Next token is expected to start
     * with slash
     */
    arg = strtok(0, " ");
    if (0 == arg) {
        PRINT("No arguments supplied\n");
        return;
    }
    if ((arg[0] != '/') || (arg[1] == 0)) {
        PRINT("Invalid first argument\n");
        PRINT("Expected /n with a decimal number n, got %s\n", arg);
        return;
    }
    count = strtol((const char*) arg + 1, &end_ptr, 10);
    if ((*end_ptr != 0) && (*end_ptr != 10)) {
        PRINT("Invalid argument, should be /n with a decimal number n\n");
        return;
    }
    /*
     * Next token is start address
     */
    arg = strtok(0, " ");
    if (0 == arg) {
        PRINT("Second argument missing\n");
        return;
    }
    base = strtoull((const char*) arg, &end_ptr, 16);
    if ((*end_ptr != 0) && (*end_ptr != 10)) {
        PRINT("Invalid second argument, should be a hexadecimal number, was %s\n", arg);
        return;
    }
    /* Now do the dump */
    PRINT("Dumping %d bytes starting at virtual %p\n", count,base);
    for (i = 0; i < count; i++) {
        if (0 == (i % 16)) {
            PRINT("\n");
            PRINT("%p    ", base+i);
        }
        if (mm_page_mapped(base+i)) {
            PRINT("%h ", *(((char*) base)+i));
        }
        else
            PRINT("NA ");
    }
    PRINT("\n");
}




/*
 * Determine whether an address is located on the stack
 */
static int on_stack(ir_context_t* context, u32 address) {
    if (!mm_page_mapped((address/4096)*4096))
            return 0;
    return 1;
}


/*
 * Print a stacktrace
 */
static void print_stacktrace(ir_context_t* context) {
    int i=0;
    int iterations;
    char* arg = strtok(0, " \n");
    if (0==arg) {
        PRINT("No argument specified - using 10 iterations as default\n");
        iterations = 10;
    }
    else {
        iterations = strtol(arg, 0, 10);
        PRINT("Doing %d iterations\n", iterations);
    }
    u32 ebp = context->ebp;
    u32* ebp_ptr = (u32*) ebp;
    PRINT("Frame (EPB)    RET         ARG1        ARG2        ARG3        ARG4\n");
    PRINT("-------------------------------------------------------------------\n");
    for (i=0;i<iterations;i++) {
        if (!on_stack(context, ebp))
            break;
        if (!on_stack(context, ebp+4))
            break;
        PRINT("%x      %x", ebp, *(ebp_ptr+1));
        if (on_stack(context, ebp+8))
            PRINT("   %x", *(ebp_ptr+2));
        else
            PRINT("   N/A      ");
        if (on_stack(context, ebp+12))
            PRINT("   %x", *(ebp_ptr+3));
        else
            PRINT("   N/A      ");
        if (on_stack(context, ebp+16))
            PRINT("   %x", *(ebp_ptr+4));
        else
            PRINT("   N/A      ");
        if (on_stack(context, ebp+20))
            PRINT("   %x", *(ebp_ptr+5));
        else
            PRINT("   N/A      ");
        PRINT("\n");
        if (*ebp_ptr < ebp)
            break;
        ebp = *ebp_ptr;
        if (0 == mm_page_mapped((ebp / 4096)*4096)) {
            PRINT("Address %x is not mapped, stopping\n", ebp);
            break;
        }
        ebp_ptr = (u32*) ebp;
    }
}

/*
 * Print some important bytes from the CMOS / RTC
 */
static void print_cmos() {
    u8 value;
    PRINT("Description         Byte      Value\n");
    PRINT("------------------------------------\n");
    value = rtc_read_register(RTC_SHUTDOWN_STS);
    PRINT("Shutdown status     0xF       %w\n", value);
}




/*
 * Inform the debugger that we are waiting for a lock
 * Parameters:
 * @addr - address of lock
 * @type - 0 = semaphore, 1 = read/write lock
 * @rw - 0 = get lock for read, 1 = get lock for write
 */
void debug_lock_wait(u32 lock_addr, int type, int rw, char* file, int line) {
    int i;
    int hit = -1;
    u32 eflags;
    /*
     * Lock info table
     */
    spinlock_get(&lock_info_lock, &eflags);
    /*
     * Find free slot
     */
    for (i = 0; i< 1024; i++) {
        if (LOCK_TYPE_NONE == lock_info[i].lock_type) {
            hit = i;
            break;
        }
    }
    if (-1 == hit) {
        ERROR("Could not register lock\n");
        spinlock_release(&lock_info_lock, &eflags);
        return;
    }
    /*
     * Store information
     */
    lock_info[hit].lock_type = type;
    lock_info[hit].rw = rw;
    lock_info[hit].task_waiting = pm_get_task_id();
    lock_info[hit].task_acquired = 0;
    lock_info[hit].lock_addr = lock_addr;
    lock_info[hit].line = line;
    lock_info[hit].lock_status = LOCK_STS_WAITING;
    if (file)
        strncpy(lock_info[i].file, file, 256);
    /*
     * and release lock
     */
    spinlock_release(&lock_info_lock, &eflags);
}

/*
 * Inform the debugger that we got a lock
 */
void debug_lock_acquired(u32 lock_addr, int rw) {
    int i;
    int hit = -1;
    u32 eflags;
    int self = pm_get_task_id();
    /*
     * Lock info table
     */
    spinlock_get(&lock_info_lock, &eflags);
    /*
     * Find slot
     */
    for (i = 0; i< 1024; i++) {
        if ((lock_addr == lock_info[i].lock_addr) && (self == lock_info[i].task_waiting) && (rw == lock_info[i].rw)) {
            hit = i;
            break;
        }
    }
    if (-1 == hit) {
        ERROR("Did not find lock info entry for lock %x, task %x\n", lock_addr, self);
        spinlock_release(&lock_info_lock, &eflags);
        return;
    }
    /*
     * Update information
     */
    lock_info[hit].lock_status = LOCK_STS_ACQUIRED;
    lock_info[hit].task_acquired = self;
    lock_info[hit].task_waiting = 0;
    /*
     * and release lock
     */
    spinlock_release(&lock_info_lock, &eflags);
}

/*
 * Inform the debugger that we have released a lock
 */
void debug_lock_released(u32 lock_addr, int rw) {
    int i;
    int hit = -1;
    u32 eflags;
    int self = pm_get_task_id();
    /*
     * Lock info table
     */
    spinlock_get(&lock_info_lock, &eflags);
    /*
     * Find slot
     */
    for (i = 0; i< 1024; i++) {
        if ((lock_addr == lock_info[i].lock_addr) && (self == lock_info[i].task_acquired) && (rw == lock_info[i].rw)) {
            hit = i;
            break;
        }
    }
    if (-1 == hit) {
        spinlock_release(&lock_info_lock, &eflags);
        return;
    }
    /*
     * Clean information
     */
    lock_info[hit].lock_type = LOCK_TYPE_NONE;
    /*
     * and release lock
     */
    spinlock_release(&lock_info_lock, &eflags);
}

/*
 * Inform the debugger that we have cancelled a lock request
 */
void debug_lock_cancel(u32 lock_addr, int rw) {
    int i;
    int hit = -1;
    u32 eflags;
    int self = pm_get_task_id();
    /*
     * Lock info table
     */
    spinlock_get(&lock_info_lock, &eflags);
    /*
     * Find slot
     */
    for (i = 0; i< 1024; i++) {
        if ((lock_addr == lock_info[i].lock_addr) && (self == lock_info[i].task_waiting) && (rw == lock_info[i].rw)) {
            hit = i;
            break;
        }
    }
    if (-1 == hit) {
        ERROR("Did not find lock info entry for lock %x\n", lock_addr);
        spinlock_release(&lock_info_lock, &eflags);
        return;
    }
    /*
     * Clean information
     */
    lock_info[hit].lock_type = LOCK_TYPE_NONE;
    /*
     * and release lock
     */
    spinlock_release(&lock_info_lock, &eflags);
}


/*
 * Print lock info
 */
static void print_locks() {
    int i;
    PRINT("Waiting  Acquired     TYPE    RW   ADDR       STS\n");
    PRINT("--------------------------------------------------------------------------\n");
    for (i = 0;i < 1024; i++) {
        if (LOCK_TYPE_NONE != lock_info[i].lock_type) {
            PRINT("%w      %w        %s     %h   %x  %h", lock_info[i].task_waiting, lock_info[i].task_acquired,
                    (lock_info[i].lock_type == LOCK_TYPE_RW) ? "RW " : "SEM", lock_info[i].rw,
                    lock_info[i].lock_addr, lock_info[i].lock_status);
            if (lock_info[i].file) {
                PRINT("  %d@%s\n", lock_info[i].line, lock_info[i].file);
            }
            else {
                PRINT("\n");
            }
        }
    }
}

/*
 * Main entry point for internal debugger. When invoked, this function will
 * first print a dump of the register set on the stack. See the function
 * debug_print_usage for a list of supported commands
 * Parameter:
 * @ir_context - the current IR context
 */
void debug_main(ir_context_t* ir_context) {
    int cpu;
    char c[2];
    char line[256];
    char* cmd;
    u32 eflags;
    cli();
    spinlock_get(&debugger_lock, &eflags);
    /*
     * Store context
     */
    store_context(ir_context);
    /*
     * If debugger is already running, enter idle loop
     */
    if (1 == debugger_running) {
        spinlock_release(&debugger_lock, &eflags);
        while (1) {
            asm("sti ; hlt");
        }
        PRINT("---------------- should never get here! -------------\n");
        asm("cli ; hlt");
    }
    debugger_running = 1;
    spinlock_release(&debugger_lock, &eflags);
    PRINT("Debugger started\n");
    PRINT("Current interrupt: %h\n", ir_context->vector);
    if (ir_context->vector <= 17) {
        PRINT("Exception type:    %s\n", exceptions[ir_context->vector]);
    }
    PRINT("CPU:               %d\n", smp_get_cpu());
    PRINT("Active task:       %d\n", pm_get_task_id());
    PRINT("Active process:    %d\n", pm_get_pid());
    print_ir_context(ir_context);
    PRINT("CR0=%x (PG=%d, PE=%d, WP=%d)\n", get_cr0(), (get_cr0() >> 31), get_cr0() & 0x1, (get_cr0() >> 16) & 0x1);
    print_stacktrace(ir_context);
    /*
     * Stop all other CPUs as well
     */
    for (cpu = 0; cpu < SMP_MAX_CPU; cpu++) {
        if (((cpu_active[cpu]) || (0==cpu)) && (cpu != smp_get_cpu()))  {
            PRINT("Sending debug IPI to CPU %d\n", cpu);
            apic_send_ipi(cpu_get_apic_id(cpu), 0, IPI_DEBUG, 0);
        }
    }
    PRINT("Enter command or help\n");
    while (1) {
        PRINT(">");
        debug_getline(line, 250);
        cmd = strtok(line, (const char*) " ");
        if (0 == cmd) {
            print_usage();
        }
        else if (0 == strncmp("help", cmd, 4)) {
            print_usage();
        }
        else if (0 == strncmp("regs", cmd, 4)) {
            print_regs(ir_context);
        }
        else if (0 == strncmp("x", cmd, 1)) {
            dump_memory();
        }
        else if (0 == strncmp("pt", cmd, 2)) {
            print_pagetables();
        }
        else if (0 == strncmp("tasks", cmd, 5)) {
            pm_print_task_table();
        }
        else if (0 == strncmp("sched", cmd, 5)) {
            sched_print();
        }
        else if (0 == strncmp("sa", cmd, 2)) {
            mm_print_stack_allocators();
        }
        else if (0 == strncmp("reboot", cmd, 6)) {
            reboot();
        }
        else if (0 == strncmp("vmem", cmd, 4)) {
            mm_print_vmem();
        }
        else if (0 == strncmp("pmem", cmd, 4)) {
            mm_print_pmem();
        }
        else if (0 == strncmp("pci", cmd, 3)) {
            pci_list_devices();
        }
        else if (0 == strncmp("pata", cmd, 4)) {
            pata_print_devices();
            PRINT("Hit enter to display PATA request queues\n");
            debug_getline(c, 1);
            pata_print_queue();
        }
        else if (0 == strncmp("buslist", cmd, 7)) {
            mptables_print_bus_list();
        }
        else if (0 == strncmp("cpulist", cmd, 7)) {
            cpu_print_list();
        }
        else if (0 == strncmp("irqr", cmd, 4)) {
            mptables_print_routing_list();
        }
        else if (0 == strncmp("mpapics", cmd, 5)) {
            mptables_print_io_apics();
        }
        else if (0 == strncmp("apicc", cmd, 5)) {
            if (0 == acpi_used())
                mptables_print_apic_conf();
            else
                apic_print_configuration(acpi_get_primary_ioapic());
        }
        else if (0 == strncmp("irqstat", cmd, 7)) {
            irq_print_stats();
        }
        else if (0 == strncmp("ahci", cmd, 4)) {
            ahci_print_ports();
            PRINT("Hit enter to display AHCI request queues\n");
            debug_getline(c, 1);
            ahci_print_queue();
        }
        else if (0 == strncmp("lsof", cmd, 4)) {
            fs_print_open_files();
        }
        else if (0 == strncmp("trace", cmd, 5)) {
            print_stacktrace(ir_context);
        }
        else if (0 == strncmp("vga", cmd, 3)) {
            vga_debug_regs();
        }
        else if (0 == strncmp("rtc", cmd, 3)) {
            print_cmos();
        }
        else if (0 == strncmp("ticks", cmd, 5)) {
            timer_print_cpu_ticks();
        }
        else if (0 == strncmp("cpus", cmd, 4)) {
            print_cpus();
        }
        else if (0 == strncmp("locks", cmd, 5)) {
            print_locks();
        }
        else if (0 == strncmp("timer", cmd, 5)) {
            timer_print_timers();
        }
        else if (0 == strncmp("lapic", cmd, 5)) {
            lapic_print_configuration();
        }
        else if (0 == strncmp("mmval", cmd, 5)) {
            mm_validate();
        }
        else if (0 == strncmp("if", cmd, 2)) {
            net_if_print();
        }
        else if (0 == strncmp("route", cmd, 5)) {
            ip_print_routing_table();
        }
        else if (0 == strncmp("pir", cmd, 3)) {
            mptables_print_pir_table();
        }
        else if (0 == strncmp("multiboot", cmd, 9)) {
            multiboot_print_info();
        }
        else if (0 == strncmp("acpi", cmd, 4)) {
            acpi_print_info();
        }
        else if (0 == strncmp("madt", cmd, 4)) {
            acpi_print_madt();
        }
        else {
            print_usage(line);
        }
    }
}

