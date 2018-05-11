#ifndef _VGA_H
#define _VGA_H

#include "multiboot.h"
#include "locks.h"
#include "console.h"

/*
 * VBE controller information
 */
typedef struct {
    unsigned char vbeSignature[4];
    unsigned short vbeVersion;
    unsigned int oemStringPtr;
    unsigned char capabilities[4];
    unsigned int videoModePtr;
    unsigned short totalMemory;
    unsigned short oemSoftwareRev;
    unsigned int oemVendorNamePtr;
    unsigned int oemProductNamePtr;
    unsigned int oemProductRevPtr;
    unsigned char reserved[222];
    unsigned char oemData[256];
} __attribute__ ((packed)) vbe_info_block_t;

/*
 * VBE Mode Info
 */
typedef struct {
    unsigned short modeAttributes;
    unsigned char winAttributes;
    unsigned char winBAttributes;
    unsigned short winGranularity;
    unsigned short winSize;
    unsigned short winASegment;
    unsigned short winBSegment;
    unsigned int winFuncPtr;
    unsigned short bytesPerScanLine;
    unsigned short xResolution;
    unsigned short yResolution;
    unsigned char xCharSize;
    unsigned char yCharSize;
    unsigned char numberOfPlanes;
    unsigned char bitsPerPixel;
    unsigned char numberOfBanks;
    unsigned char memoryModel;
    unsigned char bankSize;
    unsigned char numberOfImagePages;
    unsigned char reserved;
    unsigned char redMaskSize;
    unsigned char redFieldPosition;
    unsigned char greenMaskSize;
    unsigned char greenFieldPosition;
    unsigned char blueMaskSize;
    unsigned char blueFieldPosition;
    unsigned char rsvdMaskSize;
    unsigned char rsvdMaskPosition;
    unsigned char directColorModeInfo;
    unsigned int physBasePtr;
} __attribute__ ((packed)) vbe_mode_info_t;


/*
 * A video mode description used internally by the VGA driver
 */
typedef struct {
    int mode;                               // Kernel mode number as specified in boot parameter VGA (0 reserved for text mode)
    u32 x_resolution;                       // x resolution
    u32 y_resolution;                       // y resolution
    int bpp;                                // Bits per pixel = color depth
    u32 framebuffer_base;                   // physical base address of frame pointer
    int choice;                             // priority, 0 = highest, 255 = lowest
} vga_mode_t;

/*
 * Description of a frame buffer
 */
 
 typedef struct {
    u16 bytesPerScanLine;
    u16 xResolution;
    u16 yResolution;
    u8 bitsPerPixel;
    u8 type;
    u8 redMaskSize;
    u8 redFieldPosition;
    u8 greenMaskSize;
    u8 greenFieldPosition;
    u8 blueMaskSize;
    u8 blueFieldPosition;
    u32 physBasePtr;     
 } fb_desc_t;
 
#define FB_TYPE_RGB 6
 
/*
 * VGA text mode colors from the standard palette. The decoding is as follows.
 *
 * Bit 0: blue
 * Bit 1: green
 * Bit 2: red
 * Bit 3: intensity bit
 *
 * An exception is made for 0x6 which is not light yellow as expected but
 * brown
 */
#define VGA_COLOR_WHITE 0x7
#define VGA_COLOR_BLACK 0x0
#define VGA_COLOR_BLUE 0x1
#define VGA_COLOR_GREEN 0x2
#define VGA_COLOR_CYAN 0x3
#define VGA_COLOR_RED 0x4
#define VGA_COLOR_MAGENTA 0x5
#define VGA_COLOR_BROWN 0x6
#define VGA_COLOR_GRAY 0x7
#define VGA_COLOR_YELLOW 0xe
#define VGA_COLOR_INTENSE 0x8

/*
 * This needs to be one of the colors in the range 0 to 7! No other attributes are allowed
 */
#define VGA_STD_ATTRIB (VGA_COLOR_WHITE)
#define VGA_COLS 80
#define VGA_VIDEO_MEM 0xb8000
#define VGA_LAST_LINE 24

/*
 * Some registers
 */
#define VGA_CRT_INDEX 0x3d4
#define VGA_CRT_DATA 0x3d5
#define VGA_INPUT_STATUS_REG1 0x3da

#define VGA_CRT_CURSOR_HIGH 0xe
#define VGA_CRT_CURSOR_LOW 0xf

#define VGA_ATTR_IPAS (1 << 5)
#define VGA_ATTR_ADDRESS 0x3c0
#define VGA_ATTR_DATA_READ 0x3c1
#define VGA_ATTR_MODE_CTRL 0x10

/*
 * Modes in which we operate
 */
#define VGA_MODE_TEXT 0
#define VGA_MODE_GRAPHICS 1



/*
 * Maximum supported resolution and BPP - needed for shadow RAM
 */
#define VGA_MAX_X_RESOLUTION 1280
#define VGA_MAX_Y_RESOLUTION 1024
#define VGA_MAX_BPP 32

/*
 * VESA memory models
 */
#define VESA_DIRECT_COLOR 6

/*
 * Determine offset of a pixel within the linear framebuffer
 */
#define VGA_OFFSET(x, y, win)  ((y + win->y_origin) * __fb_desc_ptr->bytesPerScanLine + (x + win->x_origin) * (__fb_desc_ptr->bitsPerPixel / 8))

/*
 * Frames around windows
 */
#define WIN_FRAME_WIDTH 3
#define WIN_TITLE_HEIGHT 30
#define WIN_BOTTOM_HEIGHT 2

void vga_init(int);
void vga_init_win(win_t* win, u32 x_origin, u32 y_origin, u32 x_resolution, u32 y_resolution);
void vga_enable_paging();
void vga_debug_regs();
void vga_clear_win(win_t* win, u32 red, u32 green, u32 blue);
void vga_toggle_cursor(win_t* win);
void vga_no_cursor(win_t* win);
void vga_set_cursor(win_t* win, u32 x, u32 y);
void vga_put_pixel(win_t* win, u32 x, u32 y, u32 rgb);
u32 vga_vesa_color(u8 red, u8 green, u8 blue);
u8 vga_text_attr(u8 fg_rgb, u8 bg_rgb, u8 intensity, u8 blink);
void vga_decorate_window(win_t* win, char* title);
int vga_get_mode(u32* x_resolution, u32* y_resolution, u32* bpp);

#endif
