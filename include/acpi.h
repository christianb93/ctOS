#ifndef _ACPI_H_
#define _ACPI_H_

#include "ktypes.h"
#include "apic.h"

/*
 * Specs available at http://uefi.org/sites/default/files/resources/ACPI_6_2.pdf
 */  

/*
 * The ACPI RSDP
 * see section 5.2.5.3 of the specification
 */
typedef struct {
   char signature [8];
   u8   chksum1;
   char oemid[6];
   u8   revision;
   u32  rsdtAddress;
   u32  length;
   u64  xsdtAddress;
   u8   chksum2;
} __attribute__ ((packed)) acpi_rsdp_t;

/*
 * A generic table header (called entry header
 * in the specification)
 */
typedef struct {
    char signature[4];
    u32  length;
    u8   revision;
    u8   chksum;
    char oemid[6];
    char oemTableId[8];
    u32  oemRevision;
    u32  creatorId;
    u32  creatorRevision;
} __attribute__ ((packed)) acpi_entry_header_t;

/*
 * The header of the MADT 
 */
typedef struct {
    u32 lapic_address;
    u32 apic_flags;
} __attribute__ ((packed)) acpi_madt_header_t;

/*
 * The header of the FADT
 */
typedef struct {
    u32 firmware_ctrl;
    u32 dsdt_address;
} __attribute__ ((packed)) acpi_fadt_header_t;

/*
 * Some MADT entry types
 */
 
#define MADT_ENTRY_TYPE_LOCAL_APIC 0
#define MADT_ENTRY_TYPE_IO_APIC 1
#define MADT_ENTRY_TYPE_OVERRIDE 2
#define MADT_ENTRY_LAPIC_OVERRIDE 5

/*
 * Additional entries that we do for specific motherboards which do not
 * fill the MP table completely 
 */
typedef struct  {
    char oem_id[6];
    char oem_table_id[8];
    int  oem_rev;
    char src_pin;
    char src_device;
    char src_bus_id;
    char dest_irq;
} acpi_override_t;


/*
 * A local APIC entry
 */
typedef struct {
    u16 unused;     // type and length of the generic entry
    u8 acpi_cpu_id;
    u8 local_apic_id;
    u32 local_apic_flags;
} __attribute__ ((packed)) acpi_lapic_t;

#define ACPI_MADT_LAPIC_FLAGS_ENABLED 0x1

/*
 * An I/O APIC entry
 */
typedef struct {
    u16 unused;     // type and length of the generic entry
    u8 io_apic_id;
    u8 reserved;
    u32 io_apic_address;
    u32 gsi_base;
} __attribute__ ((packed)) acpi_io_apic_t;

/*
 * An MADT ISA IRQ override entry
 */
typedef struct {
    u16 unused;     // type and length of the generic entry
    u8  bus;
    u8  src_irq;
    u32 gsi;
    u16 flags;
} __attribute__ ((packed)) acpi_irq_override_t;
 

/*
 * An ISA IRQ routing as we store it internally
 */
typedef struct {
    u32 src_irq;
    u32 io_apic_input;
    int gsi;
    int polarity;
    int trigger;
} isa_irq_routing_t;

int acpi_parse();
void acpi_init();
int acpi_used();
int acpi_get_apic_pin_isa(int irq);   
int acpi_get_irq_pin_pci(int bus_id, int device,  char irq_pin);
int acpi_get_trigger_polarity(int irq, int* polarity, int* trigger_mode);
io_apic_t*  acpi_get_primary_ioapic();
 
void acpi_init_late();
void acpi_print_info();
void acpi_print_madt();

#endif
