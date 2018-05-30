/*
 * fs_ext2.h
 */

#ifndef _FS_EXT2_H_
#define _FS_EXT2_H_

#include "lib/sys/types.h"
#include "fs.h"
#include "lib/utime.h"



typedef struct {
    u32 s_inodes_count;
    u32 s_blocks_count;
    u32 s_r_blocks_count;
    u32 s_free_blocks_count;
    u32 s_free_inode_count;
    u32 s_first_data_block;
    u32 s_log_block_size;
    u32 s_log_frag_size;
    u32 s_blocks_per_group;
    u32 s_frags_per_group;
    u32 s_inodes_per_group;
    u32 s_mtime;
    u32 s_wtime;
    u16 s_mnt_count;
    u16 s_max_mnt_count;
    u16 s_magic;
    u16 s_state;
    u16 s_errors;
    u16 s_minor_rev_level;
    u32 s_last_check;
    u32 s_check_interval;
    u32 s_creator_os;
    u32 s_rev_level;
    u16 s_def_res_uid;
    u16 s_def_res_gid;
    u32 s_first_ino;
    u16 s_inode_size;
    u16 s_block_group_nr;
    u32 s_feature_compat;
    u32 s_feature_incompat;
    u32 s_feature_ro_compat;
    u32 s_uuid[4];
} __attribute__ ((packed)) ext2_superblock_t;

/*
 * This structure is an entry in the ext2
 * block group descriptor table
 */
typedef struct {
    u32 bg_block_bitmap;
    u32 bg_inode_bitmap;
    u32 bg_inode_table;
    u16 bg_free_blocks_count;
    u16 bg_free_inodes_count;
    u16 bg_used_dirs_count;
    u16 bg_pad;
    u8 reserved[12];
} __attribute__ ((packed)) ext2_bgd_t;

/*
 * An ext2 inode
 */
typedef struct {
    u16 i_mode;
    u16 i_uid;
    u32 i_size;
    u32 i_atime;
    u32 i_ctime;
    u32 i_mtime;
    u32 i_dtime;
    u16 i_gid;
    u16 i_link_count;
    u32 i_blocks;
    u32 i_flags;
    u32 i_osd1;
    u32 direct[12];
    u32 indirect1;
    u32 indirect2;
    u32 indirect3;
    u32 i_generation;
    u32 i_file_acl;
    u32 i_dir_acl;
    u32 i_faddr;
    u32 i_osd2[3];
} __attribute__ ((packed)) ext2_inode_t;

struct _ext2_inode_data_t;

/*
 * A directory entry. This structure only covers the fixed part of the
 * directory entry. The next byte is the first byte of the name
 */
typedef struct {
    u32 inode;
    u16 rec_len;
    u8 name_len;
    u8 file_type;
} __attribute__ ((packed)) ext2_direntry_t;

/*
 * This structure is used to keep track of all metadata which
 * belong to one mounted ext2 instance
 */
typedef struct _ext2_metadata_t {
    ext2_superblock_t* ext2_super;               // the ext2 superblock for this instance
    superblock_t* super;                         // the generic file system layer superblock
    ext2_bgd_t* bgdt;                            // the block group descriptor table
    dev_t device;                                // device on which the file system lives
    u32 bgdt_size;                               // number of entries in the block group descriptor table
    u32 bgdt_blocks;                             // number of blocks occupied by the block group descriptor table
    semaphore_t sb_lock;                         // lock to protect the superblock structure
    struct _ext2_metadata_t* next;
    struct _ext2_metadata_t* prev;
    struct _ext2_inode_data_t* inodes_head;
    struct _ext2_inode_data_t* inodes_tail;
    int reference_count;                         // number of times this structure is referenced from outside the module
    spinlock_t lock;                             // this lock protects the chain of inodes attached to this structure and the reference count of each inode
} ext2_metadata_t;

/*
 * Structure to tie together an ext2 inode and the
 * corresponding generic inode
 */
typedef struct _ext2_inode_data_t {
    ext2_metadata_t* ext2_meta;                  // Pointer to corresponding ext2 metadata
    ext2_inode_t* ext2_inode;                    // ext2 inode
    inode_t* inode;                              // inode as visible to the generic FS layer
    int reference_count;                         // Number of references to this inode
    struct _ext2_inode_data_t* next;
    struct _ext2_inode_data_t* prev;
} ext2_inode_data_t;

#define EXT2_SUPERBLOCK_SIZE 1024
#define EXT2_BLOCK_SIZE 1024
#define EXT2_MAGIC_NUMBER 0xef53
#define EXT2_ROOT_INODE 2
/*
 * How many blocks can we address with one indirect block
 */
#define EXT2_INDIRECT_BLOCKS (BLOCK_SIZE/sizeof(u32))

/*
 * How many blocks can we address with one double indirect block
 */
#define EXT2_DOUBLE_INDIRECT_BLOCKS (EXT2_INDIRECT_BLOCKS*EXT2_INDIRECT_BLOCKS)
/*
 * Macros to define the last block addressed by direct pointers,
 * single indirect pointers, double indirect pointers and
 * triple indirect pointers
 */
#define EXT2_LAST_DIRECT 11
#define EXT2_LAST_INDIRECT   (EXT2_LAST_DIRECT+EXT2_INDIRECT_BLOCKS)
#define EXT2_LAST_DOUBLE_INDIRECT (EXT2_LAST_INDIRECT+EXT2_INDIRECT_BLOCKS*EXT2_INDIRECT_BLOCKS)

/*
 * Ext2 file modes and access rights
 */
#define EXT2_S_IFREG 0100000


/*
 * Some operations
 */
#define EXT2_OP_READ 0
#define EXT2_OP_WRITE 1
#define EXT2_OP_TRUNC 2

int fs_ext2_probe(dev_t device);
superblock_t* fs_ext2_get_superblock(dev_t device);
int fs_ext2_is_busy(superblock_t* super);
int fs_ext2_init();
inode_t* fs_ext2_get_inode(dev_t device, ino_t inode_nr);
void fs_ext2_release_superblock(superblock_t* superblock);
ssize_t fs_ext2_inode_read(struct _inode_t* inode, ssize_t bytes, off_t offset,
        void* data);
ssize_t fs_ext2_inode_write(struct _inode_t* inode, ssize_t bytes, off_t offset,
        void* data);
int fs_ext2_inode_trunc(struct _inode_t* inode, u32 new_size);
int fs_ext2_get_direntry(struct _inode_t* inode, off_t index,
        direntry_t* direntry);
inode_t* fs_ext2_create_inode(inode_t* dir, char* name, int mode);
int fs_ext2_unlink_inode(inode_t* dir, char* name, int flags);
inode_t* fs_ext2_inode_clone(struct _inode_t* inode);
void fs_ext2_inode_release(struct _inode_t* inode);
int fs_ext2_inode_flush(struct _inode_t* inode);
int fs_ext2_inode_link(inode_t* dir, char* name, struct _inode_t* inode);
int fs_ext2_print_cache_info();

#endif /* _FS_EXT2_H_ */
