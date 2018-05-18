/*
 * irq.c
 *
 * This module contains the interrupt manager. During boot time, the interrupt manager is responsible for
 * setting up the PIC respectively the I/O APIC and the local APIC. Later, the interrupt manager dispatches incoming
 * interrupts and sends an EOI if necessary.
 *
 * The interrupt manager dynamically assigns interrupt vectors (=index into IDT) to interrupt lines and other interrupt
 * sources. This is especially relevant for interrupts routed via the local APIC, as for these interrupts, the upper 4 bits of
 * the interrupt vector determine the priority with which interrupts are forwarded to the CPU by the local APIC. Currently ctOS
 * distributes the available vectors as follows.
 *
 *                -------------------------------
 *                |         0x90 - 0xFF         |     <---- unused
 *                -------------------------------
 *                |         0x80 - 0x8F         |     <---- used for system calls (0x80), scheduler and IPIs
 *                -------------------------------
 *                |         0x70 - 0x7F         |     <---- Priority 1 hardware interrupts via I/O APIC
 *                -------------------------------
 *                |         0x60 - 0x6F         |     <---- Priority 2 hardware interrupts via I/O APIC
 *                -------------------------------
 *                |         0x50 - 0x5F         |     <---- Priority 3 hardware interrupts via I/O APIC
 *                -------------------------------
 *                |         0x40 - 0x4F         |     <---- Priority 4 hardware interrupts via I/O APIC
 *                -------------------------------
 *                |         0x30 - 0x3F         |     <---- Priority 5 hardware interrupts via I/O APIC
 *                -------------------------------
 *                |         0x20 - 0x2F         |     <---- Used in PIC mode for IRQs 0 - 15
 *                -------------------------------
 *                |         0x0 - 0x1F          |     <---- Traps and exceptions
 *                -------------------------------
 *
 * A driver can register an interrupt handler for a specific device (PCI) or by specifying a legacy ISA interrupt (ISA/LPC). When
 * processing such a request, the interrupt manager will use the ACPI tables or the MP specification table to locate the I/O APIC pin 
 * connected to this device. It then determines whether a vector has already been assigned to this IRQ. If yes, the handler is added 
 * to the list of handlers for this previously assigned vector.
 *
 * If no vector has been assigned yet, a new vector is chosen. For this purpose, the table of vectors is scanned from the top to the
 * bottom starting at the specified priority until a free entry is found. Then the newly established mapping is added to an internal
 * table and a new entry is added to the I/O APIC redirection tabl (unless MSI is used, in which case this entry is not needed).
 *
 * In PIC mode, the handling is similar, but the mapping of interrupts to vectors is fixed and determined by the hardware.
 *
 * Interrupt routing:
 *
 * If an I/O APIC is present, ctOS supports four different mechanisms to route interrupts to CPUs which can be chosen
 * via the kernel parameter apic.
 *
 * apic=1: interrupts are set up in physical delivery mode and sent to the BSP
 * apic=2: interrupts are set up in logical delivery mode and distributed to different CPUs, however the assignment is static, i.e
 *         each interrupts goes to a fixed CPU. This is the default
 * apic=3: use lowest priority delivery mode
 *
 * At boot time, when device drivers request interrupt handler mappings, all interrupts are set up in mode 1. This is done to make
 * sure that no interrupts are delivered to an AP which is not yet operating (in this case, the I/O APIC might wait indefinitely for
 * the EOI, thereby blocking any other interrupts which can freeze the machine). When all APs have been brought up, smp.c will call
 * the function irq_balance() to set up the interrupt redirection entries in the I/O APIC in the final mode.
 *
 * Some devices like the PIT are supposed to be always serviced by the BSP. For these devices, the initial setup of the handler can
 * be done with the parameter  "lock" which will mark the interrupt as not distributable to other CPUs. During balancing, these
 * interrupts are not considered.
 *
 */

#include "irq.h"
#include "mptables.h"
#include "acpi.h"
#include "debug.h"
#include "pic.h"
#include "tests.h"
#include "keyboard.h"
#include "io.h"
#include "systemcalls.h"
#include "pm.h"
#include "sched.h"
#include "util.h"
#include "kerrno.h"
#include "pit.h"
#include "pci.h"
#include "mm.h"
#include "lists.h"
#include "lib/string.h"
#include "params.h"
#include "cpu.h"
#include "smp.h"
#include "gdt_const.h"

