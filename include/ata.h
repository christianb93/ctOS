/*
 * ata.h
 *
 * Contains definitions related to the IDE / ATA protocol which are
 * used by both the PATA driver and the AHCI driver
 */

#ifndef _ATA_H_
#define _ATA_H_

/*
 * Master/slave code
 */
#define ATA_DEVICE_MASTER 0
#define ATA_DEVICE_SLAVE 1

/*
 * Some bitmasks
 */
#define IDE_STATUS_DRDY 0x40
#define IDE_STATUS_BSY 0x80
#define IDE_STATUS_DRQ 0x8
#define IDE_STATUS_ERR 0x1
#define IDE_DEVICE_CONTROL_SRST 0x4
#define IDE_DEVICE_CONTROL_NIEN 0x2
#define IDE_DEVICE_SELECT 0x10
#define IDE_DEVICE_LBA (1 << 6)
#define IDE_DEVICE_OBS1 (1 << 5)
#define IDE_DEVICE_OBS2 (1 << 7)

/*
 * Block size (i.e. sector size)
 */
#define ATA_BLOCK_SIZE 512

/*
 * Some commands
 */
#define IDE_IDENTIFY_DEVICE 0xec
#define IDE_IDENTIFY_PACKET_DEVICE 0xa1
#define IDE_READ_SECTORS 0x20
#define IDE_READ_DMA 0xc8
#define IDE_WRITE_DMA 0xca
#define IDE_READ_DMA_EXT 0x25
#define IDE_WRITE_DMA_EXT 0x35
#define IDE_READ_SECTORS_EXT 0x24

/*
 * Some fields in the output of IDENTIFY DEVICE
 */
#define IDE_IDENTIFY_DEVICE_CAP_WORD 49
#define IDE_IDENTIFY_DEVICE_CAP_LBA (1 << 9)
#define IDE_IDENTIFY_DEVICE_CAP_DMA (1 << 8)


#endif /* _ATA_H_ */
