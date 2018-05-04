/*
 * vga.c
 *
 * This module is the low-level interface to the VGA video card. It supports text mode and graphics mode via VESA / VBE.
 *
 * Essentially, this module has two different interfaces. First, there is the ordinary interface exposed via vga.h which contains functions
 * for drawing primitives in VGA mode as well as support for software cursor handling and intialization. In addition, this module exports
 * a couple of function pointers which - depending on whether we are in graphics mode or text mode - point to text mode or graphics mode
 * implementations of basic TTY routines like putting a character on the screen or copying lines and characters. These function pointers
 * are used by the console driver (console.c) which therefore does not need to know whether we are in graphics mode or text mode.
 *
 * In graphics mode, a linear framebuffer is used which is mapped into virtual memory and written directly. As writing to a video
 * memory is reasonable fast on modern cards, but reading is slow, we maintain a "shadow" RAM in which all writes to the video
 * memory are done as well. When data needs to be read from video memory, the shadow RAM is used instead.
 *
 * If possible, the SSE non-temporal stores are used to write to video memory
 *
 * Color coding:
 *
 * Color coding is a little bit tricky, as various encoding schemes are in use.
 *
 * a) 3-bit RGB encoding: i.e. a color is a mixture of red, green and blue, where the corresponding bits indicate whether the
 *    respective base color is used. Thus white is 0x7, blue is 0x1, green is 0x2 and so forth. This is
 *    sometimes referred to as VGA text mode color
 * b) VGA text mode attribute byte: here an attribute byte describes one character. Bits 0-2 are the foreground color as 3-bit RGB,
 *    bits. Bit 3 is the intensity bit, if this bit is set, the foreground color is displayed with higher intensity. Bits 6-4 are the
 *    RGB 3-bit background color, and bit 7 is the blinking bit. Note that the blinking bit only works if the so-called blinking bit in
 *    the attribute mode control register is enabled. This is normally done by the BIOS at boot time
 * c) 24 bit true-color RGB: here a color is represented as a 3 byte RGB value. Thus about 16 million colors are possible
 * d) VESA color: this is a 32-bit integer which represents a VESA pixel color, i.e. the value which needs to be written into the framebuffer
 *    area representing one pixel. Depending on the VESA mode, only the first 15, 16 or 24 bits are actually used, but within ctOS, a
 *    32 bit integer is used to store a VESA color
 * e) Packed color coding in graphics mode. Here a color is just an index into the palette maintained by the VGA card (DAC). This encoding
 *    is most often used in combination with 8-bit color depths.
 *
 * The following conversion functions are offered by the VGA driver:
 *
 * vga_vesa_color: convert a 24 bit RGB value into a VESA color valid for the currently active mode
 * vga_text_attr: convert a three-bit RGB value plus intensity bit and blink bit into a VGA text mode attribute byte
 *
 * In graphics mode, ctOS currently only supports direct color code (VESA memory model 6). i.e. 16 bit, 24 bit and 32 bit color depth. As
 * almost every graphics card present in todays desktop machines will offer a mode with 24 or 32 bpp, this is not a serious restriction
 *
 * To display fonts in graphics mode, several alternatives are possible. Instead of using pre-defined fonts (for instance the fonts which
 * come with the Bochs VGA BIOS), ctOS uses the BIOS call int 10h to read the 8x16 VGA BIOS font from the BIOS memory at startup. This
 * font is then used to display characters on the console screen.
 *
 * Also note that currently the "GUI" in ctOS is a dirty hack. Windows can be defined, but cannot be moved and cannot overlap. Moreover,
 * the framebuffer is only accessible for the kernel so that user space applications cannot use graphics at all. Essentially, in the current
 * release of ctOS, support for graphics has only been implemented to be able to display some additional information on the screen for
 * testing and debugging. In a future release, it is planned to implement a linear framebuffer which is exported into user space, either
 * as device or as memory mapped I/O. To avoid that user space graphics conflict with the terminal emulation done by the kernel, it is
 * planned to use "virtual consoles" between which the user can cycle using keyboard shortcuts. Thus there might be N virtual framebuffers
 * to which user space code can write. At any point in time, one of these framebuffers in virtual memory will be mapped to the real
 * physical framebuffer memory, whereas others are just mapped to ordinary memory areas. When the user switches to another console, the
 * physical framebuffer is remapped accordingly.Individual consoles can be reserved for the kernel and can be used to display output for
 * emulated TTYs, whereas other consoles are used exclusively by user space applications. Corresponding changes have to be made to the
 * TTY driver so that keyboard input is routed to the currently active virtual console.
 */

#include "ktypes.h"
#include "vga.h"
#include "io.h"
#include "tests.h"
#include "locks.h"
#include "params.h"
#include "lib/ctype.h"
#include "debug.h"
#include "multiboot.h"
#include "mm.h"
#include "cpu.h"
#include "smp.h"
#include "sched.h"
#include "console.h"
#include "rm.h"


/*
 * Current mode (0 = text, 1 = graphics)
 */
static int mode = 0;

/*
 * Root window
 */
static win_t root_win;


/*
 * Font data
 */
static u8 font_data[256*16];

/*
 * Shadow of VIDEO RAM. We expect at most 1024x768 bytes with 32bpp
 */
static unsigned char shadow[VGA_MAX_X_RESOLUTION*VGA_MAX_Y_RESOLUTION*VGA_MAX_BPP/8];

