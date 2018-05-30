/*
 * test_fs_ext2.c
 *
 * This unit test module tests the ext2 file system code in fs_ext2.c. To be executed, it requires a test image called rdimage
 * which is located in the current working directory. This image is supposed to contain an ext2 file system (without a device MBR)
 * with the following properties:
 *
 * 1) it contains a file with inode TEST_INODE below which is an exact copy of a file on the file system. The name of this file
 * as it is stored on the local (host) file system is contained in the constant TEST_COPY
 * 2) this file is approximately 100 MB in size, so that it contains triple indirect blocks
 * 3) the image also contains test files as produced by the tool ../tools/ext2samples.c - see the comments on the precompiler
 * constants SAMPLE_<X>_INODE below
 */

#include "kunit.h"
#include "vga.h"
#include "fs_ext2.h"
#include "drivers.h"
#include "mm.h"
#include "dm.h"
#include "drivers.h"
#include "blockcache.h"
#include "locks.h"
#include "sys/stat.h"
#include <stdio.h>

extern int __ext2_loglevel;

extern ssize_t (*bc_read)(dev_t dev, ssize_t blocks, ssize_t first_block,
        void* buffer);
extern ssize_t (*bc_write)(dev_t dev, ssize_t blocks, ssize_t first_block,
        void* buffer);

#define O_RDONLY         00
#define O_RDWR      00000002
#define O_CREAT     00000100

extern int open(const char *__file, int __oflag, ...);
extern ssize_t read(int fd, void *buf, size_t count);

blk_dev_ops_t ops;


/*
 * Size of the test hd image
 */
#define TEST_IMAGE_SIZE 256000000
/*
 * Inode of the file /test - get this with ls -li
 */
#define TEST_INODE 12

/*
 * Inode number of file with hole in second direct block
 */
#define SAMPLE_A_INODE 13

/*
 * Inode number of file with hole in the indirect block, i.e. indirect
 * block is zero, double indirect block is different from zero and first
 * direct block is different from zero
 */
#define SAMPLE_B_INODE 14

/*
 * Inode number of file with hole in the double indirect block, i.e. double indirect
 * block is zero, triple indirect block is different from zero and first
 * direct block is different from zero
 */
#define SAMPLE_C_INODE 15

/*
 * Inode number of a short file with length 10 bytes
 */
#define SAMPLE_D_INODE 16

/*
 * Copy of the file /test on the local file system
 */
#define TEST_COPY "./testfile"
/*
 * Size of the test file /test in bytes
 */
#define TEST_FILE_SIZE 133644288

void* image;

void win_putchar(win_t* win, u8 c) {
    printf("%c", c);
}

void trap() {

}

void sem_init(semaphore_t* sem, u32 value) {

}
void sem_up(semaphore_t* sem) {

}
void __sem_down(semaphore_t* sem, char* file, int line) {

}

void spinlock_get(spinlock_t* spinlock, u32* eflags) {

}

void spinlock_release(spinlock_t* spinlock, u32* eflags) {

}

void spinlock_init(spinlock_t* spinlock) {

}

void rw_lock_init(rw_lock_t* rw_lock) {

}

void* kmalloc_aligned(u32 size, u32 alignment) {
    return 0;
}

uid_t do_geteuid() {
    return 0;
}

gid_t do_getegid() {
    return 0;
}

blk_dev_ops_t* dm_get_blk_dev_ops(major_dev_t major) {
    return &ops;
}

ssize_t bc_write_stub(dev_t dev, ssize_t blocks, ssize_t first_block,
        void* buffer) {
    u32 start;
    u32 end;
    if (MAJOR(dev) == MAJOR_RAMDISK) {
        if (MINOR(dev) == 0) {
            start = ((u32) image) + first_block * 1024;
            end = start + 1024 * blocks - 1;
            if (end >= ((u32) image) + TEST_IMAGE_SIZE)
                end = ((u32) image) + TEST_IMAGE_SIZE - 1;
            memcpy((void*) start, buffer, end - start + 1);
            return end - start + 1;
        }
    }
    return -1;
}

ssize_t bc_read_stub(dev_t dev, ssize_t blocks, ssize_t first_block,
        void* buffer) {
    u32 start;
    u32 end;
    if (MAJOR(dev) == MAJOR_RAMDISK) {
        if (MINOR(dev) == 0) {
            start = ((u32) image) + first_block * 1024;
            end = start + 1024 * blocks - 1;
            if (end >= ((u32) image) + TEST_IMAGE_SIZE)
                end = ((u32) image) + TEST_IMAGE_SIZE - 1;
            memcpy(buffer, (void*) start, end - start + 1);
            return end - start + 1;
        }
    }
    return -1;
}

int bc_oc_stub(minor_dev_t device) {
    return 0;
}

/*
 * Stub for kmalloc/kfree
 */
void* kmalloc(u32 size) {
    return (void*) malloc(size);
}
void kfree(void* addr) {
    free(addr);
}

void mutex_up(semaphore_t* mutex) {

}

/*
 * Stub for do_time in rtc.c
 */
time_t do_time(time_t* ptr) {
    return time(ptr);
}

/*
 * Load test image into memory
 */
void reset() {
    int fd;
    fd = open("./rdimage", O_RDONLY);
    if (fd <= 0) {
        printf("Could not open image file rdimage for testing\n");
        exit(1);
    }
    read(fd, image, TEST_IMAGE_SIZE);
    close(fd);
}

/*
 * Set up test
 */
void setup() {
    int fd;
    bc_read = bc_read_stub;
    bc_write = bc_write_stub;
    ops.open = bc_oc_stub;
    ops.close = bc_oc_stub;
    image = (void*) malloc(TEST_IMAGE_SIZE);
    if (0 == image) {
        printf("Could not allocate memory for test image, bailing out\n");
        exit(1);
    }
    reset();
}

/*
 * Write copy of test image back to disk
 */
void save() {
    int fd;
    fd = open("./rdimage.new", O_RDWR + O_CREAT);
    if (fd <= 0) {
        printf("Could not open copy of file system image for writing\n");
        exit(1);
    }
    write(fd, (void*) image, TEST_IMAGE_SIZE);
}

/*
 * Testcase 1
 * Tested function: probe
 */
int testcase1() {
    int rc = fs_ext2_probe(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(1==rc);
    return 0;
}

/*
 * Testcase 2
 * Tested function: bc_read_bytes
 * Testcase: read less than a block with offset 0
 */
int testcase2() {
    char buffer[sizeof(ext2_superblock_t)];
    char* image_data = ((char*) image) + 1024;
    int i;
    int rc = bc_read_bytes(1, sizeof(ext2_superblock_t), buffer,
            DEVICE(MAJOR_RAMDISK, 0), 0);
    ASSERT(0==rc);
    for (i = 0; i < sizeof(ext2_superblock_t); i++)
        if (image_data[i] != buffer[i]) {
            printf("Test failed at index i=%d, have %x, expected %x\n", i,
                    buffer[i], image_data[i]);
            ASSERT(0);
        }
    return 0;
}

/*
 * Testcase 3
 * Tested function: bc_read_bytes
 * Testcase: read less than a block with offset 1
 */
int testcase3() {
    char buffer[10];
    char* image_data = ((char*) image) + 1024;
    int i;
    int rc = bc_read_bytes(1, 10, buffer, DEVICE(MAJOR_RAMDISK, 0), 1);
    ASSERT(0==rc);
    for (i = 0; i < 10; i++)
        if (image_data[i + 1] != buffer[i]) {
            printf("Test failed at index i=%d, have %x, expected %x\n", i,
                    buffer[i], image_data[i]);
            ASSERT(0);
        }
    return 0;
}

/*
 * Testcase 4
 * Tested function: bc_read_bytes
 * Testcase: read exactly one block with offset 0
 */
int testcase4() {
    char buffer[1024];
    char* image_data = ((char*) image) + 1024;
    int i;
    int rc = bc_read_bytes(1, 1024, buffer, DEVICE(MAJOR_RAMDISK, 0), 0);
    ASSERT(0==rc);
    for (i = 0; i < 1024; i++)
        if (image_data[i] != buffer[i]) {
            printf("Test failed at index i=%d, have %x, expected %x\n", i,
                    buffer[i], image_data[i]);
            ASSERT(0);
        }
    return 0;
}

/*
 * Testcase 5
 * Tested function: bc_read_bytes
 * Testcase: number of bytes less than a block, but range
 * is across a block boundary (offset < 1024, offset+bytes > 1024)
 */
int testcase5() {
    char buffer[100];
    char* image_data = ((char*) image) + 1024;
    int i;
    int rc = bc_read_bytes(1, 100, buffer, DEVICE(MAJOR_RAMDISK, 0), 1000);
    ASSERT(0==rc);
    for (i = 0; i < 100; i++)
        if (image_data[i + 1000] != buffer[i]) {
            printf("Test failed at index i=%d, have %x, expected %x\n", i,
                    buffer[i], image_data[i]);
            ASSERT(0);
        }
    return 0;
}

/*
 * Testcase 6
 * Tested function: bc_read_bytes
 * Testcase: offset exceeds one block
 */
int testcase6() {
    char buffer[10];
    char* image_data = ((char*) image) + 1024;
    int i;
    int rc = bc_read_bytes(1, 10, buffer, DEVICE(MAJOR_RAMDISK, 0), 1100);
    ASSERT(rc==0);
    for (i = 0; i < 10; i++)
        if (image_data[i + 1100] != buffer[i]) {
            printf("Test failed at index i=%d, have %x, expected %x\n", i,
                    buffer[i], image_data[i]);
            ASSERT(0);
        }
    return 0;
}

/*
 * Testcase 7
 * Tested function: fs_ext2_get_superblock
 * Testcase: first call to the function
 */
int testcase7() {
    superblock_t* super;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->device==DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super->data);
    ASSERT(EXT2_MAGIC_NUMBER == ((ext2_metadata_t*) super->data)->ext2_super->s_magic);
    ASSERT(super->get_inode);
    ASSERT(super->release_superblock);
    ASSERT(super->root==EXT2_ROOT_INODE);
    return 0;
}

/*
 * Testcase 8
 * Tested function: fs_ext2_get_inode
 * Testcase: first create a superblock, then get root inode and compare
 */
int testcase8() {
    superblock_t* super;
    inode_t* root;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->device==DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super->data);
    ASSERT(EXT2_MAGIC_NUMBER == ((ext2_metadata_t*) super->data)->ext2_super->s_magic);
    ASSERT(super->get_inode);
    ASSERT(super->release_superblock);
    ASSERT(super->root==EXT2_ROOT_INODE);
    root = fs_ext2_get_inode(DEVICE(MAJOR_RAMDISK, 0), EXT2_ROOT_INODE);
    ASSERT(root->dev == super->device);
    ASSERT(root->inode_nr == super->root);
    return 0;
}

/*
 * Testcase 9
 * Tested function: fs_ext2_get_inode
 * Testcase: call function twice and compare the resulting pointers. Verify that the reference count
 * is two after the second call and that each call increases the reference count of the superblock by one
 */
int testcase9() {
    superblock_t* super;
    inode_t* root;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->data);
    ASSERT(1==((ext2_metadata_t*) super->data)->reference_count);
    root = fs_ext2_get_inode(DEVICE(MAJOR_RAMDISK, 0), EXT2_ROOT_INODE);
    ASSERT(2==((ext2_metadata_t*) super->data)->reference_count);
    ASSERT(root==fs_ext2_get_inode(DEVICE(MAJOR_RAMDISK, 0), EXT2_ROOT_INODE));
    ASSERT(3==((ext2_metadata_t*) super->data)->reference_count);
    ASSERT(((ext2_inode_data_t*)(root->data))->reference_count==2);
    return 0;
}

/*
 * Testcase 10
 * Tested function: fs_ext2_get_inode
 * Testcase: call function once and check reference count.
 */
int testcase10() {
    superblock_t* super;
    inode_t* root;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    root = fs_ext2_get_inode(DEVICE(MAJOR_RAMDISK, 0), EXT2_ROOT_INODE);
    ASSERT(root);
    ASSERT(1==((ext2_inode_data_t*)(root->data))->reference_count);
    return 0;
}

