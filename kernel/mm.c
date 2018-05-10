/*
 * mm.c
 *
 * This is the implementation of the memory manager. The memory manager is responsible for the allocation of physical
 * memory, the initialization and maintenance of the page tables and the management of virtual memory.
 *
 * In ctOS, the virtual memory within each address space is organized as follows.
 *
 *      ----------------------------------------------------  <------ top of virtual memory
 *      |                                                  |
 *      |   0xFFC0:0000 - 0xFFFF:FFFF: page tables         |  <------ 1024 (MM_PT_ENTRIES) pages, i.e. 4 MB
 *      |                                                  |
 *      ----------------------------------------------------
 *      |                                                  |
 *      |              Some free pages                     | <-------- MM_RESERVED_PAGES pages - used for temporarily attaching pages
 *      |                                                  |
 *      ---------------------------------------------------- <-------- MM_VIRTUAL_TOS
 *      |                                                  |
 *      |   Kernel stack (top of stack: 0xFFBF:7FFF)       | <-------- MM_STACK_PAGES pages
 *      |                                                  |
 *      ---------------------------------------------------- <-------- MM_VIRTUAL_TOS_USER
 *      |                                                  |
 *      |            User space stack area                 |
 *      |                                                  |
 *      ----------------------------------------------------
 *      |                                                  |
 *      |          User space data, BSS and code           |
 *      |                                                  |
 *      ---------------------------------------------------- <-------- MM_MEMIO_END, currently 0x7FF:FFFF
 *      |                                                  |
 *      |    Common area reserved for memory mapped I/O    |
 *      |                                                  |
 *      ---------------------------------------------------- <-------- MM_MEMIO_START, currently 0x7C0:0000
 *      |                                                  |
 *      |                Kernel heap                       |
 *      |                                                  |
 *      ----------------------------------------------------
 *      |                                                  |
 *      |                 RAM disk                         |
 *      |                                                  |
 *      ---------------------------------------------------- <-------- Should not exceed MM_MEMIO_START - MIN_HEAP bytes
 *      |                                                  |
 *      |       Kernel code, data and BSS sections         |
 *      |                                                  |
 *      ---------------------------------------------------- <-------- 1 MB boundary
 *      |                                                  |
 *      |               Low memory                         |
 *      |                                                  |
 *      ----------------------------------------------------
 *
 * The following data structures are used in the memory manager:
 *
 * - the structure phys_mem_layout contains information on the layout of the physical memory
 * - the bitmask phys_mem keeps track of used and free physical pages
 * - an instance of heap_t contains the metadata for the common kernel heap
 * - corresponding to each process, there is an instance of the structure address_space_t which describes the virtual address
 *   space of this process
 * - corresponding to each task, there is an instance of the structure stack_allocator_t which is used to reserve a part of the
 *   kernel stack of the process for this thread. The stack allocators are accessible from the address space structure as a linked
 *   list
 * - after a task switch, the stack of a task is briefly switched to a per-CPU common kernel stack to be able to perform cleanup
 *   activities outside of a task context. The array common_kernel_stack within the kernel BSS area holds these stacks
 * - finally there is an array of page table directories, one per process. All page table directories are kept in the common
 *   kernel area to be accessible in every context
 *
 * The locking strategy is as follows.
 *
 * For each process, there are three locks to protect the integrity of the virtual address space.
 * - the lock pt_lock is used to protect the page table entries of the process
 * - the lock sp_lock is used to protect the list of special pages above the kernel stack which can be used temporarily, for instance
 *   to zero a physical page before using it
 * - the lock st_lock is used to protect the list of stack allocators for a given process
 * In addition, there are a few cross-process data structures and locks protecting them:
 * - phys_mem_lock - protect bitmap of available physical pages
 * - kernel_heap_lock - protect kernel heap metadata
 * - address_spaces_lock - protect list of address spaces. Each address space itself is again protected by a lock.
 *
 * To avoid deadlocks, only certain orders of getting and acquiring locks are allowed. These rules are summarized in the following chart,
 * where an arrow A ---> B means that if you own lock A, you can safely get lock B in addition (this does not mean that having A is a
 * prerequisite for getting B)
 *
 *
 *         address_space.lock             kernel_heap_lock                 st_lock[pid]
 *                 |                             |                                |
 *                 |                             |                                |
 *                 |                             |                                |
 *                 |                             |                                |
 *                 |----------------->     pt_lock[pid]   <-----------------------|
 *                 |                             |
 *                 |                             |
 *                 |                     -----------------
 *                 |                     |               |
 *                 |                     V               V
 *                 -------------->  phys_mem_lock     sp_lock[pid]
 *
 *
 *
 * TLB invalidation on SMP systems:
 *
 * On an SMP systems, it might happen that threads belonging to the same process run on different CPUs. In these cases, a change
 * which is made to the page table needs to be reflected in the TLBs of all CPUs. For the local CPU, this is done by calling invlpg
 * in mm_map_page / mm_unmap_page. For remote CPUs, two cases need to be distinguished.
 *
 * a) in case the change to the page table means that access is allowed (i.e. a page is mapped or a read access is promoted to a
 * r/w access), we use lazy TLB invalidation. More specifically, assume that a page is added to the page tables on CPU A and a thread
 * in the same process is running on CPU B. When this thread now accesses the page and has stale TLB data stating that the page is
 * not mapped, a page fault will be raised. This page fault is diverted to the page fault handler. The page fault handler will read
 * the page table entry, determine that the access is allowed, call invlpg and resume processing. When the interrupt handler returns,
 * the faulty instruction is executed once more and succeeds.
 *
 * b) things are more difficult in case access which was previously granted is denied by the change. Suppose again that a thread running
 * on CPU A removes a page mapping and another thread in the same address space running on CPU B tries to access the page. If the mapping
 * is still in the TLB of CPU B, a wrong address translation will take space. In this case, an IPI would be required to inform the remote
 * CPU about the invalidation of the page table entry. Similarly, a thread could be migrated to another CPU and try to access the page after
 * the migration, with the same result. However, ctOS does currently not use this for the following reason.
 *
 * There are only three situations in which a page is unmapped.
 *
 * a) sometimes, pages above the kernel stack are mapped and unmapped temporarily to access physical pages not mapped into the address
 *    space of the current process. However, access to these pages is restricted to a few lines of code with no possibility of migrating
 *    the task to another CPU (no blocking / sleeping), thus no special action is required here.
 * b) when a kernel thread exits, its kernel stack space is unmapped. Thus if thread B tries to access the kernel stack of task A after
 *    task A has terminated, it could use an outdated mapping. Note that a thread can easily and legally access another threads stack,
 *    this will for instance happen if thread A spawns thread B, passing a reference to a local variable as argument or putting a
 *    reference to the stack into a shared data structure. However, when thread A exits, this data would become invalid anyway as the
 *    stack is rolled back, thus this type of access AFTER a task has exited would be considered a bug anyway. As the kernel stack is not
 *    readable for user space code, we assume that this type of access does not happen in the kernel. Thus we also do not implement a
 *    remote TLB invalidation in this case.
 * c) finally, user space pages are unmapped when a process exits. In this case, however, the exit processing will make sure that all but
 *    one task within the process complete before the pages are actually unmapped. The last task will never return to user space again.
 *    Thus the user part of the address space is only accessed by one CPU at this point and no TLB invalidation on other CPUs is needed
 *    (note, however, that file system operations like closing all open files can lead to a migration to another CPU, however only kernel
 *    pages which are not unmapped will be accessed in this case).
 *
 * This is the reason why an IPI mechanism to invalidate TLBs ("TLB shootdown") is not implemented by ctOS at the moment. Should any of
 * the assumptions taken above change (if, for instance, we decide to migrate tasks to other CPUs while they are running or when we
 * implement a version of the sbrk system call which can actually give back memory to the system), this needs to be revisited.
 *
 */

#include "mm.h"
#include "lib/string.h"
#include "debug.h"
#include "lists.h"
#include "locks.h"
#include "pagetables.h"
#include "util.h"
#include "gdt.h"
#include "lib/os/heap.h"
#include "pm.h"
#include "params.h"
#include "kerrno.h"
#include "smp.h"

static char* __module = "MEM   ";

/*
 * Local debugging can be turned on here
 */
int __mm_log = 0;
#define MM_DEBUG(...) do {if (__mm_log > 0 ) { kprintf("DEBUG at %s@%d (%s): ", __FILE__, __LINE__, __FUNCTION__); \
        kprintf(__VA_ARGS__); }} while (0)


/*
 * These symbols are used by the ELF loader
 */
extern u32 _end;
extern u32 _start;

/*
 * Start and end address of the ramdisk within the common area
 * (start of first and last page)
 */
static u32 virt_ramdisk_start;
static u32 virt_ramdisk_end;
static int have_ramdisk = 0;

/*
 * This structure describes the layout of physical memory
 */
static phys_mem_layout_t phys_mem_layout;

/*
 * The variable start_search marks the page at which the next search for
 * free physical memory is started and is maintained according to the
 * following algorithm
 * - initially the value is 0
 * - when we search for a free page, we start at this page
 * - we then search until we find the first free page from there, allocate this page and set start_search
 *   to the next page
 * - when a page is returned to the pool, start_search is set to this page if this
 * page was below start_search
 * This makes sure that at any point in time, all pages below that value
 * are in use
 */
static u32 start_search = 0;

/*
 * This is a bitmask describing usage of physical memory
 * A set bit indicates that the page is in use
 */
static u8 phys_mem[MM_PHYS_MEM_PAGES / 8];
static spinlock_t phys_mem_lock;

/*
 * This structure holds the kernel heap
 */
static heap_t kernel_heap;
static spinlock_t kernel_heap_lock;
static int kernel_heap_initialized = 0;

/*
 * The following tables hold address spaces and stack allocator
 */
static stack_allocator_t stack_allocator[PM_MAX_TASK];
static address_space_t address_space[PM_MAX_PROCESS];
static spinlock_t address_spaces_lock;

/*
 * This structure holds the page table directories for each process
 */
static __attribute__ ((aligned (4096))) ptd_t proc_ptd[PM_MAX_PROCESS];

/*
 * This array contains the locks used to protect the
 * address spaces of a process - one lock for each process
 */
static mem_locks_t mem_locks[PM_MAX_PROCESS];

/*
 * This static memory is used for the common kernel stack which is switched to during the post-interrupt handler
 * As this is within the kernel BSS segment, it is within the area which is mapped 1-1 into all address spaces
 */
static u8 __attribute__ ((aligned (MM_PAGE_SIZE))) common_kernel_stack[MM_PAGE_SIZE*MM_COMMON_KERNEL_STACK_PAGES*SMP_MAX_CPU];

/*
 * Forward declarations
 */
static int access_allowed(u32 virtual_address, pte_t* ptd, int sv, int rw);

/****************************************************************************************
 * The following functions constitute the physical memory manager                       *
 ***************************************************************************************/

/*
 * Initialize the structure describing the layout of physical memory
 */
static void phys_mem_layout_init() {
    phys_mem_layout.kernel_end = ((u32) &_end) - 1;
    phys_mem_layout.kernel_start = (u32) &_start;
    phys_mem_layout.ramdisk_start = 0;
    phys_mem_layout.ramdisk_end = 0;
    phys_mem_layout.available = 0;
    phys_mem_layout.mem_end = 0;
    /*
     * Make sure that the kernel BSS section fits entirely within the common
     * area
     */
    if (phys_mem_layout.kernel_end >= MM_COMMON_AREA_SIZE) {
        PANIC("Size of common area not sufficient, kernel end is %p\n", phys_mem_layout.kernel_end);
        return;
    }
}

