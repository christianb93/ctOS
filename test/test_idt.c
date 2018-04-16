/*
 * test_idt.c
 */
#include <stdio.h>
#include "idt.h"
#include "kunit.h"

/*
 * Dummy for irq_handle_interrupt
 */
void irq_handle_interrupt() {

}

void irq_post() {

}

char stack[256];
/*
 * Dummy used as gate
 */
int gate() {
    return 0;
}

/*
 * Verify correct size of IDT structure
 */
static int testcase1() {
    ASSERT(8==sizeof(idt_entry_t));
    return 0;
}

/*
 * Verify common fields in second dword
 * Bits 5-7 should be zero
 * Bits 9 to 11 should be 1
 * Bit 12 should be zero
 * Bit 15 (present) should be 1
 */
int do_common_checks(idt_entry_t entry) {
    u32 dword1 = *(((u32*) &entry) + 1);
    ASSERT( 0 == ((dword1>>5) & 0x7) );
    ASSERT( 0x7 == ((dword1>>9) & 0x7) );
    ASSERT( 0 == ((dword1>>12) & 0x1) );
    ASSERT( 1 == ((dword1>>15) & 0x1) );
    return 0;
}

/*
 * Verify IDT entry for interrupt gate
 * into kernel code segment
 */
int testcase2() {
    idt_entry_t entry;
    u32 offset;
    u16 selector;
    entry = idt_create_entry((u32) gate, 8,0, 0);
    u32 dword1 = *(((u32*) &entry) + 1);
    u32 dword0 = *(((u32*) &entry));
    ASSERT(0==do_common_checks(entry));
    selector = dword0 >> 16;
    ASSERT(8==selector);
    offset = (dword0 & 0xffff) + (dword1 & 0xffff0000);
    ASSERT (((u32) gate) == offset);
    u8 dpl = (dword1 >> 13) & 0x3;
    ASSERT(0==dpl);
    u8 trap = (dword1 >> 8) & 0x1;
    ASSERT(0==trap);
    return 0;
}

/*
 * Verify IDT entry for interrupt gate
 * into user code segment
 */
int testcase3() {
    idt_entry_t entry;
    u32 offset;
    u16 selector;
    entry = idt_create_entry((u32) gate, 8,0, 3);
    u32 dword1 = *(((u32*) &entry) + 1);
    u32 dword0 = *(((u32*) &entry));
    ASSERT(0==do_common_checks(entry));
    selector = dword0 >> 16;
    ASSERT(8==selector);
    offset = (dword0 & 0xffff) + (dword1 & 0xffff0000);
    ASSERT (((u32) gate) == offset);
    u8 dpl = (dword1 >> 13) & 0x3;
    ASSERT(0x3==dpl);
    u8 trap = (dword1 >> 8) & 0x1;
    ASSERT(0==trap);
    return 0;
}

/*
 * Verify IDT entry for trap gate
 * into kernel code segment
 */
int testcase4() {
    idt_entry_t entry;
    u32 offset;
    u16 selector;
    entry = idt_create_entry((u32) gate, 8,1, 0);
    u32 dword1 = *(((u32*) &entry) + 1);
    u32 dword0 = *(((u32*) &entry));
    ASSERT(0==do_common_checks(entry));
    selector = dword0 >> 16;
    ASSERT(8==selector);
    offset = (dword0 & 0xffff) + (dword1 & 0xffff0000);
    ASSERT (((u32) gate) == offset);
    u8 dpl = (dword1 >> 13) & 0x3;
    ASSERT(0==dpl);
    u8 trap = (dword1 >> 8) & 0x1;
    ASSERT(1==trap);
    return 0;
}

int main() {
    INIT;
    RUN_CASE(1);
    RUN_CASE(2);
    RUN_CASE(3);
    RUN_CASE(4);
    END;
}
