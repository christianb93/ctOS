/*
 * test_mm.c
 * Unit tests for memory manager
 *
 * The main difficulty when setting up unit tests for the manager arises from the fact that
 * in a normal environment, the kernel will find the page tables themselves mapped at a given
 * location into the virtual address space, which is not the case in a unit test environment. To
 * deal with this, the function get_pt_address which the memory manager itself uses to access
 * a page table is stubbed here. Two different modes are possible:
 * 1) we keep track of all page tables within the address space and return the address of
 * the respective page table in the stub (set the flag pg_enabled_override to 1 for this). An example
 * for this method is the setup used for testcase 2
 * 2) when a page table is requested, we return the address stored as physical address in the
 * page table directory (pg_enabled_override=0)
 *
 * Another important setup step - at least when functions which call mm_get_ptd are tested - is to
 * set the variable test_ptd to the location of the page table directory. The easiest way to do this
 * is to use the variable cr3. Remember that one of the steps done by mm_init_page_tables is to put
 * the physical address of the page table directory into CR3 by calling put_cr3. In our stub for put_cr3,
 * we copy the passed address into the static variable cr3 so that we can create a pointer to the page
 * table directory from this address
 *
 * Finally most test cases require that the stub for mm_get_phys_page is prepared to deliver a given number
 * of physical pages and simulate an out-of-memory condition if more pages are requested. This is done by
 * the utility function setup_phys_pages
 */

#include "kunit.h"
#include "mm.h"
#include "vga.h"
#include "locks.h"
#include "lists.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int mm_map_page(pte_t* pd, u32 phys_base, u32 virtual_base, u8 rw,
        u8 us, u8 pcd, u32 pid);
extern void mm_init_page_tables();
extern int mm_validate_address_spaces();
extern void mm_init_address_spaces();
extern int mm_clone_ptd(pte_t* source_ptd, pte_t* target_ptd,
        u32 phys_target_ptd);

extern int __mm_log;

/*
 * Stubs
 */
static int do_putchar = 0;
void win_putchar(win_t* win, u8 c) {
    if (do_putchar)
        printf("%c", c);
}

void debug_main(ir_context_t* ir_context) {

}

static int panic = 0;
void trap() {
    panic = 1;
}

int params_get_int(char* name) {
    if (0 == strcmp("heap_validate", name))
        return 1;
    else
        return 0;
}

void debug_getline(void* c, int n) {

}


int multiboot_get_next_mmap_entry(memory_map_entry_t* next) {
    return 0;
}

int multiboot_locate_ramdisk(multiboot_ramdisk_info_block_t* ramdisk_info_block)  {
    return 0;
}

/*
 * Stub for pm functions
 */
int pm_get_pid() {
    return 0;
}

int do_kill(int pid, int signal) {
    return 0;
}

int pm_get_pid_for_task_id(u32 task_id) {
    return 0;
}

static int my_task_id = 0;
int pm_get_task_id() {
    return my_task_id;
}

int smp_get_cpu() {
    return 0;
}

/*
 * This is a bitmask describing usage of physical memory
 * A set bit indicates that the page is in use
 */
static u8 phys_mem[MM_PHYS_MEM_PAGES / 8];


/*
 * Stub for mm_get_phys_page. We keep a repository of
 * PHYS_MAX_PAGES pages which are returned one at a time
 * by this function
 */
#define PHYS_PAGES_MAX 512
static u32 phys_page[PHYS_PAGES_MAX];
static int mm_get_phys_page_called = 0;
u32 mm_get_phys_page_stub() {
    u32 page;
    if (mm_get_phys_page_called >= PHYS_PAGES_MAX)
        return 0;
    page = phys_page[mm_get_phys_page_called];
    mm_get_phys_page_called++;
    BITFIELD_SET_BIT(phys_mem, MM_PAGE(page));
    return page;
}

static u32 last_released_page = 0;
void mm_put_phys_page_stub(u32 page) {
    last_released_page = page;
    BITFIELD_CLEAR_BIT(phys_mem, MM_PAGE(page));
}

/*
 * Stub for mm_get_ptd. By setting the variable test_ptd,
 * the function can be made to point to a given test page
 * table directory so that all access of code in mm.c to the
 * page table directory of the current process is diverted to
 * this test ptd
 */
static pte_t* test_ptd = 0;
pte_t* mm_get_ptd_stub() {
    return test_ptd;
}
pte_t* (*mm_get_ptd_orig)();
pte_t* mm_get_ptd_for_pid_stub() {
    return test_ptd;
}


/*
 * Utility function to translate a virtual into a physical address
 * Assumes that virtual and physical addresses are identical for all
 * page table addresses
 * Sets errno if no mapping was possible
 */
static u32 virt_to_phys(pte_t* ptd, u32 virtual, int* errno) {
    u32 ptd_offset;
    u32 pt_offset;
    pte_t* pt;
    ptd_offset = virtual >> 22;
    pt_offset = (virtual >> 12) & 1023;
    *errno = 0;
    if (0 == ptd[ptd_offset].p) {
        *errno = 1;
        return 0;
    }
    /* At this point, we assume that the virtual address
     * of the page table is equal to its physical address
     * - to be ensured by test setup
     */
    pt = (pte_t*) (ptd[ptd_offset].page_base << 12);
    if (0 == pt[pt_offset].p) {
        *errno = 2;
        return 0;
    }
    return (virtual % 4096) + (pt[pt_offset].page_base << 12);
}

/*
 * Deliver end of kernel bss section
 */
u32 mm_get_bss_end_stub() {
    return 0x111200;
}

/*
 * Stub for mm_get_pt_address. This stub can operate in two modes
 * 1) if pg_enabled_override = 0 or paging is disabled, the function
 * will return the physical base address as stored in the ptd
 * 2) otherwise the function will simply return the value
 * of the static variable next_pt_address
 */
static pte_t* next_pt_address;
static int pg_enabled_override = 1;
pte_t* mm_get_pt_address_stub(pte_t* ptd, int ptd_offset, int pg_enabled) {
    if ((pg_enabled == 0) || (0 == pg_enabled_override)) {
        return (pte_t*) (ptd[ptd_offset].page_base * MM_PAGE_SIZE);
    }
    return next_pt_address;
}

/*
 * Stub for mm_attach_page
 */
u32 mm_attach_page_stub(u32 phys_page) {
    return phys_page;
}

/*
 * Stub for mm_detach_page
 */
void mm_detach_page_stub(u32 phys_page) {
}

/*
 * Stub for mm_copy_page
 */
static pte_t* root_ptd = 0;
int mm_copy_page_stub(u32 virtual_source, u32 physical_target) {
    int errno;
    if (root_ptd) {
        memcpy((void*) physical_target, (void*) virt_to_phys(root_ptd,
                virtual_source, &errno), 4096);
    }
    return 0;
}

/*
 * Stubs for locking operations. We maintain the counter cpulocks
 * to check that all locks have been released at some point
 */
int cpulocks = 0;
void spinlock_get(spinlock_t* lock, u32* flags) {
    cpulocks++;
}
void spinlock_release(spinlock_t* lock, u32* flags) {
    cpulocks--;
}
void spinlock_init(spinlock_t* lock) {
}
/*
 * Dummy for invalidation of TLB
 */
void invlpg(u32 virtual_address) {
}

/*
 * Stub for writing into CR3
 */
static u32 cr3 = 0;
void put_cr3(u32 _cr3) {
    cr3 = _cr3;
}
/*
 * Stub for access to CR0
 */
static int paging_enabled = 0;
u32 get_cr0() {
    return paging_enabled << 31;
}

/*
 * Declaration of function pointers inside mm.c
 */
extern u32 (*mm_get_phys_page)();
extern void (*mm_put_phys_page)(u32 page);
extern pte_t* (*mm_get_pt_address)(pte_t* ptd, int ptd_offset, int pg_enabled);
extern u32 (*mm_get_bss_end)();
extern u32 (*mm_attach_page)(u32);
extern void (*mm_detach_page)(u32);
extern int (*mm_copy_page)(u32, u32);
extern pte_t* (*mm_get_ptd)();
extern pte_t* (*mm_get_ptd_for_pid)(u32);

/*
 * Utility function to validate whether the physical
 * pages pointed to by two page table entries
 * have the same content
 */
int validate_page_content(pte_t* a, pte_t* b) {
    char* base_a;
    char* base_b;
    u32 ptr;
    base_a = (char*) (a->page_base * 4096);
    base_b = (char*) (b->page_base * 4096);
    for (ptr = 0; ptr < 4096; ptr++)
        if (base_a[ptr] != base_b[ptr])
            return 1;
    return 0;
}

/*
 * Utility function to validate a process
 * address space after it has been cloned
 * Parameter:
 * @source_ptd - pointer to source page table directory
 * @target_ptd - pointer to target page table directory
 * @stack_base - address of lowest page in stack of current task
 * @stack_top - address of highest page in stack of current task
 */
