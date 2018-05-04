/*
 * test_vga.c
 *
 */

#include "kunit.h"
#include "ktypes.h"
#include "vga.h"
#include "console.h"
#include "locks.h"

/*
 * Function pointers in vga.c
 */
extern void (*vga_setchar)(win_t*, u32, u32, char, int);
extern void (*vga_vid_copy)(win_t*, u32, u32, u32, u32);
extern void (*vga_set_hw_cursor)(win_t*, int, int);
// extern void (*vga_del_chars)(win_t*, unsigned int);
extern void (*vga_hide_hw_cursor)(win_t*);

u32 _rm_switch_end = 0;
u32 _rm_switch_start = 0;

/*
 * Dummy for setchar
 */
static char myscreen[80][25];
static unsigned char attr[80][25];
static int log_setchar = 0;
void vga_setchar_dummy(win_t* win, u32 line, u32 column, char c, int blank) {
    if (log_setchar)
        printf("Column: %d Line: %d Blank: %d\n", column, line, blank);
    if ((column<80) && (line<25)) {
        myscreen[column][line]=c;
        if (0 == blank) {
            if (log_setchar)
                printf("Setting attribute at column %d, line %d to char. attribute %x\n", column, line, win->cons_settings.char_attr);
            attr[column][line]= win->cons_settings.char_attr;
        }
        else {
            if (log_setchar)
                printf("Setting attribute at column %d, line %d to blank attribute %x\n", column, line, win->cons_settings.blank_attr);
            attr[column][line]= win->cons_settings.blank_attr;
        }
    }
    else {
        printf("line %d, column %d: out of range\n", line, column);
    }
}

int cpu_has_feature(int cpuid, unsigned long long feature) {
    return 0;
}

int sched_get_load(int cpu) {
    return 20;
}

void trap() {

}

u32 mm_map_memio(u32 phys_base, u32 size) {
    return 0;
}

void kprintf(char* template, ...) {

}



/*
 * Copy content and attributes of location (c1, l1) to
 * location (c2, l2)
 */
static void vga_vid_copy_dummy(win_t* win, u32 c1, u32 l1, u32 c2, u32 l2) {
    // printf("Copying column %d, row %d to column %d, row %d\n", c1, l1, c2, l2);
    myscreen[c2][l2] = myscreen[c1][l1];
}


/*
 * Set and hide hardware cursor
 */
void vga_set_hw_cursor_dummy(win_t* win, int x, int y) {

}
void vga_hide_hw_cursor_dummy(win_t* win) {

}

/*
 * Dummy for parameter handling
 */
int params_get_int(char* s) {
    return 0;
}


void spinlock_get(spinlock_t* lock, u32* flags) {
}
void spinlock_release(spinlock_t* lock, u32* flags) {
}
void spinlock_init(spinlock_t* lock) {
}

/*
 * Testcase 1: init screen and print one character
 */
int testcase1() {
    vga_init(0, 0);
    cons_init();
    kputchar('a');
    ASSERT(myscreen[0][0]=='a');
    return 0;
}

/*
 * Testcase 2: init screen and print two characters
 */
int testcase2() {
    vga_init(0,0);
    cons_init();
    kputchar('a');
    kputchar('b');
    ASSERT(myscreen[0][0]=='a');
    ASSERT(myscreen[1][0]=='b');
    return 0;
}

/*
 * Testcase 3: init screen and print an entire line
 */
int testcase3() {
    vga_init(0,0);
    cons_init();
    int i;
    for (i=0;i<80;i++)
        kputchar('a');
    kputchar('b');
    ASSERT(myscreen[0][0]=='a');
    ASSERT(myscreen[79][0]=='a');
    ASSERT(myscreen[0][1]=='b');
    return 0;
}

/*
 * Testcase 4: Process ESC sequence ESC [nC - parameter provided
 */
