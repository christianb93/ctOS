#ifndef _GDT_H
#define _GDT_H

#include "ktypes.h"
#include "gdt_const.h"

/*
 * Number of GDT entries. There are eight global GDT entries
 * plus one for each CPU - please have a look at the comments
 * in gdt_const.h
 */
#define NR_GDT_ENTRIES (8 + 2*SMP_MAX_CPU)


/*
 * This structure describes an entry in
 * the GDT
 * We need the attribute packed here
 * to make sure that GCC does not
 * do padding
 */
typedef struct {
    u16 limit_12;                              // first 2 bytes of limit
    u16 base_12;                               // first 2 bytes of base
    u8 base_3;                                 // next byte of base
    u8 accessed :1;                            // accessed flag - set by CPU
    u8 rw :1;                                  // read enable for code segment, write enable for data segment
    u8 expansion :1;                           // expansion flag for data, conforming for code
    u8 cd :1;                                  // 1 = code segment, 0 = data segment
    u8 s :1;                                   // descriptor type - 0 for system descriptor, 1 for code/data
    u8 dpl :2;                                 // descriptor privilege level
    u8 p :1;                                   //segment present
    u8 limit_3 :4;                             // last 4 bits of 20 bit limit field
    u8 avl :1;                                 // available for OS
    u8 l :1;                                   // long mode flag
    u8 d :1;                                   // default operation size
    u8 g :1;                                   // granularity
    u8 base_4;                                 // last byte of base;
}__attribute__ ((packed)) gdt_entry_t;

/*
 * This is a 48-bit pointer to the GDT
 * as it is expected in the GDTR register
 */
typedef struct {
    u16 limit;                                 // 16 Bit Limit value, i.e. top of GDT (offset)
    u32 base;                                  // base
}__attribute__ ((packed)) gdt_ptr_t;





gdt_entry_t gdt_create_entry(u32 base, u32 limit, u8 dpl, u8 code, u8 expansion, u8 read, u8 write);
gdt_entry_t gdt_create_tss(u32 tss_address);
void gdt_update_tss(u32 esp0, int cpuid);
u32 gdt_get_table();
gdt_ptr_t* gdt_get_ptr();

#endif