static int validate_address_space(pte_t* source_ptd, pte_t* target_ptd,
        u32 stack_base, u32 stack_top) {
    int i;
    u32 page;
    u32 ptd_offset;
    u32 pt_offset;
    pte_t* source_pt;
    pte_t* target_pt;
    int errno;
    /*
     * For the first MM_SHARED_PAGE_TABLES entries, verify that entries in source and target coincide
     */
    for (i = 0; i < MM_SHARED_PAGE_TABLES; i++) {
        ASSERT(source_ptd[i].p==target_ptd[i].p);
        ASSERT(source_ptd[i].page_base==target_ptd[i].page_base);
        ASSERT(source_ptd[i].pcd==target_ptd[i].pcd);
        ASSERT(source_ptd[i].pwt==target_ptd[i].pwt);
        ASSERT(source_ptd[i].rw==target_ptd[i].rw);
        ASSERT(source_ptd[i].us==target_ptd[i].us);
    }
    /* Go through all pages within user space
     * and the kernel stack area and verify that
     * - they are mapped
     * - on the level of PTD entries, the attributes are the same as in the source
     * - on the level of PT entries, the attributes are the same as in the source
     * - the physical base address of source and target are not the same
     * - the physical pages pointed to by both page table entries have identical content
     */
    for (page = MM_PT_ENTRIES * MM_PAGE_SIZE * MM_SHARED_PAGE_TABLES; page
            <=MM_VIRTUAL_TOS_USER; page += 4096) {
        ptd_offset = page >> 22;
        pt_offset = (page >> 12) & 1023;
        ASSERT(target_ptd[ptd_offset].p==source_ptd[ptd_offset].p);
        if (1 == source_ptd[ptd_offset].p) {
            ASSERT(source_ptd[ptd_offset].pcd==target_ptd[ptd_offset].pcd);
            ASSERT(source_ptd[ptd_offset].pwt==target_ptd[ptd_offset].pwt);
            ASSERT(source_ptd[ptd_offset].rw==target_ptd[ptd_offset].rw);
            ASSERT(source_ptd[ptd_offset].us==target_ptd[ptd_offset].us);
            source_pt = mm_get_pt_address_stub(source_ptd, ptd_offset, 0);
            target_pt = mm_get_pt_address_stub(target_ptd, ptd_offset, 0);
            ASSERT(source_pt[pt_offset].p==target_pt[pt_offset].p);
            if (1 == source_pt[pt_offset].p) {
                ASSERT(source_pt[pt_offset].pcd==target_pt[pt_offset].pcd);
                ASSERT(source_pt[pt_offset].rw==target_pt[pt_offset].rw);
                ASSERT(source_pt[pt_offset].pwt==target_pt[pt_offset].pwt);
                ASSERT(source_pt[pt_offset].us==target_pt[pt_offset].us);
                ASSERT(source_pt[pt_offset].page_base!=target_pt[pt_offset].page_base);
                ASSERT(validate_page_content(source_pt+pt_offset, target_pt+pt_offset)==0);
            }
        }
    }
    /* Test that
     * - a mapping for the kernel stack of the currently active task has been set up
     * - all physical pages for the kernel stack have been copied over
     * - the highest 4 MB of the virtual address space point to the page tables, i.e. the entry 1023 in the
     * PTD points to the PTD itself
     * - the page immediately below 0xffc0:0000 is mapped to the PTD
     *
     */
    for (page
            = MM_PAGE_START(MM_PAGE(MM_VIRTUAL_TOS-MM_STACK_PAGES_TASK*MM_PAGE_SIZE+1)); page
            <= MM_VIRTUAL_TOS; page += 4096) {
        ptd_offset = page >> 22;
        pt_offset = (page >> 12) & 1023;
        if ((page < stack_base) || (page > stack_top)) {
            continue;
        }
        ASSERT(target_ptd[ptd_offset].p==1);
        ASSERT(source_ptd[ptd_offset].pcd==target_ptd[ptd_offset].pcd);
        ASSERT(source_ptd[ptd_offset].pwt==target_ptd[ptd_offset].pwt);
        ASSERT(source_ptd[ptd_offset].rw==target_ptd[ptd_offset].rw);
        ASSERT(source_ptd[ptd_offset].us==target_ptd[ptd_offset].us);
        source_pt = mm_get_pt_address_stub(source_ptd, ptd_offset, 0);
        target_pt = mm_get_pt_address_stub(target_ptd, ptd_offset, 0);
        ASSERT(source_pt[pt_offset].p==1);
        ASSERT(source_pt[pt_offset].pcd==target_pt[pt_offset].pcd);
        ASSERT(source_pt[pt_offset].rw==target_pt[pt_offset].rw);
        ASSERT(source_pt[pt_offset].pwt==target_pt[pt_offset].pwt);
        ASSERT(source_pt[pt_offset].us==target_pt[pt_offset].us);
        ASSERT(source_pt[pt_offset].page_base!=target_pt[pt_offset].page_base);
    }
    ASSERT(target_ptd[1023].page_base == (((u32) target_ptd)/4096) );
    return 0;
}

/*
 * Utility function to set up the stub for physical page
 * allocation. This function will malloc sufficient pages and
 * set up the array phys_pages accordingly
 * Parameter:
 * @nr_of_pages - number of pages to reserve
 */
static u32 setup_phys_pages(int nr_of_pages) {
    u32 my_base;
    int i;
    u32 my_mem = (u32) malloc((nr_of_pages + 2) * 4096);
    if (nr_of_pages >= PHYS_PAGES_MAX) {
        printf("Header file has been changed, please correct test setup!\n");
        exit(1);
    }
    my_base = (my_mem / 4096) * 4096 + 4096;
    for (i = 0; i <= nr_of_pages; i++)
        phys_page[i] = my_base + i * 4096;
    for (i = nr_of_pages + 1; i < PHYS_PAGES_MAX; i++)
        phys_page[i] = 0;
    return my_mem;
}

/*
 * Testcase 1
 * Function: mm_map_page
 * Testcase: Map a page, starting with an empty page table directory, paging disabled
 * Expected result: a new page table is allocated and an entry is added
 */
int testcase1() {
    pte_t __attribute__ ((aligned(4096))) ptd[1024];
    pte_t* pt;
    /* This is the page we are going to deliver
     * to the function when a physical page
     * is requested
     */
    char __attribute__ ((aligned(4096))) page[4096];
    u32 virtual = 0xa1230000;
    u32 physical = 0xbedf0000;
    u32 ptd_offset;
    u32 pt_offset;
    int i;
    for (i = 0; i < sizeof(pte_t) * 1024; i++)
        ((char*) ptd)[i] = 0;
    for (i = 0; i < sizeof(pte_t) * 1024; i++)
        (page)[i] = 0;
    /* Set up stub for physical page allocation */
    mm_get_phys_page = mm_get_phys_page_stub;
    mm_attach_page = mm_attach_page_stub;
    mm_detach_page = mm_detach_page_stub;
    phys_page[0] = (u32) page;
    phys_page[1] = 0;
    /* Set up stub for read access to CR0 */
    paging_enabled = 0;
    /* And call function */
    mm_map_page(ptd, physical, virtual, 1, 1, 1, 0);
    /*
     * Now validate result
     * We expect that
     * - a new entry has been added to the page table directory at offset given by virtual
     * - the present bit of this entry is one
     * - the entry points to the test page on our stack
     * - the entry in the page table has present bit set to one
     * - the entry in the page table points to the correct physical address
     */
    ptd_offset = virtual >> 22;
    pt_offset = (virtual >> 12) & 1023;
    ASSERT(1==ptd[ptd_offset].p);
    ASSERT((u32) page==(ptd[ptd_offset].page_base<<12));
    pt = (pte_t*) page;
    ASSERT(1==pt[pt_offset].p);
    ASSERT(physical==(pt[pt_offset].page_base<<12));
    ASSERT(0==cpulocks);
    return 0;
}

/*
 * Testcase 2
 * Function: mm_map_page
 * Testcase: Map a page, starting with an empty page table directory, paging enabled
 * Expected result: a new page table is allocated and an entry is added
 */
int testcase2() {
    pte_t __attribute__ ((aligned(4096))) ptd[1024];
    pte_t* pt;
    /*
     * This is our test page table which we are going to
     * present to mm_map_page via the stubbed version of
     * mm_get_pt_address
     */
    char __attribute__ ((aligned(4096))) page[4096];
    u32 virtual = 0xa1230000;
    u32 physical = 0xbedf0000;
    u32 phys_page_table = (u32) page;
    u32 ptd_offset;
    u32 pt_offset;
    int i;
    for (i = 0; i < sizeof(pte_t) * 1024; i++)
        (page)[i] = 0;
    for (i = 0; i < sizeof(pte_t) * 1024; i++)
        ((char*) ptd)[i] = 0;
    /*
     * Set up stub for physical page allocation
     * We use an address different from the virtual address
     * as this address is not used except to generate the PTD entry
     */
    mm_get_phys_page = mm_get_phys_page_stub;
    mm_get_phys_page_called = 0;
    phys_page[0] = phys_page_table;
    phys_page[1] = 0;
    /* Set up stub for read access to CR0 */
    paging_enabled = 1;
    /* Set up stub for translation of PTD entry
     * to pointer.
     * In this setup, we know that only one page table is
     * used. We therefore divert all access to this page table
     * to the array page declared above (set pg_enabled_override=1).
     * Thus the function mm_map_page which is under test here will access
     * this memory area when adding entries to the page table and we can
     * run our verifications against this area as well.
     */
    mm_get_pt_address = mm_get_pt_address_stub;
    next_pt_address = (pte_t*) page;
    pg_enabled_override = 1;
    mm_map_page(ptd, physical, virtual, 1, 1, 1, 0);
    /*
     * Now validate result
     * We expect that
     * - a new entry has been added to the page table directory at offset given by virtual
     * - the present bit of this entry is one
     * - the entry points to the test page on our stack
     * - the entry in the page table has present bit set to one
     * - the entry in the page table points to the correct physical address
     */
    ptd_offset = virtual >> 22;
    pt_offset = (virtual >> 12) & 1023;
    ASSERT(1==ptd[ptd_offset].p);
    ASSERT(phys_page_table==(ptd[ptd_offset].page_base<<12));
    pt = (pte_t*) page;
    ASSERT(1==pt[pt_offset].p);
    ASSERT(physical==(pt[pt_offset].page_base<<12));
    ASSERT(0==cpulocks);
    return 0;
}

/*
 * Testcase 3
 * Function: mm_map_page
 * Testcase: Map a page while paging still disabled, page table entry in ptd exists
 * Expected result: a new entry is added to the page table, no new physical page is allocated
 */
int testcase3() {
    pte_t __attribute__ ((aligned(4096))) ptd[1024];
    pte_t __attribute__ ((aligned(4096))) pt[1024];
    u32 virtual = 0xa1230000;
    u32 physical = 0xbedf0000;
    u32 ptd_offset = virtual >> 22;
    u32 pt_offset = (virtual >> 12) & 1023;
    int i;
    for (i = 0; i < sizeof(pte_t) * 1024; i++)
        ((char*) ptd)[i] = 0;
    for (i = 0; i < sizeof(pte_t) * 1024; i++)
        ((char*) pt)[i] = 0;
    /*
     * Create entry in PTD so that we do not start with an empty PTD
     */
    ptd[ptd_offset] = pte_create(1, 0, 0, (u32) pt);
    /* Set up stub for physical page allocation
     * We set next_phys_page to zero as we do not expect any allocations
     * */
    mm_get_phys_page = mm_get_phys_page_stub;
    mm_get_phys_page_called = 0;
    phys_page[0] = 0;
    /* Set up stub for read access to CR0 */
    paging_enabled = 0;
    /* Do test call */
    mm_map_page(ptd, physical, virtual, 1, 1, 1, 0);
    /*
     * Now validate result
     * We expect that
     * - an entry has been added to the page table
     * - the entry in the page table has present bit set to one
     * - the entry in the page table points to the correct physical address
     * - no new physical page has been requested
     */
    ASSERT(1==pt[pt_offset].p);
    ASSERT(physical==(pt[pt_offset].page_base<<12));
    ASSERT(0==mm_get_phys_page_called);
    ASSERT(0==cpulocks);
    return 0;
}

