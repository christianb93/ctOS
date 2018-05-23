/*
 * pci.c
 *
 * This is the PCI bus driver which is responsible for enumerating PCI devices at boot time and
 * presenting PCI devices to device drivers
 */

#include "pci.h"
#include "io.h"
#include "locks.h"
#include "debug.h"
#include "mm.h"
#include "lists.h"
#include "keyboard.h"
#include "vga.h"
#include "cpu.h"

static char* __module = "PCI   ";


/*
 * These pointers point to head and tail of a list
 * of PCI devices
 */
static pci_dev_t* pci_dev_list_head=0;
static pci_dev_t* pci_dev_list_tail=0;

/*
 * List of PCI busses in the system
 */
static pci_bus_t* pci_bus_list_head=0;
static pci_bus_t* pci_bus_list_tail=0;

/*
 * PCI config spaces are accessed via a index/data
 * register pair. Thus concurrent reads or writes can lead to
 * wrong data. To avoid this, we use a spinlock to protect these
 * registers
 */
static spinlock_t pci_config_reg_lock;

/*
 * A table of some known classes.
 * See for instance http://www.pcidatabase.com for a full list
 */

static pci_class_t pci_class_codes[] = { {
        0x00,
        0x00,
        0x00,
        "Pre 2.0 - Non-VGA" }, { 0x00, 0x01, 0x00, "Pre 2.0 - VGA" }, {
        0x01,
        0x00,
        0x00,
        "Storage - SCSI" }, { 0x01, 0x01, 0xff, "Storage - IDE" }, {
        0x01,
        0x02,
        0x00,
        "Storage - Floppy" }, { 0x01, 0x03, 0x00, "Storage - IPI" }, {
        0x01,
        0x04,
        0x00,
        "Storage - RAID" }, { 0x01, 0x06, 0x01, "SATA - AHCI" }, {
        0x01,
        0x80,
        0x00,
        "Storage - Other" }, { 0x02, 0x00, 0x00, "Ethernet" }, {
        0x02,
        0x01,
        0x00,
        "Token Ring" }, { 0x03, 0x00, 0x00, "Display - VGA" }, {
        0x03,
        0x00,
        0x01,
        "Display - 8514" }, { 0x03, 0x01, 0x00, "Display - XGA" }, {
        0x03,
        0x80,
        0x00,
        "Display - Other" }, { 0x04, 0x03, 0xff, "Audio" }, {
        0x06,
        0x00,
        0x00,
        "Host/PCI Bridge" }, { 0x06, 0x01, 0x00, "PCI/ISA Bridge" }, {
        0x06,
        0x02,
        0x00,
        "PCI/EISA Bridge" }, { 0x06, 0x03, 0x00, "PCI/MCA Bridge" }, {
        0x06,
        0x04,
        0xff,
        "PCI/PCI Bridge" }, { 0x06, 0x05, 0x00, "PCI/PCMCIA Bridge" }, {
        0x06,
        0x80,
        0x00,
        "Bridge - Other" }, { 0x08, 0x00, 0x00, "PIC 8259" }, {
        0x08,
        0x00,
        0x01,
        "PIC ISA" }, { 0x08, 0x00, 0x02, "PIC PCI" }, {
        0x08,
        0x00,
        0x20,
        "I/O APIC" }, { 0x08, 0x01, 0x00, "DMA 8259" }, {
        0x08,
        0x01,
        0x01,
        "DMA ISA" }, { 0x08, 0x01, 0x02, "DMA EISA" }, {
        0x08,
        0x02,
        0x00,
        "Timer 8259" }, { 0x08, 0x02, 0x01, "Timer ISA" }, {
        0x08,
        0x02,
        0x02,
        "Timer EISA" }, { 0x08, 0x03, 0x00, "RTC Generic" }, {
        0x08,
        0x03,
        0x01,
        "RTC ISA" }, { 0x0C, 0x00, 0xff, "Firewire (IEEE 1394)" }, {
        0x0C,
        0x03,
        0x00,
        "USB Controller" }, { 0x0C, 0x03, 0x20, "USB EHCI" }, {
        0x0C,
        0x03,
        0x30,
        "USB XHCI" }, { 0x0C, 0x5, 0xff, "SMBus" } };
        
/*
 * Valid capabilities
 */

static capability_t capabilities[] = { 
            { 1, "Power Management" }, 
            { 2, "AGP" },
            { 3, "VPD" }, 
            { 4, "Slot" }, 
            { 5, "MSI" },
            { 6, "Compact PCI Hot Swap" }, 
            { 7, "PCI-X" }, 
            { 0xc, "PCI HotPlug" },
            {0x10, "PCI Express"}, 
            {0x11, "MSI-X"}, 
            {0x12, "SATA"}} ;        


#define CAPABILITY_LIST_SIZE (sizeof(capabilities) / sizeof(capability_t))
#define PCI_CLASS_CODE_SIZE (sizeof(pci_class_codes) / sizeof(pci_class_t))

/*
 * Forward declarations
 */
static int probe_ich9(pci_dev_t*);
static int probe_ich10R(pci_dev_t*);
static int probe_piix3(pci_dev_t*);

/*
 * Some chipset components that we know
 */
static pci_chipset_component_t chipset_components[] = {
    {PCI_CHIPSET_COMPONENT_ICH9, "ICH9", "Intel ICH9 I/O Controller Hub", 0, probe_ich9 },
    {PCI_CHIPSET_COMPONENT_ICH10R, "ICH10R", "Intel ICH10R I/O Controller Hub", 0, probe_ich10R },
    {PCI_CHIPSET_COMPONENT_PIIX3, "PIIX3", "Intel PIIX3 PCI ISA IDE XCELERATOR", 0, probe_piix3 },
};

