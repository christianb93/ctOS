/*
 * console.h
 *
 */

#ifndef _CONSOLE_H_
#define _CONSOLE_H_

#include "ktypes.h"
#include "vga.h"

/*
 * Settings
 */
typedef struct {
    char reverse;                     // Is "reverse mode" enabled?
    unsigned char fg_rgb;             // Foreground color as 3-bit RGB value
    unsigned char bg_rgb;             // Background color as 3-bit RGB value
    char blink;                       // Is "blinking" enabled?
    char bold;                        // Is "Bold" enabled?
    unsigned char char_attr;          // current attributes for characters
    unsigned char blank_attr;         // current attribute for blanks
    u32 fg_vesa_color_char;           // VESA color used as foreground for characters
    u32 fg_vesa_color_blank;          // VESA color used as foreground for blanks
    u32 bg_vesa_color_char;           // VESA color used as background for characters
    u32 bg_vesa_color_blank;          // VESA color used as background for blank
    int parser_state;                 // State of parser for escape sequences
    int parm0;                        // Parameter 0 of current escape sequence
    int have_parm0;
    int parm1;                        // Parameter 1 of current escape sequence
    int have_parm1;
    int wrap_around;                  // line wrap pending because last column has been filled
    int init;                         // has this structure been initialized?
} cons_settings_t;


/*
 * Font size. At the moment, only 8x16 fonts are supported
 */
#define VGA_FONT_WIDTH 8
#define VGA_FONT_HEIGHT 16

/*
 * This is a window
 */
typedef struct {
    u32 x_origin;
    u32 y_origin;
    u32 width;
    u32 height;
    u32 char_height;                  // Number of characters
    u32 char_width;                   // Number of character
    u32 cursor_x;                     // Text mode cursor x
    u32 cursor_y;                     // Text mode cursor y
    spinlock_t lock;                  // Lock to protect window state
    cons_settings_t cons_settings;    // Console settings
    u32 cursor_buffer[VGA_FONT_WIDTH];// buffer for content of cursor position
    int cursor_visible;
    int no_cursor;                    // 1 if cursor is suppressed for this window
} win_t;

/*
 * Macros to extract red, green and blue flags from a 3-bit RGB value
 */
#define RED(x)    ((x) >> 2)
#define GREEN(x)  (((x) & 0x2) >> 1)
#define BLUE(x)   ((x) & 0x1)

/*
 * Convert a 1-bit RGB component into an 8-bit value, using
 * an intensity bit
 */
#define RGB8(x, intensity)   ((x) * 0xB0 + (x)*(intensity)*0x4F)

/*
 * Tabsize. Tabs are at positions n*TABSIZE, n = 0, 1, ....
 */
#define TABSIZE 8

void cons_init();
void kputchar(u8 c);
void cls(win_t* win);
void win_putchar(win_t* win, u8 c);
void cons_cursor_tick();

#endif /* _CONSOLE_H_ */
