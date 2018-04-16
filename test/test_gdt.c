/*
 * test_gdt.c
 *
 */

#include <stdio.h>
#include "gdt.h"
#include "kunit.h"

/*
 * Verify correct size of GDT structure
 */
static int testcase1() {
    ASSERT(8==sizeof(gdt_entry_t));
    return 0;
}
/*
 * Do the following checks
 * Accessed bit zero
 * Descriptor type S 1
 * Segment present P 1
 * Long mode L 0
 * Default operation size 1
 * Granularity G 1
 * Conforming = 0 for code segments
 */
static int do_common_checks(gdt_entry_t gdt) {
    u32 dword0 = *((u32*) &gdt);
    u32 dword1 = *(((u32*) &gdt) + 1);
    int accessed = (dword1 >> 8) & 0x1;
    int s = (dword1 >> 12) & 0x1;
    int p = (dword1 >> 15) & 0x1;
    int l = (dword1 >> 21) & 0x1;
    int opsize = (dword1 >> 22) & 0x1;
    int g = (dword1 >> 23) & 0x1;
    int code = (dword1 >> 11) & 0x1;
    int conforming = (dword1 >> 10) & 0x1;
    if (1 == code)
        ASSERT(0==conforming);
    ASSERT(accessed==0);
    ASSERT(s==1);
    ASSERT(p==1);
    ASSERT(l==0);
    ASSERT(opsize==1);
    ASSERT(g==1);
    return 0;
}

/*
 * Set up a GDT entry for the kernel data segment
 * and verify:
 * common fields (see do_common_checks())
 * DPL = 0
 * Code/Data = 0
 * Write = 1
 */
static int testcase2() {
    gdt_entry_t gdt;
    gdt = gdt_create_entry(0x12345678, 0xfffff, 0, 0, 0, 0, 1);
    u32 dword0 = *((u32*) &gdt);
    u32 dword1 = *(((u32*) &gdt) + 1);
    int dpl = (dword1 >> 13) & 0x3;
    int write = (dword1 >> 9) & 0x1;
    int code = (dword1 >> 11) & 0x1;
    int expansion = (dword1 >> 10) & 0x1;
    u32 base = (dword0 >> 16) + ((dword1 & 0xff) << 16) + (dword1 & 0xff000000);
    u32 limit = (dword0 & 0x0000ffff) + (((dword1 >> 16) & 0xf) << 16);
    ASSERT(0==code);
    ASSERT(0==expansion);
    ASSERT(0==do_common_checks(gdt));
    ASSERT(0==dpl);
    ASSERT(1==write);
    ASSERT(0x12345678==base);
    ASSERT(0xfffff==limit);
    return 0;
}
/*
 * Set up a GDT entry for the kernel code segment
 * and verify:
 * common fields (see do_common_checks())
 * DPL = 0
 * Code/Data = 1
 * Read = 1
 */
static int testcase3() {
    gdt_entry_t gdt;
    gdt = gdt_create_entry(0, 0xfffff, 0, 1, 0, 1, 0);
    u32 dword0 = *((u32*) &gdt);
    u32 dword1 = *(((u32*) &gdt) + 1);
    int dpl = (dword1 >> 13) & 0x3;
    int read = (dword1 >> 9) & 0x1;
    int code = (dword1 >> 11) & 0x1;
    int expansion = (dword1 >> 10) & 0x1;
    ASSERT(0==expansion);
    ASSERT(0==do_common_checks(gdt));
    ASSERT(0==dpl);
    ASSERT(1==code);
    ASSERT(1==read);
    return 0;
}

/*
 * Set up a GDT entry for the kernel stack segment
 * and verify:
 * common fields (see do_common_checks())
 * DPL = 0
 * Code/Data = 0
 * Write = 1
 * expansion = 1
 */
static int testcase4() {
    gdt_entry_t gdt;
    gdt = gdt_create_entry(0, 0x0, 0, 0, 1, 0, 1);
    u32 dword0 = *((u32*) &gdt);
    u32 dword1 = *(((u32*) &gdt) + 1);
    int dpl = (dword1 >> 13) & 0x3;
    int write = (dword1 >> 9) & 0x1;
    int code = (dword1 >> 11) & 0x1;
    int expansion = (dword1 >> 10) & 0x1;
    ASSERT(1==expansion);
    ASSERT(0==do_common_checks(gdt));
    ASSERT(0==dpl);
    ASSERT(0==code);
    ASSERT(1==write);
    return 0;
}

/*
 * Set up a GDT entry for the user code segment
 * and verify:
 * common fields (see do_common_checks())
 * DPL = 3
 * Code/Data = 1
 * Read = 1
 */
static int testcase5() {
    gdt_entry_t gdt;
    gdt = gdt_create_entry(0, 0xfffff, 3, 1, 0, 1, 0);
    u32 dword0 = *((u32*) &gdt);
    u32 dword1 = *(((u32*) &gdt) + 1);
    int dpl = (dword1 >> 13) & 0x3;
    int read = (dword1 >> 9) & 0x1;
    int code = (dword1 >> 11) & 0x1;
    int expansion = (dword1 >> 10) & 0x1;
    ASSERT(0==expansion);
    ASSERT(0==do_common_checks(gdt));
    ASSERT(3==dpl);
    ASSERT(1==code);
    ASSERT(1==read);
    return 0;
}

/*
 * Tested function: gdt_create_tss
 * Testcase: create TSS entry and validate
 * that bits are correctly set and limit is
 * big enough (i.e. G bit is set to 1)
 */
static int testcase6() {
    gdt_entry_t gdt;
    gdt = gdt_create_tss(0x1000);
    u32 dword0 = *((u32*) &gdt);
    u32 dword1 = *(((u32*) &gdt) + 1);
    printf("Access byte: %x\n", (dword1 >> 8) & 0xff);
    ASSERT(gdt.base_12 == 0x1000);
    ASSERT(gdt.base_3 == 0);
    ASSERT(gdt.base_4 == 0);
    ASSERT(((dword1 >> 8) & 0x1) == 1);
    ASSERT(((dword1 >> 9) & 0x1) == 0);
    ASSERT(((dword1 >> 10) & 0x1) == 0);
    ASSERT(((dword1 >> 11) & 0x1) == 1);
    ASSERT(((dword1 >> 12) & 0x1) == 0);
    ASSERT(gdt.dpl == 0);
    ASSERT(gdt.p==1);
    ASSERT(((dword1 >> 21) & 0x1) == 0);
    ASSERT(((dword1 >> 22) & 0x1) == 0);
    ASSERT(gdt.g==1);
    return 0;
}


int main() {
    INIT;
    RUN_CASE(1);
    RUN_CASE(2);
    RUN_CASE(3);
    RUN_CASE(4);
    RUN_CASE(5);
    RUN_CASE(6);
    END;
}
