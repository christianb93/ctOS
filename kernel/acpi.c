/*
 * This is the ctOS ACPI module. Currently, only parsing of the static ACPI tables is 
 * supported, mainly the MADT, an AML interpreter is not provided
 */ 
 
 
#include "acpi.h"
#include "multiboot.h"
#include "kprintf.h"
#include "lib/string.h"
#include "apic.h"
#include "smp.h"
#include "params.h"
#include "keyboard.h"
#include "mm.h"
#include "cpu.h"
#include "irq.h"
#include "pci.h"

static char* __module = "ACPI  "; 
 
/*
 * Some data that we collect and fill over time
 */
static acpi_rsdp_t* rsdp = 0;
static acpi_entry_header_t* rsdt = 0;
static acpi_entry_header_t* xsdt = 0;
static char rsdt_oem_id[7];
static char rsdt_oem_tableid[9];
static u32 rsdt_oem_rev = 0;

/*
 * Are we ready, i.e. could we parse everything? 
 */
static int acpi_ready = 0;
/*
 * Are we going to serve information requests and therefore
 * overwrite the MP tables?
 */
static int __acpi_used = 0;

/*
 * This data is taken from the MADT
 */
static u32 local_apic_address = 0;
static int local_apic_count = 0;
static acpi_lapic_t local_apics[SMP_MAX_CPU];

static int io_apic_count = 0;
static io_apic_t primary_io_apic;
static u32 primary_gsi_base = 0;

/*
 * DSDT OEMID, OEM revision and TABLE ID
 */
static int have_dsdt = 0;
static char dsdt_oem_id[7];
static char dsdt_oem_tableid[9];
static u32 dsdt_oem_rev = 0;

/*
 * The ISA IRQ routings
 */
static isa_irq_routing_t isa_irq_routing[16];

/*
 * Additional entries for specific boards. Format is
 * Chipset component ID (must be probed successfully by pci_chipset_component_present)
 * DSDT OEM ID
 * DSDT OEM Table ID
 * DSDT OEM Revision
 * PIN 
 * Device
 * Bus
 * IO-APIC PIN
 */
static acpi_override_t acpi_overrides[] = {
    /*
     * This entry is for QEMU PIIX3, network card at device 3, bus 0
     */
    {PCI_CHIPSET_COMPONENT_PIIX3, "BOCHS ","BXPCDSDT", 1, 1, 3, 0, 0xa},
    /*
     * This entry is for QEMU Q35 (ICH9), network card at device 3, bus 0
     */
    {PCI_CHIPSET_COMPONENT_ICH9, "BOCHS ","BXPCDSDT", 1, 1, 3, 0, 0x17},
};

/*
 * Parse DSDT
 */
static void parse_dsdt(u32 dsdt_address) {
    acpi_entry_header_t* dsdt_header = (acpi_entry_header_t*) dsdt_address;
    have_dsdt = 1;
    strncpy(dsdt_oem_id, dsdt_header->oemid, 6);
    dsdt_oem_id[6] = 0;
    strncpy(dsdt_oem_tableid, dsdt_header->oemTableId, 8);
    dsdt_oem_tableid[8] = 0;
    dsdt_oem_rev = dsdt_header->oemRevision;
}

/*
 * Parse FADT.
 */
static void parse_fadt(u32 fadt_address) {
    acpi_fadt_header_t* fadt_header = (acpi_fadt_header_t*) (fadt_address + sizeof(acpi_entry_header_t));
    parse_dsdt((u32) fadt_header->dsdt_address);
}

/*
 * Parse the MADT. This function expects being called with two arguments
 * @madt - the 32 bit address of the MADT without the header
 * @length - the length of the interrupt controller structure array
 */