/*
 * Testcase 4
 * Function: mm_map_page
 * Testcase: Map a page, page table entry in ptd exists, paging enabled
 * Expected result: a new entry is added to the page table, no new physical page is allocated
 */
int testcase4() {
    pte_t __attribute__ ((aligned(4096))) ptd[1024];
    pte_t __attribute__ ((aligned(4096))) pt[1024];
    u32 virtual = 0xa1230000;
    u32 physical = 0xbedf0000;
    u32 ptd_offset = virtual >> 22;
    u32 pt_offset = (virtual >> 12) & 1023;
    int i;
    for (i = 0; i < sizeof(pte_t) * 1024; i++)
        ((char*) ptd)[i] = 0;
    for (i = 0; i < sizeof(pte_t) * 1024; i++)
        ((char*) pt)[i] = 0;
    /*
     * Create entry in PTD so that we do not start with an empty PTD
     */
    ptd[ptd_offset] = pte_create(1, 0, 0, (u32) pt);
    /* Set up stub for physical page allocation
     * We set next_phys_page to zero as we do not expect any allocations
     * */
    mm_get_phys_page = mm_get_phys_page_stub;
    mm_get_phys_page_called = 0;
    phys_page[0] = 0;
    /* Set up stub for read access to CR0 */
    paging_enabled = 1;
    /* Set up stub for translation of PTD entry
     * to pointer
     */
    mm_get_pt_address = mm_get_pt_address_stub;
    next_pt_address = pt;
    pg_enabled_override = 1;
    /* Do test call */
    mm_map_page(ptd, physical, virtual, 1, 1, 1, 0);
    /*
     * Now validate result
     * We expect that
     * - an entry has been added to the page table
     * - the entry in the page table has present bit set to one
     * - the entry in the page table points to the correct physical address
     * - no additional physical page has been requested
     */
    ASSERT(1==pt[pt_offset].p);
    ASSERT(physical==(pt[pt_offset].page_base<<12));
    ASSERT(0==mm_get_phys_page_called);
    ASSERT(0==cpulocks);
    return 0;
}

/*
 * Testcase 5
 * Function: mm_init_page_tables
 * Test initialization of paging
 */
int testcase5() {
    /*
     * Set up stub for mm_get_phys_page
     * In total, we need 1 + MM_SHARED_PAGE_TABLES + MM_STACK_PAGES_TASK = 7 physical pages
     * all of them aligned to page boundaries
     */
    int errno;
    u32 my_base;
    int i;
    pte_t* ptd;
    u32 phys;
    int nr_of_pages = 1 + MM_SHARED_PAGE_TABLES + MM_STACK_PAGES_TASK;
    u32 my_mem = setup_phys_pages(nr_of_pages);
    mm_get_phys_page_called = 0;
    mm_get_phys_page = mm_get_phys_page_stub;
    /* Set up stub for read access to CR0 */
    paging_enabled = 0;
    /* Set up stub for translation. Here we use method 2 outlined in the comment
     * at the beginning of this file. With this method, the course of events is as follows
     * 1) Initially, the page table directory is empty
     * 2) when a new mapping is requested, mm_map_page will allocate a new physical page
     * via a call to mm_get_phys_page for the page table
     * 3) it will then add an entry to the page table directory which contains the physical address
     * of this page table
     * 4) when it calls mm_get_pt_address, our stub kicks in and simply returns a pointer to this
     * physical address
     * Essentially, we simulate the case that all page tables are contained in an area in memory
     * which is mapped one-to-one
     */
    mm_get_pt_address = mm_get_pt_address_stub;
    pg_enabled_override = 0;
    /*
     * Stub for end of kernel BSS section
     */
    mm_get_bss_end = mm_get_bss_end_stub;
    /* and call function */
    mm_init_page_tables();
    /*
     * Now do the following checks:
     * - exactly nr_of_pages physical pages have been allocated
     * - cr3 has been loaded
     * - the first MM_SHARED_PAGE_TABLES entries within the page table directory are
     * pointing to existing page tables
     * - the first virtual pages up to the end of the kernel BSS section are mapped one-to-one
     * - the highest 4 MB of the virtual address space point to the page tables, i.e. the entry 1023 in the
     * PTD points to the PTD itself
     * - the page immediately below 0xffc0:0000 is mapped to the PTD
     * - MM_STACK_PAGES_TASK are allocated immediately below MM_VIRTUAL_TOS
     */
    ASSERT(nr_of_pages==mm_get_phys_page_called);
    ASSERT(cr3);
    ptd = (pte_t*) cr3;
    for (i = 0; i < MM_SHARED_PAGE_TABLES; i++)
        ASSERT(1==ptd[i].p);
    /* Now check 1-1 mapping of kernel memory */
    i = 0;
    while (i * 4096 < mm_get_bss_end_stub()) {
        phys = virt_to_phys(ptd, i * 4096, &errno);
        ASSERT(phys==i*4096);
        ASSERT(0 == errno);
        i++;
    }
    /* Check mapping of PTD and page tables */
    ASSERT(1==ptd[1023].p);
    ASSERT(ptd[1023].page_base == (((u32) ptd)/4096) );
    /* Check that kernel stack is mapped somewhere */
    u32 stack_base = (MM_VIRTUAL_TOS / 4096) * 4096 - (MM_STACK_PAGES_TASK - 1)
            * 4096;
    for (i = stack_base; i < MM_VIRTUAL_TOS; i += 4096)
        ASSERT(virt_to_phys(ptd, i, &errno));
    free((void*) my_mem);
    ASSERT(0==cpulocks);
    return 0;
}

/*
 * Testcase 6
 * Tested function: mm_clone_ptd
 * Testcase: verify that the address space is correctly cloned
 */
int testcase6() {
    pte_t __attribute__ ((aligned(4096))) target_ptd[1024];
    pte_t* source_ptd;
    u32 my_base;
    int i;
    u32 stack_top_page = (MM_VIRTUAL_TOS / MM_PAGE_SIZE) * MM_PAGE_SIZE;
    u32 stack_bottom_page = stack_top_page - (MM_STACK_PAGES_TASK - 1)
            * MM_PAGE_SIZE;
    /*
     * Zero out target PTD
     */
    memset((void*) target_ptd, 0, 4096);
    int nr_of_pages = 2 * (2 + MM_SHARED_PAGE_TABLES + MM_STACK_PAGES_TASK);
    u32 my_mem = setup_phys_pages(nr_of_pages);
    mm_get_phys_page_called = 0;
    mm_get_phys_page = mm_get_phys_page_stub;
    /* Set up stub for read access to CR0 */
    paging_enabled = 1;
    /*
     * Set up stub for translation.
     */
    mm_get_pt_address = mm_get_pt_address_stub;
    pg_enabled_override = 0;
    /*
     * Stub for end of kernel BSS section
     */
    mm_get_bss_end = mm_get_bss_end_stub;
    /*
     * Stub for mm_attach_page
     */
    mm_attach_page = mm_attach_page_stub;
    mm_detach_page = mm_detach_page_stub;
    /* and call function to set up source page tables */
    mm_init_page_tables();
    mm_init_address_spaces();
    source_ptd = (pte_t*) cr3;
    /* Set up stub for mm_copy_page */
    root_ptd = source_ptd;
    mm_copy_page = mm_copy_page_stub;
    /* Now clone */
    mm_clone_ptd(source_ptd, target_ptd, (u32) target_ptd);
    ASSERT(0==validate_address_space(source_ptd, target_ptd, stack_bottom_page, stack_top_page));
    ASSERT(0==cpulocks);
    free((void*) my_mem);
    return 0;
}

/*
 * Testcase 7
 * Tested function: mm_clone
 * Testcase: clone a process with only one task
 */
int testcase7() {
    pte_t* source_ptd;
    pte_t* target_ptd;
    u32 my_base;
    int i;
    u32 stack_top_page = (MM_VIRTUAL_TOS / MM_PAGE_SIZE) * MM_PAGE_SIZE;
    u32 stack_bottom_page = stack_top_page - (MM_STACK_PAGES_TASK - 1)
            * MM_PAGE_SIZE;
    int nr_of_pages = 2 * (2 + MM_SHARED_PAGE_TABLES + MM_STACK_PAGES_TASK);
    u32 my_mem = setup_phys_pages(nr_of_pages);
    /*
     * Zero out target memory
     */
    memset((void*) my_mem, 0, nr_of_pages * 4096);
    mm_get_phys_page_called = 0;
    mm_get_phys_page = mm_get_phys_page_stub;
    /* Set up stub for read access to CR0 */
    paging_enabled = 1;
    /*
     * Set up stub for translation.
     */
    mm_get_pt_address = mm_get_pt_address_stub;
    pg_enabled_override = 0;
    /*
     * Stub for end of kernel BSS section
     */
    mm_get_bss_end = mm_get_bss_end_stub;
    /*
     * Stub for mm_attach_page
     */
    mm_attach_page = mm_attach_page_stub;
    mm_detach_page = mm_detach_page_stub;
    /* and call function to set up source page tables */
    mm_init_address_spaces();
    mm_init_page_tables();
    source_ptd = (pte_t*) cr3;
    /* Set up stub for mm_copy_page */
    root_ptd = source_ptd;
    mm_copy_page = mm_copy_page_stub;
    /*
     * Set up stub for mm_get_ptd
     */
    test_ptd = (pte_t*) cr3;
    mm_get_ptd = mm_get_ptd_stub;
    /*
     * Clone
     */
    target_ptd = (pte_t*) mm_clone(1, 1);
    ASSERT(target_ptd);
    ASSERT(0==validate_address_space(test_ptd, target_ptd, stack_bottom_page, stack_top_page));
    free((void*) my_mem);
    return 0;
}

/*
 * Testcase 8
 * Tested function: mm_reserve_task_stack
 * Testcase:  verify that the return value
 * is different from zero and mapped
 */
