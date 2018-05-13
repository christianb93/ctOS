/*
 * hd.h
 *
 * Common data structures and functions used by both PATA and AHCI controller, mostly centered around
 * request queues and their usage
 */

#ifndef _HD_H_
#define _HD_H_

#include "lib/sys/types.h"
#include "drivers.h"
#include "pm.h"

#define HD_QUEUE_SIZE 128

typedef struct {
    dev_t minor_device;                  // minor device number of device on which we operate
    u64 first_block;                     // start of read/write operation
    ssize_t blocks;                      // blocks to read/write
    int rw;                              // 0 = read, 1 = write
    u32 buffer;                          // address of buffer (virtual address)
    int* rc;                             // used to transfer error codes between IR handler and mainline
    semaphore_t* semaphore;              // used to wake up issuing thread once request completes
    u32 data;                            // data container used to transfer driver specific data container
    int task_id;                         // the task that put the request into the queue
    int status;                          // status - for debugging only
    int submitted_by_irq;                // set if the request was submitted by IRQ handler - for debugging only
} hd_request_t;


typedef struct _hd_request_queue_t {
    u32 head;                            // head of circular buffer
    u32 tail;                            // tail of circular buffer
    semaphore_t slots_available;         // number of available slots
    int device_busy;                     // flag to indicate whether device is busy
    spinlock_t device_lock;              // synchronize access to device and protect busy flag
    void (*submit_request)
      (struct _hd_request_queue_t* queue,
       hd_request_t* request);           // submit a request
    void (*complete_request)
      (struct _hd_request_queue_t* queue,
       hd_request_t* request);           // complete a request
    void (*prepare_request)
      (struct _hd_request_queue_t* queue,
       hd_request_t* request);           // prepare a request
    ssize_t chunk_size;                  // maximum number of sectors per operation
    u32 block_size;                      // size of a sector (typically 512 bytes)
    hd_request_t queue[HD_QUEUE_SIZE];   // actual circular buffer
    u32 processed_blocks;                // number of actually processed blocks - for statistics
} hd_request_queue_t;



/*
 * Partition table entry as it is
 * stored in the master boot record
 */
typedef struct {
    u8 bootable;
    u8 chs_start[3];
    u8 type;
    u8 chs_end[3];
    u32 first_sector;
    u32 sector_count;
}__attribute__ ((packed)) part_table_entry_t;

/*
 * A primary or logical partition on
 * a drive. This is the way how we handle
 * partitions internally, independent of the actual
 * representation in the MBR
 */
typedef struct {
    int used;                            // is this entry in the list of partitions used
    u64 first_sector;                    // first sector of partition
    u64 last_sector;                     // last sector of partition
} hd_partition_t;

/*
 * A master boot record
 */
typedef struct {
    u32 bootloader[110];
    u32 signature;
    short unused;
    part_table_entry_t partition_table[4];
    u16 magic;
}__attribute__ ((packed)) mbr_t;

#define GPT_SIGNATURE 0x5452415020494645ULL
#define GPT_GUID_LENGTH 16
#define GPT_PART_NAME_LENGTH 72


/*
 * A GPT header
 */
typedef struct {
    u64 signature;
    u32 revision;
    u32 header_size;
    u32 chksum_header;
    u32 reserved;
    u64 current_lba;
    u64 backup_lba;
    u64 first_usable_lba;
    u64 last_usable_lba;
    u8  disk_guid[GPT_GUID_LENGTH];
    u64 part_table_first_lba; 
    u32 part_table_entries;
    u32 part_table_entry_size;
    u32 chksum_part_table;
}__attribute__ ((packed)) gpt_header_t;

/*
 * A GPT entry
 */
typedef struct {
    char part_type_guid[GPT_GUID_LENGTH];
    char part_guid[GPT_GUID_LENGTH];
    u64  first_lba;
    u64  last_lba;
    u64  attributes;
    char part_name[GPT_PART_NAME_LENGTH];
}__attribute__ ((packed)) gpt_entry_t;

/*
 * Request type (read or write)
 */
#define HD_READ 0
#define HD_WRITE 1

/*
 * Status of a request - actually used for debugging only
 */
#define HD_REQUEST_QUEUED 0
#define HD_REQUEST_PENDING 1

#define MBR_MAGIC_COOKIE 0xaa55
#define PART_BOOTABLE  0x80
#define PART_TYPE_EMPTY  0x0
#define PART_TYPE_FAT16  0x4
#define PART_TYPE_EXTENDED  0x5
#define PART_TYPE_VFAT  0x6
#define PART_TYPE_NTFS  0x7
#define PART_TYPE_MINIX  0x81
#define PART_TYPE_LINUX_SWAP  0x82
#define PART_TYPE_LINUX_NATIVE 0x83
#define PART_TYPE_WIN95_EXT_LBA 0xf
#define PART_TYPE_WIN95_FAT32_LBA 0xc
#define PART_TYPE_GPT 0xee


int hd_rw(hd_request_queue_t* request_queue, u32 sectors, u64 first_sector, int rw, void* buffer, minor_dev_t minor);
void hd_handle_irq(hd_request_queue_t* queue, int rc);
int hd_read_partitions(hd_partition_t* partitions, minor_dev_t minor,
        int (*read_sector)(minor_dev_t minor, u64 lba,  void* buffer),
        int table_size);
void hd_fix_ata_string(char* string, int length);

#endif /* _HD_H_ */