/*
 * Get physical end of kernel section, i.e. end of kernel BSS section
 * This has been put into a function accessed via a pointer for stubbing
 * Return value:
 * end of kernel BSS section
 */
static u32 mm_get_bss_end_impl() {
    return phys_mem_layout.kernel_end;
}
u32 (*mm_get_bss_end)() = mm_get_bss_end_impl;

/*
 * This function will walk the list of memory map entries
 * If the entry is entirely below the 32 bit address limit and marked as available RAM,
 * it will mark all pages within this area which are above the 1M limit as unused in the physical
 * memory map
 * Parameters:
 * @multiboot_info_block - pointer to the multiboot information block
 */
static void walk_memory_map() {
    memory_map_entry_t memory_map_entry;
    u32 start;
    u32 size;
    u32 end;
    u32 first_page;
    u32 last_page;
    u32 i;
    phys_mem_layout.mem_end = 0;
    while (multiboot_get_next_mmap_entry(&memory_map_entry)) {
        start = memory_map_entry.base_addr_low;
        size = memory_map_entry.length_low - 1;
        end = start + size;
        /*
         * Ignore entries above the physical 32 bit address space
         */
        if ((memory_map_entry.base_addr_high)
                || (memory_map_entry.length_high) || (size > ~start)) {
            /*
             * Skip entry
             */
        }
        else {
            /*
             * Increase mem_end if needed
             */
            if (end > phys_mem_layout.mem_end)
                phys_mem_layout.mem_end = end;
            /*
             * If area is marked as free, release pages within that area which are above 1M
             */
            if ((MB_MMAP_ENTRY_TYPE_FREE == memory_map_entry.type) && (end >= (MM_HIGH_MEM_START
                    + MM_PAGE_SIZE))) {
                first_page = MM_PAGE(start) + 1;
                last_page = MM_PAGE(end) - 1;
                if (MM_PAGE_START(first_page-1) == start)
                    first_page--;
                if (MM_PAGE_START(first_page) < MM_HIGH_MEM_START)
                    first_page = MM_PAGE(MM_HIGH_MEM_START);
                if ((MM_PAGE_END(last_page+1) == end))
                    last_page++;
                MSG("Found usable RAM at range %x - %x\n", MM_PAGE_START(first_page), MM_PAGE_END(last_page));
                for (i = first_page; i <= last_page; i++) {
                    phys_mem_layout.available++;
                    phys_mem_layout.total++;
                    BITFIELD_CLEAR_BIT(phys_mem, i);
                }
            }
        }
    }
}

/*
 * Parse the multiboot information structure to locate the
 * initial ramdisk. A ramdisk is specified as the one and
 * only module passed to a multiboot compliant boot loader.
 * If a ramdisk could be found, update the physical memory
 * layout and mark the physical pages containing the ram disk
 * as used
 * Parameter:
 * @multiboot_info_block - a pointer to the GRUB2 information block
 */
static void locate_ramdisk() {
    multiboot_ramdisk_info_block_t rd_info;
    int page;
    have_ramdisk = multiboot_locate_ramdisk(&rd_info);
    if (0 == have_ramdisk)
        return;
    phys_mem_layout.ramdisk_start = rd_info.start;
    phys_mem_layout.ramdisk_end = rd_info.end;
    MSG("Found ramdisk at %x\n", rd_info.start);
    for (page = MM_PAGE(rd_info.start); page <=MM_PAGE(rd_info.end); page++) {
        phys_mem_layout.available--;
        BITFIELD_SET_BIT(phys_mem,page);
    }
}

/*
 * Initialize bitmask of physical pages
 * Parameters:
 * @info_block_ptr - address of multiboot information block
 */
static void phys_mem_init() {
    int i;
    u32 j;
    /*
     * First we set everything to 1
     * and initialize the lock
     */
    for (j = 0; j < MM_PHYS_MEM_PAGES / 8; j++)
        phys_mem[j]=0xff;
    spinlock_init(&phys_mem_lock);
    /*
     * Next we go through the tables provided by the GRUB2
     * boot loader and mark all pages as available which
     * are entirely within a free RAM area (type=1)
     */
    walk_memory_map();
    /*
     * Now we have also marked the pages which the kernel itself contains as available
     * Fix this
     */
    i = MM_PAGE(phys_mem_layout.kernel_start);
    while (1) {
        phys_mem_layout.available--;
        BITFIELD_SET_BIT(phys_mem, i);
        if (phys_mem_layout.kernel_end < MM_PAGE_END(i))
            break;
        i++;
    }
    /*
     * Finally process the list of modules to determine the location of the
     * intial RAM disk - if any
     */
    locate_ramdisk();
}

/*
 * This function returns a free physical page in memory
 * and marks it as used. The page is returned by providing its base address
 * If no free page could be found 0 is returned
 * Return value:
 * the base address of the physical page
 * Locks:
 * phys_mem_lock
 */
static u32 mm_get_phys_page_impl() {
    u32 page = 0;
    u32 flags;
    spinlock_get(&phys_mem_lock, &flags);
    page = start_search;
    if (__mm_log)
        PRINT("Starting search at page no. %x\n", start_search);
    while ((BITFIELD_GET_BIT(phys_mem, page)) && (page < MM_PHYS_MEM_PAGES)) {
        page++;
    }
    if (MM_PHYS_MEM_PAGES == page) {
        ERROR("No physical page left, started search at %x, resuming search at beginning\n", start_search);
        page = 0;
        start_search = 0;
        while ((BITFIELD_GET_BIT(phys_mem, page)) && (page < MM_PHYS_MEM_PAGES)) {
            page++;
        }
        if (MM_PHYS_MEM_PAGES == page)  {
            ERROR("Did not find page at second attempt, giving up\n");
            spinlock_release(&phys_mem_lock, &flags);
            return 0;
        }
    }
    BITFIELD_SET_BIT(phys_mem, page);
    start_search = page + 1;
    phys_mem_layout.available--;
    spinlock_release(&phys_mem_lock, &flags);
    MM_DEBUG("Returning physical page %x\n", page);
    return MM_PAGE_START(page);
}
/*
 * Function pointer to allow for stubbing
 */
u32 (*mm_get_phys_page)() = mm_get_phys_page_impl;

/*
 * This function releases a used physical page and
 * returns it into the pool of available pages
 * Parameter:
 * @page_base - physical base address of page to be released
 * Locks:
 * phys_mem_lock
 */
static void mm_put_phys_page_impl(u32 page_base) {
    u32 flags;
    spinlock_get(&phys_mem_lock, &flags);
    BITFIELD_CLEAR_BIT(phys_mem, MM_PAGE(page_base));
    phys_mem_layout.available++;
    if (MM_PAGE(page_base) < start_search)
        start_search = MM_PAGE(page_base);
    spinlock_release(&phys_mem_lock, &flags);
}
/*
 * Function pointer to allow for stubbing
 */
void (*mm_put_phys_page)(u32 page) = mm_put_phys_page_impl;

/****************************************************************************************
 * Everything below this line is about managing the virtual memory of a process. The    *
 * first group of functions provides basic services to manipulate page tables and       *
 * page table directories                                                               *
 ***************************************************************************************/

/*
 * Get a pointer to the page table directory of an arbitrary process
 * This function should only be called once paging has been enabled
 * It is addressed via a function pointer to allow for stubbing
 * Parameter:
 * @pid - the process id for which the PTD is requested
 * Return value:
 * pointer to the page table directory of the  process. As
 * the PTD is located below the kernel BSS end, this is at the same
 * time the physical and virtual address of the PTD
 */
static pte_t* mm_get_ptd_for_pid_impl(u32 pid) {
    return proc_ptd[pid];
}
pte_t* (*mm_get_ptd_for_pid)() = mm_get_ptd_for_pid_impl;


/*
 * Get a pointer to the page table directory of the currently running process
 * This function should only be called once paging has been enabled
 * It is addressed via a function pointer to allow for stubbing
 * Return value:
 * pointer to the page table directory of the current process. As
 * the PTD is located below the kernel BSS end, this is at the same
 * time the physical and virtual address of the PTD
 */
static pte_t* mm_get_ptd_impl() {
    return proc_ptd[pm_get_pid()];
}
pte_t* (*mm_get_ptd)() = mm_get_ptd_impl;


/*
 * Get the virtual address of a page table.
 * If paging is not yet enabled, this function will simply
 * extract the address of a page table from the page table directory entry
 * at offset @offset and return it.
 * If paging is enabled, it will use the mapping of the page tables
 * into the upper 4 MB of the virtual address space
 * Parameters:
 * @ptd: pointer to the page table directory to use
 * @ptd_offset: offset of the page table entry in the PTD which we are interested in
 * @pg_enabled: set this to 1 to indicate that paging has already been enabled
 * Return value:
 * a pointer to the virtual address of the requested page table
 */
static pte_t* mm_get_pt_address_impl(pte_t* ptd, int ptd_offset, int pg_enabled) {
    if (0 == pg_enabled)
        return (pte_t*) ((ptd[ptd_offset].page_base) * MM_PAGE_SIZE);
    else
        return (pte_t*) MM_VIRTUAL_PT_ENTRY(ptd_offset, 0);
}
/*
 * Access via function pointers to ease stubbing
 */
pte_t* (*mm_get_pt_address)(pte_t* ptd, int ptd_offset, int pg_enabled) =
        mm_get_pt_address_impl;

/*
 * Temporarily attach a physical page to the virtual address space of the current
 * process and return the virtual address
 * Parameters:
 * @phys_page: the base address of the physical page which needs to be mapped
 * Return value:
 * virtual address of newly mapped page
 * Locks:
 * sp_lock - lock to protect special page slots
 */
static u32 mm_attach_page_impl(u32 phys_page) {
    u32 first_reserved_page = MM_VIRTUAL_TOS + 1;
    pte_t* ptd = (pte_t*) mm_get_ptd();
    pte_t* pt;
    u32 page;
    u32 used_page = 0;
    u32 flags;
    spinlock_t* sp_lock;
    /*
     * In this function, we assume that the reserved pages directly above the kernel
     * stack are within the same 4 MB entry as the kernel stack itself in our
     * virtual address space, so that the page table for this area exists, and that
     * all reserved pages are within the same 4 MB area. Thus to map a page, we do
     * not need to set up a new page table but can reuse the one set up for the kernel stack.
     * Make sure that this is true
     */
    KASSERT(MM_RESERVED_PAGES + MM_STACK_PAGES <= MM_PT_ENTRIES);
    /*
     * Get lock on special pages to make sure that we are not interrupted
     * by another thread in the same process
     */
    sp_lock = &(mem_locks[pm_get_pid()].sp_lock);
    spinlock_get(sp_lock, &flags);
    /*
     * Scan page table to find a free page
     */
    pt = mm_get_pt_address(ptd, PTD_OFFSET(first_reserved_page), 1);
    for (page = PT_OFFSET(first_reserved_page); page
            < PT_OFFSET(first_reserved_page) + MM_RESERVED_PAGES; page++) {
        if (0 == pt[page].p) {
            used_page = (page - PT_OFFSET(first_reserved_page)) * MM_PAGE_SIZE
                    + first_reserved_page;
            break;
        }
    }
    if (0 == used_page) {
        ERROR("No reserved page available\n");
        spinlock_release(sp_lock, &flags);
        return 0;
    }
    /*
     * Map page by adding an entry to the page table
     */
    pt[PT_OFFSET(used_page)] = pte_create(MM_READ_WRITE, MM_SUPERVISOR_PAGE, 0, phys_page);
    invlpg(used_page);
    spinlock_release(sp_lock, &flags);
    if (used_page)
        KASSERT(used_page > MM_VIRTUAL_TOS);
    return used_page;
}
u32 (*mm_attach_page)(u32) = mm_attach_page_impl;

