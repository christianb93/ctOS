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
 * processing such a request, the interrupt manager will use the MP specification table to locate the I/O APIC pin connected to this
 * device. It then determines whether a vector has already been assigned to this IRQ. If yes, the handler is added to the list of handlers
 * for this previously assigned vector.
 *
 * If no vector has been assigned yet, a new vector is chosen. For this purpose, the table of vectors is scanned from the top to the
 * bottom starting at the specified priority until a free entry is found. Then the newly established mapping is added to an internal
 * table and a new entry is added to the I/O APIC redirection table.
 *
 * In PIC mode, the handling is similar, but the mapping of interrupts to vectors is fixed and determined by the hardware.
 *
 * Interrupt routing:
 *
 * If an I/O APIC is present, ctOS supports three different mechanisms to route interrupts to CPUs which can be chosen
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

#include "debug.h"
#include "lists.h"
#include "lib/string.h"
#include "mm.h"
#include "cpu.h"
#include "keyboard.h"
#include "mptables.h"



extern void (*debug_getline)(void* c, int n);


static char* __module = "MPT   ";

/*
 * This is a linked list of all busses found
 * in the system while scanning the MP table
 */
static bus_t* bus_list_head = 0;
static bus_t* bus_list_tail = 0;
/*
 * Is the first bus (bus ID 0) an ISA bus?
 */
static int first_bus_is_isa = 0;

/*
 * A list of all I/O APICs in the system
 */
static io_apic_t* io_apic_list_head = 0;
static io_apic_t* io_apic_list_tail = 0;

/*
 * A list of detected IRQ routings
 */
static irq_routing_t* routing_list_head = 0;
static irq_routing_t* routing_list_tail = 0;


/*
 * Additional entries for specific boards. This is a workaround for the fact
 * that currenty, we do not have an ACPI implementation yet. Values should be taken from 
 * the DSL of the target system. Example for Virtualbox, taken from vbox.dsl:
 * - device is 00:31 (SATA drive)
 * - package returned by method _PRT of device PCI0 is Package (0x04) {0x001FFFFF, 0x00, 0x00, 0x17,},
 *                                                                        A                       A
 *                                                                        |                       |
 *                                                                        |                       |
 *                                                                        |                       |
 *                                                                    device 0x1F               IRQ 17
 * - first package is for IRQ pin A, second for IRQ pin B etc.
 */
static irq_forced_entry_t forced_irq_routings[] = {
    {"VBOXCPU ","VirtualBox  ", 'A', 31, 1, 0x17},
};

/****************************************************************************************
 * At boot time, this module parses the MP BIOS tables and stores the         *
 * information contained in them for later use. This is done by the following functions *
 ***************************************************************************************/

/*
 * Internal utility function to scan the first MB of physical
 * memory for the MP table. This function returns a pointer
 * to the header of the MP table or 0 if no MP table could be found
 * To support unit tests, this is implemented as a function pointer
 */
static mp_table_header_t* mp_table_scan_impl() {
    void* mp_area;
    void* top_ptr;
    void* table_search;
    void* table_start;
    u16 ebda_segment;
    void* ebda_ptr;
    u16 mem_size;
    mp_fps_table_t* mp_fps_table;
    /*
     * Locate MP floating pointer structure
     * First we search the BIOS area between 0xF0000 and 0xFFFFF
     * If we do not find the table there, we scan the first kB of
     * the extended bios data area (EBDA) next
     * Finally we scan the upper part of lower memory below 640k
     * According to the multiboot specification, the table starts
     * with the string _MP_
     */
    mp_area = (void*) 0xF0000;
    table_search = mp_area;
    table_start = 0;
    while ((0 == table_start) && (table_search < mp_area + 0x10000)) {
        if (0 == strncmp(table_search, "_MP_", 4)) {
            table_start = table_search;
        }
        table_search += 16;
    }
    /*
     * Proceed to EBDA segment
     * The EBDA segment is stored in BIOS data area at 0x40e
     */
    ebda_segment = (*((u16*) 0x40e));
    /*
     * As this is the segment in real mode x86 terminology, multiply by 16
     * to get physical address
     */
    ebda_ptr = (void*) (((unsigned int) (ebda_segment)) * 16);
    /*
     * Scan first kilobyte EBDA area if it exists and we have not found the table yet
     */
    if (ebda_segment && (0 == table_start)) {
        table_search = ebda_ptr;
        while ((0 == table_start) && (table_search <= ebda_ptr + 1024)) {
            if (0 == strncmp(table_search, "_MP_", 4)) {
                table_start = table_search;
            }
            table_search += 16;
        }
    }
    /*
     * If table has not been found yet, scan upper part of memory,
     * i.e. last kilobyte of system base memory below 640K
     */
    if (0 == table_start) {
        /*
         * Size of lower memory in kB is stored at 0x413 in BIOS data area
         */
        mem_size = *((u16*) 0x413);
        mem_size--;
        top_ptr = (void*) (((int) mem_size) * 1024);
        table_search = top_ptr;
        while ((0 == table_start) && (table_search < top_ptr + 1024)) {
            if (0 == strncmp(table_search, "_MP_", 4)) {
                table_start = table_search;
            }
            table_search += 16;
        }
    }
    if (0 == table_start) {
        return 0;
    }
    mp_fps_table = (mp_fps_table_t*) table_start;
    if (0 == mp_fps_table->mp_table_ptr) {
        return 0;
    }
    return (mp_table_header_t*) (mp_fps_table->mp_table_ptr);
}
mp_table_header_t* (*mp_table_scan)() = mp_table_scan_impl;


