/*
 * mm.h
 */

#ifndef _MM_H_
#define _MM_H_

#include "ktypes.h"
#include "irq.h"
#include "tests.h"
#include "pagetables.h"
#include "locks.h"
#include "multiboot.h"


/*
 * This structure is filled at boot time and contains
 * memory layout information which is not known at compile time
 */

typedef struct {
    u32 kernel_start;       // start of kernel code
    u32 kernel_end;         // end of kernel data and BSS section
    u32 mem_end;            // end of physical memory
    u32 ramdisk_start;      // start of physical memory for RAMDISK
    u32 ramdisk_end;        // end of physical memory for RAMDISK
    u32 available;          // number of physical pages which are available
    u32 total;              // total number of pages in the system
} phys_mem_layout_t;

/*
 * The number of entries in a page table
 * or a page table directory
 */
#define MM_PT_ENTRIES 1024

typedef pte_t ptd_t[MM_PT_ENTRIES];

/*
 * This structure describes an allocated area on the stack
 * The id is always equal to the id of the respective task
 */
typedef struct _stack_allocator_t {
    u32 id;                    // id of the respective task
    u32 valid;                 // is this entry in the tables valid
    u32 lowest_page;           // base address of lowest page
    u32 highest_page;          // base address of highest page
    u32 pid;                   // id of the process to which we belong
    struct _stack_allocator_t* next;
    struct _stack_allocator_t* prev;
} stack_allocator_t;

/*
 * Within the memory manager, this structure describes an address space
 * aka process. The address space ID is always equal to the process ID
 * For each address space, there is a linked list of stack space allocators
 */
typedef struct {
    u32 id;                      // id - always equal to process ID
    u32 valid;                   // indicates whether this table slot is valid
    u32 brk;                     // current program break, i.e. first byte above heap
    u32 end_data;                // last byte of data section, including BSS
    stack_allocator_t* head;     // head of stack allocator queue
    stack_allocator_t* tail;     // tail of stack allocator queue
    spinlock_t lock;             // lock to protect against concurrent access
} address_space_t;



/*
 * This structure defines the locks used to protect the page mapping of a process
 */
typedef struct {
    spinlock_t pt_lock; // lock to protect the page tables of the process
    spinlock_t sp_lock; // lock to protect the special pages of the process
    spinlock_t st_lock; // lock to protect the free kernel stack pages of the process
} mem_locks_t;

/*
 * Types of entries in the memory map passed by the boot loader
 */
#define MMAP_ENTRY_TYPE_FREE 1

/*
 * Page size
 */
#define MM_PAGE_SIZE 4096

/*
 * Start and end address of a page
 * The end address is by definition
 * the last address in the page
 */
#define MM_PAGE_START(page) (((page)*MM_PAGE_SIZE))
#define MM_PAGE_END(page) (((page)+1)*MM_PAGE_SIZE-1)

/*
 * Start of a 4 MB area which is described by the
 * page table at offset x in the PTD
 */
#define MM_AREA_START(page) (((page)*MM_PAGE_SIZE*MM_PT_ENTRIES))

/*
 * Page in which an address is contained
 */
#define MM_PAGE(address) (((address) / MM_PAGE_SIZE))

/*
 * 4 MB in which an address is contained
 */
#define MM_AREA(address) (((address) / (MM_PAGE_SIZE*MM_PT_ENTRIES)))

/*
 * Align a given address to the next page boundary
 */
#define MM_ALIGN(x) (MM_PAGE_END(MM_PAGE(x))+1)

/*
 * The physical mem size in pages
 * Note that this is not the actual memory size of the machine,
 * but the theoretical maximum we are supposed to deal with
 */
#define MM_PHYS_MEM_PAGES (( 0xffffffff / MM_PAGE_SIZE))

/*
 * The first address of high memory. We do not try to reserve pages
 * below this address
 */
#define MM_HIGH_MEM_START 0x100000


/*
 * The number of page tables in the lower
 * area of virtual memory which are shared between
 * all processes to realize the common area
 * Multiply this number by the memory area controlled
 * by a page table to get the maximum size of the
 * common area
 */
#define MM_SHARED_PAGE_TABLES 32
#define MM_COMMON_AREA_SIZE ((MM_SHARED_PAGE_TABLES*MM_PAGE_SIZE*MM_PT_ENTRIES))

/*
 * Start of code segment for user space programs
 */
#define MM_START_CODE (0x40000000)

