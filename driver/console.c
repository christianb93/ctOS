/*
 * console.c
 *
 * The console driver. The console driver operates on a window which is initialized at boot time. Its main interface function is
 * kputchar, which adds a character to the console window, handling, special characters and escape sequences. However, the
 * console driver is able to print into an arbitrary window if needed
 *
 * The only non-trivial part of this module is the handling of escape sequences. For this purpose, a state machine is used. The
 * following states are defined
 *
 * S0: no escape received yet
 * S1: escape received, waiting for CSI or two-character command
 * S2: CSI (control sequence introducer [) has been received
 * S3: first parameter received
 * S4: second parameter received, processing command
 *
 * The state machines states is stored in an instance of the data structure cons_settings_t. Along with the parsers state, this
 * data structure also contains the currently active foreground and background colors (encoded as "VGA attributes" as needed for
 * 80x25 VGA text mode) and mode flags for reserved mode, bold and underline.
 *
 * More precisely, the settings structure contains the following fields which determine foreground and background color and font
 * appeareance:
 *
 * fg_rgb - this is the foreground color as 3-bit RGB value
 * bg_rgb - this is the background color as 3-bit RGB value
 * blink - turn on blinking
 * bold - turn on bold
 * reverse - turn on reverse mode, i.e. use foreground color for background and vice versa
 *
 * These fields are changed by an application using escape sequences. Whenever one of these fields changes, the following derived
 * fields in in the settings structure are updated as well:
 *
 * char_attr: VGA text mode attribute byte used for printing characters
 * blank_attr: VGA text mode attribute byte used for blanks (for instance when inserting blank characters or lines)
 * fg_vesa_color_char: VESA color used for foreground pixels when a character is displayed
 * fg_vesa_color_blank: VESA color used for foreground pixels when a blank is inserted
 * bg_vesa_color_char: VESA color used for background pixels when a character is displayed
 * bg_vesa_color_blank: VESA color used for background pixels when a blank is inserted
 *
 *
 * Here the first two fields are relevant for text mode, whereas the remaining four fields are used for graphics mode.
 *
 *
 * Each window (i.e. instance of win_t) can act as an independed console and contains its own copy of the console data structure.
 * Thus all functions in this module effectively operate on a window. At boot time, a dedicated console window is established, this
 * window is used if a NULL window is passed to any of the functions in this module.
 */


#include "ktypes.h"
#include "vga.h"
#include "console.h"
#include "params.h"
#include "lib/ctype.h"
#include "io.h"
#include "kprintf.h"

/*
 * The following function pointers are exported by the VGA driver in vga.c
 * They are used by the console driver for the actual output processing and
 * are valid in text mode as well as in graphics mode
 */
extern void (*vga_setchar)(win_t*, u32, u32, char, int);
extern void (*vga_vid_copy)(win_t*, u32, u32, u32, u32);
extern void (*vga_vid_copy_line)(win_t*, u32, u32);
extern void (*vga_set_hw_cursor)(win_t*, int, int);
extern void (*vga_hide_hw_cursor)(win_t*);

/*
 * Control debugging port
 */
static int use_debug_port = 0;
static int use_vbox_port = 0;

/*
 * The console window
 */
static win_t console_win;

/*
 * State of escape sequence parser
 */
#define PARSER_STATE_S0 0
#define PARSER_STATE_S1 1
#define PARSER_STATE_S2 2
#define PARSER_STATE_S3 3


/*
 * In addition to the various ways a VGA color can be described (see the comments at the top of vga.c), ANSI / VT-100 escape
 * sequences use a slightly different color coding. This array maps these color codes to the corresponding 3-bit RGB VGA colors
 */
static unsigned char ansi_to_vga[] = { VGA_COLOR_BLACK, VGA_COLOR_RED, VGA_COLOR_GREEN, VGA_COLOR_YELLOW, VGA_COLOR_BLUE,
        VGA_COLOR_MAGENTA, VGA_COLOR_CYAN, VGA_COLOR_WHITE };




/****************************************************************************************
 * The following set of functions is provides basic functionality like scrolling and    *
 * inserting of characters and lines. All functions accept a window as parameter.       *
 * If NULL is specified, the console window is used                                     *
 ***************************************************************************************/

/*
 * Send a string to the BOCHS debug port
 * Parameter:
 * @s - the string to be printed
 */