/******************************************************************
 * These functions read and write the PCI configuration           *
 ******************************************************************/


/*
 * Read four bytes from the PCI configuration space
 * Parameter:
 * @bus: bus specifying the device to read from
 * @device: device number to read from
 * @function: function number to read from
 * @offset: offset into 256-byte config space (sometimes called register),
 *          should be a multiple of 4, if not it is rounded down to
 *          a multiple of 4 by settings bits 0 and 1 to 0
 * Return value:
 * a dword containing the content of the register
 * Locks:
 * pci_config_reg_lock
 */
static u32 pci_get_dword_config(u8 bus, u8 device, u8 function, u8 offset) {
    /*
     * We get the configuration byte using method 1 - write to 0xcf8
     * and read from 0xcfd
     * Using this method, we read 4 configuration bytes at a time,
     * i.e. if offset is 4, we actually read from registers 4,5,6
     * and 7 in one transaction
     * The double word which we have to write to 0xcf8 is build up
     * as follows
     * Bit 31: set to 1 (enable configuration read/write)
     * Bit 16-23: bus number (8 bit)
     * Bit 11-15: device number (5 bit, i.e. 0-31)
     * Bit 8-10: function number (3 bit, i.e. 0-7)
     * Bit 2-7: bits 2-7 of offset
     * Bit 0-1: always zero
     */
    u32 eflags;
    u8 offset_base;
    u32 address;
    u32 data;
    /*
     * Set bits 0 and 1 to 0
     */
    offset_base = offset & 0xfc;
    /*
     * Now assemble address to be written to 0xcf8
     * Recall that the PCI configuration space protocol specifies
     * that this is composed of bus, device, function and offset
     */
    address = 0x80000000 + (((u32) bus) << 16)
            + ((((u32) device) & 0x1f) << 11) + ((((u32) function) & 0x7) << 8)
            + offset_base;
    /*
     * Write to port and read result
     */
    spinlock_get(&pci_config_reg_lock, &eflags);
    outl(address, PCI_CONFIG_ADDRESS);
    data = inl(PCI_CONFIG_DATA);
    spinlock_release(&pci_config_reg_lock, &eflags);
    return data;
}

/*
 * Get an individual word (i.e. two registers only)
 * as opposed to the full 32-bit dword (4 registers) returned
 * by the function pci_get_dword_config
 * Parameter:
 * @bus: bus specifying the device to read from
 * @device: device number to read from
 * @function: function number to read from
 * @offset: offset into 256-byte config space 
 * Return value:
 * a word containing the content of the register

 */
static u16 pci_get_word_config(u8 bus, u8 device, u8 function, u8 offset) {
    u32 dword_pci_value;
    /*
     * Position of word we are looking for in 32-bit dword
     * returned by pci_get_dword_config
     */
    int position = offset % 4;
    dword_pci_value = pci_get_dword_config(bus, device, function, offset);
    return (u16) (dword_pci_value >> (8 * position));
}

/*
 * Get an individual byte from an individual register
 * as opposed to the full 32-bit dword (4 registers) returned
 * by the function pci_get_dword_config
 * Parameter:
 * @bus: bus specifying the device to read from
 * @device: device number to read from
 * @function: function number to read from
 * @offset: offset into 256-byte config space 
 * Return value:
 * a byte containing the content of the register

 */
static u8 pci_get_byte_config(u8 bus, u8 device, u8 function, u8 offset) {
    u32 dword_pci_value;
    /*
     * Position of byte we are looking for in 32-bit dword
     * returned by pci_get_dword_config
     */
    int position = offset % 4;
    dword_pci_value = pci_get_dword_config(bus, device, function, offset);
    return (u8) (dword_pci_value >> (8 * position));
}
/*
 * Write four bytes to the PCI configuration space
 * Parameter:
 * @bus: bus specifying the device to read from
 * @device: device number to read from
 * @function: function number to read from
 * @offset: offset into 256-byte config space (sometimes called register),
 *          should be a multiple of 4, if not it is rounded down to
 *          a multiple of 4 by settings bits 0 and 1 to 0
 * @value: a dword containing the content of the register
 * Locks:
 * pci_config_reg_lock
 */
static void pci_put_dword_config(u8 bus, u8 device, u8 function, u8 offset, u32 value) {
    u32 eflags;
    u8 offset_base;
    u32 address;
    /*
     * Set bits 0 and 1 to 0
     */
    offset_base = offset & 0xfc;
    /*
     * Now assemble address to be written to 0xcf8
     * Recall that the PCI configuration space protocol specifies
     * that this is composed of bus, device, function and offset
     */
    address = 0x80000000 + (((u32) bus) << 16)
            + ((((u32) device) & 0x1f) << 11) + ((((u32) function) & 0x7) << 8)
            + offset_base;
    /*
     * Write to both ports
     */
    spinlock_get(&pci_config_reg_lock, &eflags);
    outl(address, PCI_CONFIG_ADDRESS);
    outl(value, PCI_CONFIG_DATA);
    spinlock_release(&pci_config_reg_lock, &eflags);
    return;
}

/* Put an individual word into two consecutive registers
 * as opposed to the full 32-bit dword (4 registers) written
 * by the function pci_put_dword_config
 * Note that offset needs to be even for this to work!
 */