int testcase4() {
    vga_init(0,0);
    cons_init();
    /*
     * Cursor should be at 0,0 now. Move it five characters to the right
     */
    kputchar(27);
    kputchar('[');
    kputchar('5');
    kputchar('C');
    /*
     * Next character should print at column 5
     */
    kputchar('a');
    ASSERT('a'==myscreen[5][0]);
    return 0;
}

/*
 * Testcase 5: Process ESC sequence ESC [nC - parameter not provided
 */
int testcase5() {
    vga_init(0,0);
    cons_init();
    /*
     * Cursor should be at 0,0 now. Move it one character to the right
     */
    kputchar(27);
    kputchar('[');
    kputchar('C');
    /*
     * Next character should print at column 1
     */
    kputchar('a');
    ASSERT('a'==myscreen[1][0]);
    return 0;
}

/*
 * Testcase 6: Process ESC sequence ESC [2J (clear screen)
 */
int testcase6() {
    int i,j;
    vga_init(0,0);
    cons_init();
    /*
     * Print three characters first
     */
    kputchar('a');
    kputchar('b');
    kputchar('c');
    /*
     * Clear screen
     */
    kputchar(27);
    kputchar('[');
    kputchar('2');
    kputchar('J');
    /*
     * Check
     */
    for (i=0;i<80;i++)
        for (j=0;j<25;j++)
            ASSERT(' '==myscreen[i][j]);
    return 0;
}

/*
 * Testcase 7: Process ESC sequence ESC [m;nH (set cursor)
 */
int testcase7() {
    int i,j;
    vga_init(0,0);
    cons_init();
    /*
     * Set cursor to position row 5, col 3, i.e. row 4, col 2 in our zero-based
     * convention
     */
    kputchar(27);
    kputchar('[');
    kputchar('5');
    kputchar(';');
    kputchar('3');
    kputchar('H');
    /*
     * and print a test character
     */
    kputchar('x');
    ASSERT('x'==myscreen[2][4]);
    return 0;
}

/*
 * Testcase 8: Process ESC sequence ESC [H (set cursor to origin);
 */
int testcase8() {
    int i,j;
    vga_init(0,0);
    cons_init();
    /*
     * Print something
     */
    kputchar('y');
    /*
     * Set cursor to position row 1, col 1
     */
    kputchar(27);
    kputchar('[');
    kputchar('H');
    /*
     * and print a test character
     */
    kputchar('x');
    ASSERT('x'==myscreen[0][0]);
    return 0;
}

/*
 * Testcase 9: Process ESC sequence ESC [0J (clear from cursor to end of screen)
 */
int testcase9() {
    int i,j;
    vga_init(0,0);
    cons_init();
    /*
     * Fill entire screen
     */
    for (i=0;i<25;i++)
        for (j=0;j<80;j++)
            kputchar('x');
    /*
     * Set cursor to position row 4, col 9
     */
    kputchar(27);
    kputchar('[');
    kputchar('5');
    kputchar(';');
    kputchar('1');
    kputchar('0');
    kputchar('H');
    /*
    * and erase everything from the cursor to the end of the screen,
    * including the cursor
    */
    kputchar(27);
    kputchar('[');
    kputchar('0');
    kputchar('J');
    /*
     * Now verify that after  row 4, column 8 has been cleared. First
     * verify that rows 0 - 3 are unchanged
     */
    for (i=0;i<4;i++)
        for (j=0;j<80;j++)
            ASSERT('x'==myscreen[j][i]);
    /*
     * Now check that in row 4, columns 0-8 are unchanged
     */
    for (i=0;i<9;i++)
        ASSERT('x'==myscreen[i][4]);
    /*
     * and that all other columns are cleared
     */
    for (i=9;i<80;i++)
        ASSERT(' '==myscreen[i][4]);
    /*
     * Finally check that all other rows are cleared
     */
    for (i=5;i<25;i++)
        for (j=0;j<80;j++)
            ASSERT(' '==myscreen[j][i]);
    return 0;
}

