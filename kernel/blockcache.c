/*
 * blockcache.c
 * This module is not yet implemented in this release of ctOS, but only
 * contains stubs
 *
 * The block cache is located between the file system layer and the actual device drivers
 * for block devices. Its purpose is to cache read blocks in memory in order to speed up
 * read and write operations. The block cache uses the services offered by the device
 * driver manager to retrieve function pointers for the read, write, open and close
 * operations of a specific device
 */

#include "blockcache.h"
#include "dm.h"
#include "drivers.h"
#include "debug.h"
#include "lib/string.h"
#include "mm.h"
#include "kerrno.h"

/*
 * A local loglevel
 */
int __bc_loglevel = 0;

#define BC_DEBUG(...) do {if (__bc_loglevel > 0 ) { kprintf("DEBUG at %s@%d (%s): ", __FILE__, __LINE__, __FUNCTION__); \
        kprintf(__VA_ARGS__); }} while (0)



/*
 * Initialize block cache
 */
void bc_init() {

}

/*
 * Read the given number of blocks from the cache or - if necessary - from
 * the device
 * Parameters:
 * @device - the device from which to read
 * @blocks - the number of blocks to read
 * @first_block - first block to read
 * @buffer - this is where the data is stored
 * Return value:
 * number of bytes read or -EIO if read failed
 */
static ssize_t bc_read_impl(dev_t dev, ssize_t blocks, ssize_t first_block,
        void* buffer) {
        blk_dev_ops_t* ops = dm_get_blk_dev_ops(MAJOR(dev));
    if (0 == ops) {
        ERROR("Invalid block device operations pointer\n");
        return -EIO;
    }
    if (0 == ops->read) {
        ERROR("Invalid block device operations pointer\n");
        return -EIO;
    }
    return ops->read(MINOR(dev), blocks, first_block, buffer);
}

ssize_t
(*bc_read)(dev_t dev, ssize_t blocks, ssize_t first_block, void* buffer) =
        bc_read_impl;

/*
 * Write the given number of blocks to the cache or to the device.
 * Parameters:
 * @device - the device to which to write
 * @blocks - the number of blocks to write
 * @first_block - first block to write
 * @buffer - this is where the data is stored
 * Return value:
 * number of bytes read or -EIO if write failed
 */
static ssize_t bc_write_impl(dev_t dev, ssize_t blocks, ssize_t first_block,
        void* buffer) {
    blk_dev_ops_t* ops = dm_get_blk_dev_ops(MAJOR(dev));
    if (0 == ops) {
        ERROR("Invalid block device operations pointer\n");
        return -EIO;
    }
    if (0 == ops->write) {
        ERROR("Invalid block device operations pointer\n");
        return -EIO;
    }
    return ops->write(MINOR(dev), blocks, first_block, buffer);
}
ssize_t (*bc_write)(dev_t dev, ssize_t blocks, ssize_t first_block,
        void* buffer) = bc_write_impl;

/*
 * Open a device
 * Parameters:
 * @device - the device to be opened
 * Return value:
 * ENODEV if device open function could not be located, return value
 * of device open function otherwise
 */
int bc_open(dev_t dev) {
    blk_dev_ops_t* ops = dm_get_blk_dev_ops(MAJOR(dev));
    if (0 == ops)
        return ENODEV;
    if (0 == ops->open) {
        ERROR("Invalid block device operations pointer\n");
        return ENODEV;
    }
    return ops->open(MINOR(dev));
}

/*
 * Close a device
 * Parameters:
 * @device - the device to be closed
 * Return value:
 * ENODEV if device close function could not be located, return value
 * of device close function otherwise
 */
int bc_close(dev_t dev) {
    blk_dev_ops_t* ops = dm_get_blk_dev_ops(MAJOR(dev));
    if (0 == ops)
        return ENODEV;
    if (0 == ops->close) {
        ERROR("Invalid block device operations pointer\n");
        return ENODEV;
    }
    return ops->close(MINOR(dev));
}

/*
 * This is the main interface function to read a given number of bytes
 * from disk or the cache, starting at a specified offset within the block.
 * Note that the blocksize is supposed to be 1024 throughout and needs to
 * be converted to the actual block size by the device driver
 * Parameter:
 * @block - block where we start reading
 * @bytes - number of bytes to read
 * @buffer - buffer to store read data in
 * @device - the device
 * @offset - offset within the block where we start reading
 * Return value:
 * 0 upon success
 * ENOMEM if memory could not be allocated for temporary buffer
 * EIO if read from device failed
 */
int bc_read_bytes(u32 block, u32 bytes, void* buffer, dev_t device, u32 offset) {
    void* tmp;
    int rc;
    int blocks_to_read = ((bytes + offset - 1) / BLOCK_SIZE) + 1;
    tmp = (void*) kmalloc(blocks_to_read * BLOCK_SIZE);
    if (0 == tmp) {
        ERROR("Could not allocate memory for buffer\n");
        return ENOMEM;
    }
    rc = bc_read(device, blocks_to_read, block, tmp);
    if (rc <= 0) {
        ERROR("Disk read error\n");
        kfree(tmp);
        return EIO;
    }
    memcpy(buffer, tmp + offset, bytes);
    kfree(tmp);
    return 0;
}