/*
 * Can we use MMX
 */
static int use_mmx = 0;

/*
 * This is a list of video modes which we recognize
 * 1 - 1024 x 768 @ 24 bpp
 * 2 - 1024 x 768 @ 32 bpp
 * 3 - 1024 x 768 @ 16 bpp
 * 4 - 1280 x 1024 @ 32 bpp
 * 5 - 1280 x 800 @ 32 bpp
 *
 */
static vga_mode_t valid_modes[] = {
        {1, 1024, 768, 24, 0, -1, 1},
        {2, 1024, 768, 32, 0, -1, 2},
        {3, 1024, 768, 16, 0, -1, 3},
        {4, 1280, 1024, 16, 0, -1, 4},
        {5, 1280, 800, 32, 0, -1, 5},
        {6, 1280, 720, 32, 0, -1, 6},
        {7, 1360, 768, 32, 0, -1, 7}
};

/*
 * VBE information
 */
static vbe_mode_info_t current_mode;
static vbe_mode_info_t* vbe_mode = 0;
static u32 frame_buffer_base = 0;
static u32 multiboot_fb_addr_low = 0;
static u32 multiboot_fb_addr_high = 0;


/****************************************************************************************
 * Some basic primitives used in graphics mode                                          *
 ***************************************************************************************/

/*
 * Depending on the color depth, update the bytes for one pixel in the framebuffer and the
 * shadow frame buffer with a given color
 * Parameter:
 * @offset - offset of pixel start address into framebuffer
 * @color - VESA color
 */
static void mem_put_pixel(u32 offset, u32 color) {
    /*
     * The processing depends on the color depth:
     * - 8 bits per pixel is nos supported
     * - for 15 or 16 bits per pixel, we write an unsigned short at a time
     * - for 24 bits per pixel, we write an unsigned short and a byte
     */
    switch(vbe_mode->bitsPerPixel) {
        case 15:
        case 16:
            *((u16*) (offset + shadow)) = color & 0xFFFF;;
            *((u16*) (offset + frame_buffer_base)) = color & 0xFFFF;
            break;
        case 24:
            *((u16*) (offset + shadow)) = color & 0xFFFF;
            *((u16*) (offset + frame_buffer_base)) = color & 0xFFFF;
            *((u8*) (offset + shadow + 2)) = (color >> 16) & 0xFF;
            *((u8*) (offset + frame_buffer_base+ + 2)) =  (color >> 16) & 0xFF;
            break;
        case 32:
            *((u32*) (offset + shadow)) = color;
            *((u32*) (offset + frame_buffer_base)) = color;
            break;
        default:
            break;
    }
}

/*
 * Put a pixel on the screen in graphics mode
 * Parameter:
 * @win - the window
 * @x - x coordinate (starting with 0)
 * @y - y coordinate (starting with 0)
 * @rgb - 32 bit VESA color
 * Note: no locking is done, it is within the responsibility of the caller
 * to avoid concurrent access
 */
void vga_put_pixel(win_t* win, u32 x, u32 y, u32 color) {
    win_t* _win = &root_win;
    if (win)
        _win = win;
    if ((x >= _win->width) || (y >= _win->height))
        return;
    if (VGA_MODE_TEXT == mode)
        return;
    /*
     * Determine address of pixel to write relative to start of frame buffer
     */
    u32 address = VGA_OFFSET(x, y, _win);
    mem_put_pixel(address, color);
}

/*
 * Read a pixel from the screen in graphics mode
 * Parameter:
 * @win - the window
 * @x - x coordinate (starting with 0)
 * @y - y coordinate (starting with 0)
 * Return value:
 * the VESA color code
 */
static u32 vga_get_pixel(win_t* win, u32 x, u32 y) {
    win_t* _win = &root_win;
    u32 rgb = 0;
    if (win)
        _win = win;
    if (VGA_MODE_TEXT == mode)
        return 0;
    if ((x >= _win->width) || (y >= _win->height))
        return 0;
    /*
     * Determine address of pixel relative to start of frame buffer
     */
    u32 address = VGA_OFFSET(x, y, _win);
    /*
     * Now the processing depends on the VESA color depth:
     * - for 8 bits per pixel, we read one byte at a time
     * - for 15 or 16 bits per pixel, we read an unsigned short at a time
     * - for 24 bits per pixel, we read an unsigned short and a byte
     */
    switch(vbe_mode->bitsPerPixel) {
        case 8:
            rgb = *((u8*) (address + shadow));
            break;
        case 15:
        case 16:
            rgb = *((u16*) (address + shadow));
            break;
        case 24:
            rgb = *((u8*) (address + shadow + 2));
            rgb = rgb << 16;
            rgb = rgb + *((u16*) (address + shadow));
            break;
        case 32:
            rgb = *((u32*) (address + shadow));
            break;
        default:
            break;
    }
    return rgb;
}

/*
 * Draw a rectangle
 * Parameter:
 * @win - window
 * @x1, y1 - coordinates of upper left corner
 * @width, height - width and height
 * @color - the VESA color code
 */
