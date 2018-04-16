/*
 * dumptty.c
 *
 * Switch terminal to raw mode and dump each key until Ctrl-D is pressed
 */

#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>
#include <ctype.h>


struct termios term;

void sighandler(int signo) {
    if (SIGINT==signo) {
        printf("Interrupted, restoring original terminal settings\n");
        term.c_lflag |= (ICANON+ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &term);
        _exit(0);
    }
}

int main() {
    unsigned char c;
    int chars = 1;
    /*
     * Save old settings and install signal handler
     */
    tcgetattr(STDIN_FILENO, &term);
    signal(SIGINT, sighandler);
    /*
     * Turn off canonical mode and echo
     */

    term.c_lflag &= ~(ICANON+ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
    printf("Starting dump of keyboard input, hit Ctrl-D to stop\n");
    printf("---------------------------------------------------\n");
    while ( (chars = read(STDIN_FILENO, &c, 1))==1) {
        printf("Read character with ASCII code %d ", c);
        if (isprint(c)) {
            printf(" %c\n", c);
        }
        else {
            printf("<non-printable>\n");
        }
        if (4==c)
            break;
    }
    /*
     * Reset terminal
     */
    term.c_lflag |= (ICANON+ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}