static void pci_put_word_config(u8 bus, u8 device, u8 function, u8 offset, u16 value) {
    u32 dword_pci_value;
    /* 
     * Position of word we are looking for in 32-bit dword
     * returned by pci_get_dword_config
     */
    int position = offset % 4;
    /* 
     * First read old value 
     */
    dword_pci_value = pci_get_dword_config(bus, device, function,
            offset);
    /* 
     * Now patch requested byte into value 
     */
    dword_pci_value = dword_pci_value & ~(0xffff << (8*position));
    dword_pci_value = dword_pci_value | (((u32)value) << (8*position));
    pci_put_dword_config(bus, device, function, offset, dword_pci_value);
}


/******************************************************************
 * The following functions build and maintain our internal tables *
 * with device and configuration information                      *
 ******************************************************************/


/*
 * Locate bus by id in internal table
 * Parameter:
 * @bus_id - the bus ID
 * Return value:
 * a pointer to the bus or 0 if bus could not be found
 */
static pci_bus_t* pci_get_bus_for_id(u8 bus_id) {
    pci_bus_t* pci_bus;
    LIST_FOREACH(pci_bus_list_head, pci_bus) {
        if (bus_id == pci_bus->bus_id) {
            return pci_bus;
        }
    }
    return 0;
}

/*
 * Scan a given device and fill the pci device structure
 * passed as argument accordingly
 * Parameters:
 * @bus_id - the bus id
 * @device - the device
 * @function - the function
 * @pci_dev - the function to be filled
 */
static void pci_scan_device(u8 bus_id, u8 device, u8 function,
        pci_dev_t* pci_dev) {
    u32 command_status;
    u32 vendor_device_id;
    u8 primary_bus;
    u8 secondary_bus;
    u8 cap_pointer;
    u8 cap_id;
    int i = 0;
    /*
     * Fill structure. This is not very efficient, as we read several
     * 32 bit registers more than once, but lets keep things simple...
     */
    vendor_device_id = pci_get_dword_config(bus_id, device, function,
            PCI_HEADER_VENDOR_DEVID_REG);
    pci_dev->device = device;
    pci_dev->function = function;
    pci_dev->vendor_id = (u16) vendor_device_id;
    pci_dev->device_id = (u16) (vendor_device_id >> 16);
    pci_dev->base_class = pci_get_byte_config(bus_id, device, function,
            PCI_HEADER_BASECLASS_REG);
    pci_dev->sub_class = pci_get_byte_config(bus_id, device, function,
            PCI_HEADER_SUBCLASS_REG);
    pci_dev->prog_if = pci_get_byte_config(bus_id, device, function,
            PCI_HEADER_PROGIF_REG);
    pci_dev->header = pci_get_byte_config(bus_id, device, function,
            PCI_HEADER_TYPE_REG);
    pci_dev->msi_support = 0;
    pci_dev->uses_msi = 0;
    /*
     * Read command and status register
     * We access these registers as one double word at offset 0x4
     */
    command_status = pci_get_dword_config(bus_id, pci_dev->device,
            pci_dev->function, PCI_HEADER_COMMAND_REG);
    pci_dev->command = command_status;
    pci_dev->status = (u16) (command_status >> 16);
    if (PCI_HEADER_GENERAL_DEVICE == (pci_dev->header & 0x3)) {
        /*
         * Its a general device. Read bars
         */
        for (i = 0; i < 6; i++)
            pci_dev->bars[i] = pci_get_dword_config(bus_id, device, function, i
                    * 4 + PCI_HEADER_BAR0);
    }
    pci_dev->irq_line = pci_get_byte_config(bus_id, device, function,
            PCI_HEADER_IRQ_LINE_REG);
    pci_dev->irq_pin = pci_get_byte_config(bus_id, device, function,
            PCI_HEADER_IRQ_PIN_REG);
    /*
     * If there is a capability list, read it
     */
    if (pci_dev->status & PCI_STATUS_CAP_LIST) {
        cap_pointer = pci_get_byte_config(pci_dev->bus->bus_id,
                                        pci_dev->device, 
                                        pci_dev->function, 
                                        PCI_HEADER_CAP_POINTER_REG);
        while (cap_pointer) {
            cap_id = pci_get_byte_config(pci_dev->bus->bus_id,
                    pci_dev->device, pci_dev->function, cap_pointer);
            if (PCI_CAPABILITY_MSI == cap_id) {
                /*
                 * We found an MSI entry. Store the information that
                 * this device supports MSI and the current offset
                 * in the PCI structure for later use
                 */
                pci_dev->msi_support = 1;
                pci_dev->msi_cap_offset = cap_pointer;
            }
            cap_pointer = pci_get_byte_config(pci_dev->bus->bus_id,
                    pci_dev->device, pci_dev->function, cap_pointer + 1);
        }
    }
    if (PCI_HEADER_PCI_BRIDGE == (pci_dev->header & 0x3)) {
        /*
         * We hit upon a PCI-to-PCI bridge. Get information on primary and secondary bus
         * from bridge-specific registers. The secondary bus is the "new" bus
         */
        primary_bus = pci_get_byte_config(bus_id, device, function,
                PCI_HEADER_PRIMARY_BUS);
        secondary_bus = pci_get_byte_config(bus_id, device, function,
                PCI_HEADER_SECONDARY_BUS);
        pci_dev->primary_bus = primary_bus;
        pci_dev->secondary_bus = secondary_bus;
    }
    else {
        pci_dev->primary_bus = 0;
        pci_dev->secondary_bus = 0;
    }
}

