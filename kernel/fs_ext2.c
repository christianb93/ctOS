/*
 * fs_ext2.c
 *
 * This module contains the implementation of the EXT2 file system. It is meant to
 * be invoked by the generic file system layer in fs.c and implements the interface
 * required by the generic file system.
 */

#include "fs.h"
#include "fs_ext2.h"
#include "blockcache.h"
#include "kerrno.h"
#include "debug.h"
#include "mm.h"
#include "drivers.h"
#include "lib/string.h"
#include "lists.h"
#include "dm.h"
#include "pm.h"
#include "lib/time.h"
#include "timer.h"
#include "lib/sys/stat.h"
#include "lib/utime.h"

/*
 * The inode operations structure which we use
 */
static inode_ops_t ext2_iops = {
        fs_ext2_inode_read,
        fs_ext2_inode_write,
        fs_ext2_inode_trunc,
        fs_ext2_get_direntry,
        fs_ext2_create_inode,
        fs_ext2_unlink_inode,
        fs_ext2_inode_clone,
        fs_ext2_inode_release,
        fs_ext2_inode_flush,
        fs_ext2_inode_link
};

/*
 * A local loglevel
 */
int __ext2_loglevel = 0;


#define EXT2_DEBUG(...) do {if (__ext2_loglevel > 0 ) { kprintf("DEBUG at %s@%d (%s): ", __FILE__, __LINE__, __FUNCTION__); \
                     kprintf(__VA_ARGS__); }} while (0)

/*
 * This is a linked list of metadata information for all
 * open file systems. The list is protected by a spinlock
 * which also protects the reference count of each
 * element in the list
 */
static ext2_metadata_t* ext2_metadata_head;
static ext2_metadata_t* ext2_metadata_tail;
static spinlock_t ext2_metadata_lock;

/*
 * Data structures:
 *
 * The file system needs to manage superblocks and inodes. Both exist in two flavours, namely the
 * file system independent view presented towards the virtual file system layer in fs.c (inode_t and
 * superblock_t) as well as the EXT2 specific versions as stored on disk (ext2_inode_t and ext2_superblock_t)
 *
 * To organize these data structures, for both inodes and superblocks, there are special containers realized by
 * the structures ext2_metadata_t and ext2_inode_data_t which link to the VFS level inodes as well as to their
 * EXT2 specific equivalents. For each superblock which is in the superblock cache, there is one instance of the
 * structure ext2_metadata_t which also contains a linked list of cached inodes for this superblock. In addition,
 * a copy of the block group descriptor table (bgdt_t) is stored within this data structure.
 *
 *
 *   superblock_t
 *    A        |
 *    |        |
 *    |        |
 *    |        V
 *  ext2_metadata_t
 *    |      |    A
 *    V      |    |
 *  bgdt_t   |    ---------------> ext2_inode_data_t <--> ext2_inode_data_t <--> ...  <<<< this is the inode cache
 *           |                         A        |
 *           V                         |        |
 *         ext2_superblock_t           |        |
 *                                     V        V
 *                                  inode_t    ext2_inode_t
 *
 * You can navigate through this structure as follows.
 * - from ext2_metadata_t to superblock_t - use pointer ext2_metadata_t.super
 * - from ext2_metadata_t to ext2_superblock_t - use pointer ext2_metadata_t.ext2_super
 * - from superblock_t to ext2_metadata_t - user pointer superblock_t.data, cast to ext2_metadata_t*
 * - from ext2_metadata_t to linked list of ext2_inode_data_t structures (i.e. to the inode cache) - use pointer inodes_head
 * - from ext2_inode_data_t to inode_t - use pointer ext2_inode_data_t.ext2_inode
 * - from ext2_inode_data_t to ext2_metadata_t - use pointer ext2_inode_data_t.ext2_meta
 * - from inode_t to ext2_inode_data_t - cast pointer inode_t.data to ext2_inode_data_t*
 * - from ext2_inode_data_t to ext2_inode_t - use pointer ext2_inode_data_t.ext2_inode
 *
 * The list of ext2_metadata_t structures can be accessed via the global variables ext2_metadata_head and ext2_metadata_tail
 *
 *
 * Reference counts
 *
 * Both ext2_inode_data_t and ext2_metadata_t have a reference count. As ext2_inode_data_t contains a backward reference to
 * the corresponding instance of ext2_metadata_t, the reference count of any instance of ext2_metadata_t is at least the number
 * of entries in its inode cache.
 *
 * When a file system is mounted, the reference count of its superblock as well as the reference count of its root inode
 * are one (note that the VFS only maintains a reference of the root inode and derives the reference to the root superblock
 * from that if needed). Therefore a file system is busy iff the reference count of its superblock exceeds one.
 *
 * Locking strategy
 *
 * The following locks are used in this module:
 * 1) ext2_metadata_lock - protect the list of metadata structures. Also get this lock whenever a reference count of one
 *    of the ext2_metadata_t instances in the list is changed
 * 2) ext2_metadata_t.lock - protect the inode cache for this superblock. Also get this lock whenever a reference count of
 *    one of the inodes in the cache is changed
 * 3) ext2_metadata_t.sb_lock - this semaphore is used to protect the content of the ext2 superblock and the block group
 *    descriptor table as well as the inode and block bitmap
 *
 *
 *
 */

/*
 * When reading data from a file or writing data from a file, we need to visit all data
 * blocks which make up a file and perform a specific operation on each of the data blocks.
 * To do this, this driver uses the collection of utility functions walk_blocklist, walk_indirect_block,
 * walk_double_indirect_block and walk_triple_indirect_block. These functions initiate a walk through
 * all data blocks of a file.
 *
 * If they hit upon a hole in the file or an area outside of the original file size,
 * i.e. a block with block number zero, they are capable of allocating additional blocks
 * to enlarge the file.
 *
 * All these functions use the structure blocklist_walk to describe the parameters of the walk
 * and to exchange data on the number of blocks already visited and the number of bytes processed
 *
 * For each data block, the callback function process_block will be invoked once. The second argument
 * to the function is the block number on the device of the block which we visit. The counters bytes_processed
 * and blocks_processed are expected to be updated by the callback function if it could process the block successfully.
 *
 * Setting the abort flag will cause the blocklist to be stopped at this point
 *
 */
typedef struct _blocklist_walk_t {
    void* data;                                  // for rw-operations, this is the buffer to use
    u32 blocks_processed;                        // number of blocks already visited
    u32 bytes_processed;                         // number of bytes already processed
    u32 first_block;                             // first block of file which needs to be visited
    u32 last_block;                              // last block of file which needs to be visited
    u32 bytes;                                   // number of bytes to be processed
    u32 offset;                                  // offset into first block at which we start processing
    dev_t device;                                // device from which we read or to which we write
    int allocate;                                // if this flag is set, new blocks will be allocated during the walk if needed
    int deallocate;                              // if this is set indirect blocks will be deallocated after visiting them if they are empty
    int zero;                                    // if this is set, a block will be set to zero in the inode blocklist after visiting it
    ext2_metadata_t* ext2_meta;                  // ext2 metadata structure
    u32 block_group_nr;                          // number of block group in which the inode we process is located
    ext2_inode_t* ext2_inode;                    // the inode which we process
    int abort;                                   // a callback function can set this to stop the walk
    u32 first_byte;                              // first byte to be processed within the current block
    u32 last_byte;                               // last byte to be processed within the current block
    int (*process_block)(struct _blocklist_walk_t * request, u32 block_nr);
} blocklist_walk_t;

/*
 * Forward declarations
 */
static void destroy_ext2_inode_data(ext2_inode_data_t* ext2_inode_data);
static void init_super(ext2_metadata_t* meta);
static void destroy_meta(ext2_metadata_t* meta);
static ext2_metadata_t* new_meta();


/****************************************************************************************
 * File system initialization routines                                                  *
 ****************************************************************************************/

/*
 * Initialize the internal data structures of the file system
 */
int fs_ext2_init() {
    ext2_metadata_head = 0;
    ext2_metadata_tail = 0;
    spinlock_init(&ext2_metadata_lock);
    return 0;
}

/*
 * Given a device, read the superblock and perform a few checks
 * to make sure that the file system on the device is an ext2
 * file system which we can handle
 * This function will open the device and close it again
 * Parameter:
 * @device - the device to probe
 * Return value:
 * ENOMEM if no memory could be allocated for the superblock
 * ENODEV if the device could not be opened
 * EIO if the superblock could not be read from disk
 * 1 if the probing was successful
 * 0 if the probing was technically possible, but not successful
 */
int fs_ext2_probe(dev_t device) {
    int failed = 0;
    ext2_superblock_t* super;
    int rc = bc_open(device);
    void* buffer;
    if (rc) {
        ERROR("Could not open device for probing, rc=%d\n");
        return ENODEV;
    }
    if (0 == (buffer = (void*) kmalloc(EXT2_SUPERBLOCK_SIZE) )) {
        ERROR("Could not allocate memory for superblock\n");
        return ENOMEM;
    }
    /*
     * Read superblock from disk
     */
    if ((rc = bc_read_bytes(1, 1024, buffer, device, 0))) {
        ERROR("Could not read superblock from disk, rc=%d\n", rc);
        kfree(buffer);
        bc_close(device);
        return EIO;
    }
    
    super = (ext2_superblock_t*) buffer;
    /*
     * Check superblock
     */
    if (super->s_magic != EXT2_MAGIC_NUMBER) {
        DEBUG("Wrong magic number %x\n", super->s_magic);
        failed = 1;
    }
    if ((super->s_feature_incompat) || (super->s_feature_ro_compat)) {
        DEBUG("Incompatible feature\n");
        failed = 1;
    }
    if ((0 != super->s_log_block_size) || (0 != super->s_log_frag_size)) {
        DEBUG("Incorrect size\n");
        failed = 1;
    }
    if (sizeof(ext2_inode_t) != super->s_inode_size) {
        DEBUG("Inode size %d does not match\n", super->s_inode_size);
        failed = 1;
    }
    kfree(buffer);
    bc_close(device);
    if (failed)
        return 0;
    return 1;
}

/****************************************************************************************
 * Read file system metadata and write file system metadata                             *
 ****************************************************************************************/

/*
 * Read superblock and block group descriptor table from an
 * EXT2 file system
 * Parameter:
 * @device - device from which we read
 * Return value:
 * a metadata structure pointint to the read superblock and
 * block group descriptor table
 * Reference counts:
 * - reference count of returned metadata structure will be one
 */
static ext2_metadata_t* read_meta(dev_t device) {
    int rc = 0;
    ext2_metadata_t* meta = 0;
    if (0 == (meta = new_meta())) {
        ERROR("Could not allocate ext2 metadata - not enough memory\n");
        return 0;
    }
    meta->device = device;
    /*
     * Read superblock from disk
     */
    rc = bc_read_bytes(1, sizeof(ext2_superblock_t),
            (void*) (meta->ext2_super), device, 0);
    if (rc) {
        ERROR("Could not get superblock from disk - disk read error\n");
        destroy_meta(meta);
        kfree((void*) meta);
        return 0;
    }
    /*
     * Read block group descriptor table
     * We need to get the number of block groups from the
     * ext2 superblock to determine the number of entries in this table
     * (meta->bgdt_size)
     */
    meta->bgdt_size = (meta->ext2_super->s_blocks_count
            / meta->ext2_super->s_blocks_per_group) + 1;
    meta->bgdt_blocks = ((meta->bgdt_size * sizeof(ext2_bgd_t))-1) / BLOCK_SIZE + 1;
    meta->bgdt = (ext2_bgd_t*) kmalloc(meta->bgdt_size * sizeof(ext2_bgd_t));
    if (0 == meta->bgdt) {
        ERROR("Could not get block group table from disk - out of memory\n");
        destroy_meta(meta);
        kfree((void*) meta);
        return 0;
    }
    rc = bc_read_bytes(meta->ext2_super->s_first_data_block + 1,
            meta->bgdt_size * sizeof(ext2_bgd_t), (void*) (meta->bgdt), device,
            0);
    if (rc) {
        ERROR("Could not get block group table from disk - disk read error\n");
        destroy_meta(meta);
        kfree((void*) meta);
        return 0;
    }
    return meta;
}



/*
 * Write a changed superblock and block group descriptor table back to disk
 * Parameter:
 * @ext2_meta - ext2 metadata structure
 * Return value:
 * 0 if the operation was successful
 * EIO if the operation failed
 */
static int put_meta(ext2_metadata_t* ext2_meta) {
    /*
     * Write block group descriptor table back to disk
     */
    if (bc_write_bytes(ext2_meta->ext2_super->s_first_data_block + 1,
            ext2_meta->bgdt_size * sizeof(ext2_bgd_t),
            (void*) (ext2_meta->bgdt), ext2_meta->device, 0)) {
        ERROR("Could not write block group table to disk - disk write error\n");
        return EIO;
    }
    /*
     * Write superblock back to disk
     */
    if (bc_write_bytes(1, sizeof(ext2_superblock_t), (void*) ext2_meta->ext2_super,
            ext2_meta->device, 0)) {
        ERROR("Could not write superblock to disk - disk write error\n");
        return EIO;
    }
    return 0;
}


/****************************************************************************************
 * Read EXT2 inodes from disk and write inodes to disk                                  *
 ***************************************************************************************/

/*
 * Get an ext2 inode from disk
 * Parameters:
 * @inode_nr - the number of the inode to retrieve
 * @meta - the file system metadata
 * Return value:
 * a pointer to the inode. It is the responsibility of the caller to free
 * the memory allocated for the inode again. If the operation fails, 0
 * is returned
 */
static ext2_inode_t* get_ext2_inode(ino_t inode_nr, ext2_metadata_t* meta) {
    u32 block_group;
    u32 block;
    u32 index;
    ext2_bgd_t* bgd;
    ext2_inode_t* ext2_inode;
    /*
     * First we compute the block group in which the inode is located
     * and the index within the inode table of this block group
     */
    block_group = (inode_nr - 1) / meta->ext2_super->s_inodes_per_group;
    index = (inode_nr - 1) % meta->ext2_super->s_inodes_per_group;
    /*
     * Get the block group descriptor for the block group
     * in question
     */
    bgd = meta->bgdt + block_group;
    /*
     * From there we can get the block address of the
     * inode table
     */
    block = bgd->bg_inode_table;
    if (0 == (ext2_inode = (ext2_inode_t*) kmalloc(sizeof(ext2_inode_t)))) {
        ERROR("Could not get memory for inode\n");
        return 0;
    }
    /*
     * Now read inode from disk
     */
    EXT2_DEBUG("Reading inode %d from disk, block = %d, index = %d, size = %d\n", inode_nr, block, index, sizeof(ext2_inode_t));
    if (bc_read_bytes(block, sizeof(ext2_inode_t), ext2_inode, meta->device,
            index * sizeof(ext2_inode_t))) {
        ERROR("Error while reading from disk\n");
        kfree((void*) ext2_inode);
        return 0;
    }
    return ext2_inode;
}

