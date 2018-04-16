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
 * The following typedefs describe
 * the structure of the MP tables
 */
typedef struct {
    char signature[4];
    u32 mp_table_ptr;
    u8 length;
    u8 spec_rev;
    u8 checksum;
    u8 info_byte1;
    u8 info_byte2;
}__attribute__ ((packed)) mp_fps_table_t;

/*
 * Header of MP table
 */
typedef struct {
    char signature[4];
    u16 base_table_length;
    u8 spec_rev;
    u8 checksum;
    char oem_id[8];
    char product_id[12];
    u32 oem_table_ptr;
    u16 oem_table_size;
    u16 entry_count;
    u32 local_apic_address;
}__attribute__ ((packed)) mp_table_header_t;

/*
 * Entry in MP table describing a bus
 */
typedef struct {
    u8 entry_type;
    u8 bus_id;
    char bus_type[6];
}__attribute__ ((packed)) mp_table_bus_t;


/*
 * Entry in MP table describing an I/O APIC
 */
typedef struct {
    char entry_type;
    char io_apic_id;
    char io_apic_version;
    char io_apic_flags;
    u32 io_apic_address;
}__attribute__ ((packed)) mp_table_io_apic_t;

/*
 * Entry in MP table describing an interrupt routing
 */
typedef struct {
    char entry_type;
    char irq_type;
    u16 irq_flags;
    char src_bus_id;
    char src_bus_irq;
    char dest_apic_id;
    char dest_irq;
}__attribute__ ((packed)) mp_table_irq_t;

/*
 * Entry describing a CPU
 */
typedef struct {
    char entry_type;
    char local_apic_id;
    char local_apic_version;
    char cpu_flags;
    u32 cpu_signature;
    u32 feature_flags;
    u32 reserved[2];
}__attribute__ ((packed)) mp_table_cpu_t;

/*
 * This is an entry in our internal table of busses
 */
typedef struct _bus_t {
    char bus_id;
    char bus_type[6];
    int is_pci;
    struct _bus_t* next;
    struct _bus_t* prev;
} bus_t;


/*
 * This is an entry in the list of IRQ routings
 * which we maintain
 */
typedef struct _irq_routing_t {
    bus_t* src_bus;
    char src_irq;              // source irq for ISA irq, 0xFF for PCI
    char src_device;           // source device for PCI irq, 0 for ISA
    char src_pin;              // source pin for PCI irq, ' ' for ISA or 'A',...
    char dest_irq;             // Pin of IO APIC to which we are connected
    int polarity;
    int trigger;
    int type;
    int effective_polarity;
    int effective_trigger;
    struct _irq_routing_t* next;
    struct _irq_routing_t* prev;
} irq_routing_t;


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
 * Types of entries found in the table
 */
#define MP_TABLE_ENTRY_TYPE_CPU 0
#define MP_TABLE_ENTRY_TYPE_BUS 1
#define MP_TABLE_ENTRY_TYPE_APIC 2
#define MP_TABLE_ENTRY_TYPE_ROUTING 3

/*
 * IRQ trigger modes and polarities
 * as stored in the I/O APIC redirection table
 */
#define IRQ_TRIGGER_MODE_EDGE 0
#define IRQ_TRIGGER_MODE_LEVEL 1
#define IRQ_POLARITY_ACTIVE_HIGH 0
#define IRQ_POLARITY_ACTIVE_LOW 1

/*
 * Interrupt modes and offsets
 */
#define IRQ_MODE_PIC 0
#define IRQ_MODE_APIC 1
#define IRQ_OFFSET_PIC 0x20

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
 */
#define IRQ_UNUSED -1

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
int irq_get_mode();

#endif /* _IRQ_H_ */