/*
 * Scan a bus and add the devices which we have found
 * to the list of known devices. When hitting upon a PCI-PCI bridge
 * check whether the bus behind the bridge appears in the list of
 * known busses and add it to the tail of the list if not
 * Parameter:
 * @pci_bus - the bus to be scanned
 */
static void pci_scan_bus(pci_bus_t* pci_bus) {
    u8 device;
    u8 function;
    u32 vendor_device_id;
    pci_dev_t* pci_dev;
    pci_bus_t* secondary_pci_bus;

    u8 bus_id = pci_bus->bus_id;
    for (device = 0; device <= 31; device++) {
        for (function = 0; function <= 7; function++) {
            /*
             * First read vendor id - if that is 0xffff,
             * there is no device for that dev/func
             */
            vendor_device_id = pci_get_dword_config(bus_id, device, function,
                    PCI_HEADER_VENDOR_DEVID_REG);
            if (0xffffffff != vendor_device_id) {
                /*
                 * Seems to be an existing device - set up
                 * pci_dev structure for it and read more data
                 */
                pci_dev = (pci_dev_t*) kmalloc(sizeof(pci_dev_t));
                if (0 == pci_dev) {
                    PANIC("Could not allocate memory for PCI device\n");
                    return;
                }
                pci_bus->devfunc_count++;
                pci_dev->bus = pci_bus;
                pci_scan_device(bus_id, device, function, pci_dev);
                if (PCI_HEADER_PCI_BRIDGE == (pci_dev->header & 0x3)) {
                    /*
                     * This is a bridge to another bus. Check whether this bus already exists and set
                     * it up if not
                     */
                    if (0 == pci_get_bus_for_id(pci_dev->secondary_bus)) {
                        secondary_pci_bus = (pci_bus_t*) kmalloc(
                                sizeof(pci_bus_t));
                        if (0 == secondary_pci_bus) {
                            PANIC("Could not allocate memory for pci bus\n");
                            return;
                        }
                        secondary_pci_bus->bus_id = pci_dev->secondary_bus;
                        secondary_pci_bus->devfunc_count = 0;
                        LIST_ADD_END(pci_bus_list_head, pci_bus_list_tail, secondary_pci_bus);
                    }
                }
                /*
                 * Add device structure to list
                 */
                LIST_ADD_END(pci_dev_list_head, pci_dev_list_tail, pci_dev);
                /*
                 * If we have just processed function 0, read bit 7 of the header type
                 * to find out whether the device is a multifunction device.
                 * If not, skip remaining functions to avoid duplicates
                 */
                if (0 == function) {
                    if (!(PCI_HEADER_MF_MASK & pci_dev->header)) {
                        break;
                    }
                }
            }
        }
    }
}

/*
 * This function walks the list of registered PCI devices
 * and tries to match them against the list of known chipset
 * components by calling the probe function for each of them. The result
 * is stored in the component list
 */
static void probe_chipset_components() {
    int i;
    pci_dev_t* pci_dev;
    for (i = 0; i < sizeof(chipset_components) / sizeof(pci_chipset_component_t); i++) {
        LIST_FOREACH(pci_dev_list_head, pci_dev) {
            if (chipset_components[i].probe(pci_dev)) {
                chipset_components[i].present = 1;
                break;
            }
        }
    }
}


/*
 * Initialize the PCI bus driver and perform a
 * scan of the PCI bus system to detect busses and
 * devices
 */
void pci_init() {
    pci_bus_t* pci_bus;
    /*
     * Reset lists
     */
    pci_bus_list_head = 0;
    pci_bus_list_tail = 0;
    pci_dev_list_head = 0;
    pci_dev_list_tail = 0;
    /*
     * Init spin lock to protect registers
     */
    spinlock_init(&pci_config_reg_lock);
    /*
     * Set up one bus entry - this is the entry for bus 0
     */
    pci_bus_list_head = (pci_bus_t*) kmalloc(sizeof(pci_bus_t));
    if (0 == pci_bus_list_head) {
        PANIC("No memory available for PCI bus list\n");
        return;
    }
    pci_bus_list_head->bus_id = 0;
    pci_bus_list_head->next = 0;
    pci_bus_list_head->prev = 0;
    pci_bus_list_head->devfunc_count = 0;
    pci_bus_list_tail = pci_bus_list_head;
    /*
     * Now scan first bus. This function will add additional items to
     * the list of busses in case it hits upon bridges on the bus
     * currently being scanned, so we will stay in the loop until
     * the scan does not reveal any additional busses
     */
    pci_bus = pci_bus_list_head;
    while (pci_bus) {
        pci_scan_bus(pci_bus);
        pci_bus = pci_bus->next;
    }
    /*
     * Now probe for known chipset components
     */
    probe_chipset_components();
}


/******************************************************************
 * Some functions to query PCI device configuration               *
 ******************************************************************/


/*
 * This function calls the provided callback once for
 * each registered PCI device
 * Parameter:
 * @callback - the callback to use
 */
void pci_query_all(pci_query_callback_t callback) {
    pci_dev_t* pci_dev;
    LIST_FOREACH(pci_dev_list_head, pci_dev) {
        callback(pci_dev);
    }
}

/*
 * This function calls the provided callback once for
 * each registered PCI device which matches the provided
 * base class
 * Parameter:
 * @callback - the callback to use
 * @base_class - the base class
 */
void pci_query_by_baseclass(pci_query_callback_t callback, u8 base_class) {
    pci_dev_t* pci_dev;
    LIST_FOREACH(pci_dev_list_head, pci_dev) {
        if (pci_dev->base_class==base_class)
            callback(pci_dev);
    }
}