/*
 * Write an inode back to disk
 * Parameters:
 * @ext2_metadata - pointer to ext2 superblock metadata
 * @inode - the inode to write
 * Return value:
 * 0 if operation was successful
 * EIO if the operation failed
 */
static int put_inode(ext2_metadata_t* ext2_meta, inode_t* inode) {
    ext2_inode_data_t* ext2_inode_data = (ext2_inode_data_t*) inode->data;
    ext2_inode_t* ext2_inode = ext2_inode_data->ext2_inode;
    u32 block_group;
    u32 block;
    u32 index;
    ext2_bgd_t* bgd;
    /*
     * First we compute the block group in which the inode is located
     * and the index within the inode table of this block group
     */
    block_group = (inode->inode_nr - 1)
            / ext2_meta->ext2_super->s_inodes_per_group;
    index = (inode->inode_nr - 1) % ext2_meta->ext2_super->s_inodes_per_group;
    /*
     * Get the block group descriptor for the block group
     * in question
     */
    bgd = ext2_meta->bgdt + block_group;
    /*
     * From there we can get the block address of the
     * inode table
     */
    block = bgd->bg_inode_table;
    EXT2_DEBUG("Writing inode %d back to disk, block = %d, index = %d, size = %d\n", inode->inode_nr, block, index, sizeof(ext2_inode_t));
    if (bc_write_bytes(block, sizeof(ext2_inode_t), ext2_inode,
            ext2_meta->device, index * sizeof(ext2_inode_t))) {
        ERROR("Error while writing inode to disk\n");
        return EIO;
    }
    EXT2_DEBUG("Inode written successfully\n");
    return 0;
}


/****************************************************************************************
 * An EXT2 file system manages block and block groupa in bitmask stored on the device   *
 * The following functions are used to allocate new blocks on the device and to free    *
 * allocated blocks again                                                               *
 ***************************************************************************************/

/*
 * Utility function to allocate a free block and mark it
 * as used. This function does not acquire any locks but
 * assumes that the caller has done this
 * Parameter:
 * @ext2_meta - ext2 metadata structure for the file system
 * @bgd - block group descriptor of block group in which we try to allocate a block
 * @errno - will be set if an error occured
 * Return value:
 * number of newly allocated block or zero if no free block could be found
 */
static u32 allocate_block_in_group(ext2_metadata_t* ext2_meta, ext2_bgd_t* bgd, int *errno) {
    u8 block_bitmap[BLOCK_SIZE];
    int rc;
    ext2_superblock_t* ext2_super = ext2_meta->ext2_super;
    u32 block_group_nr = (bgd - ext2_meta->bgdt) / sizeof(ext2_bgd_t);
    u32 blocks_in_group;
    u32 i;
    u32 block_nr = 0;
    /*
     * If there is no free block at all in this group, return immediately
     */
    if (0 == bgd->bg_free_blocks_count)
        return 0;
    /*
     * We first read the block bitmap for this group. Recall that the block
     * bitmap is the third block in the block group (block 0 is the superblock
     * copy, block 1 is the block group descriptor table copy). We can get the block
     * number from the field bg_block_bitmap
     */
    rc = bc_read_bytes(bgd->bg_block_bitmap, BLOCK_SIZE, (void*) block_bitmap,
            ext2_meta->device, 0);
    if (rc) {
        ERROR("Could not read block bitmap from device\n");
        *errno = rc;
        return 0;
    }
    /*
     * Next we need to find out how many blocks our block group has. If the block group is
     * the last block group on the file system, i.e. of the block_group_nr is equal to the number
     * of entries in the block group descriptor table, the number of blocks is computed as s_blocks_count % s_block_per_group
     * unless s_blocks_count is an exact multiple of s_blocks_per_group
     * If this is not the last block, the number of blocks is s_blocks_per_group.
     */
    if ((ext2_meta->bgdt_size - 1 == block_group_nr)
            && (ext2_super->s_blocks_count % ext2_super->s_blocks_per_group)) {
        blocks_in_group = ext2_super->s_blocks_count
                % ext2_super->s_blocks_per_group;
    }
    else {
        blocks_in_group = ext2_super->s_blocks_per_group;
    }
    /*
     * Now scan block bitmap until we find a free block. Note that we can
     * find the first block of the block group by subtracting the number of
     * blocks allocated by the block group descriptor table plus one from the
     * block number of the bitmap itself stored in bg_block_bitmap in the
     * block group descriptor
     */
    for (i = 0; i < blocks_in_group; i++) {
        if (0 == BITFIELD_GET_BIT(block_bitmap,i)) {
            block_nr = i + bgd->bg_block_bitmap - ext2_meta->bgdt_blocks - 1;
            BITFIELD_SET_BIT(block_bitmap,i);
            break;
        }
    }
    /*
     * If we have found a free block, write the changed block map back to disk
     */
    if (block_nr) {
        rc = bc_write_bytes(bgd->bg_block_bitmap, BLOCK_SIZE,
                (void*) block_bitmap, ext2_meta->device, 0);
        if (rc) {
            ERROR("Could not write block bitmap to device\n");
            *errno = rc;
            return 0;
        }
        bgd->bg_free_blocks_count--;
        ext2_super->s_free_blocks_count--;
        if (put_meta(ext2_meta)) {
            ERROR("Could not write changed metadata back to disk\n");
            *errno = EIO;
            return 0;
        }
    }
    return block_nr;
}

/*
 * Utility function to allocate a free block and mark it
 * as used. The function will first try to allocate a block
 * in the block group block_group_nr. If that fails, other block
 * groups will be scanned as well
 * Parameter:
 * @ext2_meta - ext2 metadata structure for the file system
 * @block_group_nr - preferred block group number
 * @errno - will be set if an unrecoverable error occurred
 * Return value:
 * number of newly allocated block or zero if no free block could be found
 * Locks:
 * sb_lock in ext2_metadata structure
 */
static u32 allocate_block(ext2_metadata_t* ext2_meta, u32 block_group_nr, int* errno) {
    ext2_bgd_t* bgd;
    int i;
    u32 block_nr = 0;
    if (block_group_nr >= ext2_meta->bgdt_size) {
        ERROR("Preferred block group number exceeds allowed range\n");
        return 0;
    }
    /*
     * Get lock
     */
    sem_down(&ext2_meta->sb_lock);
    /*
     * Check superblock flag to see if the superblock already indicates
     * that no blocks are left
     */
    if (0 == ext2_meta->ext2_super->s_free_blocks_count) {
        mutex_up(&ext2_meta->sb_lock);
        return 0;
    }
    /*
     * First try the block which is preferred
     */
    bgd = ext2_meta->bgdt + block_group_nr;
    block_nr = allocate_block_in_group(ext2_meta, bgd, errno);
    /*
     * If we could not find an entry here, repeat this for all other block groups as well
     */
    if (0 == block_nr) {
        for (i = 0; i < ext2_meta->bgdt_size; i++) {
            if (i != block_group_nr) {
                block_nr = allocate_block_in_group(ext2_meta, ext2_meta->bgdt
                        + i, errno);
                if (block_nr)
                    break;
            }
        }
    }
    /*
     * Release lock
     */
    mutex_up(&ext2_meta->sb_lock);
    return block_nr;
}

/*
 * Deallocate a block
 * Parameter:
 * @ext2_meta - the metadata structure of the file system
 * @block_nr - the number of the block to be deallocated
 * Return value:
 * 0 if the operation was successful
 * EIO if an error occurred
 * Locks:
 * sb_lock in metadata structure
 */
static int deallocate_block(ext2_metadata_t* ext2_meta, u32 block_nr) {
    u32 block_group_nr;
    u32 index;
    ext2_bgd_t* bgd;
    u8 block_bitmap[BLOCK_SIZE];
    /*
     * Determine block group number and index within group
     */
    block_group_nr = (block_nr - 1) / ext2_meta->ext2_super->s_blocks_per_group;
    index = (block_nr - 1) % ext2_meta->ext2_super->s_blocks_per_group;
    if (block_group_nr >= ext2_meta->bgdt_size) {
        PANIC("Invalid block group number %d\n", block_group_nr);
    }
    /*
     * Get lock
     */
    sem_down(&ext2_meta->sb_lock);
    /*
     * Get entry in block group descriptor table and read block bitmap into
     * memory
     */
    bgd = ext2_meta->bgdt+block_group_nr;
    if (bc_read_bytes(bgd->bg_block_bitmap, BLOCK_SIZE, (void*) block_bitmap, ext2_meta->device, 0)) {
        ERROR("Could not read block bitmap from disk\n");
        mutex_up(&ext2_meta->sb_lock);
        return EIO;
    }
    /*
     * Flag block as unused and write back to disk
     */
    if (0 == BITFIELD_GET_BIT(block_bitmap, index)) {
        PANIC("Block %d within group not in use", index);
        mutex_up(&ext2_meta->sb_lock);
        return EIO;
    }
    BITFIELD_CLEAR_BIT(block_bitmap, index);
    if (bc_write_bytes(bgd->bg_block_bitmap, BLOCK_SIZE, (void*) block_bitmap, ext2_meta->device, 0)) {
        PANIC("Could not write block bitmap to disk\n");
        mutex_up(&ext2_meta->sb_lock);
        return EIO;
    }
    /*
     * Update block group descriptor and super block
     */
    ext2_meta->ext2_super->s_free_blocks_count++;
    bgd->bg_free_blocks_count++;
    if (put_meta(ext2_meta)) {
        PANIC("Could not write changed metadata back to disk\n");
        mutex_up(&ext2_meta->sb_lock);
        return EIO;
    }
    /*
     * Release lock
     */
    mutex_up(&ext2_meta->sb_lock);
    return 0;
}

/****************************************************************************************
 * Free and available inodes are marked in a bitmask within the EXT2 file system meta-  *
 * data on disk. These functions operate in this bitmask                                *
 ****************************************************************************************/

/*
 * Deallocate an inode, i.e. release slot in inode bitmap
 * Parameter:
 * @inode - the inode to be deallocated
 * Return value:
 * 0 if operation was successful
 * EIO if an I/O error occurred
 * ENOMEM if there was not sufficient memory to complete the operation
 * Locks:
 * sb_lock in metadata structure
 */
static int deallocate_inode(inode_t* inode) {
    ext2_metadata_t* ext2_meta = ((ext2_inode_data_t*)inode->data)->ext2_meta;
    u32 block_group_nr = 0;
    u32 inode_in_group;
    u8* inode_bitmap;
    ext2_bgd_t* bgd;
    ext2_inode_t ext2_inode;
    /*
     * Allocate memory for bitmap
     */
    if (0 == (inode_bitmap = (u8*) kmalloc(BLOCK_SIZE))) {
        ERROR("Could not allocate memory for inode bitmap\n");
        return ENOMEM;
    }
     /*
     * Get block group number and index of inode in group
     */
    block_group_nr = (inode->inode_nr -1) / ext2_meta->ext2_super->s_inodes_per_group;
    inode_in_group = (inode->inode_nr -1) % ext2_meta->ext2_super->s_inodes_per_group;
    if (block_group_nr >= ext2_meta->bgdt_size) {
        PANIC("Invalid block group number %d\n", block_group_nr);
    }
    sem_down(&ext2_meta->sb_lock);
    /*
     * Load inode bitmap
     */
    bgd = ext2_meta->bgdt + block_group_nr;
    if (bc_read_bytes(bgd->bg_inode_bitmap, BLOCK_SIZE, (void*) inode_bitmap, ext2_meta->device, 0)) {
        ERROR("Could not read inode bitmap from disk\n");
        mutex_up(&ext2_meta->sb_lock);
        kfree((void*) inode_bitmap);
        return EIO;
    }
    if (0 == BITFIELD_GET_BIT(inode_bitmap, inode_in_group)) {
        PANIC("Trying to free unallocated inode\n");
    }
    /*
     * Mark inode as unused and write bitmap back to disk
     */
    BITFIELD_CLEAR_BIT(inode_bitmap, inode_in_group);
    if (bc_write_bytes(bgd->bg_inode_bitmap, BLOCK_SIZE, (void*) inode_bitmap, ext2_meta->device, 0)) {
        PANIC("Could not write inode bitmap to disk\n");
        mutex_up(&ext2_meta->sb_lock);
        kfree((void*) inode_bitmap);
        return EIO;
    }
    /*
     * Overwrite entry in inode table with zeroes
     */
    memset((void*) &ext2_inode, 0, sizeof(ext2_inode_t));
    if (bc_write_bytes(bgd->bg_inode_table, sizeof(ext2_inode_t), (void*) &ext2_inode, ext2_meta->device,
            inode_in_group*sizeof(ext2_inode_t))) {
        PANIC("Could not write inode bitmap to disk\n");
        kfree((void*) inode_bitmap);
        mutex_up(&ext2_meta->sb_lock);
        return EIO;
    }
    /*
     * Update counters - if this is a directory, do not forget free directory counter
     */
    if (S_ISDIR(inode->mode))
        bgd->bg_used_dirs_count--;
    bgd->bg_free_inodes_count++;
    ext2_meta->ext2_super->s_free_inode_count++;
    if (put_meta(ext2_meta)) {
        PANIC("Could not write metadata back to disk\n");
        mutex_up(&ext2_meta->sb_lock);
        kfree((void*) inode_bitmap);
        return EIO;
    }
    mutex_up(&ext2_meta->sb_lock);
    kfree((void*) inode_bitmap);
    return 0;
}

/*
 * Allocate a free inode within a block group. No locks are
 * acquired - this needs to be done by the caller
 * Parameters:
 * @ext2_meta - a pointer to the ext2 metadata structure
 * @block_group_nr - number of the block group
 * @isdir - the allocated inode is a directory
 * @errno - this flag will be set if an error occured
 * Return value:
 * the inode number or 0 if no free inode could be found
 */