/*
 * Process a bus entry in the MP table, i.e. parse it and
 * add it to the internal list
 * Parameter:
 * @mp_table_entry_ptr - pointer to the table entry
 */
static void mp_table_process_bus(void* mp_table_entry_ptr) {
    mp_table_bus_t* mp_table_bus = (mp_table_bus_t*) mp_table_entry_ptr;
    bus_t* bus;
    bus = (bus_t*) kmalloc(sizeof(bus_t));
    KASSERT(bus);
    bus->bus_id = mp_table_bus->bus_id;
    strncpy(bus->bus_type, mp_table_bus->bus_type, 6);
    bus->is_pci = 0;
    if (0 == strncmp(bus->bus_type, "PCI   ", 6)) {
        bus->is_pci = 1;
    }
    else {
        if (0 == bus->bus_id)
            first_bus_is_isa = 1;
    }
    LIST_ADD_END(bus_list_head, bus_list_tail, bus);
}

/*
 * This function returns a bus from the list of
 * known busses given its id
 * Parameter:
 * @bus_id - the id of the bus to scan
 * Return value:
 * a pointer to the bus structure or 0 if the id
 * could not be resolved
 */
static bus_t* get_bus_for_id(u32 bus_id) {
    bus_t* bus;
    LIST_FOREACH(bus_list_head, bus) {
        if (bus->bus_id == bus_id)
            return bus;
    }
    return 0;
}

/*
 * Return trigger as needed for the redirection table entries
 * 0 = Edge triggered
 * 1 = Level triggered
 * Parameters:
 * @irq_routing - the routing entry
 */
static int get_trigger(irq_routing_t* irq_routing) {
    /*
     * 0 means as defined for the respective bus
     */
    if (0 == irq_routing->trigger) {
        if (1 == irq_routing->src_bus->is_pci) {
            /*
             * PCI is level triggered
             */
            return IRQ_TRIGGER_MODE_LEVEL;
        }
        /*
         * ISA is edge triggered. Ignore EISA and MCA...
         * I do not have a machine to test this anyway
         */
        return IRQ_TRIGGER_MODE_EDGE;
    }
    /*
     * 0x1 = edge triggered
     */
    else if (1 == irq_routing->trigger) {
        return IRQ_TRIGGER_MODE_EDGE;
    }
    else if (3 == irq_routing->trigger) {
        return IRQ_TRIGGER_MODE_LEVEL;
    }
    else {
        ERROR("Unknown trigger mode\n");
        return 0;
    }
}

/*
 * Return polarity as needed for the redirection table entries
 * 0 = Active high
 * 1 = Active low
 * Parameters:
 * @irq_routing - the routing entry
 */
static int get_polarity(irq_routing_t* irq_routing) {
    /*
     * 0 means as "defined for the respective bus"
     */
    if (0 == irq_routing->polarity) {
        if (1 == irq_routing->src_bus->is_pci) {
            /*
             * PCI is active low
             */
            return IRQ_POLARITY_ACTIVE_LOW;
        }
        /*
         * ISA is active high. Again we tacitly ignore EISA and MCA..
         */
        return IRQ_POLARITY_ACTIVE_HIGH;
    }
    /*
     * 0x1 = Active high
     */
    else if (1 == irq_routing->polarity) {
        return IRQ_POLARITY_ACTIVE_HIGH;
    }
    else if (3 == irq_routing->polarity) {
        return IRQ_POLARITY_ACTIVE_LOW;
    }
    else {
        ERROR("Unknown polarity\n");
        return 0;
    }
}

