/*
 * test_debug.c
 * Test harness for the debugging code
 * and needed stubs
 */
#include <stdio.h>
#include "lib/sys/types.h"
#include "debug.h"
#include "irq.h"
#include "pagetables.h"
#include "locks.h"
#include "vga.h"


extern void (*debug_getline) (char* , int);

int __mm_log;

int ticks[256];

/*
 * Dummy for early getchar - this function needs to be present
 * to make the linker happy but is not actually used
 */
u8 early_getchar() {
    return 0;
}

void lapic_print_configuration() {

}

void acpi_print_madt() {
    
}

void net_if_print() {

}

void multiboot_print_info() {
    
}

void acpi_print_info() {
    
}

int mm_validate() {
    return 0;
}

void timer_print_timers() {

}

void timer_print_cpu_ticks() {

}

void mptables_print_pir_table() {
}

void cpuid(u32 eax, u32* ebx, u32* ecx, u32* edx) {

}


void save_eflags(u32* eflags) {

}

void restore_eflags(u32* eflags) {

}

void ip_print_routing_table() {

}

void cls(win_t* win) {

}
u32 get_gs() {
    return 0;
}

void cli() {

}
void sti() {

}

void pm_print_timers() {

}

void rdmsr(u32 msr, u32* low, u32* high) {

}

/*
 * Dummy for reboot
 */
void reboot() {
}

int apic_send_ipi_others(u8 ipi, u8 vector) {
    return 0;
}

int apic_send_ipi(u8 apic_id, u8 ipi, u8 vector, int deassert) {
    return 0;
}

u32 get_cr3() {
    return 0;
}

int smp_get_cpu() {
    return 0;
}

void spinlock_get(spinlock_t* lock, u32* flags) {
}
void spinlock_release(spinlock_t* lock, u32* flags) {
}
void spinlock_init(spinlock_t* lock) {
}

void vga_debug_regs() {
}

void pata_print_queue() {

}


void ahci_print_queue() {

}
/*
 * Dummy for task switch
 */
void sched_override(int task) {
}

/*
 * Dummy for pm functions
 */
void pm_print_task_table() {
}

int pm_get_task_id() {
    return 0;
}
int pm_get_pid() {
    return 0;
}

void sched_print() {

}

void mm_print_stack_allocators() {

}

int mm_page_mapped(u32 page) {
    return 1;
}

void mm_print_vmem() {

}
void mm_print_pmem() {

}

void pci_list_devices() {

}
void pata_print_devices() {

}

u32 kmalloc(u32 size) {
    return (u32) malloc(size);
}

void mptables_print_bus_list() {

}
u8 rtc_read_register(u8 index) {
    return 0;
}

void mptables_print_routing_list() {

}
void mptables_print_io_apics() {

}
void mptables_print_apic_conf() {

}

void irq_print_stats() {

}

int fs_print_open_files() {
    return 0;
}
void ahci_print_ports() {

}

/*
 * Buffer used for testing memory dump
 */
static char mymem[32];

/*
 * Stub
 */
void win_putchar(win_t* win, u8 c) {
    putchar(c);
}

/*
 * Dummy for paging toggle functions
 */
void enable_paging() {
}

void disable_paging() {
}

/*
 * Stub for keyboard read function
 */
void mygetline(char* buffer, int max) {
    fgets(buffer, max-1, stdin);
}

/*
 * Stub for access to CR0
 */
u32 get_cr0() {
    return 0xffffffff;
}



void test_kprintf() {
    kprintf("%s", "This is a string\n");
    kprintf("%d decimal = %x hex\n", 0xffff, 0xffff);
}

void test_debug_main() {
    ir_context_t ir_context;
    debug_main(&ir_context);
}

int main() {
    int i;
    test_kprintf();
    for (i=0;i<32;i++) {
        mymem[i]=i;
    }
    printf("Address of 32 byte memory test segment is %p\n",mymem);
    debug_getline = &mygetline;
    test_debug_main();
    return 0;
}