/*
 * Remove a physical page from the virtual address space
 * Parameter:
 * @virt_page: the virtual base address of the page to be removed
 */
static void mm_detach_page_impl(u32 virt_page) {
    KASSERT(virt_page > MM_VIRTUAL_TOS);
    pte_t* ptd = (pte_t*) mm_get_ptd();
    pte_t* pt = mm_get_pt_address(ptd, PTD_OFFSET(virt_page), 1);
    pt[PT_OFFSET(virt_page)].p = 0;
    invlpg(virt_page);
}
void (*mm_detach_page)(u32) = mm_detach_page_impl;

/*
 * Map a given virtual address (32 bit base address of page) into a given
 * physical page (32 bit page address of page). The function performs the following
 * tasks:
 * - determine the entry in the page table directory which is relevant for the virtual address
 * - use the present bit in the page table directory to find out whether a page table exists
 * for this area
 * - if not, allocate a physical page and create an empty page table directory
 * - add an entry to the page table directory
 * This function can be used before paging has been enabled and after
 * paging has been enabled
 * Also note that this function assumes that the page table directory passed
 * as first argument is the page table directory of the current process
 * in case paging has already been enabled
 * Parameter:
 * @pd - page table directory
 * @phys_base - the base address of the physical page
 * @virtual_base - the base address of the virtual page
 * @rw - read/write bit
 * @us - user/supervisor bit
 * @pcd - pcd bit
 * @pid - pid, used to determine the page table lock
 * Return value:
 * ENOMEM if no memory available for new page table
 * 0 upon success
 * Locks:
 * pt_lock - lock to protect page table structure
 * Cross-monitor function calls:
 * mm_attach_page
 * mm_detach_page
 * mm_get_phys_page
 */
int mm_map_page(pte_t* pd, u32 phys_base, u32 virtual_base, u8 rw, u8 us,
        u8 pcd, u32 pid) {
    u8 pg_enabled;
    pte_t* pt;
    u32 flags;
    u32 tmp_page;
    u32 page_table_base;
    spinlock_t* pt_lock;
    pg_enabled = get_cr0() >> 31;
    MM_DEBUG("Requested mapping for virtual page %x, rw = %d, us = %d\n", virtual_base, rw, us);
    /*
     * Get lock on page tables of current process
     */
    pt_lock = &(mem_locks[pid].pt_lock);
    spinlock_get(pt_lock, &flags);
    if (0 == pd[PTD_OFFSET(virtual_base)].p) {
        /*
         * No page table for this area yet. Get one
         */
        MM_DEBUG("Need to get page table first\n");
        if (0 == (page_table_base = mm_get_phys_page())) {
            ERROR("Could not allocate physical page for page table\n");
            spinlock_release(pt_lock, &flags);
            return ENOMEM;
        }
        /*
         * zero it
         */
        MM_DEBUG("Got physical page %x, initializing with zeroes\n", page_table_base);
        if (pg_enabled) {
            if (0 == (tmp_page = mm_attach_page(page_table_base))) {
                ERROR("Could not map physical page into address space\n");
                mm_put_phys_page(page_table_base);
                spinlock_release(pt_lock, &flags);
                return ENOMEM;
            }
        }
        else
            tmp_page = page_table_base;
        memset((void*) tmp_page, 0, MM_PT_ENTRIES * sizeof(pte_t));
        MM_DEBUG("Detaching page again\n");
        if (pg_enabled)
            mm_detach_page(tmp_page);
        /*
         * and add it to the page table directory. Note that this assignment is an atomic
         * operation on x86, thus the page table directory is valid at each point in time
         */
        pd[PTD_OFFSET(virtual_base)] = pte_create(rw, MM_USER_PAGE, pcd,
                page_table_base);
        /*
         * The page table is now mapped into our virtual address space. However, to make
         * sure that the page is visible we need to invalidate the TLB first
         */
        if (1 == pg_enabled)
            invlpg(MM_VIRTUAL_PT_ENTRY(PTD_OFFSET(virtual_base), 0));
        /*
         * Get pointer to newly created page table.
         * Note that if paging is enabled, the invlpg call above
         * will have mapped our page table into the upper 4 MB of memory
         * so that we can access it there
         */
        pt = mm_get_pt_address(pd, PTD_OFFSET(virtual_base), pg_enabled);
    }
    /*
     * Get pointer to page table
     * and add entry
     */
    pt = mm_get_pt_address(pd, PTD_OFFSET(virtual_base), pg_enabled);
    MM_DEBUG("Address of page table is %x\n", pt);
    pt[PT_OFFSET(virtual_base)] = pte_create(rw, us, pcd, phys_base);
    MM_DEBUG("Address of page table entry is %x\n", &(pt[PT_OFFSET(virtual_base)]));
    MM_DEBUG("Added entry, pte->us = %d\n", pt[PT_OFFSET(virtual_base)].us);
    /*
     * If we are already in paging mode, flush TLB
     * for the address which we have just mapped
     */
    if (1 == pg_enabled)
        invlpg(virtual_base);
    spinlock_release(pt_lock, &flags);
    return 0;
}

/*
 * Remove a given virtual page from the virtual address
 * space represented by the passed page table directory
 * and free the used physical page
 * This function can be used before paging has been enabled and after
 * paging has been enabled
 * Also note that this function assumes that the page table directory passed
 * as first argument is the page table directory of the current process
 * in case paging has already been enabled
 * Parameter:
 * @pd - a pointer to the page table directory to be used
 * @virtual_base - the base of the page to be unmapped
 * @pid - pid, used to locate the lock for the page table
 * Return value:
 * 0 upon success
 * Locks:
 * pt_lock - lock to protect page table of current process
 * Cross-monitor function calls:
 * mm_put_phys_page
 */
int mm_unmap_page(pte_t* pd, u32 virtual_base, u32 pid) {
    u8 pg_enabled;
    pte_t* pt;
    u32 flags;
    spinlock_t* pt_lock;
    pg_enabled = get_cr0() >> 31;
    /*
     * If we have a ramdisk, verify that we do not
     * by accident unmap any of its pages
     */
    if (virt_ramdisk_end > virt_ramdisk_start) {
        if ((virtual_base >= virt_ramdisk_start) && (virtual_base <= virt_ramdisk_end)) {
            PANIC("Trying to unmap page %x within RAMDISK (%x - %x)", virtual_base, virt_ramdisk_start, virt_ramdisk_end);
        }
    }
    /*
     * Get lock on page tables of current process
     */
    pt_lock = &(mem_locks[pid].pt_lock);
    spinlock_get(pt_lock, &flags);
    if (pd[PTD_OFFSET(virtual_base)].p) {
        /*
         * Get pointer to page table
         */
        pt = mm_get_pt_address(pd, PTD_OFFSET(virtual_base), pg_enabled);
        /*
         * Mark page as unused and flush TLB if needed
         */
        pt[PT_OFFSET(virtual_base)].p = 0;
        if (1 == pg_enabled)
            invlpg(virtual_base);
        /*
         * Release physical page
         */
        mm_put_phys_page(pt[PT_OFFSET(virtual_base)].page_base * MM_PAGE_SIZE);
    }
    spinlock_release(pt_lock, &flags);
    return 0;
}



/*
 * Return the physical address for a given virtual address
 * Parameter:
 * @virtual - the virtual address
 * Return value:
 * the physical address or 0 if the page is not mapped
 */
u32 mm_virt_to_phys(u32 virtual) {
    pte_t* ptd = (pte_t*) mm_get_ptd();
    /*
     * First make sure that we have a page table for this address
     */
    if (0 == ptd[PTD_OFFSET(virtual)].p)
        return 0;
    pte_t* pt = mm_get_pt_address(ptd, PTD_OFFSET(virtual), 1);
    if (0 == pt[PT_OFFSET(virtual)].p)
        return 0;
    return pt[PT_OFFSET(virtual)].page_base * MM_PAGE_SIZE + (virtual % MM_PAGE_SIZE);
}

/*
 * This function will determine whether a given virtual address is mapped
 * into the address space of the currently active process
 * Parameter:
 * @virtual_base - the base address of the page to be checked
 * Return value:
 * 1 if the page is mapped
 * 0 if it is not mapped
 */
static int mm_page_mapped_impl(u32 virtual_base) {
    pte_t* ptd = mm_get_ptd();
    pte_t* pt;
    if (0 == ptd[PTD_OFFSET(virtual_base)].p) {
        return 0;
    }
    pt = mm_get_pt_address(ptd, PTD_OFFSET(virtual_base), get_cr0() >> 31);
    if (0 == pt[PT_OFFSET(virtual_base)].p)
        return 0;
    return 1;
}
int (*mm_page_mapped)(u32) = mm_page_mapped_impl;

/****************************************************************************************
 * Functions for the initialization of the memory manager and the first process         *
 ***************************************************************************************/


/*
 * Initialize locks
 */
static void mm_init_locks() {
    int i;
    for (i = 0; i < PM_MAX_PROCESS; i++) {
        spinlock_init(&mem_locks[i].st_lock);
        spinlock_init(&mem_locks[i].sp_lock);
        spinlock_init(&mem_locks[i].pt_lock);
    }
}

/*
 * Set up the page tables for the initial process.
 * This function is supposed to be called before paging is enabled
 * It will set up an initial mapping as follows.
 * - allocate MM_SHARED_PAGE_TABLES page tables at the lower end of the physical address spaces
 * - set up the area between 0x0 and the end of the kernel BSS section as one-to-one mapping
 * - map the ramdisk - if any - into virtual memory
 * - the upper 4 MB of memory are set up to contain the page tables themselves
 * - map stack area
 * All mappings are done with rw=1, us=0
 * When the page tables have been initialized, the physical address of the PTD
 * is loaded into CR3
 * In total we will therefore request the following physical pages:
 * MM_SHARED_PAGE_TABLES for the shared page tables
 * 1 page table for the area immediately below 0xffffffff - 4 MB
 * MM_STACK_PAGES_TASK pages for the stack
 * --> in total, 1 + MM_SHARED_PAGE_TABLES + MM_STACK_PAGES are used
 * As this function is only called once, no locking is done
 * Note that we do not have to invalidate page caches here, as we do all
 * this before loading CR3 initially
 */