/*
 * Process a routing entry in the MP table
 * Parameters:
 * @mp_table_entry_ptr - pointer to the table entry
 */
static void mp_table_process_routing(void* mp_table_entry) {
    mp_table_irq_t* mp_table_irq = (mp_table_irq_t*) mp_table_entry;
    irq_routing_t* irq_routing_ptr = kmalloc(sizeof(irq_routing_t));
    int is_pci;
    KASSERT(irq_routing_ptr);
    /*
     * Fill fields of our entry
     */
    irq_routing_ptr->src_bus = get_bus_for_id(mp_table_irq->src_bus_id);
    irq_routing_ptr->dest_irq = mp_table_irq->dest_irq;
    /*
     * Polarity is in bits 0 - 1 of flags, trigger mode in bits 2 - 3
     */
    irq_routing_ptr->polarity = (mp_table_irq->irq_flags) & 0x3;
    irq_routing_ptr->trigger = (mp_table_irq->irq_flags & 0xc) / 4;
    irq_routing_ptr->type = (mp_table_irq->irq_type);
    /*
     * We store the polarity and trigger mode in the decoding required for the I/O APIC
     * redirection entries in the fields effective_xxx
     */
    irq_routing_ptr->effective_polarity = get_polarity(irq_routing_ptr);
    irq_routing_ptr->effective_trigger = get_trigger(irq_routing_ptr);
    if (0 == irq_routing_ptr->src_bus) {
        is_pci = 0;
    }
    else {
        is_pci = irq_routing_ptr->src_bus->is_pci;
    }
    if (0 == is_pci) {
        irq_routing_ptr->src_irq = mp_table_irq->src_bus_irq;
        irq_routing_ptr->src_pin = ' ';
        irq_routing_ptr->src_device = 0;
    }
    else {
        irq_routing_ptr->src_irq = 0xff;
        irq_routing_ptr->src_device = ((mp_table_irq->src_bus_irq) & 0x7f) / 4;
        irq_routing_ptr->src_pin = (mp_table_irq->src_bus_irq & 0x3) + 'A';
    }
    DEBUG("Found routing table entry: bus_id = %x, src_device = %d, src_pin = %d, src_irq = %x\n", irq_routing_ptr->src_bus->bus_id, irq_routing_ptr->src_device, irq_routing_ptr->src_pin, irq_routing_ptr->src_irq);
    
    LIST_ADD_END(routing_list_head, routing_list_tail, irq_routing_ptr);
}

/*
 * Process a local interrupt entry in the MP table
 * Parameters:
 * @mp_table_entry_ptr - pointer to the table entry
 */
static void mp_table_process_local(void* mp_table_entry) {
    mp_table_local_t* mp_table_local = (mp_table_local_t*) mp_table_entry;
    DEBUG("Found local assignment entry, source bus id = %d\n", mp_table_local->src_bus_id);
}

/*
 * This function will scan the MP table and build a list of
 * IRQ routings out of the entries it finds.
 * Parameters:
 * @mp_table_header - pointer to header of MP table
 */