/*
 * Testcase 10: Process ESC sequence ESC [1J (clear from start of screen to cursor, not including cursor)
 */
int testcase10() {
    int i,j;
    vga_init(0,0);
    cons_init();
    /*
     * Fill entire screen
     */
    for (i=0;i<25;i++)
        for (j=0;j<80;j++)
            myscreen[j][i]='x';
    /*
     * Set cursor to position row 4, col 9
     */
    kputchar(27);
    kputchar('[');
    kputchar('5');
    kputchar(';');
    kputchar('1');
    kputchar('0');
    kputchar('H');
    /*
    * and erase everything from the start of the screen to the cursor
    * including the cursor
    */
    kputchar(27);
    kputchar('[');
    kputchar('1');
    kputchar('J');
    /*
     * Now verify that before row 4, column 10 everything has been cleared. First
     * verify that rows 0 - 3 are clear
     */
    for (i=0;i<4;i++)
        for (j=0;j<80;j++)
            ASSERT(' '==myscreen[j][i]);
    /*
     * Now check that in row 4, columns 0-9 are clear
     */
    for (i=0;i<10;i++)
        ASSERT(' '==myscreen[i][4]);
    /*
     * and that all other columns are unchanged
     */
    for (i=10;i<80;i++)
        ASSERT('x'==myscreen[i][4]);
    /*
     * Finally check that all other rows are unchanged
     */
    for (i=5;i<25;i++)
        for (j=0;j<80;j++) {
            ASSERT('x'==myscreen[j][i]);
        }
    return 0;
}

/*
 * Testcase 11: Process ESC sequence ESC [2A (move cursor up 2 lines)
 */
int testcase11() {
    int i,j;
    vga_init(0,0);
    cons_init();
    /*
     * Set cursor to position row 2, col 1
     */
    kputchar(27);
    kputchar('[');
    kputchar('3');
    kputchar(';');
    kputchar('2');
    kputchar('H');
    /*
     * Move up two lines
     */
    kputchar(27);
    kputchar('[');
    kputchar('2');
    kputchar('A');
    /*
     * Now we should be in row 0, col 1. Check this by printing x there
     */
    kputchar('x');
    ASSERT('x'==myscreen[1][0]);
    return 0;
}

/*
 * Testcase 12: Process ESC sequence ESC [2B (move cursor down 2 lines)
 */
int testcase12() {
    int i,j;
    vga_init(0,0);
    cons_init();
    /*
     * Set cursor to position row 2, col 1
     */
    kputchar(27);
    kputchar('[');
    kputchar('3');
    kputchar(';');
    kputchar('2');
    kputchar('H');
    /*
     * Move down two lines
     */
    kputchar(27);
    kputchar('[');
    kputchar('2');
    kputchar('B');
    /*
     * Now we should be in row 4, col 1. Check this by printing x there
     */
    kputchar('x');
    ASSERT('x'==myscreen[1][4]);
    return 0;
}

/*
 * Testcase 13: Process ESC sequence ESC [2C (move cursor right 2 cells)
 */
int testcase13() {
    int i,j;
    vga_init(0,0);
    cons_init();
    /*
     * Set cursor to position row 2, col 1
     */
    kputchar(27);
    kputchar('[');
    kputchar('3');
    kputchar(';');
    kputchar('2');
    kputchar('H');
    /*
     * Move right two cells
     */
    kputchar(27);
    kputchar('[');
    kputchar('2');
    kputchar('C');
    /*
     * Now we should be in row 2, col 3. Check this by printing x there
     */
    kputchar('x');
    ASSERT('x'==myscreen[3][2]);
    return 0;
}

/*
 * Testcase 14: Process ESC sequence ESC [2D (move cursor left 2 cells)
 */