static void parse_madt(u32 madt_address, int length) {
    int i;
    int type;
    int read = 0;
    u32 madt_entry_address = 0;
    u32 entry_length = 0;
    acpi_madt_header_t* header = (acpi_madt_header_t*) madt_address;
    acpi_lapic_t* acpi_lapic;
    acpi_io_apic_t* acpi_io_apic;
    acpi_irq_override_t* acpi_irq_override;
    isa_irq_routing_t* isa_irq_routing_entry;
    local_apic_address = header->lapic_address;
    DEBUG("Using local APIC address %x\n", local_apic_address);
    /*
     * Next we parse the MADT itself. The first byte of each entry is
     * the type of the structure, the second byte its length.
     */
    madt_entry_address = madt_address + sizeof(acpi_madt_header_t);
    while (read < length) {
        type =  ((u8*)madt_entry_address)[0];
        entry_length = ((u8*)madt_entry_address)[1];
        switch (type) {
            /* TODO: parse local APIC address overrides as well */
            case MADT_ENTRY_TYPE_IO_APIC: 
               /*
                * For the time being, we ignore all but the first I/O APIC - 
                * after all, we only need to route the ISA interrupts anyway
                * as we rely on MSI for the PCI interrupts
                */
                if (0 == io_apic_count) {
                    DEBUG("Adding entry for primary IO APIC\n");
                    acpi_io_apic = (acpi_io_apic_t*) madt_entry_address;
                    primary_io_apic.apic_id = acpi_io_apic->io_apic_id;
                    primary_io_apic.base_address = acpi_io_apic->io_apic_address;
                    primary_gsi_base = acpi_io_apic->gsi_base;
                    io_apic_count = 1;
                }
                break;
            case MADT_ENTRY_TYPE_LOCAL_APIC:
                acpi_lapic = (acpi_lapic_t*) madt_entry_address;
                local_apics[local_apic_count].acpi_cpu_id = acpi_lapic->acpi_cpu_id;
                local_apics[local_apic_count].local_apic_id = acpi_lapic->local_apic_id;
                local_apics[local_apic_count].local_apic_flags = acpi_lapic->local_apic_flags;
                local_apic_count++;
                DEBUG("Added entry for local APIC\n");
                break;
            case MADT_ENTRY_TYPE_OVERRIDE:
                /*
                 * We only fill the GSI field now, as we might not yet have seen the I/O APIC
                 * and therefore do not know the GSI base
                 */
                acpi_irq_override = (acpi_irq_override_t*) madt_entry_address;
                isa_irq_routing_entry = isa_irq_routing + acpi_irq_override->src_irq;
                DEBUG("Processing override for source irq %h, GSI = %d\n", acpi_irq_override->src_irq, acpi_irq_override->gsi);
                if (acpi_irq_override->src_irq > 15) {
                    ERROR("Unexpected src irq %d\n", acpi_irq_override->src_irq);
                }
                else {
                    isa_irq_routing_entry->gsi = acpi_irq_override->gsi;
                    isa_irq_routing_entry->polarity = acpi_irq_override->flags & 0x3;
                    isa_irq_routing_entry->trigger = (acpi_irq_override->flags >> 2) & 0x3;
                }
                break;
        }
        madt_entry_address += entry_length;
        read += entry_length;
    }
    /*
     * We are now done with the table. The only thing that remains to do is to fix
     * up the overrides - convert the GSI into the input pin of the primary I/O APIC
     */
    if (io_apic_count) {
        for (i = 0; i < 16; i++) {
            if (isa_irq_routing[i].gsi != -1) {
                /*
                 * This entry has been overridden. Determine the new
                 * I/O APIC pin
                 */
                /* TODO: what happens if this is negative? Is there any rule that the first 
                 * I/O APIC needs to cover 0 - 15? Or should we define the primary I/O APIC to be the
                 * one that covers this area?
                 */
                isa_irq_routing[i].io_apic_input = isa_irq_routing[i].gsi - primary_gsi_base;
            }
        }
    }
}

/*
 * Parse a single static table, as described
 * in section 5.2.6
 */