static char* __module = "IRQ   ";
static int __irq_loglevel = 0;
#define IRQ_DEBUG(...) do {if (__irq_loglevel > 0 ) { kprintf("DEBUG at %s@%d (%s): ", __FILE__, __LINE__, __FUNCTION__); \
        kprintf(__VA_ARGS__); }} while (0)

extern void (*debug_getline)(void* c, int n);

/*
 * This static flag is the mode in which we operate, i.e. IRQ_MODE_PIC
 * or IRQ_MODE_APIC
 */
static int irq_mode;

/*
 * Tables holding a list of ISRs per vector
 */
static isr_handler_t* isr_handler_list_head[IRQ_MAX_VECTOR+1];
static isr_handler_t* isr_handler_list_tail[IRQ_MAX_VECTOR+1];

/*
 * Table holding a list of vector - interrupt assignments.
 * A value of -1 means "not used".
 */
static int irq[IRQ_MAX_VECTOR+1];


/*
 * Interrupt counter per CPU
 */
static u32 irq_count[SMP_MAX_CPU][256];

/*
 * Is the interrupt locked? An interrupt is locked if it is not distributed
 * to a CPU other than the BSP. This is needed for the global timer which is
 * always connected to the BSP
 */
static int irq_locked[IRQ_MAX_VECTOR];

/*
 * APIC mode, i.e. validated value of kernel parameter APIC
 */
static int apic_mode = 1;


/****************************************************************************************
 * The following functions perform the assignment of hardware interrupts to vectors     *
 * and manage interrupt handlers                                                        *
 ***************************************************************************************/

/*
 * Given a priority, locate a free slot in the interrupt - vector assignment table
 * Parameter:
 * @irq - the IRQ which needs to be assigned a vector
 * @priority - the priority, ranging from 1 (highest) to 5 (lowest)
 * @new - will be set if the assignment is new
 * Return value:
 * -1 if no free slot could be found
 * the assigned vector
 * Note: no locking is done here, only use this at boot time
 */
 static int assign_vector(int _irq, int priority, int* new) {
    int vector;
    int top;
    int bottom;
    /*
     * In PIC mode, the assignment is fixed
     */
    if (IRQ_MODE_PIC == irq_mode) {
        *new = 0;
        return _irq + IRQ_OFFSET_PIC;
    }
    /*
     * If we get here, we are not in PIC mode and have some work ahead of us
     * First scan the table to see whether we already have
     * assigned a vector for this IRQ previously - but not for MSI
     */
    if (IRQ_MSI != _irq) {
        for (vector = 0; vector < IRQ_MAX_VECTOR; vector++) {
            if (_irq == irq[vector]) {
                *new = 0;
                return vector;
            }
        }
    }
    /*
     * If we get to this point, we have to define a new assignment.
     * Start search at highest vector for the given priority
     */
    if (priority > IRQ_PRIO_LOWEST)
        priority = IRQ_PRIO_LOWEST;
    if (priority < IRQ_PRIO_HIGHEST)
        priority = IRQ_PRIO_HIGHEST;
    top = 0x7F - 0x10 * (priority - 1);
    bottom = IRQ_OFFSET_APIC;
    for (vector = top; vector >= bottom; vector--) {
        if (IRQ_UNUSED == irq[vector]) {
            irq[vector] = _irq;
            *new = 1;
            return vector;
        }
    }
    ERROR("Could not determine free vector for IRQ %d\n", _irq);
    return -1;
}

/*
 * Read some configuration data from the ACPI / MP tables, 
 * specifically get the I/O APIC to be used, the trigger mode
 * and the polarity
 * Parameter:
 * @_irq - the I/O APIC pin 
 * @trigger - will be filled
 * @polarity - will be filled
 * @ioapic - will be filled
 * Returns:
 * 1 if the data could be found
 * 0 if we do not have that data
 * 
 */
