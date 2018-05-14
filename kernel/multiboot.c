/*
 * multiboot.c
 * 
 * This module is responsible for parsing the multiboot information at startup and
 * making this information available to other modules
 * 
 * At boot time, we go through the following stages:
 * - the first stage starts when we call multiboot_init - then the part of the information which is not dynamic and 
 *   does therefore not require kmalloc is extracted and saved
 * - the second stage starts after the memory manager has been set up. We then call multiboot_clone to clone all the other
 *   information that we might need, now using dynamic data structures and kmalloc
 */
 
 
 
#include "multiboot.h"
#include "debug.h"
#include "kprintf.h"
#include "lib/string.h"
#include "io.h"
#include "vga.h"
#include "acpi.h"

/*
 * Forward declarations
 */
static void parse_multiboot(u32 multiboot_info_ptr);
static void parse_multiboot2(u32 multiboot_info_ptr);

/*
 * Keep track of the stage we are in
 */
static u32 __multiboot_stage = 0;

/*
 * Some data that we store during the first stage
 */
static u32 magic = 0;
static u32 multiboot_version = 0;
static char cmdline[MULTIBOOT_MAX_CMD_LINE];
static mb1_info_block_t* mb1_info_block = 0;
static mb1_memory_map_entry_t* mb1_memory_map_entry_next = 0;
static mb1_memory_map_entry_t* mb1_memory_map_entry_start = 0;
static u32 mb1_memory_map_length = 0;
static u32 mb2_rd_start = 0;
static u32 mb2_rd_end = 0;
static mb2_memory_map_entry_t* mb2_memory_map_entry_start = 0;
static int mb2_memory_map_entries = 0;
static int mb2_memory_map_index = 0;
static int mb2_have_fb = 0;
static mb2_mbi_tag_fb_t mb2_fb_tag;
static acpi_rsdp_t* acpi_rsdp = 0;
static acpi_rsdp_t acpi_rsdp_copy;
static int vbe_startup_mode = -1;

/*
 * Call this at early boot time to do the 
 * first part of initialization
 * Parameter:
 * @multiboot_info_ptr - a pointer to the multiboot information structure
 * @magic - the magic value stored in EAX by the boot loader
 */
void multiboot_init(u32 multiboot_info_ptr, u32 __magic) {
    if (__multiboot_stage != MB_STAGE_NOT_READY) {
        PANIC("Wrong multiboot stage\n");
    }
    magic = __magic;
    switch (magic) {
        case MB_MAGIC_V1: 
            multiboot_version = 1;
            parse_multiboot(multiboot_info_ptr);
            break;
        case MB_MAGIC_V2:
            multiboot_version = 2;
            parse_multiboot2(multiboot_info_ptr);
            break;
        default:
            PANIC("Unknown multiboot magic value %x\n", magic);
    }
    __multiboot_stage = MB_STAGE_EARLY;
}


/*
 * This function is called by the kernel mainline to indicate that
 * the memory occupied by the multiboot information structure could
 * be reused and therefore the multiboot module should save any information
 * it needs. The kernel will make sure that when this is called, a working
 * implementation of kmalloc exists
 */
 void multiboot_clone() {
     __multiboot_stage = MB_STAGE_DONE;
 }

/*
 * Parse multiboot information structure 
 */
static void parse_multiboot(u32 multiboot_info_ptr) {
    mb1_info_block = (mb1_info_block_t*) multiboot_info_ptr; 
    if CMD_LINE_VALID(mb1_info_block)
        strncpy(cmdline, (const char*) mb1_info_block->cmdline, MULTIBOOT_MAX_CMD_LINE - 1);
    else    
        cmdline[0]=0;
    /*
     * Parse memory data
     */
    KASSERT(MEM_MAP_VALID(mb1_info_block));
    mb1_memory_map_entry_next = (mb1_memory_map_entry_t*) mb1_info_block->mmap_addr;
    mb1_memory_map_entry_start = mb1_memory_map_entry_next;
    mb1_memory_map_length = mb1_info_block->mmap_length;
    /*
     * Parse VBE table
     */
    if (VBE_DATA_VALID(mb1_info_block)) {
        vbe_startup_mode = mb1_info_block->vbe_mode;
    } 
}

/*
 * Parse the multiboot 2 information structure
 * Parameter:
 * @multiboot_info_ptr - a pointer to the header of
 *                       the MBI structure
 */