int testcase8() {
    int errno;
    u32 page;
    u32 tos;
    pte_t __attribute__ ((aligned(4096))) target_ptd[1024];
    int i;
    u32 my_base;
    /*
     * Zero out target PTD
     */
    memset((void*) target_ptd, 0, 4096);
    u32 my_mem = setup_phys_pages(8);
    mm_get_phys_page_called = 0;
    mm_get_phys_page = mm_get_phys_page_stub;
    /* Set up stub for read access to CR0 */
    paging_enabled = 1;
    /*
     * Set up stub for translation.
     */
    mm_get_pt_address = mm_get_pt_address_stub;
    pg_enabled_override = 0;
    /*
     * Set up stub for mm_get_ptd
     */
    test_ptd = target_ptd;
    mm_get_ptd = mm_get_ptd_stub;
    mm_init_address_spaces();
    tos = mm_reserve_task_stack(1, 0, &page);
    /*
     * Validate return values
     */
    ASSERT(MM_STACK_PAGES_TASK==page);
    ASSERT(tos);
    ASSERT(0==((tos+1) % MM_PAGE_SIZE));
    /*
     * Validate mapping
     */
    ASSERT(virt_to_phys(target_ptd, tos, &errno));
    ASSERT(virt_to_phys(target_ptd, tos-4096, &errno));
    virt_to_phys(target_ptd, tos - page * 4096, &errno);
    ASSERT(errno);
    ASSERT(0==cpulocks);
    /*
     * Validate data structures
     */
    ASSERT(0==mm_validate_address_spaces());
    free((void*) my_mem);
    return 0;
}

/*
 * Testcase 9
 * Tested function: mm_reserve_task_stack
 * Testcase:  add two stack allocators
 */
int testcase9() {
    int errno;
    u32 page;
    u32 tos1, tos2;
    pte_t __attribute__ ((aligned(4096))) target_ptd[1024];
    int i;
    u32 my_base;
    /*
     * Zero out target PTD
     */
    memset((void*) target_ptd, 0, 4096);
    /*
     * Set up memory - we have 12 pages available
     */
    u32 my_mem = setup_phys_pages(12);
    mm_get_phys_page_called = 0;
    mm_get_phys_page = mm_get_phys_page_stub;
    /*
     * Set up stub for translation.
     */
    mm_get_pt_address = mm_get_pt_address_stub;
    pg_enabled_override = 0;
    paging_enabled = 1;
    /*
     * Set up stub for mm_get_ptd
     */
    test_ptd = target_ptd;
    mm_get_ptd = mm_get_ptd_stub;
    mm_init_address_spaces();
    ASSERT(0==mm_validate_address_spaces());
    tos1 = mm_reserve_task_stack(1, 0,  &page);
    /*
     * Validate return values
     */
    ASSERT(MM_STACK_PAGES_TASK==page);
    ASSERT(tos1);
    ASSERT(0==((tos1+1) % MM_PAGE_SIZE));
    /*
     * Validate mapping
     */
    ASSERT(virt_to_phys(target_ptd, tos1, &errno));
    ASSERT(virt_to_phys(target_ptd, tos1-4096, &errno));
    virt_to_phys(target_ptd, tos1 - page * 4096, &errno);
    ASSERT(errno);
    /*
     * Get second allocator
     */
    tos2 = mm_reserve_task_stack(1, 0, &page);
    ASSERT(tos2);
    ASSERT(tos2>tos1);
    ASSERT(virt_to_phys(target_ptd, tos2, &errno));
    ASSERT(virt_to_phys(target_ptd, tos2-4096, &errno));
    ASSERT(0==cpulocks);
    /*
     * Validate data structures
     */
    ASSERT(0==mm_validate_address_spaces());
    free((void*) my_mem);
    return 0;
}

/*
 * Testcase 10
 * Tested function: mm_reserve_task_stack
 * Testcase:  add stack allocators until stack is filled up
 */
int testcase10() {
    int errno;
    u32 page;
    u32 tos;
    u32 tos_old;
    pte_t __attribute__ ((aligned(4096))) target_ptd[1024];
    int i;
    u32 my_base;
    int pages;
    int allocations;
    /*
     * Zero out target PTD
     */
    memset((void*) target_ptd, 0, 4096);
    /*
     * Set up memory
     */
    u32 my_mem = setup_phys_pages(PHYS_PAGES_MAX - 1);
    mm_get_phys_page_called = 0;
    mm_get_phys_page = mm_get_phys_page_stub;
    /*
     * Set up stub for translation.
     */
    mm_get_pt_address = mm_get_pt_address_stub;
    pg_enabled_override = 0;
    paging_enabled = 1;
    /*
     * Set up stub for mm_get_ptd
     */
    test_ptd = target_ptd;
    mm_get_ptd = mm_get_ptd_stub;
    mm_init_address_spaces();
    ASSERT(0==mm_validate_address_spaces());
    /*
     * Setup done
     * Our stack area has MM_STACK_PAGES in total. Initially, we have used the top
     * MM_STACK_PAGES_TASK for the first kernel stack. Each new allocation requires
     * MM_STACK_PAGES_TASK + MM_STACK_PAGES_GAP pages. So we can do
     * (MM_STACK_PAGES - MM_STACK_PAGES_TASK) / (MM_STACK_PAGES_TASK + MM_STACK_PAGES_GAP)
     * allocations
     */
    allocations = (MM_STACK_PAGES - MM_STACK_PAGES_TASK) / (MM_STACK_PAGES_TASK
            + MM_STACK_PAGES_GAP);
    tos = 0;
    for (i = 0; i < allocations; i++) {
        tos_old = tos;
        tos = mm_reserve_task_stack(i + 1,0,  &pages);
        ASSERT(tos);
        ASSERT(tos > tos_old);
    }
    tos = mm_reserve_task_stack(i + 1, 0, &pages);
    ASSERT(0==mm_validate_address_spaces());
    ASSERT(0==tos);
    free((void*) my_mem);
    return 0;
}

/*
 * Testcase 11
 * Tested function: mm_clone
 * Testcase: clone a process with two tasks
 * and make sure that only one task is copied
 * Active task is task 0
 */
int testcase11() {
    pte_t* source_ptd;
    pte_t* target_ptd;
    u32 my_base;
    u32 tos;
    int i;
    int errno;
    u32 pages;
    u32 stack_top_page = (MM_VIRTUAL_TOS / MM_PAGE_SIZE) * MM_PAGE_SIZE;
    u32 stack_bottom_page = stack_top_page - (MM_STACK_PAGES_TASK - 1)
            * MM_PAGE_SIZE;
    int nr_of_pages = 2 * (2 + MM_SHARED_PAGE_TABLES + MM_STACK_PAGES_TASK);
    u32 my_mem = setup_phys_pages(nr_of_pages);
    /*
     * Zero out target memory
     */
    memset((void*) my_mem, 0, nr_of_pages * 4096);
    mm_get_phys_page_called = 0;
    mm_get_phys_page = mm_get_phys_page_stub;
    /* Set up stub for read access to CR0 */
    paging_enabled = 1;
    /*
     * Set up stub for translation.
     */
    mm_get_pt_address = mm_get_pt_address_stub;
    pg_enabled_override = 0;
    paging_enabled = 1;
    /*
     * Stub for end of kernel BSS section
     */
    mm_get_bss_end = mm_get_bss_end_stub;
    /*
     * Stub for mm_attach_page
     */
    mm_attach_page = mm_attach_page_stub;
    mm_detach_page = mm_detach_page_stub;
    /* and call function to set up source page tables */
    mm_init_address_spaces();
    mm_init_page_tables();
    source_ptd = (pte_t*) cr3;
    /* Set up stub for mm_copy_page */
    root_ptd = source_ptd;
    mm_copy_page = mm_copy_page_stub;
    /*
     * Set up stub for mm_get_ptd
     */
    test_ptd = (pte_t*) cr3;
    mm_get_ptd = mm_get_ptd_stub;
    /*
     * Create an additional stack area for a new task
     */
    tos = mm_reserve_task_stack(1, 0, &pages);
    ASSERT(tos);
    ASSERT(virt_to_phys(source_ptd, tos, &errno));
    /*
     * Clone
     */
    target_ptd = (pte_t*) mm_clone(1, 2);
    ASSERT(target_ptd);
    ASSERT(0==validate_address_space(test_ptd, target_ptd, stack_bottom_page, stack_top_page));
    /*
     * Validate stack allocators
     */
    ASSERT(0==mm_validate_address_spaces());
    /*
     * Verify that stack area of second task has not been cloned
     */
    ASSERT(0==virt_to_phys(target_ptd, tos, &errno));
    /*
     * Verify that stack area of first task has been cloned
     */
    for (i = 0; i < MM_STACK_PAGES_TASK; i++)
        ASSERT(virt_to_phys(target_ptd, stack_bottom_page+i*MM_PAGE_SIZE, &errno));
    free((void*) my_mem);
    return 0;
}

/*
 * Testcase 12
 * Tested function: mm_clone
 * Testcase: clone a process with two tasks
 * and make sure that only one task is copied
 * Active task is task 1
 */
int testcase12() {
    pte_t* source_ptd;
    pte_t* target_ptd;
    u32 my_base;
    u32 tos;
    int i;
    int errno;
    u32 pages;
    u32 stack_top_page = (MM_VIRTUAL_TOS / MM_PAGE_SIZE) * MM_PAGE_SIZE;
    u32 stack_bottom_page = stack_top_page - (MM_STACK_PAGES_TASK - 1)
            * MM_PAGE_SIZE;
    int nr_of_pages = 2 * (2 + MM_SHARED_PAGE_TABLES + MM_STACK_PAGES_TASK);
    u32 my_mem = setup_phys_pages(nr_of_pages);
    /*
     * Zero out target memory
     */
    memset((void*) my_mem, 0, nr_of_pages * 4096);
    mm_get_phys_page_called = 0;
    mm_get_phys_page = mm_get_phys_page_stub;
    /* Set up stub for read access to CR0 */
    paging_enabled = 1;
    /*
     * Set up stub for translation.
     */
    mm_get_pt_address = mm_get_pt_address_stub;
    pg_enabled_override = 0;
    paging_enabled = 1;
    /*
     * Stub for end of kernel BSS section
     */
    mm_get_bss_end = mm_get_bss_end_stub;
    /*
     * Stub for mm_attach_page
     */
    mm_attach_page = mm_attach_page_stub;
    mm_detach_page = mm_detach_page_stub;
    /* and call function to set up source page tables */
    mm_init_address_spaces();
    mm_init_page_tables();
    source_ptd = (pte_t*) cr3;
    /* Set up stub for mm_copy_page */
    root_ptd = source_ptd;
    mm_copy_page = mm_copy_page_stub;
    /*
     * Set up stub for mm_get_ptd
     */
    test_ptd = (pte_t*) cr3;
    mm_get_ptd = mm_get_ptd_stub;
    /*
     * Create an additional stack area for a new task
     * this will be task 1
     */
    tos = mm_reserve_task_stack(1, 0, &pages);
    ASSERT(tos);
    ASSERT(virt_to_phys(source_ptd, tos, &errno));
    /*
     * Switch to task 1
     */
    my_task_id = 1;
    /*
     * Clone
     */
    target_ptd = (pte_t*) mm_clone(1, 2);
    ASSERT(target_ptd);
    ASSERT(0==validate_address_space(test_ptd, target_ptd,
                    tos+1-MM_PAGE_SIZE*MM_STACK_PAGES_TASK, tos-(MM_PAGE_SIZE-1)));
    /*
     * Validate stack allocators
     */
    ASSERT(0==mm_validate_address_spaces());
    /*
     * Verify that stack area of first task has not been cloned
     */
    ASSERT(0==virt_to_phys(target_ptd, stack_bottom_page, &errno));
    /*
     * Verify that stack area of first task has been cloned
     */
    for (i = 0; i < MM_STACK_PAGES_TASK; i++)
        ASSERT(virt_to_phys(target_ptd, tos+1-MM_PAGE_SIZE*MM_STACK_PAGES_TASK+i*MM_PAGE_SIZE, &errno));
    free((void*) my_mem);
    return 0;
}