static void vga_draw_rectangle(win_t* win, u32 x1, u32 y1, u32 width, u32 height, u32 color) {
    u32 x;
    u32 y;
    win_t* _win = &root_win;
    u32 line_start_address;
    u32 line_offset;
    if (win)
        _win = win;
    if (VGA_MODE_TEXT == mode)
        return;
    /*
     * Check parameters
     */
    if ((x1 >= _win->width) || (x1 + width > _win->width))
        return;
    if ((y1 >= _win->height) || (y1 + height > _win->height))
        return;
    /*
     * Compute address of first pixel in first line
     */
    line_start_address = VGA_OFFSET(x1, y1, _win);
    for (y = y1; y < y1 + height; y++) {
        line_offset = 0;
        /*
         * Draw line
         */
        for (x = x1; x < x1 + width; x++) {
            mem_put_pixel(line_start_address + line_offset, color);
            line_offset += vbe_mode->bitsPerPixel >> 3;
        }
        /*
         * Advance line start address
         */
        line_start_address += vbe_mode->bytesPerScanLine;
    }
}

/*
 * Given values for red, green and blue, compute a corresponding VESA color which
 * can be used as argument for vga_put_pixel
 * Parameter:
 * @red - red value
 * @green - green value
 * @blue - blue value
 * Return value:
 * VESA color code
 */
u32 vga_vesa_color(u8 red, u8 green, u8 blue) {
    u32 color = 0;
    u32 _red;
    u32 _green;
    u32 _blue;
    /*
     * Do nothing in text mode
     */
    if ((VGA_MODE_TEXT == mode) || (0 == vbe_mode))
        return 0;
    /*
     * If memory model is not 6 (direct vga_vesa_color) give up
     * as we do not know the palette
     */
    if (VESA_DIRECT_COLOR != vbe_mode->memoryModel)
        return 0xff;
    /*
     * For each color, we shift the input as far as needed to
     * fit into vbe_mode->xxxMaskSize bits
     */
    _red = (red >> (8 - vbe_mode->redMaskSize));
    _green = (green >> (8 - vbe_mode->greenMaskSize));
    _blue = (blue >> (8 - vbe_mode->blueMaskSize));
    color = _red << vbe_mode->redFieldPosition;
    color = color + (_green << vbe_mode->greenFieldPosition);
    color = color + (_blue << vbe_mode->blueFieldPosition);
    return color;
}

/*
 * Convert a three-bit RGB value plus intensity bit and blinking mode bit
 * into a VGA attribute byte
 * Parameter:
 * @fg_rgb - three bit RGB value for foreground color
 * @bg_rgb - three bit RGB value for background color
 * @intensity - intensity bit
 * @blink - blink bit
 */
u8 vga_text_attr(u8 fg_rgb, u8 bg_rgb, u8 intensity, u8 blink) {
    return (fg_rgb & 0x7) + ((intensity & 0x1) << 3) + ((bg_rgb & 0x7) << 4) + ((blink & 0x1) << 7);
}

/*
 * Show cursor, i.e. copy the current content of the last line of the
 * cursor position into an internal buffer and draw cursor line
 * Parameter:
 * @win - the window
 */
static void vga_show_cursor(win_t* win) {
    int i;
    u32 x;
    u32 y;
    u32 new_color;
    win_t* _win = &root_win;
    if (VGA_MODE_TEXT == mode)
        return;
    if (win)
        _win = win;
    if (_win->cursor_visible)
        return;
    if (_win->no_cursor)
        return;
    new_color = vga_vesa_color(0xff, 0xff, 0xff);
    x = VGA_FONT_WIDTH*_win->cursor_x;
    y = VGA_FONT_HEIGHT*_win->cursor_y + VGA_FONT_HEIGHT - 1;
    for (i = 0; i < VGA_FONT_WIDTH; i++) {
        _win->cursor_buffer[i] = vga_get_pixel(_win, x + i, y);
        vga_put_pixel(_win, x + i, y, new_color);
    }
    _win->cursor_visible = 1;
}

/*
 * Hide cursor, i.e. copy the content of the internal buffer back to the
 * screen
 * Parameter:
 * @win - the window
 */
static void vga_hide_cursor(win_t* win) {
    int i;
    u32 x;
    u32 y;
    if (VGA_MODE_TEXT == mode)
        return;
    win_t* _win = &root_win;
    if (win)
        _win = win;
    if (0 == _win->cursor_visible)
        return;
    x = VGA_FONT_WIDTH*_win->cursor_x;
    y = VGA_FONT_HEIGHT*_win->cursor_y + VGA_FONT_HEIGHT - 1;
    for (i = 0; i < VGA_FONT_WIDTH; i++) {
        vga_put_pixel(_win, x + i, y, _win->cursor_buffer[i]);
    }
    _win->cursor_visible = 0;
}

/*
 * Clear a window, i.e. fill it with a background color
 * Parameter:
 * @win - the window
 * @red - RGB red value for background color
 * @green - RGB green value
 * @blue - RGB blue value
 */
void vga_clear_win(win_t* win, u32 red, u32 green, u32 blue) {
    win_t* _win = &root_win;
    if (win)
        _win = win;
    if (VGA_MODE_TEXT == mode)
        return;
    u32 color = vga_vesa_color(red, green, blue);
    vga_hide_cursor(_win);
    vga_draw_rectangle(_win, 0, 0, _win->width, _win->height, color);
}

/*
 * Print a character into a window
 * Parameter:
 * @win -the window
 * @x_org - x coordinate where we start (measured in characters)
 * @y_org - y coordinate where we start (measured in characters)
 * @c - the character to be printed
 * @transparent - set this to one to not overwrite the background
 * @fg_color - VESA foreground color
 * @bg_color - VESA background color
 */
