/*
 * pagetables.h
 */

#ifndef _PAGETABLES_H_
#define _PAGETABLES_H_

#include "ktypes.h"
/*
 * This structure describes a page table entry
 * in 32 bit paging mode
 */
typedef struct {
    u8 p : 1; // Present bit, 1 indicates that page is mapped
    u8 rw : 1; // read-write, if zero writes are not allowed
    u8 us : 1 ; // user-supervisor flag, if 0 page cannot be accessed from user space
    u8 pwt : 1; // page-level write through
    u8 pcd : 1 ; // page-level cache disable
    u8 a : 1; // accessed
    u8 d : 1; // dirty
    u8 reserved0 : 5; // reserved or ignored
    u32 page_base : 20 ; // upper 20 bits of page base address
} __attribute ((packed)) pte_t;


pte_t pte_create (u8 rw, u8 us, u8 pcd, u32 page_base);

#endif /* _PAGETABLES_H_ */