/*
 * Testcase 11
 * Tested function: fs_ext2_get_superblock
 * Testcase: call function twice and verify that the result is
 * the same, i.e. that caching works
 */
int testcase11() {
    superblock_t* super;
    inode_t* root;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super==fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0)));
    return 0;
}

/*
 * Testcase 12
 * Tested function: fs_ext2_get_superblock
 * Testcase: call function twice and verify reference count. Then call release_superblock
 * and verify that reference count is reduced by one
 */
int testcase12() {
    superblock_t* super;
    inode_t* root;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(((ext2_metadata_t*)super->data)->reference_count==1);
    ASSERT(super==fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0)));
    ASSERT(((ext2_metadata_t*)super->data)->reference_count==2);
    super->release_superblock(super);
    ASSERT(((ext2_metadata_t*)super->data)->reference_count==1);
    return 0;
}

/*
 * Testcase 13
 * Tested function: fs_ext2_inode_read
 * Testcase: read from the first twelve blocks of a file with offset 0
 */
int testcase13() {
    char* buffer[1024];
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    ext2_direntry_t* direntry = (ext2_direntry_t*) buffer;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), EXT2_ROOT_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    ret = inode->iops->inode_read(inode, 1024, 0, buffer);
    ASSERT(ret);
    ASSERT(direntry->inode==2);
    return 0;
}

/*
 * Testcase 14
 * Tested function: fs_ext2_inode_read
 * Testcase: read from the first twelve blocks of a file with offset 0
 * and verify that only the specified number of bytes is read
 */
int testcase14() {
    char* buffer[1024];
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    ext2_direntry_t* direntry = (ext2_direntry_t*) buffer;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), EXT2_ROOT_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    /*
     * Put "magic number" at bytes 4-7
     */
    *((u32*) (buffer + 4)) = 0xfafd;
    ret = inode->iops->inode_read(inode, 4, 0, buffer);
    ASSERT(ret==4);
    ASSERT(direntry->inode==2);
    ASSERT(*((u32*)(buffer+4))==0xfafd);
    return 0;
}

/*
 * Testcase 15
 * Tested function: fs_ext2_inode_read
 * Testcase: read from the first twelve blocks of a file with offset 1
 * and verify that only the specified number of bytes is read
 * and that the read bytes are correct (first directory entry in root directory)
 */
int testcase15() {
    char buffer[1024];
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), EXT2_ROOT_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    /*
     * Put "magic number" at bytes 8-11
     */
    *((u32*) (buffer + 8)) = 0xfafd;
    ret = inode->iops->inode_read(inode, 7, 1, buffer + 1);
    ASSERT(ret==7);
    ASSERT(buffer[1]==0);
    ASSERT(buffer[4]==12);
    ASSERT(*((u32*)(buffer+8))==0xfafd);
    return 0;
}

/*
 * Testcase 16
 * Tested function: fs_ext2_inode_read
 * Testcase: read from the first twelve blocks of a file with offset
 * crossing a block border
 */
int testcase16() {
    char buffer[1024];
    int fd;
    int i;
    char diff[100];
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), TEST_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    /*
     * Put "magic number" at bytes 100-103
     */
    *((u32*) (buffer + 100)) = 0xfafd;
    ret = inode->iops->inode_read(inode, 100, 4000, buffer);
    ASSERT(ret==100);
    /*
     * Check that magic number is still there
     */
    ASSERT(*((u32*)(buffer+100))==0xfafd);
    /*
     * Compare with copy on local file system
     */
    fd = open(TEST_COPY, O_RDONLY);
    if (fd < 0) {
        printf("Could not open local file for comparison\n");
        ASSERT(0);
    }
    pread(fd, diff, 100, 4000);
    for (i = 0; i < 100; i++) {
        ASSERT(diff[i]==buffer[i]);
    }
    close(fd);
    return 0;
}

/*
 * Testcase 17
 * Tested function: fs_ext2_inode_read
 * Testcase: read from the first twelve blocks of a file
 * read an exact multiple of blocks
 */
int testcase17() {
    char buffer[2048];
    int fd;
    int i;
    char diff[2048];
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), TEST_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    ret = inode->iops->inode_read(inode, 2048, 0, buffer);
    ASSERT(ret==2048);
    /*
     * Compare with copy on local file system
     */
    fd = open(TEST_COPY, O_RDONLY);
    if (fd < 0) {
        printf("Could not open local file for comparison\n");
        ASSERT(0);
    }
    pread(fd, diff, 2048, 0);
    for (i = 0; i < 2048; i++) {
        ASSERT(diff[i]==buffer[i]);
    }
    close(fd);
    return 0;
}

/*
 * Testcase 18
 * Tested function: fs_ext2_inode_read
 * Testcase: read last direct block
 */
int testcase18() {
    char buffer[1024];
    int fd;
    int i;
    char diff[1024];
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), TEST_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    ret = inode->iops->inode_read(inode, 1024, 11 * 1024, buffer);
    ASSERT(ret==1024);
    /*
     * Compare with copy on local file system
     */
    fd = open(TEST_COPY, O_RDONLY);
    if (fd < 0) {
        printf("Could not open local file for comparison\n");
        ASSERT(0);
    }
    pread(fd, diff, 1024, 11 * 1024);
    for (i = 0; i < 1024; i++) {
        ASSERT(diff[i]==buffer[i]);
    }
    close(fd);
    return 0;
}

/*
 * Testcase 19
 * Tested function: fs_ext2_inode_read
 * Testcase: read first indirect block
 */
int testcase19() {
    u8 buffer[1028];
    int fd;
    int i;
    u8 diff[1028];
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), TEST_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    /*
     * Put "magic number" at bytes 1024-1027
     */
    *((u32*) (buffer + 1024)) = 0xfafd;
    ret = inode->iops->inode_read(inode, 1024, 12 * 1024, buffer);
    ASSERT(ret==1024);
    ASSERT(*((u32*)(buffer+1024))==0xfafd);
    /*
     * Compare with copy on local file system
     */
    fd = open(TEST_COPY, O_RDONLY);
    if (fd < 0) {
        printf("Could not open local file for comparison\n");
        ASSERT(0);
    }
    pread(fd, diff, 1024, 12 * 1024);
    for (i = 0; i < 1024; i++) {
        ASSERT(diff[i]==buffer[i]);
    }
    close(fd);
    return 0;
}

/*
 * Testcase 20
 * Tested function: fs_ext2_inode_read
 * Testcase: read across boundary of last direct and first indirect block
 */
int testcase20() {
    u8 buffer[104];
    int fd;
    int i;
    u8 diff[100];
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), TEST_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    /*
     * Put "magic number" at bytes 100-103
     */
    *((u32*) (buffer + 100)) = 0xfafd;
    ret = inode->iops->inode_read(inode, 100, 11 * 1024 + 1020, buffer);
    ASSERT(ret==100);
    ASSERT(*((u32*)(buffer+100))==0xfafd);
    /*
     * Compare with copy on local file system
     */
    fd = open(TEST_COPY, O_RDONLY);
    if (fd < 0) {
        printf("Could not open local file for comparison\n");
        ASSERT(0);
    }
    pread(fd, diff, 100, 11 * 1024 + 1020);
    for (i = 0; i < 100; i++) {
        ASSERT(diff[i]==buffer[i]);
    }
    close(fd);
    return 0;
}

/*
 * Testcase 21
 * Tested function: fs_ext2_inode_read
 * Testcase: read last indirect block
 */
int testcase21() {
    u8 buffer[1028];
    int fd;
    int i;
    u8 diff[1024];
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), TEST_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    /*
     * Put "magic number" at bytes 1024-1027
     */
    *((u32*) (buffer + 1024)) = 0xfafd;
    ret = inode->iops->inode_read(inode, 1024, EXT2_LAST_INDIRECT * 1024,
            buffer);
    ASSERT(ret==1024);
    ASSERT(*((u32*)(buffer+1024))==0xfafd);
    /*
     * Compare with copy on local file system
     */
    fd = open(TEST_COPY, O_RDONLY);
    if (fd < 0) {
        printf("Could not open local file for comparison\n");
        ASSERT(0);
    }
    ret = pread(fd, diff, 1024, EXT2_LAST_INDIRECT * 1024);
    ASSERT(ret==1024);
    for (i = 0; i < 1024; i++) {
        ASSERT(diff[i]==buffer[i]);
    }
    close(fd);
    return 0;
}

/*
 * Testcase 22
 * Tested function: fs_ext2_inode_read
 * Testcase: read first double indirect block
 */
int testcase22() {
    u8 buffer[1028];
    int fd;
    int i;
    u8 diff[1024];
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), TEST_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    /*
     * Put "magic number" at bytes 1024-1027
     */
    *((u32*) (buffer + 1024)) = 0xfafd;
    ret = inode->iops->inode_read(inode, 1024, (EXT2_LAST_INDIRECT + 1) * 1024,
            buffer);
    ASSERT(ret==1024);
    ASSERT(*((u32*)(buffer+1024))==0xfafd);
    /*
     * Compare with copy on local file system
     */
    fd = open(TEST_COPY, O_RDONLY);
    if (fd < 0) {
        printf("Could not open local file for comparison\n");
        ASSERT(0);
    }
    pread(fd, diff, 1024, (EXT2_LAST_INDIRECT + 1) * 1024);
    for (i = 0; i < 1024; i++) {
        ASSERT(diff[i]==buffer[i]);
    }
    close(fd);
    return 0;
}

/*
 * Testcase 23
 * Tested function: fs_ext2_inode_read
 * Testcase: read first two double indirect blocks
 */
int testcase23() {
    u8 buffer[2052];
    int fd;
    int i;
    u8 diff[2048];
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), TEST_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    /*
     * Put "magic number" at bytes 2048-2051
     */
    *((u32*) (buffer + 2048)) = 0xfafd;
    ret = inode->iops->inode_read(inode, 2048, (EXT2_LAST_INDIRECT + 1) * 1024,
            buffer);
    ASSERT(ret==2048);
    ASSERT(*((u32*)(buffer+2048))==0xfafd);
    /*
     * Compare with copy on local file system
     */
    fd = open(TEST_COPY, O_RDONLY);
    if (fd < 0) {
        printf("Could not open local file for comparison\n");
        ASSERT(0);
    }
    pread(fd, diff, 2048, (EXT2_LAST_INDIRECT + 1) * 1024);
    for (i = 0; i < 2048; i++) {
        ASSERT(diff[i]==buffer[i]);
    }
    close(fd);
    return 0;
}

/*
 * Testcase 24
 * Tested function: fs_ext2_inode_read
 * Testcase: read from indirect block and double indirect block
 */
int testcase24() {
    u8 buffer[104];
    int fd;
    int i;
    u8 diff[100];
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), TEST_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    /*
     * Put "magic number" at bytes 100-103
     */
    *((u32*) (buffer + 100)) = 0xfafd;
    ret = inode->iops->inode_read(inode, 100, EXT2_LAST_INDIRECT * 1024 + 1000,
            buffer);
    ASSERT(ret==100);
    ASSERT(*((u32*)(buffer+100))==0xfafd);
    /*
     * Compare with copy on local file system
     */
    fd = open(TEST_COPY, O_RDONLY);
    if (fd < 0) {
        printf("Could not open local file for comparison\n");
        ASSERT(0);
    }
    pread(fd, diff, 100, EXT2_LAST_INDIRECT * 1024 + 1000);
    for (i = 0; i < 100; i++) {
        ASSERT(diff[i]==buffer[i]);
    }
    close(fd);
    return 0;
}

/*
 * Testcase 25
 * Tested function: fs_ext2_inode_read
 * Testcase: read from second block which is in double indirect area
 */