/*
 * Testcase 13
 * Test memory layout constants for consistency
 */
int testcase13() {
    /*
     * Validate that the start of the page tables
     * in virtual memory plus the size of the page tables
     * is exactly the end of the virtual 32 bit address space
     */
    u32 page_tables_start = MM_VIRTUAL_PT_ENTRY(0,0);
    ASSERT((page_tables_start + MM_PT_ENTRIES*sizeof(pte_t)*MM_PT_ENTRIES)==0);
    /*
     * Validate that the top of the kernel stack plus 1 is page aligned
     */
    ASSERT(((MM_VIRTUAL_TOS+1) % MM_PAGE_SIZE)==0);
    /*
     * Validate that the top of the kernel stack plus one
     * is the bottom of the last special page
     */
    ASSERT(MM_VIRTUAL_TOS+1+MM_RESERVED_PAGES*MM_PAGE_SIZE==page_tables_start);
    /*
     * Validate that the top of the user space stack plus 1 is the bottom of
     * the kernel stack
     */
    ASSERT(MM_VIRTUAL_TOS+1-MM_PAGE_SIZE*MM_STACK_PAGES==MM_VIRTUAL_TOS_USER+1);
    /*
     * Validate that MM_MEMIO_PAGE_TABLES fits into MM_SHARED_PAGE_TABLES
     */
    ASSERT(MM_MEMIO_PAGE_TABLES < MM_SHARED_PAGE_TABLES);
    /*
     * Verify that the lowest address within user space is below the top
     * of the user stack
     */
    ASSERT(MM_COMMON_AREA_SIZE<MM_VIRTUAL_TOS_USER);
    /*
     * Make sure that stack is also above start of code section
     */
    ASSERT(MM_START_CODE<MM_VIRTUAL_TOS_USER);
    /*
     * Make sure that MM_HIGH_MEM_START is correctly set
     */
    ASSERT(MM_HIGH_MEM_START==1024*1024);
    return 0;
}

/*
 * Testcase 14
 * Function: mm_unmap_page
 * Testcase: Map a page, then unmap it again
 * Expected result: after unmapping the page, no translation takes place any more
 */
int testcase14() {
    pte_t __attribute__ ((aligned(4096))) ptd[1024];
    pte_t* pt;
    /*
     * Here we use a similar setup as in test case 2
     */
    char __attribute__ ((aligned(4096))) page[4096];
    u32 virtual = 0xa1230000;
    u32 physical = 0xbedf0000;
    u32 phys_page_table = (u32) page;
    u32 ptd_offset;
    u32 pt_offset;
    int i;
    for (i = 0; i < sizeof(pte_t) * 1024; i++)
        (page)[i] = 0;
    for (i = 0; i < sizeof(pte_t) * 1024; i++)
        ((char*) ptd)[i] = 0;
    /* Set up stub for physical page allocation
     * We use an address different from the virtual address
     * as this address is not used except to generate the PTD entry
     */
    mm_get_phys_page = mm_get_phys_page_stub;
    mm_get_phys_page_called = 0;
    phys_page[0] = phys_page_table;
    phys_page[1] = 0;
    /* Set up stub for read access to CR0 */
    paging_enabled = 1;
    /*
     * Set up stub for translation of PTD entry
     * to pointer. We divert page table access
     * to our test page
     */
    mm_get_pt_address = mm_get_pt_address_stub;
    next_pt_address = (pte_t*) page;
    pg_enabled_override = 1;
    /*
     * First map page and validate that mapping worked
     */
    mm_map_page(ptd, physical, virtual, 1, 1, 1, 0);
    ptd_offset = virtual >> 22;
    pt_offset = (virtual >> 12) & 1023;
    ASSERT(1==ptd[ptd_offset].p);
    ASSERT(phys_page_table==(ptd[ptd_offset].page_base<<12));
    pt = (pte_t*) page;
    ASSERT(1==pt[pt_offset].p);
    ASSERT(physical==(pt[pt_offset].page_base<<12));
    ASSERT(0==cpulocks);
    /*
     * Set up stub for mm_put_phys_page
     */
    mm_put_phys_page = mm_put_phys_page_stub;
    /*
     * Then remove page again
     */
    mm_unmap_page(ptd, virtual);
    /*
     * And verify that mapping has become invalid
     * and that page has been returned
     */
    ASSERT(0==pt[pt_offset].p);
    ASSERT(last_released_page==physical);
    return 0;
}

/*
 * Testcase 15
 * Tested function: mm_release_task_stack
 * Testcase:  verify that the virtual memory mapping for a
 * task stack is reverted
 */
int testcase15() {
    int errno;
    u32 page;
    u32 tos;
    pte_t __attribute__ ((aligned(4096))) target_ptd[1024];
    int i;
    u32 my_base;
    /*
     * Zero out target PTD
     */
    memset((void*) target_ptd, 0, 4096);
    u32 my_mem = setup_phys_pages(8);
    mm_get_phys_page_called = 0;
    mm_get_phys_page = mm_get_phys_page_stub;
    /* Set up stub for read access to CR0 */
    paging_enabled = 1;
    /*
     * Set up stub for translation.
     */
    mm_get_pt_address = mm_get_pt_address_stub;
    pg_enabled_override = 0;
    /*
     * Set up stub for mm_get_ptd and for mm_get_ptd_for_pid
     */
    test_ptd = target_ptd;
    mm_get_ptd = mm_get_ptd_stub;
    mm_get_ptd_for_pid = mm_get_ptd_for_pid_stub;
    mm_init_address_spaces();
    tos = mm_reserve_task_stack(1, 0, &page);
    /*
     * Validate return values
     */
    ASSERT(MM_STACK_PAGES_TASK==page);
    ASSERT(tos);
    ASSERT(0==((tos+1) % MM_PAGE_SIZE));
    /*
     * Validate mapping
     */
    ASSERT(virt_to_phys(target_ptd, tos, &errno));
    ASSERT(virt_to_phys(target_ptd, tos-4096, &errno));
    virt_to_phys(target_ptd, tos - page * 4096, &errno);
    ASSERT(errno);
    ASSERT(0==cpulocks);
    /*
     * Validate data structures
     */
    ASSERT(0==mm_validate_address_spaces());
    /*
     * Now call mm_release_task_stack and verify that mapping
     * has been removed
     */
    ASSERT(0==mm_release_task_stack(1, 0));
    errno = 0;
    virt_to_phys(target_ptd, tos, &errno);
    ASSERT(errno);
    errno = 0;
    virt_to_phys(target_ptd, tos - 4096, &errno);
    ASSERT(errno);
    ASSERT(0==mm_validate_address_spaces());
    /*
     * Now verify that we can again reserve the stack region
     */
    ASSERT(tos == mm_reserve_task_stack(1, 0, &page));
    free((void*) my_mem);
    return 0;
}

/*
 * Testcase 16
 * Tested function: mm_map_user_segment
 * Testcase: add two pages in user space
 */
int testcase16() {
    u32 my_base;
    int errno;
    int nr_of_pages = 2 * (2 + MM_SHARED_PAGE_TABLES + MM_STACK_PAGES_TASK);
    u32 my_mem = setup_phys_pages(nr_of_pages);
    /*
     * Zero out target memory
     */
    memset((void*) my_mem, 0, nr_of_pages * 4096);
    mm_get_phys_page_called = 0;
    mm_get_phys_page = mm_get_phys_page_stub;
    /*
     * Set up stub for read access to CR0
     * and page table access
     */
    paging_enabled = 1;
    pg_enabled_override = 0;
    mm_get_pt_address = mm_get_pt_address_stub;
    /*
     * Stub for end of kernel BSS section
     */
    mm_get_bss_end = mm_get_bss_end_stub;
    /*
     * Stub for mm_attach_page
     */
    mm_attach_page = mm_attach_page_stub;
    mm_detach_page = mm_detach_page_stub;
    /* and call function to set up source page tables */
    mm_init_address_spaces();
    mm_init_page_tables();
    /*
     * Remove stub for mm_get_ptd!!!
     */
    test_ptd = (pte_t*) cr3;
    mm_get_ptd = mm_get_ptd_orig;
    /*
     * Setup done. Now request two pages in user space
     */
    ASSERT(MM_START_CODE==mm_map_user_segment(MM_START_CODE, MM_START_CODE+2*MM_PAGE_SIZE-1));
    /*
     * verify that they have been mapped
     */
    ASSERT(virt_to_phys(test_ptd, MM_START_CODE, &errno));
    ASSERT(virt_to_phys(test_ptd, MM_START_CODE+MM_PAGE_SIZE, &errno));
    return 0;
}

/*
 * Testcase 17
 * Tested function: mm_init_user_area
 * Testcase: verify that pages for the stack have been added
 */
