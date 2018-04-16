/*
 * lc.c
 *
 * Count lines from STDIN
 */

#include <stdio.h>

int main() {
    char c;
    int count = 0;
    while ((c=getc(stdin))!=EOF) {
        if ('\n'==c)
            count++;
    }
    printf("%d\n", count);
    return 0;
}
