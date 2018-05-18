/*
 * irq.h
 *
 */

#ifndef _IRQ_H_
#define _IRQ_H_

#include "lib/sys/types.h"
#include "pci.h"
#include "apic.h"

/*
 * This structure describes the layout of the stack when an interrupt handler is invoked
 * EFLAGS, the old CS and EIP are pushed by the CPU, all other registers are pushed by our interrupt handler
 * Note that the vector is really the vector inside the CPU, i.e. the index in the IDT
 */
typedef struct {
    u32 cr3;
    u32 esp;
    u32 cr2;
    u32 ds;
    u32 ebp;
    u32 edi;
    u32 esi;
    u32 edx;
    u32 ecx;
    u32 ebx;
    u32 eax;
    u32 vector;
    u32 err_code;
    u32 eip;
    u32 cs_old;
    u32 eflags;
} ir_context_t;


/*
 * This type defines an interrupt service handler
 */
typedef int (*isr_t)(ir_context_t* ir_context);

/*
 * An entry in the list of interrupt handlers which we
 * maintain per vector
 */
typedef struct _isr_handler_t {
    isr_t handler;
    struct _isr_handler_t* next;
    struct _isr_handler_t* prev;
} isr_handler_t;

/*
 * Base address at which we start to look for PIR table
 */
#define PIR_BASE    0xF0000
/*
 * Size of area to be scanned
 */
#define PIR_LENGTH      0x10000

/*
 * Interrupt modes and offsets
 */
#define IRQ_MODE_PIC 0
#define IRQ_MODE_APIC 1
#define IRQ_OFFSET_PIC 0x20
#define IRQ_OFFSET_APIC 0x30

/*
 * IRQ trigger modes and polarities
 */
#define IRQ_TRIGGER_MODE_EDGE 0
#define IRQ_TRIGGER_MODE_LEVEL 1
#define IRQ_POLARITY_ACTIVE_HIGH 0
#define IRQ_POLARITY_ACTIVE_LOW 1

/*
 * Some traps
 */
#define IRQ_TRAP_PF 0xe
#define IRQ_TRAP_NM 0x7

/*
 * Maximum supported number of interrupt vectors
 */
#define IRQ_MAX_VECTOR 255
/*
 * Dummy IRQ for "not used"
 * and "reserved for MSI"
 */
#define IRQ_UNUSED -1
#define IRQ_MSI    -2

 
/*
 * Priorities
 */
#define IRQ_PRIO_HIGHEST 1
#define IRQ_PRIO_LOWEST 5


/*
 * The following interrupt vector is reserved for entering the internal
 * debugger. This needs to be above 0x10, as the we need to send this to
 * all CPUs via an IPI in some use cases and a local APIC will not accept
 * an IPI with vectors up to 0x10
 */
#define IPI_DEBUG 0x82

/*
 * Interrupts enabled?
 */
#define IRQ_ENABLED(eflags) (((eflags & (1 << 9)) >> 9))

/*
 * Test whether a vector has been
 * raised by the PIC
 */
#define ORIGIN_PIC(vector)  (((vector>=IRQ_OFFSET_PIC) && (vector<=IRQ_OFFSET_PIC+0xf)) ? 1 : 0)


/*
 * Public interface of interrupt manager
 */
void irq_init();
u32 irq_handle_interrupt(ir_context_t context);
void irq_post();
int irq_add_handler_pci(isr_t new_isr, int priority, pci_dev_t* pci_dev);
int irq_add_handler_isa(isr_t new_isr, int priority, int _irq, int lock);
void irq_balance();
void irq_print_bus_list();
void irq_print_routing_list();
void irq_print_io_apics();
void irq_print_apic_conf();
void irq_print_stats();
void irq_print_vectors();
void irq_print_pir_table();
int irq_get_mode();

#endif /* _IRQ_H_ */