int testcase17() {
    u32 my_base;
    int errno;
    u32 page;
    int nr_of_pages = 2 * (2 + MM_SHARED_PAGE_TABLES + MM_STACK_PAGES_TASK);
    u32 my_mem = setup_phys_pages(nr_of_pages);
    /*
     * Zero out target memory
     */
    memset((void*) my_mem, 0, nr_of_pages * 4096);
    mm_get_phys_page_called = 0;
    mm_get_phys_page = mm_get_phys_page_stub;
    paging_enabled = 1;
    pg_enabled_override = 0;
    mm_get_pt_address = mm_get_pt_address_stub;
    /*
     * Stub for end of kernel BSS section
     */
    mm_get_bss_end = mm_get_bss_end_stub;
    /*
     * Stub for mm_attach_page
     */
    mm_attach_page = mm_attach_page_stub;
    mm_detach_page = mm_detach_page_stub;
    /* and call function to set up source page tables */
    mm_init_address_spaces();
    mm_init_page_tables();
    /*
     * Set up stub for mm_get_ptd
     */
    test_ptd = (pte_t*) cr3;
    mm_get_ptd = mm_get_ptd_stub;
    /*
     * Setup done. Now initialize user ara
     */
    ASSERT(MM_VIRTUAL_TOS_USER-3==mm_init_user_area());
    /*
     * verify that pages for the user space stack have been mapped
     */
    for (page = MM_VIRTUAL_TOS_USER - 4095; page > MM_VIRTUAL_TOS_USER + 1
            - MM_STACK_PAGES_TASK * MM_PAGE_SIZE; page -= 4096) {
        ASSERT(virt_to_phys(test_ptd, page, &errno));
    }
    return 0;
}

/*
 * Testcase 18
 * Tested function: mm_teardown_user_area
 * Testcase: verify that all pages within the user area have been released
 */
int testcase18() {
    u32 my_base;
    int errno;
    u32 page;
    int nr_of_pages = 2 * (2 + MM_SHARED_PAGE_TABLES + MM_STACK_PAGES_TASK);
    u32 my_mem = setup_phys_pages(nr_of_pages);
    /*
     * Zero out target memory
     */
    memset((void*) my_mem, 0, nr_of_pages * 4096);
    mm_get_phys_page_called = 0;
    mm_get_phys_page = mm_get_phys_page_stub;
    paging_enabled = 1;
    pg_enabled_override = 0;
    mm_get_pt_address = mm_get_pt_address_stub;
    /*
     * Stub for end of kernel BSS section
     */
    mm_get_bss_end = mm_get_bss_end_stub;
    /*
     * Stub for mm_attach_page
     */
    mm_attach_page = mm_attach_page_stub;
    mm_detach_page = mm_detach_page_stub;
    /* and call function to set up source page tables */
    mm_init_address_spaces();
    mm_init_page_tables();
    /*
     * Set up stub for mm_get_ptd
     */
    test_ptd = (pte_t*) cr3;
    mm_get_ptd = mm_get_ptd_stub;
    /*
     * Setup done. Now initialize user area
     */
    ASSERT(MM_VIRTUAL_TOS_USER-3==mm_init_user_area());
    /*
     * verify that pages for the user space stack have been mapped
     */
    for (page = MM_VIRTUAL_TOS_USER - 4095; page > MM_VIRTUAL_TOS_USER + 1
            - MM_STACK_PAGES_TASK * MM_PAGE_SIZE; page -= 4096) {
        ASSERT(virt_to_phys(test_ptd, page, &errno));
    }
    /*
     * Next we add two pages to the code segment and verify that the mapping
     * was successful
     */
    ASSERT(MM_START_CODE==mm_map_user_segment(MM_START_CODE, MM_START_CODE+2*MM_PAGE_SIZE-1));
    /*
     * verify that they have been mapped
     */
    ASSERT(virt_to_phys(test_ptd, MM_START_CODE, &errno));
    ASSERT(virt_to_phys(test_ptd, MM_START_CODE+MM_PAGE_SIZE, &errno));
    /*
     * Now the actual test starts. We call mm_teardown_user_area and then check
     * that all the pages above are no longer mapped
     */
    mm_teardown_user_area();
    for (page = MM_VIRTUAL_TOS_USER - 4095; page > MM_VIRTUAL_TOS_USER + 1
            - MM_STACK_PAGES_TASK * MM_PAGE_SIZE; page -= 4096) {
        ASSERT(0==virt_to_phys(test_ptd, page, &errno));
    }
    ASSERT(0==virt_to_phys(test_ptd, MM_START_CODE, &errno));
    ASSERT(0==virt_to_phys(test_ptd, MM_START_CODE+MM_PAGE_SIZE, &errno));
    return 0;
}

/*
 * Testcase 19
 * Tested function: mm_release_page_tables
 * Testcase: verify that all page tables above the common area have been
 * removed
 */
int testcase19() {
    u32 my_base;
    int errno;
    u32 page;
    int i;
    int nr_of_pages = 2 * (2 + MM_SHARED_PAGE_TABLES + MM_STACK_PAGES_TASK);
    u32 my_mem = setup_phys_pages(nr_of_pages);
    /*
     * Zero out target memory
     */
    memset((void*) my_mem, 0, nr_of_pages * 4096);
    mm_get_phys_page_called = 0;
    mm_get_phys_page = mm_get_phys_page_stub;
    paging_enabled = 1;
    pg_enabled_override = 0;
    mm_get_pt_address = mm_get_pt_address_stub;
    /*
     * Stub for end of kernel BSS section
     */
    mm_get_bss_end = mm_get_bss_end_stub;
    /*
     * Stub for mm_attach_page
     */
    mm_attach_page = mm_attach_page_stub;
    mm_detach_page = mm_detach_page_stub;
    /* and call function to set up source page tables */
    mm_init_address_spaces();
    mm_init_page_tables();
    /*
     * Set up stub for mm_get_ptd
     */
    test_ptd = (pte_t*) cr3;
    mm_get_ptd = mm_get_ptd_stub;
    mm_get_ptd_for_pid = mm_get_ptd_for_pid_stub;
    /*
     * Setup done. Now we start the actual test.
     */
    mm_release_page_tables(0);
    /*
     * Walk page table directory and check that all pages above common area
     * are not present, whereas all pages in the common area are still there
     */
    for (i = 0; i < MM_PT_ENTRIES; i++) {
        if (i<MM_SHARED_PAGE_TABLES) {
            ASSERT(1==test_ptd[i].p);
        }
        else {
            ASSERT(0==test_ptd[i].p);
            ASSERT(0==BITFIELD_GET_BIT(phys_mem, MM_PAGE(test_ptd[i].page_base*MM_PAGE_SIZE)));
        }
    }
    return 0;
}

/*
 * Testcase 20
 * Function: mm_virt_to_phys
 * Testcase: Map a page, starting with an empty page table directory, paging enabled. Then call
 * mm_virt_to_phys
 * Expected result: correct physical address is returned
 */
int testcase20() {
    pte_t __attribute__ ((aligned(4096))) ptd[1024];
    pte_t* pt;
    /*
     * This is our test page table which we are going to
     * present to mm_map_page via the stubbed version of
     * mm_get_pt_address
     */
    char __attribute__ ((aligned(4096))) page[4096];
    u32 virtual = 0xa1230000;
    u32 physical = 0xbedf0000;
    u32 phys_page_table = (u32) page;
    u32 ptd_offset;
    u32 pt_offset;
    int i;
    for (i = 0; i < sizeof(pte_t) * 1024; i++)
        (page)[i] = 0;
    for (i = 0; i < sizeof(pte_t) * 1024; i++)
        ((char*) ptd)[i] = 0;
    /*
     * Set up stub for physical page allocation
     * We use an address different from the virtual address
     * as this address is not used except to generate the PTD entry
     */
    mm_get_phys_page = mm_get_phys_page_stub;
    mm_get_phys_page_called = 0;
    phys_page[0] = phys_page_table;
    phys_page[1] = 0;
    /* Set up stub for read access to CR0 */
    paging_enabled = 1;
    /* Set up stub for translation of PTD entry
     * to pointer.
     * In this setup, we know that only one page table is
     * used. We therefore divert all access to this page table
     * to the array page declared above (set pg_enabled_override=1).
     * Thus the function mm_map_page which is under test here will access
     * this memory area when adding entries to the page table and we can
     * run our verifications against this area as well.
     */
    mm_get_pt_address = mm_get_pt_address_stub;
    test_ptd = ptd;
    mm_get_ptd=mm_get_ptd_stub;
    next_pt_address = (pte_t*) page;
    pg_enabled_override = 1;
    mm_map_page(ptd, physical, virtual, 1, 1, 1, 0);
    /*
     * Now call translation function
     */
    ASSERT(physical==mm_virt_to_phys(virtual));
    ASSERT(physical+1==mm_virt_to_phys(virtual+1));
    return 0;
}

/*
 * Testcase 21
 * Tested function: mm_map_memio
 * Testcase: map one mem I/O page and verify that a virtual
 * address different from zero is returned which is located
 * in the area reserved for memory mapped I/O
 */
int testcase21() {
    u32 my_base;
    int errno;
    u32 memio;
    u32 page;
    int i;
    int nr_of_pages = 2 * (2 + MM_SHARED_PAGE_TABLES + MM_STACK_PAGES_TASK);
    u32 my_mem = setup_phys_pages(nr_of_pages);
    /*
     * Zero out target memory
     */
    memset((void*) my_mem, 0, nr_of_pages * 4096);
    mm_get_phys_page_called = 0;
    mm_get_phys_page = mm_get_phys_page_stub;
    paging_enabled = 1;
    pg_enabled_override = 0;
    mm_get_pt_address = mm_get_pt_address_stub;
    /*
     * Stub for end of kernel BSS section
     */
    mm_get_bss_end = mm_get_bss_end_stub;
    /*
     * Stub for mm_attach_page
     */
    mm_attach_page = mm_attach_page_stub;
    mm_detach_page = mm_detach_page_stub;
    /* and call function to set up source page tables */
    mm_init_address_spaces();
    mm_init_page_tables();
    /*
     * Set up stub for mm_get_ptd
     */
    test_ptd = (pte_t*) cr3;
    mm_get_ptd = mm_get_ptd_stub;
    mm_get_ptd_for_pid = mm_get_ptd_for_pid_stub;
    /*
     * Setup done. Now we start the actual test.
     */
    memio = mm_map_memio(0xfec00000, 14);
    ASSERT(memio);
    ASSERT(memio>=MM_MEMIO_START);
    ASSERT((memio+13)<=MM_MEMIO_END);
    return 0;
}

/*
 * Testcase 22
 * Tested function: mm_map_memio
 * Testcase: map two mem I/O page and verify that both pages
 * are mapped to adjacent physical addresses
 */
