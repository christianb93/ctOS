/*
 * ttysamples.c
 *
  Test behaviour of a TTY
 */

#include <stdio.h>
#include <termios.h>
#include <unistd.h>

int main() {
    int rc;
    char c;
    char buffer[128];
    struct termios old_termios;
    struct termios new_termios;
    /*
     * First make sure that terminal is in canonical mode
     */
    tcgetattr(STDIN_FILENO, &old_termios);
    new_termios = old_termios;
    if (new_termios.c_lflag & ICANON) {
        printf("Terminal is in canonical mode\n");
    }
    else {
        printf("Terminal is not in canonical mode\n");
        new_termios.c_lflag |= ICANON;
        tcsetattr(STDIN_FILENO, TCSANOW, &old_termios);
    }
    printf("Special character EOF is %d\n", old_termios.c_cc[VEOF]);
    /*
     * VINTR is 0 on Linux, see /usr/include/asm-generic/termbits
     */
    printf("Special character INTR is %d\n", old_termios.c_cc[VINTR]);
    printf("Special character SUSP is %d\n", old_termios.c_cc[VSUSP]);
    printf("Special character KILL is %d\n", old_termios.c_cc[VKILL]);
    /*
     * Test scanf
     */
    printf("Please enter a decimal number: \n");
    scanf("%d", &rc);
    printf("You entered %d\n", rc);
    /*
     * Now read lines from the terminal until an empty line is read
     */
    while ((rc=read(STDIN_FILENO, buffer, 2)) > 0) {
        printf("read %d characters \n", rc);
        buffer[rc]=0;
        printf("Read string %s\n", buffer);
    }
    printf("Received EOF, now switching to read from STDIN stream\n");
    while ((c=getc(stdin))!=EOF) {
        printf("Got character\n");
    }
    printf("done\n");
    /*
     * Restore original settings
     */
    tcsetattr(STDIN_FILENO, TCSANOW, &old_termios);
}

