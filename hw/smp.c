/*
 * smp.c
 *
 * This file contains SMP specific code
 */

/*
 * SMP specific routines
 *
 * This module contains code which is specific for symmetric multiprocessing (SMP) support. This includes startup code as well as code
 * to determine the ID of the CPU on which a thread is running.
 *
 * The startup code collaborates closely with the trampoline code in trampoline.S, so read this as well to understand what is going on
 */

#include "debug.h"
#include "io.h"
#include "rtc.h"
#include "lib/string.h"
#include "gdt.h"
#include "cpu.h"
#include "util.h"
#include "tests.h"
#include "smp.h"
#include "sched.h"
#include "pm.h"
#include "mm.h"
#include "smp_const.h"
#include "timer.h"
#include "params.h"


/*
 * Is SMP enabled?
 */
static int __smp_enabled = 1;

/*
 * Number of detected CPUs
 */
static int cpu_count = 1;

/*
 * Module name which appears in messages
 * created via the MSG macro
 */
static char* __module = "SMP   ";

/*
 * These symbols are defined in trampoline.S and mark the beginning and the
 * end of the trampoline code in the kernel ELF file
 */
extern u32 _trampoline_start;
extern u32 _trampoline_end;


/*
 * These are some flags which are used to synchronize the startup between BSP
 * and AP
 * When entering smp_ap_main, the AP will wait for the flag smp_run_main to
 * be set to 1 by the BSP
 * When the AP has entered the idle loop, it will set the flag smp_gone_idle to 1
 */
static int smp_run_main[SMP_MAX_CPU];
static int smp_gone_idle[SMP_MAX_CPU];


/****************************************************************************************
 * These functions set and read some flags which are used to synchronize the startup of *
 * an AP with the processing on the BSP                                                 *
 ***************************************************************************************/

/*
 * Release the main startup routine for the AP. This needs to be called by the BSP
 * to allow the AP to start the processing in smp_ap_main
 * Parameter:
 * @cpuid - ID of the CPU for which the processing is to be released
 */
void smp_start_main(int cpuid) {
    smp_run_main[cpuid] = 1;
    smp_mb();
}

/*
 * Wait until an AP has entered idle loop
 * Parameter:
 * @cpuid - ID of the CPU for which we want to wait
 */
void smp_wait_idle(int cpuid) {
    KASSERT(1 == IRQ_ENABLED(get_eflags()));
    while (0 == smp_gone_idle[cpuid]) {
        asm("hlt");
    }
}

/****************************************************************************************
 * The following functions contain the SMP startup code. While smp_start_aps() is       *
 * called by the BSP to start all APs, smp_ap_main() is the code which is run by an AP  *
 * immediately after starting in protected mode                                         *
 ***************************************************************************************/


/*
 * Utility function to copy the trampoline code (i.e. the code which
 * the AP will start to execute immediately after startup) to its final
 * location below 1 MB. The trampoline code itself is defined in trampoline.S
 * and linked into the kernel ELF file. As the ELF file is located above 1 MB
 * and the APs start in real mode, we need to relocate it to an area below 1 MB
 */
static void copy_trampoline_code() {
    int bytes = ((int)&_trampoline_end) - ((int)&_trampoline_start);
    memcpy((void*) TRAMPOLINE, &_trampoline_start, bytes);
}

/*
 * Start up an individual AP
 * Parameter:
 * @cpuid - the ID of the CPU to be started
 * Return value:
 * 0 if the operation was successful
 * 1 if the CPU is not present or could not be started
 */