int testcase22() {
    u32 my_base;
    int errno;
    u32 memio;
    u32 page;
    int i;
    int nr_of_pages = 2 * (2 + MM_SHARED_PAGE_TABLES + MM_STACK_PAGES_TASK);
    u32 my_mem = setup_phys_pages(nr_of_pages);
    /*
     * Zero out target memory
     */
    memset((void*) my_mem, 0, nr_of_pages * 4096);
    mm_get_phys_page_called = 0;
    mm_get_phys_page = mm_get_phys_page_stub;
    paging_enabled = 1;
    pg_enabled_override = 0;
    mm_get_pt_address = mm_get_pt_address_stub;
    /*
     * Stub for end of kernel BSS section
     */
    mm_get_bss_end = mm_get_bss_end_stub;
    /*
     * Stub for mm_attach_page
     */
    mm_attach_page = mm_attach_page_stub;
    mm_detach_page = mm_detach_page_stub;
    /* and call function to set up source page tables */
    mm_init_address_spaces();
    mm_init_page_tables();
    /*
     * Set up stub for mm_get_ptd
     */
    test_ptd = (pte_t*) cr3;
    mm_get_ptd = mm_get_ptd_stub;
    mm_get_ptd_for_pid = mm_get_ptd_for_pid_stub;
    /*
     * Setup done. Now we start the actual test.
     */
    memio = mm_map_memio(0xfec00000, 4097);
    ASSERT(memio);
    ASSERT(memio>=MM_MEMIO_START);
    ASSERT((memio+13)<=MM_MEMIO_END);
    ASSERT(mm_virt_to_phys(memio));
    ASSERT(mm_virt_to_phys(memio+4096));
    ASSERT(mm_virt_to_phys(memio+4096)==(mm_virt_to_phys(memio)+4096));
    return 0;
}

/*
 * Testcase 23
 * Tested function: mm_map_memio
 * Testcase: map one mem I/O page and verify that a second
 * mapping returns a different virtual address
 */
int testcase23() {
    u32 my_base;
    int errno;
    u32 memio1;
    u32 memio2;
    u32 page;
    int i;
    int nr_of_pages = 2 * (2 + MM_SHARED_PAGE_TABLES + MM_STACK_PAGES_TASK);
    u32 my_mem = setup_phys_pages(nr_of_pages);
    /*
     * Zero out target memory
     */
    memset((void*) my_mem, 0, nr_of_pages * 4096);
    mm_get_phys_page_called = 0;
    mm_get_phys_page = mm_get_phys_page_stub;
    paging_enabled = 1;
    pg_enabled_override = 0;
    mm_get_pt_address = mm_get_pt_address_stub;
    /*
     * Stub for end of kernel BSS section
     */
    mm_get_bss_end = mm_get_bss_end_stub;
    /*
     * Stub for mm_attach_page
     */
    mm_attach_page = mm_attach_page_stub;
    mm_detach_page = mm_detach_page_stub;
    /* and call function to set up source page tables */
    mm_init_address_spaces();
    mm_init_page_tables();
    /*
     * Set up stub for mm_get_ptd
     */
    test_ptd = (pte_t*) cr3;
    mm_get_ptd = mm_get_ptd_stub;
    mm_get_ptd_for_pid = mm_get_ptd_for_pid_stub;
    /*
     * Setup done. Now we start the actual test.
     */
    memio1 = mm_map_memio(0xfec00000, 14);
    ASSERT(memio1);
    ASSERT(memio1>=MM_MEMIO_START);
    memio2 = mm_map_memio(0xfec00000, 14);
    ASSERT(memio2);
    ASSERT(memio2>=MM_MEMIO_START);
    ASSERT(memio1 != memio2);
    return 0;
}

/*
 * Testcase 24
 * Tested function: mm_map_memio
 * Testcase: map two mem I/O page and verify that
 * another call returns a region which does not overlap with that
 * returned by the first call
 */
int testcase24() {
    u32 my_base;
    int errno;
    u32 memio;
    u32 page;
    int i;
    int nr_of_pages = 2 * (2 + MM_SHARED_PAGE_TABLES + MM_STACK_PAGES_TASK);
    u32 my_mem = setup_phys_pages(nr_of_pages);
    /*
     * Zero out target memory
     */
    memset((void*) my_mem, 0, nr_of_pages * 4096);
    mm_get_phys_page_called = 0;
    mm_get_phys_page = mm_get_phys_page_stub;
    paging_enabled = 1;
    pg_enabled_override = 0;
    mm_get_pt_address = mm_get_pt_address_stub;
    /*
     * Stub for end of kernel BSS section
     */
    mm_get_bss_end = mm_get_bss_end_stub;
    /*
     * Stub for mm_attach_page
     */
    mm_attach_page = mm_attach_page_stub;
    mm_detach_page = mm_detach_page_stub;
    /* and call function to set up source page tables */
    mm_init_address_spaces();
    mm_init_page_tables();
    /*
     * Set up stub for mm_get_ptd
     */
    test_ptd = (pte_t*) cr3;
    mm_get_ptd = mm_get_ptd_stub;
    mm_get_ptd_for_pid = mm_get_ptd_for_pid_stub;
    /*
     * Setup done. Now we start the actual test.
     */
    memio = mm_map_memio(0xfec00000, 4097);
    ASSERT(memio);
    ASSERT(memio>=MM_MEMIO_START);
    ASSERT((memio+13)<=MM_MEMIO_END);
    ASSERT(mm_virt_to_phys(memio));
    ASSERT(mm_virt_to_phys(memio+4096));
    ASSERT(mm_virt_to_phys(memio+4096)==(mm_virt_to_phys(memio)+4096));
    ASSERT(mm_map_memio(0xfec00000, 4) > (memio+4096));
    return 0;
}

/*
 * Testcase 25
 * Function: mm_validate_buffer
 * Test case that a buffer is valid, i.e. entirely contained in user space
 */
int testcase25() {
    pte_t __attribute__ ((aligned(4096))) ptd[1024];
    /*
     * Set up stub for mm_get_phys_page. We are going to map two pages, so we need
     * one page table directory and one page table
     */
    int errno;
    u32 my_base;
    int i;
    u32 phys;
    int nr_of_pages = 2;
    u32 my_mem = setup_phys_pages(nr_of_pages);
    mm_get_phys_page_called = 0;
    mm_get_phys_page = mm_get_phys_page_stub;
    /*
     * Set up stub for read access to CR0
     */
    paging_enabled = 0;
    /*
     * Zero page table directory
     */
    memset((void*) ptd, 0, sizeof(pte_t)*1024);
    /*
     * Set up stub for translation. Here we use method 2 outlined in the comment
     * at the beginning of this file.
     */
    mm_get_pt_address = mm_get_pt_address_stub;
    pg_enabled_override = 0;
    test_ptd = ptd;
    /*
     * Stub for end of kernel BSS section
     */
    mm_get_bss_end = mm_get_bss_end_stub;
    /*
     * Now map a physical page at address 0 for write in user mode
     */
    ASSERT(0 == mm_map_page(ptd, 0x10000, 0x20000, MM_READ_WRITE, MM_USER_PAGE, 0, 0));
    /*
     * and a second page
     */
    ASSERT(0 == mm_map_page(ptd, 0x10000 + 4096, 0x20000 + 4096, MM_READ_WRITE, MM_USER_PAGE, 0, 0));
    /*
     * and call mm_validate_buffer
     */
    ASSERT(0 == mm_validate_buffer(0x20000, 8192, 1));
    __mm_log = 0;
    do_putchar = 0;
    return 0;
}

/*
 * Testcase 26
 * Function: mm_validate_buffer
 * Test case that a buffer is mapped, but not as user space page
 */
int testcase26() {
    pte_t __attribute__ ((aligned(4096))) ptd[1024];
    /*
     * Set up stub for mm_get_phys_page. We are going to map two pages, so we need
     * one page table directory and one page table
     */
    int errno;
    u32 my_base;
    int i;
    u32 phys;
    int nr_of_pages = 2;
    u32 my_mem = setup_phys_pages(nr_of_pages);
    mm_get_phys_page_called = 0;
    mm_get_phys_page = mm_get_phys_page_stub;
    /*
     * Set up stub for read access to CR0
     */
    paging_enabled = 0;
    /*
     * Zero page table directory
     */
    memset((void*) ptd, 0, sizeof(pte_t)*1024);
    /*
     * Set up stub for translation. Here we use method 2 outlined in the comment
     * at the beginning of this file.
     */
    mm_get_pt_address = mm_get_pt_address_stub;
    pg_enabled_override = 0;
    test_ptd = ptd;
    /*
     * Stub for end of kernel BSS section
     */
    mm_get_bss_end = mm_get_bss_end_stub;
    /*
     * Now map a physical page at address 0 for write in kernel mode
     */
    ASSERT(0 == mm_map_page(ptd, 0x10000, 0x20000, MM_READ_WRITE, MM_SUPERVISOR_PAGE, 0, 0));
    /*
     * and a second page
     */
    ASSERT(0 == mm_map_page(ptd, 0x10000 + 4096, 0x20000 + 4096, MM_READ_WRITE, MM_SUPERVISOR_PAGE, 0, 0));
    /*
     * and call mm_validate_buffer
     */
    __mm_log = 0;
    ASSERT(-1 == mm_validate_buffer(0x20000, 8192, 1));
    return 0;
}

/*
 * Testcase 27
 * Function: mm_validate_buffer
 * Test case that a buffer is mapped, but not writable
 */
int testcase27() {
    pte_t __attribute__ ((aligned(4096))) ptd[1024];
    /*
     * Set up stub for mm_get_phys_page. We are going to map two pages, so we need
     * one page table directory and one page table
     */
    int errno;
    u32 my_base;
    int i;
    u32 phys;
    int nr_of_pages = 2;
    u32 my_mem = setup_phys_pages(nr_of_pages);
    mm_get_phys_page_called = 0;
    mm_get_phys_page = mm_get_phys_page_stub;
    /*
     * Set up stub for read access to CR0
     */
    paging_enabled = 0;
    /*
     * Zero page table directory
     */
    memset((void*) ptd, 0, sizeof(pte_t)*1024);
    /*
     * Set up stub for translation. Here we use method 2 outlined in the comment
     * at the beginning of this file.
     */
    mm_get_pt_address = mm_get_pt_address_stub;
    pg_enabled_override = 0;
    test_ptd = ptd;
    /*
     * Stub for end of kernel BSS section
     */
    mm_get_bss_end = mm_get_bss_end_stub;
    /*
     * Now map a physical page at address 0 for read in user mode
     */
    ASSERT(0 == mm_map_page(ptd, 0x10000, 0x20000, MM_READ_ONLY, MM_USER_PAGE, 0, 0));
    /*
     * and a second page
     */
    ASSERT(0 == mm_map_page(ptd, 0x10000 + 4096, 0x20000 + 4096, MM_READ_ONLY, MM_USER_PAGE, 0, 0));
    /*
     * and call mm_validate_buffer
     */
    ASSERT(-1 == mm_validate_buffer(0x20000, 8192, 1));
    ASSERT(0 == mm_validate_buffer(0x20000, 8192, 0));
    return 0;
}