static void mp_table_build_routing_list(mp_table_header_t* mp_table_header, char* oem_id, char* product_id) {
    u32 mp_table_entry_max;
    void* mp_table_entry_ptr;
    int mp_table_entry_count = 0;
    u8 mp_table_entry_type;
    irq_forced_entry_t* forced;
    int i;
    /*
     * We should have at most mp_table_header->entry_count entries
     */
    mp_table_entry_max = mp_table_header->entry_count;
    /*
     * The first entry starts after header at offset 44
     */
    mp_table_entry_ptr = ((void*) mp_table_header) + 44;
    while ((mp_table_entry_count < mp_table_entry_max)) {
        mp_table_entry_type = *((unsigned char*) mp_table_entry_ptr);
        DEBUG("Processing entry %d of type %d\n", mp_table_entry_count, mp_table_entry_type);
        /*
         * Process entry
         */
        if (MP_TABLE_ENTRY_TYPE_ROUTING == mp_table_entry_type) {
            
            mp_table_process_routing(mp_table_entry_ptr);
        }
        if (MP_TABLE_ENTRY_TYPE_LOCAL == mp_table_entry_type) {
            
            mp_table_process_local(mp_table_entry_ptr);
        }
        /*
         * Advance to next entry
         */
        if (0 == mp_table_entry_type) {
            mp_table_entry_ptr += 20;
        }
        else {
            mp_table_entry_ptr += 8;
        }
        mp_table_entry_count += 1;
    }
    /*
     * Now see whether we have to add any additional entries for known 
     * motherboards that do not create a full MP configuration table any
     * more
     */
    for (i = 0; i < sizeof(forced_irq_routings) / sizeof(irq_forced_entry_t); i++) {
        forced = forced_irq_routings + i;
        if ((0 == strncmp(oem_id, forced->oem_id,8)) && (0 == strncmp(product_id,forced->product_id, 12))) {
            MSG("Applying MP table workaround for %s / %s\n", oem_id, product_id);
            irq_routing_t* irq_routing = kmalloc(sizeof(irq_routing_t));
            irq_routing->src_bus = get_bus_for_id(forced->src_bus_id);
            irq_routing->src_irq = 0xFF;
            irq_routing->src_device = forced->src_device;
            irq_routing->src_pin = forced->src_pin;
            irq_routing->dest_irq = forced->dest_irq;
            irq_routing->polarity = 1;
            irq_routing->trigger = 0;
            irq_routing->type = 0;
            irq_routing->effective_polarity = 0;
            irq_routing->effective_trigger = 1;
            LIST_ADD_END(routing_list_head, routing_list_tail, irq_routing);
        }
    }
}


/*
 * Parse an entry in the MP table describing an I/O APIC and add the
 * entry to our internal list of I/O APICs. At this point, we also
 * map the I/O APICs memory registers into our virtual address space
 * Parameters:
 * @mp_table_entry_ptr - MP table entry
 */
static void mp_table_process_apic(void* mp_table_entry_ptr) {
    mp_table_io_apic_t* mp_table_io_apic =
            (mp_table_io_apic_t*) mp_table_entry_ptr;
    io_apic_t* io_apic = (io_apic_t*) kmalloc(sizeof(io_apic_t));
    KASSERT(io_apic);
    io_apic->apic_id = mp_table_io_apic->io_apic_id;
    io_apic->base_address = mm_map_memio(mp_table_io_apic->io_apic_address, 14);
    KASSERT(io_apic->base_address);
    LIST_ADD_END(io_apic_list_head, io_apic_list_tail, io_apic);
}

/*
 * This function will scan the MP table and build a list of
 * busses and connected I/O APICs out of the entries it finds.
 * In addition, it will handle entries describing a CPU to find
 * the local APIC ID of the BSP
 * Parameters:
 * @mp_table_header - pointer to header of MP table
 */
static void mp_table_build_bus_list(mp_table_header_t* mp_table_header) {
    u32 mp_table_entry_max;
    void* mp_table_entry_ptr;
    int mp_table_entry_count = 0;
    u8 mp_table_entry_type;
    mp_table_cpu_t* cpu_entry;
    /*
     * We should have at most mp_table_header->entry_count entries
     */
    mp_table_entry_max = mp_table_header->entry_count;
    /*
     * The first entry starts after header at offset 44
     */
    mp_table_entry_ptr = ((void*) mp_table_header) + 44;
    while ((mp_table_entry_count < mp_table_entry_max)) {
        mp_table_entry_type = *((unsigned char*) mp_table_entry_ptr);
        /*
         * Process entry
         */
        if (MP_TABLE_ENTRY_TYPE_BUS == mp_table_entry_type) {
            mp_table_process_bus(mp_table_entry_ptr);
        }
        if (MP_TABLE_ENTRY_TYPE_APIC == mp_table_entry_type) {
            mp_table_process_apic(mp_table_entry_ptr);
        }
        if (MP_TABLE_ENTRY_TYPE_CPU == mp_table_entry_type) {
            cpu_entry = (mp_table_cpu_t*) mp_table_entry_ptr;
            /*
             * CPU entries are ignored if the enable flag (bit 0 in CPU flags field)
             * is clear
             */
            if (cpu_entry->cpu_flags & 0x1) {
                /*
                 * Check BSP flag
                 */
                if ((cpu_entry->cpu_flags) & 0x2) {
                    cpu_add(cpu_entry->local_apic_id, 1, cpu_entry->local_apic_version);
                }
                else
                    cpu_add(cpu_entry->local_apic_id, 0, cpu_entry->local_apic_version);
            }
            else {
                MSG("Found disabled CPU in MP configuration table\n");
            }
        }
        /*
         * Advance to next entry
         */
        if (0 == mp_table_entry_type) {
            mp_table_entry_ptr += 20;
        }
        else {
            mp_table_entry_ptr += 8;
        }
        mp_table_entry_count += 1;
        /*
         * On some systems, the table is broken
         * - bail out if we have more than 4096 entries
         */
        if (mp_table_entry_count > 4096) {
            PANIC("MP table has more than 4096 entries - this can't be right!!!\n");
            return;
        }
    }
}

