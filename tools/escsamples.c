/*
 * escsamples.c
 *
 *  Samples for ANSI escape sequences
 */


#include <stdio.h>
#include <unistd.h>
#include <string.h>

void cls() {
    write(STDOUT_FILENO, "\33[2J", 4);
}


void home() {
    write(STDOUT_FILENO, "\33[H", 3);
}

/*
 * Move cursor to column x, row y
 * (zero-based)
 */
void moveto(int x, int y) {
    char ctl_string[128];
    snprintf(ctl_string, 128, "\33[%d;%dH", y+1, x+1);
    write(STDOUT_FILENO, ctl_string, strlen(ctl_string));
}

void cup(int lines) {
    char ctl_string[128];
    snprintf(ctl_string, 128, "\33[%dA", lines);
    write(STDOUT_FILENO, ctl_string, strlen(ctl_string));
}

void cdown(int lines) {
    char ctl_string[128];
    snprintf(ctl_string, 128, "\33[%dB", lines);
    write(STDOUT_FILENO, ctl_string, strlen(ctl_string));
}

void cright(int cols) {
    char ctl_string[128];
    snprintf(ctl_string, 128, "\33[%dC", cols);
    write(STDOUT_FILENO, ctl_string, strlen(ctl_string));
}

void cleft(int cols) {
    char ctl_string[128];
    snprintf(ctl_string, 128, "\33[%dD", cols);
    write(STDOUT_FILENO, ctl_string, strlen(ctl_string));
}

int main() {
    char mystring[16];
    cls();
    home();
    /*
     * Print three characters
     */
    write(STDOUT_FILENO, "123", 3);
    /*
     * Move cursor back to second column
     */
    moveto(1, 0);
    /*
     * and erase everything from cursor to end-of-line
     * including the cursor
     */
    mystring[0]=27;
    mystring[1]='[';
    mystring[2]='K';
    write(STDOUT_FILENO, mystring, 3);
    /*
     * Move cursor to position row 10, column 1
     */
    moveto(0, 9);
    /*
     * Print some stuff
     */
    write(STDOUT_FILENO, "abcde", 6);
    /*
     * Move cursor up one line
     */
    cup(1);
    /*
     * and print some more stuff
     */
    write(STDOUT_FILENO, "fg", 3);
    /*
     * Move cursor two characters to the right
     */
    cright(2);
    /*
     * and print some more stuff
     */
    write(STDOUT_FILENO, "h", 1);
    /*
     * Move cursor down two lines
     */
    cdown(2);
    /*
     * and print some more stuff
     */
    write(STDOUT_FILENO, "i", 1);
    /*
     * Move cursor left two characters
     */
    cleft(2);
    /*
     * and print some more stuff
     */
    write(STDOUT_FILENO, "j", 1);
    /*
     * Move cursor to position row 15, column 1
     */
    moveto(0, 14);
    /*
     * Print three characters
     */
    write(STDOUT_FILENO, "123", 3);
    /*
     * Move cursor to position row 15, column 2
     */
    moveto(1, 14);
    /*
     * and erase everything to the end of the screen
     * including the character at row 20, column 2 (i.e. '2')
     */
    mystring[0]=27;
    mystring[1]='[';
    mystring[2]='0';
    mystring[3]='J';
    write(STDOUT_FILENO, mystring, 4);
    /*
     * Move cursor to position row 20, column 1
     */
    moveto(0,19);
};
