/*
 * test_pagetables.c
 */
#include "pagetables.h"
#include "kunit.h"
#include <stdio.h>

/*
 * Validate common fields
 * Present = 1
 * accessed = 0
 * dirty = 0
 * pwt = 0
 */
int check_common_fields(pte_t pte) {
    u32 word = *((u32*) &pte);
    u8 present = (word & 0x1);
    u8 dirty = (word >> 6) & 0x1;
    u8 pwt = (word >> 3) & 0x1;
    u8 accessed = (word >> 5) & 0x1;
    ASSERT(1==present);
    ASSERT(0==dirty);
    ASSERT(0==accessed);
    ASSERT(0==pwt);
    return 0;
}

/*
 * Testcase 1: check rw flag
 */
int testcase1() {
    /* First set up entry with rw = 1 */
    pte_t pte = pte_create(1, 0, 0, 0x100);
    u32 pte_word = *((u32*) &pte);
    u8 rw = (pte_word >> 1) & 0x1;
    ASSERT(0==check_common_fields(pte));
    ASSERT(1==rw);
    /* Now set up entry with rw = 0
     * and all other fields being the same
     */
    pte = pte_create(0, 0, 0, 0x100);
    pte_word = *((u32*) &pte);
    rw = (pte_word >> 1) & 0x1;
    ASSERT(0==rw);
    return 0;
}

/*
 * Testcase 2: check us flag
 */
int testcase2() {
    /* First set up entry with rw = 1 */
    pte_t pte = pte_create(1, 1, 0, 0x100);
    u32 pte_word = *((u32*) &pte);
    u8 us = (pte_word >> 2) & 0x1;
    ASSERT(0==check_common_fields(pte));
    ASSERT(1==us);
    /* Now set up entry with us = 0
     * and all other fields being the same
     */
    pte = pte_create(1, 0, 0, 0x100);
    pte_word = *((u32*) &pte);
    us = (pte_word >> 2) & 0x1;
    ASSERT(0==us);
    return 0;
}

/*
 * Testcase 3: check pcd flag
 */
int testcase3() {
    /* First set up entry with pcd = 1 */
    pte_t pte = pte_create(0, 0, 1, 0x100);
    u32 pte_word = *((u32*) &pte);
    u8 pcd = (pte_word >> 4) & 0x1;
    ASSERT(0==check_common_fields(pte));
    ASSERT(1==pcd);
    /* Now set up entry with us = 0
     * and all other fields being the same
     */
    pte = pte_create(0, 0, 0, 0x100);
    pte_word = *((u32*) &pte);
    pcd = (pte_word >> 4) & 0x1;
    ASSERT(0==pcd);
    return 0;
}

/*
 * Testcase 4: check page base address
 */
int testcase4() {
    pte_t pte = pte_create(0, 0, 1, 0x10000);
    u32 pte_word = *((u32*) &pte);
    u32 page_base = (pte_word >> 12);
    ASSERT(0==check_common_fields(pte));
    ASSERT(0x10==page_base);
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
