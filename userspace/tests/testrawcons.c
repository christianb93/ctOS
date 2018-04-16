/*
 * testrawcons.c
 *
 * Test raw console input and ANSI ESC sequences
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>

struct termios term;


/*
 * Clear screen
 */
static void cls() {
    write(STDOUT_FILENO, "\33[2J", 4);
}

/*
 * Move to position 1,1
 */
static void home() {
    write(STDOUT_FILENO, "\33[H", 3);
}

/*
 * Scroll down
 */
static void sr() {
    write(STDOUT_FILENO, "\33M", 2);
}

/*
 * Bold font
 */
static void font_bold() {
    write(STDOUT_FILENO, "\33[1m", 4);
}



/*
 * Reverse
 */
static void font_reverse() {
    write(STDOUT_FILENO, "\33[7m", 4);
}

/*
 * Blinking
 */
static void font_blinking() {
    write(STDOUT_FILENO, "\33[5m", 4);
}

/*
 * Normal font
 */
static void font_normal() {
    write(STDOUT_FILENO, "\33[0m", 4);
}

/*
 * Set foreground color to red
 */
static void fg_red() {
    write(STDOUT_FILENO, "\33[31m", 5);
}

/*
 * Set background color to green
 */
static void bg_green() {
    write(STDOUT_FILENO, "\33[42m", 5);
}

/*
 * Set background color to black
 */
static void bg_black() {
    write(STDOUT_FILENO, "\33[40m", 5);
}

/*
 * Set foreground color to white
 */
static void fg_white() {
    write(STDOUT_FILENO, "\33[37m", 5);
}

/*
 * Signal handler - used to restore previous terminal settings
 */
void sighandler(int signo) {
    if (SIGINT==signo) {
        printf("Interrupted, restoring original terminal settings\n");
        term.c_lflag |= (ICANON+ECHO+ECHOCTL);
        tcsetattr(STDIN_FILENO, TCSANOW, &term);
        _exit(0);
    }
}

int main() {
    unsigned char c;
    int chars = 1;
    struct termios check;
    /*
     * Save old settings and install signal handler
     */
    tcgetattr(STDIN_FILENO, &term);
    signal(SIGINT, sighandler);
    /*
     * Turn off canonical mode, ECHO and ECHOCTL
     */
    term.c_lflag &= ~(ICANON+ECHOCTL+ECHO);
    if (tcsetattr(STDIN_FILENO, TCSANOW, &term)) {
        printf("tcsetattr failed\n");
        _exit(1);
    }
    /*
     * Check that this was successful
     */
    tcgetattr(STDIN_FILENO, &check);
    if (check.c_lflag & ICANON) {
        printf("Ups, canonical mode still active\n");
        _exit(1);
    }
    /*
     * Clear screen
     */
    cls();
    /*
     * Move to upper left position of screen
     */
    home();
    printf("Hit Ctrl-D to exit\n");
    printf("Navigation: \n");
    printf("Use cursor keys to position cursor and type any character to see it on the screen\n");
    printf("1 - go to upper left position of screen\n");
    printf("2 - go to upper right position of screen\n");
    printf("3 - go to lower right position of screen\n");
    printf("4 - go to lower left position of screen\n");
    printf("X - erase character at cursor position\n");
    printf("Y - erase two characters at cursor position\n");
    printf("S - scroll down (i.e. scroll reverse)\n");
    printf("I - insert blank character\n");
    printf("O - insert 79 blank characters\n");
    printf("L - insert blank line\n");
    printf("K - delete line\n");
    fg_red();
    bg_green();
    printf("Red text on green background\n");
    fg_white();
    bg_black();
    font_reverse();
    printf("Reverse video\n");
    font_normal();
    font_bold();
    printf("Bold font\n");
    font_normal();
    font_blinking();
    printf("Blinking\n");
    font_normal();
    while ( (chars = read(STDIN_FILENO, &c, 1))==1) {
        switch (c) {
            case '1':
                home();
                break;
            case '2':
                write(STDOUT_FILENO, "\33[1;80H", 7);
                break;
            case '3':
                write(STDOUT_FILENO, "\33[25;80H", 8);
                break;
            case '4':
                write(STDOUT_FILENO, "\33[25;1H", 7);
                break;
            case 'X':
                write(STDOUT_FILENO, "\33[P", 3);
                break;
            case 'Y':
                write(STDOUT_FILENO, "\33[2P", 4);
                break;
            case 'I':
                write(STDOUT_FILENO, "\33[@", 3);
                break;
            case 'O':
                write(STDOUT_FILENO, "\33[79@", 5);
                break;
            case 'S':
                sr();
                break;
            case 'L':
                write(STDOUT_FILENO, "\33[L", 3);
                break;
            case 'K':
                write(STDOUT_FILENO, "\33[M", 3);
                break;
            default:
                write(STDOUT_FILENO, (char*) &c, 1);
                break;
        }
        if (4==c)
            break;
    }
    /*
     * Reset terminal
     */
    term.c_lflag |= (ICANON+ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
    return 0;
}