int testcase25() {
    u8 buffer[104];
    int fd;
    int i;
    u8 diff[100];
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), TEST_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    /*
     * Put "magic number" at bytes 100-103
     */
    *((u32*) (buffer + 100)) = 0xfafd;
    ret = inode->iops->inode_read(inode, 100, (EXT2_LAST_INDIRECT + 2) * 1024,
            buffer);
    ASSERT(ret==100);
    ASSERT(*((u32*)(buffer+100))==0xfafd);
    /*
     * Compare with copy on local file system
     */
    fd = open(TEST_COPY, O_RDONLY);
    if (fd < 0) {
        printf("Could not open local file for comparison\n");
        ASSERT(0);
    }
    pread(fd, diff, 100, (EXT2_LAST_INDIRECT + 2) * 1024);
    for (i = 0; i < 100; i++) {
        ASSERT(diff[i]==buffer[i]);
    }
    close(fd);
    return 0;
}

/*
 * Testcase 26
 * Tested function: fs_ext2_inode_read
 * Testcase: read from block 256 within indirect area,
 * i.e. we read from the first block in the file which is
 * addressed via the second entry in the double indirect block
 */
int testcase26() {
    u8 buffer[104];
    int fd;
    int i;
    u8 diff[100];
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), TEST_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    /*
     * Put "magic number" at bytes 100-103
     */
    *((u32*) (buffer + 100)) = 0xfafd;
    ret = inode->iops->inode_read(inode, 100, (EXT2_LAST_INDIRECT
            +EXT2_INDIRECT_BLOCKS + 1) * 1024, buffer);
    ASSERT(ret==100);
    ASSERT(*((u32*)(buffer+100))==0xfafd);
    /*
     * Compare with copy on local file system
     */
    fd = open(TEST_COPY, O_RDONLY);
    if (fd < 0) {
        printf("Could not open local file for comparison\n");
        ASSERT(0);
    }
    pread(fd, diff, 100, (EXT2_LAST_INDIRECT + EXT2_INDIRECT_BLOCKS + 1) * 1024);
    for (i = 0; i < 100; i++) {
        ASSERT(diff[i]==buffer[i]);
    }
    close(fd);
    return 0;
}

/*
 * Testcase 27
 * Tested function: fs_ext2_inode_read
 * Testcase: read from first block within triple indirect area
 */
int testcase27() {
    u8 buffer[104];
    int fd;
    int i;
    u8 diff[100];
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), TEST_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    /*
     * Put "magic number" at bytes 100-103
     */
    *((u32*) (buffer + 100)) = 0xfafd;
    ret = inode->iops->inode_read(inode, 100, (EXT2_LAST_DOUBLE_INDIRECT + 1)
            * 1024, buffer);
    ASSERT(ret==100);
    ASSERT(*((u32*)(buffer+100))==0xfafd);
    /*
     * Compare with copy on local file system
     */
    fd = open(TEST_COPY, O_RDONLY);
    if (fd < 0) {
        printf("Could not open local file for comparison\n");
        ASSERT(0);
    }
    pread(fd, diff, 100, (EXT2_LAST_DOUBLE_INDIRECT + 1) * 1024);
    for (i = 0; i < 100; i++) {
        ASSERT(diff[i]==buffer[i]);
    }
    close(fd);
    return 0;
}

/*
 * Testcase 28
 * Tested function: fs_ext2_inode_read
 * Testcase: read from last block within double indirect area and first block within triple indirect area
 */
int testcase28() {
    u8 buffer[104];
    int fd;
    int i;
    u8 diff[100];
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), TEST_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    /*
     * Put "magic number" at bytes 100-103
     */
    *((u32*) (buffer + 100)) = 0xfafd;
    ret = inode->iops->inode_read(inode, 100, (EXT2_LAST_DOUBLE_INDIRECT + 1)
            * 1024 - 50, buffer);
    ASSERT(ret==100);
    ASSERT(*((u32*)(buffer+100))==0xfafd);
    /*
     * Compare with copy on local file system
     */
    fd = open(TEST_COPY, O_RDONLY);
    if (fd < 0) {
        printf("Could not open local file for comparison\n");
        ASSERT(0);
    }
    pread(fd, diff, 100, (EXT2_LAST_DOUBLE_INDIRECT + 1) * 1024 - 50);
    for (i = 0; i < 100; i++) {
        ASSERT(diff[i]==buffer[i]);
    }
    close(fd);
    return 0;
}

/*
 * Testcase 29
 * Tested function: fs_ext2_inode_read
 * Testcase: read beyond end of file
 */
int testcase29() {
    u8 buffer[14];
    int fd;
    int i;
    u8 diff[10];
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), TEST_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    /*
     * Put "magic number" at bytes 10-13
     */
    *((u32*) (buffer + 10)) = 0xfafd;
    /*
     * Read 100 bytes from end of file - 10 --> only 10 bytes should be read
     */
    ret = inode->iops->inode_read(inode, 100, TEST_FILE_SIZE - 10, buffer);
    ASSERT(ret==10);
    ASSERT(*((u32*)(buffer+10))==0xfafd);
    /*
     * Compare with copy on local file system
     */
    fd = open(TEST_COPY, O_RDONLY);
    if (fd < 0) {
        printf("Could not open local file for comparison\n");
        ASSERT(0);
    }
    pread(fd, diff, 100, TEST_FILE_SIZE - 10);
    for (i = 0; i < 10; i++) {
        ASSERT(diff[i]==buffer[i]);
    }
    close(fd);
    return 0;
}

/*
 * Testcase 30
 * Tested function: fs_ext2_inode_read
 * Testcase: start read beyond end of file
 */
int testcase30() {
    u8 buffer[14];
    int fd;
    int i;
    u8 diff[10];
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), TEST_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    /*
     * Put "magic number" at bytes 10-13
     */
    *((u32*) (buffer + 10)) = 0xfafd;
    /*
     * Read from end of file
     */
    ret = inode->iops->inode_read(inode, 100, TEST_FILE_SIZE, buffer);
    ASSERT(ret==0);
    ASSERT(*((u32*)(buffer+10))==0xfafd);
    return 0;
}

/*
 * Testcase 31
 * Tested function: fs_ext2_inode_read
 * Testcase: read entire file in units of 1000 bytes
 */
int testcase31() {
    u8 buffer[1000];
    int fd;
    int i;
    int j;
    u8 diff[1000];
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), TEST_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    fd = open(TEST_COPY, O_RDONLY);
    if (fd < 0) {
        printf("Could not open local file for comparison\n");
        ASSERT(0);
    }
    /*
     * Read through file in units of 1000
     */
    for (i = 0; i < TEST_FILE_SIZE / 1000; i++) {
        ret = inode->iops->inode_read(inode, 1000, i * 1000, buffer);
        ASSERT(ret==1000);
        pread(fd, diff, 1000, i * 1000);
        for (j = 0; j < 1000; j++) {
            ASSERT(diff[j]==buffer[j]);
        }
    }
    /*
     * Read last patch of file
     */
    ret = inode->iops->inode_read(inode, TEST_FILE_SIZE % 1000, (TEST_FILE_SIZE
            / 1000) * 1000, buffer);
    ASSERT(ret==(TEST_FILE_SIZE % 1000));
    pread(fd, diff, ret, (TEST_FILE_SIZE / 1000) * 1000);
    for (j = 0; j < ret; j++) {
        ASSERT(diff[j]==buffer[j]);
    }
    close(fd);
    return 0;
}

/*
 * Testcase 32
 * Tested function: fs_ext2_inode_release
 * Testcase: get inode, then call release on it and check that cache is empty
 */
int testcase32() {
    superblock_t* super;
    inode_t* test;
    ext2_metadata_t* meta;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    meta = (ext2_metadata_t*) super->data;
    ASSERT(meta);
    /*
     * Cache should be empty at this point
     */
    ASSERT(0==meta->inodes_head);
    /*
     * Now get test inode
     */
    test = fs_ext2_get_inode(DEVICE(MAJOR_RAMDISK, 0), TEST_INODE);
    ASSERT(test);
    ASSERT(1==((ext2_inode_data_t*)(test->data))->reference_count);
    /*
     * Now we should have one item in the list
     */
    ASSERT(meta->inodes_head);
    ASSERT(0==meta->inodes_head->next);
    /*
     * Release inode - we should now have the same state as
     * before calling fs_ext2_get_inode for test
     */
    fs_ext2_inode_release(test);
    ASSERT(0==meta->inodes_head);
    return 0;
}

/*
 * Testcase 33
 * Tested function: fs_ext2_inode_clone, fs_ext2_inode_release
 * Testcase: get inode, then call clone on it and check reference count
 * Then call inode_release and check reference count once more
 */
int testcase33() {
    superblock_t* super;
    inode_t* test;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(((ext2_metadata_t*)super->data)->reference_count==1);
    test = fs_ext2_get_inode(DEVICE(MAJOR_RAMDISK, 0), TEST_INODE);
    ASSERT(test);
    ASSERT(1==((ext2_inode_data_t*)(test->data))->reference_count);
    ASSERT(((ext2_metadata_t*)super->data)->reference_count==2);
    fs_ext2_inode_clone(test);
    ASSERT(2==((ext2_inode_data_t*)(test->data))->reference_count);
    ASSERT(((ext2_metadata_t*)super->data)->reference_count==3);
    fs_ext2_inode_release(test);
    ASSERT(1==((ext2_inode_data_t*)(test->data))->reference_count);
    ASSERT(((ext2_metadata_t*)super->data)->reference_count==2);
    return 0;
}

/*
 * Testcase 34
 * Tested function: fs_ext2_release_superblock
 * Testcase: release superblock twice, but keep reference to root inode
 * second release does not lead to destruction of superblock
 */
int testcase34() {
    superblock_t* super;
    fs_ext2_init();
    inode_t* root;
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(((ext2_metadata_t*)super->data)->reference_count==1);
    ASSERT(super==fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0)));
    ASSERT(((ext2_metadata_t*)super->data)->reference_count==2);
    root = super->get_inode(super->device, super->root);
    ASSERT(root);
    ASSERT(1== ((ext2_inode_data_t*) root->data)->reference_count);
    ASSERT(((ext2_metadata_t*)super->data)->reference_count==3);
    /*
     * Now release superblock - reference count of superblock should
     * be two again, reference count of root inode still one
     */
    fs_ext2_release_superblock(super);
    ASSERT(((ext2_metadata_t*)super->data)->reference_count==2);
    ASSERT(1== ((ext2_inode_data_t*) root->data)->reference_count);
    /*
     * Do second release
     */
    fs_ext2_release_superblock(super);
    ASSERT(((ext2_metadata_t*)super->data)->reference_count==1);
    ASSERT(1== ((ext2_inode_data_t*) root->data)->reference_count);
    return 0;
}

/*
 * Testcase 35
 * Tested function: fs_ext2_get_inode
 * Testcase: get one inode twice and verify that each call to get_inode
 * increases reference count of superblock
 */
int testcase35() {
    superblock_t* super;
    fs_ext2_init();
    inode_t* root;
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(((ext2_metadata_t*)super->data)->reference_count==1);
    root = super->get_inode(super->device, super->root);
    ASSERT(root);
    ASSERT(1== ((ext2_inode_data_t*) root->data)->reference_count);
    ASSERT(((ext2_metadata_t*)super->data)->reference_count==2);
    ASSERT(root==super->get_inode(super->device, super->root));
    ASSERT(2== ((ext2_inode_data_t*) root->data)->reference_count);
    ASSERT(((ext2_metadata_t*)super->data)->reference_count==3);
    return 0;
}

/*
 * Testcase 36
 * Tested function: fs_ext2_get_direntry
 * Testcase: read first directory entry from root dir
 */
int testcase36() {
    superblock_t* super;
    direntry_t direntry;
    fs_ext2_init();
    inode_t* root;
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    root = super->get_inode(super->device, super->root);
    ASSERT(root);
    ASSERT(0==fs_ext2_get_direntry(root, 0, &direntry));
    ASSERT(0==strcmp(direntry.name, "."));
    return 0;
}

/*
 * Testcase 37
 * Tested function: fs_ext2_get_direntry
 * Testcase: read second directory entry from root dir
 */
int testcase37() {
    superblock_t* super;
    direntry_t direntry;
    fs_ext2_init();
    inode_t* root;
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    root = super->get_inode(super->device, super->root);
    ASSERT(root);
    ASSERT(0==fs_ext2_get_direntry(root, 1, &direntry));
    ASSERT(0==strcmp(direntry.name, ".."));
    return 0;
}