/*
 * This function calls the provided callback once for
 * each registered PCI device which matches the provided
 * base class and sub class
 * Parameter:
 * @callback - the callback to use
 * @base_class - the base class
 * @sub_class -the sub class
 */
void pci_query_by_class(pci_query_callback_t callback, u8 base_class, u8 sub_class) {
    pci_dev_t* pci_dev;
    LIST_FOREACH(pci_dev_list_head, pci_dev) {
        if ((pci_dev->base_class==base_class) && (pci_dev->sub_class==sub_class))
            callback(pci_dev);
    }
}


/*
 * Get the PCI status register from offset 0x6 of configuration space
 * Parameter:
 * @pci_dev - the PCI device
 * Returns:
 * the value of the status register
 */
u16 pci_get_status(pci_dev_t* pci_dev) {
    u32 command_status;
    command_status = pci_get_dword_config(pci_dev->bus->bus_id, pci_dev->device,
                pci_dev->function, PCI_HEADER_COMMAND_REG);
    return (u16) (command_status >> 16);
}

/*
 * Get the PCI command register 
 * Parameter:
 * @pci_dev - the PCI device
 * Returns:
 * the value of the status register
 * 
 */
u16 pci_get_command(pci_dev_t* pci_dev) {
    u32 command_status;
    command_status = pci_get_dword_config(pci_dev->bus->bus_id, pci_dev->device,
                pci_dev->function, PCI_HEADER_COMMAND_REG);
    return (u16) (command_status);
}
 
/******************************************************************
 * The functions in this section are dealing with PCI interrupts. *
 * We can enable bus mastering DMA and MSI for individual devices *
 ******************************************************************/
 
 
/*
 * Enable the PCI bus master functionality by setting the
 * bus master configuration bit
 * Parameter:
 * @pci_dev - the device for which we want to enable bus mastering DMA
 */
void pci_enable_bus_master_dma(pci_dev_t* pci_dev) {
     /*
     * First get the current value of the command / status register
     */
    u32 value;
    value = pci_get_dword_config(pci_dev->bus->bus_id, pci_dev->device,
            pci_dev->function, PCI_HEADER_COMMAND_REG);
    /*
     * If bit 2 is already set we are fine
     */
    if (value & PCI_COMMAND_BUS_MASTER) {
        return;
    }
    MSG("Bus master DMA not yet enabled - writing configuration bit\n");
    /*
     * Now set bit 2 of the command register
     */
    value = value | PCI_COMMAND_BUS_MASTER;
    /*
     * and write back
     */
    pci_put_dword_config(pci_dev->bus->bus_id, pci_dev->device,
            pci_dev->function, PCI_HEADER_COMMAND_REG, value);
    /*
     * Did it work?
     */
    value = pci_get_dword_config(pci_dev->bus->bus_id, pci_dev->device,
            pci_dev->function, PCI_HEADER_COMMAND_REG);
    if (0 == (value & PCI_COMMAND_BUS_MASTER)) {
        PANIC("Could not set bus master configuration bit, giving up\n");
    }
    return;
}

/*
 * This function reads the MSI configuration for a given device
 * Parameter:
 * @dev - the device
 * @msi_config - the MSI config structure that we fill
 */
static void pci_get_msi_config(pci_dev_t* dev, msi_config_t* msi_config) {
    u16 msg_control;
    /*
     * See the PCI 3.0 Local bus specification, section 6.8 for a description
     * of the layout of the MSI capability
     */
    /*
     * Read the message control register first. The configuration space 
     * is built up as follows here:
     * - at offset dev->msi_cap_offset, we have the MSI capability (one byte)
     *   followed by the pointer to the next capability (one byte)
     * - the message control register is at bytes 2 and 3
     */
    msg_control = pci_get_dword_config(dev->bus->bus_id, 
                                dev->device, 
                                dev->function, dev->msi_cap_offset) >> 16;
    /*
     * Is this 64 bit MSI or 32 bit MSI?
     */
    msi_config->is64 = (msg_control & PCI_MSI_64_SUPP) / PCI_MSI_64_SUPP;
    /*
     * Is MSI enabled?
     */
    msi_config->msi_enabled = msg_control & PCI_MSI_CNTL_ENABLED;
    /*
     * Bits 4 - 6 determine how many messages we enable
     */
     msi_config->multi_msg_enabled = (msg_control >> 4) & 0x7;
    /*
     * The next dword is the message address 
     */
    msi_config->msg_address = pci_get_dword_config(dev->bus->bus_id,
                                dev->device,
                                dev->function,
                                dev->msi_cap_offset + 4);
    /*
     * If the device supports 64 bit message addresses, read the
     * upper 32 bits next, then the data register (2 bytes)
     */
    if (msi_config->is64) {
        msi_config->msg_address_upper = pci_get_dword_config(dev->bus->bus_id,
                                dev->device,
                                dev->function,
                                dev->msi_cap_offset + 8);
        msi_config->msg_data =  pci_get_word_config(dev->bus->bus_id,
                                dev->device,
                                dev->function,
                                dev->msi_cap_offset + 12);
    }
    else {
        msi_config->msg_address_upper = 0;
        msi_config->msg_data = pci_get_word_config(dev->bus->bus_id,
                                dev->device,
                                dev->function,
                                dev->msi_cap_offset + 8);
    }
}

/*
 * Write configuration information to a given device
 * Parameter:
 * @dev - the PCI device
 * @msi_config - the MSI configuration to be written
 */