static int get_irq_config_data(int _irq, int* trigger, int* polarity, io_apic_t** io_apic) {
    int found = 0;
    if (0 == acpi_used()) {
        found = mptables_get_trigger_polarity(_irq, polarity, trigger);
        *io_apic = mptables_get_primary_ioapic();
    }
    else {
        found = acpi_get_trigger_polarity(_irq, polarity, trigger);
        if (0 == found) {
            /*
             * Fall back to MP tables
             */
             found = mptables_get_trigger_polarity(_irq, polarity, trigger);
        }
        *io_apic = acpi_get_primary_ioapic();
    }
    return found;
}

/*
 * Given an interrupt number and a priority, determine a free vector number
 * (or a vector number assigned previously to this IRQ), add an entry to the
 * I/O APIC redirection table if needed and set up an ISR
 * Parameter:
 * @irq - the IRQ number
 * @priority - priority (1 - 5)
 * @isr - the interrupt service handler
 * @force_bsp - force delivery to BSP
 * Return value:
 * the vector assigned for this interrupt
 * a negative number if the assignment failed
 */
 static int add_isr(int _irq, int priority, isr_t isr, int force_bsp, pci_dev_t* pci_dev) {
     int vector;
     int new;
     isr_handler_t* isr_handler;
     int polarity;
     int trigger_mode;
     int found = 0;
     io_apic_t* io_apic;
     /*
      * Determine vector to use or reuse existing one
      */
    vector = assign_vector(_irq, priority, &new);
    DEBUG("Using interrupt vector %d\n", vector);
    /*
     * If this is not an MSI, we need to get some
     * configuration information first and set up the
     * IO APIC.
     * We need trigger, polarity and the IO APIC
     */
    if (IRQ_MSI != _irq) {
        found = get_irq_config_data(_irq, &trigger_mode, &polarity, &io_apic);
        /*
        * If this is the first assignment, add entry to I/O APIC
        */
        if ((1 == new) && (IRQ_MODE_APIC == irq_mode)) {
            if (0 == io_apic) {
                ERROR("Could not detect IO APIC to use\n");
                irq[vector] = IRQ_UNUSED;
                return -1;
            }
            if (found) {
                apic_add_redir_entry(io_apic, _irq, polarity, trigger_mode,
                        vector, ( (1 == force_bsp) ? 1 : apic_mode));
            }
            else {
                ERROR("Could not locate entry in configuration tables for IRQ %d\n", _irq);
                irq[vector] = IRQ_UNUSED;
                return -1;
            }
        }
    }
    else {
        /* 
         * MSI case. First make sure that we have a PCI device
         * in this case (this function is also called for ISA 
         * devices)
         */
        if (0 == pci_dev) {
            ERROR("No PCI device specified\n");
            irq[vector] = IRQ_UNUSED;
            return -1;
        }
        /*
         * Now ask the PCI bus driver to set up the MSI
         * routing for us
         */
        pci_config_msi(pci_dev, vector);
    }
    /*
     * Check whether this handler has already been added to the list for this
     * vector - if yes return
     */
    LIST_FOREACH(isr_handler_list_head[vector], isr_handler) {
        if (isr_handler->handler == isr) {
            return vector;
        }
    }
    /*
     * Add new handler to list
     */
    isr_handler = (isr_handler_t*) kmalloc(sizeof(isr_handler_t));
    if (0 == isr_handler) {
        ERROR("Could not allocate memory for ISR handler\n");
        return -ENOMEM;
    }
    isr_handler->handler = isr;
    LIST_ADD_END(isr_handler_list_head[vector], isr_handler_list_tail[vector], isr_handler);
    return vector;
 }