void mm_init_page_tables() {
    u32 page;
    int i;
    pte_t* ptd_root;
    pte_t* pt;
    u32 stack_page_v;
    u32 stack_page_p;
    ptd_root = proc_ptd[0];
    memset((void*) ptd_root, 0, MM_PT_ENTRIES * sizeof(pte_t));
    /*
     * Verify that we have at least MIN_HEAP_BYTES between the end of
     * the kernel BSS section and the supposed end of the common
     * kernel area
     */
    if (phys_mem_layout.kernel_end + MIN_HEAP_BYTES > MM_MEMIO_END) {
        PANIC("Kernel BSS section ends at %x, not enough room left for kernel heap and RAM disk\n", phys_mem_layout.kernel_end);
    }
    /*
     * Initialize the first MM_SHARED_PAGE_TABLES entries
     */
    for (i = 0; i < MM_SHARED_PAGE_TABLES; i++) {
        if (0 == (pt = (pte_t*) mm_get_phys_page())) {
            PANIC("Could not get memory for shared page table\n");
            return;
        }
        memset((void*) pt, 0, sizeof(pte_t) * MM_PT_ENTRIES);
        ptd_root[i] = pte_create(1, 0, 0, (u32) pt);
    }
    /*
     * First do one-to-one mapping for all pages up to the end of the kernel bss section
     * As this is within the common area, this should not allocate any additional physical page
     */
    page = 0;
    while (MM_PAGE_END(page) <= MM_PAGE_END(MM_PAGE(mm_get_bss_end()))) {
        mm_map_page(ptd_root, MM_PAGE_START(page), MM_PAGE_START(page),
                MM_READ_WRITE, MM_SUPERVISOR_PAGE, 0, 0);
        page++;
    }
    /*
     * If there is a ramdisk, map its pages right above the kernel BSS section
     * still within the common area
     */
    page = 0;
    virt_ramdisk_start = (MM_PAGE(mm_get_bss_end()) + 1) * MM_PAGE_SIZE;
    /*
     * Current end of ramdisk - we increase this in the following loop
     * so that it will have the correct value after finishing the loop
     */
    virt_ramdisk_end = virt_ramdisk_start;
    while (phys_mem_layout.ramdisk_start + MM_PAGE_SIZE * page
            < phys_mem_layout.ramdisk_end) {
        mm_map_page(ptd_root, phys_mem_layout.ramdisk_start + page
                * MM_PAGE_SIZE, virt_ramdisk_start + MM_PAGE_SIZE * page,
                MM_READ_WRITE, MM_SUPERVISOR_PAGE, 0, 0);
        virt_ramdisk_end = virt_ramdisk_start + MM_PAGE_SIZE * page;
        page++;
    }
    /*
      * Make sure that the end of the ramdisk is still within the common area
      */
     KASSERT(virt_ramdisk_end < MM_COMMON_AREA_SIZE);
    /*
     * Now set up the last entry in the PDT to point to itself to map pages tables
     * starting at 0xffc0:0000
     */
    ptd_root[MM_PT_ENTRIES - 1] = pte_create(1, 0, 0, (u32) ptd_root);
    /*
     * Allocate physical pages for stack and map it
     * stack_page_v is the virtual address of the lowest page of the stack
     */
    stack_page_v = ((MM_PAGE(MM_VIRTUAL_TOS)) - (MM_STACK_PAGES_TASK - 1))
            * MM_PAGE_SIZE;
    for (i = 0; i < MM_STACK_PAGES_TASK; i++) {
        if (0 == (stack_page_p = mm_get_phys_page())) {
            PANIC("Could not get memory for stack\n");
            return;
        }
        mm_map_page(ptd_root, stack_page_p, stack_page_v, MM_READ_WRITE,
                MM_SUPERVISOR_PAGE, 0, 0);
        stack_page_v += MM_PAGE_SIZE;
    }
    /*
     * Finally move physical address of PTD into CR3
     */
    put_cr3((u32) ptd_root);
}

/*
 * Initialize address space structures and stack allocators
 */
void mm_init_address_spaces() {
    int i;
    for (i = 0; i < PM_MAX_PROCESS; i++)
        address_space[i].valid = 0;
    for (i = 0; i < PM_MAX_TASK; i++)
        stack_allocator[i].valid = 0;
    address_space[0].id = 0;
    address_space[0].valid = 1;
    address_space[0].head = stack_allocator;
    address_space[0].tail = stack_allocator;
    address_space[0].end_data = MM_START_CODE - 1;
    address_space[0].brk = MM_START_CODE;
    spinlock_init(&(address_space[0].lock));
    stack_allocator[0].valid = 1;
    stack_allocator[0].next = 0;
    stack_allocator[0].prev = 0;
    stack_allocator[0].id = 0;
    stack_allocator[0].highest_page = (MM_VIRTUAL_TOS / MM_PAGE_SIZE)
            * MM_PAGE_SIZE;
    stack_allocator[0].lowest_page = stack_allocator[0].highest_page
            - (MM_STACK_PAGES_TASK - 1) * MM_PAGE_SIZE;
    stack_allocator[0].pid = 0;
    spinlock_init(&address_spaces_lock);
}

/*
 * Initialization of memory manager
 * This function will
 * - determine the kernel start and end addresses from ELF symbols
 * - parse the memory table received by the boot loader
 * - initialize bit mask describing free physical pages
 * - initialize page tables
 * Parameter:
 * @info_block_ptr - address of multiboot information structure
 */
void mm_init(u32 info_block_ptr) {
    phys_mem_layout_init();
    phys_mem_init(info_block_ptr);
    mm_init_locks();
    mm_init_address_spaces();
    mm_init_page_tables();
}

/****************************************************************************************
 * The following set of functions is responsible for managing the kernel heap           *
 ****************************************************************************************/

/*
 * Extension function for the kernel heap
 * This function is expected to return the new top of the heap if there is sufficient memory to fulfill the request
 * Memory is always allocated as a multiple of pages
 * By definition, there is enough space left if the new top of the heap is still below the start of the area reserved for
 * memory mapped I/O
 * Locking the heap structure needs to be done by the caller
 * Parameter:
 * @size - number of bytes needed in addition
 * @current_top - current top of heap
 * Return value:
 * new top of the heap or 0 if no more memory could be allocated
 */
static u32 mm_extend_heap(u32 size, u32 current_top) {
    u32 max_kheap_top = MM_MEMIO_START - 1;
    u32 new_top;
    u32 phys_page;
    u32 page;
    /*
     * Current top should always be last byte of page - make sure that this holds
     */
    KASSERT(0 == ((current_top + 1) % MM_PAGE_SIZE));
    new_top = MM_ALIGN(current_top + size) - 1;
    if (new_top <= max_kheap_top) {
        /*
         * We allocate all physical pages in a loop. This implies that if the last allocation fails,
         * the other pages will remain allocated. This is not perfect, but given the size of the common area in which
         * all this happens in virtual memory compared to the number of physical pages in a standard PC
         * these days, the likelihood of failure here is small
         */
        for (page = current_top + 1; page < new_top + 1; page += MM_PAGE_SIZE) {
            if (0 == (phys_page = mm_get_phys_page())) {
                ERROR("Out of memory - no physical pages left\n");
                return 0;
            }
            mm_map_page(mm_get_ptd(), phys_page, page, MM_READ_WRITE,
                    MM_SUPERVISOR_PAGE, 0, pm_get_pid());
        }
    }
    else {
        ERROR("Out of memory - heap region exhausted\n");
        return 0;
    }
    return new_top;
}

/*
 * Kmalloc
 * This function allocates space within the kernel heap
 * As the kernel heap is a shared data structure, we use a spinlock
 * to protect it
 * Parameters:
 * @size - size of memory to be allocated
 * Return value:
 * pointer to newly allocated heap memory or 0 if no memory could be obtained
 * Locks:
 * kernel_heap_lock
 * Cross-monitor function calls:
 * mm_get_phys_page (via extension function)
 * mm_map_page (via extension function)
 */
void* kmalloc(u32 size) {
    void* rc;
    u32 flags;
    if (0 == kernel_heap_initialized) {
        ERROR("Trying to call kmalloc even though kernel heap is not yet initialized\n");
        return 0;
    }
    spinlock_get(&kernel_heap_lock, &flags);
    rc = __ctOS_heap_malloc(&kernel_heap, size);
    spinlock_release(&kernel_heap_lock, &flags);
    return rc;
}

/*
 * Kmalloc with alignment
 * Parameters:
 * @size - size of memory to be allocated
 * @alignment - alignment to be preserved
 * Return value:
 * pointer to newly allocated heap memory or 0 if no memory could be obtained
 * Locks:
 * kernel_heap_lock
 * Cross-monitor function calls:
 * mm_get_phys_page (via extension function)
 * mm_map_page (via extension function)
 */
void* kmalloc_aligned(u32 size, u32 alignment) {
    void* rc;
    u32 flags;
    spinlock_get(&kernel_heap_lock, &flags);
    rc = __ctOS_heap_malloc_aligned(&kernel_heap, size, alignment);
    spinlock_release(&kernel_heap_lock, &flags);
    return rc;
}

/*
 * Free memory on the kernel heap
 * Parameters:
 * @ptr - a pointer to the address to free
 * Locks:
 * kernel_heap_lock
 */
void kfree(void* ptr) {
    u32 flags;
    KASSERT(ptr);
    spinlock_get(&kernel_heap_lock, &flags);
    __ctOS_heap_free(&kernel_heap, ptr);
    spinlock_release(&kernel_heap_lock, &flags);
}

/*
 * Set up the kernel heap
 * The function does not perform any locking and thus should only be called
 * during the initialization phase in a single-threaded environment
 * but after paging has been enabled
 * It will allocate one initial page for the kernel heap and map it into the common area
 * into the virtual page right after the end of the kernel bss section resp. the RAM disk
 */
void mm_init_heap() {
    u32 phys_page;
    int rc;
    /*
     * We start the heap in the first page above the ramdisk
     */
    u32 heap_start = virt_ramdisk_end + MM_PAGE_SIZE;
    u32 max_kheap_top = (MM_SHARED_PAGE_TABLES - MM_MEMIO_PAGE_TABLES)
            * MM_PAGE_SIZE * MM_PT_ENTRIES - 1;
    /*
     * Make sure that even when the memory layout is changed, there is enough
     * space for the kernel heap
     */
    KASSERT(max_kheap_top > heap_start+MM_PAGE_SIZE-1);
    /*
     * Get lock on heap
     */
    spinlock_init(&kernel_heap_lock);
    /*
     * Allocate one page and establish a heap structure within it
     */
    if (0 == (phys_page = mm_get_phys_page())) {
        PANIC("Could not allocate initial page for kernel heap\n");
        return;
    }
    mm_map_page(mm_get_ptd(), phys_page, heap_start, MM_READ_WRITE,
            MM_SUPERVISOR_PAGE, 0, 0);
    rc = __ctOS_heap_init(&kernel_heap, heap_start, heap_start + MM_PAGE_SIZE - 1,
            mm_extend_heap);
    kernel_heap.validate = params_get_int("heap_validate");
    if (rc) {
        PANIC("Initialization of kernel heap failed with rc %d\n", rc);
        return;
    }
    kernel_heap_initialized = 1;
}


/****************************************************************************************
 * To support kernel level threads, the kernel stack of a process is divided up between *
 * the threads using a structure called stack allocators. The following functions       *
 * manage this structure                                                                *
 ***************************************************************************************/

/*
 * Add a new stack allocator to the sorted list of stack allocators
 * for the current process. Locks need to be allocated by the caller
 * This will keep the list sorted, i.e. the stack allocator with the
 * lowest address is always the head
 * As the lists head and tail might change due to that, we return the new head
 * and the new tail
 * Parameters:
 * @task_id - task id of the stack area to be added, this is also the index into the table
 * stack_allocator which identifies the stack allocator to be added to the list
 */
static void mm_add_stack_allocator(u32 task_id) {
    address_space_t* as = address_space + pm_get_pid();
    stack_allocator_t* current;
    stack_allocator_t* sa = stack_allocator + task_id;
    if ((0 == as->head) || (0 == as->tail)) {
        PANIC("No stack allocators found for process %d, this should never happen\n", pm_get_pid());
        return;
    }
    /*
     * First check whether we can add the new item at the front
     * of the list
     */
    if (sa->highest_page < as->head->lowest_page) {
        LIST_ADD_FRONT(as->head, as->tail, sa);
        return;
    }
    /*
     * Walk the list to find the correct point to add it
     */
    LIST_FOREACH(as->head, current) {
        /*
         * Check whether we have reached the end of the list
         */
        if (0 == current->next) {
            LIST_ADD_END(as->head, as->tail, sa);
            return;
        }
        if ((current->highest_page < sa->lowest_page)
                && (current->next->lowest_page > sa->highest_page)) {
            LIST_ADD_AFTER(as->head, as->tail, current, sa);
            return;
        }
    }
}