/*
 * Testcase 38
 * Tested function: fs_ext2_get_direntry
 * Testcase: read non-existing directory entry from root dir
 */
int testcase38() {
    superblock_t* super;
    direntry_t direntry;
    fs_ext2_init();
    inode_t* root;
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    root = super->get_inode(super->device, super->root);
    ASSERT(root);
    ASSERT(0!=fs_ext2_get_direntry(root, 100, &direntry));
    return 0;
}

/*
 * Testcase 39
 * Tested function: fs_ext2_inode_write
 * Testcase: write to the first block of a file. Then read the data back and check that the write was successful
 */
int testcase39() {
    u8 buffer[4];
    u8 write_buffer[4];
    int fd;
    int i;
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), TEST_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    /*
     * Put some data into the buffer
     */
    *((u32*) write_buffer) = 0xffeeddcc;
    ret = inode->iops->inode_write(inode, 4, 0, write_buffer);
    ASSERT(ret==4);
    /*
     * Read data back and compare
     */
    ret = inode->iops->inode_read(inode, 4, 0, buffer);
    for (i = 0; i < 4; i++)
        ASSERT(write_buffer[i]==buffer[i]);
    return 0;
}

/*
 * Testcase 40
 * Tested function: fs_ext2_inode_read
 * Testcase: read from a file with a hole in the direct addressable range and verify that the hole reads as zeroes
 */
int testcase40() {
    u8 buffer[10];
    int fd;
    int i;
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), SAMPLE_A_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    /*
     * Put some data into the buffer
     */
    for (i = 0; i < 10; i++)
        buffer[i] = 0xff;
    ret = inode->iops->inode_read(inode, 10, 1024, buffer);
    ASSERT(ret==10);
    /*
     * Check that buffer content is now zero
     */
    for (i = 0; i < 10; i++)
        ASSERT(buffer[i]==0);
    return 0;
}

/*
 * Testcase 41
 * Tested function: fs_ext2_inode_read
 * Testcase: read from a file with a hole in the indirect block range
 */
int testcase41() {
    u8 buffer[10];
    int fd;
    int i;
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), SAMPLE_B_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    /*
     * Put some data into the buffer
     */
    for (i = 0; i < 10; i++)
        buffer[i] = 0xff;
    ret = inode->iops->inode_read(inode, 10, 1024 * (12 + 1024 / sizeof(u32))
            - 5, buffer);
    ASSERT(ret==10);
    /*
     * Check that buffer content is now zero
     * for the first five bytes read
     */
    for (i = 0; i < 5; i++)
        ASSERT(buffer[i]==0);
    ASSERT(buffer[5]=='a');
    return 0;
}

/*
 * Testcase 42
 * Tested function: fs_ext2_inode_read
 * Testcase: read from a file with a hole in the triple indirect block range
 */
int testcase42() {
    u8 buffer[10];
    int fd;
    int i;
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), SAMPLE_C_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    /*
     * Put some data into the buffer
     */
    for (i = 0; i < 10; i++)
        buffer[i] = 0xff;
    ret = inode->iops->inode_read(inode, 10, 1024 * (12 + 256 + 256 * 256) - 5,
            buffer);
    ASSERT(ret==10);
    /*
     * Check that buffer content is now zero
     * for the first five bytes read
     */
    for (i = 0; i < 5; i++)
        ASSERT(buffer[i]==0);
    ASSERT(buffer[5]=='a');
    return 0;
}

/*
 * Testcase 43
 * Tested function: fs_ext2_inode_write
 * Testcase: write to a region of 10 bytes overlapping with the first and second block of a file
 */
int testcase43() {
    u8 buffer[2048];
    u8 write_buffer[10];
    u8 orig_content[2048];
    int fd;
    int i;
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), TEST_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    /*
     * Store original content of first two blocks
     */
    ret = inode->iops->inode_read(inode, 2048, 0, orig_content);
    ASSERT(2048==ret);
    /*
     * Put some data into the buffer
     */
    memcpy((void*) write_buffer, (void*) "0123456789", 10);
    ret = inode->iops->inode_write(inode, 10, 1020, write_buffer);
    ASSERT(ret==10);
    /*
     * Read data back and compare
     */
    ret = inode->iops->inode_read(inode, 2048, 0, buffer);
    ASSERT(2048==ret);
    for (i = 0; i < 10; i++)
        ASSERT(write_buffer[i]==buffer[i+1020]);
    /*
     * Now make sure that the remainder of the file has not changed
     */
    for (i = 0; i < 1020; i++)
        ASSERT(buffer[i]==orig_content[i]);
    for (i = 1030; i < 2048; i++)
        if (buffer[i] != orig_content[i]) {
            printf(
                    "Comparison failed for i=%d, orig_content=%x, new content=%x\n",
                    i, orig_content[i], buffer[i]);
            ASSERT(0);
        }
    return 0;
}

/*
 * Testcase 44
 * Tested function: fs_ext2_inode_write
 * Testcase: write to a region of 10 bytes contained entirely within the first block but not starting at byte 0
 */
int testcase44() {
    u8 buffer[2048];
    u8 write_buffer[10];
    u8 orig_content[2048];
    int fd;
    int i;
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), TEST_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    /*
     * Store original content of first two blocks
     */
    ret = inode->iops->inode_read(inode, 2048, 0, orig_content);
    ASSERT(2048==ret);
    /*
     * Put some data into the buffer
     */
    memcpy((void*) write_buffer, (void*) "0123456789", 10);
    ret = inode->iops->inode_write(inode, 10, 100, write_buffer);
    ASSERT(ret==10);
    /*
     * Read data back and compare
     */
    ret = inode->iops->inode_read(inode, 2048, 0, buffer);
    ASSERT(2048==ret);
    for (i = 0; i < 10; i++)
        ASSERT(write_buffer[i]==buffer[i+100]);
    /*
     * Now make sure that the remainder of the file has not changed
     */
    for (i = 0; i < 100; i++)
        ASSERT(buffer[i]==orig_content[i]);
    for (i = 110; i < 2048; i++)
        if (buffer[i] != orig_content[i]) {
            printf(
                    "Comparison failed for i=%d, orig_content=%x, new content=%x\n",
                    i, orig_content[i], buffer[i]);
            ASSERT(0);
        }
    return 0;
}

/*
 * Testcase 45
 * Tested function: fs_ext2_inode_write
 * Testcase: write to a region of 1030 bytes starting at the first byte of block 0
 */
int testcase45() {
    u8 buffer[2048];
    u8 write_buffer[1030];
    u8 orig_content[2048];
    int fd;
    int i;
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), TEST_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    /*
     * Store original content of first two blocks
     */
    ret = inode->iops->inode_read(inode, 2048, 0, orig_content);
    ASSERT(2048==ret);
    /*
     * Put some data into the buffer
     */
    memset((void*) write_buffer, 0xff, 1030);
    ret = inode->iops->inode_write(inode, 1030, 0, write_buffer);
    ASSERT(ret==1030);
    /*
     * Read data back and compare
     */
    ret = inode->iops->inode_read(inode, 2048, 0, buffer);
    ASSERT(2048==ret);
    for (i = 0; i < 1030; i++)
        ASSERT(write_buffer[i]==buffer[i]);
    /*
     * Now make sure that the remainder of the file has not changed
     */
    ASSERT(buffer[i]==orig_content[i]);
    for (i = 1030; i < 2048; i++)
        if (buffer[i] != orig_content[i]) {
            printf(
                    "Comparison failed for i=%d, orig_content=%x, new content=%x\n",
                    i, orig_content[i], buffer[i]);
            ASSERT(0);
        }
    return 0;
}

/*
 * Testcase 46
 * Tested function: fs_ext2_inode_write
 * Testcase: write into a hole in the direct area
 */
int testcase46() {
    u8 buffer[10];
    u8 check_buffer[10];
    int fd;
    int i;
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), SAMPLE_A_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    /*
     * Put some data into the buffer
     */
    for (i = 0; i < 10; i++)
        buffer[i] = 0xff;
    /*
     * Write the buffer content to the start of the second block (which is not yet allocated)
     */
    ret = inode->iops->inode_write(inode, 10, 1024, buffer);
    ASSERT(ret==10);
    /*
     * Read and compare
     */
    ret = inode->iops->inode_read(inode, 10, 1024, check_buffer);
    for (i = 0; i < 10; i++)
        ASSERT(buffer[i]==check_buffer[i]);
    return 0;
}

/*
 * Testcase 47
 * Tested function: fs_ext2_inode_read
 * Testcase: write to a hole in the indirect block range
 */
int testcase47() {
    u8 buffer[10];
    u8 read_buffer[10];
    int fd;
    int i;
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), SAMPLE_B_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    /*
     * Put some data into the buffer
     */
    for (i = 0; i < 10; i++)
        buffer[i] = 0xff;
    ret = inode->iops->inode_write(inode, 10, 1024 * (12 + 1024 / sizeof(u32))
            - 5, buffer);
    ASSERT(ret==10);
    /*
     * Read back content and compare
     */
    ret = inode->iops->inode_read(inode, 10, 1024 * (12 + 1024 / sizeof(u32))
            - 5, read_buffer);
    ASSERT(ret==10);
    /*
     * Check that buffer content is as expected
     */
    for (i = 0; i < 10; i++)
        ASSERT(buffer[i]==read_buffer[i]);
    return 0;
}

/*
 * Testcase 48
 * Tested function: fs_ext2_inode_write
 * Testcase: given a file of total length 10, add another 1024 bytes so that a new block needs to
 * be allocated on the device. Check that write was successful by reading back the data. In addition,
 * verify that the next inode is not changed
 */
int testcase48() {
    u8 buffer[1024];
    u8 read_buffer[1024];
    int fd;
    int i;
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    inode_t* next_inode;
    ext2_inode_t* next_ext2_inode;
    ext2_inode_t next_ext2_inode_backup;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), SAMPLE_D_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    ASSERT(inode->size == 10);
    /*
     * Backup up next ext2 inode for later comparison
     */
    next_inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), SAMPLE_D_INODE + 1);
    ASSERT(next_inode);
    next_ext2_inode = ((ext2_inode_data_t*) next_inode->data)->ext2_inode;
    memcpy((void*) &next_ext2_inode_backup, next_ext2_inode,
            sizeof(ext2_inode_t));
    /*
     * Put some data into the buffer
     */
    for (i = 0; i < 1024; i++)
        buffer[i] = 'x';
    ret = inode->iops->inode_write(inode, 1024, 10, buffer);
    ASSERT(ret==1024);
    ASSERT(inode->size == 1034);
    /*
     * Read back content and compare
     */
    ret = inode->iops->inode_read(inode, 1024, 10, read_buffer);
    ASSERT(ret==1024);
    for (i = 0; i < 1024; i++)
        ASSERT(read_buffer[i]=='x');
    /*
     * Read next inode again from disk and check if anything has changed
     */
    next_inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), SAMPLE_D_INODE + 1);
    ASSERT(next_inode);
    next_ext2_inode = ((ext2_inode_data_t*) next_inode->data)->ext2_inode;
    for (i = 0; i < sizeof(ext2_inode_t); i++) {
        if (((char*) (&next_ext2_inode_backup))[i]
                != ((char*) (next_ext2_inode))[i]) {
            printf("Inode has changed at byte %d\n", i);
            ASSERT(0);
        }
    }
    return 0;
}

/*
 * Testcase 49
 * Tested function: fs_ext2_inode_write
 * Testcase: extend a file consisting of two blocks only into the area described by the indirect block.
 * NOTE: this test cases reuses the file used for testcase 48!!
 */
int testcase49() {
    u8 buffer[1024];
    u8 read_buffer[1024];
    int fd;
    int i;
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), SAMPLE_D_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    /*
     * Put some data into the buffer
     */
    for (i = 0; i < 1024; i++)
        buffer[i] = 'x';
    /*
     * Write twelve blocks to fill up the entire direct area
     */
    for (i = 0; i < 12; i++) {
        ret = inode->iops->inode_write(inode, 1024, i * 1024, buffer);
        ASSERT(ret==1024);
    }
    /*
     * Write first block within the indirect area
     */
    ret = inode->iops->inode_write(inode, 1024, 12 * 1024, buffer);
    ASSERT(ret==1024);
    /*
     * Read back content and compare
     */
    ret = inode->iops->inode_read(inode, 1024, 12 * 1024, read_buffer);
    ASSERT(ret==1024);
    for (i = 0; i < 1024; i++)
        ASSERT(read_buffer[i]=='x');
    return 0;
}