static void pci_put_msi_config(pci_dev_t* dev, msi_config_t* msi_config) {
    u8 msi_offset = dev->msi_cap_offset;
    u16 msg_control;
    msg_control = pci_get_dword_config(dev->bus->bus_id, 
                                    dev->device,
                                    dev->function, 
                                    msi_offset) >> 16;
    /*
     * Adapt MSI enabled flag 
     */
    msg_control = msg_control & ~(0x1);
    msg_control = msg_control | (msi_config->msi_enabled & 0x1);
    /*
     * Adapt multi message enabled bits
     */
    msg_control = msg_control & ~(0x7 << 4);
    msg_control = msg_control | ((msi_config->multi_msg_enabled & 0x7) << 4);
    /*
     * and write message control register back
     */
    pci_put_word_config(dev->bus->bus_id, dev->device, dev->function,
                                msi_offset+2,
                                msg_control);
    /*
     * Now write message address and message data
     */
    pci_put_dword_config(dev->bus->bus_id,
                                dev->device,
                                dev->function,
                                msi_offset + 4, 
                                msi_config->msg_address);
    if (msi_config->is64) {
        pci_put_dword_config(dev->bus->bus_id,
                    dev->device,
                    dev->function,
                    msi_offset + 8, 
                    msi_config->msg_address_upper);
        pci_put_word_config(dev->bus->bus_id,
                    dev->device,
                    dev->function,
                    msi_offset + 12, 
                    msi_config->msg_data);

    }
    else {
        pci_put_word_config(dev->bus->bus_id,
                    dev->device,
                    dev->function,
                    msi_offset + 8, 
                    msi_config->msg_data);
    }
}

/*
 * Set up a device for MSI. We do not check any more that 
 * the device supports this, but assume that this has been 
 * done before
 * Parameter:
 * @pci_dev - the device
 * @vector - the vector we want to use (-1 = re-use existing vector)
 * @irq_dlv - delivery mode: 1 = fixed delivery to BSP, 2=logical, 3=lowest priority
 */
void pci_config_msi(pci_dev_t* pci_dev, int vector, int irq_dlv) {
    msi_config_t msi_config;
    u8 dest_id;
    u32 rh = 0;
    u32 dm = 0;
    u32 dlv_mode = 0;
    /*
     * Read existing configuration first
     */
    pci_get_msi_config(pci_dev, &msi_config);
    /*
     * If vector = -1, i.e. use existing one, extract
     * vector from configuration
     */
    if (-1 == vector) {
        vector = msi_config.msg_data & 0xFF;
        DEBUG("Reconfiguring vector %d\n", vector);
    }
    /*
     * If MSI is already enabled, turn it off first
     * so that we can change the configuration safely
     */
    if (1 == msi_config.msi_enabled) {
        msi_config.msi_enabled = 0;
        pci_put_msi_config(pci_dev, &msi_config);
    }
    /*
     * Set MSI enabled flag
     */
    msi_config.msi_enabled = 1;
    /*
     * We only support one vector, i.e. we set the
     * multi-message enabled bits to zero
     */
    msi_config.multi_msg_enabled = 0;
    /*
     * Build message address. This is platform
     * specific. For x86, section 10.11 of Intels
     * Software Developers manual, Volume 3 tells 
     * us the following: 
     * Bits 20-31: 0xfee - fixed value
     * Bits 0-11: depending on delivery mode
     * Bits 12-19: destination ID (as bits 63:56 of APIC redirection entry)
     */
    switch (irq_dlv) {
        case 1:
            /*
             * Fixed delivery to BSP
             */
            dest_id = cpu_get_apic_id(0) & 0xff;
            rh = 0;
            dm = 0;
            dlv_mode = 0;
            break;
        case 2:
            /*
             * Logical delivery 
             */
            dest_id = ( 1 << (vector % cpu_get_cpu_count()));
            rh = 0;
            dm = 1 << 2;
            dlv_mode = 0;
            break;
        case 3:
            /*
             * Lowest priority. Use bitmask matching all existing 
             * CPUs
             */
            dest_id = ((1 << cpu_get_cpu_count()) - 1); 
            rh = 1 << 3;
            dm = 1 << 2;
            dlv_mode = 1;
            break;
    }
    msi_config.msg_address = 0xfee00000 + (dest_id << 12) + dm + rh;
    msi_config.msg_address_upper = 0;
    /*
     * Build message data, again according to the 
     * Intel manual.
     * Bits 0-7: vector 
     * Bits 8-10: dlv_mode
     * Bit 14,15: set to zero
     */
    msi_config.msg_data = vector + (dlv_mode << 8);
    /*
     * Write configuration data back
     */
    pci_put_msi_config(pci_dev, &msi_config);
    /*
     * Set an earmark that the device is enabled for 
     * MSI
     */
    pci_dev->uses_msi = 1;
}

/*
 * Rebalance interrupts for all devices. This function is called
 * by irq_balance in the interrupt manager. It will visit all devices
 * for which MSI has been enabled earlier and reconfigure them
 * according to the actual irq delivery mode
 * Parameter:
 * @irq_dlv - the value of the kernel parameter irq_dlv, see  
 *            irq.c for a detailed description
 */
void pci_rebalance_irqs(int irq_dlv) {
    pci_dev_t* pci_dev;
    LIST_FOREACH(pci_dev_list_head, pci_dev) {
        if (1 == pci_dev->uses_msi) {
            pci_config_msi(pci_dev, -1, irq_dlv);
        }
    }     
}


/*
 * Probe for the presence of an Intel ICH9 I/O controller hub
 */