static int startup_ap(int cpuid) {
    u8 lapic;
    int external_apic;
    /*
     * Get ID of local APIC and APIC version
     */
    if (0xff == (lapic = cpu_get_apic_id(cpuid)))
        return 1;
    if (-1 == (external_apic = cpu_external_apic(cpuid)))
        return 1;
    /*
     * Store logical CPUID at an address where the trampoline code can find it
     */
    *((u32*)(AP_CPUID_ADDR+AP_DS*0x10)) = cpuid;
    /*
     * Reset AP status flags
     */
    *((u8*)(AP_RM_STATUS_ADDR+AP_DS*0x10)) = 0;
    *((u8*)(AP_PM_STATUS_ADDR+AP_DS*0x10)) = 0;
    /*
     * According to the MP specification, we are supposed to write the address of the trampoline code
     * to the warm reset vector if we are dealing with a 486DX.
     * We need to store the address as expected by an indirect far jmp, i.e. the offset
     * goes to 0x467, the code segment to 0x469. As the trampoline code starts on a page boundary,
     * the offset is zero and the segment is TRAMPOLINE / 0x10
     */
    if (external_apic) {
        *((u16*) WARM_RESET_VECTOR) = 0;
        *((u16*) (WARM_RESET_VECTOR + 2)) = TRAMPOLINE / 0x10;
        /*
         * In addition we should write 0xa into shutdown status in CMOS
         * so that the BIOS startup code will jump to the address in the warm reset
         * vector
         */
        rtc_write_register(RTC_SHUTDOWN_STS, RESET_ACTION_JMP);
    }
    /*
     * Send INIT IPI
     */
    if (apic_send_ipi(lapic, IPI_INIT, 0x0, 0)) {
        PANIC("INIT IPI failed for cpuid %d (lapic = %x)\n", cpuid, lapic);
    }
    /*
     * For an external APIC, we need the INIT Level de-assert IPI in addition
     */
    if (external_apic) {
        if (apic_send_ipi(lapic, IPI_INIT, 0x0, 1)) {
            PANIC("INIT Level de-assert IPI failed\n");
        }
    }
    /*
     * Wait for approx. 10 ms
     */
    mdelay(10);
    /*
     * Send Startup IPI to AP if this is an on-chip APIC. For an 486DX, the INIT IPI should have forced
     * a reset and the BIOS startup code should have taken us to the address specified in the warm reset
     * vector, so our trampoline code is already running. For later CPUs, the INIT IPI only places the CPU in
     * a "wait-for-SIPI" state, so the STARTUP IPI is necessary to get things going
     */
    if (!external_apic) {
        if (apic_send_ipi(lapic, IPI_STARTUP, TRAMPOLINE / 0x1000, 0)) {
            PANIC("SIPI failed\n");
        }
        /*
         * Now wait again for 10 ms
         */
        mdelay(10);
        /*
         * AP started? If not, send second STARTUP IPI. Note that
         * 1) Intel recommends to send two STARTUP IPIs to the CPU in any case, however I do not like this idea
         *    as if the CPU comes up after the first IPI, it is a matter of timing in what state it is hit by the
         *    second IPI
         * 2) The Intel specification mentions (System Programming Guide, Section 10.6) that in contrast to other
         *    IPIs, the STARTUP IPI is not automatically retried if it cannot be delivered by the local APIC. This
         *    might be the reason why Intel recommends to send two IPIs and we therefore are prepared to send the
         *    second IPI if needed
         */
        if (0 == *((u8*)(AP_RM_STATUS_ADDR+AP_DS*0x10))) {
            if (apic_send_ipi(lapic, IPI_STARTUP, TRAMPOLINE / 0x1000, 0)) {
                PANIC("SIPI failed\n");
            }
            /*
             * Wait again
             */
            mdelay(10);
        }
    }
    /*
     * Wait 100 ms until AP has reached protected mode
     */
    timer_wait_ticks(HZ/10);
    if (0 == *((u8*)(AP_PM_STATUS_ADDR+AP_DS*0x10))) {
        PANIC("AP still not in protected mode, giving up\n");
        return 1;
    }
    cpu_count++;
    /*
     * If we have touched the shutdown status, reset it now
     */
    if (external_apic) {
        rtc_write_register(RTC_SHUTDOWN_STS, 0x0);
    }
    /*
     * Register CPU with debugger
     */
    debug_add_cpu(cpuid);
    /*
     * Do SMP tests immediately after boot if this is the first AP
     */
    if (SMP_BSP_ID + 1 == cpuid)
        do_smp_tests_boot_bsp();
    /*
     * Release main task for this CPU
     */
    smp_start_main(cpu_is_ap(lapic));
    /*
     * and wait for CPU
     */
    smp_wait_idle(cpu_is_ap(lapic));
    return 0;
}

/*
 * This is the main startup code in SMP mode. Once the BSP has completed
 * its initialization, it calls this function to detect and bring up all APs.
 *
 * At this point in time, the BSP is fully initialized and interrupts are
 * enabled (see the comments at the top of main.c)
 */
void smp_start_aps() {
    gdt_ptr_t* gdt_ptr;
    int cpuid;
    int ap_count = 0;
    /*
     * If the kernel parameter smp is set to zero, do nothing
     */
    if (0 == params_get_int("smp")) {
        MSG("Skipping APs as smp=0\n");
        return;
    }
    MSG("Starting all available CPUs\n");
    /*
     * First copy our trampoline code to its final location
     */
    copy_trampoline_code();
    /*
     * Get the address of an 48 bit pointer to the GDT
     * and store it at the fixed address 0x10004 where
     * it is expected by the trampoline code
     */
    gdt_ptr = gdt_get_ptr();
    memcpy((void*) (AP_GDTR_LOC+AP_DS*0x10), gdt_ptr, sizeof(gdt_ptr_t));
    /*
     * and copy content of CR3 to address 0x10014
     */
    *((u32*)(AP_CR3_ADDR+AP_DS*0x10)) = get_cr3();
    /*
     * Now try to bring up all CPUs
     */
    for (cpuid = SMP_BSP_ID + 1; cpuid < SMP_MAX_CPU; cpuid++) {
        if (0 == startup_ap(cpuid))
            ap_count++;
    }
    if (0 == ap_count)
        __smp_enabled = 0;
    /*
     * and rebalance IRQs
     */
    MSG("Rebalancing IRQs\n");
    irq_balance();
}