static void cdebug(char* s) {
    int i = 0;
    if (use_debug_port) {
        while (s[i]) {
            outb(s[i++], 0xe9);
        }
    }
}


/*
 * Scroll up one line (forward scrolling), i.e. discard the top line, add a new line
 * at the bottom and move all other lines up
 * Parameter:
 * @win - the window on which we operate
 */
static void scroll_up(win_t* win) {
    win_t* _win = &console_win;
    if (win)
        _win = win;
    u32 i, j;
    for (i = 1; i < _win->char_height; i++) {
        /*
         * copy line i to line i-1
         */
        vga_vid_copy_line(_win, i, i-1);
    }
    /* fill last line with empty spaces */
    for (j = 0; j < _win->char_width; j++) {
        vga_setchar(_win, _win->char_height - 1,  j, ' ', 1);
    }
}



/*
 * Scroll down one line (reverse scrolling), i.e. discard the bottom line, add a new line
 * at the top and move all other lines down
 * Parameter:
 * @win - the window on which we operate
 */
static void scroll_down(win_t* win) {
    win_t* _win = &console_win;
    u32 i, j;
    if (win)
        _win = win;
    for (i = _win->char_height - 1; i >0; i--) {
        /* copy line i-1 to line 1 */
        for (j = 0; j < _win->char_width; j++) {
            vga_vid_copy(win, j, i-1, j, i);
        }
    }
    /* fill first line with empty spaces */
    for (j = 0; j < _win->char_width; j++) {
        vga_setchar(win, 0, j, ' ', 1);
    }
}

/*
 * Delete n chars at cursor and move remainder of line to the left
 * Parameter:
 * @win - the window on which we operate
 * @n - the number of characters to be deleted
 */
static void del_chars(win_t* win, unsigned int n) {
    win_t* _win = &console_win;
    int j;
    if (win)
        _win = win;
    if (n > _win->char_width - _win->cursor_x)
        n = _win->char_width - _win->cursor_x;
    /*
     * Scroll entire line by n positions to the left, starting at cursor_x
     */
    for (j = _win->cursor_x; j < _win->char_width - n; j++) {
        vga_vid_copy(_win, j+n, _win->cursor_y, j, _win->cursor_y);
    }
    /*
     * Blank out last n characters
     */
    for (j = _win->char_width - 1;j >= _win->char_width - n; j--)
        vga_setchar(_win, _win->cursor_y, j, ' ', 1);
}


/*
 * Delete n lines at cursor
 * Parameter:
 * @win - the window on which we operate
 * @n - the number of lines to be deleted
 */
static void del_lines(win_t* win, unsigned int n) {
    int j;
    int i;
    win_t* _win = &console_win;
    if (win)
        _win = win;
    if (n > _win->char_height - _win->cursor_y)
        n = _win->char_height - _win->cursor_y;
    /*
     * Scroll entire screen up by n lines, starting at cursor_y
     */
    for (j = _win->cursor_y; j <= _win->char_height - 1 -n; j++) {
        for (i = 0; i < _win->char_width; i++)
            vga_vid_copy(_win, i, j+n, i, j);
    }
    /*
     * Blank out last n lines
     */
    for (j = _win->char_height - 1; j > _win->char_height - 1 - n; j--)
        for (i = 0; i< _win->char_width; i++)
            vga_setchar(_win, j, i, ' ', 1);
}

/*
 * Insert n chars at cursor and move remainder of line to the right
 * Parameter:
 * @win - the window on which we operate
 * @n - the number of characters
 */
static void ins_chars(win_t* win, unsigned int n) {
    int j;
    win_t* _win = &console_win;
    if (win)
        _win = win;
    /*
     * We can insert at most screen width - cursor_x characters
     */
    if (n > _win->char_width - _win->cursor_x)
        n = _win->char_width - _win->cursor_x;
    /*
     * Scroll entire line by n positions to the right, starting at cursor_x
     */
    for (j = _win->char_width - 1; j >= _win->cursor_x + n; j--) {
        vga_vid_copy(_win, j-n, _win->cursor_y, j, _win->cursor_y);
    }
    /*
     * Blank out n characters at cursor position
     */
    for (j = _win->cursor_x; j < _win->cursor_x + n; j++)
        vga_setchar(_win, _win->cursor_y, j, ' ', 1);
}