static int probe_ich9(pci_dev_t* pci_dev) {
    /*
     * We are detecting the presence of an ICH9 I/O hub by 
     * looking for the PCI-LPC bridge. According to the Intel ICH9 
     * datasheet and the Intel ICH9 specification update, this is
     * device 31, function 0 with vendor id 0x8086 and device id 
     * 0x2918
     */
    if ((31 == pci_dev->device) && (0 == pci_dev->function)) {
        if ((0x8086 == pci_dev->vendor_id) && (0x2918 == pci_dev->device_id)) {
            return 1;
        }
    }
    return 0;
}

/******************************************************************
 * The following functions probe for the presence of specific     *
 * chipset components. They are usually not called directly, but  *
 * by probe_chipset_components                                    *
 ******************************************************************/

/*
 * Probe for the presence of an Intel ICH10 I/O controller hub
 */
static int probe_ich10R(pci_dev_t* pci_dev) {
    /*
     * We are detecting the presence of an ICH10R I/O hub by 
     * looking for the PCI-LPC bridge. According to the Intel ICH9 
     * datasheet and the Intel ICH10 specification update, this is
     * device 31, function 0 with vendor id 0x8086 and device id 
     * 0x3a16
     */
    if ((31 == pci_dev->device) && (0 == pci_dev->function)) {
        if ((0x8086 == pci_dev->vendor_id) && (0x3a16 == pci_dev->device_id)) {
            return 1;
        }
    }
    return 0;
}

/*
 * Probe for the presence of an Intel PIIX3 ISA/IDE
 * chipset
 */
static int probe_piix3(pci_dev_t* pci_dev) {
    /*
     * We are looking for a multifunction device for which function
     * 0 has vendor id 0x8086 and device id 0x7000
     */
    if (0 == pci_dev->function) {
        if ((0x8086 == pci_dev->vendor_id) && (0x7000 == pci_dev->device_id)) {
            return 1;
        }
    }
    return 0;
}

/*
 * This public functions can be used by other kernel modules to
 * learn whether a given chipset component (identified by the
 * chipset componend ID) has been detected.
 * Parameter:
 * @component_id - the chipset component id
 * Returns:
 * 1 if the component is present, 0 otherwise
 *
 */
int pci_chipset_component_present(int component_id) {
    int i;
    for (i = 0; i < sizeof(chipset_components) / sizeof(pci_chipset_component_t);  i++) {
        if (chipset_components[i].component_id == component_id) {
            return chipset_components[i].present;
        }
    }
    return 0;
}


/******************************************************************
 * The functions below this line are for debugging purpose only   *
 * and are meant to be used by the internal debugger              *
 ******************************************************************/


/*
 * Return a short description for the device,
 * based on class codes
 * Parameters:
 * @base_class - base class of device
 * @sub_class - sub class of device
 * @prog_if - programming interface of device
 * Return value:
 * a string describing the type of the device
 */
static char* get_desc_for_cc(u8 base_class, u8 sub_class, u8 prog_if) {
    int i = 0;
    while (i < PCI_CLASS_CODE_SIZE) {
        if ((pci_class_codes[i].base_class == base_class)
                && (pci_class_codes[i].sub_class == sub_class)
                && ((pci_class_codes[i].prog_if == prog_if)
                        || (pci_class_codes[i].prog_if == 0xff))) {
            return pci_class_codes[i].desc;
        }
        i++;
    }
    return "Unknown";
}

/*
 * Return the name of a capability
 */
char* get_capability_name(u8 capability) {
    int i = 0;
    while (i < CAPABILITY_LIST_SIZE) {
        if (capabilities[i].id == capability) {
            return capabilities[i].name;
        }
        i++;
    }
    return "Unknown";

}

/*
 * Format output for a base address register (BAR)
 * Parameters:
 * @bar - the BAR content
 * @nr - the number of the register
 */
static void pci_print_bar(u32 bar, int nr) {
    u32 address;
    u32 type;
    u32 io;
    io = bar & BAR_IO_SPACE;
    if (!io) {
        address = bar & 0xfffffff0;
        type = bar & BAR_TYPE;
        PRINT("BAR%d=%x (MEM,type=%h)    ", nr, address, type);
    }
    else {
        address = bar & 0xfffffffc;
        PRINT("BAR%d=%x (I/O)            ", nr, address);
    }
    if (0==((nr+1) % 2))
        PRINT("\n");
}


/*
 * Print details for a PCI device
 * Parameter:
 * @pci_dev - the device
 */
