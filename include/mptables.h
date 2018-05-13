/*
 * mptables.h
 *
 */

#ifndef _MPTABLES_H_
#define _MPTABLES_H_

#include "lib/sys/types.h"
#include "pci.h"
#include "apic.h"

/*
 * The PIR table header
 */
typedef struct {
	u32 signature;	
	u16 version;		
	u16 table_size;	/* total size of table */
	u8 bus;		
	u8 devfunc;	
	u16 pci_irqs;	
	u32 compatible;	
	u32 miniport_data;	
	u8 reserved[11]; 
	u8 checksum;
} __attribute__ ((packed)) pir_table_t;

/*
 * An entry in the PIR table
 */
typedef struct {
	u8	bus;		
	u8  device;	 	
	u8	inta_link_value;	
	u16	inta_irqs_allowed;	
	u8 intb_link_value;	
	u16 intb_irqs_allowed;	
	u8 intc_link_value;	
	u16 intc_irqs_allowed;	
	u8 intd_link_value;	
	u16 intd_irqs_allowed;	
	u8 slot;		
	u8 reserved;	
} __attribute__ ((packed)) pir_entry_t;


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
    u32 local_apic_address;     // address at which local APIC can be seen in memory
}__attribute__ ((packed)) mp_table_header_t;

/*
 * Entry in MP table describing a bus
 */
typedef struct {
    u8 entry_type;              
    u8 bus_id;                  // unique ID of the bus
    char bus_type[6];           // bus type - ISA, PCI, ..
}__attribute__ ((packed)) mp_table_bus_t;


/*
 * Entry in MP table describing an I/O APIC
 */
typedef struct {
    char entry_type;
    char io_apic_id;            // unique ID of the APIC
    char io_apic_version;       // Bits 0 - 7 of the APIC version register
    char io_apic_flags;         // Bit 0: usable?
    u32 io_apic_address;        // Base address for this APIC
}__attribute__ ((packed)) mp_table_io_apic_t;

/*
 * Entry in MP table describing an interrupt routing
 */
typedef struct {
    char entry_type;
    char irq_type;              // 0 = vectored interrupt, 1 = NMI, 2 = SMI, 3 = external interrupt
    u16 irq_flags;              // Bits 0, 1: polarity, bits 3, 4: trigger mode
    char src_bus_id;            // ID of the source bus
    char src_bus_irq;           // source interrupt (for PCI: bits 0,1 are pin, remaining bits are device)
    char dest_apic_id;          // ID of destination APIC
    char dest_irq;              // input line of destination APIC
}__attribute__ ((packed)) mp_table_irq_t;

/*
 * Entry in MP table describing a local interrupt
 * routing
 */
typedef struct {
    char entry_type;
    char irq_type;
    u16 irq_flags;
    char src_bus_id;
    char src_bus_irq;
    char dest_apic_id;
    char dest_pin;
}__attribute__ ((packed)) mp_table_local_t;

/*
 * Entry describing a CPU
 */
typedef struct {
    char entry_type;
    char local_apic_id;         // ID of local APIC for this CPU
    char local_apic_version;    // its version
    char cpu_flags;             // Bit 2: is this the BSP?
    u32 cpu_signature;          // CPU signature
    u32 feature_flags;          // CPU feature flags
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
 * Additional entries that we do for specific motherboards which do not
 * fill the MP table completely 
 */
typedef struct _irq_forced_entry_t {
    char oem_id[8];
    char product_id[12];
    char src_pin;
    char src_device;
    char src_bus_id;
    char dest_irq;
} irq_forced_entry_t;


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
#define MP_TABLE_ENTRY_TYPE_LOCAL 4

/*
 * IRQ trigger modes and polarities
 * as stored in the I/O APIC redirection table
 */
#define IRQ_TRIGGER_MODE_EDGE 0
#define IRQ_TRIGGER_MODE_LEVEL 1
#define IRQ_POLARITY_ACTIVE_HIGH 0
#define IRQ_POLARITY_ACTIVE_LOW 1


/*
 * Public interface of the MP tables module
 */
void mptables_init();
int mptables_get_trigger_polarity(int irq_line, int* polarity, int* trigger_mode);
int mptables_get_irq_pin_pci(int bus, int device, int pin);
int mptables_get_apic_pin_isa(int irq);
void mptables_print_bus_list();
void mptables_print_routing_list();
void mptables_print_io_apics();
void mptables_print_apic_conf();
void mptables_print_pir_table();
io_apic_t* mptables_get_primary_ioapic();

#endif 