static u32 allocate_inode_in_group(ext2_metadata_t* ext2_meta, int block_group_nr, int isdir, int* errno) {
    u8* inode_bitmap;
    ext2_bgd_t* bgd = ext2_meta->bgdt + block_group_nr;
    ext2_superblock_t* ext2_super = ext2_meta->ext2_super;
    u32 inodes_in_group;
    int rc;
    int i;
    u32 inode_nr;
    if (0 == (inode_bitmap = (u8*) kmalloc(BLOCK_SIZE))) {
        ERROR("Could not allocate memory for inode bitmap\n");
        return ENOMEM;
    }
    /*
     * We first read the inode bitmap for this group. We can get the block
     * number from the field bg_block_bitmap
     */
    rc = bc_read_bytes(bgd->bg_inode_bitmap, BLOCK_SIZE, (void*) inode_bitmap,
            ext2_meta->device, 0);
    if (rc) {
        ERROR("Could not read inode bitmap from device\n");
        *errno = rc;
        kfree((void*) inode_bitmap);
        return 0;
    }
    /*
     * Next we need to find out how many inodes our block group has. If the block group is
     * the last block group on the file system, i.e. of the block_group_nr is equal to the number
     * of entries in the block group descriptor table, the number of inodes is computed as s_inodes_count % s_inodes_per_group
     * unless s_inodes_count is an exact multiple of s_inodes_per_group
     * If this is not the last block, the number of inodes is s_inodes_per_group.
     */
    if ((block_group_nr == ext2_meta->bgdt_size - 1)
            && (ext2_super->s_inodes_count % ext2_super->s_inodes_per_group)) {
        inodes_in_group = ext2_super->s_inodes_count
                % ext2_super->s_inodes_per_group;
    }
    else {
        inodes_in_group = ext2_super->s_inodes_per_group;
    }
    /*
     * Now scan inode bitmap until we find a free slot. Recall
     * that by convention, inode 1 is the first inode (not inode zero!)
     */
    for (i = 0; i < inodes_in_group; i++) {
        if (0 == BITFIELD_GET_BIT(inode_bitmap,i)) {
            inode_nr = i+1 + block_group_nr*ext2_super->s_inodes_per_group;
            EXT2_DEBUG("Allocated inode %d\n", inode_nr);
            BITFIELD_SET_BIT(inode_bitmap,i);
            break;
        }
    }
    /*
     * If we have found a free inode, write the changed inode map back to disk
     */
    if (inode_nr) {
        rc = bc_write_bytes(bgd->bg_inode_bitmap, BLOCK_SIZE,
                (void*) inode_bitmap, ext2_meta->device, 0);
        if (rc) {
            PANIC("Could not write inode bitmap to device\n");
            *errno = rc;
            kfree((void*) inode_bitmap);
            return 0;
        }
        bgd->bg_free_inodes_count--;
        ext2_super->s_free_inode_count--;
        /*
         * If the inode represents a directory, we need to increase the used directory counter
         * as well
         */
        if (isdir) {
            bgd->bg_used_dirs_count++;
            if (0 == bgd->bg_used_dirs_count) {
                PANIC("Overflow in bg_used_dirs_count\n");
            }
        }
        /*
         * Write block group descriptor and superblock table back to disk
         */
        if (put_meta(ext2_meta)) {
            PANIC("Could not write file system meta data to disk - disk write error\n");
            kfree((void*) inode_bitmap);
            *errno = rc;
            return 0;
        }
    }
    kfree((void*) inode_bitmap);
    return inode_nr;
}

/*
 * Utility function to allocate a free inode number. Locks need to be acquired
 * by caller
 * Parameter:
 * @ext2_meta - pointer to the ext2 metadata structure
 * @isdir - the new inode is a directory
 * @errno - an error number which is set when an error occurred
 * Return value:
 * an inode nr or zero if no free inode could be found
 * Locks: lock on superblock for ext2 metadata structure
 */
static u32 allocate_inode(ext2_metadata_t* ext2_meta, int isdir, int* errno) {
    u32 inode_nr = 0;
    int i;
    sem_down(&ext2_meta->sb_lock);
    /*
     * Shortcut - return immediately if superblock tells
     * us that there is no free inode
     */
    if (0 == ext2_meta->ext2_super->s_free_inode_count) {
        EXT2_DEBUG("No free inode on device\n");
        mutex_up(&ext2_meta->sb_lock);
        return 0;
    }
    /*
     * Walk block group descriptor list until we find a free
     * inode
     */
    for (i=0; i<ext2_meta->bgdt_size;i++) {
        if (ext2_meta->bgdt[i].bg_free_inodes_count) {
            /*
             * Found free slot - get inode from there
             */
            inode_nr = allocate_inode_in_group(ext2_meta, i, isdir, errno);
            if (inode_nr)
                break;
        }
    }
    mutex_up(&ext2_meta->sb_lock);
    return inode_nr;
}


/****************************************************************************************
 * For each device known to the file system which contains a valid EXT2 file system,    *
 * a superblock and a metadata structure (i.e. an instance of ext2_metadata) are        *
 * kept in a list. These functions handle entries in this list                          *
 ***************************************************************************************/

/*
 * Given a device, check internal list of superblocks
 * and return metadata for this device if present
 * Parameter:
 * @device - the device for which we are looking for a superblock
 * Return value:
 * 0 if the operation failed
 * a pointer to the metadata structure otherwise
 * Reference counts:
 * -reference count of returned metadata structure is increased by one
 */
static ext2_metadata_t* get_meta(dev_t device) {
    ext2_metadata_t* ret = 0;
    LIST_FOREACH(ext2_metadata_head, ret) {
        if ((ret->device == device) && (ret->reference_count > 0)) {
            ret->reference_count++;
            return ret;
        }
    }
    return 0;
}

/*
 * This function is like get_meta, but gets the lock on the
 * list of superblocks
* Parameter:
 * @device - the device for which we are looking for a superblock
 * Return value:
 * 0 if the operation failed
 * a pointer to the metadata structure otherwise
 * Locks:
 * ext2_metadata_lock - only acquired if have_lock = 0
 * Reference counts:
 * -reference count of returned metadata structure is increased by one
 */
static ext2_metadata_t* get_meta_lock(dev_t device) {
    u32 eflags;
    ext2_metadata_t* ret = 0;
    spinlock_get(&ext2_metadata_lock, &eflags);
    ret = get_meta(device);
    spinlock_release(&ext2_metadata_lock, &eflags);
    return ret;
}

/*
 * Clone a reference to an ext2 metadata structure
 * by increasing its reference count
 * Parameter:
 * @meta - the ext2 metadata structure to be cloned
 * Return value:
 * the cloned structure
 * Locks:
 * lock on list of superblocks (ext2_metadata_lock)
 * Reference counts:
 * - reference count of ext2 metadata structure is incremented by one
 */
static ext2_metadata_t* clone_meta(ext2_metadata_t* meta) {
    u32 eflags;
    spinlock_get(&ext2_metadata_lock, &eflags);
    meta->reference_count++;
    spinlock_release(&ext2_metadata_lock, &eflags);
    return meta;
}

/*
 * Utility function to allocate space for an
 * ext2 metadata structure.
 * Return value:
 * a pointer to the newly allocated structure or 0 if
 * the operation failed
 * Reference counts:
 * -The reference count of the structure will be set to one
 */
static ext2_metadata_t* new_meta() {
    ext2_metadata_t* meta = 0;
    if (0 == (meta = (ext2_metadata_t*) kmalloc(sizeof(ext2_metadata_t)))) {
        ERROR("Could not get superblock from disk - out of memory\n");
        return 0;
    }
    if (0 == (meta->super = (superblock_t*) kmalloc(sizeof(superblock_t)))) {
        ERROR("Could not get superblock from disk - out of memory\n");
        return 0;
    }
    if (0 == (meta->ext2_super = (ext2_superblock_t*) kmalloc(sizeof(ext2_superblock_t)))) {
        ERROR("Could not get superblock from disk - out of memory\n");
        return 0;
    }
    meta->bgdt = 0;
    meta->inodes_head = 0;
    meta->inodes_tail = 0;
    meta->reference_count = 1;
    spinlock_init(&(meta->lock));
    sem_init(&meta->sb_lock, 1);
    return meta;
}

/*
 * Destroy a metadata structure and return the allocated memory
 * Parameter:
 * @meta - the data to be destroyed
 */
static void destroy_meta(ext2_metadata_t* meta) {
    ext2_inode_data_t* current;
    ext2_inode_data_t* next;
    if (meta->ext2_super)
        kfree(meta->ext2_super);
    if (meta->bgdt)
        kfree(meta->bgdt);
    if (meta->super)
        kfree(meta->super);
    current = meta->inodes_head;
    while (current) {
        next = current->next;
        destroy_ext2_inode_data(current);
        current = next;
    }
}

/*
 * Utility function to fill a generic superblock
 * based on the information in the ext2 superblock
 * Parameter:
 * @meta - the ext2 superblock meta data
 */
static void init_super(ext2_metadata_t* meta) {
    meta->super->device = meta->device;
    meta->super->get_inode = fs_ext2_get_inode;
    meta->super->release_superblock = fs_ext2_release_superblock;
    meta->super->root = EXT2_ROOT_INODE;
    meta->super->data = (void*) meta;
    meta->super->is_busy = fs_ext2_is_busy;
}

/*
 * Get a superblock from an ext2 file system. This function
 * will also add the superblock to the list of superblocks and
 * read the corresponding block group descriptor table from disk
 * as well
 * Parameter:
 * @device - the device on which the superblock is located
 * Return value:
 * a pointer to the superblock or 0 if the operation failed
 * Locks:
 * ext2_metadata_lock
 * Reference counts:
 * - reference count of returned superblock is incremented by one
 */
superblock_t* fs_ext2_get_superblock(dev_t device) {
    ext2_metadata_t* meta = 0;
    ext2_metadata_t* check = 0;
    u32 eflags;
    /*
     * First scan list to see whether we have loaded the
     * superblock before. Note that this will increase the
     * reference count of the superblock by one
     */
    if ((meta = get_meta_lock(device)))
        return meta->super;
    /*
     * Not in list. Get it from disk
     */
    meta = read_meta(device);
    /*
     * Lock list. Check that entry is still not there, i.e. it has not
     * been added by a concurrent read, then add it
     */
    spinlock_get(&ext2_metadata_lock, &eflags);
    if (0 == (check = get_meta(device))) {
        LIST_ADD_END(ext2_metadata_head, ext2_metadata_tail, meta);
        /*
         * Set up generic superblock structure
         */
        init_super(meta);
        spinlock_release(&ext2_metadata_lock, &eflags);
        return meta->super;
    }
    /*
     * If we get here, another thread has already added the entry
     * Drop our entry and return entry already in list
     */
    destroy_meta(meta);
    kfree((void*) meta);
    spinlock_release(&ext2_metadata_lock, &eflags);
    return check->super;
}

/*
 * Release a superblock, i.e. decrease the reference count
 * by one and destroy the superblock if the reference count
 * reaches zero
 * Parameter:
 * @superblock - the superblock to be released
 * Locks:
 * ext2_metadata_lock - lock to protect the list of superblocks
 */
void fs_ext2_release_superblock(superblock_t* superblock) {
    u32 eflags;
    ext2_metadata_t* meta;
    EXT2_DEBUG("Releasing superblock of device %x\n", superblock->device);
    spinlock_get(&ext2_metadata_lock, &eflags);
    meta = (ext2_metadata_t*) superblock->data;
    meta->reference_count--;
    if (0 == meta->reference_count) {
        EXT2_DEBUG("Reference count of superblock dropped to zero\n");
        LIST_REMOVE(ext2_metadata_head, ext2_metadata_tail, meta);
        destroy_meta(meta);
        kfree((void*) meta);
    }
    spinlock_release(&ext2_metadata_lock, &eflags);
}

/****************************************************************************************
 * Attached to each superblock, there is a list of associated cached inodes. This cache *
 * is managed by the following functions                                                *
 ****************************************************************************************/

/*
 * Utility function to create an inode data structure. This function will allocate
 * memory for this structure, it is the responsibility of the caller to free it again
 * Parameter:
 * @inode - the inode to which the structure refers
 * @ext2_inode - the ext2 inode to which the structure refers
 * @meta - the ext2 metadata to which the structure refers
 * Return value:
 * the newly created ext2 inode data structure
 * Reference counts:
 * - the reference count of the newly created structure is set to one
 *
 */
static ext2_inode_data_t* init_ext2_inode_data(inode_t* inode, ext2_inode_t* ext2_inode, ext2_metadata_t* meta) {
    ext2_inode_data_t* ext2_inode_data = 0;
    if (0 == (ext2_inode_data = (ext2_inode_data_t*) kmalloc(sizeof(ext2_inode_data_t)))) {
        ERROR("Could not allocate memory for ext2 inode data structure\n");
        return 0;
    }
    ext2_inode_data->inode = inode;
    ext2_inode_data->ext2_inode = ext2_inode;
    ext2_inode_data->ext2_meta = meta;
    ext2_inode_data->reference_count = 1;
    return ext2_inode_data;
}

/*
 * Destroy an ext2 inode data structure and free the allocated
 * memory
 * Parameter:
 * @ext2_inode_data - the data structure to be freed
 */
static void destroy_ext2_inode_data(ext2_inode_data_t* ext2_inode_data) {
    if (ext2_inode_data->ext2_inode)
        kfree(ext2_inode_data->ext2_inode);
    if (ext2_inode_data->inode)
        kfree(ext2_inode_data->inode);
}

/*
 * Utility function to initialize an inode structure from
 * its associated ext2 inode structure. Note that this function
 * allocates memory for the inode structure, it is the responsibility
 * of the caller to free that memory again
 * Parameters:
 * @inode - the inode
 * @ext2_inode - the ext2 inode
 * @ext2_meta - the ext2 metadata structure
 * @mode - the file mode
 */
static inode_t* init_inode(ext2_inode_t* ext2_inode, ext2_metadata_t* ext2_meta, u32 inode_nr) {
    inode_t* inode = 0;
    if (0 == (inode = (inode_t*) kmalloc(sizeof(inode_t)))) {
        ERROR("Could not allocate memory for inode\n");
        return 0;
    }
    inode->data = ext2_inode;
    inode->dev = ext2_meta->device;
    inode->inode_nr = inode_nr;
    inode->mode = ext2_inode->i_mode;
    inode->mount_point = 0;
    inode->mtime = ext2_inode->i_mtime;
    inode->atime = ext2_inode->i_atime;
    inode->size = ext2_inode->i_size;
    inode->iops = &ext2_iops;
    inode->owner = 0;
    inode->group = 0;
    inode->link_count = ext2_inode->i_link_count;
    inode->super = ext2_meta->super;
    rw_lock_init(&inode->rw_lock);
    /*
     * For ext2, the device is stored in first direct
     * block if the inode represents a character or block device
     */
    inode->s_dev = (dev_t) ext2_inode->direct[0];
    return inode;
}