/*
 * Read the MP tables from memory and store the relevant contents
 * in our internal data structures. Also initialize local APIC
 * based on that information
 */
void mptables_init() {
    char oem_id[16];
    char product_id[16];
    mp_table_header_t* mp_table;
    mp_table = mp_table_scan();
    if (0 == mp_table) {
        DEBUG("Could not locate MP table\n");
        return;
    }
    DEBUG("Found MP table at address %x\n", mp_table);
    strncpy(oem_id, mp_table->oem_id, 8);
    oem_id[8]=0;
    strncpy(product_id, mp_table->product_id, 12);
    product_id[12]=0;
    DEBUG("OEM ID: >%s<, PRODUCT_ID: >%s<\n", oem_id, product_id);
    mp_table_build_bus_list(mp_table);
    mp_table_build_routing_list(mp_table, oem_id, product_id);
    /*
     * We use the local APIC address from the MP table
     * to set up the local APIC and map its address
     * range into physical memory
     */
    apic_init_bsp(mp_table->local_apic_address);
}

/*
 * Given an IRQ line, return polarity and trigger mode of
 * the first entry of type 0 (vectored interrupt) in the
 * MP table for this line.
 * The values returned have the semantics as used in the
 * APIC redirection table entries
 * Parameters:
 * @irq_line - the APIC pin for which we search
 * @polarity - pointer to int in which the found polarity is stored
 * @trigger_mode - pointer to int in which the found trigger mode is stored
 * Return value:
 * 1 if an entry has been found, 0 otherwise
 */
int mptables_get_trigger_polarity(int irq_line, int* polarity, int* trigger_mode) {
    irq_routing_t* irq_routing;
    LIST_FOREACH(routing_list_head, irq_routing) {
        if ((irq_routing->dest_irq == irq_line) && (irq_routing->type == 0)) {
            *polarity = irq_routing->effective_polarity;
            *trigger_mode = irq_routing->effective_trigger;
            return 1;
        }
    }
    return 0;
}

/*
 * Scan MP table and get input line of I/O APIC
 * a device is connected to.
 * Only routings of type 0 are considered, i.e
 * vectored interrupts
 * Parameters:
 * @bus: the bus on which the device is located
 * @device: the PCI device number
 * @pin: the pin used by the device, 1=A, 4=D
 * Return value:
 * If no entry is found in the MP table, -1
 * is returned, otherwise the IRQ line is returned
 */
int mptables_get_irq_pin_pci(int bus, int device, int pin) {
    irq_routing_t* irq_routing;
    int irq = IRQ_UNUSED;
    char src_pin = (char)(pin+'A'-1);
    /*
     * If the first bus (bus 0) is the ISA bus, the bus numbers
     * for the PCI bus start with 1, so PCI bus 0 is bus 1 etc.
     * This is a dirty hack...
     */
    if (1 == first_bus_is_isa)
       bus+=1;
    DEBUG("Looking for entry with src_pin=%c, src_device=%d, bus_id=%d\n", src_pin, device, bus); 
    LIST_FOREACH(routing_list_head, irq_routing) {
        if ((0 == irq_routing->type) && (irq_routing->src_device == device) &&
                (irq_routing->src_pin == src_pin) &&
                (irq_routing->src_bus->bus_id == bus))
            irq = irq_routing->dest_irq;
    }
    return irq;
}

/*
 * Extract the APIC pin to which a legacy ISA IRQ is connected
 * from the MP table. For that purpose, the table is scanned
 * until we hit upon an entry with src_irq = _irq and return the
 * destination IRQ of that entry. Only consider vectored interrupts
 * (type = 0)
 * Parameter:
 * @_irq - the ISA IRQ to look for
 * Return value:
 * APIC IRQ of ISA interrupt or IRQ_UNUSED if no entry could be found
 */