/*
 * Internal utility function to locate and reserve a free area on the kernel
 * stack of the currently active process
 * Parameter:
 * @kernel_stack_base_page - first page of kernel stack area
 * @kernel_stack_top_page - last page of kernel stack area
 * @task_id - task id for the new task
 * Return value:
 * the base of the top page of the identified area or
 * zero if no area could be found
 * Locks:
 * st_lock for current process - protect list of stack allocators
 */
static u32 mm_find_free_stack(u32 kernel_stack_base_page,
        u32 kernel_stack_top_page, u32 task_id) {
    u32 new_top_page = 0;
    stack_allocator_t* sa;
    u32 limit;
    u32 pid = pm_get_pid();
    u32 eflags;
    spinlock_t* st_lock;
    /*
      * Get lock to protect stack allocators
      */
     st_lock = &(mem_locks[pm_get_pid()].st_lock);
     spinlock_get(st_lock, &eflags);
    /*
     * First check whether there is enough space left at the bottom of the stack
     * We need MM_STACK_PAGES_TASK + 2*MM_STACK_PAGES_GAP pages
     */
    if (0 == (sa = address_space[pid].head)) {
        spinlock_release(st_lock, &eflags);
        PANIC("No stack allocator found for process %d, this should never happen\n", pid);
        return 0;
    }
    new_top_page = kernel_stack_base_page + (MM_STACK_PAGES_GAP
            + MM_STACK_PAGES_TASK - 1) * MM_PAGE_SIZE;
    if ((sa->lowest_page <= new_top_page + MM_STACK_PAGES_GAP * MM_PAGE_SIZE)) {
        new_top_page = 0;
    }
    /*
     * We need to walk the list of stack allocators until we find an entry after which there is
     * sufficient space left
     */
    if (new_top_page == 0) {
        LIST_FOREACH(address_space[pid].head, sa) {
            /*
             * Determine the limit address below which our stack needs to fit
             */
            limit = sa->next ? sa->next->lowest_page : (MM_VIRTUAL_TOS + 1);
            /*
             * Enough space left between sa->highest_page and limit?
             */
            if ((((limit - sa->highest_page) / MM_PAGE_SIZE) - 1)
                    >= (MM_STACK_PAGES_TASK + 2 * MM_STACK_PAGES_GAP)) {
                new_top_page = sa->highest_page + MM_PAGE_SIZE
                        * (MM_STACK_PAGES_GAP + MM_STACK_PAGES_TASK);
                break;
            }
        }
    }
    if (0 == new_top_page) {
        spinlock_release(st_lock, &eflags);
        return 0;
    }
    /*
     * Initialize new stack allocator
     * and add it to the list
     */
    stack_allocator[task_id].valid = 1;
    stack_allocator[task_id].id = task_id;
    stack_allocator[task_id].lowest_page = new_top_page - (MM_STACK_PAGES_TASK
            - 1) * MM_PAGE_SIZE;
    stack_allocator[task_id].highest_page = new_top_page;
    stack_allocator[task_id].pid = pm_get_pid();
    mm_add_stack_allocator(task_id);
    spinlock_release(st_lock, &eflags);
    return new_top_page;
}

/*
 * This function will try to locate a contiguous area of
 * MM_STACK_PAGES_TASK free pages on the kernel stack of the current process
 * If such an area is found and enough physical memory is available,
 * a pointer to the top of the stack is returned and the number of allocated
 * pages is returned in *pages. The return value is the last byte within the
 * top page of the new stack
 * Search is done from the bottom to the top of the entire available stack
 * region, i.e. the lowest available address is returned
 * MM_STACK_PAGES_GAP pages will be left free between any two allocations
 * Parameter:
 * @task_id - the id of the task which is also used as id of the stack allocator
 * @pid - the pid of the process
 * @pages - buffer at which the number of allocated pages is stored
 * Return value:
 * top of the new stack area or 0 if operation failed
 */
u32 mm_reserve_task_stack(int task_id, int pid, int* pages) {
    u32 kernel_stack_top_page = MM_PAGE_START(MM_PAGE(MM_VIRTUAL_TOS));
    u32 kernel_stack_base_page = kernel_stack_top_page - (MM_STACK_PAGES - 1)
            * MM_PAGE_SIZE;
    u32 new_top_page = 0;
    u32 stack_page_p;
    u32 page;
    /*
     * Find free area on kernel stack
     */
    new_top_page = mm_find_free_stack(kernel_stack_base_page,
            kernel_stack_top_page, task_id);
    if (0 == new_top_page) {
        ERROR("No space left in kernel stack area\n");
        return 0;
    }
    /*
     * Add MM_STACK_PAGES_TASK pages to the virtual memory
     * if necessary
     */
    for (page = new_top_page; new_top_page - page < MM_STACK_PAGES_TASK
            * MM_PAGE_SIZE; page -= MM_PAGE_SIZE) {
        if (!mm_page_mapped(page)) {
            if (0 == (stack_page_p = mm_get_phys_page())) {
                ERROR("No physical pages available\n");
                return 0;
            }
            mm_map_page(mm_get_ptd(), stack_page_p, page, MM_READ_WRITE,
                    MM_SUPERVISOR_PAGE, 0, pm_get_pid());
        }
    }
    *pages = MM_STACK_PAGES_TASK;
    return new_top_page + MM_PAGE_SIZE - 1;
}

/*
 * This function accepts a task id as argument. It uses the stack allocator
 * data structure to determine the area on the kernel stack reserved for this task
 * It will then destroy the stack allocator and remove the stack pages from
 * the virtual address space of the current process, releasing physical memory
 * Parameter:
 * @task_id - the task id for which the stack is to be deallocated
 * Return value:
 * EINVAL if the task id is not valid
 * 0 upon success
 * Locks:
 * st_lock - protect stack allocators of current process
 * Cross-monitor function calls:
 * mm_unmap_page
 *
 * Note that this function will only work if the task is a part of the currently
 * running process.
 *
 * IMPORTANT:
 * this function does not assume that the return value of pm_get_pid
 * is identical to the address space id of the currently active page table set
 * It can therefore be used during a post-irq handler
 *
 */
int mm_release_task_stack(u32 task_id, pid_t pid) {
    spinlock_t* st_lock;
    u32 flags;
    u32 page;
    if (task_id >= PM_MAX_TASK) {
        return EINVAL;
    }
    if (0 == stack_allocator[task_id].valid) {
        return EINVAL;
    }
    /*
     * Get lock to protect stack area
     */
    st_lock = &(mem_locks[pid].st_lock);
    spinlock_get(st_lock, &flags);
    /*
     * Remove stack allocator from list
     */
    LIST_REMOVE(address_space[pid].head, address_space[pid].tail, &(stack_allocator[task_id]));
    /*
     * Unmap pages, then invalidate stack allocator (keep the lock on the stack allocator list,
     * otherwise someone else might jump in, reuse the stack area and map the pages)
     */
    for (page = stack_allocator[task_id].lowest_page; page
            <= stack_allocator[task_id].highest_page; page += 4096)
        mm_unmap_page(mm_get_ptd_for_pid(pid), page, pid);
    stack_allocator[task_id].valid = 0;
    spinlock_release(st_lock, &flags);
    return 0;
}

/*
 * Get the top of the kernel stack of a given task
 * as dword aligned value
 * Parameter:
 * @task_id - id of the task in question
 * Return value:
 * top of the kernel stack of the task (dword aligned)
 */
u32 mm_get_kernel_stack(u32 task_id) {
    if (task_id >= PM_MAX_TASK)
        return 0;
    if (stack_allocator[task_id].valid == 0) {
        return 0;
    }
    return stack_allocator[task_id].highest_page + MM_PAGE_SIZE - 4;
}

/*
 * Validate the address space and stack allocator structures
 * Return value:
 * 0 if address space is valid
 * 1 if address space is not valid
 */
int mm_validate_address_spaces() {
    int i;
    int count;
    stack_allocator_t* sa;
    u32 last;
    for (i = 0; i < PM_MAX_PROCESS; i++) {
        if (address_space[i].valid) {
            if (0 == address_space[i].head) {
                ERROR("Process %d has no list of stack allocators\n", i);
                return 1;
            }
            if (0 == address_space[i].tail) {
                ERROR("Process %d has no list of stack allocators\n", i);
                return 1;
            }
            /*
             * Validate list of stack allocators
             */
            last = 0;
            count = 0;
            LIST_FOREACH(address_space[i].head, sa) {
                count++;
                if (0 == sa->valid) {
                    ERROR("Invalid stack allocator at position %d, process %d\n", count, i);
                    return 1;
                }
                if (sa->highest_page != (sa->lowest_page + MM_PAGE_SIZE
                        * (MM_STACK_PAGES_TASK - 1))) {
                    ERROR("Invalid stack allocator\n");
                    return 1;
                }
                if (sa->lowest_page <= last) {
                    ERROR("List of stack allocators not sorted\n");
                    return 1;
                }
                if (sa->lowest_page - last < (MM_STACK_PAGES_GAP + 1)) {
                    ERROR("Gap not big enough\n");
                    return 1;
                }
                last = sa->highest_page;
            }
        }
    }
    return 0;
}

/****************************************************************************************
 * The following functions are used to clone an existing address space as part of the   *
 * fork system call and to clean up the adress space again during exit processing       *
 ***************************************************************************************/

/*
 * Copy the content of a virtual page in the current process
 * address space into a given physical page
 * Parameters:
 * @virtual_page_base: the virtual address of the source page
 * @physical_page_base: the physical address of the target page
 * Return value:
 * ENOMEM if attaching of temporary page did not work
 * 0 upon success
 */
static int mm_copy_page_impl(u32 virtual_page_base, u32 physical_page_base) {
    u32 target = mm_attach_page(physical_page_base);
    if (0 == target) {
        ERROR("Could not attach page\n");
        return ENOMEM;
    }
    memcpy((void*) target, (void*) virtual_page_base, MM_PAGE_SIZE);
    mm_detach_page(target);
    return 0;
}
int (*mm_copy_page)(u32, u32) = mm_copy_page_impl;

/*
 * Clone a page table, i.e.:
 * for all entries in the user area and kernel stack in the source page table,
 * allocate a new physical page, copy its content
 * from the existing page and set up a mapping in the new page table
 * It uses the following utility functions:
 * - mm_copy_page - copy a page
 * - mm_get_phys_page - allocate an unused physical page
 * Parameters:
 * @source_pt: the source page table to be cloned
 * @target_pt: a pointer to the target page table
 * @pt_base: the lowest virtual address described by the page table, i.e. the first address within the
 * 4 MB address space described by it
 * Return value:
 * ENOMEM if no memory is available for the new page table structures
 * 0 upon success
 */
static int mm_clone_pt(pte_t* source_pt, pte_t* target_pt, u32 pt_base) {
    u32 phys_page;
    u32 page;
    u32 page_base;
    int rc;
    int do_clone = 1;
    u32 page_base_current_stack = stack_allocator[pm_get_task_id()].lowest_page;
    u32 page_top_current_stack = stack_allocator[pm_get_task_id()].highest_page;
    for (page = 0; page < MM_PT_ENTRIES; page++) {
        page_base = pt_base + MM_PAGE_SIZE * page;
        if ((source_pt[page].p == 1) && (page_base < MM_VIRTUAL_TOS)) {
            do_clone = 1;
            /*
             * If the page is within the kernel stack area, only do the clone
             * if the page is within the stack area of the currently active process
             */
            if (page_base > MM_VIRTUAL_TOS_USER)
                if (!((page_base >= page_base_current_stack) && (page_base
                        <= page_top_current_stack)))
                    do_clone = 0;
            if (do_clone) {
                /*
                 * Allocate new physical page and map it into target page table
                 */
                if (0 == (phys_page = mm_get_phys_page())) {
                    ERROR("No physical memory left\n");
                    return ENOMEM;
                }
                target_pt[page] = pte_create(source_pt[page].rw,
                        source_pt[page].us, source_pt[page].pcd, phys_page);
                /* Copy content of original page into new page */
                rc = mm_copy_page(page_base, phys_page);
                if (rc) {
                    return rc;
                }
            }
        }
    }
    return 0;
}