/*
 * Testcase 28
 * Function: mm_validate_buffer
 * Borderline case - page is exactly one page / one byte plus one page
 */
int testcase28() {
    pte_t __attribute__ ((aligned(4096))) ptd[1024];
    /*
     * Set up stub for mm_get_phys_page. We are going to map two pages, so we need
     * one page table directory and one page table
     */
    int errno;
    u32 my_base;
    int i;
    u32 phys;
    int nr_of_pages = 2;
    u32 my_mem = setup_phys_pages(nr_of_pages);
    mm_get_phys_page_called = 0;
    mm_get_phys_page = mm_get_phys_page_stub;
    /*
     * Set up stub for read access to CR0
     */
    paging_enabled = 0;
    /*
     * Zero page table directory
     */
    memset((void*) ptd, 0, sizeof(pte_t)*1024);
    /*
     * Set up stub for translation. Here we use method 2 outlined in the comment
     * at the beginning of this file.
     */
    mm_get_pt_address = mm_get_pt_address_stub;
    pg_enabled_override = 0;
    test_ptd = ptd;
    /*
     * Stub for end of kernel BSS section
     */
    mm_get_bss_end = mm_get_bss_end_stub;
    /*
     * Now map a physical page at address 0 for read in user mode
     */
    ASSERT(0 == mm_map_page(ptd, 0x10000, 0x20000, MM_READ_ONLY, MM_USER_PAGE, 0, 0));
    /*
     * and call mm_validate_buffer
     */
    ASSERT(0 == mm_validate_buffer(0x20000, 4096, 0));
    ASSERT(-1 == mm_validate_buffer(0x20000, 4096 + 1, 0));
    return 0;
}

/*
 * Testcase 29
 * Function: mm_validate_buffer
 * Validate a string
 */
int testcase29() {
    pte_t __attribute__ ((aligned(4096))) ptd[1024];
    unsigned char __attribute__ ((aligned(4096))) mybuffer[4096];
    /*
     * Set up stub for mm_get_phys_page. We are going to map two pages, so we need
     * one page table directory and one page table
     */
    int errno;
    u32 my_base;
    int i;
    u32 phys;
    int nr_of_pages = 2;
    u32 my_mem = setup_phys_pages(nr_of_pages);
    mm_get_phys_page_called = 0;
    mm_get_phys_page = mm_get_phys_page_stub;
    /*
     * Set up stub for read access to CR0
     */
    paging_enabled = 0;
    /*
     * Zero page table directory
     */
    memset((void*) ptd, 0, sizeof(pte_t)*1024);
    /*
     * Set up stub for translation. Here we use method 2 outlined in the comment
     * at the beginning of this file.
     */
    mm_get_pt_address = mm_get_pt_address_stub;
    pg_enabled_override = 0;
    test_ptd = ptd;
    /*
     * Stub for end of kernel BSS section
     */
    mm_get_bss_end = mm_get_bss_end_stub;
    /*
     * Now we map the physical page mybuffer 1:1 as readable
     * user space page
     */
    ASSERT(0 == mm_map_page(ptd, (u32) mybuffer, (u32) mybuffer, MM_READ_ONLY, MM_USER_PAGE, 0, 0));
    /*
     * and place a string in the first few bytes
     */
    memset((void*) mybuffer, 0, 4096);
    strcpy(mybuffer, "hello");
    /*
     * We should now be able to validate the string buffer successfully
     */
    ASSERT(0 == mm_validate_buffer((u32) mybuffer, 0, 0));
    return 0;
}

/*
 * Testcase 30
 * Function: mm_validate_buffer
 * Validate a string that crosses a page boundary
 */
int testcase30() {
    pte_t __attribute__ ((aligned(4096))) ptd[1024];
    unsigned char __attribute__ ((aligned(4096))) mybuffer[8192];
    /*
     * Set up stub for mm_get_phys_page. We are going to map two pages, so we need
     * one page table directory and one page table
     */
    int errno;
    u32 my_base;
    int i;
    u32 phys;
    int nr_of_pages = 2;
    u32 my_mem = setup_phys_pages(nr_of_pages);
    mm_get_phys_page_called = 0;
    mm_get_phys_page = mm_get_phys_page_stub;
    /*
     * Set up stub for read access to CR0
     */
    paging_enabled = 0;
    /*
     * Zero page table directory
     */
    memset((void*) ptd, 0, sizeof(pte_t)*1024);
    /*
     * Set up stub for translation. Here we use method 2 outlined in the comment
     * at the beginning of this file.
     */
    mm_get_pt_address = mm_get_pt_address_stub;
    pg_enabled_override = 0;
    test_ptd = ptd;
    /*
     * Stub for end of kernel BSS section
     */
    mm_get_bss_end = mm_get_bss_end_stub;
    /*
     * Now we map the physical page mybuffer 1:1 as readable
     * user space page
     */
    ASSERT(0 == mm_map_page(ptd, (u32) mybuffer, (u32) mybuffer, MM_READ_ONLY, MM_USER_PAGE, 0, 0));
    /*
     * and place a string near the page end which crosses the page boundary
     */
    memset((void*) mybuffer, 0, 8192);
    strcpy(mybuffer+4094, "some_string");
    /*
     * Validation should fail
     */
    ASSERT(-1 == mm_validate_buffer((u32) mybuffer + 4094, 0, 0));
    /*
     * Now map second page
     */
    ASSERT(0 == mm_map_page(ptd, (u32) mybuffer + 4096, (u32) mybuffer + 4096, MM_READ_ONLY, MM_USER_PAGE, 0, 0));
    /*
     * and repeat validation - this time it should work
     */
    ASSERT(0 == mm_validate_buffer((u32) mybuffer + 4094, 0, 0));
    return 0;
}

/*
 * Testcase 31
 * Function: mm_validate_buffer
 * Validate a string that ends at a page boundary, i.e. 0 is last byte of page
 */
int testcase31() {
    pte_t __attribute__ ((aligned(4096))) ptd[1024];
    unsigned char __attribute__ ((aligned(4096))) mybuffer[8192];
    /*
     * Set up stub for mm_get_phys_page. We are going to map two pages, so we need
     * one page table directory and one page table
     */
    int errno;
    u32 my_base;
    int i;
    u32 phys;
    int nr_of_pages = 2;
    u32 my_mem = setup_phys_pages(nr_of_pages);
    mm_get_phys_page_called = 0;
    mm_get_phys_page = mm_get_phys_page_stub;
    /*
     * Set up stub for read access to CR0
     */
    paging_enabled = 0;
    /*
     * Zero page table directory
     */
    memset((void*) ptd, 0, sizeof(pte_t)*1024);
    /*
     * Set up stub for translation. Here we use method 2 outlined in the comment
     * at the beginning of this file.
     */
    mm_get_pt_address = mm_get_pt_address_stub;
    pg_enabled_override = 0;
    test_ptd = ptd;
    /*
     * Stub for end of kernel BSS section
     */
    mm_get_bss_end = mm_get_bss_end_stub;
    /*
     * Now we map the physical page mybuffer 1:1 as readable
     * user space page
     */
    ASSERT(0 == mm_map_page(ptd, (u32) mybuffer, (u32) mybuffer, MM_READ_ONLY, MM_USER_PAGE, 0, 0));
    /*
     * and place a string near the page end which ends at the page boundary. More specifically, we
     * place the string abcde in the bytes 4090, 4091, 4092, 4093, 4094 so that the trailing 0 is the
     * last byte of the page
     */
    memset((void*) mybuffer, 0, 8192);
    strcpy(mybuffer + 4090, "abcde");
    ASSERT('e' == mybuffer[4094]);
    ASSERT(0 == mybuffer[4095]);
    /*
     * Validation should be successful
     */
    ASSERT(0 == mm_validate_buffer((u32) mybuffer + 4090, 0, 0));
    return 0;
}

/*
 * Testcase 32
 * Function: mm_validate_buffer
 * Validate a string that ends at a page boundary, i.e. 0 is last byte of page
 */
int testcase32() {
    pte_t __attribute__ ((aligned(4096))) ptd[1024];
    unsigned char __attribute__ ((aligned(4096))) mybuffer[8192];
    /*
     * Set up stub for mm_get_phys_page. We are going to map two pages, so we need
     * one page table directory and one page table
     */
    int errno;
    u32 my_base;
    int i;
    u32 phys;
    int nr_of_pages = 2;
    u32 my_mem = setup_phys_pages(nr_of_pages);
    mm_get_phys_page_called = 0;
    mm_get_phys_page = mm_get_phys_page_stub;
    /*
     * Set up stub for read access to CR0
     */
    paging_enabled = 0;
    /*
     * Zero page table directory
     */
    memset((void*) ptd, 0, sizeof(pte_t)*1024);
    /*
     * Set up stub for translation. Here we use method 2 outlined in the comment
     * at the beginning of this file.
     */
    mm_get_pt_address = mm_get_pt_address_stub;
    pg_enabled_override = 0;
    test_ptd = ptd;
    /*
     * Stub for end of kernel BSS section
     */
    mm_get_bss_end = mm_get_bss_end_stub;
    /*
     * Now we map the physical page mybuffer 1:1 as readable
     * user space page
     */
    ASSERT(0 == mm_map_page(ptd, (u32) mybuffer, (u32) mybuffer, MM_READ_ONLY, MM_USER_PAGE, 0, 0));
    /*
     * and place a string near the page end which crosses at the page boundary. More specifically, we
     * place the string abcdef in the bytes 4090, 4091, 4092, 4093, 4094 and 4095 so that the trailing 0 is the
     * first byte of the following page
     */
    memset((void*) mybuffer, 0, 8192);
    strcpy(mybuffer + 4090, "abcdef");
    ASSERT('f' == mybuffer[4095]);
    ASSERT(0 == mybuffer[4096]);
    /*
     * Validation should not be successful
     */
    ASSERT(-1 == mm_validate_buffer((u32) mybuffer + 4090, 0, 0));
    /*
     * Now map second page
     */
    ASSERT(0 == mm_map_page(ptd, (u32) mybuffer + 4096, (u32) mybuffer + 4096, MM_READ_ONLY, MM_USER_PAGE, 0, 0));
    /*
     * and repeat validation - this time it should work
     */
    ASSERT(0 == mm_validate_buffer((u32) mybuffer + 4090, 0, 0));
    return 0;
}


int main() {
    INIT;
    /*
     * Save original pointer to mm_get_ptd
     */
    mm_get_ptd_orig = mm_get_ptd;
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
    END;
}