static void setchar_win(win_t* win, u32 x_org, u32 y_org, u8 c, int transparent, u32 fg_color, u32 bg_color) {
    unsigned char* char_bitmap = 0;
    unsigned int x;
    unsigned int y;
    unsigned int _x;
    unsigned int _y;
    win_t* _win = &root_win;
    if (VGA_MODE_TEXT == mode)
        return;
    if (win)
        _win = win;
    /*
     * When initializing the adapter, we have read the VGA BIOS font data into the array font_data. For each
     * character, there is one entry. Each byte of this entry describes one of 16 lines, where a set bit is a
     * set pixel and a clear bit is a cleared pixel
     */
    char_bitmap = font_data + c*16;
    x = x_org;
    y = y_org;
    for (_y = y; _y < y + VGA_FONT_HEIGHT; _y++) {
        for (_x = x; _x < x + VGA_FONT_WIDTH; _x++) {
            if (char_bitmap[_y - y] & (1 << (VGA_FONT_WIDTH - (_x - x)))) {
                vga_put_pixel(_win, _x, _y, fg_color);
            }
            else {
                if (!transparent)
                    vga_put_pixel(_win, _x, _y, bg_color);
            }
        }
    }
}

/*
 * Decorate a window
 * Parameter:
 * @win - window
 * @title - title which will be printed into the windows header bar
 */
void vga_decorate_window(win_t* win, char* title) {
    u32 frame_color;
    u32 title_bg;
    int x;
    if (VGA_MODE_TEXT == mode)
        return;
    if (0 == win)
        return;
    /*
     * White
     */
    frame_color = vga_vesa_color(255, 255, 255);
    /*
     * Left hand side
     */
    vga_draw_rectangle(&root_win, win->x_origin - WIN_FRAME_WIDTH, win->y_origin - WIN_TITLE_HEIGHT,
            WIN_FRAME_WIDTH - 1, WIN_TITLE_HEIGHT + win->height, frame_color);
    /*
     * Right hand side
     */
    vga_draw_rectangle(&root_win, win->x_origin + win->width + 1, win->y_origin - WIN_TITLE_HEIGHT,
            WIN_FRAME_WIDTH - 1, WIN_TITLE_HEIGHT + win->height, frame_color);
    /*
     * Bottom
     */
    vga_draw_rectangle(&root_win, win->x_origin - WIN_FRAME_WIDTH, win->y_origin + win->height + 1,
            win->width + 2 * WIN_FRAME_WIDTH, WIN_BOTTOM_HEIGHT - 1, frame_color);
    /*
     * Top - use dark grey here
     */
    frame_color = vga_vesa_color(102, 102, 102);
    vga_draw_rectangle(&root_win, win->x_origin, win->y_origin - WIN_TITLE_HEIGHT,
            win->width, WIN_TITLE_HEIGHT - 1, frame_color);
    /*
     * Now write the title
     */
    if (0 == title)
        return;
    x = 0;
    frame_color = vga_vesa_color(255, 255, 255);
    title_bg = vga_vesa_color(0, 0, 0);
    while (title[x]) {
        if (x < win->char_width) {
            setchar_win(&root_win, x * VGA_FONT_WIDTH + win->x_origin, win->y_origin - WIN_TITLE_HEIGHT + 5, title[x], 1, frame_color,
                    title_bg);
        }
        x++;
    }
}



/*
 * Initialize a window
 * Parameter:
 * @win - the window
 * @x_origin, y_origin: upper left corner with respect to root window
 * @x_resolution, y_resolution - resolution in pixels
 */
void vga_init_win(win_t* win, u32 x_origin, u32 y_origin, u32 x_resolution, u32 y_resolution) {
    win->cursor_x = 0;
    win->cursor_y = 0;
    win->char_height = y_resolution / 16;
    win->char_width = x_resolution / 8;
    win->height = y_resolution;
    win->width = x_resolution;
    win->x_origin = x_origin;
    win->y_origin = y_origin;
    spinlock_init(&win->lock);
    /*
     * Initialize settings
     */
    win->cons_settings.bg_rgb = VGA_COLOR_BLACK;
    win->cons_settings.fg_rgb = VGA_STD_ATTRIB;
    win->cons_settings.blink = 0;
    win->cons_settings.bold = 0;
    win->cons_settings.reverse = 0;
    win->cons_settings.char_attr = VGA_STD_ATTRIB;
    win->cons_settings.blank_attr = VGA_COLOR_BLACK;
}



/******************************************************************************************
 * The following functions form a common access layer for the console driver              *
 * They are accessed via function pointers which are set by vga_init() depending on       *
 * whether we are in text or graphics mode                                                *
 *****************************************************************************************/


/*
 * Print a character at position specified by @line and @column
 * @line=0,@column=0 is the upper left position)
 * Parameter:
 * @win - the window to which we print ( 0 == console)
 * @line - line on the screen
 * @column - column on the screen
 * @c - character to print
 * @blank - set this to 1 to print character with "blank attribute"
 */