/*
 * Register a handler for a given PCI device
 * Parameter:
 * @isr - interrupt service handler
 * @priority - priority (1 - 5)
 * @pci_dev - the PCI device
 * Return value:
 * the vector which has been assigned to this interrupt
 * a negative number if the assignment failed
 */
 int irq_add_handler_pci(isr_t new_isr, int priority, pci_dev_t* pci_dev) {
     DEBUG("Adding handler, isr = %p\n", new_isr);
     int _irq;
     if (0 == new_isr) {
         ERROR("Invalid argument - null handler\n");
         return EINVAL;
     }
     if (0 == pci_dev) {
         ERROR("Invalid argument - PCI device is null\n");
         return EINVAL;
     }
     /* 
      * If we can use msi, do this
      */
    if ((1 == pci_dev->msi_support) & (1 == params_get_int("use_msi"))) {
        _irq = IRQ_MSI;
        MSG("Using MSI for this device\n");
    }
    else {
        /*
        * Scan MP table to locate IRQ for this device or get
        * legacy IRQ from device in PIC mode. For PCI devices,
        * we try to read the MP table even if we are in ACPI mode
        * as the PCI routing is in AML part of the DSDT which we 
        * do not support
        */
       if (IRQ_MODE_APIC == irq_mode) {
            _irq = mptables_get_irq_pin_pci(pci_dev->bus->bus_id, pci_dev->device, pci_dev->irq_pin);
            DEBUG("Got IRQ %d from configuration tables\n", _irq);
        }
        else {
            _irq = pci_dev->irq_line;
            DEBUG("Got legacy IRQ %d\n", _irq);
        }
    }
    if (IRQ_UNUSED == _irq) {
        ERROR("Could not locate MP table entry for device %d, pin %d on bus %d\n", pci_dev->device, pci_dev->irq_pin,
                 pci_dev->bus->bus_id);
        return -EINVAL;
    }
    /*
     * Add handler and redirection entry if needed
     */
    return add_isr(_irq, priority, new_isr, 1, pci_dev);
 }

 /*
  * Register a handler for a legacy ISA IRQ
  * Parameter:
  * @isr - interrupt service handler
  * @priority - the priority
  * @irq - the interrupt number
  * @lock - always send interrupt to BSP even after rebalancing
  * Return value:
  * the vector which has been assigned to this interrupt
  * or a negative error code
  */
int irq_add_handler_isa(isr_t new_isr, int priority, int _irq, int lock) {
    int apic_pin = 0;
    int vector;
    if (0 == new_isr) {
        ERROR("Invalid argument - null handler\n");
        return -EINVAL;
    }
    /*
     * Scan ACPI / MP table to locate IRQ for this device or directly use
     * legacy IRQ
     */
    if (IRQ_MODE_APIC == irq_mode) {
        if (0 == acpi_used()) {
            apic_pin = mptables_get_apic_pin_isa(_irq);    
            DEBUG("Got APIC pin %h from MP tables\n", apic_pin);
        }
        else {
            apic_pin = acpi_get_apic_pin_isa(_irq);    
            DEBUG("Got APIC pin %h from ACPI tables\n", apic_pin); 
        }
    }
    else {
        apic_pin = _irq;
    }
    if (IRQ_UNUSED == apic_pin) {
        ERROR("Could not get APIC PIN for legacy IRQ %d\n", _irq);
        return -EINVAL;
    }
    /*
     * Add handler and redirection entry if needed
     * Use priority 1
     */
    DEBUG("Adding redirection entry for APIC pin %h\n", apic_pin);
    vector = add_isr(apic_pin, priority, new_isr, 1, 0);
    DEBUG("Got vector %d for apic_pin = %d, priority = %d\n", vector, apic_pin, priority);
    /*
     * Remember if the interrupt needs to be locked
     */
    if (vector > 0)
        if (lock)
            irq_locked[vector] = 1;
    return vector;
 }


/*
 * Redistribute interrupts to different CPUs according to the kernel parameter apic
 */
void irq_balance() {
    int vector;
    int trigger_mode;
    int polarity;
    io_apic_t* io_apic;
    int found = 0;
    /*
     * Do nothing if we are in PIC mode or if APIC mode is one
     */
    if ((IRQ_MODE_PIC == irq_mode) || (1 == apic_mode))
        return;
    /*
     * Walk all assigned vectors
     */
    for (vector = 0; vector <= IRQ_MAX_VECTOR; vector++) {
        /* TODO: handle MSI interrupts here */
        if ((IRQ_UNUSED != irq[vector]) && (IRQ_MSI != irq[vector])) {
            /*
             * Remap entry in I/O APIC only if irq_locked is not set
             * for this entry
             */
            if (0 == irq_locked[vector]) {
                found = get_irq_config_data(irq[vector], &polarity, &trigger_mode, &io_apic);
                if (found && io_apic) 
                    apic_add_redir_entry(io_apic, irq[vector], polarity, trigger_mode, vector, apic_mode);
            }
        }
    }
}