int testcase14() {
    int i,j;
    vga_init(0,0);
    cons_init();
    /*
     * Set cursor to position row 2, col 8
     */
    kputchar(27);
    kputchar('[');
    kputchar('3');
    kputchar(';');
    kputchar('9');
    kputchar('H');
    /*
     * Move left two cells
     */
    kputchar(27);
    kputchar('[');
    kputchar('2');
    kputchar('D');
    /*
     * Now we should be in row 2, col 6. Check this by printing x there
     */
    kputchar('x');
    ASSERT('x'==myscreen[6][2]);
    return 0;
}

/*
 * Testcase 15: Process ESC sequence ESC [P (delete char at cursor)
 */
int testcase15() {
    int i,j;
    vga_init(0,0);
    cons_init();
    kputchar('x');
    kputchar('y');
    /*
     * Move cursor to start of line again
     */
    kputchar(27);
    kputchar('[');
    kputchar('H');
    /*
     * and delete one character
     */
    kputchar(27);
    kputchar('[');
    kputchar('P');
    /*
     * Line should now be y
     */
    ASSERT('y'==myscreen[0][0]);
    ASSERT(' '==myscreen[1][0]);
    return 0;
}

/*
 * Testcase 16: Process ESC sequence ESC [2P (delete two chars at cursor)
 */
int testcase16() {
    int i,j;
    vga_init(0,0);
    cons_init();
    kputchar('1');
    kputchar('2');
    kputchar('3');
    kputchar('4');
    /*
     * Move cursor to second column
     */
    kputchar(27);
    kputchar('[');
    kputchar('1');
    kputchar(';');
    kputchar('2');
    kputchar('H');
    /*
     * and delete two characters
     */
    kputchar(27);
    kputchar('[');
    kputchar('2');
    kputchar('P');
    /*
     * Line should now be 14
     */
    ASSERT('1'==myscreen[0][0]);
    ASSERT('4'==myscreen[1][0]);
    for (i=2;i<80;i++)
        ASSERT(' '==myscreen[i][0]);
    return 0;
}

/*
 * Testcase 17: Process ESC sequence ESC [@ (insert character at cursor)
 */
int testcase17() {
    int i,j;
    vga_init(0,0);
    cons_init();
    kputchar('1');
    kputchar('2');
    kputchar('3');
    kputchar('4');
    /*
     * Move cursor to second column
     */
    kputchar(27);
    kputchar('[');
    kputchar('1');
    kputchar(';');
    kputchar('2');
    kputchar('H');
    /*
     * and insert one character
     */
    kputchar(27);
    kputchar('[');
    kputchar('@');
    /*
     * Line should now be 1 234
     */
    ASSERT('1'==myscreen[0][0]);
    ASSERT(' '==myscreen[1][0]);
    ASSERT('2'==myscreen[2][0]);
    ASSERT('3'==myscreen[3][0]);
    ASSERT('4'==myscreen[4][0]);
    for (i=5;i<80;i++)
        ASSERT(' '==myscreen[i][0]);
    return 0;
}

/*
 * Testcase 18: Process ESC sequence ESC [2@ (insert two characters at cursor)
 */
int testcase18() {
    int i,j;
    vga_init(0,0);
    cons_init();
    kputchar('1');
    kputchar('2');
    kputchar('3');
    kputchar('4');
    /*
     * Move cursor to second column
     */
    kputchar(27);
    kputchar('[');
    kputchar('1');
    kputchar(';');
    kputchar('2');
    kputchar('H');
    /*
     * and insert two characters
     */
    kputchar(27);
    kputchar('[');
    kputchar('2');
    kputchar('@');
    /*
     * Line should now be 1  234
     */
    ASSERT('1'==myscreen[0][0]);
    ASSERT(' '==myscreen[1][0]);
    ASSERT(' '==myscreen[2][0]);
    ASSERT('2'==myscreen[3][0]);
    ASSERT('3'==myscreen[4][0]);
    ASSERT('4'==myscreen[5][0]);
    for (i=6;i<80;i++) {
        ASSERT(' '==myscreen[i][0]);
    }
    return 0;
}