static void setchar_impl_text(win_t* win, u32 line, u32 column, char c, int blank) {
    win_t* _win = &root_win;
    if (win)
        _win = win;
    /*
     * Each character corresponds to 2 bytes
     * in memory: character and attribute
     */
    u8* ptr = VGA_VIDEO_MEM + (u8*) (2 * VGA_COLS * line + 2 * column);
    *ptr++ = c;
    if (0 == blank)
        *ptr = _win->cons_settings.char_attr;
    else
        *ptr = _win->cons_settings.blank_attr;
}
static void setchar_impl_graphics(win_t* win, u32 line, u32 column, char c, int blank) {
    win_t* _win = &root_win;
    if (win)
        _win = win;
    vga_hide_cursor(_win);
    if (0 == blank)
        setchar_win(_win, column * VGA_FONT_WIDTH, line * VGA_FONT_HEIGHT, c, 0, _win->cons_settings.fg_vesa_color_char,
                _win->cons_settings.bg_vesa_color_char);
    else
        setchar_win(_win, column * VGA_FONT_WIDTH, line * VGA_FONT_HEIGHT, c, 0, _win->cons_settings.fg_vesa_color_blank,
                        _win->cons_settings.bg_vesa_color_blank);

}
void (*vga_setchar)(win_t*, u32, u32, char, int) = setchar_impl_text;

/*
 * Copy a character from location (c1, l1) to
 * location (c2, l2)
 * Parameter:
 * @win - the window to use
 * @c1 - column of source character
 * @l1 - line of target character
 * @c2 - column of source character
 * @l2 - line of source character
 */
static void vid_copy_impl_text(win_t* win, u32 c1, u32 l1, u32 c2, u32 l2) {
    u8* ptr1;
    u8* ptr2;
    ptr1 = (u8*) (VGA_VIDEO_MEM + 2 * VGA_COLS * l1 + 2 * c1);
    ptr2 = (u8*) (VGA_VIDEO_MEM + 2 * VGA_COLS * l2 + 2 * c2);
    *ptr2 = *ptr1;
    *(ptr2 + 1) = *(ptr1 + 1);
}
static void vid_copy_impl_graphics(win_t* win, u32 c1, u32 l1, u32 c2, u32 l2) {
    unsigned int src_offset;
    unsigned int target_offset;
    unsigned char* src;
    unsigned char* src_shadow;
    unsigned char* target_shadow;
    unsigned char* target;
    int line;
    int byte;
    win_t* _win = &root_win;
    if (win)
        _win = win;
    vga_hide_cursor(_win);
    /*
     * We first compute the source and target address for the upper left corner of the character,
     * first byte
     */
    src_offset = (l1 * VGA_FONT_HEIGHT + _win->y_origin) * vbe_mode->bytesPerScanLine +
            (c1 * VGA_FONT_WIDTH + _win->x_origin) * (vbe_mode->bitsPerPixel / 8);
    target_offset = (l2 * VGA_FONT_HEIGHT + _win->y_origin) * vbe_mode->bytesPerScanLine +
            (c2 * VGA_FONT_WIDTH + _win->x_origin) * (vbe_mode->bitsPerPixel / 8);
    src = (unsigned char*) (src_offset + frame_buffer_base);
    target = (unsigned char*) (target_offset + frame_buffer_base);
    src_shadow = (unsigned char*) (src_offset + shadow);
    target_shadow = (unsigned char*) (target_offset + shadow);
    /*
     * Now copy the data, update shadow as well as real video mem
     */
    for (line = 0; line < VGA_FONT_HEIGHT; line ++) {
        for (byte = 0; byte < vbe_mode->bitsPerPixel; byte++) {
            *target = *src_shadow;
            *target_shadow = *src_shadow;
            target++;
            target_shadow++;
            src++;
            src_shadow++;
        }
        target -= vbe_mode->bitsPerPixel;
        target += vbe_mode->bytesPerScanLine;
        src -= vbe_mode->bitsPerPixel;
        src += vbe_mode->bytesPerScanLine;
        target_shadow -= vbe_mode->bitsPerPixel;
        target_shadow += vbe_mode->bytesPerScanLine;
        src_shadow -= vbe_mode->bitsPerPixel;
        src_shadow += vbe_mode->bytesPerScanLine;
    }
}
void (*vga_vid_copy)(win_t*, u32, u32, u32, u32) = vid_copy_impl_text;

/*
 * Copy line X to line Y
 * Parameter:
 * @win - the window
 * @l1 - source line
 * @l2 - target line
 */