/*
 * The number of page tables (4 MB) within the common area reserved for memory mapped I/O
 * and DMA buffers - so this must always be smaller than MM_SHARED_PAGE_TABLES. The remainder
 * of the shared page tables is available for the common kernel heap
 * Similarly to the common area, this can be multiplied by the amount of memory controlled
 * by one page table to obtain the size of the memory mapped I/O area
 */
#define MM_MEMIO_PAGE_TABLES 1
#define MM_MEMIO_SIZE ((MM_MEMIO_PAGE_TABLES*MM_PAGE_SIZE*MM_PT_ENTRIES))

/*
 * The start and the end of the memory reserved for memory mapped I/O -
 * this is computed from the constants above
 */
#define MM_MEMIO_START ((MM_SHARED_PAGE_TABLES - MM_MEMIO_PAGE_TABLES)* MM_PAGE_SIZE * MM_PT_ENTRIES)
#define MM_MEMIO_END (MM_COMMON_AREA_SIZE-1)

/*
 * As we have mapped the last entry in the page table
 * directory to the physical address of the page table directory
 * itself, the following macro expands to the virtual address of word w
 * in page table pt, 0<=x<1024, 0<=y<256.
 */
#define MM_VIRTUAL_PT_ENTRY(pt,w)  ((0xffc00000+(pt*MM_PAGE_SIZE)+w*4))

/*
 * Reserved pages between top of kernel stack and PTD
 */
#define MM_RESERVED_PAGES 8

/*
 * Address of top of kernel stack in virtual address space
 */
#define MM_VIRTUAL_TOS (((0xffc00000)-1-MM_PAGE_SIZE*MM_RESERVED_PAGES))

/*
 * Number of pages which the kernel stack has. Note that this is the total number
 * of pages which all tasks within that address space need to share
 */
#define MM_STACK_PAGES 128

/*
 * Number of pages for the common kernel stack
 */
#define MM_COMMON_KERNEL_STACK_PAGES 1

/*
 * Number of pages within the kernel stack area which a single task has
 * by default
 */
#define MM_STACK_PAGES_TASK 4
/*
 * Number of pages which we leave empty between two consecutive
 * stacks for two tasks within the same process
 */
#define MM_STACK_PAGES_GAP 2

/*
 * Address of top of user stack
 */
#define MM_VIRTUAL_TOS_USER ((MM_VIRTUAL_TOS - MM_PAGE_SIZE*(MM_STACK_PAGES)))

/*
 * Number of bytes which we can assume at least for RAM disk and kernel heap
 */
#define MIN_HEAP_BYTES (32*1024*1024)

/*
 * Determine offset of a given page in the page table directory
 * and in the page table
 */
#define PTD_OFFSET(x) ((x >> 22))
#define PT_OFFSET(x)  (((x >> 12) & 1023))


/*
 * Protection flags
 */
#define MM_READ_ONLY 0
#define MM_READ_WRITE 1
#define MM_SUPERVISOR_PAGE 0
#define MM_USER_PAGE 1



void mm_init(u32 info_block_ptr);
void mm_init_heap();
void mm_release_page_tables(u32 pid);
void* kmalloc_aligned(u32 size, u32 alignment);
void* kmalloc(u32 size);
void kfree(void* ptr);
u32 mm_virt_to_phys(u32 virtual);
int mm_is_kernel_code(u32 code_segment);
u32 mm_reserve_task_stack(int task_id, int pid, int* pages);
int mm_release_task_stack(u32 task_id, pid_t pid);
void mm_print_stack_allocators();
u32 mm_clone(int pid, int new_task_id);
int mm_have_ramdisk();
u32 mm_get_initrd_base();
u32 mm_get_initrd_top();
u32 mm_map_user_segment(u32 region_base, u32 region_end);
u32 mm_init_user_area();
void mm_teardown_user_area();
u32 mm_get_kernel_stack(u32 task_id);
void mm_print_vmem();
void mm_print_pmem();
u32 mm_map_memio(u32 phys_base, u32 size);
u32 do_sbrk(u32 size);
u32 mm_get_top_of_common_stack();
int mm_validate_buffer(u32 buffer, u32 len, int read_write);
int mm_handle_page_fault(ir_context_t* ir_context);
int mm_validate();
u32 mm_phys_mem();
u32 mm_phys_mem_available();


#ifdef DO_PAGING_TEST
void mm_do_paging_test();
#endif

#ifdef DO_PHYS_PAGES_TEST
void do_phys_pages_test();
#endif

#ifdef DO_KHEAP_TEST
void mm_do_kheap_test();
#endif

#ifdef DO_ATTACH_TEST
void mm_do_attach_test();
#endif

#endif /* _MM_H_ */