int mptables_get_apic_pin_isa(int _irq) {
    irq_routing_t* routing;
    LIST_FOREACH(routing_list_head, routing) {
        if ((_irq == routing->src_irq) && (0 == routing->type))
            return routing->dest_irq;
    }
    return IRQ_UNUSED;
}


/*
 * Get primary (i.e. first) I/O APIC that we could find
 */ 
io_apic_t* mptables_get_primary_ioapic() {
    return io_apic_list_head;
} 

/********************************************************
 * Everything below this line is for debugging and      *
 * testing only                                         *
 *******************************************************/

/*
 * Print a list of all busses found in the system
 */
void mptables_print_bus_list() {
    bus_t* bus;
    char bus_type[7];
    PRINT("Bus ID        Type   \n");
    PRINT("------------------\n");
    LIST_FOREACH(bus_list_head, bus) {
        memset((void*) bus_type, 0, 7);
        strncpy(bus_type, bus->bus_type, 6);
        PRINT("%x     %s\n", bus->bus_id, bus_type);
    }
}





/*
 * Print an IRQ entry in the MP table
 * Parameters:
 * irq_routing - the routing to be printed
 */
static void print_irq_routing(irq_routing_t* irq_routing) {
    int is_pci = 0;
    int src_bus_id = 0;
    if (irq_routing->src_bus)
        is_pci = irq_routing->src_bus->is_pci;
    src_bus_id = irq_routing->src_bus->bus_id;
    if (is_pci) {
        PRINT(
                "%h           %h:%c        %h    %h    %h        %h          %h       %h\n",
                src_bus_id, irq_routing->src_device, irq_routing->src_pin,
                irq_routing->dest_irq, irq_routing->type,
                irq_routing->polarity, irq_routing->effective_polarity,
                irq_routing->trigger, irq_routing->effective_trigger);
    }
    else {
        PRINT(
                "%h     %h                %h    %h    %h        %h          %h       %h\n",
                src_bus_id, irq_routing->src_irq, irq_routing->dest_irq,
                irq_routing->type, irq_routing->polarity,
                irq_routing->effective_polarity, irq_routing->trigger,
                irq_routing->effective_trigger);
    }
}

/*
 * Print a list of all routings
 */
void mptables_print_routing_list() {
    irq_routing_t* irq_routing_ptr;
    int linecount = 0;
    char c[2];
    if (0 == bus_list_head) {
        return;
    }
    irq_routing_ptr = routing_list_head;
    PRINT("Source                   Destination\n");
    PRINT(
            "       ISA   PCI                     Orig      Effective   Orig     Effective \n");
    PRINT(
            "Bus    IRQ   Device/PIN  IRQ   Type  Polarity  Polarity    Trigger  Trigger   \n");
    PRINT(
            "--------------------------------------------------------------------------------\n");
    while (irq_routing_ptr) {
        print_irq_routing(irq_routing_ptr);
        linecount++;
        irq_routing_ptr = irq_routing_ptr->next;
        if (linecount > 16) {
            PRINT("Hit any key to see next page\n");
            debug_getline(c, 1);
            linecount = 0;
            PRINT("Source                   Destination\n");
            PRINT(
                    "       ISA   PCI                     Orig      Effective   Orig     Effective \n");
            PRINT(
                    "Bus    IRQ   Device/PIN  IRQ   Type  Polarity  Polarity    Trigger  Trigger   \n");
            PRINT(
                    "--------------------------------------------------------------------------------\n");
        }
    }
}

/*
 * Print I/O APICs
 */
void mptables_print_io_apics() {
    io_apic_t* io_apic;
    PRINT("ID     Base address\n");
    PRINT("-------------------\n");
    LIST_FOREACH(io_apic_list_head, io_apic) {
        PRINT("%h     %p\n", io_apic->apic_id, io_apic->base_address);
    }
}

/*
 * Print configuration of first I/O APIC
 */
void mptables_print_apic_conf() {
    if (0 == io_apic_list_head)
        PRINT("No APIC present\n");
    else
        apic_print_configuration(io_apic_list_head);
}


/*
 * Print PIR table entries
 */
void mptables_print_pir_table() {
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
