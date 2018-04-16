/*
 * pagetables.c
 *
 * This module contains functions to handle
 * page tables on the x86 architecture
 * We only support 32 bit paging at the moment
 * with a page size of 4 kB
 */

#include "pagetables.h"

/*
 * Create a page table entry
 * Parameters:
 * @rw - set to one to allow writes to the page
 * @us - user/supervisor flag, if 0 page is reserved for supervisor mode
 * @pcd - set to one to disable caching of this page
 * @page_base - 32 bit base address of page, must be a multiple of page size
 * Present bit = 1
 * Accessed = 0
 * dirty = 0
 * PWT = 0
 * Return value:
 * the newly created page table entry
 */
pte_t pte_create (u8 rw, u8 us, u8 pcd, u32 page_base) {
    pte_t pte;
    pte.a = 0;
    pte.d = 0;
    pte.page_base = (page_base >> 12);
    pte.pcd = pcd;
    pte.pwt = 0;
    pte.reserved0 = 0;
    pte.rw = rw;
    pte.us = us;
    pte.p = 1;
    return pte;
}