static void parse_acpi_table(u32 table_address) {
    acpi_entry_header_t* header = (acpi_entry_header_t*) table_address;
    if (0 == strncmp(header->signature, "APIC", 4)) {
        MSG("Found MADT\n");
        parse_madt(table_address + sizeof(acpi_entry_header_t), header->length - sizeof(acpi_entry_header_t) - sizeof(acpi_madt_header_t));
    }
    if (0 == strncmp(header->signature, "FACP", 4)) {
        MSG("Found FADT\n");
        parse_fadt(table_address);
    }
}

/*
 * Parse the RSDT
 */
static int parse_rsdt() {
    u32 entries;
    u32 rsdt_address = (u32) rsdt;
    u32 entry_address = 0;
    u32 table_address;
    int i;
    MSG("Parsing RSDT\n");
    /*
     * Get some data from the header
     */
    strncpy(rsdt_oem_id, rsdt->oemid, 6);
    rsdt_oem_id[6]=0;
    strncpy(rsdt_oem_tableid, rsdt->oemTableId, 8);
    rsdt_oem_tableid[8]=0;
    rsdt_oem_rev = rsdt->oemRevision;
    /*
     * The RSDT entries are 32 bit addresses of
     * the further tables. 
     */
    entries = (rsdt->length - sizeof(acpi_entry_header_t)) / sizeof(u32);
    for (i = 0; i < entries; i++) {
        entry_address = rsdt_address + sizeof(acpi_entry_header_t) + sizeof(u32) * i;
        table_address = *((u32*) entry_address);
        parse_acpi_table(table_address);
    }
    return 1;
}
 
/*
 * Parse the XSDT
 */
static int parse_xsdt() {
    u32 entries;
    u32 xsdt_address = (u32) xsdt;
    u32 entry_address = 0;
    u32 table_address_low;
    u32 table_address_high;
    int i;
    MSG("Parsing XSDT\n");
    /*
     * Get some data from the header
     */
    strncpy(rsdt_oem_id, xsdt->oemid, 6);
    rsdt_oem_id[6]=0;
    strncpy(rsdt_oem_tableid, xsdt->oemTableId, 8);
    rsdt_oem_tableid[8]=0;
    rsdt_oem_rev = xsdt->oemRevision;    
    /*
     * The XSDT entries are 64 bit addresses of
     * the further tables. 
     */
    entries = (xsdt->length - sizeof(acpi_entry_header_t)) / sizeof(u64);
    for (i = 0; i < entries; i++) {
        entry_address = xsdt_address + sizeof(acpi_entry_header_t) + sizeof(u64) * i;
        table_address_low = *((u32*) (entry_address));
        table_address_high = *((u32*) (entry_address + sizeof(u32)));
        if (table_address_high) {
            MSG("Ignoring table above 4 Gb\n");
        }
        parse_acpi_table(table_address_low);
    }
    return 1;    
} 
 
/*
 * Initialize the module. We try to locate and parse the static part of the 
 * ACPI tables
 */