/****************************************************************************************
 * Actual interrupt processing at runtime                                               *
 ***************************************************************************************/

/*
 * Do EOI processing
 */
static void do_eoi(ir_context_t* ir_context) {
    if (ir_context->vector == 0x80)
        return;
    if (ir_context->vector == 0x81)
            return;
    if (ir_context->vector != 0x20)
        IRQ_DEBUG("Doing EOI for vector %d\n", ir_context->vector);
    if (params_get_int("irq_watch") == (ir_context->vector)) {
        DEBUG("Got EOI for context vector %d, ORIGIN_PIC = %d\n", ir_context->vector, ORIGIN_PIC(ir_context->vector));
    }
    /*
     * Is this a hardware interrupt?
     */
    if ((ir_context->vector >= IRQ_OFFSET_PIC)) {
         if (ORIGIN_PIC(ir_context->vector))
             pic_eoi(ir_context->vector, IRQ_OFFSET_PIC);
         else
             /*
              * The interrupt came via the local APIC, triggered either
              * by the I/O APIC or by MSI.
              */
             apic_eoi();
     }
}

/*
 * Handle exceptions, i.e. extract the vector from an IR
 * context and execute an exception handler if needed
 */
static void handle_exception(ir_context_t* ir_context) {
    /*
     * If this is not an exception or trap, return
     */
    if (ir_context->vector >= IRQ_OFFSET_PIC)
        return;
    switch (ir_context->vector) {
        case IRQ_TRAP_PF:
            mm_handle_page_fault(ir_context);
            break;
        case IRQ_TRAP_NM:
            pm_handle_nm_trap();
            break;
        default:
            debug_main(ir_context);
            break;
    }
}

/*
 * This interrupt handler is invoked by the
 * common interrupt handler in gates.S and is the main interrupt
 * level entry point for hard- as well as for soft-interrupts
 * If this function returns a non-zero value, the post interrupt handler will be
 * invoked afterwards, using this value as address for the common kernel stack
 * Parameters:
 * @ir_context - the current IR context
 * Return value:
 * if this function returns 1, the post-IRQ handler will be invoked for
 * this interrupt
 */
