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

#include "irq.h"
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
 * This is a linked list of all busses found
 * in the system while scanning the MP table
 */
static bus_t* bus_list_head = 0;
static bus_t* bus_list_tail = 0;


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
 * This is the local APIC id of the BSP
 */
static u32 bsp_apic_id = 0;

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
 * At boot time, the interrupt manager parses the MP BIOS tables and stores the         *
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
    LIST_ADD_END(routing_list_head, routing_list_tail, irq_routing_ptr);
}

/*
 * This function will scan the MP table and build a list of
 * IRQ routings out of the entries it finds.
 * Parameters:
 * @mp_table_header - pointer to header of MP table
 */
static void mp_table_build_routing_list(mp_table_header_t* mp_table_header) {
    u32 mp_table_entry_max;
    void* mp_table_entry_ptr;
    int mp_table_entry_count = 0;
    u8 mp_table_entry_type;
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
        if (MP_TABLE_ENTRY_TYPE_ROUTING == mp_table_entry_type) {
            mp_table_process_routing(mp_table_entry_ptr);
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
    io_apic->apic_flags = mp_table_io_apic->io_apic_flags;
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
                    bsp_apic_id = cpu_entry->local_apic_id;
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
static void mp_table_init() {
    mp_table_header_t* mp_table;
    mp_table = mp_table_scan();
    if (0 == mp_table) {
        return;
    }
    mp_table_build_bus_list(mp_table);
    mp_table_build_routing_list(mp_table);
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
static int get_trigger_polarity(int irq_line, int* polarity, int* trigger_mode) {
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
static int get_irq_pin_pci(int bus, int device, int pin) {
    irq_routing_t* irq_routing;
    int irq = IRQ_UNUSED;
    char src_pin = (char)(pin+'A'-1);
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
static int get_apic_pin_isa(int _irq) {
    irq_routing_t* routing;
    LIST_FOREACH(routing_list_head, routing) {
        if ((_irq == routing->src_irq) && (0 == routing->type))
            return routing->dest_irq;
    }
    return IRQ_UNUSED;
}



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
     * First scan the table to see whether we already have
     * assigned a vector for this IRQ previously
     */
    for (vector = 0; vector < IRQ_MAX_VECTOR; vector++) {
        if (_irq == irq[vector]) {
            *new = 0;
            return vector;
        }
    }
    /*
     * In PIC mode, the assignment is fixed
     */
    if (IRQ_MODE_PIC == irq_mode) {
        *new = 0;
        return _irq + IRQ_OFFSET_PIC;
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
    bottom = 0x30;
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
 static int add_isr(int _irq, int priority, isr_t isr, int force_bsp) {
     int vector;
     int new;
     isr_handler_t* isr_handler;
     int polarity;
     int trigger_mode;
     /*
       * Determine vector to use or reuse existing one
       */
      vector = assign_vector(_irq, priority, &new);
      /*
       * If this is the first assignment, add entry to I/O APIC
       */
      if ((1 == new) && (IRQ_MODE_APIC == irq_mode)) {
          if (get_trigger_polarity(_irq, &polarity, &trigger_mode)) {
              apic_add_redir_entry(io_apic_list_head, _irq, polarity, trigger_mode,
                      vector, ( (1 == force_bsp) ? 1 : apic_mode));
          }
          else {
              ERROR("Could not locate entry in MP table for IRQ %d\n", _irq);
              irq[vector] = IRQ_UNUSED;
              return -1;
          }
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
     /*
      * Scan MP table to locate IRQ for this device or get
      * legacy IRQ from device in PIC mode
      */
     if (IRQ_MODE_APIC == irq_mode) {
         _irq = get_irq_pin_pci(pci_dev->bus->bus_id, pci_dev->device, pci_dev->irq_pin);
     }
     else {
         _irq = pci_dev->irq_line;
         DEBUG("Got legacy IRQ %d\n", _irq);
     }
     if (IRQ_UNUSED == _irq) {
         ERROR("Could not locate MP table entry for device %d, pin %d on bus %d\n", pci_dev->device, pci_dev->irq_pin,
                 pci_dev->bus->bus_id);
         return -EINVAL;
     }
     /*
      * Add handler and redirection entry if needed
      */
     return add_isr(_irq, priority, new_isr, 1);
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
      * Scan MP table to locate IRQ for this device or directly use
      * legacy IRQ
      */
     if (IRQ_MODE_APIC == irq_mode) {
         apic_pin = get_apic_pin_isa(_irq);
     }
     else {
         apic_pin = _irq;
     }
     if (IRQ_UNUSED == apic_pin) {
         ERROR("Could not locate MP table entry for legacy IRQ %d\n", _irq);
         return -EINVAL;
     }
     /*
      * Add handler and redirection entry if needed
      * Use priority 1
      */
     vector = add_isr(apic_pin, priority, new_isr, 1);
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
    /*
     * Do nothing if we are in PIC mode or if APIC mode is one
     */
    if ((IRQ_MODE_PIC == irq_mode) || (1 == apic_mode))
        return;
    /*
     * Walk all assigned vectors
     */
    for (vector = 0; vector <= IRQ_MAX_VECTOR; vector++) {
        if (IRQ_UNUSED != irq[vector]) {
            /*
             * Remap entry in I/O APIC only if irq_locked is not set
             * for this entry
             */
            if (0 == irq_locked[vector])
                if (get_trigger_polarity(irq[vector], &polarity, &trigger_mode))
                    apic_add_redir_entry(io_apic_list_head, irq[vector], polarity, trigger_mode, vector, apic_mode);
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
    IRQ_DEBUG("Doing EOI for vector %d\n", ir_context->vector);
    if (params_get_int("irq_watch") == (ir_context->vector)) {
        DEBUG("Got EOI for context vector %d, ORIGIN_PIC = %d\n", ir_context->vector, ORIGIN_PIC(ir_context->vector));
    }
    if ((ir_context->vector >= 32)) {
         if (ORIGIN_PIC(ir_context->vector))
             pic_eoi(ir_context->vector, IRQ_OFFSET_PIC);
         else
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
    if (ir_context->vector > 31)
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
        else if (ir_context.vector > 31) {
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
    for (i = 0; i <= IRQ_MAX_VECTOR; i++) {
        irq[i] = IRQ_UNUSED;
    }
    /*
     * Do we have at least one APIC? If yes,
     * use it, otherwise use PIC
     */
    use_apic = params_get_int("apic");
    if (use_apic)
        mp_table_init();
    if ((0 == io_apic_list_head) || (0 == use_apic)) {
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
 * Print a list of all busses found in the system
 */
void irq_print_bus_list() {
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
void irq_print_routing_list() {
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
void irq_print_io_apics() {
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
void irq_print_apic_conf() {
    if (0 == io_apic_list_head)
        PRINT("No APIC present\n");
    else
        apic_print_configuration(io_apic_list_head);
}

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