/*
 * Insert n lines at cursor
 * Parameter:
 * @win - the window on which we operate
 * @n - the number of lines to be inserted
 */
static void ins_lines(win_t* win, unsigned int n) {
    int j;
    int i;
    win_t* _win = &console_win;
    if (win)
        _win = win;
    /*
     * We can insert at most screen height -cursor_y lines
     */
    if (n > _win->char_height - _win->cursor_y)
        n = _win->char_height - _win->cursor_y;
    /*
     * Scroll all lines down by one line starting at line cursor_y + n
     */
    for (j = _win->char_height - 1;j >= _win->cursor_y + n; j--) {
        for (i = 0; i < _win->char_width; i++)
            vga_vid_copy(_win, i, j-n, i, j);
    }
    /*
     * Blank out n lines starting at line cursor_y
     */
    for (j = _win->cursor_y; j < _win->cursor_y + n; j++)
        for (i = 0; i < _win->char_width; i++)
            vga_setchar(_win, j, i, ' ', 1);
}



/*
 * Update attributes. Set character attribute byte and blank attribute byte
 * based on the current content of a provided console settings
 * Parameter:
 * @settings - the console settings on which we operate
 */
static void update_attr(cons_settings_t* settings) {
    if (0 == settings->reverse) {
        settings->char_attr = vga_text_attr(settings->fg_rgb, settings->bg_rgb, settings->bold, settings->blink);
        settings->fg_vesa_color_char = vga_vesa_color(RGB8(RED(settings->fg_rgb), settings->bold),
                RGB8(GREEN(settings->fg_rgb), settings->bold), RGB8(BLUE(settings->fg_rgb), settings->bold));
        settings->bg_vesa_color_char = vga_vesa_color(RGB8(RED(settings->bg_rgb), settings->bold),
                RGB8(GREEN(settings->bg_rgb), settings->bold), RGB8(BLUE(settings->bg_rgb), settings->bold));
    }
    else {
        /*
         * If reverse bit is set, use reversed coloring scheme for characters
         */
        settings->char_attr = vga_text_attr(settings->bg_rgb, settings->fg_rgb, settings->bold, settings->blink);
        settings->bg_vesa_color_char = vga_vesa_color(RGB8(RED(settings->fg_rgb), settings->bold),
                RGB8(GREEN(settings->fg_rgb), settings->bold), RGB8(BLUE(settings->fg_rgb), settings->bold));
        settings->fg_vesa_color_char = vga_vesa_color(RGB8(RED(settings->bg_rgb), settings->bold),
                RGB8(GREEN(settings->bg_rgb), settings->bold), RGB8(BLUE(settings->bg_rgb), settings->bold));
    }
    settings->blank_attr = vga_text_attr(settings->fg_rgb, settings->bg_rgb, 0, 0);
    settings->fg_vesa_color_blank = vga_vesa_color(RGB8(RED(settings->fg_rgb), settings->bold),
            RGB8(GREEN(settings->fg_rgb), settings->bold), RGB8(BLUE(settings->fg_rgb), settings->bold));
    settings->bg_vesa_color_blank = vga_vesa_color(RGB8(RED(settings->bg_rgb), settings->bold),
            RGB8(GREEN(settings->bg_rgb), settings->bold), RGB8(BLUE(settings->bg_rgb), settings->bold));
}

/*
 * Initialize attributes
 */
static void init_attr(cons_settings_t* settings) {
    if (0 == settings->init) {
        update_attr(settings);
        settings->init = 1;
    }
}

/*
 * Set cursor position
 * Parameter:
 * @win - the window
 * @x - x coordinate of new cursor position
 * @y - y coordinate of new cursor position
 */
static void set_cursor(win_t* win, int x, int y) {
    win_t* _win = &console_win;
    if (win)
        _win = win;
    _win->cursor_x = x;
    _win->cursor_y = y;
    if (_win->cursor_x < 0)
        _win->cursor_x =0;
    if (_win->cursor_x > _win->char_width - 1)
        _win->cursor_x = _win->char_width - 1;
    if (_win->cursor_y < 0)
        _win->cursor_y = 0;
    if (_win->cursor_y > _win->char_height - 1)
        _win->cursor_y = _win->char_height - 1;
}