/*
 * Testcase 19: Process ESC sequence ESC [n@ with maximum value of n
 */
int testcase19() {
   int i,j;
   vga_init(0,0);
   cons_init();
   kputchar('1');
   kputchar('2');
   kputchar('3');
   kputchar('4');
   /*
    * Move cursor to first column
    */
   kputchar(27);
   kputchar('[');
   kputchar('1');
   kputchar(';');
   kputchar('1');
   kputchar('H');
   /*
    * and insert 80 characters - this should clear the entire line
    */
   kputchar(27);
   kputchar('[');
   kputchar('8');
   kputchar('0');
   kputchar('@');
   /*
    * Line should now be clear
    */
   for (i=0;i<80;i++) {
       if (' '!= myscreen[i][0])
           printf("seem to have an issue with column %d\n",i);
       ASSERT(' '==myscreen[i][0]);
   }
   return 0;
}

/*
 * Testcase 20: Process ESC sequence ESC [n@ with maximum value of n minus 1
 */
int testcase20() {
   int i,j;
   vga_init(0,0);
   cons_init();
   kputchar('1');
   kputchar('2');
   kputchar('3');
   kputchar('4');
   /*
    * Move cursor to first column
    */
   kputchar(27);
   kputchar('[');
   kputchar('1');
   kputchar(';');
   kputchar('1');
   kputchar('H');
   /*
    * and insert 79 characters
    */
   kputchar(27);
   kputchar('[');
   kputchar('7');
   kputchar('9');
   kputchar('@');
   /*
    * Line should now be clear up to column 79
    */
   for (i=0;i<79;i++) {
       if (' '!= myscreen[i][0])
           printf("seem to have an issue with column %d\n",i);
       ASSERT(' '==myscreen[i][0]);
   }
   ASSERT('1'==myscreen[79][0]);
   return 0;
}

/*
 * Testcase 21: Process ESC sequence ESC [L (insert line at cursor)
 */
int testcase21() {
    int i,j;
    vga_init(0,0);
    cons_init();
    kputchar('1');
    kputchar('2');
    kputchar('3');
    kputchar('4');
    /*
     * Insert a line
     */
    kputchar(27);
    kputchar('[');
    kputchar('L');
    /*
     * second line should now be 1  234
     */
    ASSERT('1'==myscreen[0][1]);
    ASSERT('2'==myscreen[1][1]);
    ASSERT('3'==myscreen[2][1]);
    ASSERT('4'==myscreen[3][1]);
    /*
     * First line should be empty
     */
    for (i=0;i<80;i++)
        ASSERT(' '==myscreen[i][0]);
    return 0;
}

/*
 * Testcase 22: Process ESC sequence ESC [24L (insert 24 lines at cursor)
 */
int testcase22() {
    int i,j;
    vga_init(0,0);
    cons_init();
    kputchar('1');
    kputchar('2');
    kputchar('3');
    kputchar('4');
    /*
     * Insert 24 lines
     */
    kputchar(27);
    kputchar('[');
    kputchar('2');
    kputchar('4');
    kputchar('L');
    /*
     * last line should now be 1234
     */
    ASSERT('1'==myscreen[0][24]);
    ASSERT('2'==myscreen[1][24]);
    ASSERT('3'==myscreen[2][24]);
    ASSERT('4'==myscreen[3][24]);
    /*
     * all other lines should be empty
     */
    for (i=0;i<80;i++)
        for (j=0;j<24;j++) {
            if (' '!=myscreen[i][j])
                printf("something went wrong at line %d, column %d\n", j, i);
            ASSERT(' '==myscreen[i][j]);
        }
    return 0;
}

/*
 * Testcase 23: Process ESC sequence ESC [25L (insert 25 lines at cursor)
 */
