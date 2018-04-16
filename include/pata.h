/*
 * pata.h
 *
 * Declarations and constants for the PATA driver
 */

#ifndef _PATA_H_
#define _PATA_H_

#include "drivers.h"
#include "hd.h"
#include "mm.h"
#include "tests.h"
#include "ata.h"


/*
 * This structure describes a PCI IDE controller. Note the variable
 * used in this and most of the following structures. We keep these
 * structures in a statically allocated table and this flag tells us
 * whether the entry in the table is used or available
 */
typedef struct {
    int used;                          // is this controller slot used?
    u32 bus_master_base;               // base address of bus master registers in I/O space
} pata_cntl_t;

/*
 * A channel of a bus master PCI IDE controller
 */
typedef struct {
    u32 bus_master_command;            // address of bus master command register in I/O space
    u32 bus_master_status;             // address of bus master status register in I/O space
    u32 bus_master_prdt;               // address of bus master PRDT register in I/O space
    int operating_mode;                // Operating mode (1 = native, 0 = compatibility)
    u32 ata_command_block;             // Base address of command block in I/O space
    u32 ata_alt_status;                // address of alternate status register in I/O space
    int vector;                        // IRQ vector used for this channel
    u32 used;                          // channel is in use
} pata_channel_t;



/*
 * An actual drive attached to a channel either as master or slave
 */
typedef struct {
    int master_slave;                  // Master (0) or slave (1) on channel
    char serial[21];                   // Serial number + trailing 0
    char model[41];                    // Model number + trailing 0
    int lba_long;                      // 48 bit LBA supported
    int used;                          // is this drive present
} pata_drive_t;

/*
 * A physical region descriptor table entry
 * for bus master DMA transfer
 */
typedef struct  {
    u32 region_base;
    u16 region_size;
    u8 reserved;
    u8 eot;
} __attribute__ ((packed)) pata_dma_prd_t;



/*
 * Maximum number of controller supported
 */
#define PATA_MAX_CNTL 4
/*
 * Partitions per drive
 */
#define PATA_PART_DRIVE 16



/*
 * Ports in legacy mode
 */
#define IDE_LEGACY_PRIMARY_DATA_REGISTER 0x1f0
#define IDE_LEGACY_PRIMARY_ALT_STATUS_REGISTER 0x3f6
#define IDE_LEGACY_SECONDARY_DATA_REGISTER 0x170
#define IDE_LEGACY_SECONDARY_ALT_STATUS_REGISTER 0x376


/*
 * Offsets of registers to data register (command block)
 */
#define IDE_DATA_REGISTER 0x0
#define IDE_SECTOR_COUNT_REGISTER 0x2
#define IDE_LBA_LOW_REGISTER 0x3
#define IDE_LBA_MID_REGISTER 0x4
#define IDE_LBA_HIGH_REGISTER 0x5
#define IDE_DEVICE_REGISTER 0x6
#define IDE_COMMAND_REGISTER 0x7
#define IDE_ERROR_REGISTER 0x1


/*
 * Operating modes for PCI IDE drives
 */
#define IDE_MODE_NATIVE 1
#define IDE_MODE_COMPAT 0

/*
 * Masks to determine operating mode from programming interface
 */
#define IDE_MODE_PRIMARY 1
#define IDE_MODE_SECONDARY 4
/*
 * Offsets of bus master registers
 */
#define IDE_BUS_MASTER_STATUS_PRIMARY 0x2
#define IDE_BUS_MASTER_STATUS_SECONDARY 0xa
#define IDE_BUS_MASTER_COMMAND_PRIMARY 0x0
#define IDE_BUS_MASTER_COMMAND_SECONDARY 0x8
#define IDE_BUS_MASTER_PRDT_PRIMARY 0x4
#define IDE_BUS_MASTER_PRDT_SECONDARY 0xc

/*
 * Bit masks for bus master status register
 */
#define BMS_MASTER_DMA_CAPABLE (1 << 5)
#define BMS_SLAVE_DMA_CAPABLE (1 << 6)
#define BMS_ERROR 0x2
#define BMS_INT 0x4

/*
 * Bit masks for bus master command register
 */
#define BMC_WRITE 0x8
#define BMC_START 0x1

/*
 * Legacy interrupts
 */
#define IDE_LEGACY_IRQ_PRIMARY 14
#define IDE_LEGACY_IRQ_SECONDARY 15
/*
 * Request queue parameters
 */

#define PATA_CHUNK_SIZE ((1<<16) - 2*(MM_PAGE_SIZE / ATA_BLOCK_SIZE))
#define PATA_PRDT_COUNT ((1<<16)*ATA_BLOCK_SIZE/MM_PAGE_SIZE)

/*
 * EOT flag in PRDT entry
 */
#define DMA_PRD_EOT 0x80

/*
 * Timeouts
 */
#define PATA_TIMEOUT_PROBE_SELECT 100000
#define PATA_TIMEOUT_PROBE_IDLE 10000
#define PATA_TIMEOUT_RESET 1000000
#define PATA_TIMEOUT_IDLE 10000

void pata_init();
char* pata_drive_name(int n);
u32 pata_processed_kbyte();
void pata_print_devices();
void pata_print_queue();
#ifdef DO_PATA_TEST
void pata_do_tests();
#endif

#endif /* _PATA_H_ */