/*
 * Move cursor to the next tab stop, or to the right margin 
 * if there are no more tab stops in the current line
 */
static void move_to_next_tab(win_t* win) {
    if (0 == win)
        return;
    int next_tab = (win->cursor_x / TABSIZE) * TABSIZE + TABSIZE;
    if (next_tab >= win->char_width) 
        next_tab = win->char_width;
    win->cursor_x = next_tab;
}

/****************************************************************************************
 * The next few functions provide the core functionality of the console driver, in      *
 * particular they handle the parsing of escape sequences                               *
 ***************************************************************************************/

/*
 * Write a character to the screen and do scrolling without parsing
 * esc sequences
 * Parameter:
 * @win - the window on which we operate
 * @c - the character to be printed
 */
static void plain_putchar(win_t* win, u8 c) {
    win_t* _win = &console_win;
    if (win)
        _win = win;
    /*
     * Process newline. Note that this will also clear
     * the wrap_around bit
     */
    if (c == '\n') {
         _win->cursor_y++;
         _win->cursor_x = 0;
         _win->cons_settings.wrap_around = 0;
     }
    /*
     * If wrap_around bit is set do line wrap now.
     */
    if (_win->cons_settings.wrap_around) {
        _win->cursor_x = 0;
        _win->cursor_y++;
        _win->cons_settings.wrap_around = 0;
    }
    if (_win->cursor_y > _win->char_height - 1) {
        scroll_up(_win);
        _win->cursor_y--;
    }
    /*
     * Print character if it is a printable character
     */
    if ((c > 0x1f) && (c < 0x7f)) {
        vga_setchar(_win, _win->cursor_y, _win->cursor_x, c, 0);
        if (_win->cursor_x < _win->char_width - 1)
            _win->cursor_x += 1;
        else
            _win->cons_settings.wrap_around = 1;
    }
    if (c == '\r')
        _win->cursor_x = 0;
    if (127 == c) {
        if (_win->cursor_x > 0) {
            _win->cursor_x--;
            vga_setchar(_win, _win->cursor_y, _win->cursor_x, ' ', 1);
        }
    }
    if (('\b' == c)  && (_win->cursor_x)) {
        _win->cursor_x--;
    }
    if ('\t' == c)
        move_to_next_tab(win);
}


/*
 * Set attribute. This functions accepts an integer
 * which is the parameter of an ESC [m escape sequence
 * and performs the respective action
 * Parameter:
 * @win - the window on which we operate
 * @n - the parameter of the ESC [ m escape sequence
 */
static void set_attr(win_t* win, int n) {
    win_t* _win = &console_win;
    if (win)
        _win = win;
    switch (n) {
        case 0:
            _win->cons_settings.reverse = 0;
            _win->cons_settings.bold = 0;
            _win->cons_settings.blink = 0;
            _win->cons_settings.bg_rgb = VGA_COLOR_BLACK;
            _win->cons_settings.fg_rgb = VGA_STD_ATTRIB;
            break;
        case 1:
            /*
             * Bold - we do this by setting the "intensity bit"
             */
            _win->cons_settings.bold = 1;
            break;
        case 4:
            /*
             * Underline. Not supported
             */
            break;
        case 5:
            /*
             * Blink
             */
            _win->cons_settings.blink = 1;
            break;
        case 7:
            /*
             * Reverse background and foreground vga_color
             */
            _win->cons_settings.reverse = 1;
            break;
        default:
            break;
    }
    /*
     * Handle colors
     */
    if ((n >= 30) && (n < 50)) {
        if (n == 39)
            n = 30 + VGA_STD_ATTRIB;
        if (n == 49)
            n = 30 + (VGA_STD_ATTRIB >> 4);
        if (n < 38) {
            /*
             * Change foreground vga_color, keep background vga_color
             */
            _win->cons_settings.fg_rgb = ansi_to_vga[n-30];
        }
        if ((n >= 40) && (n < 48)) {
            _win->cons_settings.bg_rgb = ansi_to_vga[n-40];
        }
    }
    update_attr(&_win->cons_settings);
}

/*
 * Process an ESC sequence command character
 * Parameter:
 * @win - the window to use
 * @c - the command character
 */