int testcase23() {
    int i,j;
    vga_init(0,0);
    cons_init();
    kputchar('1');
    kputchar('2');
    kputchar('3');
    kputchar('4');
    /*
     * Insert 25 lines
     */
    kputchar(27);
    kputchar('[');
    kputchar('2');
    kputchar('5');
    kputchar('L');
    /*
     * all lines should be empty
     */
    for (i=0;i<80;i++)
        for (j=0;j<25;j++) {
            if (' '!=myscreen[i][j])
                printf("something went wrong at line %d, column %d\n", j, i);
            ASSERT(' '==myscreen[i][j]);
        }
    return 0;
}


/*
 * Testcase 24: Process ESC sequence ESC [M (delete line)
 */
int testcase24() {
    int i,j;
    vga_init(0,0);
    cons_init();
    kputchar('1');
    kputchar('2');
    kputchar('\n');
    kputchar('3');
    kputchar('4');
    /*
     * Go back to home position
     */
    kputchar(27);
    kputchar('[');
    kputchar('H');
    /*
     * Delete line
     */
    kputchar(27);
    kputchar('[');
    kputchar('M');
    /*
     * First line should be 34 now
     */
    ASSERT('3'==myscreen[0][0]);
    ASSERT('4'==myscreen[1][0]);
    /*
     * all other lines should be empty
     */
    for (i=1;i<25;i++)
        for (j=0;j<80;j++)
            ASSERT(' '==myscreen[j][i]);
    return 0;
}

/*
 * Testcase 25: Print a character with standard attributes
 */
int testcase25() {
    vga_init(0,0);
    cons_init();
    kputchar('1');
    ASSERT('1'==myscreen[0][0]);
    ASSERT(VGA_STD_ATTRIB==attr[0][0]);
    return 0;
}

/*
 * Testcase 26: Set foreground color to red and print a character
 */
int testcase26() {
    vga_init(0,0);
    cons_init();
    kputchar('\33');
    kputchar('[');
    kputchar('3');
    kputchar('1');
    kputchar('m');
    kputchar('1');
    ASSERT('1'==myscreen[0][0]);
    ASSERT(VGA_COLOR_RED==attr[0][0]);
    return 0;
}

/*
 * Testcase 27: Set background color to red and print a character
 */
int testcase27() {
    vga_init(0,0);
    cons_init();
    kputchar('\33');
    kputchar('[');
    kputchar('4');
    kputchar('1');
    kputchar('m');
    kputchar('1');
    ASSERT('1'==myscreen[0][0]);
    ASSERT((VGA_COLOR_RED<<4)+VGA_STD_ATTRIB==attr[0][0]);
    return 0;
}

int main() {
    int i,j;
    /*
     * Divert function pointers in vga.c to our stubs
     */
    vga_setchar = vga_setchar_dummy;
    vga_set_hw_cursor = vga_set_hw_cursor_dummy;
    vga_vid_copy = vga_vid_copy_dummy;
    vga_hide_hw_cursor = vga_hide_hw_cursor_dummy;
    /*
     * Clear screen
     */
    for (i=0;i<80;i++)
        for (j=0;j<25;j++)
            myscreen[i][j]=-1;
    /*
     * Run testcases
     */
    INIT;
    RUN_CASE(1);
    RUN_CASE(2);
    RUN_CASE(3);
    RUN_CASE(4);
    RUN_CASE(5);
    RUN_CASE(6);
    RUN_CASE(7);
    RUN_CASE(8);
    RUN_CASE(9);
    RUN_CASE(10);
    RUN_CASE(11);
    RUN_CASE(12);
    RUN_CASE(13);
    RUN_CASE(14);
    RUN_CASE(15);
    RUN_CASE(16);
    RUN_CASE(17);
    RUN_CASE(18);
    RUN_CASE(19);
    RUN_CASE(20);
    RUN_CASE(21);
    RUN_CASE(22);
    RUN_CASE(23);
    RUN_CASE(24);
    RUN_CASE(25);
    RUN_CASE(26);
    RUN_CASE(27);
    END;
}