static void vid_copy_line_impl_text(win_t* win, u32 l1, u32 l2) {
    int j;
    for (j = 0; j < 80; j++) {
        vga_vid_copy(0, j, l1, j, l2);
    }
}
static void vid_copy_line_impl_graphics(win_t* win, u32 l1, u32 l2) {
    u32 target_offset;
    u32 src_offset;
    u32 target_addr;
    u32 src_addr_shadow;
    u32 target_addr_shadow;
    int line;
    int qwords_per_line;
    unsigned long long* target;
    unsigned long long* src_shadow;
    unsigned long long* target_shadow;
    int i;
    win_t* _win = &root_win;
    if (win)
        _win = win;
    vga_hide_cursor(_win);
    target_offset = (l2 * VGA_FONT_HEIGHT + _win->y_origin) * vbe_mode->bytesPerScanLine + _win->x_origin * vbe_mode->bitsPerPixel / 8;
    src_offset = (l1 * VGA_FONT_HEIGHT + _win->y_origin) * vbe_mode->bytesPerScanLine + _win->x_origin * vbe_mode->bitsPerPixel / 8;
    target_addr =  target_offset + frame_buffer_base;
    src_addr_shadow =  src_offset + ((u32) shadow);
    target_addr_shadow = target_offset + ((u32) shadow);
    /*
     * Number of quad words which we need to copy per line. The number of pixels we need to
     * copy is the number of characters per line times the number of pixels per character
     * We get the number of bits by multiplying this by the bits per pixel value specific
     * to the VBE mode and finally divide by the number of bits in a quad word
     */
    qwords_per_line = _win->char_width  * VGA_FONT_WIDTH * vbe_mode->bitsPerPixel / (sizeof(long long int) * 8);
    /*
     * Copy source to target, one qword at a time. Use shadow as source
     * and update it as well. If available, use non-temporal SSE store instructions if USE_SSE is defined
     *
     * Note that we assume at this point that we already hold a spinlock on the window and
     * can therefore not be interrupted. Thus it is safe to use the MMX registers. Note however that
     * this will destroy the state of the FPU. As restoring the state is slow, USE_SSE is currently not set
     *
     */
    for (line = 0 ; line < VGA_FONT_HEIGHT; line++) {
        target = (unsigned long long*)(target_addr);
        src_shadow = (unsigned long long*) (src_addr_shadow);
        target_shadow = (unsigned long long*)(target_addr_shadow);
        for (i = 0; i < qwords_per_line; i++) {
            target_shadow[i] = src_shadow[i];
            if (0 == use_mmx)
                target[i] = src_shadow[i];
            else
                asm("movq %0, %%mm0 ; movntq %%mm0, %1" : : "m" (src_shadow[i]) , "m" (target[i]));
        }
        target_addr += vbe_mode->bytesPerScanLine;
        src_addr_shadow += vbe_mode->bytesPerScanLine;
        target_addr_shadow += vbe_mode->bytesPerScanLine;
    }
}
void (*vga_vid_copy_line)(win_t*, u32, u32) = vid_copy_line_impl_text;

/*
 * Set the hardware text cursor to x / y location
 * We do this by writing to the cursor address high
 * and cursor address low location
 * Parameter:
 * @x - line
 * @y - column
 */
static void set_hw_cursor_impl_text(win_t* win, int x, int y) {
    if ((x<0) || (x>=VGA_COLS) || (y<0) || (y>VGA_LAST_LINE))
        return;
    u32 location = x + VGA_COLS*y;
    /*
     * Write low byte of location
     */
    outb(VGA_CRT_CURSOR_LOW, VGA_CRT_INDEX);
    outb((u8) location, VGA_CRT_DATA);
    /*
     * Write high byte of location
     */
    outb(VGA_CRT_CURSOR_HIGH, VGA_CRT_INDEX);
    outb((u8)(location >> 8), VGA_CRT_DATA);
}
static void set_hw_cursor_impl_graphics(win_t* win, int x, int y) {
    vga_show_cursor(win);
}
void (*vga_set_hw_cursor)(win_t*, int, int) = set_hw_cursor_impl_text;

/*
 * Hide the cursor
 */
static void hide_hw_cursor_impl_text(win_t* win) {
}
static void hide_hw_cursor_impl_graphics(win_t* win) {
    vga_hide_cursor(win);
}
void (*vga_hide_hw_cursor)(win_t*) = hide_hw_cursor_impl_text;

/*
 * Toggle cursor on a window (only relevant for software controlled cursor
 * mode if graphics mode is used)
 * Parameter:
 * @win - the window
 */
void vga_toggle_cursor(win_t* win) {
    u32 eflags;
    win_t* _win = &root_win;
    if (VGA_MODE_TEXT == mode)
        return;
    if (win)
        _win = win;
    spinlock_get(&_win->lock, &eflags);
    if (_win->cursor_visible)
        vga_hide_cursor(_win);
    else
        vga_show_cursor(_win);
    spinlock_release(&_win->lock, &eflags);
}

/*
 * Turn off cursor for a window
 * Parameter:
 * @win - the window
 */
void vga_no_cursor(win_t* win) {
    win_t* _win = &root_win;
    if (win)
        _win = win;
    _win->no_cursor = 1;
}

/*
 * Set the cursor for a window
 * Parameter:
 * @win - the window
 * @x - new x coordinate
 * @y - new y coordinate
 */
void vga_set_cursor(win_t* win, u32 x, u32 y) {
    u32 eflags;
    win_t* _win = &root_win;
    if (win)
        _win = win;
    if ((x >= _win->char_width) || (y >= _win->char_height))
        return;
    spinlock_get(&_win->lock, &eflags);
    vga_hide_hw_cursor(_win);
    _win->cursor_x = x;
    _win->cursor_y = y;
    vga_set_hw_cursor(_win, x, y);
    spinlock_release(&_win->lock, &eflags);
}

/****************************************************************************************
 * Initialization                                                                       *
 ***************************************************************************************/

/*
 * These symbols are defined in testrm.S and mark the beginning and the
 * end of the code to return to real mode in the kernel ELF file
 */
extern u32 _rm_switch_start;
extern u32 _rm_switch_end;

/*
 * Switch to real mode, invoke a BIOS function and go back to protected mode
 * See the code in rm.S
 * Parameter:
 * @function - the function to be passed to the code in rm.S
 * Return value:
 * 0 if BIOS call was successful
 * 1 if an error occurred
 * Note that the function is NOT the VBE BIOS function! See the comments in rm.S
 * for a list of available functions
 */