/*
 * Clone an existing set of page tables for a new process
 * This function will perform the following steps
 * - walk through the source page table directory and all mapped page tables within common area and user area
 * - for each page mapping,
 *   - if the page is in the common area, copy the link to the page table into
 *     the target page table directory
 *   - if the page is in the user area or the kernel stack, clone the page, i.e. allocate a new physical
 *     page and copy its contents, create a new page table if necessary and add the mapping accordingly,
 *     but only do this is the page is within the stack area of the currently active task
 * - set up the remaining mappings in the private system area
 * Note that this function does not lock the page table structure - it is within
 * the responsibility of the caller to do this if necessary
 * Parameters:
 * @source_ptd: the source page table directory to be cloned
 * @target_ptd: a pointer to an (already allocated) target page table directory
 * @phys_target_ptd: the physical address of the target page table directory
 * Return value:
 * 0 upon success
 * ENOMEM if no memory is available
 */
int mm_clone_ptd(pte_t* source_ptd, pte_t* target_ptd, u32 phys_target_ptd) {
    int ptd_offset;
    pte_t* target_pt;
    u32 target_pt_phys;
    pte_t* source_pt;
    /*
     * This function only works if paging is already enabled
     */
    KASSERT(1 == (get_cr0()>>31));
    /*
     * First take care of pages within the common area
     */
    memcpy(target_ptd, source_ptd, sizeof(pte_t) * MM_SHARED_PAGE_TABLES);
    /*
     * Now go through all page tables which at least partially belong to the
     * user area or the kernel stack
     * IMPORTANT: we assume that the kernel stack is immediately above the user space stack!
     * This holds true in the current layout but might change in the future
     * so we place an assertion here to be alerted if the condition is no longer met
     */
    KASSERT((MM_VIRTUAL_TOS_USER+1) == (MM_PAGE_START(MM_PAGE(MM_VIRTUAL_TOS)-MM_STACK_PAGES+1)));
    for (ptd_offset = MM_SHARED_PAGE_TABLES; ptd_offset < MM_PT_ENTRIES; ptd_offset++) {
        /*
         * For each mapped page table, check whether the 4 MB area described by the page table belongs
         * at least partially to user space or kernel space, i.e. whether the start of the 4 MB area
         * is located below the top of the kernel stack
         * */
        if ((MM_AREA_START(ptd_offset) <= MM_VIRTUAL_TOS)
                && (source_ptd[ptd_offset].p == 1)) {
            /*
             * Allocate a new page table and attach it to a temporary slot
             */
            if (0 == (target_pt_phys = mm_get_phys_page())) {
                PANIC("Could not get free physical page\n");
                return ENOMEM;
            }
            if (0 == (target_pt = (pte_t*) mm_attach_page(target_pt_phys))) {
                ERROR("Could not attach physical page\n");
                return ENOMEM;
            }
            memset((void*) target_pt, 0, MM_PT_ENTRIES * sizeof(pte_t));
            /*
             * Add new page table to target page table directory
             */
            target_ptd[ptd_offset] = pte_create(source_ptd[ptd_offset].rw,
                    source_ptd[ptd_offset].us, source_ptd[ptd_offset].pcd,
                    target_pt_phys);
            /*
             * Get pointer to source page table and clone page table
             */
            source_pt = mm_get_pt_address(source_ptd, ptd_offset, 1);
            mm_clone_pt(source_pt, target_pt, MM_AREA_START(ptd_offset));
            /*
             * done, detach page again
             */
            mm_detach_page((u32) target_pt);
        }
    }
    /*
     * Now set up the last entry in the PDT
     * to point to itself to map pages tables
     * starting at 0xffc0:0000
     */
    target_ptd[MM_PT_ENTRIES - 1] = pte_create(1, 0, 0, (u32) phys_target_ptd);
    return 0;
}


/*
 * Clone address space and stack allocator data structures
 * Parameters:
 * @task_id - the id of the source task
 * @new_task_id - the id of the target task
 * @new_pid - the id of the new process
 * Locks:
 * address_spaces_lock - lock to protect address space structure
 */
static void mm_clone_address_space(int task_id, int new_task_id, int new_pid) {
    u32 flags;
    /*
     * Clone stack allocator of current process
     */
    spinlock_get(&address_spaces_lock, &flags);
    stack_allocator[new_task_id].id = new_task_id;
    stack_allocator[new_task_id].next = 0;
    stack_allocator[new_task_id].prev = 0;
    stack_allocator[new_task_id].pid = new_pid;
    stack_allocator[new_task_id].highest_page
            = stack_allocator[task_id].highest_page;
    stack_allocator[new_task_id].lowest_page
            = stack_allocator[task_id].lowest_page;
    stack_allocator[new_task_id].valid = 1;
    /*
     * Create new entry in address space table
     */
    address_space[new_pid].id = new_pid;
    address_space[new_pid].head = stack_allocator + new_task_id;
    address_space[new_pid].tail = stack_allocator + new_task_id;
    address_space[new_pid].valid = 1;
    address_space[new_pid].brk = address_space[pm_get_pid()].brk;
    address_space[new_pid].end_data = address_space[pm_get_pid()].end_data;
    spinlock_release(&address_spaces_lock, &flags);
}

/*
 * Clone an entire address space
 * and return the physical address of the new page table directory
 * During the cloning, only one task stack area will be created in the
 * new address space which corresponds to the clone of the currently running
 * task
 * Parameters
 * @new_pid - pid of new process
 * @new_task_id - task id of new task, this will be used as id for the new stack allocator
 * Return value:
 * physical address of the new page table directory or 0 if operation failed
 *  */
u32 mm_clone(int new_pid, int new_task_id) {
    pte_t* new_ptd;
    int rc;
    /*
     * We will place our new page table directory within
     * the address space structure of the new process
     * As this is within the kernel BSS segment and therefore
     * mapped 1-1, this is at the same time the physical address of
     * the new page table directory
     */
    new_ptd = proc_ptd[new_pid];
    memset((void*) new_ptd, 0, sizeof(pte_t) * MM_PT_ENTRIES);
    /*
     * Clone page table directory
     */
    rc = mm_clone_ptd(mm_get_ptd(), new_ptd, (u32) new_ptd);
    if (rc) {
        ERROR("mm_clone_ptd returned with rc=%d\n", rc);
        return 0;
    }
    /*
     * And clone stack allocators and address space data structure
     */
    mm_clone_address_space(pm_get_task_id(), new_task_id, new_pid);
    return (u32) new_ptd;
}

/*
 * This function assumes that all physical pages other
 * than the page tables above the common area have already
 * been unmapped and released. It will remove all page tables above
 * the common area and will therefore lead to a crash if this
 * part of the virtual memory is still be accessed. In particular,
 * interrupts should be disabled when calling this function and
 * the stack should be located in the common area
 *
 * IMPORTANT: this function  might be called
 * during the post-irq handler when the PID has already been updated, but we are still
 * operating in the old address space.
 *
 * Parameters:
 * @pid - the pid of the process for which page tables should be released
 *
 */
void mm_release_page_tables(u32 pid) {
    pte_t* ptd = mm_get_ptd_for_pid(pid);
    int i;
    KASSERT(ptd);
    /*
     * For each entry in the page table directory which
     * does not point to the shared page tables for the common
     * area, release the physical page for the page table and
     * mark the entry as invalid
     */
    for (i = MM_SHARED_PAGE_TABLES; i < MM_PT_ENTRIES; i++) {
        if (ptd[i].p == 1) {
            /*
             * Only release physical memory if the entry does not
             * point to the PTD itself
             */
            if (i != MM_PT_ENTRIES - 1) {
                mm_put_phys_page(ptd[i].page_base * MM_PAGE_SIZE);
            }
            ptd[i].p = 0;
        }
    }
}


/****************************************************************************************
 * Make some information on the layout of the current address space available to other  *
 * parts of the kernel                                                                  *
 ***************************************************************************************/

/*
 * Return true if the passed code segment selector matches
 * the kernel code segment
 * Parameter:
 * @code_segment - code segment to check
 * Return:
 * 1 if the argument is the kernel code selector
 * 0 otherwise
 */
int mm_is_kernel_code(u32 code_segment) {
    return ((code_segment / 8) == (SELECTOR_CODE_KERNEL / 8));
}


/*
 * Return the base address of the initial ramdisk
 */
u32 mm_get_initrd_base() {
    return virt_ramdisk_start;
}

/*
 * Return the highest address within the initial ramdisk
 */
u32 mm_get_initrd_top() {
    return virt_ramdisk_end + MM_PAGE_SIZE - 1;
}

/*
 * Do we have a ramdisk?
 */
int mm_have_ramdisk() {
    return have_ramdisk;
}

/*
 * Return the top of the common kernel stack, aligned to a dword
 * boundary
 */
u32 mm_get_top_of_common_stack() {
    return ((u32) common_kernel_stack) +MM_PAGE_SIZE*MM_COMMON_KERNEL_STACK_PAGES*(smp_get_cpu()+1) - 4;
}


/*
 * Validate that a given pointer points to a valid area of the specified length in user space, i.e. that
 * - all pages touched by this area are mapped in user space
 * - all pages are read/write if requested
 * - the buffer does not reach past the virtual address space
 * Parameter:
 * @buffer - start of buffer
 * @len - length of buffer - 0 means that buffer is a string
 * @read_write - 1 if buffer needs to be writable
 * If @len is zero, the buffer will be considered as a string and validated up to
 * and including a trailing zero
 * Return value:
 * 0 if validation is successful or buffer is NULL
 * -1 if validation failed
 */
int mm_validate_buffer(u32 buffer, u32 len, int read_write) {
    MM_DEBUG("buffer = %x, len = %d, read_write = %d\n", buffer, len, read_write);
    if (0 == buffer) {
        MM_DEBUG("Buffer is null\n");
        return 0;
    }
    int hit_page_end = 0;
    u32 current = buffer;
    u32 page_base = MM_PAGE_START(MM_PAGE(buffer));
    /*
     * If we wrap around, len is invalid
     */
    if ((buffer + len) < buffer) {
        MM_DEBUG("Wrapping around\n");
        return -1;
    }
    /*
     * We now start to visit all pages which are touched by the
     * buffer
     */
    while ((0 == len) || (page_base < buffer + len)) {
        MM_DEBUG("Checking page %x\n", page_base);
        /*
         * Validate mapping of that page
         */
        if (0 == mm_page_mapped(page_base)) {
            MM_DEBUG("Page at %x is not mapped\n", page_base);
            return -1;
        }
        /*
         * If page is mapped, check access
         */
        if (0 == access_allowed(page_base, mm_get_ptd(), 0, read_write)) {
            MM_DEBUG("Page %x: access not allowed\n", page_base);
            return -1;
        }
        /*
         * Move on to next page. For len > 0, we just advance page_base by one page. For
         * the case 0 == len (string buffer), we advance current until we hit upon the page
         * boundary - in which case we set page_base to current - or the end of the string
         */
        if (len) {
            page_base += MM_PAGE_SIZE;
        }
        else {
            MM_DEBUG("current = %x\n", current);
            /*
             * Note that we have validated above that current points to a mapped page, thus it
             * is safe to deference it
             */
            hit_page_end = 0;
            while (*((u8*)current)) {
                current++;
                if (0 == current) {
                    MM_DEBUG("Wrapped around\n");
                    return -1;
                }
                if (0 == (current % MM_PAGE_SIZE)) {
                    page_base = current;
                    hit_page_end = 1;
                    break;
                }
            }
            MM_DEBUG("hit_page_end = %d\n", hit_page_end);
            if (0 == hit_page_end) {
                /*
                 * If we get to this point, we have reached the end of the string without
                 * crossing another page boundary - validation successful
                 */
                MM_DEBUG("String has ended before we reach next page boundary\n");
                return 0;
            }
        }
    }
    return 0;
}