/*
 * This is the main interface function to write a given number of bytes
 * to disk or to the cache, starting at a specified offset within the block
 * Note that the blocksize is supposed to be 1024 throughout and needs to
 * be converted to the actual block size by the device driver
 * Special care is taken to make sure that partial blocks at the start or
 * end of a write request are read first from the device so that no stale
 * data is written
 * Parameter:
 * @block - block where we start writing
 * @bytes - number of bytes to write
 * @buffer - buffer which contains the data to be written
 * @device - the device
 * @offset - offset within the block where we start writing
 * Return value:
 * 0 upon success
 * ENOMEM if memory could not be allocated for temporary buffer
 * EIO if read from device failed
 * EINVAL if the offset exceeds the block size
 */
int bc_write_bytes(u32 block, u32 bytes, void* buffer, dev_t device, u32 offset) {
    void* tmp;
    int rc;
    int first_partial = 0;
    BC_DEBUG("block=%d, bytes=%d, offset=%d\n", block, bytes, offset);
    if (offset >= BLOCK_SIZE) {
        block = block + offset / BLOCK_SIZE;
        offset = offset % BLOCK_SIZE;
    }
    int blocks_to_write = ((bytes + offset - 1) / BLOCK_SIZE) + 1;
    tmp = (void*) kmalloc(blocks_to_write * BLOCK_SIZE);
    BC_DEBUG("block=%d, bytes=%d, blocks_to_write=%d, offset=%d\n", block, bytes, blocks_to_write, offset);
    if (0 == tmp) {
        ERROR("Could not allocate memory for buffer\n");
        return ENOMEM;
    }
    /*
     * Check whether we have any partial block at the start or
     * end of the data to be written. If yes, reads these blocks
     * first from disk into our temporary buffer to avoid writing
     * random data
     */
    if ((offset % BLOCK_SIZE)) {
        /*
         * Read first block
         */
        BC_DEBUG("Reading block %d\n", block);
        first_partial = 1;
        rc = bc_read(device, 1, block, tmp);
        if (rc <= 0) {
            ERROR("Disk read error, rc=-%d\n", (-1)*rc);
            return EIO;
        }
    }
    if ((offset + bytes) % BLOCK_SIZE) {
        /*
         * Read last block if not yet done in the IF block above
         */
        if (!((first_partial == 1) && (blocks_to_write == 1))) {
            BC_DEBUG("Reading last block %d\n", block + blocks_to_write-1);
            rc = bc_read(device, 1, block + blocks_to_write - 1, tmp
                    + (blocks_to_write - 1) * BLOCK_SIZE);
            if (rc <= 0) {
                ERROR("Disk read error, rc=-%d\n", (-1)*rc);
                return EIO;
            }
        }
    }
    BC_DEBUG("Copying %d bytes to tmp+%d\n", bytes, offset);
    memcpy(tmp + offset, buffer, bytes);
    BC_DEBUG("Writing temporary area back to block %d\n", block);
    rc = bc_write(device, blocks_to_write, block, tmp);
    if (rc <= 0) {
        ERROR("Disk write error\n");
        kfree(tmp);
        return EIO;
    }
    kfree(tmp);
    return 0;
}


/***************************************************************
 * Everything below this line is for debugging only            *
 **************************************************************/


/*
 * A testcase designed to test cross-page boundary reads from a disk
 */
void bc_test_cross_page_read() {
    void* pages;
    void* cmp;
    int i, j;
    unsigned char old, new;
    int offset = 3896;
    int block = 8197;
    PRINT("Testing cross-page boundary read. Let me first get a few pages in memory\n");
    pages = kmalloc_aligned(8192, 4096);
    if (0 == pages) {
        PANIC("Could not get pages\n");
    }
    /*
     * Fill up with something different from zero
     */
    memset(pages, 1, 8192);
    /*
     * We first do an aligned read
     */ 
    cmp = kmalloc(1024);
    if (0 == cmp) {
        PANIC("Could not get memory for compare buffer\n");
    }
    PRINT("Reading logical block %d into page aligned buffer at %x\n", block, pages);
    if (1024 != bc_read_impl(DEVICE(3,1), 1, block, pages)) {
        PANIC("Could not read from device\n");
    }
    /*
     * Copy results into compare buffer
     */
    memcpy(cmp, pages, 1024);
    PRINT("Now doing second, unaligned read at %x\n", pages + offset);
    memset(pages, 2, 8192);
    PRINT("Before read: pages[4096] = %x\n", ((char*) pages)[4096]);
    if (1024 != bc_read_impl(DEVICE(3,1), 1, block, pages + offset)) {
        PANIC("Could not read from device\n");
    }
    PRINT("After read: pages[4096] = %x\n", ((char*) pages)[4096]);
    /*
     * Compare
     */
    PRINT("Comparing results\n");
    for (i = 0; i < 1024; i++) {
        old = ((unsigned char*) cmp)[i];
        new = ((unsigned char*) (pages + offset))[i];
        /* PRINT("i = %d, old = %x, new = %x\n", i, old, new); */
        if (old != new) {
            for (j = ((i >= 8) ? i-8 : 0); j <= ((i + 8) < 1024 ? i+8 : 1024); j++) {
                PRINT("i = %d, old = %x, new = %x\n", j, ((unsigned char*) cmp)[j], ((unsigned char*) (pages + offset))[j]);
                if (0 == (((u32) pages + offset + j) % 4096)) {
                    PRINT("------------------------------\n");
                }
            }
            PANIC("Test failed at index %d, old = %x, new = %x\n", i, old, new);

        }
    }
    kfree(cmp);
    kfree(pages);
}