/*
 * Testcase 50
 * Tested function: fs_ext2_inode_write
 * Testcase: extend a file which already extends into the indirect area one more block
 * NOTE: this test cases reuses the file used for testcase 49!!
 */
int testcase50() {
    u8 buffer[1024];
    u8 read_buffer[1024];
    int fd;
    int i;
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), SAMPLE_D_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    /*
     * Put some data into the buffer
     */
    for (i = 0; i < 1024; i++)
        buffer[i] = 'x';
    /*
     * Write second block within the indirect area
     */
    ret = inode->iops->inode_write(inode, 1024, 13 * 1024, buffer);
    ASSERT(ret==1024);
    /*
     * Read back content and compare
     */
    ret = inode->iops->inode_read(inode, 1024, 13 * 1024, read_buffer);
    ASSERT(ret==1024);
    for (i = 0; i < 1024; i++)
        ASSERT(read_buffer[i]=='x');
    return 0;
}

/*
 * Testcase 51
 * Tested function: fs_ext2_inode_write
 * Testcase: Write into a hole in the double indirect area, i.e. for a file for which the entire double indirect area,
 * write into block 268 (which is the first block in the double indirect area)
 */
int testcase51() {
    u8 buffer[1024];
    u8 read_buffer[1024];
    int fd;
    int i;
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), SAMPLE_C_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    /*
     * Put some data into the buffer
     */
    for (i = 0; i < 1024; i++)
        buffer[i] = 'x';
    /*
     * Write first block within the double indirect area
     */
    ret = inode->iops->inode_write(inode, 1024, 268 * 1024, buffer);
    ASSERT(ret==1024);
    /*
     * Read back content and compare
     */
    ret = inode->iops->inode_read(inode, 1024, 268 * 1024, read_buffer);
    ASSERT(ret==1024);
    for (i = 0; i < 1024; i++)
        ASSERT(read_buffer[i]=='x');
    return 0;
}

/*
 * Testcase 52
 * Tested function: fs_ext2_inode_write
 * Testcase: extend a file which already extends into the indirect area into the double indirect area
 * (i.e. into the first block of the double indirect area which is block 268)
 * NOTE: this test cases reuses the file used for testcase 50!!
 */
int testcase52() {
    u8 buffer[1024];
    u8 read_buffer[1024];
    int fd;
    int i;
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), SAMPLE_D_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    /*
     * Put some data into the buffer
     */
    for (i = 0; i < 1024; i++)
        buffer[i] = 'x';
    /*
     * Write first block within the double indirect area
     */
    ret = inode->iops->inode_write(inode, 1024, 268 * 1024, buffer);
    ASSERT(ret==1024);
    /*
     * Read back content and compare
     */
    ret = inode->iops->inode_read(inode, 1024, 268 * 1024, read_buffer);
    ASSERT(ret==1024);
    for (i = 0; i < 1024; i++)
        ASSERT(read_buffer[i]=='x');
    return 0;
}

/*
 * Testcase 53
 * Tested function: fs_ext2_inode_write
 * Testcase: extend a file which already extends into the the double indirect area
 * by one more block
 * NOTE: this test cases reuses the file used for testcase 52!!
 */
int testcase53() {
    u8 buffer[2048];
    u8 read_buffer[2048];
    int fd;
    int i;
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), SAMPLE_D_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    /*
     * Put some data into the buffer
     */
    for (i = 0; i < 2048; i++)
        buffer[i] = 'x';
    /*
     * Write second and third block within the double indirect area
     */
    ret = inode->iops->inode_write(inode, 2048, 269 * 1024, buffer);
    ASSERT(ret==2048);
    /*
     * Read back content and compare
     */
    ret = inode->iops->inode_read(inode, 2048, 269 * 1024, read_buffer);
    ASSERT(ret==2048);
    for (i = 0; i < 2048; i++)
        ASSERT(read_buffer[i]=='x');
    return 0;
}

/*
 * Testcase 54
 * Tested function: fs_ext2_inode_write
 * Testcase: extend a file which already extends into the the double indirect area
 * into the triple indirect area
 * NOTE: this test cases reuses the file used for testcase 53!!
 */
int testcase54() {
    u8 buffer[100];
    u8 read_buffer[100];
    int fd;
    int i;
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), SAMPLE_D_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    /*
     * Put some data into the buffer
     */
    for (i = 0; i < 100; i++)
        buffer[i] = 'y';
    /*
     * Write 100 bytes to first block within double indirect area
     */
    ret = inode->iops->inode_write(inode, 100, (EXT2_LAST_DOUBLE_INDIRECT + 1)
            * 1024, buffer);
    ASSERT(ret==100);
    /*
     * Read back content and compare
     */
    ret = inode->iops->inode_read(inode, 100, (EXT2_LAST_DOUBLE_INDIRECT + 1)
            * 1024, read_buffer);
    ASSERT(ret==100);
    for (i = 0; i < 100; i++)
        ASSERT(read_buffer[i]=='y');
    return 0;
}

/*
 * Testcase 55
 * Tested function: fs_ext2_inode_write
 * Testcase: extend a file which already extends into the the triple indirect area
 * by one additional block
 * NOTE: this test cases reuses the file used for testcase 54!!
 */
int testcase55() {
    u8 buffer[100];
    u8 read_buffer[100];
    int fd;
    int i;
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), SAMPLE_D_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    /*
     * Put some data into the buffer
     */
    for (i = 0; i < 100; i++)
        buffer[i] = 'y';
    /*
     * Write 100 bytes to second block within double indirect area, using offset 5
     */
    ret = inode->iops->inode_write(inode, 100, (EXT2_LAST_DOUBLE_INDIRECT + 2)
            * 1024 + 5, buffer);
    ASSERT(ret==100);
    /*
     * Read back content and compare
     */
    ret = inode->iops->inode_read(inode, 100, (EXT2_LAST_DOUBLE_INDIRECT + 2)
            * 1024 + 5, read_buffer);
    ASSERT(ret==100);
    for (i = 0; i < 100; i++)
        ASSERT(read_buffer[i]=='y');
    return 0;
}

/*
 * Testcase 56
 * Tested function: fs_ext2_inode_write
 * Testcase: read and write an entire file in units of 1000 bytes and check after each write
 * that the data has actually been written.
 */
int testcase56() {
    u8 buffer[1000];
    u8 read_buffer[1000];
    u32 number_of_chunks = TEST_FILE_SIZE / 1000;
    int chunk;
    int i;
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), TEST_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    for (chunk = 0; chunk < number_of_chunks; chunk++) {
        /*
         * Put some data into the buffer
         */
        for (i = 0; i < 1000; i++)
            buffer[i] = 'y';
        /*
         * Write 1000 bytes
         */
        ret = inode->iops->inode_write(inode, 1000, chunk*1000, buffer);
        ASSERT(ret==1000);
        /*
         * Read back content and compare
         */
        ret = inode->iops->inode_read(inode, 1000, chunk*1000, read_buffer);
        ASSERT(ret==1000);
        for (i = 0; i < 1000; i++)
            ASSERT(read_buffer[i]=='y');
    }
    return 0;
}

/*
 * Testcase 57
 * Tested function: fs_ext2_inode_write
 * Testcase: extend a file by 10.000 blocks, so that we can test the case that we allocate blocks within another block
 * group than the preferred one
 * NOTE: we reuse the file from testcase 55 here!
 */
int testcase57() {
    u8 buffer[1024];
    u8 read_buffer[1024];
    u32 number_of_chunks = 10000;
    int chunk;
    int i;
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), SAMPLE_D_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    for (chunk = 0; chunk < number_of_chunks; chunk++) {
        /*
         * Put some data into the buffer
         */
        for (i = 0; i < 1024; i++)
            buffer[i] = 'y';
        /*
         * Write 1024 bytes
         */
        ret = inode->iops->inode_write(inode, 1024, chunk*1024, buffer);
        ASSERT(ret==1024);
        /*
         * Read back content and compare
         */
        ret = inode->iops->inode_read(inode, 1024, chunk*1024, read_buffer);
        ASSERT(ret==1024);
        for (i = 0; i < 1024; i++)
            ASSERT(read_buffer[i]=='y');
    }
    return 0;
}

/*
 * Testcase 58
 * Tested function: fs_ext2_inode_write
 * Testcase: given a file of total length 10, simulate the case that we try to write 1024 additional
 * bytes but the device is full
 * NOTE: this testcase resets the test image
 */
int testcase58() {
    u8 buffer[1024];
    u8 read_buffer[1024];
    int fd;
    int i;
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    inode_t* next_inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ext2_metadata_t* ext2_meta = (ext2_metadata_t*) super->data;
    ASSERT(ext2_meta);
    ext2_superblock_t* ext2_super = ext2_meta->ext2_super;
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), SAMPLE_D_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    ASSERT(inode->size == 10);
    /*
     * Put some data into the buffer
     */
    for (i = 0; i < 1024; i++)
        buffer[i] = 'x';
    /*
     * Simulate case that no block is free
     */
    ext2_super->s_free_blocks_count = 0;
    /*
     * Try to write. We expect that the first 1014 bytes are written as
     * this does not require a new block
     */
    ret = inode->iops->inode_write(inode, 1024, 10, buffer);
    ASSERT(ret==1014);
    ASSERT(inode->size == 1024);
    /*
     * Read back content and compare
     */
    ret = inode->iops->inode_read(inode, 1014, 10, read_buffer);
    ASSERT(ret==1014);
    for (i = 0; i < 1014; i++)
        ASSERT(read_buffer[i]=='x');
    return 0;
}

/*
 * Testcase 59
 * Tested function: fs_ext2_inode_write
 * Testcase: given a file of total length 1024, simulate the case that we try to write 10 additional
 * bytes but the device is full
 * NOTE: this testcase reuses the file from testcase 58
 */
int testcase59() {
    u8 buffer[1024];
    u8 read_buffer[1024];
    int fd;
    int i;
    int backup;
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    inode_t* next_inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ext2_metadata_t* ext2_meta = (ext2_metadata_t*) super->data;
    ASSERT(ext2_meta);
    ext2_superblock_t* ext2_super = ext2_meta->ext2_super;
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), SAMPLE_D_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    ASSERT(inode->size == 1024);
    /*
     * Put some data into the buffer
     */
    for (i = 0; i < 1024; i++)
        buffer[i] = 'x';
    /*
     * Simulate case that no block is free
     */
    backup = ext2_super->s_free_blocks_count;
    ext2_super->s_free_blocks_count = 0;
    /*
     * Try to write. We expect that an error is raised
     */
    ret = inode->iops->inode_write(inode, 1024, 1024, buffer);
    ext2_super->s_free_blocks_count = backup;
    ASSERT(ret==-117);
    ASSERT(inode->size == 1024);
    return 0;
}

/*
 * Testcase 60
 * Tested function: fs_ext2_inode_write
 * Testcase: given a file of total length 1024, simulate the case that we try to write an area which
 * overlaps with the indirect region so that an indirect block needs to be allocated, but the device is full
 * NOTE: this testcase reuses the file from testcase 59
 */
int testcase60() {
    u8 buffer[1024];
    u8 read_buffer[1024];
    int fd;
    int i;
    int backup;
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    inode_t* next_inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ext2_metadata_t* ext2_meta = (ext2_metadata_t*) super->data;
    ASSERT(ext2_meta);
    ext2_superblock_t* ext2_super = ext2_meta->ext2_super;
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), SAMPLE_D_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    ASSERT(inode->size == 1024);
    /*
     * Put some data into the buffer
     */
    for (i = 0; i < 1024; i++)
        buffer[i] = 'x';
    /*
     * Simulate case that no block is free
     */
    backup = ext2_super->s_free_blocks_count;
    ext2_super->s_free_blocks_count = 0;
    /*
     * Try to write into first indirect block. We expect that an error is raised
     */
    ret = inode->iops->inode_write(inode, 1024, 1024*12, buffer);
    ext2_super->s_free_blocks_count = backup;
    ASSERT(ret==-117);
    ASSERT(inode->size == 1024);
    return 0;
}