/*
 * Add a fully prepared inode to the cache - this
 * is just a wrapper around LIST_ADD which gets the necessary lock
 */
static void store_inode(ext2_metadata_t* ext2_metadata, ext2_inode_data_t* ext2_inode_data) {
    u32 eflags;
    spinlock_get(&ext2_metadata->lock, &eflags);
    LIST_ADD_END(ext2_metadata->inodes_head, ext2_metadata->inodes_tail, ext2_inode_data);
    spinlock_release(&ext2_metadata->lock, &eflags);
}

/*
 * Get an inode. This function first checks whether the inode is already
 * in the cache. If not, it is read from disk and added to the cache
 * Parameter:
 * @inode_nr  - the number of the inode
 * @meta - a pointer to the metadata for this file system
 * Return value:
 * a pointer to the inode or 0 if the operation failed
 * Locks:
 * spinlock in superblock meta data structure (meta->lock)
 * Reference counts:
 * - reference count of inode data structure is incremented by one
 */
static inode_t* get_inode(ino_t inode_nr, ext2_metadata_t* meta) {
    inode_t* inode;
    ext2_inode_t* ext2_inode;
    ext2_inode_data_t* ext2_inode_data;
    ext2_inode_data_t* check;
    u32 eflags;
    /*
     * Already in cache?
     */
    EXT2_DEBUG("Looking for inode %d in cache\n", inode_nr);
    spinlock_get(&meta->lock, &eflags);
    LIST_FOREACH(meta->inodes_head, ext2_inode_data) {
        if (ext2_inode_data->inode->inode_nr == inode_nr) {
            ext2_inode_data->reference_count++;
            spinlock_release(&meta->lock, &eflags);
            return ext2_inode_data->inode;
        }
    }
    /*
     * Inode is not yet in cache. Get it from disk. Make sure to release
     * spinlock first as this might involve sleeping
     */
    spinlock_release(&meta->lock, &eflags);
    if (0 == (ext2_inode = get_ext2_inode(inode_nr, meta))) {
        ERROR("Could not get ext2 inode from disk\n");
        return 0;
    }
    /*
     * Init inode data structure
     */
    if (0 == (inode = init_inode(ext2_inode, meta, inode_nr))) {
        ERROR("Could not allocate memory for inode\n");
        return 0;
    }
    /*
     * Build up inode metadata
     */
    if (0 == (ext2_inode_data = init_ext2_inode_data(inode, ext2_inode, meta))) {
        ERROR("Could not allocate memory\n");
        kfree((void*) inode);
        return 0;
    }
    /*
     * Another thread might have added an entry for this
     * inode in parallel in the meantime. To avoid duplicates,
     * we check the list again, this time while having the lock.
     */
    spinlock_get(&meta->lock, &eflags);
    LIST_FOREACH(meta->inodes_head, check) {
        if (check->inode->inode_nr == inode_nr) {
            destroy_ext2_inode_data(ext2_inode_data);
            kfree((void*) ext2_inode_data);
            spinlock_release(&meta->lock, &eflags);
            return check->inode;
        }
    }
    /*
     * Still not there - add it
     */
    LIST_ADD_END(meta->inodes_head, meta->inodes_tail, ext2_inode_data);
    inode->data = (void*) ext2_inode_data;
    spinlock_release(&meta->lock, &eflags);
    return inode;
}

/*
 * Get an inode
 * Parameter:
 * @device - the device on which the inode is located
 * @inode_nr  - the number of the inode
 * Return value:
 * a pointer to the inode or 0 if the operation failed
 * Reference counts:
 * - increase reference count of returned inode by one
 * - increase reference count of associated superblock by one
 */
inode_t* fs_ext2_get_inode(dev_t device, ino_t inode_nr) {
    ext2_metadata_t* meta;
    inode_t* inode = 0;
    /*
     * Walk table of mounted file systems until we find the device
     * Note that get_meta_lock will increase the reference count of
     * the superblock - which is fine, as we are going to add the
     * returned reference permanently to the inode in get_inode
     */
    meta = get_meta_lock(device);
    if (meta) {
        inode = get_inode(inode_nr, meta);
    }
    return inode;
}

/*
 * Clone a reference to an inode by incrementing
 * its reference count
 * Parameters:
 * @inode - the inode which is cloned
 * Return value:
 * a pointer to the inode
 * Locks:
 * lock on ext2 metadata structure for this inode
 * Reference counts:
 * - increase reference count of returned inode by one
 * - increase reference count of associated superblock by one
 */
inode_t* fs_ext2_inode_clone(inode_t* inode) {
    u32 eflags;
    KASSERT(inode);
    ext2_inode_data_t* idata = (ext2_inode_data_t*) inode->data;
    KASSERT(idata);
    /*
     * Use clone to get a reference to the metadata structure
     * as we need to increase the reference count of that structure
     * as well. We do this in the same order as in get_inode,
     * i.e. we first clone the metadata structure, then the inode
     * itself
     */
    ext2_metadata_t* meta = clone_meta(idata->ext2_meta);
    spinlock_get(&meta->lock, &eflags);
    idata->reference_count++;
    spinlock_release(&meta->lock, &eflags);
    return inode;
}

/*
 * Check if there are any open inodes referring to this part of the file system,
 * i.e. if there is an inode in the cache except the root inode or if the
 * reference count of the root inode is greater than one
 * Parameter:
 * @super - the super block
 * Return value:
 * 1 if the device is busy
 * 0 if the device is not busy
 * Locks:
 * lock on list of superblocks (ext2_metadata_lock)
 */
int fs_ext2_is_busy(superblock_t* super) {
    ext2_metadata_t* meta = (ext2_metadata_t*) super->data;
    u32 eflags;
    int rc = 0;
    if (0 == meta)
        return 0;
    spinlock_get(&ext2_metadata_lock, &eflags);
    if (meta->reference_count > 1)
        rc = 1;
    spinlock_release(&ext2_metadata_lock, &eflags);
    return rc;
}


/****************************************************************************************
 * The EXT2 file system stores blocks which belong to a file in a tree-like structure   *
 * within the inode. The following set of functions is used to navigate this tree       *
 ****************************************************************************************/

/*
 *  Load an indirect block (which can be a double indirect block, a single indirect block
 *  or a triple indirect block) into memory.
 *  If the block number requested is zero, a new block will be allocated, filled with
 *  zeroes and written to disk
 *  Parameters:
 *  @request - the request structure
 *  is written
 *  @block_nr - the number (i.e. logical block number on the device) of the block to be loaded
 *  @dirty - a flag which is set by this function if the block number was initially zero and has been replaced by the number
 *  @errno - this is set if an error occurred
 *  of a newly allocated block
 *  Return value:
 *  the address of the indirect block or 0 if the operation failed
 */
static u32* load_indirect_block(blocklist_walk_t* request, u32* block_nr,
        int* dirty, int* errno) {
    u32* indirect_block;
    /*
     * Try to allocate new block in memory first
     */
    if (0 == (indirect_block = (u32*) kmalloc(BLOCK_SIZE))) {
        ERROR("Could not allocate indirect block\n");
        return 0;
    }
    /*
     * If block_nr is zero, we are reading from a hole - just put zeroes
     * into buffer in case of a read or allocate a new block if requested
     * Otherwise read indirect block from disk
     */
    if (0 == *block_nr) {
        EXT2_DEBUG("Indirect block is zero - processing hole\n");
        memset((void*) indirect_block, 0, BLOCK_SIZE);
        if (1 == request->allocate) {
            /*
             * Allocate a new block,
             * fill it with zeroes and write it to *block_nr
             */
            *block_nr = allocate_block(request->ext2_meta,
                    request->block_group_nr, errno);
            if (0 == *block_nr) {
                kfree(indirect_block);
                return 0;
            }
            request->ext2_inode->i_blocks += (BLOCK_SIZE / 512);
            EXT2_DEBUG("Allocated new block %d\n", *block_nr);
            if (bc_write_bytes(*block_nr, BLOCK_SIZE, (void*) indirect_block,
                    request->device, 0)) {
                ERROR("Could not write newly allocated indirect block to disk\n");
                *errno = EIO;
                kfree(indirect_block);
                return 0;
            }
            *dirty = 1;
        }
    }
    /*
     * Block number is different from zero, so we have allocated
     * an indirect block previously
     */
    else {
        /*
         * Read indirect block from disk
         */
        EXT2_DEBUG("Reading indirect block %d from disk\n", *block_nr);
        if (bc_read_bytes(*block_nr, BLOCK_SIZE, (void*) indirect_block,
                request->device, 0)) {
            ERROR("Could not read indirect block from disk\n");
            *errno = EIO;
            kfree(indirect_block);
            return 0;
        }
    }
    return indirect_block;
}

/*
 * Callback function to read from a block. This function is called once
 * for every data block by walk_blocklist. It is responsible for performing
 * the appropriate action on the block and updating the fields bytes_processed
 * and blocks_processed of the request structure.
 * When it is invoked, bytes_processed and blocks_processed contain the number
 * of blocks and bytes already processed during the walk up to this point
 * Parameters:
 * @request - the request describing the block walk
 * @block_nr - the logical block number on the device of the block to be processed
 * Return value:
 * 0 upon success
 * EIO in case an error occurs
 */
static int read_block(blocklist_walk_t* request, u32 block_nr) {
    if (block_nr) {
        if (bc_read_bytes(block_nr, request->last_byte - request->first_byte + 1, request->data
                + request->bytes_processed, request->device, request->first_byte)) {
            ERROR("Error while reading from device\n");
            return EIO;
        }
    }
    else {
        memset(request->data + request->bytes_processed, 0,request->last_byte - request->first_byte + 1 );
    }
    request->bytes_processed += request->last_byte - request->first_byte + 1;
    request->blocks_processed++;
    return 0;
}

/*
 * Callback function to write to a block. This function is called once
 * for every data block by walk_blocklist. It is responsible for performing
 * the appropriate action on the block and updating the fields bytes_processed
 * and blocks_processed of the request structure.
 * When it is invoked, bytes_processed and blocks_processed contain the number
 * of blocks and bytes already processed during the walk up to this point
 * Parameters:
 * @request - the request describing the block walk
 * @block_nr - the logical block number on the device of the block to be processed
 * Return value:
 * 0 upon success
 * EIO if an error occurred
 */
static int write_block(blocklist_walk_t* request, u32 block_nr) {
    if (0 == block_nr) {
        ERROR("Block number 0 not valid for writing\n");
        return EIO;
    }
    if (bc_write_bytes(block_nr, request->last_byte - request->first_byte + 1, request->data
            + request->bytes_processed, request->device, request->first_byte)) {
        ERROR("Error while writing to device\n");
        return EIO;
    }
    request->bytes_processed += request->last_byte - request->first_byte + 1;
    request->blocks_processed++;
    return 0;
}

/*
 * Callback function to truncate to a block. This function is called once
 * for every data block by walk_blocklist. It is responsible for performing
 * the appropriate action on the block and updating the fields bytes_processed
 * and blocks_processed of the request structure.
 * When it is invoked, bytes_processed and blocks_processed contain the number
 * of blocks and bytes already processed during the walk up to this point
 * Parameters:
 * @request - the request describing the block walk
 * @block_nr - the logical block number on the device of the block to be processed
 * Return value:
 * 0 if the operation was successful
 * EIO if an error occurred
 */
static int truncate_block(blocklist_walk_t* request, u32 block_nr) {
    if (block_nr) {
        if (deallocate_block(request->ext2_meta, block_nr)) {
            ERROR("Could not deallocate block %d\n", block_nr);
            return EIO;
        }
        request->ext2_inode->i_blocks-=2;
    }
    request->bytes_processed += request->last_byte - request->first_byte + 1;
    request->blocks_processed++;
    return 0;
}

/*
 * This utility function will process a part of a file
 * determined by a blocklist, i.e. an array of dwords
 * containing block addresses. For each data block, it
 * will invoke the callback function request->process_block
 * Parameters:
 * @request - a pointer to the block walk structure
 * @blocklist - a pointer to the address of the first block
 * @blocks - number of blocks in the blocklist
 * @dirty - this flag will be set if the blocklist has been modified and needs
 * to be written back to disk
 * Return value:
 * EIO if the operation failed
 * 0 if the operation is successful
 */
static int walk_blocklist(blocklist_walk_t* request, u32* blocklist,
        u32 blocks, int* dirty) {
    u32 i;
    int errno = 0;
    /*
     * Walk through blocklist
     */
    for (i = 0; i < blocks; i++) {
        /*
         * Allocate block if needed
         */
        if ((1 == request->allocate) && (0 == blocklist[i])) {
            /*
             * If the entry in the blocklist is zero, this implies that we hit upon a hole,
             * i.e. an unallocated area. In this case we need to allocate a new block and
             * add it to the blocklist if the flag allocate in the request structure is set
             */
            blocklist[i] = allocate_block(request->ext2_meta,
                    request->block_group_nr, &errno);
            if (0 == blocklist[i]) {
                /*
                 * If an error occured, return EIO. Otherwise simply return
                 * zero and set the the abort flag in the request so that the
                 * operation aborts silently
                 */
                if (errno) {
                    ERROR("Could not allocate additional block for file\n");
                    return EIO;
                }
                else {
                    EXT2_DEBUG("Device full\n");
                    request->abort = 1;
                    return 0;
                }
            }
            EXT2_DEBUG("Allocated block %d\n", blocklist[i]);
            /*
             * Recall that i_blocks is measured in units of 512 bytes!!
             */
            (request->ext2_inode->i_blocks) += BLOCK_SIZE / 512;
            *dirty = 1;
        }
        /*
         * Determine first and last byte within this block which we
         * will read. For the first block, the first byte is the offset, for all other blocks the
         * first byte is zero. For the last block, the last byte is the last byte (offset+bytes-1)
         * to be read/written modulo the block size, for all others it is the last byte within the block
         */
        request->first_byte = (request->blocks_processed == 0) ? (request->offset
                % BLOCK_SIZE) : 0;
        request->last_byte = (request->blocks_processed == (request->last_block
                - request->first_block)) ? ((request->bytes + request->offset - 1)
                        % BLOCK_SIZE) : (BLOCK_SIZE - 1);
        if (request->process_block(request, blocklist[i])) {
            ERROR("Error while processing block %d", blocklist[i]);
            return EIO;
        }
        /*
         * If the flag request->zero is set, set the blocklist entry in the inode to zero. We also mark
         * the blocklist as dirty so it will be written back to disk
         */
        if (1 == request->zero) {
            blocklist[i] = 0;
            *dirty = 1;
        }
    }
    return 0;
}