/*
 * Enter idle loop for AP
 */
static void smp_idle_loop() {
    int cpuid;
    cpuid = smp_get_cpu();
    /*
     * Enter loop
     */
    smp_gone_idle[cpuid] = 1;
    /*
     * Do tests if this is the first AP
     */
    if (SMP_BSP_ID + 1 == cpuid)
        do_pre_init_tests_ap();
    while (1) {
        asm("hlt");
    }
}

/*
 * This is the main entry point to the kernel for an AP
 * It is called by the trampoline code in trampoline.S.
 * At this time, the CPU is in the following state:
 * - the CPU is in protected mode in ring 0
 * - paging is enabled
 * - the address space is that of process 0
 * - no IDT exists yet
 * - no TSS exists yet for this CPU
 * - interrupts are disabled
 */
void smp_ap_main() {
    u32 tos;
    u32 gs;
    int pages;
    u8 local_apic_id;
    u32 idle_task;
    int cpuid;
    local_apic_id = apic_get_id();
    cpu_up(local_apic_id);
    /*
     * Store ID of CPU in GS register
     */
    cpuid = cpu_is_ap(apic_get_id());
    gs = SMP_CPUID_TO_GS(cpuid);
    set_gs(gs);
#ifdef DO_SMP_TEST
    /*
     * Do tests if we are the first AP
     */
    if (SMP_BSP_ID + 1 == cpuid)
        do_smp_tests_boot_ap();
#endif
    /*
     * Wait until BSP sets smp_run_main
     */
    while (0 == smp_run_main[cpuid]) {
        smp_mb();
        asm("pause");
    }
    /*
     * Create idle task
     */
    if ((idle_task = pm_create_idle_task(cpuid)) < 0) {
        PANIC("Could not create idle task, return code is %d\n", idle_task);
    }
    /*
     * and add it as idle task to scheduler queue
     */
    sched_add_idle_task(idle_task, cpuid);
    /*
     * Reserve stack area and switch to it
     */
    if (0 == (tos = mm_reserve_task_stack(idle_task, 0,  &pages))) {
        PANIC("Could not reserve task stack\n");
    }
    /*
     * Align top of stack to a double word. Note that mm_reserve_task_stack
     * will return a top-of-stack which is the top of a page minus 1. Thus the
     * alignment below gives us one double word on the stack which we need to
     * place the return address when we do any function calls
     */
    tos = (tos / sizeof(u32)) * sizeof(u32);
    /*
     * and load it. Note that once we have done this, we
     * can no longer access any local variables within this
     * function
     */
    asm("mov %0, %%esp" : : "r" (tos));
    /*
     * Set up local APIC
     */
    apic_init_ap();
    /*
     * Set up timer on AP
     */
    timer_init_ap();
    /*
     * enable interrupts
     */
    sti();
    /*
     * Enter idle loop
     */
    smp_idle_loop();
    /*
     * We should never get to this point - but if it happens, stop CPU
     */
    asm("cli ; hlt");
}

/****************************************************************************************
 * The following functions are used by other kernel modules to get SMP status           *
 * information                                                                          *
 ***************************************************************************************/

/*
 * Return the number of the CPU on which we are currently running. This number is potentially
 * invalidated if the task goes to sleep or yields the CPU!
 * Note that this returns the number of the logical CPU, starting at 0. By convention,
 * the ID of the BSP is 0. APs are number 1...n by the order in which they have
 * been found in the MP table
 * We access the CPU list without lock here as we assume that after startup, this list
 * is not modified any more
 * Return value:
 * CPU number
 */
int smp_get_cpu() {
    u32 cpuid;
    u32 gs;
    if (0 == __smp_enabled)
        return SMP_BSP_ID;
    gs = get_gs();
    cpuid = SMP_GS_TO_CPUID(gs);
    return cpuid;
}

/*
 * Return true if SMP is enabled.
 */
int smp_enabled() {
    return __smp_enabled;
}

/*
 * Return number of available CPUs
 */
int smp_get_cpu_count() {
    return cpu_count;
}
