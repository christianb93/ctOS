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

/*
 * Forward declarations
 */
static void parse_multiboot(u32 multiboot_info_ptr);

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
    if (2 == multiboot_version) {
        PANIC("Multiboot version 2 - does not yet know how to do this\n");
    }
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
    if (2 == multiboot_version) {
        PANIC("Multiboot version 2 - does not yet know how to do this\n");
    }
    if (MB_STAGE_EARLY  != __multiboot_stage) {
        PANIC("Called too early\n");
    }   
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
  * Return the location of the ramdisk
  * Returns:
  * - 0 if no ramdisk could found
  * - 1 if a ramdisk was detected, then the ramdisk_info_block is filled
  */
int multiboot_locate_ramdisk(multiboot_ramdisk_info_block_t* ramdisk_info_block) {
    mb1_module_entry_t* mb1_mod_entry;
    if (2 == multiboot_version) {
        PANIC("Multiboot version 2 - does not yet know how to do this\n");
    }
    
    if (MB_STAGE_EARLY != __multiboot_stage) {
        PANIC("Called too early or too late");
    }
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

/***************************************************************
 * Everything below this line is for debugging only            *
 **************************************************************/

 void multiboot_print_info() {
     PRINT("Multiboot stage:         %d\n", __multiboot_stage);
     PRINT("Magic number:            %x\n", magic);
     PRINT("Multiboot version:       %d\n", multiboot_version);
     PRINT("Command line:            %s\n", cmdline);
     PRINT("VBE mode at startup:     %d\n", vbe_startup_mode);
 }