static void process_esc_command(win_t* win, char c) {
    int i,j;
    win_t* _win = &console_win;
    if (win)
        _win = win;
    switch (c) {
        case 'A':
            if ((0 == _win->cons_settings.have_parm0) || (0 == _win->cons_settings.parm0))
                _win->cons_settings.parm0 = 1;
            if (_win->cons_settings.parm0 > _win->cursor_y)
                _win->cons_settings.parm0 = _win->cursor_y;
            _win->cursor_y -= _win->cons_settings.parm0;
            break;
        case 'B':
            if ((0 == _win->cons_settings.have_parm0) || (0 == _win->cons_settings.parm0))
                _win->cons_settings.parm0 = 1;
            _win->cursor_y += _win->cons_settings.parm0;
            if (_win->cursor_y > _win->char_height - 1)
                _win->cursor_y = _win->char_height - 1;
            break;
        case 'C':
            if ((0 == _win->cons_settings.have_parm0) || (0 == _win->cons_settings.parm0))
                _win->cons_settings.parm0 = 1;
            _win->cursor_x += _win->cons_settings.parm0;
            if (_win->cursor_x >= _win->char_width)
                _win->cursor_x = _win->char_width - 1;
            _win->cons_settings.wrap_around = 0;
            break;
        case 'D':
            if ((0 == _win->cons_settings.have_parm0) || (0 == _win->cons_settings.parm0))
                _win->cons_settings.parm0 = 1;
            if (_win->cons_settings.parm0 > _win->cursor_x)
                _win->cons_settings.parm0 = _win->cursor_x;
            _win->cursor_x -= _win->cons_settings.parm0;
            _win->cons_settings.wrap_around = 0;
            break;
        case 'J':
            if (0 == _win->cons_settings.have_parm0)
                _win->cons_settings.parm0 = 0;
            if (0 == _win->cons_settings.parm0) {
                /*
                 * Clear from cursor to end of screen, including cursor position.
                 * First we clear all characters in the current line starting at the
                 * cursor position
                 */
                for (i = _win->cursor_x; i < _win->char_width; i++)
                    vga_setchar(_win, _win->cursor_y, i, ' ', 1);
                /*
                 * Now clear all lines below cursor_y
                 */
                for (i = _win->cursor_y + 1; i <= _win->char_height - 1; i++)
                    for (j = 0;j < _win->char_width; j++)
                        vga_setchar(_win, i, j, ' ', 1);
            }
            else if (1 == _win->cons_settings.parm0) {
                /*
                  * Clear from start of screen to cursor, including cursor position.
                  * First we clear all characters in the current line until we reach the
                  * cursor position
                  */
                 for (i = 0; i <= _win->cursor_x; i++)
                     vga_setchar(_win, _win->cursor_y, i, ' ', 1);
                 /*
                  * Now clear all lines above cursor_y
                  */
                 for (i = 0; i < _win->cursor_y; i++)
                     for (j = 0; j < _win->char_width; j++)
                         vga_setchar(_win, i, j, ' ', 1);
            }
            else if (2 == _win->cons_settings.parm0) {
                /*
                 * Do NOT simply call cls as this tries to get the lock
                 * which we already have...
                 */
                for (i = 0; i < _win->char_width; i++)
                    for (j = 0; j <= _win->char_height - 1; j++)
                        vga_setchar(_win, j, i, ' ', 1);
            }
            _win->cons_settings.wrap_around = 0;
            break;
        case 'H':
            if (0 == _win->cons_settings.have_parm0)
                _win->cons_settings.parm0 = 1;
            if (0 == _win->cons_settings.have_parm1)
                _win->cons_settings.parm1 = 1;
            set_cursor(_win, _win->cons_settings.parm1 - 1, _win->cons_settings.parm0 - 1);
            _win->cons_settings.wrap_around = 0;
            break;
        case 'm':
            if (_win->cons_settings.have_parm0)
                set_attr(_win, _win->cons_settings.parm0);
            if (_win->cons_settings.have_parm1)
                set_attr(_win, _win->cons_settings.parm1);
            _win->cons_settings.wrap_around = 0;
            break;
        case 'P':
            if ((0 == _win->cons_settings.have_parm0) || (0 == _win->cons_settings.parm0))
                _win->cons_settings.parm0 = 1;
            /*
             * Delete n chars at cursor position
             */
            del_chars(_win, _win->cons_settings.parm0);
            _win->cons_settings.wrap_around = 0;
            break;
        case 'M':
            if ((0 == _win->cons_settings.have_parm0) || (0 == _win->cons_settings.parm0))
                _win->cons_settings.parm0 = 1;
            /*
             * Delete n lines at cursor position
             */
            del_lines(_win, _win->cons_settings.parm0);
            _win->cons_settings.wrap_around = 0;
            break;
        case '@':
            if ((0 == _win->cons_settings.have_parm0) || (0 == _win->cons_settings.parm0))
                _win->cons_settings.parm0 = 1;
            /*
             * Insert n chars at cursor position
             */
            ins_chars(_win, _win->cons_settings.parm0);
            _win->cons_settings.wrap_around = 0;
            break;
        case 'L':
             if ((0 == _win->cons_settings.have_parm0) || (0 == _win->cons_settings.parm0))
                _win->cons_settings.parm0 = 1;
             /*
              * Insert n lines at cursor position
              */
             ins_lines(_win, _win->cons_settings.parm0);
             _win->cons_settings.wrap_around = 0;
             break;
        case 'K':
            if (0 == _win->cons_settings.have_parm0)
                _win->cons_settings.parm0 = 0;
            if (0 == _win->cons_settings.parm0) {
                /*
                 * Clear from cursor to end-of-line, including cursor
                 */
                for (i = _win->cursor_x; i < _win->char_width; i++)
                    vga_setchar(_win, _win->cursor_y, i, ' ', _win->cons_settings.blank_attr);
            }
            if (1 == _win->cons_settings.parm0) {
                /*
                 * Clear from start of line to cursor, including cursor
                 */
                for (i = 0; i <= _win->cursor_x; i++)
                    vga_setchar(_win, _win->cursor_y, i, ' ', 1);
            }
            if (2 == _win->cons_settings.parm0) {
                /*
                 * Clear entire line
                 */
                for (i = 0;i < _win->char_width; i++)
                    vga_setchar(_win, _win->cursor_y, i, ' ', 1);
            }
            _win->cons_settings.wrap_around = 0;
            break;
        default:
            cdebug("CONSOLE DRIVER: Invalid escape command, last char was ");
            if (use_debug_port)
                outb(c, 0xe9);
            switch (_win->cons_settings.parser_state) {
                case PARSER_STATE_S0:
                    cdebug("\nParser state is S0\n");
                    break;
                case PARSER_STATE_S1:
                    cdebug("\nParser state is S1\n");
                    break;
                case PARSER_STATE_S2:
                    cdebug("\nParser state is S2\n");
                    break;
                case PARSER_STATE_S3:
                    cdebug("\nParser state is S3\n");
                    break;
                default:
                    cdebug("\nParser state is unknown\n");
                    break;

            }
            break;
    }
    _win->cons_settings.parser_state = PARSER_STATE_S0;
}

