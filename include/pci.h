/*
 * pci.h
 *
 */

#ifndef _PCI_H_
#define _PCI_H_

#include "ktypes.h"


/*
 * Data structure to represent a PCI bus
 */
typedef struct _pci_bus_t {
    u8 bus_id;
    int devfunc_count;
    struct _pci_bus_t* next;
    struct _pci_bus_t* prev;
} pci_bus_t;

/* Entry in a table of capabilities */
typedef struct {
    u8 id;
    char* name;
} capability_t;


/*
 * This is the internal data structure which we use to
 * represent a PCI device
 */
typedef struct _pci_dev_t {
    pci_bus_t* bus;
    u8 device;
    u8 function;
    u16 vendor_id;
    u16 device_id;
    u8 base_class;
    u8 sub_class;
    u8 prog_if;
    u8 irq_line;
    u8 irq_pin;
    u8 header;
    u16 status;
    u16 command;
    u8 msi_support;
    u8 msi_cap_offset;
    u8 uses_msi;
    /* These fields are only valid for bridges */
    u8 primary_bus;
    u8 secondary_bus;
    /* These fields are only valid for type 0 - generic device */
    u32 bars[6];
    /*
     * Pointer to next and previous device in list
     */
    struct _pci_dev_t* next;
    struct _pci_dev_t* prev;
} pci_dev_t;

/*
 * We use this structure to maintain a table of valid classes
 */
typedef struct {
    u8 base_class;
    u8 sub_class;
    u8 prog_if;
    char* desc;
} pci_class_t;

/*
 * This structure represents an MSI configuration
 */
typedef struct {
    u32 msg_address;
    u32 msg_address_upper;
    u16 msg_data;
    int msi_enabled;
    u8 multi_msg_enabled;
    int is64;
} msi_config_t;


/*
 * I/O register to use for access
 * to configuration space
 */
#define PCI_CONFIG_ADDRESS 0xcf8
#define PCI_CONFIG_DATA 0xcfc


/*
 * Offsets of some standard configuration space registers
 */

/* This register contains vendor id in bits 0-15
 * and device id in bits 16-31
 */
#define PCI_HEADER_VENDOR_DEVID_REG 0x0
/* The header type. Bits 0-1 contain
 * the type of the device, bit 7 indicates
 * whether the device is a multifunction device
 */
#define PCI_HEADER_TYPE_REG 0xe
/* The next three bytes together
 * form the classcode, made up of
 * base class, subclass and programming
 * interface
 */
#define PCI_HEADER_BASECLASS_REG 0xb
#define PCI_HEADER_SUBCLASS_REG 0xa
#define PCI_HEADER_PROGIF_REG 0x9
/* Capability pointer (one byte) */
#define PCI_HEADER_CAP_POINTER_REG 0x34
/* Interrupt line (one byte) */
#define PCI_HEADER_IRQ_LINE_REG 0x3c
/* Interrupt pin (one byte), 0x1 is PINA# */
#define PCI_HEADER_IRQ_PIN_REG 0x3d
/* 16 bit status register */
#define PCI_HEADER_STATUS_REG 0x6
/* 16 bit command register */
#define PCI_HEADER_COMMAND_REG 0x4
/* Offset of first BAR */
#define PCI_HEADER_BAR0 0x10

/* These register are only valid for bridges (header=1) */
/* Primary bus: this is the bus "closer" to the CPU (one byte) */
#define PCI_HEADER_PRIMARY_BUS 0x18
/* Bus to which the bridge connects */
#define PCI_HEADER_SECONDARY_BUS 0x19

/* Possible values for the header field */
#define PCI_HEADER_GENERAL_DEVICE 0x0
#define PCI_HEADER_PCI_BRIDGE 0x1
#define PCI_HEADER_CARDBUS_BRIDGE 0x2
#define PCI_HEADER_MF_MASK 0x80

/* Flags/masks for command and status register */
/* I/O space access enabled */
#define PCI_COMMAND_IO_ENABLED 0x1
/* Memory space access enabled */
#define PCI_COMMAND_MEM_ENABLED 0x2
/* Bus master enabled */
#define PCI_COMMAND_BUS_MASTER 0x4
/* Interrupt disable */
#define PCI_COMMAND_IRQ_DISABLE 0x400
/* Capability list present */
#define PCI_STATUS_CAP_LIST 0x10

/* Some capabilities */
#define PCI_CAPABILITY_MSI 0x5
#define PCI_CAPABILITY_MSIX 0x11

/*
 * MSI specific stuff
 */
#define PCI_MSI_CNTL_ENABLED 0x1
#define PCI_MSI_MASKING_SUPP (1 << 8) 
#define PCI_MSI_64_SUPP (1<<7)
 

/* Bit masks for BARs */
/* This flag determines whether the device
 * is mapped into I/O space or memory space
 */
#define BAR_IO_SPACE 0x1
/* These two bits define whether the device
 * is mapped into 32 bit memory space (0x0) or 64 bit memory space (0x2)
 * It has no meaning for I/O mapped devices
 */
#define BAR_TYPE 0x6

/*
 * When printing the list of devices, this is the number
 * of lines which will be printed on one screen
 */
#define DEVICE_LIST_PAGE_SIZE 8

/*
 * Some base and sub classes
 */
#define PCI_BASE_CLASS_MASS_STORAGE 0x1
#define PATA_SUB_CLASS 0x1
#define AHCI_SUB_CLASS 0x6
#define PCI_BASE_CLASS_NIC 0x2
#define ETH_SUB_CLASS 0x0

/*
 * Chipset components that we know
 */
typedef struct {
    u32 component_id;
    char* short_name;
    char* long_name;
    int present;
    int (*probe)(pci_dev_t* pci_dev);
} pci_chipset_component_t;

/*
 * Some values for the component ID
 */
#define PCI_CHIPSET_COMPONENT_ICH9      0x1
#define PCI_CHIPSET_COMPONENT_ICH10R    0x2
#define PCI_CHIPSET_COMPONENT_PIIX3     0x3
#define PCI_CHIPSET_COMPONENT_PIIX4     0x4

/*
 * A callback function for PCI query functions
 */
typedef void (*pci_query_callback_t)(const pci_dev_t*);

/*
 * Public interface
 */
void pci_init();
void pci_list_devices();
void pci_query_all(pci_query_callback_t callback);
void pci_query_by_baseclass(pci_query_callback_t callback, u8 base_class);
void pci_query_by_class(pci_query_callback_t callback, u8 base_class, u8 sub_class);
u16 pci_get_status(pci_dev_t* pci_dev);
u16 pci_get_command(pci_dev_t* pci_dev);
void pci_enable_bus_master_dma(pci_dev_t* pci_dev);
void pci_config_msi(pci_dev_t* pci_dev, int vector, int irq_dlv);
void pci_rebalance_irqs(int irq_dlv);
int pci_chipset_component_present(int component_id);

#endif /* _PCI_H_ */