/*
 * Check whether an indirect block is empty, i.e. whether all entries
 * are zero. 
 */
static int blocklist_is_empty(u32* indirect_block, u32 blocks) {
    int i;
    for (i = 0; i < blocks; i++) {
        if (indirect_block[i]) {
            return 0;
        }
    }
    return 1;
}

/*
 * Utility function to read or write all blocks referred by an indirect block
 * This function will load an indirect block, i.e. a block containing
 * dword pointers to blocks on disk. Based upon the request structure, it
 * will determine the first entry and the last entry to use from this indirect block
 * and call walk_blocklist to walk the blocklist spanned between these two entries
 * If there is no overlap between the area requested in @request and the area addressed
 * via this indirect block, the function returns immediately
 * Parameter:
 * @request - the request structure for this operation
 * @indirect_start - first block within the file addressed via this indirect block
 * @block_nr - number of indirect block on file system
 * @dirty - flag which the function will set if it updates the block nr
 * Return value:
 * 0 if the operation is successful
 * EIO if reading of the blocks failed
 */
static int walk_indirect_block(blocklist_walk_t* request, u32 indirect_start,
        u32* block_nr, int* dirty) {
    u32 indirect_end = indirect_start + EXT2_INDIRECT_BLOCKS - 1;
    u32* indirect_block;
    u32 actual_start;
    u32 actual_end;
    int blocklist_dirty = 0;
    int errno = 0;
    int indirect_block_empty = 0;
    /*
     * Determine first and last indirect block to read
     */
    actual_start = (request->first_block < indirect_start) ? indirect_start
            : request->first_block;
    actual_end = (request->last_block <= indirect_end) ? request->last_block
            : indirect_end;
    if (actual_end < actual_start)
        return 0;
    /*
     * Load the indirect block
     */
    indirect_block = load_indirect_block(request, block_nr, dirty, &errno);
    if (0 == indirect_block) {
        if (errno) {
            ERROR("Could not load indirect block\n");
            return EIO;
        }
        else {
            EXT2_DEBUG("Device full\n");
            request->abort = 1;
            return 0;
        }
    }
    /*
     * Use this as blocklist to read more blocks
     * We start with offset indirect1_start - indirect_start
     * in the block list
     */
    if (walk_blocklist(request, indirect_block
            + (actual_start - indirect_start), actual_end - actual_start + 1,
            &blocklist_dirty)) {
        ERROR("Could not walk blocklist\n");
        kfree((void*) indirect_block);
        return EIO;
    }
    /*
     * If walk_blocklist has changed the blocklist, write it back to disk
     */
    if (blocklist_dirty) {
        if (bc_write_bytes(*block_nr, BLOCK_SIZE, (void*) indirect_block,
                request->device, 0)) {
            ERROR("Could not write changed indirect block to disk\n");
            kfree((void*) indirect_block);
            return EIO;
        }
    }
    /*
     * We need a special processing for a truncate. We know that the part of
     * the blocklist that we have visited, i.e. starting at entry 
     * (actual_start - indirect_start)
     * has been zeroed out by the walk. For a truncate, we also truncate right
     * to the end of the file, so we always walk a full blocklist or to the end of
     * the file. So in order to check that we can now safely deallocate the block,
     * we only have to check that there are no further non-zero entries 
     * in the first few entries that we have not seen
     */
    indirect_block_empty = blocklist_is_empty(indirect_block, actual_start - indirect_start);
    kfree((void*) indirect_block);

    /*
     * If requested deallocate indirect block
     */
    if (*block_nr && (1 == request->deallocate)) {
        if (indirect_block_empty) {
            EXT2_DEBUG("Deallocating indirect block %d\n", *block_nr);
            if (deallocate_block(request->ext2_meta, *block_nr)) {
                ERROR("Could not deallocate indirect block %d\n", *block_nr);
                return EIO;
            }
            request->ext2_inode->i_blocks-=2;
            *dirty=1;
            /*
             * Signal to the caller that we have deallocated the entire block
             */
            *block_nr = 0;
        }
        else {
            EXT2_DEBUG("Skipping deallocation as block %d is not empty\n", *block_nr);
        }
    }
    return 0;
}

/*
 * Utility function to walk a double indirect block
 * This function will load the double indirect block stored on the disk
 * at block @block_nr. For each entry in this block, i.e. for each
 * indirect block number stored there, it will invoke the function
 * walk_indirect_block to read and process this block
 * If there is no overlap between the area described in the read
 * request @request and the area in the file addressed via this double
 * indirect block, the function will return immediately
 * Parameters:
 * @request - the request structure for this operation
 * @double_indirect_start - the first block within the file addressed via this double indirect block
 * @block_nr - the absolute block number of the double indirect block on the device
 * @dirty - a flag which will be set by the function if if modifies the content of block_nr
 * Return value:
 * 0 upon success
 * EIO if the  operation failed
 */
static int walk_double_indirect_block(blocklist_walk_t* request,
        u32 double_indirect_start, u32* block_nr, int* dirty) {
    u32 double_indirect_end = double_indirect_start
            + EXT2_DOUBLE_INDIRECT_BLOCKS - 1;
    u32* double_indirect_block = 0;
    int errno = 0;
    u32 block_ptr;
    int blocklist_dirty = 0;
    u32 indirect_start;
    if ((request->first_block > double_indirect_end) || (request->last_block
            < double_indirect_start))
        return 0;
    /*
     * Read double indirect block into memory first
     */
    EXT2_DEBUG("Starting walk through double indirect block %d, reading data from disk\n", *block_nr);
    double_indirect_block = load_indirect_block(request, block_nr, dirty, &errno);
    if (0 == double_indirect_block) {
        if (errno) {
            ERROR("Could not load double indirect block\n");
            return EIO;
        }
        else {
            EXT2_DEBUG("Device full\n");
            request->abort = 1;
            return 0;
        }
    }
    /*
     * Now loop through blocks stored in double indirect blocks
     * until we have read enough or leave the double indirect area
     */
    block_ptr = 0;
    while ((request->blocks_processed < request->last_block
            - request->first_block + 1) && (block_ptr < EXT2_INDIRECT_BLOCKS)) {
        /*
         * Determine absolute block number of first block
         * within the file covered by this indirect block
         */
        indirect_start = double_indirect_start + (block_ptr
                * EXT2_INDIRECT_BLOCKS);
        /*
         * Now process this indirect block
         */
        EXT2_DEBUG("Calling walk_indirect_block on entry %d\n", block_ptr);
        if (walk_indirect_block(request, indirect_start, double_indirect_block
                + block_ptr, &blocklist_dirty)) {
            ERROR("Reading indirect block failed\n");
            kfree((void*)double_indirect_block);
            return EIO;
        }
        if (request->abort)
            break;
        block_ptr++;
    }
    /*
     * Check whether walk_indirect_block has modified any of the entries in the
     * double indirect block. If yes, write it back to disk
     */
    if (blocklist_dirty) {
        if (bc_write_bytes(*block_nr, BLOCK_SIZE,
                (void*) double_indirect_block, request->device, 0)) {
            ERROR("I/O error while writing changed block to device\n");
            kfree((void*)double_indirect_block);
            return EIO;
        }
    }
    int double_indirect_block_empty = blocklist_is_empty(double_indirect_block, block_ptr);
    kfree(double_indirect_block);
    /*
     * If requested deallocate double indirect block but only
     * if no blocks are used in front of the area that we have walked
     */
    if (*block_nr && (1 == request->deallocate)) {
        if (double_indirect_block_empty) {
            EXT2_DEBUG("Deallocating double indirect block %d\n", *block_nr);
            if (deallocate_block(request->ext2_meta, *block_nr)) {
                ERROR("Could not deallocate double indirect block %d\n", *block_nr);
                return EIO;
            }
            request->ext2_inode->i_blocks-=2;
            *block_nr = 0;
        }
        else {
            EXT2_DEBUG("Skipping deallocation as double indirect block %d is not empty\n", *block_nr);
        }
    }
    return 0;
}

/*
 * Utility function to walk a triple indirect block
 * This function will load the triple indirect block
 * which has absolute address @block_nr from the device
 * For each double word in this block, it will call
 * walk_double_indirect_block
 * Parameters:
 * @request - the original read request
 * @triple_indirect_start - the first block within the file addressed via this triple indirect block
 * @block_nr - the block number
 * Return value:
 * EIO if the read from disk failed
 * 0 upon success
 */
static int walk_triple_indirect_block(blocklist_walk_t* request,
        u32 triple_indirect_start, u32* block_nr) {
    u32* triple_indirect_block;
    u32 block_ptr;
    int blocklist_dirty = 0;
    int dirty = 0;
    int errno = 0;
    /*
     * Load triple indirect block from disk
     */
    triple_indirect_block = load_indirect_block(request, block_nr, &dirty, &errno);
    if (0 == triple_indirect_block) {
        if (errno) {
            ERROR("Could not load triple indirect block\n");
            return EIO;
        }
        else {
            EXT2_DEBUG("No space left on device\n");
            request->abort=1;
            return 0;
        }
    }
    /*
     * Now loop through all entries and process the double indirect
     * block pointed to by each of the entries
     */
    block_ptr = 0;
    while ((request->blocks_processed < request->last_block
            - request->first_block + 1) && (block_ptr < EXT2_INDIRECT_BLOCKS)) {
        if (walk_double_indirect_block(request, triple_indirect_start
                + block_ptr * EXT2_DOUBLE_INDIRECT_BLOCKS,
                triple_indirect_block + block_ptr, &blocklist_dirty)) {
            ERROR("Could not read double indirect block from disk\n");
            kfree((void*)triple_indirect_block);
            return EIO;
        }
        if (request->abort)
            break;
        block_ptr++;
    }
    /*
     * If the walk through the triple indirect block has changed any of
     * the entries, write entire block back to disk
     */
    if (blocklist_dirty) {
        if (bc_write_bytes(*block_nr, BLOCK_SIZE,
                (void*) triple_indirect_block, request->device, 0)) {
            ERROR("Could not read triple indirect block from disk\n");
            kfree((void*)triple_indirect_block);
            return EIO;
        }
    }
    int triple_indirect_block_empty = blocklist_is_empty(triple_indirect_block, block_ptr);
    kfree((void*)triple_indirect_block);
    /*
     * If requested deallocate triple indirect block
     */
    if (*block_nr && (1 == request->deallocate)) {
        if (triple_indirect_block_empty) {
            if (deallocate_block(request->ext2_meta, *block_nr)) {
                ERROR("Could not deallocate indirect block %d\n", *block_nr);
                return EIO;
            }
            request->ext2_inode->i_blocks-=2;
            *block_nr = 0;
        }
        else {
            EXT2_DEBUG("Skipping deallocation of triple indirect block %d as block is not empty\n", *block_nr);
        }
    }
    return 0;
}
/*
 * Utility function to initialize a blocklist walk
 * Parameters:
 * @request - the request structure to be initialized
 * @ext2_inode - the ext2 inode from which we read
 * @bytes - the number of bytes to read
 * @offset - the offset at which we start reading
 * @data - the buffer to which we write
 * @device - the device from which we read
 * @op - operation to be performed
 * @ext2_meta - ext2 metadata structure to use
 * @block_group_nr - number of block group in which the inode is located
 */
static void init_request(blocklist_walk_t* request, ext2_inode_t* ext2_inode,
        ssize_t bytes, off_t offset, void* data, dev_t device, int op,
        ext2_metadata_t* ext2_meta, u32 block_group_nr) {
    request->bytes = bytes;
    if ((request->bytes + offset > ext2_inode->i_size) && (EXT2_OP_READ == op))
        request->bytes = ext2_inode->i_size - offset;
    request->offset = offset;
    request->data = data;
    request->device = device;
    request->abort= 0;
    if (EXT2_OP_READ == op) {
        request->allocate = 0;
        request->deallocate = 0;
        request->zero = 0;
        request->process_block = read_block;
    }
    else  if (EXT2_OP_WRITE == op) {
        request->allocate = 1;
        request->deallocate = 0;
        request->zero = 0;
        request->process_block = write_block;
    }
    else if (EXT2_OP_TRUNC == op) {
        request->allocate = 0;
        request->deallocate = 1;
        request->zero = 1;
        request->process_block = truncate_block;
    }
    else
        PANIC("Invalid operation number\n");
    /*
     * First block containing data which we read
     */
    request->first_block = offset / BLOCK_SIZE;
    /*
     * Last block containing data which we read
     */
    request->last_block = (offset + request->bytes - 1) / BLOCK_SIZE;
    /*
     * Counters for processed blocks and processed bytes which
     * we will update as we go
     */
    request->blocks_processed = 0;
    request->bytes_processed = 0;
    /*
     * Ext2 metadata structure
     */
    request->ext2_meta = ext2_meta;
    /*
     * Block group number and inode
     */
    request->block_group_nr = block_group_nr;
    request->ext2_inode = ext2_inode;
}

/****************************************************************************************
 * The following functions implement reading from and writing to an inode               *
 ****************************************************************************************/

/*
 * Perform a specified operation on a given range of an inode
 * Parameters:
 * @inode - the inode from which we read
 * @bytes - the number of bytes to read
 * @offset - the offset in bytes at which we start reading
 * @op - EXT2_OP_READ to read, EXT2_OP_WRITE to write, EXT2_OP_TRUNC to deallocate all blocks used by the file
 * Return value:
 * number of bytes read if the operation was successful
 * -EIO if the operation failed
 * -ENOSPC if the device is full
 *
 * Note that the number of bytes read can be smaller than the number of bytes requested
 * if the end of the file has been reached.
 *
 * For a write operation, if the device is full, but at least one byte could be written,
 * the function will return the number of bytes written. If the number of bytes written is zero
 * because the disk is full, -ENOSPC is returned.
 *
 */