/****************************************************************************************
 * The public interface of the console driver. Other parts of the kernel can use it to  *
 * a) print a character into an arbitrary window, handling escape sequences and         *
 *    cursor positioning                                                                *
 * b) print a character into the standard console window                                *
 * c) clear the console window                                                          *
 * d) initialize the console                                                            *
 ***************************************************************************************/

/*
 * Write a character onto the screen, do scrolling if necessary
 * Parameter:
 * @c - the character to be written
 * Locks:
 * screen_lock - protect cursor position
 */
void win_putchar(win_t* win, u8 c) {
    u32 flags;
    u32 x_res;
    u32 y_res;
    u32 bpp;
    win_t* _win = &console_win;
    if (win)
        _win = win;
    /*
     * In text mode, refuse to print anything if the window
     * is not the console
     */
    if ((0 == vga_get_mode(&x_res, &y_res, &bpp)) && (_win != &console_win))
        return;
    /*
     * Get lock
     */
    spinlock_get(&_win->lock, &flags);
    /*
     * Make sure attributes have been initialized
     */
    init_attr(&_win->cons_settings);
    /*
     * Hide cursor
     */
    vga_hide_hw_cursor(_win);
    /*
     * Determine action based on current state of ESC
     * sequence parser
     */
    switch (_win->cons_settings.parser_state) {
        case PARSER_STATE_S0:
            /*
             * Ordinary processing, either advance to state
             * S1 if ESC is read or print character
             */
            if (27 == c) {
                _win->cons_settings.parser_state = PARSER_STATE_S1;
                _win->cons_settings.have_parm0 = 0;
                _win->cons_settings.parm0 = 0;
                _win->cons_settings.have_parm1 = 0;
                _win->cons_settings.parm1 = 0;
            }
            else {
                plain_putchar(_win, c);
                /*
                 * Send byte to Bochs debugging port 0xe9 if
                 * window is the console window
                 */
                if ((use_debug_port) && (_win == &console_win))
                    outb(c, 0xe9);
                /*
                 * and the same for Virtualbox
                 */
                if ((use_vbox_port) && (_win == &console_win))
                    outb(c, 0x504);
            }
            break;
        case PARSER_STATE_S1:
            /*
             * Expecting CSI character [
             */
            if ('[' == c) {
                _win->cons_settings.parser_state = PARSER_STATE_S2;
            }
            else if ('M' == c) {
                /*
                 * ESC M - scroll down if we are at top line
                 */
                if (0 == _win->cursor_y) {
                    _win->cons_settings.wrap_around = 0;
                    scroll_down(_win);
                }
                else
                    _win->cursor_y--;
                _win->cons_settings.parser_state = PARSER_STATE_S0;
            }
            else {
               _win->cons_settings.parser_state = PARSER_STATE_S0;
            }
            break;
        case PARSER_STATE_S2:
            /*
             * Expecting digits which make up the first parameter
             * or ; to advance to second parameter or a command
             */
            if (isdigit(c)) {
                _win->cons_settings.parm0 = _win->cons_settings.parm0*10 + (c-'0');
                _win->cons_settings.have_parm0 = 1;
            }
            else if (';' == c) {
                _win->cons_settings.parser_state = PARSER_STATE_S3;
            }
            else {
                process_esc_command(_win, c);
            }
            break;
        case PARSER_STATE_S3:
            /*
             * Expecting digits which make up the second parameter
             * or a command
             */
            if (isdigit(c)) {
                _win->cons_settings.parm1 = _win->cons_settings.parm1*10 + (c-'0');
                _win->cons_settings.have_parm1 = 1;
            }
            else {
                process_esc_command(_win, c);
            }
            break;
        default:
            plain_putchar(_win, c);
            break;
    }
    /*
     * Reposition cursor
     */
    vga_set_hw_cursor(_win, _win->cursor_x, _win->cursor_y);
    spinlock_release(&_win->lock, &flags);
}