void parse_multiboot2(u32 multiboot_info_ptr) {
    mb2_mbi_tag_t* mbi_tag = (mb2_mbi_tag_t*)(multiboot_info_ptr + sizeof(mb2_mbi_header_t));
    mb2_mbi_tag_fb_t* fb_tag = 0;
    char* cmdline_start;
    u32 next_addr;
    /*
     * Initialize some data 
     */
    cmdline[0]=0;
    /*
     * Now walk the tags
     */
    while (mbi_tag->type != 0) {
        switch (mbi_tag->type) {
            case 1:
                // Command line
                cmdline_start = &(((mb2_mbi_tag_cmdline_t*) mbi_tag)->cmdline);
                strncpy(cmdline, cmdline_start, MULTIBOOT_MAX_CMD_LINE -1);
                break;
            case 3:
                // Modules, i.e. ramdisk
                mb2_rd_start = ((mb2_mbi_tag_module_t*) mbi_tag)->start;
                mb2_rd_end = ((mb2_mbi_tag_module_t*) mbi_tag)->end;
                break;
            case 6:
                // Memory map
                mb2_memory_map_entry_start =  (mb2_memory_map_entry_t*)(((u32) mbi_tag) + sizeof(mb2_mbi_tag_mmap_t));
                mb2_memory_map_entries = (((mb2_mbi_tag_mmap_t*) mbi_tag)->size - 32) / sizeof(mb2_memory_map_entry_t);
                break;
            case 8:
                // Framebuffer information
                fb_tag = (mb2_mbi_tag_fb_t*) mbi_tag;
                if (fb_tag->fb_type == 1) {
                    /*
                     * We have a framebuffer as we need it (linear / RGB)
                     */
                    mb2_have_fb = 1; 
                    memcpy(&mb2_fb_tag, (void*) fb_tag, sizeof(mb2_fb_tag));
                }
                break;
            case 15:
                // ACPI RSDP 
                memcpy(&acpi_rsdp_copy, (void*)(((u32) mbi_tag) + 8), mbi_tag->size - 8);
                acpi_rsdp = &acpi_rsdp_copy;
                break;
        }
        /*
         * Advance to next tag. Note that the next tag is again aligned, so we might have to fill up
         * to make sure that we are aligned at an 8-byte boundary
         */
        next_addr = ((u32) mbi_tag) + mbi_tag->size;
        if (next_addr % 8) {
            next_addr += (8 - (next_addr % 8));
        }
        mbi_tag = (mb2_mbi_tag_t*) next_addr;
    } 
}


/*
 * Return kernel command line
 */
const char* multiboot_get_cmdline() {
    if (MB_STAGE_NOT_READY == __multiboot_stage) {
        PANIC("Called too early\n");
    }
    return cmdline;
}

/*
 * Return the next memory map entry. Be careful - this has a state
 * and it is assumed that the memory map is only walked once
 * Returns:
 * 0 - no more entries exist
 * 1 - the next entry was written into *next
 */
int multiboot_get_next_mmap_entry(memory_map_entry_t* next) {
    if (MB_STAGE_EARLY  != __multiboot_stage) {
       PANIC("Called too early\n");
    }   
    if (1 == multiboot_version) {
        /*
         * Multiboot 1
         */
        /*
        * If we have already reached the end return 0
        */
        if (0 == mb1_memory_map_entry_next)
            return 0;
        /*
        * Copy data
        */
        next->base_addr_low = mb1_memory_map_entry_next->base_addr_low;
        next->base_addr_high = mb1_memory_map_entry_next->base_addr_high;
        next->length_low = mb1_memory_map_entry_next->length_low;
        next->length_high = mb1_memory_map_entry_next->length_high;
        next->type = mb1_memory_map_entry_next->type;
        /*
        * 
        * Move on to next pointer
        */
        if (((u32) (mb1_memory_map_entry_next) - (u32) (mb1_memory_map_entry_start)) < mb1_memory_map_length) {
            /*
            * There is still at least one more entry
            */
            mb1_memory_map_entry_next = (mb1_memory_map_entry_t*) (((u32) mb1_memory_map_entry_next)
                    + mb1_memory_map_entry_next->size + sizeof(mb1_memory_map_entry_next->size));
        }
        else {
            mb1_memory_map_entry_next = 0;  
        }
        return 1;
    }
    /*
     * Multiboot 2
     */
    else {
        if (mb2_memory_map_index >= mb2_memory_map_entries){
            return 0;
        }
        next->base_addr_low  = (mb2_memory_map_entry_start + mb2_memory_map_index)->base_addr_low;
        next->base_addr_high  = (mb2_memory_map_entry_start + mb2_memory_map_index)->base_addr_high;
        next->length_low = (mb2_memory_map_entry_start + mb2_memory_map_index)->length_low;
        next->length_high = (mb2_memory_map_entry_start + mb2_memory_map_index)->length_high;
        next->type = (mb2_memory_map_entry_start + mb2_memory_map_index)->type;
        mb2_memory_map_index++;
        return 1;
    }
 }
 
 /*
  * Return the location of the ramdisk
  * Returns:
  * - 0 if no ramdisk could found
  * - 1 if a ramdisk was detected, then the ramdisk_info_block is filled
  */