static ssize_t fs_ext2_inode_rw(struct _inode_t* inode, ssize_t bytes,
        off_t offset, void* data, int op) {
    blocklist_walk_t request;
    u32 direct_end;
    ext2_inode_t* ext2_inode = ((ext2_inode_data_t*) inode->data)->ext2_inode;
    ext2_metadata_t* ext2_meta = ((ext2_inode_data_t*) inode->data)->ext2_meta;
    ext2_superblock_t* ext2_super = ext2_meta->ext2_super;
    u32 block_group_nr = 0;
    int blocklist_dirty = 0;
    KASSERT(ext2_inode);
    /*
     * Validate parameters
     */
    if ((bytes <= 0) || ((offset >= ext2_inode->i_size) && (EXT2_OP_READ == op))) {
        return 0;
    }
    /*
     * Determine number of block group in which the inode is located
     */
    block_group_nr = (inode->inode_nr - 1) / ext2_super->s_inodes_per_group;
    /*
     * Set up request structure
     */
    init_request(&request, ext2_inode, bytes, offset, data, inode->dev, op,
            ext2_meta, block_group_nr);
    /*
     * Is a part of the requested area within the part of the file
     * which is accessed via the direct pointers?
     */
    if (request.first_block <= EXT2_LAST_DIRECT) {
        /* Determine last block within the direct area */
        direct_end = (request.last_block > EXT2_LAST_DIRECT) ? EXT2_LAST_DIRECT
                : request.last_block;
        /*
         * Use utility function to read/write blocks from the blocklist
         * starting at the address of direct block first_block within
         * the inode data structure
         */
        EXT2_DEBUG("Doing walk in direct area\n");
        if (walk_blocklist(&request, ext2_inode->direct + request.first_block,
                direct_end - request.first_block + 1, &blocklist_dirty)) {
            ERROR("Error while reading from direct area\n");
            return -EIO;
        }
    }
    /*
     * At this point we have already processed blocks_processed blocks and bytes_processed bytes
     * Continue to read/write within next area (indirect block) if needed
     */
    if ((request.blocks_processed < request.last_block - request.first_block
            + 1) && (request.first_block <= EXT2_LAST_INDIRECT)) {
        EXT2_DEBUG("Doing walk in indirect area\n");
        if (walk_indirect_block(&request, EXT2_LAST_DIRECT + 1,
                &(ext2_inode->indirect1), &blocklist_dirty)) {
            ERROR("Error while reading from indirect area\n");
            return -EIO;
        }
    }
    /*
     * Check whether we need walk the double indirect blocks
     * as well
     */
    if ((request.blocks_processed < request.last_block - request.first_block
            + 1) && (request.first_block <= EXT2_LAST_DOUBLE_INDIRECT) && (!request.abort)) {
        if (walk_double_indirect_block(&request, EXT2_LAST_INDIRECT + 1,
                &(ext2_inode->indirect2), &blocklist_dirty)) {
            ERROR("Error while reading from double indirect area\n");
            return -EIO;
        }
    }
    /*
     * Finally check whether there is a need to walk the triple indirect block
     */
    if ((request.blocks_processed < request.last_block - request.first_block
            + 1) && (!request.abort)) {
        if (walk_triple_indirect_block(&request, EXT2_LAST_DOUBLE_INDIRECT + 1,
                &(ext2_inode->indirect3))) {
            ERROR("Error while reading from triple indirect area\n");
            return -EIO;
        }
    }
    /*
      * Extend file size if needed
      */
    if ((request.offset + request.bytes_processed > ext2_inode->i_size) && (EXT2_OP_WRITE == op) && (request.bytes_processed>0)) {
        ext2_inode->i_size = request.offset + request.bytes_processed;
        inode->size = ext2_inode->i_size;
        EXT2_DEBUG("Updated file size to %d\n", inode->size);
    }
    /*
     * If we have written to the inode, update modification time and write inode back to disk
     * Also update copy in generic inode
     */
    if ((EXT2_OP_WRITE == op) || (EXT2_OP_TRUNC == op)) {
        ext2_inode->i_mtime = (u32) do_time(0);
        inode->mtime = ext2_inode->i_mtime;
        if (put_inode(ext2_meta, inode)) {
            ERROR("Could not write inode back to disk\n");
            return -EIO;
        }
    }
    if ((op==1) && (request.bytes_processed==0))
        return -ENOSPC;
    return request.bytes_processed;
}

/*
 * Update access time, modification time and mode from an inode, i.e.
 * propage the respective values from the inode_t structure to the EXT2
 * inode and the file system
 */
int fs_ext2_inode_flush(inode_t* inode) {
    ext2_inode_t* ext2_inode = ((ext2_inode_data_t*) inode->data)->ext2_inode;
    ext2_metadata_t* ext2_meta = ((ext2_inode_data_t*) inode->data)->ext2_meta;
    EXT2_DEBUG("Flushing inode\n");
    /*
     * Update times
     */
    ext2_inode->i_mtime = inode->mtime;
    ext2_inode->i_atime = inode->atime;
    /*
     * and mode
     */
    ext2_inode->i_mode = inode->mode;
    /*
     * and write inode back to disk
     */
    if (put_inode(ext2_meta, inode)) {
        ERROR("Could not write inode back to disk\n");
        return -EIO;
    }
    return 0;
}

/*
 * Read from an inode
 * Parameters:
 * @inode - the inode from which we read
 * @bytes - the number of bytes to read
 * @offset - the offset in bytes at which we start reading
 * Return value:
 * number of bytes read if the operation was successful
 * -EIO if the operation failed
 */
ssize_t fs_ext2_inode_read(struct _inode_t* inode, ssize_t bytes, off_t offset,
        void* data) {
    return fs_ext2_inode_rw(inode, bytes, offset, data, EXT2_OP_READ);
}

/*
 * Write to an inode.
 * Parameters:
 * @inode - the inode to which a write is requested
 * @bytes - the number of bytes to write
 * @offset - the offset into the inode at which we start to write
 * @data - a pointer to the data to be written
 * Return value:
 * the number of bytes written upon success
 * -EIO if the operation failed
 * -ENOSPC if the device is full
 */
ssize_t fs_ext2_inode_write(struct _inode_t* inode, ssize_t bytes,
        off_t offset, void* data) {
    return fs_ext2_inode_rw(inode, bytes, offset, data, EXT2_OP_WRITE);
}

/*
 * Truncate an inode, i.e. deallocate all blocks occupied by the inode on the disk
 * and set the size of the inode to zero
 * Parameter:
 * @inode - the inode to be truncated
 * Return value:
 * 0 upon successful completion
 * EIO if an I/O error occured
 */
int fs_ext2_inode_trunc(inode_t* inode, u32 new_size) {
    int new_blocks = 0;
    int old_blocks = 0;
    ext2_inode_t* ext2_inode = ((ext2_inode_data_t*) inode->data)->ext2_inode;
    ext2_metadata_t* ext2_meta = ((ext2_inode_data_t*) inode->data)->ext2_meta;
    EXT2_DEBUG("Truncating inode %d from size %d to target size %d\n", inode->inode_nr, inode->size, new_size);
    /*
     * We do not yet support enlarging a file via truncate
     */
    if (new_size > ext2_inode->i_size) {
        EXT2_DEBUG("Target size exceeding current size not yet supported\n");
        return EINVAL;
    }
    /*
     * Do nothing if the inode is not a regular file or a directory
     */
    if ((!S_ISREG(inode->mode)) && (!S_ISDIR(inode->mode)))
        return 0;
    /*
     * Figure out how many data blocks we need now and after the operation
     */
    new_blocks = (new_size / BLOCK_SIZE) + (new_size % BLOCK_SIZE ? 1 : 0);
    old_blocks = (ext2_inode->i_size / BLOCK_SIZE) + (ext2_inode->i_size % BLOCK_SIZE ? 1 : 0);
    /*
     * Deallocate all blocks occupied by the inode exceeding the target size. 
     * Note that this will also update the direct blocklist inside the inode.
     */
    if (new_blocks != old_blocks) {
        EXT2_DEBUG("Deallocating blocks starting at byte offset %d occupied by inode on disk\n", new_blocks*BLOCK_SIZE);
        if (fs_ext2_inode_rw(inode, (old_blocks - new_blocks) * BLOCK_SIZE, new_blocks*BLOCK_SIZE, 0, EXT2_OP_TRUNC) < 0) {
            return EIO;
        }
        /*
         * See whether we still have indirect blocks
         */
        if (new_blocks <= (EXT2_LAST_DIRECT+1)) {
            EXT2_DEBUG("No indirect blocks any more\n");
            ext2_inode->indirect1 = 0;
        }
        /*
         * Do we still have double indirect blocks?
         */
        if (new_blocks <= (EXT2_LAST_INDIRECT+1)) {
            EXT2_DEBUG("No double indirect blocks any more\n");
            ext2_inode->indirect2 = 0;
        }
        /*
         * and what about triple indirect blocks*
         */
        if (new_blocks <= (EXT2_LAST_DOUBLE_INDIRECT+1)) {
            EXT2_DEBUG("No triple indirect blocks any more\n");
            ext2_inode->indirect3 = 0;
        }
    }
    /*
     * Set size of inode to new value. Note that during deallocation,
     * the i_blocks value has already been updated
     */
    ext2_inode->i_size = new_size;
    inode->size = new_size;
    EXT2_DEBUG("Current value of i_blocks: %d\n", ext2_inode->i_blocks);
    /*
     * Write inode back to disk
     */
    if (put_inode(ext2_meta, inode)) {
        return EIO;
    }
    return 0;
}

/*
 * Wipe an inode, i.e. deallocate all blocks occupied by the inode on the disk
 * and mark the entry in the inode table as available
 * Parameter:
 * @inode - the inode to be wiped out
 */
static void wipe_inode(inode_t* inode) {
    /*
     * Deallocate all blocks occupied by the inode
     */
    EXT2_DEBUG("Deallocating all blocks occupied by inode on disk\n");
    if (fs_ext2_inode_trunc(inode, 0)) {
        ERROR("Could not truncate inode, proceeding anyway\n");
    }
    /*
     * Now mark entry in inode descriptor table as free
     */
    if (deallocate_inode(inode)) {
        ERROR("Could not mark inode as free\n");
    }
}


/****************************************************************************************
 * The following functions handle directory operations                                  *
 ****************************************************************************************/

/*
 * Get a directory entry from an inode
 * Parameter:
 * @inode - the inode from which we read the entry
 * @index - the index of the entry within the directory, starting with zero
 * Return value:
 * 0 upon success
 * EIO if the directory inode could not be read
 * -1 if the directory entry could not be found
 */
int fs_ext2_get_direntry(struct _inode_t* inode, off_t index,
        direntry_t* direntry) {
    ext2_direntry_t ext2_direntry;
    u32 offset = 0;
    u32 read = 0;
    u32 name_len;
    /*
     * If size of inode is zero, the directory has been removed, but is still
     * present on the disk as there are still pending references to it. Return -1
     */
    if (0 == inode->size) {
        return -1;
    }
    EXT2_DEBUG("Starting walk of directory\n");
    while (read <= index) {
        if (fs_ext2_inode_read(inode, sizeof(ext2_direntry_t), offset,
                (void*) &ext2_direntry) != sizeof(ext2_direntry)) {
            ERROR("Could not read directory entry from inode (%d, %d)\n", inode->dev, inode->inode_nr);
            return EIO;
        }
        /*
         * Ignore directory entries with inode number set to zero
         */
        if (ext2_direntry.inode) {
            /*
             * If this is the entry we are looking for, copy data from
             * it to direntry structure
             */
            if (read == index) {
                direntry->inode_nr = ext2_direntry.inode;
                name_len = ext2_direntry.name_len;
                if (name_len > FILE_NAME_MAX - 1)
                    name_len = FILE_NAME_MAX - 1;
                if (fs_ext2_inode_read(inode, name_len, offset
                        + sizeof(ext2_direntry_t), direntry->name) != name_len) {
                    ERROR("Could not read file name from directory inode\n");
                    return EIO;
                }
                (direntry->name)[name_len] = 0;
                EXT2_DEBUG("Found entry with inode nr %d\n", direntry->inode_nr);
                return 0;
            }
            /*
             * Verify that length is a multiple of 4
             */
            if (ext2_direntry.rec_len % sizeof(u32)) {
                PANIC("Length of directory entry %d in inode %d is %d - not a multiple of 4\n", read,
                        inode->inode_nr, ext2_direntry.rec_len);
                return EIO;
            }
            read++;
        }
        else {
            if (0 == ext2_direntry.rec_len) {
                PANIC("Got invalid directory inode entry with length and inode zero- can this be true?\n");
            }
        }
        /*
         * Advance to next entry
         */
        offset += ext2_direntry.rec_len;
        if (offset >= inode->size)
            break;
    }
    return -1;
}

/*
 * Perform some validations on a directory entry and a directory
 * Parameter:
 * @ext2_direntry - the entry to be validated
 * @dir - the directory to be validated
 * Return value:
 * 1 if a problem has been found
 * 0 if all validations are successful
 */
static int validate_direntry(ext2_direntry_t* direntry, inode_t* dir) {
    if (direntry->rec_len % 4) {
        ERROR("Record length of entry for inode %d in directory inode %d is not a multiple of four!\n",
                direntry->inode, dir->inode_nr);
        return 1;
    }
    if (dir->size % BLOCK_SIZE) {
        ERROR("Size of directory inode %d is not a multiple of the block size\n", dir->inode_nr);
        return 1;
    }
    return 0;
}

/*
 * This function will try to splice a new directory entry into an existing entry by making use
 * of free space between the end of the file name and the end of the entry.
 * Parameter:
 * @existing_entry - the existing entry
 * @new_entry - the new entry
 * @dir - the inode representing the directory
 * @offset - the offset in bytes of the existing entry within the directory inode
 * @name - the name of the inode described by the new entry
 * Return value:
 * 0 if the operation could not be completed because the existing entry is too small
 * -1 if an error occurred
 * 1 if the operation could be completed
 */