static u16 call_bios(int function) {
    int i;
    /*
     * Copy our code to 0x7C00
     */
    int bytes = ((int)&_rm_switch_end) - ((int)&_rm_switch_start);
    for (i = 0; i < bytes; i++) {
        ((char*)(0x7c00))[i] = ((char*)(&_rm_switch_start))[i];
    }
    /*
     * Store function at address 0x10000
     */
    *((u16*) 0x10000) = function;
    /*
     * and call code
     */
    asm("call 0x7c00");
    /*
     * Finally get result back from 0x10000
     */
    return *((u16*) 0x10000);
}

/*
 * Given a VBE mode, determine whether the mode is supported
 * and return a pointer to the VGA mode structure for it
 * Parameter:
 * @vbe_mode - VBE mode
 * Return value:
 * VGA mode or NULL if not supported
 */
static vga_mode_t* mode_supported(vbe_mode_info_t* vbe_mode) {
    int i;
    if (0 == vbe_mode)
        return 0;
    for (i = 0 ; i < sizeof(valid_modes) / sizeof(vga_mode_t); i++) {
        /*
         * A video mode is supported if
         * - it is supported by the hardware (bit 0 of mode attributes)
         * - it appears in the list of supported modes
         * - it has a linear frame buffer
         * - it is a graphics mode (bit 4 of mode attributes)
         * - it has direct color
         */
        if ((vbe_mode->xResolution == valid_modes[i].x_resolution) && (vbe_mode->yResolution == valid_modes[i].y_resolution)
                && (vbe_mode->bitsPerPixel == valid_modes[i].bpp) && (vbe_mode->physBasePtr) &&
                (VESA_DIRECT_COLOR == vbe_mode->memoryModel) && (vbe_mode->modeAttributes & 0x11)) {
            return valid_modes+i;
        }
    }
    return 0;
}

/*
 * Evaluate the kernel parameter vga. This function will
 * walk the list of supported modes and mark the mode
 * with priority 0 which has been selected using the vga
 * kernel parameter. It returns the value of the parameter
 * Return value:
 * value of kernel parameter vga
 */
static int evaluate_kparm() {
    int vga = params_get_int("vga");
    int i;
    if (0 == vga)
        return 0;
    for (i = 0; i < sizeof(valid_modes) / sizeof(vga_mode_t); i++) {
        if (valid_modes[i].mode == vga)
            valid_modes[i].choice = 0;
    }
    return vga;
}

/*
 * Read font data from VGA BIOS
 */
static void bios_read_font() {
    int i;
    /*
     * Call BIOS using our real mode stub. This will
     * copy the font data to the linear address 0x10006
     */
    call_bios(BIOS_VGA_GET_FONT);
    /*
     * Copy the data from 0x10006 to our font data array
     */
    for (i = 0; i < 256*16; i++) {
        font_data[i] = ((u8*) 0x10006)[i];
    }
}

/*
 * Determine the preferred mode and switch to it.
 * Return value:
 * 0 upon success
 * 1 upon failure
 */
static int vbe_switch_mode() {
    int rc;
    int i;
    u16* videoModePtr;
    u16 good_mode;
    vga_mode_t* vga_mode;
    static u16 vbeMode[512];
    int last_priority = 255;
    vbe_info_block_t* vbe_info;
    /*
     * Get font data first
     */
    bios_read_font();
    /*
     * Get VBE info block
     */
    if (1 == (rc = call_bios(BIOS_VBE_GET_INFO))) {
        ERROR("VBE call GET INFO failed\n");
        return 1;
    }
    /*
     * Save list of video modes, as subsequent calls might
     * overwrite our memory at 0x10000
     */
    if (0 == (vbe_info = (vbe_info_block_t*) (0x10004))) {
        ERROR("VBE call GET INFO delivered invalid info block\n");
        return 1;
    }
    if (0 == (videoModePtr = (u16*) FAR_PTR_TO_ADDR(vbe_info->videoModePtr))) {
        ERROR("VBE delivered invalid video mode list\n");
        return 1;
    }
    i = 0;
     while (videoModePtr[i] != 0xFFFF) {
         if (i < 512)
             vbeMode[i] = videoModePtr[i];
         i++;
     }
     if (i < 512)
         vbeMode[i]=0xFFFF;
     /*
       * Now determine supported modes
       */
      i = 0;
      while (vbeMode[i] != 0xFFFF) {
          *((u16*)(0x10002)) = vbeMode[i];
          call_bios(BIOS_VBE_GET_MODE);
          vbe_mode = (vbe_mode_info_t*) (0x10004);
          /*
           * If the mode is supported, add mode number and physical base pointer to our
           * internal list
           */
          if ((vga_mode = mode_supported(vbe_mode))) {
              vga_mode->vbe_mode_number = videoModePtr[i];
              /*
               * If this mode has a higher priority than the previously
               * detected mode, use it
               */
              if (vga_mode->choice < last_priority) {
                  good_mode = vbeMode[i];
                  current_mode = *vbe_mode;
                  last_priority = vga_mode->choice;
                  frame_buffer_base = vbe_mode->physBasePtr;
              }
          }
          i++;
      }
      if (good_mode) {
          vbe_mode = &current_mode;
          /*
           * Write mode plus frame buffer bit at address 0x10002
           */
          *((u16*) 0x10002) = (good_mode & 0x1FF) + (1 << 14);
          /*
           * and call real mode function
           */
          if (0 == call_bios(BIOS_VBE_SELECT_MODE))
              return 0;
          ERROR("Switch to video mode %w failed\n", good_mode);
      }
      return 1;
}