int multiboot_locate_ramdisk(multiboot_ramdisk_info_block_t* ramdisk_info_block) {
    mb1_module_entry_t* mb1_mod_entry;
    if (MB_STAGE_EARLY != __multiboot_stage) {
        PANIC("Called too early or too late");
    }
    /*
     * Multiboot 1
     */
    if (1 == multiboot_version) {
        if (!(MOD_MAP_VALID(mb1_info_block))) {
            DEBUG("No valid module information in multiboot header\n");
            return 0;
        }
        if (mb1_info_block->mods_count > 1) {
            DEBUG("More than one module passed, cannot determine ramdisk\n");
            return 0;
        }
        if (0 == mb1_info_block->mods_count)
            return 0;
        
        mb1_mod_entry = ((mb1_module_entry_t*) (mb1_info_block->mods_addr));
        ramdisk_info_block->start = mb1_mod_entry->mod_start;
        ramdisk_info_block->end = mb1_mod_entry->mod_end;
        return 1;
    }
    /*
     * Multiboot 2
     */
    else {
        if (0 == mb2_rd_start) {
            return 0;
        }
        ramdisk_info_block->start = mb2_rd_start;
        ramdisk_info_block->end = mb2_rd_end;
        return 1;
    }
}

/*
 * This function will try to figure out whether we haven been
 * placed in text mode by the boot loader or in graphics mode
 * If we are in text mode, 0 is returned
 * If we are in graphics mode, 1 is returned and the structure
 * fb_desc is filled
 */
int multiboot_probe_video_mode(fb_desc_t* fb_desc) {
    if (1 == multiboot_version) {
        /*
         * For multiboot1, we do not support startup in graphics mode
         * and assume that have been brought up in text mode
         * This is a hack, but multiboot1 is only used by QEMU these days and
         * QEMU does not have VBE support anyway
         */
         return 0;
    }
    if (0 == mb2_have_fb)
        return 0;
   /*
    * We have a framebuffer tag of type 1, so we are in graphics mode
    */
   fb_desc->bitsPerPixel = mb2_fb_tag.bitsPerPixel;
   fb_desc->bytesPerScanLine = mb2_fb_tag.bytesPerScanline;
   fb_desc->xResolution = mb2_fb_tag.width;
   fb_desc->yResolution = mb2_fb_tag.height;
   fb_desc->bitsPerPixel = mb2_fb_tag.bitsPerPixel;
   fb_desc->type = FB_TYPE_RGB;
   fb_desc->redMaskSize = mb2_fb_tag.redMaskSize;
   fb_desc->redFieldPosition = mb2_fb_tag.redFieldPosition;
   fb_desc->greenMaskSize = mb2_fb_tag.greenMaskSize;
   fb_desc->greenFieldPosition = mb2_fb_tag.greenFieldPosition;
   fb_desc->blueMaskSize = mb2_fb_tag.blueMaskSize;
   fb_desc->blueFieldPosition = mb2_fb_tag.blueFieldPosition;
   fb_desc->physBasePtr = mb2_fb_tag.fb_addr_low;
   KASSERT(0 == mb2_fb_tag.fb_addr_high);
   return 1;
}

/*
 * Return the address of a copy of the RSDT if we have it
 */
u32 multiboot_get_acpi_rsdp() {
    return (u32) acpi_rsdp;
}

/***************************************************************
 * Everything below this line is for debugging only            *
 **************************************************************/

void multiboot_print_info() {
    PRINT("Multiboot stage:         %d\n", __multiboot_stage);
    PRINT("Magic number:            %x\n", magic);
    PRINT("Multiboot version:       %d\n", multiboot_version);
    PRINT("Command line:            %s\n", cmdline);
    PRINT("VBE mode at startup:     %d\n", vbe_startup_mode);
    if (2 == multiboot_version ) {
        PRINT("RAM disk start:          %x\n", mb2_rd_start);
    }
    if (acpi_rsdp) {
        PRINT("Have RSDP\n");
    }
}