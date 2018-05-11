/*
 * multiboot.h
 *
 */

#ifndef _MULTIBOOT_H_
#define _MULTIBOOT_H_

#include "ktypes.h"
#include "vga.h"

/*
 * Magic values
 */
 
 #define MB_MAGIC_V1 0x2BADB002
 #define MB_MAGIC_V2 0x36d76289

/*
 * Cut off length for the command line
 */
#define MULTIBOOT_MAX_CMD_LINE 512

/*******************************************
 * These structures are protocol specific
 *******************************************/

/*
 * Structure of multiboot information block
 */

typedef struct {
    u32 flags;
    u32 mem_lower;
    u32 mem_upper;
    u32 boot_device;
    u32 cmdline;
    u32 mods_count;
    u32 mods_addr;
    u32 syms[4];
    u32 mmap_length;
    u32 mmap_addr;
    u32 drives_length;
    u32 drives_addr;
    u32 config_table;
    u32 boot_loader_name;
    u32 apm_table;
    u32 vbe_control_info;
    u32 vbe_mode_info;
    u16 vbe_mode;
    u16 vbe_interface_seg;
    u16 vbe_interface_off;
    u16 vbe_interface_len;
    u32 framebuffer_addr_low;
    u32 framebuffer_addr_high;
}__attribute__ ((packed)) mb1_info_block_t;

/* Structure of a multiboot 1 memory map entry.
 * Note that size does not include the field
 * size itself
 */
typedef struct {
    u32 size;
    u32 base_addr_low;
    u32 base_addr_high;
    u32 length_low;
    u32 length_high;
    u32 type;
} mb1_memory_map_entry_t;

/*
 * The module entry
 */
typedef struct {
    u32 mod_start;
    u32 mod_end;
    char* string;
    u32 reserved;
} mb1_module_entry_t;

/*
 * The flags field in the multiboot
 * structure indicates whether the
 * other fields are valid
 * These macros check the corresponding
 * bitmasks
 */
#define MEM_LOWER_VALID(x) ((x->flags) & 0x1)
#define MEM_UPPER_VALID(x) ((x->flags) & 0x1)
#define MEM_MAP_VALID(x) (((x->flags) >> 6) & 0x1)
#define MOD_MAP_VALID(x) (((x->flags) >> 3) & 0x1)
#define VBE_DATA_VALID(x) (((x->flags) >> 11) & 0x1)
#define FB_DATA_VALID(x) (((x->flags) >> 12) & 0x1)
#define CMD_LINE_VALID(x) (((x->flags) >> 2) & 0x1)

/*
 * The header of the Multiboot2 
 * information structure
 */
typedef struct {
    u32 total_size;  
    u32 reserved;
} mb2_mbi_header_t;

/*
 * A multiboot2 tag in the MBI
 */
 typedef struct {
     u32 type;
     u32 size;
 } mb2_mbi_tag_t;

/*
 * A multiboot2 command line tag
 */
 typedef struct {
     u32 type;
     u32 size;
     char cmdline;
 } mb2_mbi_tag_cmdline_t;

/*
 * A multiboot2 memory map tag
 */
 typedef struct {
     u32 type;
     u32 size;
     u32 entry_size;
     u32 entry_version;
 } mb2_mbi_tag_mmap_t;

/* Structure of a multiboot 2 memory map entry.
 */
typedef struct {
    u32 base_addr_low;
    u32 base_addr_high;
    u32 length_low;
    u32 length_high;
    u32 type;
    u32 reserved;
} mb2_memory_map_entry_t;

/*
 * A multiboot2 module tag
 */
typedef struct {
    u32 type;
    u32 size;
    u32 start;
    u32 end;
    unsigned char* name;
 } mb2_mbi_tag_module_t;


/*
 * A multiboot2 framebuffer tag
 * This is only valid if the framebuffer type is 1
 * Note that the reserved field is 2 bytes in contrast
 * to the multiboot spec as GRUB2 will align the fields
 * starting at redFieldPosition on a 64-bit boundary
 */
typedef struct {
    u32 type;
    u32 size;
    u32 fb_addr_low;
    u32 fb_addr_high;
    u32 bytesPerScanline;
    u32 width;
    u32 height;
    u8 bitsPerPixel;
    u8 fb_type;
    u16 reserved;
    u8 redFieldPosition;
    u8 redMaskSize;
    u8 greenFieldPosition;
    u8 greenMaskSize;
    u8 blueFieldPosition;
    u8 blueMaskSize;
} __attribute__ ((packed)) mb2_mbi_tag_fb_t;
 
/*******************************************
 * These structures are not protocol specific
 *******************************************/


/* The version independent memory map entry
 * Note that size does not include the field
 * size itself
 */
typedef struct {
    u32 size;
    u32 base_addr_low;
    u32 base_addr_high;
    u32 length_low;
    u32 length_high;
    u32 type;
} memory_map_entry_t;

/*
 * Types of entries in the memory map passed by the boot loader
 */
#define MB_MMAP_ENTRY_TYPE_FREE 1

/*
 * A structure describing size and location
 * of the ramdisk
 */
typedef struct {
    u32 start;
    u32 end;
} multiboot_ramdisk_info_block_t;

/*
 * Stages
 */
#define MB_STAGE_NOT_READY 0
#define MB_STAGE_EARLY 1
#define MB_STAGE_DONE 2

void multiboot_init(u32 multiboot_info_ptr, u32 magic);
void multiboot_print_info();
const char* multiboot_get_cmdline();
int multiboot_get_next_mmap_entry(memory_map_entry_t* next);
int multiboot_locate_ramdisk(multiboot_ramdisk_info_block_t* multiboot_ramdisk_info_block);
void multiboot_clone();
int multiboot_probe_video_mode(fb_desc_t* fb_desc);

#endif /* _MULTIBOOT_H_ */
