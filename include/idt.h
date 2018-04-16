#ifndef _IDT_H
#define _IDT_H

#include "ktypes.h"

/*
 * Pointer to an IDT structure as it is loaded by LIDT
 * This is what Intel calls a pseudo-descriptor
 */
typedef struct {
    u16 limit; // 16 Bit Limit value, i.e. top of IDT (offset)
    u32 base; // base
}__attribute__ ((packed)) idt_ptr_t;

/*
 * An interrupt descriptor table entry
 */
typedef struct {
    u16 offset_12; // first 2 bytes of offset
    u16 selector; // destination selector
    u8 reserved0; // not used, should be set to zero
    u8 trap :1; // set if entry describes a trap gate, i.e. further interrupts are allowed
    u8 fixed0 :2; // should always be set to 0x3
    u8 d :1; // default operation size for gate, 1 for 32 bit code
    u8 s :1; // type of descriptor, should be zero
    u8 dpl :2; // descriptor privilege level - who is allowed to invoke interrupt
    u8 p :1; // present flag, should be 1
    u16 offset_34; // last 2 bytes of offset
}__attribute__ ((packed)) idt_entry_t;

idt_entry_t idt_create_entry(u32 offset, u16 selector, u8 trap, u8 dpl);
u32 idt_create_table();
u32 idt_get_table();

#endif
