/*
 * test_irq.c
 *
 * Testcases for the interrupt manager
 */

#include "kunit.h"
#include "irq.h"
#include "mptables.h"
#include "vga.h"
#include "locks.h"
#include <stdio.h>


/*
 * Stubs for some standard functions
 */
static int do_putchar = 0;
void win_putchar(win_t* win, u8 c) {
    if (do_putchar)
        printf("%c", c);
}
void cond_reschedule() {

}

void cls(win_t* win) {
}

void save_eflags(u32* eflags) {

}

void restore_eflags(u32* eflags) {

}

void cpuid(u32 eax, u32* ebx, u32* ecx, u32* edx) {

}

void pm_handle_nm_trap() {

}


int debug_running() {
    return 0;
}

void debug_getline(void* c, int n) {

}
u32 get_gs() {
    return 0;
}

void spinlock_get(spinlock_t* lock, u32* flags) {
}
void spinlock_release(spinlock_t* lock, u32* flags) {
}
void spinlock_init(spinlock_t* lock) {
}

void rdmsr(u32 msr, u32* low, u32* high) {

}
int __force_int3 = 0;

void cli() {
}

void sti() {

}

int mm_handle_page_fault(ir_context_t* ir_context) {
    return 0;
}

int smp_get_cpu() {
    return 0;
}

u32 mm_get_top_of_common_stack() {
    return 0x1000;
}

int pm_handle_exit_requests() {
    return 0;
}

int mm_is_kernel_code(u32 code_segment) {
    return ((code_segment / 8) == (32 / 8));
}

int pm_process_signals(ir_context_t* context) {
    return 0;
}

void debug_main(ir_context_t* context) {

}

u8 early_getchar() {
    return 'x';
}

int params_get_int(char* name) {
    return 0;
}

int syscall_dispatch(ir_context_t* ir_context) {
    return 0;
}

void pit_init() {

}

int pm_update_exec_level(ir_context_t* ir_context, int* old_level) {
    *old_level = 1;
    return 3;
}

void pm_restore_exec_level(int old_level) {

}

/*
 * Stub for memory manager
 */
u32 kmalloc(size_t size) {
    return malloc(size);
}
void kfree(u32 addr) {
    free((void*) addr);
}

u32 mm_map_memio(u32 phys) {
    return 0;
}

/*
 * Stub for trap
 */
void trap() {

}

/*
 * Stubs for PIC and APIC
 */

void apic_add_redir_entry(io_apic_t* io_apic, int irq, int polarity,
        int trigger, int vector,  int apic_mode) {

}

void apic_eoi(u32 vector, u32 vector_base) {

}

void apic_init_bsp(u32 phys_base) {

}

void apic_print_configuration(io_apic_t* io_apic) {

}

void pic_disable() {

}


void pic_eoi() {

}

void pic_init() {

}

int acpi_used() {
    return 0;
}

int acpi_get_apic_pin_isa(int i) {
    return IRQ_UNUSED;
}

int acpi_get_trigger_polarity(int irq, int* trigger, int* polarity) {
    *trigger = IRQ_TRIGGER_MODE_EDGE;
    *polarity = IRQ_POLARITY_ACTIVE_HIGH;
    return 1;
}

io_apic_t* acpi_get_primary_ioapic() {
    return 0;
}

/*
 * Stubs for process manager and scheduler
 */

void pm_cleanup_task() {

}

void pm_do_tick() {

}

void pm_switch_task(u32 task_id) {

}

void sched_do_tick() {

}

u32 sched_schedule() {
    return 0;

}

/*
 * Stub for the function which is used to scan for the MP table
 * Currently we just return 0 here to force the IRQ manager into PIC mode
 */
extern mp_table_header_t* (*mp_table_scan)();
static mp_table_header_t* mp_table_scan_stub() {
    return 0;
}

/*
 * Our test handler
 */
static int dummy_irq_handler1_calls = 0;
int dummy_irq_handler1(ir_context_t* context) {
    dummy_irq_handler1_calls++;
    return 0;
}
static int dummy_irq_handler2_calls = 0;
int dummy_irq_handler2(ir_context_t* context) {
    dummy_irq_handler2_calls++;
    return 0;
}

/*
 * Testcase 1
 * Tested function: irq_add_handler
 * Test case: register a handler and invoke the respective interrupt. Make sure that the handler is
 * called
 */
int testcase1() {
    ir_context_t context;
    mp_table_scan = mp_table_scan_stub;
    irq_init();
    context.vector = 0x40;
    context.vector = irq_add_handler_isa(dummy_irq_handler1, 1, 0x40, 0);
    dummy_irq_handler1_calls = 0;
    irq_handle_interrupt(context);
    ASSERT(1==dummy_irq_handler1_calls);
    ASSERT(0==dummy_irq_handler2_calls);
    return 0;
}

/*
 * Testcase 2
 * Tested function: irq_add_handler
 * Test case: register a handler and invoke a different interrupt. Make sure that the handler is
 * not called
 */
int testcase2() {
    ir_context_t context;
    mp_table_scan = mp_table_scan_stub;
    irq_init();
    context.vector = 0x41;
    context.vector = irq_add_handler_isa(dummy_irq_handler1, 1, 0x40, 0) + 1;
    dummy_irq_handler1_calls = 0;
    irq_handle_interrupt(context);
    ASSERT(0==dummy_irq_handler1_calls);
    ASSERT(0==dummy_irq_handler2_calls);
    return 0;
}

/*
 * Testcase 3
 * Tested function: irq_add_handler
 * Test case: register two handlers and invoke the interrupt. Make sure that
 * both handlers are called
 */
int testcase3() {
    ir_context_t context;
    mp_table_scan = mp_table_scan_stub;
    irq_init();
    context.vector = irq_add_handler_isa(dummy_irq_handler1, 1, 0x40, 0);
    irq_add_handler_isa(dummy_irq_handler2, 1, 0x40, 0);
    dummy_irq_handler1_calls = 0;
    dummy_irq_handler2_calls = 0;
    irq_handle_interrupt(context);
    ASSERT(1==dummy_irq_handler1_calls);
    ASSERT(1==dummy_irq_handler2_calls);
    return 0;
}


int main() {
    INIT;
    RUN_CASE(1);
    RUN_CASE(2);
    RUN_CASE(3);
    END;
}
