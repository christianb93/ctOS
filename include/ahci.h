/*
 * ahci.h
 *
 * Declarations for the AHCI driver
 */

#ifndef _AHCI_H_
#define _AHCI_H_

#include "lib/sys/types.h"
#include "pci.h"
#include "drivers.h"
#include "hd.h"
#include "ata.h"

/*
 * Forward declaration
 */
struct _ahci_cntl_t;

/*
 * This structure describes the memory mapped register set
 * of an AHCI port. Please see the publicly available AHCI
 * specification for a description of the individual fields -
 * the variable names match the register names used in the
 * specification
 */
typedef struct {
    u32 pxclb;
    u32 pxclbu;
    u32 pxfb;
    u32 pxfbu;
    u32 pxis;
    u32 pxie;
    u32 pxcmd;
    u32 reserved0;
    u32 pxtfd;
    u32 pxsig;
    u32 pxssts;
    u32 pxsctl;
    u32 pxserr;
    u32 pxsact;
    u32 pxci;
} __attribute__ ((packed)) ahci_port_regs_t;

/*
 * This is an AHCI PRDT entry
 */
typedef struct {
    u32 base_address;
    u32 base_address_upper;
    u32 reserved0;
    u32 dbc :22;
    u32 reserved1 :9;
    u32 i :1;
}__attribute__ ((packed)) ahci_prd_t;

/*
 * This structure describes a host to device
 * register FIS
 */
typedef struct {
    u8 fis_type;
    u8 pm :4;
    u8 reserved0 :3;
    u8 c :1;
    u8 command;
    u8 feature;
    u8 lba_low;
    u8 lba_mid;
    u8 lba_high;
    u8 device;
    u8 lba_low_ext;
    u8 lba_mid_ext;
    u8 lba_high_ext;
    u8 feature_ext;
    u8 sector_count;
    u8 sector_count_ext;
    u8 reserved1;
    u8 device_control;
    u8 reserved2[4];
}__attribute__ ((packed)) h2d_register_fis_t;

/*
 * A command header
 */
typedef struct {
    u8 cfisl :5;
    u8 atapi :1;
    u8 write :1;
    u8 prefetch :1;
    u8 reset :1;
    u8 bist :1;
    u8 c :1;
    u8 reserved0 :1;
    u8 pmp :4;
    u16 prdtl;
    u32 prdbc;
    u32 command_table_base;
    u32 command_table_base_upper;
    u32 reserved1[4];
}__attribute__ ((packed)) ahci_command_header_t;

/*
 * Number of entries in the PRDT within one command table
 */
#define AHCI_PRDT_COUNT 8200

/*
 * A command table. Note that according to the specification,
 * we need to reserve 64 bytes for the command FIS
 * We use only 8200 of the up to 65536 possible PRD entries
 * as this is sufficient to transfer the maximum of 65536 sectors
 * (i.e. 32 MB) if each entry describes an area which is 4096 bytes long
 * (taking partial regions into account)
 */
typedef struct {
    h2d_register_fis_t cfis;
    u8 reserved1[64 - sizeof(h2d_register_fis_t)];
    u8 atapi_cmd[16];
    u8 reserved[0x30];
    ahci_prd_t prd[AHCI_PRDT_COUNT];
}__attribute__ ((packed)) ahci_command_table_t;

/*
 * Maximum number of partitions per drive, including
 * partition 0 (the raw device)
 */
#define AHCI_MAX_PARTITIONS 16

/*
 * An AHCI port
 */
typedef struct _ahci_port_t {
    struct _ahci_cntl_t* ahci_cntl;                           // the controller to which we are attached
    int index;                                                // the port number (0-31) within the controller
    ahci_port_regs_t* regs;                                   // memory mapped register set of the port
    ahci_command_header_t* command_list;                      // list of 32 command slots (this is where PxCLB will point to)
    ahci_command_table_t* command_tables;                     // a list of HD_QUEUE_SIZE command tables
    char model[41];                                           // model string as returned by the IDENTIFY DEVICE command
    u8* received_fis;                                         // pointer to received FIS data area
    struct _ahci_port_t* next;                                // next in list
    struct _ahci_port_t* prev;                                // previous in list
    minor_dev_t minor;                                        // minor device number of the port (i.e. partition 0)
    hd_partition_t partitions[AHCI_MAX_PARTITIONS+1];         // partitions found on the device attached to this port
    hd_request_queue_t* request_queue;                        // request queue used for this port
} ahci_port_t;