static int splice_direntry(ext2_direntry_t* existing_entry, ext2_direntry_t* new_entry, inode_t* dir, u32 offset, char* name) {
    u32 first_free_byte;
    void* buffer;
    if (validate_direntry(existing_entry, dir)) {
        ERROR("Entry at offset %d in directory inode %d is invalid!\n", offset, dir->inode_nr);
        return -1;
    }
     /*
     * First determine first byte within the inode
     * which is contained within the record but not part of the name and align
     * this to a dword boundary
     */
    first_free_byte = offset + (existing_entry->name_len + sizeof(ext2_direntry_t));
    if (first_free_byte % 4) {
        first_free_byte = (first_free_byte & ~0x3) + 4;
    }
    /*
     * Now check whether placing the new entry at this byte would fit into the currently reserved
     * area within the inode
     */
    if (first_free_byte + new_entry->rec_len - 1 < offset + existing_entry->rec_len) {
        /*
         * Get buffer space and copy directory entry and name into buffer
         */
        if (0 == (buffer = kmalloc(BLOCK_SIZE))) {
            ERROR("Could not allocate enough memory\n");
            return -1;
        }
        KASSERT(sizeof(ext2_direntry_t)+new_entry->name_len <= BLOCK_SIZE);
        /*
         * Set size of new entry so that it takes up the entire available space
         */
        new_entry->rec_len = offset + existing_entry->rec_len - first_free_byte;
        /*
         * Adjust record length of existing entry
         */
        existing_entry->rec_len = first_free_byte - offset;
        if (fs_ext2_inode_write(dir, sizeof(ext2_direntry_t), offset,
                (void*) existing_entry) != sizeof(ext2_direntry_t)) {
            ERROR("Could not write directory entry\n");
            kfree((void*) buffer);
            return -1;
        }
        /*
         * Write new entry
         */
        memcpy(buffer, (void*) new_entry, sizeof(ext2_direntry_t));
        memcpy(buffer + sizeof(ext2_direntry_t), (void*) name, new_entry->name_len);
        if (fs_ext2_inode_write(dir, new_entry->rec_len, first_free_byte,
                (void*) buffer) != new_entry->rec_len) {
            ERROR("Could not write directory entry\n");
            kfree((void*) buffer);
            return -1;
        }
        kfree((void*) buffer);
        return 1;
    }
    else
        return 0;
}


/*
 * Utility function to append a new directory entry to an existing directory. This should
 * only be done if merging the entry into an existing one is not possible
 * Parameter:
 * @dir - the directory into which we insert the new entry
 * @inode_nr - the inode number for the new entry
 * @name - the name of the new entry
 * Return value:
 * 0 if the operation was successful
 * EIO if an I/O error occured
 * ENOMEM if we are running out of memory
 */
static int append_direntry(inode_t* dir, u32 inode_nr, char* name) {
    ext2_direntry_t new_direntry;
    void* buffer;
    /*
     * Create directory entry structure
     */
    new_direntry.file_type = 0;
    new_direntry.inode = inode_nr;
    new_direntry.name_len = strlen(name);
    new_direntry.rec_len = BLOCK_SIZE;
    /*
     * Get buffer space and copy directory entry and name into buffer
     */
    if (0 == (buffer = kmalloc(BLOCK_SIZE))) {
        ERROR("Could not allocate enough memory\n");
        return ENOMEM;
    }
    /*
     * Fill buffer with zeroes
     */
    memset((void*) buffer, 0, BLOCK_SIZE);
    KASSERT(sizeof(ext2_direntry_t) + new_direntry.name_len <= BLOCK_SIZE);
    memcpy(buffer, (void*) &new_direntry, sizeof(ext2_direntry_t));
    memcpy(buffer + sizeof(ext2_direntry_t), (void*) name, new_direntry.name_len);
    if (fs_ext2_inode_write(dir, BLOCK_SIZE, dir->size, (void*) buffer) != BLOCK_SIZE) {
        kfree((void*) buffer);
        return EIO;
    }
    kfree((void*) buffer);
    return 0;
}

/*
 * Utility function to update a directory entry, i.e. adapt the inode number
 * of an existing directory entry
 * Parameter:
 * @dir - inode of the directory
 * @index - index of entry to be updated
 * @inode_nr - new inode number
 * Return values:
 * EIO if the directory could not be read or written
 * -1 if the entry with the specified index does not exist
 */
static int update_direntry(inode_t* dir, int index, u32 inode_nr) {
    ext2_direntry_t ext2_direntry;
    u32 offset = 0;
    u32 read = 0;
    /*
     * If size of inode is zero, the directory has been removed, but is still
     * present on the disk as there are still pending references to it. Return -1
     */
    if (0 == dir->size) {
        return -1;
    }
    EXT2_DEBUG("Starting walk of directory\n");
    while (read <= index) {
        if (fs_ext2_inode_read(dir, sizeof(ext2_direntry_t), offset,
                (void*) &ext2_direntry) != sizeof(ext2_direntry)) {
            ERROR("Could not read directory entry from inode (%d, %d)\n", dir->dev, dir->inode_nr);
            return EIO;
        }
        /*
         * Ignore directory entries with inode number set to zero
         */
        if (ext2_direntry.inode) {
            /*
             * If this is the entry we are looking for, update and write back to disk
             */
            if (read == index) {
                ext2_direntry.inode = inode_nr;
                if (fs_ext2_inode_write(dir, sizeof(ext2_direntry_t), offset,
                        (void*) &ext2_direntry) != sizeof(ext2_direntry)) {
                    ERROR("Could not write directory entry back to inode (%d, %d)\n", dir->dev, dir->inode_nr);
                    return EIO;
                }
                return 0;
            }
            read++;
        }
        /*
         * Advance to next entry
         */
        offset += ext2_direntry.rec_len;
        if (offset >= dir->size)
            break;
    }
    return -1;
}

/*
 * Utility function to create a directory entry for an inode. This function will first
 * scan the directory to find an existing entry which is large enough so that it can be shrinked
 * to get space for the new entry. If that fails, a new block is allocated and added to the directory inode
 * Parameter:
 * @dir - the inode representing the directory
 * @inode_nr - the number of the inode to be added
 * @name - the file name
 * Return value:
 * 0 if the operation was successful
 * EIO if an error occurred
 * EINVAL if the length of the name exceeds FILE_NAME_MAX
 * ENOMEM if we are running out of memory
 * Note that we could improve this algorithm to also scan for empty directory entries
 * which come into existence when we remove the first entry in a directory block
 */
static int create_direntry(inode_t* dir, u32 inode_nr, char* name) {
    u32 offset = 0;
    int rc;
    ext2_direntry_t ext2_direntry;
    ext2_direntry_t new_direntry;
    if (strlen(name) > FILE_NAME_MAX) {
        return EINVAL;
    }
    /*
     * Create directory entry structure
     */
    new_direntry.file_type = 0;
    new_direntry.inode = inode_nr;
    new_direntry.name_len = strlen(name);
    new_direntry.rec_len = sizeof(ext2_direntry_t) + strlen(name);
    /*
     * Align to a dword boundary
     */
    if (new_direntry.rec_len % 4) {
        new_direntry.rec_len &= ~0x3;
        new_direntry.rec_len += 4;
    }
    /*
     * Now walk through directory entries and try to locate
     * an entry which has enough space left to be split in two parts
     */
    while (1) {
        if (fs_ext2_inode_read(dir, sizeof(ext2_direntry_t), offset,
                (void*) &ext2_direntry) != sizeof(ext2_direntry_t)) {
            ERROR("Could not read directory entry\n");
            return EIO;
        }
        /*
         * Try to merge new entry into this entry
         */
        if (-1 == (rc = splice_direntry(&ext2_direntry, &new_direntry, dir, offset, name))) {
            ERROR("Splicing operation failed\n");
            return EIO;
        }
        if (1 == rc) {
            return 0;
        }
        /*
         * Advance to next entry
         */
        offset += ext2_direntry.rec_len;
        if (offset >= dir->size)
            break;
    }
    /*
     * If we get to this point, we have not been able to splice the entry
     * into an existing entry and write a new entry which takes up an entire block
     */
    return append_direntry(dir, inode_nr, name);
 }

/*
 * Utility function to remove a directory entry. We use the following algorithm to do this:
 * - if the directory entry is the first entry within a block, we set the inode number in the entry
 * to zero and mark it as unused
 * - if the entry is not the first within the block, we extend the preceding entry so that it ends
 * at the point where the entry to be removed ended previously
 * This function assumes that the entry to be removed (current_entry) and the entry preceding it (preceding_entry)
 * have already been read from disk
 * Parameters:
 * @current_entry - pointer to direntry structure describing the entry to be removed
 * @preceding_entry - pointer to direntry structure describing the entry which precedes the current entry (might be null)
 * @dir - inode representing the directory
 * @current_offset - the offset into the directory inode at which the current entry is located
 * @preceding_offset - the offset into the directory inode at which the preceding entry is located
 * Return value:
 * 0 if the operation was successful
 * EIO if an error occurred
 * EINVAL if an argument was not valid
 */
static int remove_direntry(ext2_direntry_t* current_entry, ext2_direntry_t* preceding_entry, inode_t* dir,
        u32 current_offset, u32 preceding_offset) {
    if (0 == current_entry) {
        ERROR("Current entry must not be zero\n");
        return EINVAL;
    }
    /*
     * Check whether we are at a block boundary as processing is different in this case
     */
    if (current_offset % BLOCK_SIZE) {
        /*
         * In this case we extend the previous entry
         * and write the previous entry back
         */
        if (0 == preceding_entry) {
            ERROR("We are in the middle of a block, but there is no preceding entry???\n");
            return EINVAL;
        }
        preceding_entry->rec_len += current_entry->rec_len;
        if (fs_ext2_inode_write(dir, sizeof(ext2_direntry_t), preceding_offset, (void*) preceding_entry)
                != sizeof(ext2_direntry_t)) {
            ERROR("Could not write adapted entry back to disk\n");
            return EIO;
        }
    }
    /*
     * This is the first entry in the block - set inode to zero and write current entry back
     */
    else {
        current_entry->inode = 0;
        if (fs_ext2_inode_write(dir, sizeof(ext2_direntry_t), current_offset, (void*) current_entry)
                != sizeof(ext2_direntry_t)) {
            ERROR("Could not write adapted entry back to disk\n");
            return EIO;
        }
    }
    return 0;
}

/*
 * Utility function to increase the link count of an inode on disk. Note that
 * the caller is in charge for checking against LINK_MAX
 * Parameter:
 * @inode - the inode
 */
static int inc_link_count(inode_t* inode) {
    ext2_inode_t* ext2_inode;
    ext2_metadata_t* meta;
    KASSERT(inode->data);
    ext2_inode = ((ext2_inode_data_t*) inode->data)->ext2_inode;
    KASSERT(ext2_inode);
    meta = ((ext2_inode_data_t*) inode->data)->ext2_meta;
    KASSERT(meta);
    if (LINK_MAX == ext2_inode->i_link_count) {
        PANIC("Could not increase link count, limit reached\n");
    }
    ext2_inode->i_link_count++;
    inode->link_count = ext2_inode->i_link_count;
    return put_inode(meta, inode);
}


/*
 * Utility function to reduce the link count of an inode on disk
 * Parameter:
 * @inode - the inode
 */
static int dec_link_count(inode_t* inode) {
    ext2_inode_t* ext2_inode;
    ext2_metadata_t* meta;
    KASSERT(inode->data);
    ext2_inode = ((ext2_inode_data_t*) inode->data)->ext2_inode;
    KASSERT(ext2_inode);
    meta = ((ext2_inode_data_t*) inode->data)->ext2_meta;
    KASSERT(meta);
    ext2_inode->i_link_count--;
    inode->link_count = ext2_inode->i_link_count;
    if (0 == ext2_inode->i_link_count) {
        ext2_inode->i_dtime = do_time(0);
        ext2_inode->i_mtime = ext2_inode->i_dtime;
    }
    return put_inode(meta, inode);
}

/*
 * Prepare a directory for deletion and validate references
 * 1) Fail if the directory is the root directory
 * 2) Fail if the directory is a mount point
 * 3) Fail if the directory link count is more than 2
 * 4) Set directory size to 0 to wipe contents and avoid further writes
 */
static int prep_dir_for_deletion(inode_t* dir, inode_t* parent, int flags) {
    direntry_t direntry;
    ext2_inode_t* ext2_inode = ((ext2_inode_data_t*) dir->data)->ext2_inode;
    /*
     * It is not allowed to remove the root directory
     */
    if (dir->super->root == dir->inode_nr) {
        EXT2_DEBUG("Cannot remove root directory\n");
        return EEXIST;
    }
    /*
     * It is not allowed to remove a mount point
     */
    if (dir->mount_point) {
        EXT2_DEBUG("Cannot remove mount point\n");
        return EBUSY;
    }
    EXT2_DEBUG("Link count of inode %d is %d\n", dir->inode_nr, ext2_inode->i_link_count);
    /*
     * Link count should be at most 2
     */
    if ((ext2_inode->i_link_count > 2) && (0 == (flags & FS_UNLINK_FORCE))) {
        EXT2_DEBUG("Unexpected additional hard links found\n");
        return EEXIST;
    }
    /*
     * Directory should be empty, i.e. a directory scan with index 2 should not
     * return anything - this can be overridden with force flag
     */
    if ((0 == fs_ext2_get_direntry(dir, 2, &direntry)) && (0 == (flags & FS_UNLINK_FORCE))) {
        EXT2_DEBUG("Directory not empty\n");
        return EEXIST;
    }
    /*
     * Truncate directory and adapt link counts to reflect removal of dot
     */
    if (0 == (flags & FS_UNLINK_NOTRUNC)) {
        fs_ext2_inode_trunc(dir, 0);
        ext2_inode->i_link_count = 1;
        dir->link_count = 1;
    }
    /*
     * Even if we have not removed dot-dot, reduce link count for parent
     */
    return dec_link_count(parent);
}

/*
 * Unlink an inode, i.e. remove an existing directory entry.
 * We first walk the directory and locate the entry within the
 * linked list of directory entries. Then we adjust the record length
 * of the previous entry such that it points to the end of the entry to be
 * removed and write the previous entry back to disk.
 * If the entry to be removed is the first entry within a block, this will not
 * work as an entry may not span a block boundary. We therefore simply zero out
 * the entry in this case.
 * In any case, we reduce the reference count on disk of the respective inode by one
 * Thus the callee should own locks on the directory as well as on the inode
 * Parameter:
 * @dir - the directory in which the inode is located
 * @name - the name of the inode
 * @flags - flags which control the operation, see below
 * Return value:
 * 0 if the operation is successful
 * EIO if an I/O error occurred
 * ENOENT if the entry could not be found
 * EEXIST if the inode is the root directory
 * EEXIST if the inode is a directory which has additional hard links
 * EEXIST if the inode is a non-empty directory
 * EBUSY if the inode is a mount point
 * Supported flags:
 * FS_UNLINK_NOTRUNC - if name is a directory, do not truncate it (but still reduce link count of parent)
 * FS_UNLINK_FORCE - perform unlink even is name is a directory with additional hard links
 */
