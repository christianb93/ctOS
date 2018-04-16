/*
 * ext2dump.c
 *
 * This utility uses the Ext2 filesystem driver code in kernel/fs_ext2.c to dump some Ext2 filesystem structures. See print_usage
 * for a description of the available functions.
 *
 * Note that this tool assumes that the first parameter is a raw image of an ext2 file system, i.e. without the MBR.
 */

#include "fs_ext2.h"
#include "drivers.h"
#include "blockcache.h"
#include "vga.h"
#include <stdio.h>

/*
 * Some forward declarations to make gcc happy
*/

int strncmp(const char *s1, const char *s2, size_t n);
long int strtol(const char *nptr, char **endptr, int base);
void *malloc(int size);
void free(void *ptr);
time_t time(time_t* ptr);

/*
 * Number of bytes in a line in the hex dump
 */
#define LINE_LENGTH 32

/*
 * Some library functions which we use
 */
extern int open(const char *pathname, int flags, ...);
extern ssize_t read(int fd, void *buf, size_t count);
extern off_t lseek(int fd, off_t offset, int whence);

/*
 * Function pointers in blockcache.c which we stub
 */
extern ssize_t (*bc_read)(dev_t dev, ssize_t blocks, ssize_t first_block,
        void* buffer);
extern ssize_t (*bc_write)(dev_t dev, ssize_t blocks, ssize_t first_block,
        void* buffer);

/*
 * This file descriptor points to the image which we use
 */
static int fd = -1;

/*
 * Stub for do_time in rtc.c
 */
time_t do_time(time_t* ptr) {
    return time(ptr);
}

uid_t do_geteuid() {
    return 0;
}

gid_t do_getegid() {
    return 0;
}


/*
 * Implementation of kputchar
 */
void win_putchar(win_t* win, u8 c) {
    printf("%c", c);
}

blk_dev_ops_t ops;

blk_dev_ops_t* dm_get_blk_dev_ops(major_dev_t major) {
    return &ops;
}

void trap() {

}

void spinlock_get(spinlock_t* spinlock, u32* eflags) {

}

void spinlock_release(spinlock_t* spinlock, u32* eflags) {

}

void spinlock_init(spinlock_t* spinlock) {

}

void sem_init(semaphore_t* sem, u32 value) {

}

void mutex_up(semaphore_t* mutex)  {
}

void __sem_down(semaphore_t* mutex, char* file, int line)  {
}


void rw_lock_init(rw_lock_t* rw_lock) {

}

/*
 * Replacement for kmalloc/kfree
 */
void* kmalloc(u32 size) {
    return (void*) malloc(size);
}
void kfree(void* addr) {
    free(addr);
}

/*
 * Implementations of bc_write and bc_read which access our test image
 */
ssize_t bc_write_stub(dev_t dev, ssize_t blocks, ssize_t first_block,
        void* buffer) {
    return -1;
}

ssize_t bc_read_stub(dev_t dev, ssize_t blocks, ssize_t first_block,
        void* buffer) {
    int rc;
    if ((MAJOR(dev) == MAJOR_RAMDISK) && (MINOR(dev) == 0)) {
        rc = lseek(fd, first_block * 1024, 0);
        if (rc < 0) {
            printf("lseek returned with error code -%d\n", (-1) * rc);
            return -1;
        }
        rc = read(fd, buffer, blocks * 1024);
        if (rc < 0) {
            printf("lseek returned with error code -%d\n", (-1) * rc);
            return -1;
        }
        return rc;
    }
    return -1;
}

int bc_oc_stub(minor_dev_t device) {
    return 0;
}

/*
 * Print information on an inode
 * Parameters:
 * @inode - the number of the inode as a string
 */