/*
 * Testcase 61
 * Tested function: fs_ext2_inode_write
 * Testcase: given a file of total length 1024, simulate the case that we first write into the first block
 * of the indirect area so that an indirect block is allocated, but the second write to the indirect area
 * fails because the disk is full
 * NOTE: this testcase reuses the file from testcase 60
 */
int testcase61() {
    u8 buffer[1024];
    u8 read_buffer[1024];
    int fd;
    int i;
    int backup;
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    inode_t* next_inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ext2_metadata_t* ext2_meta = (ext2_metadata_t*) super->data;
    ASSERT(ext2_meta);
    ext2_superblock_t* ext2_super = ext2_meta->ext2_super;
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), SAMPLE_D_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    ASSERT(inode->size == 1024);
    /*
     * Put some data into the buffer
     */
    for (i = 0; i < 1024; i++)
        buffer[i] = 'x';
    /*
     * First write the first block in the indirect area
     */
    ret = inode->iops->inode_write(inode, 1024, 1024*12, buffer);
    ASSERT(ret==1024);
    ASSERT(inode->size == 1024*13);
    /*
     * Simulate case that no block is free
     */
    backup = ext2_super->s_free_blocks_count;
    ext2_super->s_free_blocks_count = 0;
    /*
     * Try to write into second indirect block. We expect that an error is raised
     */
    ret = inode->iops->inode_write(inode, 1024, 1024*13, buffer);
    ext2_super->s_free_blocks_count = backup;
    ASSERT(ret==-117);
    ASSERT(inode->size == 1024*13);
    return 0;
}

/*
 * Testcase 62
 * Tested function: fs_ext2_inode_write
 * Testcase: given a file of 13 blocks in total, simulate the case that we write into the first block
 * of the double indirect area but the device is full
 * NOTE: this testcase reuses the file from testcase 61
 */
int testcase62() {
    u8 buffer[1024];
    u8 read_buffer[1024];
    int fd;
    int i;
    int backup;
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    inode_t* next_inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ext2_metadata_t* ext2_meta = (ext2_metadata_t*) super->data;
    ASSERT(ext2_meta);
    ext2_superblock_t* ext2_super = ext2_meta->ext2_super;
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), SAMPLE_D_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    ASSERT(inode->size == 13*1024);
    /*
     * Put some data into the buffer
     */
    for (i = 0; i < 1024; i++)
        buffer[i] = 'x';
    /*
     * Simulate case that no block is free
     */
    backup = ext2_super->s_free_blocks_count;
    ext2_super->s_free_blocks_count = 0;
    /*
     * Try to write into second indirect block. We expect that an error is raised
     */
    ret = inode->iops->inode_write(inode, 1024, 1024*(13+256), buffer);
    ext2_super->s_free_blocks_count = backup;
    ASSERT(ret==-117);
    ASSERT(inode->size == 1024*13);
    return 0;
}

/*
 * Testcase 63
 * Tested function: fs_ext2_inode_write
 * Testcase: given a file of 13 blocks in total, simulate the case that we write into the first block
 * of the double indirect area successfully but the write to the second block fails because the device is full
 * NOTE: this testcase reuses the file from testcase 62
 */
int testcase63() {
    u8 buffer[1024];
    u8 read_buffer[1024];
    int fd;
    int i;
    int backup;
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    inode_t* next_inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ext2_metadata_t* ext2_meta = (ext2_metadata_t*) super->data;
    ASSERT(ext2_meta);
    ext2_superblock_t* ext2_super = ext2_meta->ext2_super;
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), SAMPLE_D_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    ASSERT(inode->size == 13*1024);
    /*
     * Put some data into the buffer
     */
    for (i = 0; i < 1024; i++)
        buffer[i] = 'x';
    /*
     * Write the first block in the double indirect area
     */
    ret = inode->iops->inode_write(inode, 1024, 1024*(12+256), buffer);
    ASSERT(ret==1024);
    ASSERT(inode->size == 1024*(13+256));
    /*
     * Simulate case that no block is free
     */
    backup = ext2_super->s_free_blocks_count;
    ext2_super->s_free_blocks_count = 0;
    /*
     * Try to write into second indirect block. We expect that an error is raised
     */
    ret = inode->iops->inode_write(inode, 1024, 1024*(13+256), buffer);
    ext2_super->s_free_blocks_count = backup;
    ASSERT(ret==-117);
    ASSERT(inode->size == 1024*(13+256));
    return 0;
}

/*
 * Testcase 64
 * Tested function: fs_ext2_inode_write
 * Testcase: given a file of 13+256 blocks in total, simulate the case that we write into the first block
 * of the triple indirect area but the device is full
 * NOTE: this testcase reuses the file from testcase 62
 */
int testcase64() {
    u8 buffer[1024];
    u8 read_buffer[1024];
    int fd;
    int i;
    int backup;
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    inode_t* next_inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ext2_metadata_t* ext2_meta = (ext2_metadata_t*) super->data;
    ASSERT(ext2_meta);
    ext2_superblock_t* ext2_super = ext2_meta->ext2_super;
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), SAMPLE_D_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    ASSERT(inode->size == (13+256)*1024);
    /*
     * Put some data into the buffer
     */
    for (i = 0; i < 1024; i++)
        buffer[i] = 'x';
    /*
     * Simulate case that no block is free
     */
    backup = ext2_super->s_free_blocks_count;
    ext2_super->s_free_blocks_count = 0;
    /*
     * Try to write into first triple indirect block. We expect that an error is raised
     */
    ret = inode->iops->inode_write(inode, 1024, 1024*(13+256+256*256), buffer);
    ext2_super->s_free_blocks_count = backup;
    ASSERT(ret==-117);
    ASSERT(inode->size == 1024*(13+256));
    return 0;
}

/*
 * Testcase 65
 * Tested function: fs_ext2_inode_write
 * Testcase: given a file of 13+256 blocks in total, simulate the case that we write into the first block
 * of the triple double indirect area successfully but the write to the second block fails because the device is full
 * NOTE: this testcase reuses the file from testcase 64
 */
int testcase65() {
    u8 buffer[1024];
    u8 read_buffer[1024];
    int fd;
    int i;
    int backup;
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    inode_t* next_inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ext2_metadata_t* ext2_meta = (ext2_metadata_t*) super->data;
    ASSERT(ext2_meta);
    ext2_superblock_t* ext2_super = ext2_meta->ext2_super;
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), SAMPLE_D_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    ASSERT(inode->size == (13+256)*1024);
    /*
     * Put some data into the buffer
     */
    for (i = 0; i < 1024; i++)
        buffer[i] = 'x';
    /*
     * Write the first block in the double indirect area
     */
    ret = inode->iops->inode_write(inode, 1024, 1024*(12+256+256*256), buffer);
    ASSERT(ret==1024);
    ASSERT(inode->size == 1024*(13+256+256*256));
    /*
     * Simulate case that no block is free
     */
    backup = ext2_super->s_free_blocks_count;
    ext2_super->s_free_blocks_count = 0;
    /*
     * Try to write into second triple indirect block. We expect that an error is raised
     */
    ret = inode->iops->inode_write(inode, 1024, 1024*(13+256+256*256), buffer);
    ext2_super->s_free_blocks_count = backup;
    ASSERT(ret==-117);
    ASSERT(inode->size == 1024*(13+256+256*256));
    return 0;
}

/*
 * Testcase 66
 * Tested function: fs_ext2_inode_write
 * Testcase: given a file which extends into the first block triple indirect area, simulate the case that we write into the second
 * of the triple double indirect area successfully but the write to the third block fails because the device is full - write covers
 * first and second block
 * NOTE: this testcase reuses the file from testcase 64
 */
int testcase66() {
    u8 buffer[1024];
    u8 read_buffer[1024];
    int fd;
    int i;
    int backup;
    superblock_t* super;
    ssize_t ret;
    inode_t* inode;
    inode_t* next_inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ext2_metadata_t* ext2_meta = (ext2_metadata_t*) super->data;
    ASSERT(ext2_meta);
    ext2_superblock_t* ext2_super = ext2_meta->ext2_super;
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), SAMPLE_D_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_read);
    ASSERT(inode->size == 1024*(13+256+256*256));
    /*
     * Put some data into the buffer
     */
    for (i = 0; i < 1024; i++)
        buffer[i] = 'x';
    /*
     * Write 10 bytes to the second block in the double indirect area
     */
    ret = inode->iops->inode_write(inode, 10, 1024*(13+256+256*256), buffer);
    ASSERT(ret==10);
    ASSERT(inode->size == 1024*(13+256+256*256)+10);
    /*
     * Now do a write which extends into the next block, but simulate the
     * case that no block is free
     */
    backup = ext2_super->s_free_blocks_count;
    ext2_super->s_free_blocks_count = 0;
    /*
     * Try to write
     */
    ret = inode->iops->inode_write(inode, 1024, 1024*(13+256+256*256)+10, buffer);
    ext2_super->s_free_blocks_count = backup;
    ASSERT(ret==1014);
    ASSERT(inode->size == 1024*(13+256+256*256)+1024);
    return 0;
}

/*
 * Testcase 67
 * Tested function: fs_ext2_create_inode
 * Testcase: try to create a file in the root directory. Verify that file exists and the size
 * of the root directory does not change
 */
int testcase67() {
    superblock_t* super;
    inode_t* ret;
    inode_t* root_inode;
    inode_t* new_inode;
    ext2_metadata_t* meta;
    ext2_inode_data_t* inode_data;
    fs_ext2_init();
    direntry_t direntry;
    int i;
    int found;
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    meta = (ext2_metadata_t*) super->data;
    ASSERT(super->get_inode);
    root_inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), EXT2_ROOT_INODE);
    ASSERT(root_inode);
    ASSERT(root_inode->iops);
    ASSERT(root_inode->iops->inode_create);
    ASSERT(1024==root_inode->size);
    ASSERT(2==meta->reference_count);
    /*
     * Now try to create a file
     */
    ret = root_inode->iops->inode_create(root_inode, "new", 0);
    ASSERT(ret);
    inode_data = (ext2_inode_data_t*) ret->data;
    ASSERT(1==inode_data->reference_count);
    ASSERT(3==meta->reference_count);
    /*
     * make sure that when we now get the inode again, we get the same thing
     */
    new_inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), ret->inode_nr);
    ASSERT(new_inode==ret);
    /*
     * Finally make sure that we find the inode in the root directory
     */
    i = 0;
    found = 0;
    while (0==fs_ext2_get_direntry(root_inode, i, &direntry)) {
        if (strncmp(direntry.name, "new", 3)==0) {
            found = 1;
            break;
        }
        i++;
    }
    ASSERT(found);
    ASSERT(1024==root_inode->size);
    ASSERT(direntry.inode_nr==ret->inode_nr);
    return 0;
}


/*
 * Testcase 68
 * Tested function: fs_ext2_create_inode
 * Testcase: Create 10 new entries with a 100 character name in the root directory so that a new
 * block needs to be allocated.
 */
int testcase68() {
    superblock_t* super;
    inode_t* ret;
    inode_t* root_inode;
    inode_t* new_inode;
    fs_ext2_init();
    direntry_t direntry;
    char filename [101];
    int i;
    int j;
    int found;
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    root_inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), EXT2_ROOT_INODE);
    ASSERT(root_inode);
    ASSERT(root_inode->iops);
    ASSERT(root_inode->iops->inode_create);
    ASSERT(root_inode->size==1024);
    memset((void*) filename, 'x', 100);
    filename[100]=0;
    for (i=0;i<10;i++) {
        /*
         * Determine name to use
         */
        filename[99]='0'+i;
        /*
         * Now try to create a file
         */
        ret = root_inode->iops->inode_create(root_inode, filename, 0);
        ASSERT(ret);
        /*
         * Finally make sure that we find the inode in the root directory
         */
        j = 0;
        found = 0;
        while (0==fs_ext2_get_direntry(root_inode, j, &direntry)) {
            if (strncmp(direntry.name, filename, 100)==0) {
                found = 1;
                break;
            }
            j++;
        }
        ASSERT(found);
    }
    ASSERT(root_inode->size == 2048);
    return 0;
}