/*
 * Clear screen
 * Locks:
 * screen_lock - protect cursor position
 */
void cls(win_t* win) {
    u32 flags;
    u32 i;
    u32 j;
    u32 x_res;
    u32 y_res;
    u32 bpp;
    win_t* _win = &console_win;
    if (win)
        _win = win;
    /*
     * In text mode, refuse to print anything if the window
     * is not the console
     */
    if ((0 == vga_get_mode(&x_res, &y_res, &bpp)) && (_win != &console_win))
        return;
    spinlock_get(&_win->lock, &flags);
    vga_hide_hw_cursor(_win);
    for (i = 0; i < _win->char_width; i++)
        for (j = 0; j < _win->char_height; j++)
            vga_setchar(_win, j, i, ' ', 1);
    _win->cursor_x = 0;
    _win->cursor_y = 0;
    vga_set_hw_cursor(_win, 0, 0);
    spinlock_release(&_win->lock, &flags);
}


/*
 * Write a character to the console window
 */
void kputchar(u8 c) {
    win_putchar(&console_win, c);
}

/*
 * Initialize the console driver. This is invoked by the VGA driver at boot time and needs to be done before kputchar
 * or kprintf is used for the first time
 */
void cons_init() {
    /*
     * Control debugging port
     */
    use_debug_port = params_get_int("use_debug_port");
    use_vbox_port = params_get_int("use_vbox_port");
    /*
     * Initialize console window at position 5/50
     */
    vga_init_win(&console_win, 50, 50, 640, 400);
    update_attr(&console_win.cons_settings);
    cls(&console_win);
    vga_decorate_window(&console_win, "Console");
}

/*
 * This function needs to be called periodically to support software cursor blinking
 */
void cons_cursor_tick()  {
    vga_toggle_cursor(&console_win);
}
