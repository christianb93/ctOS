/*
 * test_fs_ext2.c
 */

#include "kunit.h"
#include "fs_ext2.h"
#include "drivers.h"
#include "mm.h"
#include "dm.h"
#include "lib/sys/types.h"
#include "lib/unistd.h"
#include <stdio.h>
#include "vga.h"


#define O_RDONLY         00


/*
 * Size of the test hd image
 */
#define TEST_IMAGE_SIZE 208486400
/*
 * Inode of the file /test - get this with ls -li
 */
#define TEST_INODE 13
/*
 * Copy of the file /test on the local file system
 */
#define TEST_COPY "/home/chr/Downloads/gparted-live-0.8.1-3.iso"
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

void* kmalloc_aligned(u32 size, u32 alignment) {
    return 0;
}

/*
 * Stub for read from device
 */
ssize_t my_read(minor_dev_t minor, ssize_t blocks, ssize_t first_block, void* buffer) {
    memcpy(buffer, image+first_block*1024, blocks*1024 );
    return blocks;
}

ssize_t my_write(minor_dev_t minor, ssize_t blocks, ssize_t first_block, void* buffer) {
    memcpy(image+first_block*1024, buffer, blocks*1024 );
    return blocks;
}


/*
 * Stubs for device manager functions
 */
blk_dev_ops_t dummy_ops = {0, 0, my_read, my_write};
blk_dev_ops_t* dm_get_blk_dev_ops(major_dev_t major) {
    return &dummy_ops;
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

/*
 * Set up test image in memory
 */
void setup() {
    image = (void*) malloc(TEST_IMAGE_SIZE);
    memset((void*) image, 0xee, TEST_IMAGE_SIZE);
}


/*
 * Reset test image
 */
void reset() {
    memset((void*) image, 0xee, TEST_IMAGE_SIZE);
}

/*
 * Testcase 1
 * Tested function: bc_read_bytes
 * Testcase: read less than a block with offset 0
 */
int testcase1() {
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
 * Testcase 2
 * Tested function: bc_read_bytes
 * Testcase: read less than a block with offset 1
 */
int testcase2() {
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
 * Testcase 3
 * Tested function: bc_read_bytes
 * Testcase: read exactly one block with offset 0
 */
int testcase3() {
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
 * Testcase 4
 * Tested function: bc_read_bytes
 * Testcase: number of bytes less than a block, but range
 * is across a block boundary (offset < 1024, offset+bytes > 1024)
 */
int testcase4() {
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
 * Testcase 5
 * Tested function: bc_read_bytes
 * Testcase: offset exceeds one block
 */
int testcase5() {
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
 * Testcase 6:
 * Tested function: bc_write_bytes
 * Testcase: write to an area which starts at a block boundary and is shorter than one block
 */
int testcase6() {
    int rc;
    int i;
    u8 orig_block[1024];
    u8 write_buffer[10];
    u8 read_buffer[1024];
    memset((void*) write_buffer, 0xff, 10);
    /*
     * Save original content
     */
    bc_read_bytes(0, 1024, (void*) orig_block, 0, 0);
    rc = bc_write_bytes(0, 10, write_buffer, 0, 0);
    ASSERT(rc==0);
    /*
     * Verify that first 10 bytes have been written
     */
    bc_read_bytes(0, 1024, read_buffer, 0, 0);
    for (i=0;i<10;i++)
        ASSERT(read_buffer[i]==write_buffer[i]);
    /*
     * and that all other bytes in the block are unchanged
     */
    for (i=10;i<1024;i++)
            ASSERT(read_buffer[i]==orig_block[i]);
    reset();
    return 0;
}

/*
 * Testcase 7:
 * Tested function: bc_write_bytes
 * Testcase: write to an area which starts after a block boundary and is shorter than one block
 */
int testcase7() {
    int rc;
    int i;
    u8 orig_block[1024];
    u8 write_buffer[10];
    u8 read_buffer[1024];
    memset((void*) write_buffer, 0xff, 10);
    /*
     * Save original content
     */
    bc_read_bytes(0, 1024, (void*) orig_block, 0, 0);
    rc = bc_write_bytes(0, 10, write_buffer, 0, 5);
    ASSERT(rc==0);
    /*
     * Verify that  10 bytes have been written
     */
    bc_read_bytes(0, 1024, read_buffer, 0, 0);
    for (i=5;i<15;i++)
        ASSERT(read_buffer[i]==write_buffer[i-5]);
    /*
     * and that all other bytes in the block are unchanged
     */
    for (i=15;i<1024;i++)
            ASSERT(read_buffer[i]==orig_block[i]);
    for (i=0;i<5;i++)
            ASSERT(read_buffer[i]==orig_block[i]);
    reset();
    return 0;
}


/*
 * Testcase 8:
 * Tested function: bc_write_bytes
 * Testcase: write to an area which starts after a block boundary and extends into the second block
 */
int testcase8() {
    int rc;
    int i;
    u8 orig_block[2048];
    u8 write_buffer[10];
    u8 read_buffer[2048];
    memset((void*) write_buffer, 0xff, 10);
    /*
     * Save original content
     */
    bc_read_bytes(0, 2048, (void*) orig_block, 0, 0);
    rc = bc_write_bytes(0, 10, write_buffer, 0, 1020);
    ASSERT(rc==0);
    /*
     * Verify that  10 bytes have been written
     */
    bc_read_bytes(0, 2048, read_buffer, 0, 0);
    for (i=1020;i<1030;i++)
        ASSERT(read_buffer[i]==write_buffer[i-1020]);
    /*
     * and that all other bytes in the block are unchanged
     */
    for (i=1030;i<2048;i++)
            ASSERT(read_buffer[i]==orig_block[i]);
    for (i=0;i<1020;i++)
            ASSERT(read_buffer[i]==orig_block[i]);
    reset();
    return 0;
}

/*
 * Testcase 9:
 * Tested function: bc_write_bytes
 * Testcase: write to an area which starts at a block boundary and extends into the second block
 */
int testcase9() {
    int rc;
    int i;
    u8 orig_block[2048];
    u8 write_buffer[1030];
    u8 read_buffer[2048];
    memset((void*) write_buffer, 0xff, 10);
    /*
     * Save original content
     */
    bc_read_bytes(0, 2048, (void*) orig_block, 0, 0);
    rc = bc_write_bytes(0, 1030, write_buffer, 0, 0);
    ASSERT(rc==0);
    /*
     * Verify that  1030 bytes have been written
     */
    bc_read_bytes(0, 2048, read_buffer, 0, 0);
    for (i=0;i<1030;i++)
        ASSERT(read_buffer[i]==write_buffer[i]);
    /*
     * and that all other bytes in the block are unchanged
     */
    for (i=1030;i<2048;i++)
            ASSERT(read_buffer[i]==orig_block[i]);
    reset();
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
    END;
}

