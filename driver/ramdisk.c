/*
 * ramdisk.c
 *
 * Driver module for a RAM disk
 */

#include "ramdisk.h"
#include "mm.h"
#include "debug.h"
#include "dm.h"
#include "lib/string.h"

static int have_ramdisk = 0;
static u32 ramdisk_start = 0;
static u32 ramdisk_end = 0;

static char* __module = "RAMDSK";
/*
 * This is the block device operations structure which we use
 */
static blk_dev_ops_t ramdisk_ops;

/*
 * Static utility functions to validate a minor device
 * Parameters:
 * @minor - the minor device number
 * Return value:
 * ENODEV if the device does not exit or is invalid
 * 0 if device is valid
 */
static int validate_minor_dev(minor_dev_t minor) {
    if (0==have_ramdisk) {
        ERROR("No ramdisk registered\n");
        return ENODEV;
    }
    if (minor!=0) {
        ERROR("Open called with invalid minor device %x\n", minor);
        return ENODEV;
    }
    return 0;
}

/*
 * Implementations of open and close. These functions will only check
 * whether the minor number is valid and return
 * Parameter:
 * @minor - minor device number of device to be opened and closed
 * Return value:
 * 0 upon success
 * ENODEV if device is not valid
 */
static int ramdisk_close(minor_dev_t minor) {
    return validate_minor_dev(minor);
}

static int ramdisk_open(minor_dev_t minor) {
    return validate_minor_dev(minor);
}

/*
 * Common utility functions to read and write from/to RAM disk
 * Parameter:
 * @minor - minor device number of device to read from
 * @blocks - number of blocks to read
 * @lba - address of first block to read
 * @buffer - data
 * @write - 0 to read, 1 to write
 * Return value:
 * -EIO if operation failed
 * -ENODEV if the device is not valid
 * number of bytes written or read otherwise
 */
static ssize_t ramdisk_rw(minor_dev_t minor, ssize_t blocks, ssize_t lba, void* buffer, int write) {
    int rc;
    u32 start;
    u32 end;
    /*
     * Validate parameters
     */
    rc = validate_minor_dev(minor);
    if (rc) {
        ERROR("Validation of device failed with return code %d\n", rc);
        return -ENODEV;
    }
    /*
     * First and last byte on RAM disk to read/write
     */
    start = lba*RAMDISK_BLOCK_SIZE + ramdisk_start;
    end = start + blocks*RAMDISK_BLOCK_SIZE-1;
    if (end > ramdisk_end) {
        ERROR("Tried to read/write outside of RAM disk area\n");
        return -EIO;
    }
    /*
     * Copy data from RAM disk into buffer
     * or vice versa
     */
    if (0==write)
        memcpy(buffer, (void*) start, end-start+1);
    else
        memcpy((void*) start, buffer, end-start+1);
    return end-start+1;
}

/*
 * Read data from the RAM disk
 * Parameters:
 * @minor - minor device
 * @blocks - number of blocks to read
 * @lba - logical address of first block
 * @buffer - pointer to data area where result will be stored
 * Return value:
 * number of bytes read upon success
 * -ENODEV if the device is not valid
 * -EIO if the operation failed for any other reason
 */
static ssize_t ramdisk_read(minor_dev_t minor, ssize_t blocks, ssize_t lba, void* buffer) {
    return ramdisk_rw(minor, blocks, lba, buffer, 0);
}

static ssize_t ramdisk_write(minor_dev_t minor, ssize_t blocks, ssize_t lba, void* buffer) {
    return ramdisk_rw(minor, blocks, lba, buffer, 1);
}

/*
 * Initialize the RAM disk:
 * Get information on the location of the RAM disk in virtual memory from
 * the memory manager and call the device driver manager to register the RAM disk
 */
void ramdisk_init() {
    int rc;
    if (0==mm_have_ramdisk()) {
        return;
    }
    have_ramdisk = 1;
    ramdisk_start = mm_get_initrd_base();
    ramdisk_end = mm_get_initrd_top();
    MSG("Found RAMDISK at %x - %x\n", ramdisk_start, ramdisk_end);
    /*
     * Set up block device operations and register with driver manager
     */
    ramdisk_ops.close = ramdisk_close;
    ramdisk_ops.open = ramdisk_open;
    ramdisk_ops.read = ramdisk_read;
    ramdisk_ops.write = ramdisk_write;
    rc = dm_register_blk_dev(MAJOR_RAMDISK, &ramdisk_ops);
    if (rc) {
        ERROR("Could not register RAM disk with driver manager, rc=%d\n", rc);
    }
}