int acpi_parse() {
    u16 ebda_segment;
    void* ebda_ptr;
    void* table_search;
    int i;
    /*
     * Initialize the routing tables
     */
    for (i = 0; i < 16; i++) {
        isa_irq_routing[i].trigger = 0; 
        isa_irq_routing[i].polarity = 0; 
        isa_irq_routing[i].src_irq = i;
        isa_irq_routing[i].io_apic_input = i;
        isa_irq_routing[i].gsi = -1; // -1 = not overridden
    }    
    /*
     * First ask the multiboot module whether
     * the boot loader has provided a copy of the RSDP
     */
    rsdp = (acpi_rsdp_t*) multiboot_get_acpi_rsdp();
    if (0 == rsdp) {
        DEBUG("Scanning EBDA for ACPI RSDP\n");
        /*
         * On an EFI system with GRUB2, GRUB2 would have provided an RSDP. So we
         * are either on an unsupported boot loader or a BIOS system. So we try
         * to find the signature in memory next. 
         * According to section 5.2.5.1 of the specification,
         * we search in the first kb of the EBDA segment next (on 16 byte boundaries)
         */
        ebda_segment = (*((u16*) 0x40e));
        /*
         * As this is the segment in real mode x86 terminology, multiply by 16
         * to get physical address
         */
        ebda_ptr = (void*) (((unsigned int) (ebda_segment)) * 16);
        /*
        * Scan first kilobyte EBDA 
        */
        if (ebda_segment) {
            table_search = ebda_ptr;
            while (table_search <= ebda_ptr + 1024) {
                if (0 == strncmp(table_search, "RSD PTR ", 8)) {
                    MSG("Found ACPI RSDP at address %x\n", table_search);
                    rsdp = (acpi_rsdp_t*) table_search;
                    break;
                }
                table_search += 16;
            }
        }
    }
    /*
     * If we have not yet found the RSDP, continue search in the
     * BIOS read only area between 0E0000h and 
     */
    if (0 == rsdp) {
        DEBUG("Scanning BIOS read only area for RSDP\n");
        for (table_search = (void*) 0x0E0000; table_search < (void*) 0xFFFFF; table_search+=16) {
            if (0 == strncmp(table_search, "RSD PTR ", 8)) {
                DEBUG("Found ACPI RSDT at address %x\n", table_search);
                rsdp = (acpi_rsdp_t*) table_search;
                break;
            }
        }
    }
    if (0 == rsdp)
        return 0;
    /*
     * Now start to walk the structure. We next get the RSDT.
     */
    if (0 == rsdp->revision) {
        rsdt = (acpi_entry_header_t*) rsdp->rsdtAddress;
    } 
    else {
        DEBUG("Looks like version 2 upwards\n");
        /*
         * If the xsdt points to an area within the first 
         * 4 GB, use it, otherwise use 32 bit address
         * of RSDT
         */
        if (0 == (rsdp->xsdtAddress >> 32)) {
            xsdt = (acpi_entry_header_t*) ((u32) rsdp->xsdtAddress);
        }
        else {
            rsdt = (acpi_entry_header_t*) rsdp->rsdtAddress;
        }
    }
    /*
     * Next parse RSDT or XSDT
     */
    if ((0 == rsdt) && (0 == xsdt)) {
        return 0;
    }
    if (rsdt) {
        acpi_ready = parse_rsdt();
    }
    else {
        acpi_ready = parse_xsdt();
    }
    return acpi_ready;
}

/*
 * This needs to be called once paging is enabled. 
 */
void acpi_init() {
    int i;
    if (0 == acpi_ready)
        return;
    /*
     * If the parameter use_acpi is not set, we do nothing
     */
    if (0 == params_get_int("use_acpi")) {
        __acpi_used = 0;
        return;
    }
    __acpi_used = 1;
    MSG("Using ACPI as primary information source\n");
    if (local_apic_address) {
        /*
        * Inform the CPU module about the local APIC address to use. 
        * This will map the local APIC page into virtual memory
        * and therefore requires paging
        */
        DEBUG("Setting up LAPIC paging for BSP\n");
        apic_init_bsp(local_apic_address);
    }
    /*
     * Next we map the memory space used by the IO APIC
     */
    DEBUG("Mapping IO APIC base address %x into virtual memory\n", primary_io_apic.base_address);
    primary_io_apic.base_address = mm_map_memio(primary_io_apic.base_address, 14); 
    DEBUG("Done, base address is now %x\n", primary_io_apic.base_address);
    /*
     * Inform the CPU module about the local APICs that we have found
     */
    for (i = 0 ; i < local_apic_count; i++) {
        if (local_apics[i].local_apic_flags & ACPI_MADT_LAPIC_FLAGS_ENABLED) {
            DEBUG("Handing over CPU %d to CPU module\n", i);
            /*
             * A few words on the version. The only point where we really use
             * the version is to figure out (in cpu_external_apic) whether the 
             * local APIC is on-chip or external. This information is not present
             * in the ACPI tables, but these days we can assume that it is integrated
             */
            cpu_add(local_apics[i].local_apic_id, (i == 0 ? 1 : 0), 0x10);
        }
        else {
            DEBUG("Found disabled CPU entry\n");
        }
    }
}

/*
 * Is the ACPI the leading configuration source?
 */
