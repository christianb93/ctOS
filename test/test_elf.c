/*
 * test_elf.c
 * This test uses a test file TEST_FILE and uses same data from this file.
 * Complete the following steps to create the test file, then
 * fill in the constants below from the output of readelf -l
 * Program:
 * static int x=0;
 * int main() {
 *   while(1) {};
 * }
 * Commands to compile and link:
 * gcc -c -o test.o -fno-builtin -nostdinc test.c
 * ld -Ttext 0x40000000 -o test test.o
 */

#define TEST_FILE "./elf_test"
/*
 * Number of program headers
 */
#define NR_PGM_HDRS 3
/*
 * Entry point
 */
#define ENTRY_POINT 0x40000000
/*
 * File size, aligned to a multiple of a page
 */
#define TEST_FILE_SIZE 8192
/*
 * Virtual address of static variable - use readelf -s
 */
#define STATIC_VAR_ADDRESS 0x40001008
/*
 * First dword at entry point
 */
#define FIRST_CODE_DWORD 0xebe58955

#include "elf.h"
#include "vga.h"
#include "kunit.h"
#include <fcntl.h>

/*
 * Stubs for file system functions
 */
ssize_t do_lseek(int fd, off_t offset, int whence) {
    return lseek(fd, offset, whence);
}
ssize_t do_read(int fd, void* buffer, ssize_t bytes) {
    return read(fd, buffer, bytes);
}
int do_open(char* path, int flags) {
    return open(path, flags);
}

/*
 * Stub for kputchar
 */
void win_putchar(win_t* win, u8 c) {
    printf("%c", c);
}

/*
 * Stubs for kmalloc and kfree
 */
void* kmalloc(u32 size) {
    return (void*) malloc(size);
}
void kfree(void* addr) {
    free(addr);
}
/*
 * Stub for memory manager functions
 */
static u32 mem_base;
u32 mm_map_user_segment(u32 segment_base, u32 segment_top) {
    return mem_base + (segment_base-ENTRY_POINT);
}

/*
 * Testcase 1
 * Tested function: elf_get_metadata
 */
int testcase1() {
    int fd;
    elf_metadata_t meta;
    fd = open(TEST_FILE, O_RDONLY);
    elf_get_metadata(fd, &meta);
    ASSERT(meta.program_header_count==NR_PGM_HDRS);
    ASSERT(meta.file_header->e_entry==ENTRY_POINT);
    elf_free_metadata(&meta);
    close(fd);
    return 0;
}

/*
 * Testcase 2
 * Tested function: elf_get_program_header
 */
int testcase2() {
    int fd;
    int count = 0;
    elf_metadata_t meta;
    fd = open(TEST_FILE, O_RDONLY);
    elf_get_metadata(fd, &meta);
    /*
     * Check number of program headers
     */
    while (elf_get_program_header(&meta, count)) {
        count++;
    }
    ASSERT(count==NR_PGM_HDRS);
    elf_free_metadata(&meta);
    close(fd);
    return 0;
}

/*
 * Testcase 3
 * Tested function: elf_load_executable
 */
int testcase3() {
    int rc;
    int i;
    u32 entry_point;
    char __attribute__ ((aligned (4096))) mem[TEST_FILE_SIZE];
    /*
     * Fill with 0xff
     */
    for (i=0;i<TEST_FILE_SIZE;i++)
        mem[i]=0xff;
    mem_base = (u32) mem;
    /*
     * Do test - call in validation mode first
     */
    rc = elf_load_executable(TEST_FILE, &entry_point, 1);
    ASSERT(0==rc);
    rc = elf_load_executable(TEST_FILE, &entry_point, 0);
    ASSERT(0==rc);
    ASSERT(entry_point==ENTRY_POINT);
    ASSERT(FIRST_CODE_DWORD == *((u32*)mem));
    /*
     * Check that we have filled up with zeros
     */
    ASSERT(0==mem[STATIC_VAR_ADDRESS-ENTRY_POINT]);
    return 0;
}

int main() {
    INIT;
    RUN_CASE(1);
    RUN_CASE(2);
    RUN_CASE(3);
    END;
}