u32 irq_handle_interrupt(ir_context_t ir_context) {
    int rc = 0;
    isr_handler_t* isr_handler;
    int previous_execution_level = 0;
    int restart = 0;
    int first_exec = 1;
    ir_context_t saved_ir_context;
    int debug_flag = 0;
    /*
     * If debugger is running, ignore all interrupts except exceptions and the debugger IPI
     */
    if (debug_running() && (ir_context.vector >= 32) && (ir_context.vector != IPI_DEBUG)) {
        do_eoi(&ir_context);
        return 0;
    }
    /*
     * Increase IRQ count
     */
    irq_count[smp_get_cpu()][ir_context.vector]++;
    /*
     * Enter debugger right away if this is int 3 to avoid additional exceptions in the
     * following processing
     */
    if (0x3 == ir_context.vector)
        debug_main(&ir_context);
    /*
     * Determine new execution level and store old level on the stack
     */
    pm_update_exec_level(&ir_context, &previous_execution_level);
    while (restart || first_exec) {
        /*
         * Reset first execution flag and save interrupt context for later use when a restart is done
         */
        if (1 == first_exec) {
            first_exec = 0;
            saved_ir_context = ir_context;
        }
        else if (1 == restart) {
            /*
             * In case of restart, we need to restore the interrupt context saved during the first execution
             * to re-execute the system call again with the original context
             */
            ir_context = saved_ir_context;
        }
        /*
         * Is a system call? If yes, turn on interrupts and execute system call, then disable interrupts again
         */
        if (SYSCALL_IRQ == ir_context.vector) {
            sti();
            syscall_dispatch(&ir_context, previous_execution_level);
            cli();
        }
        /*
         * Hardware interrupt or scheduler interrupt. Search for a handler and execute it.
         */
        else if (ir_context.vector >= IRQ_OFFSET_PIC) {
            /*
             * Restart only allowed for system calls
             */
            if (restart) {
                PANIC("Restart flag set for a non-system call interrupt\n");
            }
            debug_flag = 0;
            LIST_FOREACH(isr_handler_list_head[ir_context.vector], isr_handler) {
                  if (isr_handler->handler) {
                      if (params_get_int("irq_watch") == (ir_context.vector)) {
                        DEBUG("Handling interrupt for vector %d, handler is %p\n", ir_context.vector, isr_handler->handler);
                      }
                      if (isr_handler->handler(&ir_context))
                          debug_flag = 1;
                  }
            }
            /*
             * Do EOI processing
             */
            do_eoi(&ir_context);
            /*
             * If this is the special debugging vector 0x82 used for
             * IPIs, or if a handler has set the debug flag, call debugger.
             */
              if ((IPI_DEBUG == ir_context.vector) || (1 == debug_flag)) {
                  debug_flag = 0;
                  debug_main(&ir_context);
              }
        }
        /*
         * Exception
         */
        else {
            handle_exception(&ir_context);
        }
        /*
         * Give process manager a chance to handle signals
         */
        restart = pm_process_signals(&ir_context);
        /*
         * As the process manager might have changed the task status, execute
         * a dummy hardware interrupt in case a restart is requested to trigger
         * rescheduling if needed
         */
        if (1 == restart) {
            cond_reschedule();
        }
    }
    /*
     * If we return to level 0 (user space) or 1 (kernel thread),
     * handle exit processing
     */
    if ((EXECUTION_LEVEL_KTHREAD == previous_execution_level) || (EXECUTION_LEVEL_USER == previous_execution_level)) {
        sti();
        pm_handle_exit_requests();
        cli();
    }
    /*
     * Restore previous execution level
     */
    pm_restore_exec_level(&ir_context, previous_execution_level);
    /*
     * Call scheduler to get next task to be run
     * and ask process manager to prepare task switch - but only do this
     * if we return to system call, kernel thread or user level. Thus the execution
     * of a hardware interrupt can be interrupted but is not preempted
     */
    rc = 0;
    if (!(EXECUTION_LEVEL_IRQ == previous_execution_level)) {
        rc = pm_switch_task(sched_schedule(), &ir_context);
    }
    if (rc)
        return mm_get_top_of_common_stack();
    return 0;
}

/*
 * This handler is called by the common handler in
 * gates.S after we have switched the stack to a common
 * kernel stack within the common area
 */
void irq_post() {
    /*
     * Execute cleanup function of process manager
     */
    pm_cleanup_task();
}

/****************************************************************************************
 * Initialization                                                                       *
 ***************************************************************************************/

/*
 * Initialize the IRQ manager, PIC and APIC
 */
void irq_init() {
    int i;
    int use_apic;
    int have_apic = 0;
    for (i = 0; i <= IRQ_MAX_VECTOR; i++) {
        irq[i] = IRQ_UNUSED;
    }
    /*
     * Do we have at least one APIC? If yes,
     * use it, otherwise use PIC
     */
    use_apic = params_get_int("apic");
    /*
     * See whether we have an IO APIC and fall back to 
     * PIC mode if not
     */
    if (0 == acpi_used())
        have_apic = (0 != mptables_get_primary_ioapic());
    else
        have_apic = (0 != acpi_get_primary_ioapic());
    if ((0 == have_apic) || (0 == use_apic)) {
        MSG("Setting up PIC\n");
        pic_init(IRQ_OFFSET_PIC);
        irq_mode = IRQ_MODE_PIC;
    }
    else {
        MSG("Setting up APIC\n");
        pic_init(IRQ_OFFSET_PIC);
        pic_disable();
        irq_mode = IRQ_MODE_APIC;
    }
    /*
     * Turn on debugging mode if requested
     */
    if (params_get_int("irq_log"))
        __irq_loglevel = 1;
    /*
     * Set apic mode and print a corresponding message
     */
    switch (params_get_int("apic")) {
         case 0:
             apic_mode = 0;
         case 1:
             MSG("Using physical / fixed delivery mode\n");
             apic_mode = 1;
             break;
         case 2:
             MSG("Using logical / fixed delivery mode\n");
             apic_mode = 2;
             break;
         case 3:
             MSG("Using logical / lowest priority delivery mode\n");
             apic_mode = 3;
             break;
         default:
             PANIC("Invalid value (%d) of kernel parameter apic\n");
             break;
     }
}