int fs_ext2_unlink_inode(inode_t* dir, char* name, int flags) {
    ext2_direntry_t current_entry;
    ext2_direntry_t preceding_entry;
    u32 preceding_offset = 0;
    char current_name[FILE_NAME_MAX+1];
    u32 offset = 0;
    int found = 0;
    int rc;
    inode_t* removed_inode = 0;
    /*
     * We start to walk through the directory until we find the
     * entry we are looking for
     */
    while (1) {
        if (fs_ext2_inode_read(dir, sizeof(ext2_direntry_t), offset,
                (void*) &current_entry) != sizeof(ext2_direntry_t)) {
            ERROR("Could not read directory entry at offset %d\n", offset);
            return EIO;
        }
        /*
         * If this is a valid entry, get the name from disk
         */
        if (current_entry.name_len && current_entry.inode) {
            if (fs_ext2_inode_read(dir, current_entry.name_len, offset+sizeof(ext2_direntry_t),(void*) current_name)
                    != current_entry.name_len) {
                ERROR("Could not read directory entry\n");
                return EIO;
            }
            /*
             * Is this the inode we wish to remove?
             */
            if (0 == strncmp(name, current_name, strlen(name))) {
                found = 1;
                /*
                 * Get reference to inode - we will need this later on when we
                 * adapt the reference count of the inode on disk
                 */
                removed_inode = fs_ext2_get_inode(dir->dev, current_entry.inode);
                EXT2_DEBUG("Found inode %s to be removed, inode_nr is %d in directory %d\n", name,
                        removed_inode->inode_nr, dir->inode_nr);
                /*
                 * If this is a directory, prepare it for deletion, i.e. remove dot and dot-dot, and
                 * validate that there are no hard links other than the entry in dir and the dot entry
                 * pointing to the directory
                 */
                if (S_ISDIR(removed_inode->mode)) {
                    if ((rc = prep_dir_for_deletion(removed_inode, dir, flags))) {
                        /*
                         * Validation failed
                         */
                        EXT2_DEBUG("Validation failed with rc %d\n", rc);
                        removed_inode->iops->inode_release(removed_inode);
                        return rc;
                    }
                }
                /*
                 * Invoke utility function to remove directory entry
                 */
                if (remove_direntry(&current_entry, &preceding_entry, dir, offset, preceding_offset)) {
                    ERROR("Could not remove directory entry\n");
                    if (removed_inode)
                        removed_inode->iops->inode_release(removed_inode);
                    return EIO;
                }
                break;
            }
        }
        /*
         * Advance to next entry
         */
        preceding_offset = offset;
        memcpy(&preceding_entry, &current_entry, sizeof(ext2_direntry_t));
        offset += current_entry.rec_len;
        if (offset >= dir->size)
            break;
    }
    if (0 == found)
        return ENOENT;
    /*
     * We have just removed an entry for the inode from a directory. We will therefore need
     * to reduce the link count within the ext2 inode by one. Note that reducing the link
     * count does not remove the file, this will only be done once the last reference to
     * the inode is dropped
     */
    if (0 == removed_inode) {
        PANIC("Could not get pointer to removed inode\n");
        return EIO;
    }
    if (dec_link_count(removed_inode)) {
        ERROR("Could not decrement link count\n");
        fs_ext2_inode_release(removed_inode);
        return EIO;
    }
    fs_ext2_inode_release(removed_inode);
    return 0;
}


/****************************************************************************************
 * The following functions implement the public interface of the ext2 file system which *
 * is invoked by the virtual file system to create or destroy inodes                    *
 ***************************************************************************************/

/*
 * Utility function to initialize an ext2 inode data structure. This function will allocate
 * the necessary memory, it is the responsibility of the caller to free it again
 * Parameter:
 * @mode - file mode (only bits 07777 are used)
 * Return value:
 * the initialized inode
 */
static ext2_inode_t* init_ext2_inode(int mode) {
    ext2_inode_t* ext2_inode = 0;
    if (0 == (ext2_inode = (ext2_inode_t*) kmalloc(sizeof(ext2_inode_t)))) {
        ERROR("Running out of memory\n");
        return 0;
    }
    memset((void*) ext2_inode, 0, sizeof(ext2_inode_t));
    ext2_inode->i_mode = (mode & 07777) + EXT2_S_IFREG;
    ext2_inode->i_link_count = 0;
    ext2_inode->i_ctime = do_time(0);
    ext2_inode->i_atime = ext2_inode->i_ctime;
    ext2_inode->i_mtime = ext2_inode->i_ctime;
    ext2_inode->i_gid = do_getegid();
    ext2_inode->i_uid = do_geteuid();
    return ext2_inode;
}

/*
 * Link an inode into an existing directory, i.e. add a directory entry for an
 * existing inode to a directory and increase link count of inode. The changed
 * inode will be written back to disk
 * Parameter:
 * @dir - the directory to which we add an entry
 * @name - the name of the entry
 * @inode - the inode to be added
 * Note that in case the inode is a directory, the link count of the directory
 * is also increased to account for ..
 * Return value:
 * 0 upon success
 * a positive return code if an error occurs
 */
int fs_ext2_inode_link(inode_t* dir, char* name, struct _inode_t* inode) {
    int index;
    int rc;
    int found;
    direntry_t direntry;
    ext2_inode_data_t* ext2_inode_data = (ext2_inode_data_t*) inode->data;
    /*
     * Check that maximum link count is not exceeded
     */
    if (LINK_MAX == ext2_inode_data->ext2_inode->i_link_count ) {
        return EMLINK;
    }
    /*
     * If the inode is a directory, increase link count of directory
     * as well to account for ..
     */
    if (S_ISDIR(inode->mode)) {
        if (LINK_MAX == ((ext2_inode_data_t*) dir->data)->ext2_inode->i_link_count) {
            return EMLINK;
        }
        if (inc_link_count(dir)) {
            return EIO;
        }
    }
    /*
     * Create actual directory entry
     */
    if ((rc = create_direntry(dir, inode->inode_nr, name))) {
        ERROR("Could not create directory entry for new inode\n");
        dec_link_count(dir);
        return rc;
    }
    /*
     * If this is a directory, also fix the .. entry in the inode
     */
    if (S_ISDIR(inode->mode)) {
        /*
         * .. should be index 0 or index 1
         */
        found = 0;
        for (index = 0; index < 2; index++) {
            memset((void*) &direntry, 0, sizeof(direntry_t));
            rc = fs_ext2_get_direntry(inode, index, &direntry);
            if (-1 == rc)
                break;
            if (rc)
                return rc;
            if (0 == strcmp("..", direntry.name)) {
                found = 1;
                rc = update_direntry(inode, index, dir->inode_nr);
                if (-1 == rc)
                    return EIO;
                if (rc)
                    return rc;
                break;
            }
        }
        if (0 == found) {
            PANIC("Did not find .. entry in directory (%d, %d)\n", inode->dev, inode->inode_nr);
        }
    }
    /*
     * Update link count and write changed inode to disk
     */
    inc_link_count(inode);
    EXT2_DEBUG("Writing inode to disk\n");
    if ((rc = put_inode(ext2_inode_data->ext2_meta, inode))) {
        ERROR("Could not write inode to disk\n");
        return rc;
    }
    return 0;
}

/*
 * Create a new inode and add a directory entry for it. If the inode is
 * a directory (i.e. if S_IFDIR(mode) evaluates to true), entries for "."
 * and ".." are added to it
 * Parameters:
 * @dir - the directory in which the new file is to be created
 * @name - the name of the new inode
 * @mode - mode of new inode
 * Return value: the new inode or 0 if no inode could be created
 * Reference counts:
 * - the returned inode will have reference count one
 */
inode_t* fs_ext2_create_inode(inode_t* dir, char* name, int mode) {
    ext2_inode_t* ext2_inode;
    u32 inode_nr;
    ext2_inode_data_t* ext2_inode_data = (ext2_inode_data_t*) dir->data;
    inode_t* inode;
    int errno;
    ext2_metadata_t* ext2_metadata = 0;
    /*
     * If the length of the directory is zero, this implies that the directory
     * has already been marked for deletion. In this case, do not add any further entries
     * to it
     */
    if (0 == dir->size) {
        return 0;
    }
    /*
     * Allocate memory and initialize inode data structure
     */
    if (0 == (ext2_inode = init_ext2_inode(mode))) {
        ERROR("Could not allocate inode - out of memory\n");
        return 0;
    }
    /*
     * Use clone to obtain reference to metadata as we are going to
     * add this pointer to the newly created inode permanently
     */
    ext2_metadata = clone_meta(ext2_inode_data->ext2_meta);
    /*
     * Allocate an inode
     */
    if (0 == (inode_nr = allocate_inode(ext2_metadata, (S_ISDIR(mode) ? 1: 0), &errno))) {
        if (errno) {
            ERROR("Error while trying to allocate inode\n");
        }
        else
            EXT2_DEBUG("Device full\n");
        kfree((void*) ext2_inode);
        fs_ext2_release_superblock(ext2_metadata->super);
        return 0;
    }
    /*
     * If inode is a directory, update mode accordingly
     */
    if (S_ISDIR(mode)) {
        ext2_inode->i_mode = (ext2_inode->i_mode & 07777) + S_IFDIR;
    }
    /*
    * Initialize inode data structure
    */
    if (0 == (inode = init_inode(ext2_inode, ext2_metadata, inode_nr))) {
        ERROR("Could not initialize inode data structure\n");
        kfree((void*) ext2_inode);
        /*
         * Call fs_ext2_release_superblock to drop reference
         * to ext2 metadata structure again properly
         */
        fs_ext2_release_superblock(ext2_metadata->super);
        return 0;
    }
    /*
     * Build up inode metadata and add it to list
     */
    if (0 == (ext2_inode_data = init_ext2_inode_data(inode, ext2_inode, ext2_metadata))) {
        fs_ext2_release_superblock(ext2_metadata->super);
        kfree((void*) ext2_inode);
        kfree((void*) inode);
        PANIC("Could not allocate memory\n");
        return 0;
    }
    inode->data = (void*) ext2_inode_data;
    store_inode(ext2_metadata, ext2_inode_data);
    /*
     * If the new inode is itself a directory, we need to add two directory
     * entries to it - one for "." and one for "..". We also need to increase
     * the link counts accordingly for the inode itself and the parent directory
     */
    if (S_ISDIR(mode)) {
        /*
         * We are going to increase the link count of the parent - make sure that is
         * does not overflow
         */
        if (((ext2_inode_data_t*) dir->data)->ext2_inode->i_link_count == LINK_MAX) {
            fs_ext2_inode_release(inode);
            return 0;
        }
        /*
         * Add entry ".". We need to do this using append_direntry as create_direntry assumes an existing
         * directory structure.
         */
        if (append_direntry(inode, inode->inode_nr, ".")) {
            ERROR("Could not create directory entry . for new inode\n");
            fs_ext2_inode_release(inode);
            return 0;
        }
        /*
         * Increase link count
         */
        if (inc_link_count(inode)) {
            ERROR("Could not increment link count\n");
            fs_ext2_inode_release(inode);
            return 0;
        }
        /*
         * next add entry ".." and increase link count for parent
         */
        if (create_direntry(inode, dir->inode_nr, "..")) {
            ERROR("Could not create directory entry for new inode\n");
            dec_link_count(dir);
            fs_ext2_inode_release(inode);
            return 0;
        }
    }
    /*
     * Add directory entry for our new inode. If the new inode is a directory,
     * this will also fix the .. entry.
     */
    if (fs_ext2_inode_link(dir, name, inode)) {
        ERROR("Could not link new inode into directory\n");
        fs_ext2_inode_release(inode);
        return 0;
    }
    return inode;
}


/*
 * Release an inode, i.e. decrement its reference count
 * and delete the inode if the reference count reaches zero
 * Parameters:
 * @inode - the inode which is to be released
 * Locks:
 * lock on ext2 metadata structure for this inode
 * Reference counts:
 * - decrease reference count of inode by one
 * - decrease reference count of associated superblock by one
 */
void fs_ext2_inode_release(inode_t* inode) {
    u32 eflags;
    KASSERT(inode);
    ext2_inode_data_t* idata = (ext2_inode_data_t*) inode->data;
    KASSERT(idata);
    ext2_metadata_t* meta = idata->ext2_meta;
    EXT2_DEBUG("Releasing inode_nr %d on device %x\n", inode->inode_nr, inode->dev);
    spinlock_get(&meta->lock, &eflags);
    idata->reference_count--;
    if (0 == idata->reference_count) {
        EXT2_DEBUG("Reference count of inode dropped to zero\n");
        LIST_REMOVE(meta->inodes_head, meta->inodes_tail, idata);
    }
    spinlock_release(&meta->lock, &eflags);
    /*
     * If the reference count is now zero, deallocate inode. If in
     * addition the link count on disk is zero for the inode,
     * remove the inode on the disk as well.
     */
    if (0 == idata->reference_count) {
        if (0 == idata->ext2_inode->i_link_count)
            wipe_inode(inode);
        destroy_ext2_inode_data(idata);
        EXT2_DEBUG("Freeing idata (%x)\n", idata);
        kfree(idata);
    }
    /*
     * We still need the metadata up to this point, but
     * in this was the last inode we can get rid of it now
     */
    fs_ext2_release_superblock(meta->super);
    EXT2_DEBUG("Done\n");
}


/***************************************************************
 * Everything below this line is for debugging only            *
 **************************************************************/

/*
 * Utility functions to print some information on inode cache and superblock
 * cache on the screen. Returns the sum of reference counts of all cached inodes
 * and caches superblocks
 */
int fs_ext2_print_cache_info() {
    int rc = 0;
    ext2_metadata_t* meta;
    ext2_inode_data_t* idata;
    PRINT("Ext2 inode and superblock cache info\n");
    PRINT("------------------------------------\n");
    LIST_FOREACH(ext2_metadata_head, meta) {
        rc += meta->reference_count;
        PRINT("Superblock entry: \n");
        PRINT("------------------\n");
        PRINT("Device:         (%d, %d)\n", MAJOR(meta->device), MINOR(meta->device));
        PRINT("Ref. count:     %d\n", meta->reference_count);
        PRINT("Cached inodes:\n");
        PRINT("--------------\n");
        LIST_FOREACH(meta->inodes_head, idata) {
            rc += idata->reference_count;
            PRINT("    Inode:       %d\n", idata->inode->inode_nr);
            PRINT("    Mount point: %d\n", idata->inode->mount_point);
            PRINT("    Ref. count:  %d\n", idata->reference_count);
        }
    }
    return rc;
}