/*
 * Testcase 69
 * Tested function: fs_ext_unlink_inode
 * Testcase: try to remove a non-existing file
 */
int testcase69() {
    superblock_t* super;
    inode_t* root_inode;
    fs_ext2_init();
    direntry_t direntry;
    int i;
    int found;
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    root_inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), EXT2_ROOT_INODE);
    ASSERT(root_inode);
    ASSERT(root_inode->iops);
    ASSERT(root_inode->iops->inode_create);
    ASSERT(116==fs_ext2_unlink_inode(root_inode, "notthere", 0));
    return 0;
}

/*
 * Testcase 70
 * Tested function: fs_ext2_unlink_inode
 * Testcase: unlink the file created in testcase 67 and verify that its directory entry has been removed
 * and that the link count of the inode has been reduced as well
 */
int testcase70() {
    superblock_t* super;
    inode_t* root_inode;
    ext2_inode_t* ext2_inode;
    inode_t* inode;
    int old_link_count;
    fs_ext2_init();
    direntry_t direntry;
    int i;
    int found;
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    root_inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), EXT2_ROOT_INODE);
    ASSERT(root_inode);
    ASSERT(root_inode->iops);
    ASSERT(root_inode->iops->inode_create);
    /*
     * First make sure that we find the inode in the root directory
     */
    i = 0;
    found = 0;
    while (0==fs_ext2_get_direntry(root_inode, i, &direntry)) {
        if (strncmp(direntry.name, "new", 3)==0) {
            found = 1;
            inode = fs_ext2_get_inode(DEVICE(MAJOR_RAMDISK, 0), direntry.inode_nr);
            break;
        }
        i++;
    }
    ASSERT(found);
    ASSERT(inode);
    ext2_inode = ((ext2_inode_data_t*) inode->data)->ext2_inode;
    old_link_count = ext2_inode->i_link_count;
    /*
     * Now unlink inode. Note that as we still have a reference to the
     * inode, this will NOT remove the inode itself but should reduce the link count
     */
    ASSERT(0==root_inode->iops->inode_unlink(root_inode, "new", 0));
    ASSERT(ext2_inode->i_link_count==(old_link_count-1));
    /*
     * and scan directory once more to verify that entry is gone
     */
    i = 0;
    found = 0;
    while (0==fs_ext2_get_direntry(root_inode, i, &direntry)) {
        if (strncmp(direntry.name, "new", 3)==0) {
            found = 1;
            break;
        }
        i++;
    }
    /*
     * Finally drop reference
     */
    fs_ext2_inode_release(inode);
    ASSERT(0==found);
    return 0;
}

/*
 * Testcase 71
 * Tested function: fs_ext2_unlink_inode
 * Testcase: remove all entries again created in testcase 68 - this will test the case that we remove a directory entry
 * which is located at the start of a directory block
 */
int testcase71() {
    superblock_t* super;
    int ret;
    inode_t* root_inode;
    inode_t* new_inode;
    fs_ext2_init();
    direntry_t direntry;
    char filename [101];
    int i;
    int j;
    int found;
    u32 old_dir_size;
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->get_inode);
    root_inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), EXT2_ROOT_INODE);
    ASSERT(root_inode);
    ASSERT(root_inode->iops);
    ASSERT(root_inode->iops->inode_create);
    old_dir_size = root_inode->size;
    memset((void*) filename, 'x', 100);
    filename[100]=0;
    for (i=0;i<10;i++) {
        /*
         * Determine name to use
         */
        filename[99]='0'+i;
        /*
         * Now remove file
         */
        ret = root_inode->iops->inode_unlink(root_inode, filename, 0);
        ASSERT(ret==0);
        /*
         * Finally make sure that we do not find the inode in the root directory
         */
        j = 0;
        found = 0;
        while (0==fs_ext2_get_direntry(root_inode, j, &direntry)) {
            if (strncmp(direntry.name, filename, 100)==0) {
                found = 1;
                break;
            }
            j++;
        }
        if (found) {
            printf("Hm...file %s is still there...\n", filename);
        }
        ASSERT(found==0);
    }
    ASSERT(root_inode->size == old_dir_size);
    return 0;
}

/*
 * Testcase 72
 * Tested function: fs_ext2_unlink_inode
 * Testcase: remove a small file which only occupies space in the direct area and check that the count of free blocks
 * is increased again
 */
int testcase72() {
    superblock_t* super;
    ext2_superblock_t* ext2_super;
    ext2_inode_t* ext2_inode = 0;
    direntry_t direntry;
    inode_t* inode = 0;
    inode_t* root_inode = 0;
    int i;
    int found;
    fs_ext2_init();
    u32 old_free_blocks;
    u32 old_free_inodes;
    u32 used_blocks;
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->data);
    ext2_super = ((ext2_metadata_t*) super->data)->ext2_super;
    ASSERT(ext2_super);
    root_inode = super->get_inode(DEVICE(MAJOR_RAMDISK,0), EXT2_ROOT_INODE);
    ASSERT(root_inode);
    old_free_blocks = ext2_super->s_free_blocks_count;
    old_free_inodes = ext2_super->s_free_inode_count;
    /*
     * Get number of blocks used by inode
     */
    i = 0;
    found = 0;
    while (0==fs_ext2_get_direntry(root_inode, i, &direntry)) {
        if (strncmp(direntry.name, "sampleA", 7)==0) {
            found = 1;
            break;
        }
        i++;
     }
    ASSERT(1==found);
    inode = fs_ext2_get_inode(DEVICE(MAJOR_RAMDISK,0), direntry.inode_nr);
    ASSERT(inode);
    ASSERT(inode->data);
    ext2_inode =((ext2_inode_data_t*) inode->data)->ext2_inode;
    ASSERT(ext2_inode);
    used_blocks = ext2_inode->i_blocks / 2;
    fs_ext2_inode_release(inode);
    /*
     * Now remove file
     */
    ASSERT(0==fs_ext2_unlink_inode(root_inode, "sampleA", 0));
    ASSERT(ext2_super->s_free_blocks_count==old_free_blocks+used_blocks);
    ASSERT(ext2_super->s_free_inode_count==old_free_inodes+1);
    return 0;
}

/*
 * Testcase 73
 * Tested function: fs_ext2_unlink_inode
 * Testcase: remove a file which occupies space in the direct, indirect and dobule indirect area and check that the count of free blocks
 * is increased again
 */
int testcase73() {
    superblock_t* super;
    ext2_superblock_t* ext2_super;
    ext2_inode_t* ext2_inode = 0;
    direntry_t direntry;
    inode_t* inode = 0;
    inode_t* root_inode = 0;
    int i;
    int found;
    fs_ext2_init();
    u32 old_free_blocks;
    u32 old_free_inodes;
    u32 used_blocks;
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->data);
    ext2_super = ((ext2_metadata_t*) super->data)->ext2_super;
    ASSERT(ext2_super);
    root_inode = super->get_inode(DEVICE(MAJOR_RAMDISK,0), EXT2_ROOT_INODE);
    ASSERT(root_inode);
    old_free_blocks = ext2_super->s_free_blocks_count;
    old_free_inodes = ext2_super->s_free_inode_count;
    /*
     * Get number of blocks used by inode
     */
    i = 0;
    found = 0;
    while (0==fs_ext2_get_direntry(root_inode, i, &direntry)) {
        if (strncmp(direntry.name, "sampleB", 7)==0) {
            found = 1;
            break;
        }
        i++;
     }
    ASSERT(1==found);
    inode = fs_ext2_get_inode(DEVICE(MAJOR_RAMDISK,0), direntry.inode_nr);
    ASSERT(inode);
    ASSERT(inode->data);
    ext2_inode =((ext2_inode_data_t*) inode->data)->ext2_inode;
    ASSERT(ext2_inode);
    used_blocks = ext2_inode->i_blocks / 2;
    fs_ext2_inode_release(inode);
    /*  - deallocate indirect blocks as well when removing a file
     *
     * Now remove file
     */
    ASSERT(0==fs_ext2_unlink_inode(root_inode, "sampleB", 0));
    if(ext2_super->s_free_blocks_count!=old_free_blocks+used_blocks) {
        printf("Missing blocks: %d\n", used_blocks - (ext2_super->s_free_blocks_count-old_free_blocks));
        ASSERT(0);
    }
    ASSERT(ext2_super->s_free_inode_count==old_free_inodes+1);
    return 0;
}

/*
 * Testcase 74
 * Tested function: fs_ext2_unlink_inode
 * Testcase: remove a file which occupies space in all areas and check that the count of free blocks
 * is increased again
 */
int testcase74() {
    superblock_t* super;
    ext2_superblock_t* ext2_super;
    ext2_inode_t* ext2_inode = 0;
    direntry_t direntry;
    inode_t* inode = 0;
    inode_t* root_inode = 0;
    int i;
    int found;
    fs_ext2_init();
    u32 old_free_blocks;
    u32 old_free_inodes;
    u32 used_blocks;
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ASSERT(super->data);
    ext2_super = ((ext2_metadata_t*) super->data)->ext2_super;
    ASSERT(ext2_super);
    root_inode = super->get_inode(DEVICE(MAJOR_RAMDISK,0), EXT2_ROOT_INODE);
    ASSERT(root_inode);
    old_free_blocks = ext2_super->s_free_blocks_count;
    old_free_inodes = ext2_super->s_free_inode_count;

    /*
     * Get number of blocks used by inode
     */
    i = 0;
    found = 0;
    while (0==fs_ext2_get_direntry(root_inode, i, &direntry)) {
        if (strncmp(direntry.name, "sampleD", 7)==0) {
            found = 1;
            break;
        }
        i++;
     }
    ASSERT(1==found);
    inode = fs_ext2_get_inode(DEVICE(MAJOR_RAMDISK,0), direntry.inode_nr);
    ASSERT(inode);
    ASSERT(inode->data);
    ext2_inode =((ext2_inode_data_t*) inode->data)->ext2_inode;
    ASSERT(ext2_inode);
    used_blocks = ext2_inode->i_blocks / 2;
    fs_ext2_inode_release(inode);
    /*
     * Now remove file
     */
    ASSERT(0==fs_ext2_unlink_inode(root_inode, "sampleD", 0));
    if(ext2_super->s_free_blocks_count!=old_free_blocks+used_blocks) {
        printf("Missing blocks: %d\n", used_blocks - (ext2_super->s_free_blocks_count-old_free_blocks));
        ASSERT(0);
    }
    ASSERT(ext2_super->s_free_inode_count==old_free_inodes+1);
    return 0;
}

/*
 * Testcase 75: truncate an inode (sampleC). Note that this is large
 * enough so that we have to walk all blocklist components
 */
int testcase75() {
    superblock_t* super;
    inode_t* inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ext2_metadata_t* ext2_meta = (ext2_metadata_t*) super->data;
    ASSERT(ext2_meta);
    ext2_superblock_t* ext2_super = ext2_meta->ext2_super;
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), SAMPLE_C_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_trunc);
    ASSERT(0==inode->iops->inode_trunc(inode, 0));
    ASSERT(0==inode->size);
    inode->iops->inode_release(inode);
    return 0;
}

/*
 * Testcase 76
 * Tested function: fs_ext2_create_inode
 * Testcase: create a directory below the root directory
 */