static void pci_print_device_details(pci_dev_t* pci_dev) {
    int i;
    u32 bar32;
    u32 command_status;
    u16 command;
    u16 status;
    u8 cap_pointer;
    u8 cap_id;
    cls(0);
    PRINT("Details on device %h:%h.%h\n", pci_dev->bus->bus_id, pci_dev->device,
            pci_dev->function);
    PRINT("------------------------------------\n");
    PRINT("Vendor ID: %w  Device ID: %w\n", pci_dev->vendor_id,
            pci_dev->device_id);
    PRINT("Class code: base=%h, subclass=%h, programming interface=%h\n",
            pci_dev->base_class, pci_dev->sub_class, pci_dev->prog_if);
    PRINT("Class description: %s\n", get_desc_for_cc(pci_dev->base_class,
                    pci_dev->sub_class, pci_dev->prog_if));
    PRINT("MSI:             %d\n", pci_dev->msi_support);
    PRINT("IRQ line:        %h  IRQ pin:       %h\n", pci_dev->irq_line,
            pci_dev->irq_pin);
    command_status = pci_get_dword_config(pci_dev->bus->bus_id, pci_dev->device,
            pci_dev->function, PCI_HEADER_COMMAND_REG);
    command = command_status;
    status = (u16) (command_status >> 16);
    PRINT("Header type:     %h  Status:        %w     Command: %w\n",
            pci_dev->header, status, command);
    if (PCI_HEADER_PCI_BRIDGE == (pci_dev->header & 0x3)) {
        PRINT("Primary bus: %h  Secondary bus: %h\n",
                pci_dev->primary_bus, pci_dev->secondary_bus);
    }
    if (PCI_HEADER_GENERAL_DEVICE == (pci_dev->header & 0x3)) {
        for (i = 0; i < 6; i++) {
            bar32 = pci_get_dword_config(pci_dev->bus->bus_id, pci_dev->device,
                    pci_dev->function, PCI_HEADER_BAR0 + 4 * i);
            pci_print_bar(bar32, i);
        }
    }
    PRINT("Access via I/O space enabled: %h\n", (pci_dev->command
                    & PCI_COMMAND_IO_ENABLED) / PCI_COMMAND_IO_ENABLED);
    PRINT("Access via memory space enabled: %h\n", (pci_dev->command
                    & PCI_COMMAND_MEM_ENABLED) / PCI_COMMAND_MEM_ENABLED);
    PRINT("Capability list present: %h\n", (pci_dev->status
                    & PCI_STATUS_CAP_LIST) / PCI_STATUS_CAP_LIST);
    if (pci_dev->status & PCI_STATUS_CAP_LIST) {
        PRINT("ID      Name\n");
        PRINT("---------------------\n");
        cap_pointer = pci_get_byte_config(pci_dev->bus->bus_id,
                                        pci_dev->device, 
                                        pci_dev->function, 
                                        PCI_HEADER_CAP_POINTER_REG);
        while (cap_pointer) {
            cap_id = pci_get_byte_config(pci_dev->bus->bus_id,
                    pci_dev->device, pci_dev->function, cap_pointer);
            PRINT("%h      %s\n", cap_id, get_capability_name(cap_id));
            cap_pointer = pci_get_byte_config(pci_dev->bus->bus_id,
                    pci_dev->device, pci_dev->function, cap_pointer + 1);
        }
    }
    else {
        kprintf("This device does not implement any capabilities\n");
    }                    
    PRINT("Hit any key to return to list\n");
    early_getchar();
}

/*
 * Print some information on a given device
 * Parameter:
 * @nr - the position on the screen
 * @pci_dev - the device to be printed
 */
static void pci_print_device_summary(int nr, pci_dev_t* pci_dev) {
    PRINT("%h  %h   %h.%h     %w    %w    %w   %w   %w  %s\n", nr,
            pci_dev->bus->bus_id, pci_dev->device, pci_dev->function,
            pci_dev->vendor_id, pci_dev->device_id, pci_dev->base_class,
            pci_dev->sub_class, pci_dev->prog_if, get_desc_for_cc(
                    pci_dev->base_class, pci_dev->sub_class, pci_dev->prog_if));
}

/*
 * List all PCI devices on the busses scanned so far
 */
void pci_list_devices() {
    pci_dev_t* pci_dev;
    pci_dev = pci_dev_list_head;
    int nr = 1;
    int i, j;
    char input;
    /*
     * We first print a list of detected chipset components
     */
    cls(0);
    PRINT("Known chipset components that I could detect: \n");
    PRINT("-------------------------------------------------------------------------\n");
    j = 0;
    for (i = 0; i < sizeof(chipset_components) / sizeof(pci_chipset_component_t); i++) {
        if (chipset_components[i].present) {
            j = 1; 
            PRINT("%x        %s\n", chipset_components[i].component_id, chipset_components[i].long_name);
        }
    }
    if (0 == j) {
        PRINT("None\n");
    }
    PRINT("-------------------------------------------------------------------------\n");
    PRINT("PCI devices: \n");
    /*
     * This is an array containing pointers to the devices
     * listed on the current page. Used to branch to the details page
     * In particular, the entry at index 1 is a pointer to the first
     * device on the current page
     */
    pci_dev_t * shown_devices[DEVICE_LIST_PAGE_SIZE + 1];
    PRINT("         Device/   Vendor  Device  Base   Sub    Prog  Class      \n");
    PRINT("Nr  Bus  Function  ID      ID      Class  Class  If    Description\n");
    PRINT("-------------------------------------------------------------------------\n");
    while (pci_dev) {
        pci_print_device_summary(nr, pci_dev);
        shown_devices[nr] = pci_dev;
        pci_dev = pci_dev->next;
        nr++;
        if ((nr > DEVICE_LIST_PAGE_SIZE) || (!pci_dev)) {
            /* Reached end of page */
            nr--;
            if (pci_dev)
                PRINT(
                        "Hit a number to display details or any other key to proceed to next page\n");
            else
                PRINT(
                        "Hit a number to display details or any other key to return to prompt\n");
            input = early_getchar();
            if ((input >= '1') && (input <= ('1' + nr))) {
                /* Its a number between 1 and the number of displayed items */
                pci_print_device_details(shown_devices[input - '0']);
                /* Reset pointer so that we start printing at the first device on
                 * the current page again
                 */
                pci_dev = shown_devices[1];
            }
            /* If there are entries left,  set up header for next page */
            if (pci_dev) {
                nr = 1;
                cls(0);
                PRINT("         Device/   Vendor  Device  Base   Sub    Prog            \n");
                PRINT("Nr  Bus  Function  ID      ID      Class  Class  If    Description\n");
                PRINT("-------------------------------------------------------------------------\n");
            }
        }
    }
}