/*
 * Get interrupt mode
 */
int irq_get_mode() {
    return irq_mode;
}

/********************************************************
 * Everything below this line is for debugging and      *
 * testing only                                         *
 *******************************************************/


/*
 * Print IRQ statistics
 */
void irq_print_stats() {
    int i;
    int cpu;
    PRINT("CPU    Vector   IRQ   Count\n");
    PRINT("------------------------------\n");
    for (i = 0;i < 255; i++) {
        for (cpu = 0; cpu < SMP_MAX_CPU; cpu++)
            if (irq_count[cpu][i]) {
                if (IRQ_UNUSED == irq[i])
                    PRINT("%h     %h             %d\n", cpu, i, irq_count[cpu][i]);
                else
                    PRINT("%h     %h        %h   %d\n", cpu, i, irq[i], irq_count[cpu][i]);
            }
    }
}

/*
 * Print mapping of interrupts to vectors
 */
void irq_print_vectors() {
    int vector;
    PRINT("Vector       IRQ         Locked\n");
    PRINT("-------------------------------\n");
    for (vector = 0; vector <= IRQ_MAX_VECTOR; vector++) {
        if (IRQ_UNUSED != irq[vector]) {
            PRINT("%x    %x   %d\n", vector, irq[vector], irq_locked[vector]);
        }
    }
}

/*
 * Print PIR table entries
 */
void irq_print_pir_table() {
    int i;
	void* table_start=0;
	void* table_search;
	pir_table_t* pir_table = 0;
	pir_entry_t* pir_entry = 0;
	int nr_of_slots = 0;
    int linecount = 0;
	unsigned char checksum;
	/*
	 * Search area for string "$PIR" on 16-byte boundaries         
	*/
	table_search = (void*) PIR_BASE;
	while ((0==table_start) && (table_search<= (void*)(PIR_BASE+PIR_LENGTH))) {
		if (0==strncmp(table_search,"$PIR",4)) {
			table_start=table_search;
		}
		table_search+=16;
	}
    cls(0);
	if (0==table_start) {
		PRINT("Could not locate PIR table in memory\n");
		return;
	}
	PRINT("Start address of PIR is %p\n",table_start);
	pir_table=(pir_table_t*) table_start;
    checksum = 0;
    for (i=0;i<pir_table->table_size;i++)
        checksum += *(((unsigned char*) pir_table) + i);
    PRINT("Checksum (should be 0):   %x\n", checksum);
    PRINT("Checksum field:           %x\n", pir_table->checksum);
    /*
     * Now print actual contents
     */
	nr_of_slots = (pir_table->table_size-sizeof(pir_table_t)) / 16;
	PRINT("Bus         Device          PIN  Link          Slot\n");
	PRINT("----------------------------------------------------\n");
	pir_entry = (pir_entry_t*)(table_start+sizeof(pir_table_t));
	for (i=0;i<nr_of_slots;i++) {
        PRINT("%2x   %2x       A    %2x     %d\n", 
			pir_entry->bus, 
			pir_entry->device/8,
			pir_entry->inta_link_value,pir_entry->slot);
        PRINT("%2x   %2x       B    %2x     %d\n", 
			pir_entry->bus, 
			pir_entry->device/8,
			pir_entry->intb_link_value,pir_entry->slot);
        PRINT("%2x   %2x       C    %2x     %d\n", 
				pir_entry->bus, 
				pir_entry->device/8,
				pir_entry->intc_link_value,pir_entry->slot);
		PRINT("%2x   %2x       D    %2x     %d\n", 
				pir_entry->bus, 
				pir_entry->device/8,
				pir_entry->intd_link_value,pir_entry->slot);
		pir_entry++;		
        linecount+=4;
        if (linecount >=16) {
            PRINT("Hit any key to proceed to next page\n");
            early_getchar();
            cls(0);
            PRINT("Bus         Device          PIN  Link          Slot\n");
            PRINT("----------------------------------------------------\n");
            linecount = 0;
        }
	}

}