static void print_inode(char* inode_str) {
    u32 inode_nr;
    int i;
    char* end_ptr;
    ext2_inode_t* ext2_inode;
    inode_t* inode;
    ext2_inode_data_t* ext2_inode_data;
    inode_nr = strtol(inode_str, &end_ptr, 10);
    if (inode_nr) {
        inode = fs_ext2_get_inode(DEVICE(MAJOR_RAMDISK, 0), inode_nr);
        if (0 == inode) {
            printf(
                    "Request for inode %d did not return a valid inode structure\n",
                    inode_nr);
            return;
        }
        ext2_inode_data = (ext2_inode_data_t*) inode->data;
        ext2_inode = ext2_inode_data->ext2_inode;
        printf("Size of inode (i_size):     %d\n", ext2_inode->i_size);
        printf("Blocks (i_blocks):          %d (--> %d kB)\n",
                ext2_inode->i_blocks, ext2_inode->i_blocks / 2);
        for (i = 0; i < 12; i++) {
            printf("Direct block %.2d:            %d\n", i,
                    ext2_inode->direct[i]);
        }
        printf("Indirect block:             %d\n", ext2_inode->indirect1);
        printf("Double indirect block:      %d\n", ext2_inode->indirect2);
        printf("Triple indirect block:      %d\n", ext2_inode->indirect3);
    }
    else {
        printf("%s is not a valid inode number\n", inode_str);
    }
}

/*
 * Print an ext2 superblock
 * Parameters:
 * @super - the superblock
 */
static void print_super(ext2_superblock_t* super) {
    printf("Blocks:          %d\n", super->s_blocks_count);
    printf("Inodes:          %d\n", super->s_inodes_count);
    printf("First inode:     %d\n", super->s_first_ino);
}

/*
 * Dump a file system block
 * Parameter:
 * @block_str - block number to be dumped as string
 */
static void print_block(char* block_str) {
    u32 block;
    int rc;
    int i;
    int j;
    char c;
    u8 buffer[1024];
    char* end_ptr;
    block = strtol(block_str, &end_ptr, 10);
    if (0==block) {
        printf("%s is not a valid block number\n");
        return;
    }
    rc = bc_read_bytes(block, 1024, buffer, DEVICE(MAJOR_RAMDISK, 0), 0);
    if (rc < 0) {
        printf("Could not read from device, rc=-%d\n", (-1)*rc);
        return;
    }
    printf("Block %d\n", block);
    for (i=0;i<1024/LINE_LENGTH;i++) {
        printf("%4x:   ", i*LINE_LENGTH);
        for (j=0;j<LINE_LENGTH;j++)
            printf("%.2x ", buffer[i*LINE_LENGTH+j]);
        printf("       ");
        for (j=0;j<LINE_LENGTH;j++) {
            c = buffer[i*LINE_LENGTH+j];
            if ( (c>='!') && (c<='~'))
                printf("%c", c);
            else
                printf(".");
        }
        printf("\n");
    }
}

/*
 * Print usage information and exit
 */
static void print_usage() {
    printf("Usage: ext2dump image command\n");
    printf(
            "where image is the name of the ext2 file system image to use and command is one of the following: \n");
    printf("super - print superblock of file system\n");
    printf("inode <n> - print information on inode <n>\n");
    printf("block <n> - dump block <n>\n");
    _exit(1);
}

void main(int argc, char** argv) {
    char* cmd;
    superblock_t* super;
    ext2_superblock_t* ext2_super;
    ext2_metadata_t* ext2_meta;
    /*
     * Print usage information if no image is supplied
     */
    if (argc < 3) {
        print_usage();
    }
    ops.close = bc_oc_stub;
    ops.open = bc_oc_stub;
    bc_read = bc_read_stub;
    bc_write = bc_write_stub;
    fd = open(argv[1], 2);
    if (fd < 0) {
        printf("Could not open image file %s\n", argv[1]);
        _exit(1);
    }
    cmd = argv[2];
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ext2_meta = ((ext2_metadata_t*) (super->data));
    ext2_super = ext2_meta->ext2_super;
    if (0 == strncmp("super", cmd, 4)) {
        print_super(ext2_super);
    }
    else if (0 == strncmp("inode", cmd, 5)) {
        if (argc == 3) {
            print_usage();
        }
        else
            print_inode(argv[3]);
    }
    else if (0 == strncmp("block", cmd, 4)) {
        if (argc == 3) {
            print_usage();
        }
        else
            print_block(argv[3]);
    }
    close(fd);
}