int acpi_used() {
    return __acpi_used;
}

/*
 * Get the IO APIC pin for an ISA interrupt
 * Parameter:
 * @irq - the ISA interrupt number
 */
int acpi_get_apic_pin_isa(int irq) {
    if ((0 > irq) || (irq > 15))
        return IRQ_UNUSED;
    return isa_irq_routing[irq].io_apic_input;
}


/*
 * Get the IO APIC pin for a PCI interrupt
 */
int acpi_get_irq_pin_pci(int bus_id, int device,  char irq_pin) {
    int i;
    if (0 == have_dsdt)
        return IRQ_UNUSED;
    for (i = 0; i < sizeof(acpi_overrides) / sizeof(acpi_override_t); i++) {
        if ((0 == strncmp(dsdt_oem_id, acpi_overrides[i].oem_id, 6)) && 
            (pci_chipset_component_present(acpi_overrides[i].chipset_component_id))) {
            if (0 == strncmp(dsdt_oem_tableid, acpi_overrides[i].oem_table_id, 8)) {
                if ((irq_pin  == acpi_overrides[i].src_pin) 
                        && (dsdt_oem_rev == acpi_overrides[i].oem_rev)
                        && (device == acpi_overrides[i].src_device)
                        && (bus_id == acpi_overrides[i].src_bus_id)) {
                    MSG("Applying override for device %d:%d:%d:  %d\n", bus_id, device, irq_pin, acpi_overrides[i].dest_irq);
                    return acpi_overrides[i].dest_irq;
                }
            }
        }
    }    
    return IRQ_UNUSED;
}

/*
 * Search the table of overrides for a given pin
 * and return 1 if a matching override exists
 */
static int search_overrides(int pin) {
    int i;
    if (0 == have_dsdt)
        return 0;
    for (i = 0; i < sizeof(acpi_overrides) / sizeof(acpi_override_t); i++) {
        if ((0 == strncmp(dsdt_oem_id, acpi_overrides[i].oem_id, 6))  && 
            (pci_chipset_component_present(acpi_overrides[i].chipset_component_id))) {
            if (0 == strncmp(dsdt_oem_tableid, acpi_overrides[i].oem_table_id, 8)) {
                if ((pin == acpi_overrides[i].dest_irq) && (dsdt_oem_rev == acpi_overrides[i].oem_rev)) {
                    MSG("Applying override for IRQ pin %d\n", pin);
                    return 1;
                }
            }
        }
    }
    return 0;
}

/*
 * Get interrupt trigger mode and polarity for an IO APIC pin
 * Parameter:
 * @pin - the IO APIC pin
 * @trigger, polarity: pointer for return values
 * Returns
 * 1 - the interrupt is known
 * 0 - the interrupt is not known
 */
int acpi_get_trigger_polarity(int pin, int* polarity, int* trigger) {
    int i;
    int src_irq = -1;
    if (0 == __acpi_used)
        return 0;
    if (pin < 0) 
        return 0;
    /*
     * Find the ISA IRQ table entry
     */
    if (pin < 16) {
        for (i = 0; i < 16; i++) {
            if (pin == isa_irq_routing[i].io_apic_input) 
                src_irq = i;
        }
    }
    if (-1 == src_irq) {
        /*
         * Not found. See whether we have any overrides
         */
        if (search_overrides(pin)) {
            /*
             * We have a PCI device connected to this
             * PIN. Assume PCI defaults.
             */
            *polarity = IRQ_POLARITY_ACTIVE_LOW;
            *trigger = IRQ_TRIGGER_MODE_LEVEL;
            return 1;
        }
        return 0;
    }
    /*
     * First determine polarity
     */
    switch (isa_irq_routing[src_irq].polarity) {
        case 0:
            /*
             * As determine by bus. As we assume ISA bus here, we use
             * active high
             */
            *polarity = IRQ_POLARITY_ACTIVE_HIGH;
            break;
        case 1: 
            *polarity = IRQ_POLARITY_ACTIVE_HIGH;
            break;
        case 3:
            *polarity = IRQ_POLARITY_ACTIVE_LOW;
            break;
        default:
            ERROR("Unknown polarity\n");
    }
    /*
     * Now we do the trigger mode
     */
    switch (isa_irq_routing[src_irq].trigger) {
        case 0:
            /*
             * Again we assume ISA and use edge triggered
             */
            *trigger = IRQ_TRIGGER_MODE_EDGE;
            break;
        case 1:
            *trigger = IRQ_TRIGGER_MODE_EDGE;
            break;
        case 3:
            *trigger = IRQ_TRIGGER_MODE_LEVEL;
            break;
        default: 
            ERROR("Unknown polarity\n");
    }
    return 1;
}