/*
 * This structure describes an AHCI controller
 */
typedef struct _ahci_cntl_t {
    u32 ahci_base_address;                                    // start address of AHCI register set in virtual memory
    int sclo;                                                 // is CLO supported?
    int irq;                                                  // interrupt vector for which we have registered
    struct _ahci_cntl_t* next;                                // next in list
    struct _ahci_cntl_t* prev;                                // previous in list
    /*
     * The pointers below this line
     * are pointers to the global
     * registers
     */
    u32* cap;                                                 // Capability register
    u32* ghc;                                                 // global host control register
    u32* is;                                                  // interrupt status register
    u32* pi;                                                  // ports implemented register
} ahci_cntl_t;





/*
 * Constants for the register set
 * The first constants describes where the register
 * set of one port starts
 */
#define AHCI_OFFSET_PORT(x)  ( 0x100 + x*0x80)
#define AHCI_REGISTER_SET_SIZE (0x100 + 32*0x80-1)

/*
 * Number of entries in a command list
 */
#define AHCI_COMMAND_LIST_ENTRIES 32

/*
 * Size of received FIS structure
 */
#define AHCI_RECEIVED_FIS_SIZE 256

/*
 * Signatures
 */
#define AHCI_SIG_ATA 0x101
#define AHCI_SIG_ATAPI 0xeb140101


/*
 * Some offsets
 */
#define AHCI_CAP 0x0
#define AHCI_GHC 0x4
#define AHCI_IS 0x8
#define AHCI_PI 0xc
#define AHCI_PXSSTS 0x28
#define AHCI_PXCMD 0x18
#define AHCI_PXSIG 0x24
#define AHCI_PXFB 0x8
#define AHCI_PXCLB 0x0
#define AHCI_PXCI 0x38
#define AHCI_PXSERR 0x30
#define AHCI_PXTFD 0x20
#define AHCI_PXIS 0x10
#define AHCI_PXCLBU 0x4
#define AHCI_PXFBU 0xc
#define AHCI_PXSCTL 0x2c
#define AHCI_PXSACT 0x34

/*
 * Some bits in the GHC register
 */
#define AHCI_GHC_ENABLED (1 << 31)
#define AHCI_GHC_IE (1 << 1)

/*
 * Some bits in the port command / status register
 */
#define PXCMD_IS_ATAPI (1 << 24)
#define PXCMD_ST 0x1
#define PXCMD_CR (1 << 15)
#define PXCMD_FR (1<<14)
#define PXCMD_FRE (1<<4)
#define PXCMD_SUD 0x2
#define PXCMD_POD 0x4
#define PXCMD_CLO 0x8


/*
 * Bitmasks for the PXSSTS register
 */
#define PXSSTS_DET 0xf

/*
 * FIS type host to device
 */
#define FIS_TYPE_H2D 0x27

/*
 * Bits in PXSSTS.DET
 */
#define PXSSTS_DET_PRESENT 0x1
#define PXSSTS_DET_PHY 0x2


/*
 * Maximum number of ports which we can take care of
 */
#define AHCI_MAX_PORTS 4

/*
 * Chunk size. This is 1 << 16, as the sector cound register only allows
 * us to read up to 65536 vectors per request
 */
#define AHCI_CHUNK_SIZE 65536

/*
 * Timeouts for various operations (all values in milliseconds)
 */
#define AHCI_TIMEOUT_STOP_CMD 1000
#define AHCI_TIMEOUT_STOP_FIS 1000
#define AHCI_TIMEOUT_START_CMD 1000
#define AHCI_TIMEOUT_IDLE 1000


void ahci_init();
void ahci_print_ports();
char* ahci_drive_name(int n);
void ahci_do_tests();
void ahci_print_queue();
u32 ahci_processed_kbyte();

#endif /* _AHCI_H_ */