/*
 * Initialize video driver
 * Parameter:
 * @mode_switch - set this to one to locate a graphics mode and switch to it
 */
void vga_init(int mode_switch, u32 multiboot_ptr) {
    multiboot_info_block_t* multiboot_info_block = (multiboot_info_block_t*) multiboot_ptr;
    if (multiboot_info_block) {
        if (FB_DATA_VALID(multiboot_info_block)) {
            multiboot_fb_addr_low = multiboot_info_block->framebuffer_addr_low;
            multiboot_fb_addr_high = multiboot_info_block->framebuffer_addr_high;
        }
    }
    /*
     * If no VGA mode is requested, return
     */
    if (0 == evaluate_kparm())
        return;
    if (mode_switch && (mode == VGA_MODE_TEXT)) {
        if (0 == vbe_switch_mode()) {
            mode = VGA_MODE_GRAPHICS;
            vga_set_hw_cursor = set_hw_cursor_impl_graphics;
            vga_hide_hw_cursor = hide_hw_cursor_impl_graphics;
            vga_setchar = setchar_impl_graphics;
            vga_vid_copy = vid_copy_impl_graphics;
            vga_vid_copy_line = vid_copy_line_impl_graphics;
            vga_init_win(&root_win, 0, 0, vbe_mode->xResolution, vbe_mode->yResolution);
            vga_clear_win(&root_win, 32, 0, 32);
        }
    }
}

/*
 * When we have turned on paging, this function is called by the startup sequence and
 * needs to map the frame buffer into virtual memory
 */
void vga_enable_paging() {
    if (VGA_MODE_GRAPHICS == mode) {
        u32 frame_buffer_size = vbe_mode->yResolution * vbe_mode->bytesPerScanLine + vbe_mode->xResolution * (vbe_mode->bitsPerPixel / 8);
        u32 virt_frame_buffer = mm_map_memio(vbe_mode->physBasePtr, frame_buffer_size);
        if (0 == virt_frame_buffer) {
            PANIC("Could not map frame buffer into virtual memory\n");
        }
        frame_buffer_base = virt_frame_buffer;
    }
}


/*
 * Return the resolution of the display
 * Parameter:
 * @x_resolution - X resolution will be stored here
 * @y_resolution - Y resolution will be stored here
 * @bpp - bits per pixel will be stored here
 * Return value:
 * 1 on success
 * 0 if we are in text mode
 */
int vga_get_mode(u32* x_resolution, u32* y_resolution, u32* bpp) {
    if (0 == vbe_mode)
        return 0;
    *x_resolution = vbe_mode->xResolution;
    *y_resolution = vbe_mode->yResolution;
    *bpp = vbe_mode->bitsPerPixel;
    return 1;
}


/***************************************************************
 * Everything below this line is for debugging only            *
 **************************************************************/

/*
 * Print some debugging output
 */
void vga_debug_regs() {
    u8 reg;
    PRINT("General VGA information\n");
    PRINT("-----------------------------------------------------\n");
    PRINT("Mode:                                 %x\n", mode);
    PRINT("Multiboot framebuffer address - low:  %x\n", multiboot_fb_addr_low);
    PRINT("Multiboot framebuffer address - high: %x\n", multiboot_fb_addr_high);
    PRINT("VGA registers\n");
    PRINT("-----------------------------------------------------\n");
    /*
     * Read the attribute mode control register. First read from input status
     * register 1 to reset the flip-flop which controls access to the address / data
     * register for the attribute controller
     */
    inb(VGA_INPUT_STATUS_REG1);
    /*
     * Now load index of register into address register and set IPAS bit
     */
    outb(VGA_ATTR_IPAS + VGA_ATTR_MODE_CTRL, VGA_ATTR_ADDRESS);
    /*
     * and read data from data register
     */
    reg = inb(VGA_ATTR_DATA_READ);
    PRINT("Attribute mode control register:    %x\n", reg);
    PRINT("Graphics mode:                      %d\n", reg & 0x1);
    PRINT("Monochrome emulation:               %d\n", (reg >> 1) & 0x1);
    PRINT("Enable line graphics character:     %d\n", (reg >> 2) & 0x1);
    PRINT("Enable blinking:                    %d\n", (reg >> 3) & 0x1);
    PRINT("\nVBE information:\n");
    if (0 == vbe_mode) {
        PRINT("Not in graphics mode\n");
    }
    else {
        PRINT("Resolution:                         %d x %d @ %d bpp\n", vbe_mode->xResolution, vbe_mode->yResolution,
                vbe_mode->bitsPerPixel);
        PRINT("Physical frame buffer:              %x\n", vbe_mode->physBasePtr);
        PRINT("Red mask size and field position:   %d / %d\n", vbe_mode->redMaskSize, vbe_mode->redFieldPosition);
        PRINT("Green mask size and field position: %d / %d\n", vbe_mode->greenMaskSize, vbe_mode->greenFieldPosition);
        PRINT("Blue mask size and field position:  %d / %d\n", vbe_mode->blueMaskSize, vbe_mode->blueFieldPosition);
        PRINT("Memory model:                       %d\n", vbe_mode->memoryModel);
    }
}