int testcase76() {
    superblock_t* super;
    inode_t* ret;
    inode_t* root_inode;
    inode_t* new_inode;
    ext2_metadata_t* meta;
    ext2_inode_data_t* inode_data;
    fs_ext2_init();
    direntry_t direntry;
    int i;
    int found;
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    meta = (ext2_metadata_t*) super->data;
    ASSERT(super->get_inode);
    root_inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), EXT2_ROOT_INODE);
    ASSERT(root_inode);
    ASSERT(root_inode->iops);
    ASSERT(root_inode->iops->inode_create);
    ASSERT(2 == meta->reference_count);
    /*
     * Now try to create a directory inside the root directory
     */
    ret = root_inode->iops->inode_create(root_inode, "newdir", S_IFDIR);
    ASSERT(ret);
    inode_data = (ext2_inode_data_t*) ret->data;
    ASSERT(1 == inode_data->reference_count);
    ASSERT(3 == meta->reference_count);
    /*
     * Make sure that we find the inode in the root directory
     */
    i = 0;
    found = 0;
    while (0 == fs_ext2_get_direntry(root_inode, i, &direntry)) {
        if (strncmp(direntry.name, "newdir", 6) == 0) {
            found = 1;
            break;
        }
        i++;
    }
    ASSERT(found);
    ASSERT(direntry.inode_nr == ret->inode_nr);
    /*
     * First directory entry in new inode should be ., followed
     * by ..
     */
    memset((void*) &direntry, 0, sizeof(direntry_t));
    ASSERT(0 == fs_ext2_get_direntry(ret, 0, &direntry));
    ASSERT(0 == strcmp(".", direntry.name));
    ASSERT(ret->inode_nr == direntry.inode_nr);
    memset((void*) &direntry, 0, sizeof(direntry_t));
    ASSERT(0 == fs_ext2_get_direntry(ret, 1, &direntry));
    ASSERT(0 == strcmp("..", direntry.name));
    ASSERT(root_inode->inode_nr == direntry.inode_nr);
    ASSERT(-1 == fs_ext2_get_direntry(ret, 2, &direntry));
    return 0;
}


/*
 * Testcase 77
 * Tested function: fs_ext2_unlink inode
 * Testcase: remove the directory created in testcase 76
 */
int testcase77() {
    superblock_t* super;
    inode_t* ret;
    inode_t* root_inode;
    inode_t* new_inode;
    ext2_metadata_t* meta;
    ext2_inode_data_t* inode_data;
    fs_ext2_init();
    direntry_t direntry;
    int i;
    int found;
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    meta = (ext2_metadata_t*) super->data;
    ASSERT(super->get_inode);
    root_inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), EXT2_ROOT_INODE);
    ASSERT(root_inode);
    ASSERT(root_inode->iops);
    ASSERT(root_inode->iops->inode_create);
    ASSERT(2 == meta->reference_count);
    /*
     * Find directory entry for newdir
     */
    i = 0;
    found = -1;
    while (0 == fs_ext2_get_direntry(root_inode, i, &direntry)) {
        if (strncmp(direntry.name, "newdir", 6) == 0) {
            found = i;
            break;
        }
        i++;
    }
    ASSERT(-1 != found);
    /*
     * Unlink corresponding inode
     */
    __ext2_loglevel = 0;
    ASSERT(0 == fs_ext2_unlink_inode(root_inode, "newdir", 0));
    __ext2_loglevel = 0;

    /*
     * Make sure that entry is gone
     */
    i = 0;
    found = -1;
    while (0 == fs_ext2_get_direntry(root_inode, i, &direntry)) {
        if (strncmp(direntry.name, "newdir", 6) == 0) {
            found = i;
            break;
        }
        i++;
    }
    ASSERT(-1 == found);
    return 0;
}

/*
 * Testcase 78
 * Tested function: fs_ext2_unlink inode
 * Testcase: verify that the root directory cannot be removed
 */
int testcase78() {
    superblock_t* super;
    inode_t* ret;
    inode_t* root_inode;
    inode_t* new_inode;
    ext2_metadata_t* meta;
    ext2_inode_data_t* inode_data;
    fs_ext2_init();
    direntry_t direntry;
    int i;
    int found;
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    meta = (ext2_metadata_t*) super->data;
    ASSERT(super->get_inode);
    root_inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), EXT2_ROOT_INODE);
    ASSERT(root_inode);
    ASSERT(root_inode->iops);
    ASSERT(root_inode->iops->inode_create);
    ASSERT(2 == meta->reference_count);
    __ext2_loglevel = 0;
    ASSERT(130 == fs_ext2_unlink_inode(root_inode, ".", 0));
    __ext2_loglevel = 0;
    return 0;
}

/*
 * Testcase 79: truncate an inode (sampleC) that uses triple indirect
 * blocks to a size that needs the same number of blocks
 */
int testcase79() {
    superblock_t* super;
    inode_t* inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ext2_metadata_t* ext2_meta = (ext2_metadata_t*) super->data;
    ASSERT(ext2_meta);
    ext2_superblock_t* ext2_super = ext2_meta->ext2_super;
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), SAMPLE_C_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_trunc);
    /*
     * Determine new target size
     */
    int target_size = (inode->size / 1024) * 1024 +1 ;
    ASSERT(target_size < inode->size);
    ASSERT(0==inode->iops->inode_trunc(inode, target_size));
    ASSERT(target_size==inode->size);
    inode->iops->inode_release(inode);
    return 0;
}

/*
 * Testcase 80: truncate an inode that only uses direct blocks to a size
 * that requires one block less
 */
int testcase80() {
    char buffer[512];
    superblock_t* super;
    inode_t* inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ext2_metadata_t* ext2_meta = (ext2_metadata_t*) super->data;
    ASSERT(ext2_meta);
    ext2_superblock_t* ext2_super = ext2_meta->ext2_super;
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), SAMPLE_D_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    ASSERT(inode->iops->inode_trunc);
    /*
     * Make sure that the file occupies one block only
     */
    ASSERT(10 == inode->size);
    memset(buffer, 1, 512);
    fs_ext2_inode_write(inode, 512, 10, buffer);
    fs_ext2_inode_write(inode, 512, 10+512, buffer);
    inode->iops->inode_release(inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), SAMPLE_D_INODE);
    ASSERT(1034 == inode->size);
    int target_size = (inode->size / 1024) * 1024 - 1 ;
    ASSERT(target_size < inode->size);
    /*
     * Now truncate
     */
    ASSERT(0==inode->iops->inode_trunc(inode, target_size));
    ASSERT(target_size==inode->size);
    inode->iops->inode_release(inode);
    return 0;
}

/*
 * Testcase 81: truncate an inode that uses indirect blocks by one block - inode
 * still has indirect blocks afterwards
 */
int testcase81() {
    char buffer[1024];
    superblock_t* super;
    inode_t* inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ext2_metadata_t* ext2_meta = (ext2_metadata_t*) super->data;
    ASSERT(ext2_meta);
    ext2_superblock_t* ext2_super = ext2_meta->ext2_super;
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), SAMPLE_A_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    /*
     * Make sure that we have no indirect blocks yet
     */
    ext2_inode_t* ext2_inode =((ext2_inode_data_t*) inode->data)->ext2_inode; 
    ASSERT(0 == ext2_inode->indirect1);
    /*
     * Append bytes until we have an indirect block
     */
    memset(buffer, 1, 1024);
    while ( 0 == ext2_inode->indirect1) {
        fs_ext2_inode_write(inode, 1024, inode->size, buffer);
    }
    ASSERT(ext2_inode->indirect1);
    ASSERT(0 == ext2_inode->indirect2);
    /*
     * Do one more
     */
    fs_ext2_inode_write(inode, 1024, inode->size, buffer); 
    /*
     * and write to disk
     */
    inode->iops->inode_release(inode);
    /*
     * Now read the inode again
     */
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), SAMPLE_A_INODE);
    ext2_inode =((ext2_inode_data_t*) inode->data)->ext2_inode; 
    ASSERT(ext2_inode->indirect1);    
    /*
     * Print out blocklist
     */
    /*
     * Truncate by one block 
     */
    int target_size = (inode->size / 1024) * 1024 - 1 ;
    ASSERT(0 == fs_ext2_inode_trunc(inode, target_size));
    ASSERT(target_size == inode->size);
    /*
     * The inode should still have an indirect block now
     */
    ASSERT(ext2_inode->indirect1);     
    inode->iops->inode_release(inode);
    return 0;
}

/*
 * Testcase 82: truncate an inode that uses indirect blocks by one block - inode
 * has no indirect blocks any more afterwards
 */
int testcase82() {
    char buffer[1024];
    superblock_t* super;
    inode_t* inode;
    fs_ext2_init();
    super = fs_ext2_get_superblock(DEVICE(MAJOR_RAMDISK, 0));
    ASSERT(super);
    ext2_metadata_t* ext2_meta = (ext2_metadata_t*) super->data;
    ASSERT(ext2_meta);
    ext2_superblock_t* ext2_super = ext2_meta->ext2_super;
    ASSERT(super->get_inode);
    inode = super->get_inode(DEVICE(MAJOR_RAMDISK, 0), SAMPLE_A_INODE);
    ASSERT(inode);
    ASSERT(inode->iops);
    /*
     * We should still have one indirect block from the last testcase
     */
    ext2_inode_t* ext2_inode =((ext2_inode_data_t*) inode->data)->ext2_inode; 
    ASSERT(ext2_inode->indirect1);
    ASSERT(0 == ext2_inode->indirect2);
    /*
     * Truncate by one block 
     */
    int target_size = (inode->size / 1024) * 1024 - 1 ;
    ASSERT(0 == fs_ext2_inode_trunc(inode, target_size));
    ASSERT(target_size == inode->size);
    /*
     * The inode should not have an indirect block any more
     */
    ASSERT(0 == ext2_inode->indirect1);     
    inode->iops->inode_release(inode);
    return 0;
}


int main() {
    INIT;
    setup();
    RUN_CASE(1);
    RUN_CASE(2);
    RUN_CASE(3);
    RUN_CASE(4);
    RUN_CASE(5);
    RUN_CASE(6);
    RUN_CASE(7);
    RUN_CASE(8);
    RUN_CASE(9);
    RUN_CASE(10);
    RUN_CASE(11);
    RUN_CASE(12);
    RUN_CASE(13);
    RUN_CASE(14);
    RUN_CASE(15);
    RUN_CASE(16);
    RUN_CASE(17);
    RUN_CASE(18);
    RUN_CASE(19);
    RUN_CASE(20);
    RUN_CASE(21);
    RUN_CASE(22);
    RUN_CASE(23);
    RUN_CASE(24);
    RUN_CASE(25);
    RUN_CASE(26);
    RUN_CASE(27);
    RUN_CASE(28);
    RUN_CASE(29);
    RUN_CASE(30);
    RUN_CASE(31);
    RUN_CASE(32);
    RUN_CASE(33);
    RUN_CASE(34);
    RUN_CASE(35);
    RUN_CASE(36);
    RUN_CASE(37);
    RUN_CASE(38);
    RUN_CASE(39);
    RUN_CASE(40);
    RUN_CASE(41);
    RUN_CASE(42);
    RUN_CASE(43);
    RUN_CASE(44);
    RUN_CASE(45);
    RUN_CASE(46);
    RUN_CASE(47);
    RUN_CASE(48);
    RUN_CASE(49);
    RUN_CASE(50);
    RUN_CASE(51);
    RUN_CASE(52);
    RUN_CASE(53);
    RUN_CASE(54);
    RUN_CASE(55);
    RUN_CASE(56);
    RUN_CASE(57);
    reset();
    RUN_CASE(58);
    RUN_CASE(59);    
    RUN_CASE(60);    
    RUN_CASE(61);    
    RUN_CASE(62);    
    RUN_CASE(63);    
    RUN_CASE(64);    
    RUN_CASE(65);    
    RUN_CASE(66);    
    RUN_CASE(67);    
    RUN_CASE(68);    
    RUN_CASE(69);    
    RUN_CASE(70);    
    RUN_CASE(71);    
    RUN_CASE(72);    
    RUN_CASE(73);    
    RUN_CASE(74);
    RUN_CASE(75);
    RUN_CASE(76);
    RUN_CASE(77);
    reset();
    RUN_CASE(78);
    reset();
    RUN_CASE(79);
    RUN_CASE(80);
    RUN_CASE(81);
    RUN_CASE(82);
    /*
     * Uncomment the following line to save a copy of the changed image back to disk as rdimage.new
     * for further analysis (for instance with fsck.ext2 -f -v)
     */
    // save();
    END;
}