/****************************************************************************************
 * The following set of functions is responsible for maintaining the user space area    *
 * within the virtual address space                                                     *
 ***************************************************************************************/

/*
 * Utility function to allocate a region in user space.
 * The region requested needs to span a multiple of the page
 * size.
 * Parameters:
 * @region_base - the base address of the region to be mapped, must be aligned to a page boundary
 * @region_end - the last byte of the region to be mapped, region_end+1 must be a multiple of the page size
 * Return value:
 * 0 if operation is successful
 * EINVAL if the arguments are not valid
 * ENOMEM if there is not enough memory available
 */
static int add_user_space_pages(u32 region_base, u32 region_end) {
    u32 page;
    u32 phys_page;
    int rc;
    int pid = pm_get_pid();
    /*
     * Validate that start and end of region are page aligned
     */
    if ((region_base % MM_PAGE_SIZE) || ((region_end + 1) % MM_PAGE_SIZE)) {
        ERROR("Invalid alignment, region_base=%x, region_end=%x\n", region_base, region_end);
        return EINVAL;
    }
    /*
     * Validate that we leave at least MM_STACK_PAGES_TASK+1 pages for stack
     */
    if (MM_PAGE(region_end + 1) >= MM_PAGE(MM_VIRTUAL_TOS_USER) - MM_STACK_PAGES) {
        ERROR("Conflict with user stack area\n");
        return EINVAL;
    }
    for (page = region_base; page < region_end + 1; page++) {
        if (!mm_page_mapped(page)) {
            if (0 == (phys_page = mm_get_phys_page())) {
                ERROR("Out of physical memory\n");
                return ENOMEM;
            }
            rc = mm_map_page(mm_get_ptd(), phys_page, page, MM_READ_WRITE,
                    MM_USER_PAGE, 0, pid);
            if (rc) {
                ERROR("mm_map_page returned with error, rc=%d\n", rc);
                return ENOMEM;
            }
        }
    }
    return 0;
}

/*
 * Allocate pages for a region in user space. The region requested needs to span a multiple of the page
 * size. No locking is done, so this should only be used during initialization of a process for execution
 * (do_exec in pm.c resp. the ELF related functions which it calls)
 * Parameters:
 * @region_base - the base address of the region to be mapped, must be aligned to a page boundary
 * @region_end - the last byte of the region to be mapped, region_end+1 must be a multiple of the page size
 * Return value:
 * 0 if operation failed
 * base address of allocated region if operation was successful
 * Locks:
 * lock on current address space
 */
u32 mm_map_user_segment(u32 region_base, u32 region_end) {
    u32 eflags;
    int pid = pm_get_pid();
    /*
     * Validate that we do not load below MM_START_CODE
     */
    if (region_base < MM_START_CODE) {
        ERROR("Trying to load code below %x\n", MM_START_CODE);
        return 0;
    }
    /*
     * Do actual allocation
     */
    if (add_user_space_pages(region_base, region_end)) {
        return 0;
    }
    /*
     * Update address space data if needed
     */
    spinlock_get(&address_space[pid].lock, &eflags);
    if (region_end > address_space[pid].end_data) {
        address_space[pid].end_data = region_end;
        address_space[pid].brk = region_end + 1;
    }
    spinlock_release(&address_space[pid].lock, &eflags);
    return region_base;
}

/*
 * Increase the break of the currently running process. By definition, the break is the first unallocated
 * byte above the user space heap and is always a multiple of the page size. This function will
 * increase the break by at least the specified number of bytes and return the new break. Usually, this is more
 * than requested as the new break will again be page aligned.
 * Parameters:
 * @size - number of bytes requested
 * Return value:
 * 0 - no additional memory could be allocated
 * new break upon success
 * Locks:
 * lock on current address space
 * Cross-monitor function calls:
 * mm_map_page - via add user space pages
 * mm_get_phys_page - via add_user_space_pages
 */
u32 do_sbrk(u32 size) {
    u32 eflags;
    u32 old_brk;
    u32 new_brk;
    int pid = pm_get_pid();
    /*
     * Lock address space
     */
    spinlock_get(&address_space[pid].lock, &eflags);
    /*
     * If size is zero, simply return old break without changing anything
     */
    if (0 == size) {
        spinlock_release(&address_space[pid].lock, &eflags);
        return address_space[pid].brk;
    }
    old_brk = address_space[pid].brk;
    new_brk = old_brk + size;
    if (new_brk % MM_PAGE_SIZE)
        new_brk = (new_brk / MM_PAGE_SIZE) * MM_PAGE_SIZE + MM_PAGE_SIZE;
    /*
     * Do actual allocation
     */
    if (add_user_space_pages(old_brk, new_brk-1)) {
        return 0;
    }
    address_space[pid].brk = new_brk;
    spinlock_release(&address_space[pid].lock, &eflags);
    return new_brk;
}

/*
 * Initialize the user area and allocate MM_STACK_PAGES_TASK pages for the user space stack
 * Note that this function will not unmap any pages. It does not do page table locking and should only
 * be called once per process
 * Return value:
 * the top of the newly allocated stack (aligned to a double word) or 0 if the operation failed
 * Locks:
 * lock on current address space
 */
u32 mm_init_user_area() {
    u32 eflags;
    u32 pid = pm_get_pid();
    u32 page;
    u32 phys_page;
    /*
     * Allocate pages for stack if not yet mapped
     */
    for (page = MM_PAGE(MM_VIRTUAL_TOS_USER) - MM_STACK_PAGES_TASK + 1; page
            <= MM_PAGE(MM_VIRTUAL_TOS_USER); page++) {
        if (!mm_page_mapped(MM_PAGE_START(page))) {
            if (0 == (phys_page = mm_get_phys_page())) {
                ERROR("No physical page left for stack area\n");
                return 0;
            }
            mm_map_page(mm_get_ptd(), phys_page, MM_PAGE_START(page),
                    MM_READ_WRITE, MM_USER_PAGE, 0, pid);
        }
    }
    /*
     * Reset user area memory layout to default values
     */
    spinlock_get(&address_space[pid].lock, &eflags);
    address_space[pid].end_data = MM_START_CODE-1;
    address_space[pid].brk = MM_START_CODE;
    spinlock_release(&address_space[pid].lock, &eflags);
    return (MM_VIRTUAL_TOS_USER / 4) * 4;
}

/*
 * Remove all mappings for the user area of the currently active
 * address space, i.e. unmap all pages between the end of the common
 * area and the top of the user space stack area
 */
void mm_teardown_user_area() {
    u32 page;
    pte_t* ptd;
    /*
     * Get pointer to page table directory
     */
    ptd = mm_get_ptd();
    KASSERT(ptd);
    /*
     * Walk all pages between end of common area and end of user
     * space stack and remove mapping if needed
     */
    for (page = MM_COMMON_AREA_SIZE; page < MM_VIRTUAL_TOS_USER; page += 4096) {
        if (mm_page_mapped(page)) {
            mm_unmap_page(ptd, page, pm_get_pid());
        }
    }
}

/****************************************************************************************
 * Allow drivers to map physical memory into the area within the virtual address space  *
 * reserved for memory mapped I/O                                                       *
 ***************************************************************************************/

/*
 * Map a given region in physical memory into the virtual
 * memory area reserved for memory mapped I/O, setting the
 * page cache disabled bit to one. All pages are mapped as
 * supervisor pages and for read/write.
 * If no contiguous area of the required size is found in
 * the virtual memory area reserved for memory mapped I/O,
 * 0 is returned. No locking is done, as this function is
 * only supposed to be called during system initialization
 * Parameters:
 * @phys_base - physical base address, needs to be page aligned
 * @size - size of region in bytes to be mapped
 * Return value:
 * 0 if no free virtual page could be found
 * the base address of the allocated region in virtual memory otherwise
 */
u32 mm_map_memio(u32 phys_base, u32 size) {
    u32 pages = (size / MM_PAGE_SIZE)+1;
    int rc;
    int walk;
    u32 base;
    u32 virtual;
    /*
     * First walk area between MM_MEMIO_START
     * and MM_MEMIO_END until we find a region
     * which is not yet mapped and large enough
     * The variable walk is the length of the largest
     * free area in pages found so far, base is its base
     */
    base = 0;
    virtual = MM_MEMIO_START;
    walk = 0;
    while (virtual < MM_MEMIO_END) {
        if (!mm_page_mapped(virtual)) {
            /*
             * Continue walk
             */
            walk++;
            /*
             * If walk has just started, set base address
             */
            if (!base)
                base = virtual;
        }
        else {
            /*
             * Bad luck this time - abort walk
             */
            walk = 0;
            base = 0;
        }
        /*
         * We have successfully found an area which is long enough
         */
        if (walk>=pages)
            break;
        virtual +=MM_PAGE_SIZE;
    }
    if (walk < pages)
        return ENOMEM;
    /*
     * Now base is the base address of the area which we map
     */
    u32 page = base;
    while (page-base < pages*MM_PAGE_SIZE) {
        /*
         * We do not set the page cache disable bit at this point even for memory mapped I/O because we assume that
         * the BIOS has set up the MTRRs for us properly
         */
        rc = mm_map_page(mm_get_ptd(), phys_base+(page-base), page, MM_READ_WRITE, MM_SUPERVISOR_PAGE, 0, pm_get_pid());
        if (rc) {
            ERROR("mm_map_page returned with error code, rc=%d\n", rc);
            return 0;
        }
        page+=MM_PAGE_SIZE;
    }
    return base;
}


/****************************************************************************************
 * Utility functions to check access rights and handle page faults                      *
 ***************************************************************************************/

/*
 * This utility function accepts a linear address (which is supposed to be mapped) and
 * checks whether the specified access is allowed based on its page table entry
 * Parameters:
 * @virtual_address - virtual address to check
 * @ptd - page table directory
 * @sv - set this to 1 if access is done in supervisor mode
 * @rw - set this to 1 if access is done via a write
 */
static int access_allowed(u32 virtual_address, pte_t* ptd, int sv, int rw) {
    pte_t* pt;
    pte_t* pte;
    int rc = 1;
    MM_DEBUG("Using PTD at %x\n", ptd);
    /*
     * Get pointer to page table
     */
    pt = mm_get_pt_address(ptd, PTD_OFFSET(virtual_address), 1);
    if (0 == pt) {
        PANIC("Page table not mapped\n");
    }
    /*
     * Get page table entry
     */
    pte = pt+PT_OFFSET(virtual_address);
    MM_DEBUG("Address of page table entry is %x\n", pte);
    /*
     * Now check all flags. We assume that the WP bit in CR0 is set. Thus access
     * is forbidden if either
     * a) a write is requested, but the entry is read-only
     * b) an access in user-mode is requested, but the page is mapped with the us-flag set to 0
     */
    MM_DEBUG("pte->rw = %d, rw = %d, pte->us = %d, sv = %d\n", pte->rw, rw, pte->us, sv);
    if ((1 == rw) && (0 == pte->rw))
        rc = 0;
    if ((0 == sv) && (0 == pte->us))
        rc = 0;
    return rc;
}