/*
 * Return a pointer to the primary I/O APIC
 */
io_apic_t* acpi_get_primary_ioapic() {
    if (0 == __acpi_used) 
        return 0;
    if (io_apic_count == 0)
        return 0;
    return &primary_io_apic;
}


/***************************************************************
 * Everything below this line is for debugging only            *
 **************************************************************/

void acpi_print_info() {
    int i;
    PRINT("Address of RSDP:         %x\n", rsdp);
    PRINT("ACPI ready:              %d\n", acpi_ready);
    PRINT("ACPI used:               %d\n", __acpi_used);
    if (rsdp) {
        PRINT("Revision:                %d\n", rsdp->revision);
        PRINT("OEMID:                   ");
        for (i = 0; i < 6; i++) {
            kprintf("%c",rsdp->oemid[i]);
        }
        kprintf("\n");
        PRINT("RSDT address:            %x\n", rsdt);
        PRINT("XSDT address:            %x\n", xsdt);
    }
    PRINT("RSDT OEM ID:             %s\n", rsdt_oem_id);
    PRINT("RSDT OEM TABLE ID:       %s\n", rsdt_oem_tableid);
    PRINT("RSDT OEM REV:            %d\n", rsdt_oem_rev);
    if (have_dsdt) {
        PRINT("DSDT OEM ID:             %s\n", dsdt_oem_id);
        PRINT("DSDT OEM TABLE ID:       %s\n", dsdt_oem_tableid);
        PRINT("DSDT OEM REV:            %d\n", dsdt_oem_rev);
    }
}

void acpi_print_madt() {
    int i;
    /*
     * First print all the local APIC entries 
     */
    PRINT("Local APIC entries: \n");
    PRINT("-----------------------------------\n");
    PRINT("CPU   LAPIC      Flags\n");
    PRINT("ID    ID              \n");
    PRINT("------------------------------------------\n");
    for (i = 0 ; i < local_apic_count; i++) {
        PRINT("%h    %h         %x\n",
            local_apics[i].acpi_cpu_id,
            local_apics[i].local_apic_id,
            local_apics[i].local_apic_flags);
    }
    PRINT("------------------------------------------\n");
    PRINT("Primary IO APIC entry: \n");
    PRINT("------------------------------------------\n");
    if (0 == io_apic_count) {
        PRINT("None\n");
    }
    else {
        PRINT("IO APIC ID:      %h\n", primary_io_apic.apic_id);
        PRINT("Base address:    %x\n", primary_io_apic.base_address);
        PRINT("GSI base:        %d\n", primary_gsi_base);
    }
    PRINT("Hit any key to continue\n");
    early_getchar();
    PRINT("--------------------------------------------------\n");
    PRINT("ISA IRQ Overrides: \n");
    PRINT("--------------------------------------------------\n");
    PRINT("SRC         IO APIC   Override   Polarity  Trigger\n");
    PRINT("IRQ         PIN \n");
    PRINT("--------------------------------------------------\n");
    for (i = 0; i < 16; i++) {
        PRINT("%h          %h        %c          %d         %d\n",
            isa_irq_routing[i].src_irq,
            isa_irq_routing[i].io_apic_input,
            (isa_irq_routing[i].gsi == -1 ? 'N' : 'Y' ),
            isa_irq_routing[i].polarity,
            isa_irq_routing[i].trigger);
    }
}