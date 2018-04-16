/*
 * gdt.c
 *
 * This module contains low-level functions to manipulate the GDT
 */


#include "gdt.h"
#include "tss.h"
#include "mm.h"
#include "lib/string.h"
#include "smp.h"

/*
 * Reserve space for the GDT.
 * We use the attribute section to
 * force GCC to put these symbols into the code section
 * to give us some more control on the location of the GDT
 */
static gdt_entry_t  gdt[NR_GDT_ENTRIES];

/*
 * GDT pseudo descriptor
 * this will later be loaded into the
 * 48-Bit register GDTR
 */
static gdt_ptr_t gdt_ptr;

/*
 * TSS. We need one TSS for each CPU. As the TSS needs to be aligned on a 1kByte boundary,
 * we need to reserve an area of SMP_MAX_CPU * 1024 bytes for this
 */
static u8 __attribute__ ((aligned(1024))) tss_area[SMP_MAX_CPU][1024];

/*
 * Create a GDT entry
 * @base: start of segment
 * @limit: 20 bit limit field, specifies segment size in pages of 4096
 * @dpl: descriptor privilege level
 * @code : code segment (1) or data segment (0)
 * @expansion : expansion flag for data segment
 * @read : for code segment: read access enabled, for data segment: not used
 * @write: for code segment: not used, for data segment: write enable
 * The other flags in the GDT are set as follows
 * accessed = 0
 * default operation size d = 1
 * conforming = 0
 * granularity = 1
 * long mode flag = 0
 * present = 1
 * descriptor type = 1 (code/data)
 * Return value:
 * the newly created GDT entry
 */
gdt_entry_t gdt_create_entry(u32 base, u32 limit, u8 dpl, u8 code, u8 expansion, u8 read, u8 write) {
    gdt_entry_t entry;
    entry.accessed = 0;
    entry.avl = 0;
    entry.base_12 = (u16) base;
    entry.base_3 = (u8)  (base >> 16);
    entry.base_4 = (u8)  (base >> 24);
    entry.cd = code;
    entry.d = 1;
    entry.dpl = dpl;
    if (0==code)
        entry.expansion = expansion;
    else
        entry.expansion = 0;
    entry.g = 1;
    entry.l = 0;
    entry.limit_12 = (u16) limit;
    entry.limit_3 = (u8) (limit >> 16) & 0x0f;
    entry.p = 1;
    if (0==code)
        entry.rw = write;
    else
        entry.rw = read;
    entry.s = 1;
    return entry;
}

/*
 * Create a TSS entry for the GDT
 * Parameter:
 * @tss_address - physical address of TSS
 * Return value:
 * the new GDT entry
 */
gdt_entry_t gdt_create_tss(u32 tss_address) {
    gdt_entry_t entry = gdt_create_entry(tss_address, 1, 0, 1, 0, 0, 0);
    /*
     * Overwrite a few values
     */
    entry.s = 0;
    entry.accessed = 1;
    entry.d = 0;
    return entry;
}

/*
 * Put new value for ESP0 into TSS
 * Parameter:
 * @esp0 - new value for ESP0
 * @cpuid - the logical CPUID, starting with 0 for the BSP
 */
void gdt_update_tss(u32 esp0,  int cpuid) {
    tss_t* tss = (tss_t*) tss_area[cpuid];
    tss->esp0 = esp0;
    tss->ss0 = SELECTOR_STACK_KERNEL;
}

/*
 * Set up GDT in memory
 * and return a pointer to the GDT pointer structure
 * for usage with the LGDT instruction
 * Return value:
 * physical address of GDT pointer structure
 */
u32 gdt_get_table() {
    int cpu;
    tss_t* tss;
    /*
     * Prepare GDT entries
     * first initialize with zeros
     * which will also create the null descriptor
     */
    memset((void*) gdt, 0, NR_GDT_ENTRIES*sizeof(gdt_entry_t));
    /*
     * prepare the GDT entry for the kernel code
     * Remember that the limit within the GDT is only 20 bits
     * which is filled up by the CPU with 0xfff at the right
     * if the granularity of the segment is 1
     */
    gdt[SELECTOR_CODE_KERNEL / 8] = gdt_create_entry(0, 0xfffff, 0, 1, 0, 1, 0);
    /* data segment for kernel */
    gdt[SELECTOR_DATA_KERNEL / 8] = gdt_create_entry(0, 0xfffff,0, 0, 0, 0, 1);
    /* and stack. Use limit 0 as we set the expansion bit  */
    gdt[SELECTOR_STACK_KERNEL / 8] = gdt_create_entry(0, 0x0, 0, 0, 1, 0, 1);
    /*
     * Do the same for user space
     */
    gdt[SELECTOR_CODE_USER / 8] = gdt_create_entry(0, 0xfffff, 3, 1, 0, 1, 0);
    gdt[SELECTOR_DATA_USER / 8] = gdt_create_entry(0, 0xfffff,3, 0, 0, 0, 1);
    gdt[SELECTOR_STACK_USER / 8] = gdt_create_entry(0, 0x0, 3, 0, 1, 0, 1);
    /*
     * Finally create a code segment for 16 bit mode
     */
    gdt[SELECTOR_CODE_16 / 8] = gdt_create_entry(0, 0xfffff, 0, 1, 0, 1, 0);
    gdt[SELECTOR_CODE_16 / 8].d = 0;
    /*
     * TSS segment
     */
    for (cpu = 0; cpu < SMP_MAX_CPU; cpu++) {
        tss = (tss_t*) tss_area[cpu];
        gdt[(SELECTOR_TSS+cpu*8) / 8] = gdt_create_tss((u32) tss);
        tss->io_map_offset = sizeof(tss_t);
    }
    /*
     * CPU specific data segment. This needs to be accessible in ring 3 as well
     * as otherwise, the CPU will silently put zero into GS when we switch to
     * ring 3
     */
    for (cpu = 0; cpu < SMP_MAX_CPU; cpu++) {
        gdt[(SELECTOR_CODE_16+SMP_MAX_CPU*8+(cpu+1)*8) / 8] = gdt_create_entry(0, 0xfffff,3, 0, 0, 0, 1);
    }
    /* now create gdt pseudo descriptor */
    gdt_ptr.limit = sizeof(gdt_entry_t) * NR_GDT_ENTRIES;
    gdt_ptr.base = (u32) gdt;
    /* and return pointer to it */
    return (u32) (&gdt_ptr);
}

/*
 * Get gdt pseudo-descriptor address. Only call this if gdt_get_table
 * has already been executed successfully, as it will only access
 * the cached value.
 *
 */
gdt_ptr_t* gdt_get_ptr() {
    return &gdt_ptr;
}