/*
 * Page fault handler
 *
 * This handler is invoked by the interrupt manager if a page fault (#14) is encountered. It
 * determines the reason for the page fault and performs the following actions:
 *
 * 1) if the error is due to the usage of reserved bits in the page table,
 *    panic
 * 2) if the error has been caused by an instruction fetch, send SIGSEV to
 *    the currently running process and return
 * 3) if the page is not mapped, then
 *    a) panic if the error occurred in kernel mode
 *    b) send SIGSEV to the process and return if the error occurred in user
 *       mode
 * 4) in all other cases, get the page table entry for the specified linear
 *    address and determine whether the entry matches the access. If yes,
 *    do invlpg and return. If no, panic
 *
 */
int mm_handle_page_fault(ir_context_t* ir_context) {
    int write_error = 0;
    int page_missing = 0;
    int supervisor_mode = 0;
    int reserved_bits = 0;
    int instruction_fetch = 0;
    int allowed = 0;
    u32 address = ir_context->cr2;
    /*
     * First determine the type of error using the following
     * bits of the error register:
     * Bit 0: set if the page table entry was present for the page
     * Bit 1: set if the error was caused by a write
     * Bit 2: clear if CPU was in supervisor mode
     * Bit 3: set if the CPU detected a reserved bit being 1 in the PD
     * Bit 4: set if the error was caused by an instruction fetch
     */
    page_missing = (ir_context->err_code & 0x1) ? 0 : 1;
    write_error = (ir_context->err_code & 0x2) >> 1;
    supervisor_mode = (ir_context->err_code & 0x4) ? 0 : 1;
    reserved_bits = (ir_context->err_code & 0x8) >> 3;
    instruction_fetch = (ir_context->err_code & 0x10) >> 4;
    MM_DEBUG("PF@%x: page_missing = %d, write_error = %d, sv_mode = %d\nreserved_bits = %d, instruction_fetch = %d\n", address, page_missing, write_error, supervisor_mode, reserved_bits, instruction_fetch);
    if (reserved_bits) {
        MM_DEBUG("Page fault handler: detected illegal state of page table entry, reserved bits are in use\n");
        return 1;
    }
    else if (instruction_fetch) {
            MM_DEBUG("PF due to instruction fetch, killing process\n");
            do_kill(pm_get_pid(), __KSIGSEGV);
            return 0;
    }
    /*
     * We now check whether the page is mapped. Note that we do not lock the page tables. Hence it
     * is possible that we read the page table, find that the page is mapped, invalidate the TLB and
     * proceed while some other thread unmaps the page. However, even if this happens, the next page
     * fault will correctly lead to a SIGSEV
     */
    else if (0 == mm_page_mapped(address)) {
        MM_DEBUG("Page not mapped\n");
        if (supervisor_mode) {
            PRINT("Unmapped page in kernel mode, CR2 = %x, EIP = %x, CR3 = %x\n", ir_context->cr2, ir_context->eip, ir_context->cr3);
            debug_main(ir_context);
            PANIC("Debugger returned from PF exception\n");
        }
    }
    else {
        /*
         * Check whether access is allowed
         */
        allowed = access_allowed(address, (pte_t*) ir_context->cr3, supervisor_mode, write_error);
        if (1 == allowed) {
            invlpg(address);
            return 0;
        }
        return 1;
    }
    return 0;
}

/*
 * Return the amount of physical memory in the machine in kB
 */
u32 mm_phys_mem() {
    return phys_mem_layout.total*4;
}

/*
 * Return the amount of available physical RAM in kb
 */
u32 mm_phys_mem_available() {
    return phys_mem_layout.available*4;
}

/***************************************************************
 * Everything below this line is for debugging only            *
 **************************************************************/

extern void (*debug_getline)(void* c, int n);

/*
 * Print a list of all stack allocators
 * This function is mainly used for debugging purposes
 */
void mm_print_stack_allocators() {
    int i;
    char c[2];
    int count = 0;
    stack_allocator_t* sa;
    u32 kernel_stack_top_page = MM_PAGE_START(MM_PAGE(MM_VIRTUAL_TOS));
    u32 kernel_stack_base_page = kernel_stack_top_page - (MM_STACK_PAGES - 1)
            * MM_PAGE_SIZE;
    PRINT("Kernel stack base page: %x\n", kernel_stack_base_page);
    PRINT("Kernel stack top page:  %x\n", kernel_stack_top_page);
    PRINT("End of user stack:      %x\n", MM_VIRTUAL_TOS_USER);
    PRINT("Pages on kernel stack:  %d\n", (kernel_stack_top_page-kernel_stack_base_page) / MM_PAGE_SIZE+1);
    PRINT("Task ID     PID     First page    Last page\n");
    PRINT("-------------------------------------------\n");
    for (i = 0; i < PM_MAX_TASK; i++) {
        if (stack_allocator[i].valid == 1) {
            sa = stack_allocator + i;
            PRINT("%w        %w    %x     %x\n", sa->id, sa->pid, sa->lowest_page, sa->highest_page );
            count++;
            if (0 == (count % 8)) {
                PRINT("Hit ENTER to see next page\n");
                count = 0;
                debug_getline(c, 1);
                PRINT("Task ID     PID     First page    Last page\n");
                PRINT("-------------------------------------------\n");
            }
        }
    }
}

/*
 * Print some information on the layout of the virtual memory
 */
void mm_print_vmem() {
    PRINT("End of kernel BSS section:        %x\n", mm_get_bss_end());
    if (mm_have_ramdisk()) {
        PRINT("RAMDISK:                          %x - %x\n", mm_get_initrd_base(), mm_get_initrd_top());
    }
    PRINT("Kernel heap:                      %x - %x\n", kernel_heap.start, kernel_heap.current_top);
    PRINT("MMIO area:                        %x - %x\n", MM_MEMIO_START, MM_MEMIO_END);
    PRINT("End of common system area:        %x\n", MM_COMMON_AREA_SIZE-1);
    PRINT("Top of default user space stack:  %x\n", MM_VIRTUAL_TOS_USER);
    PRINT("Top of kernel stack area:         %x\n", MM_VIRTUAL_TOS);
}

/*
 * Print out some information on the layout of physical memory and
 * perform basic validations
 */
void mm_print_pmem() {
    u32 page = 0;
    u32 i, j;
    int count;
    while (page < phys_mem_layout.kernel_end) {
        if (!BITFIELD_GET_BIT(phys_mem, MM_PAGE(page))) {
            PRINT("WARNING: page %x not marked as used, although it is below the end of the kernel BSS section\n", page);
        }
        page += MM_PAGE_SIZE;
    }
    PRINT("Physical memory layout:\n");
    PRINT("-------------------------\n");
    PRINT("Start of kernel code:         %x\n", phys_mem_layout.kernel_start);
    PRINT("End of kernel BSS section:    %x\n", phys_mem_layout.kernel_end);
    PRINT("Start of RAMDISK:             %x\n", phys_mem_layout.ramdisk_start);
    PRINT("End of RAMDISK:               %x\n", phys_mem_layout.ramdisk_end);
    PRINT("Top of physical memory:       %x (%d MB)\n", phys_mem_layout.mem_end, phys_mem_layout.mem_end/(1024*1024));
    PRINT("Available physical memory:    %d pages (%d MB)\n", phys_mem_layout.available, (phys_mem_layout.available*4)/1024);
    PRINT("Available low memory:         %d kB\n", *((u16*) 0x413));
    PRINT("\n\nPage table usage per process (w/o common area):\n");
    PRINT("PID         # of allocated page tables\n");
    PRINT("--------------------------------------\n");
    for (i = 0; i < PM_MAX_PROCESS; i++) {
        count = 0;
        for (j = MM_SHARED_PAGE_TABLES; j < MM_PT_ENTRIES; j++) {
            if (proc_ptd[i][j].p == 1) {
                count++;
                if (!BITFIELD_GET_BIT(phys_mem, proc_ptd[i][j].page_base))
                    PRINT("WARNING: page table entry %d for process %d points to unreserved memory\n", j, i);
            }
        }
        if (count)
            PRINT("%w        %d\n", i, count);
    }
}

/*
 * Do some validations. Return 0 upon success
 */
int mm_validate() {
    u32 page;
     /*
      * Verify that all pages in the ramdisk have been mapped
      */
     if (virt_ramdisk_end > virt_ramdisk_start) {
         for (page = virt_ramdisk_start; page <= virt_ramdisk_end; page+=MM_PAGE_SIZE) {
             if (0 == mm_page_mapped(page)) {
                 ERROR("Page %x is within ram disk area (%x - %x) but not mapped, something went wrong\n", page, virt_ramdisk_start,
                         virt_ramdisk_end);
                 return 1;
             }
         }
     }
     return 0;
}

/*
 * Test function
 * This test will map two virtual pages to the same physical page
 * and then write to both of them to check that their content is equal
 */
#ifdef DO_PAGING_TEST
void mm_do_paging_test() {
    u32 virt1 = 0x20000000;
    u32 virt2 = virt1 + MM_PAGE_SIZE;
    u32 ptd_base = (u32) mm_get_ptd();
    u32 phys;
    int i;
    PRINT("Memory manager: starting paging test...");
    phys = mm_get_phys_page();
    if (0 == phys)
    PANIC("Could not get physical page\n");
    mm_map_page((pte_t*) ptd_base, phys, virt1, 1, 0, 0, 0);
    mm_map_page((pte_t*) ptd_base, phys, virt2, 1, 0, 0, 0);
    for (i = 0; i < 256; i++)
    *((unsigned char*) virt1 + i) = i;
    for (i = 0; i < 256; i++)
    if (*((unsigned char*) virt2 + i) != i) {
        PRINT("Mismatch at offset %x, paging test failed\n", i);
    }
    PRINT("success, removing pages again\n");
    mm_unmap_page((pte_t*) ptd_base, virt1, 0);
    mm_unmap_page((pte_t*) ptd_base, virt2, 0);
    return;
}
#endif

#ifdef DO_PHYS_PAGES_TEST
void do_phys_pages_test() {
    u32 first;
    u32 second;
    PRINT("Testing allocation of physical pages...");
    first = mm_get_phys_page();
    second = mm_get_phys_page();
    if (first + MM_PAGE_SIZE == second)
    PRINT("success\n");
    else
    PRINT("failure\n");
}
#endif

#ifdef DO_KHEAP_TEST
void mm_do_kheap_test() {
    void* mem;
    PRINT("Testing aligned version of kmalloc...");
    mem = kmalloc_aligned(100, 256);
    if (mem && (0 == (((u32) mem) % 256)))
    PRINT("success\n");
    else {
        PRINT("failure\n");
        PANIC("Test of kernel heap failed\n");
    }
    kfree((void*) mem);
    PRINT("Testing allocating a large area (32 MB)\n");
    mem = kmalloc(32*1924*1024);
    KASSERT(mem);
    kfree((void*) mem);
}
#endif

#ifdef DO_ATTACH_TEST
void mm_do_attach_test() {
    u32 phys_page;
    u32 virt1;
    u32 virt2;
    int i;
    phys_page = mm_get_phys_page();
    PRINT("Using page %p for test of mm_attach_page...", phys_page);
    virt1 = mm_attach_page(phys_page);
    KASSERT(virt1);
    virt2 = mm_attach_page(phys_page);
    KASSERT(virt2);
    /* Write into page via virt1 */
    for (i = 0; i < 1024; i++)
    ((u32*) virt1)[i] = i;
    /* Read via virt2 */
    for (i = 0; i < 1024; i++)
    KASSERT(((u32*)virt2)[i]==i);
    mm_detach_page(virt1);
    mm_detach_page(virt2);
    /* Check that if we now allocate again, we get virt1 back */
    virt2 = mm_attach_page(phys_page);
    KASSERT(virt1==virt2);
    mm_detach_page(virt2);
    PRINT("success\n");
}
#endif
