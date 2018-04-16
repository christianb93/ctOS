/*
 * dumptermcap.c
 *
 *
 * Print out a few termcap entries
 */

#include <stdio.h>
#include <termcap.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

static char* caps[] = {"cm", "up",  "do", "al", "sr", "ce", "ic", "dc", "ho", "ku", "ti", "te", "sc", "rc", "ks", "ke",  "sg", "ug",
        "rp",  "ll",  "vb", "af", "ac", "bc"};

#define QTY(x) (sizeof((x)) / sizeof(char*))
#define TERM "minix"

/*
 * Print an ESC sequence to stdout, converting
 * control characters to strings
 */
void print_esc_sequence(char* s) {
    int i;
    for (i=0;s[i];i++) {
        if (iscntrl(s[i])) {
            printf("^%c", s[i]+0x40);
        }
        else {
            printf("%c", s[i]);
        }
    }
}

void main() {
    int i;
    char* capstring = 0;
    char tbuffer[2048];
    char capbuffer[256];
    if (1 != tgetent(tbuffer, TERM)) {
        printf("Could not locate termcap entry for terminal %s\n", TERM);
        _exit(1);
    }
    printf("Print termcap entries for terminal type %s\n", TERM);
    printf("-----------------------------------------------------\n");
    for (i=0;i<QTY(caps);i++) {
        capstring = capbuffer;
        memset(capbuffer, 0, 256);
        if (0==tgetstr(caps[i], &capstring)) {
            printf("Could not find capability %s\n", caps[i]);
        }
        else {
            printf("Capability %s: ", caps[i]);
            print_esc_sequence(capbuffer);
            printf("\n");
        }
    }

}
