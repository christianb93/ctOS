/*
 * 
 * This program will print two rules onto the screen. The first ruler marks the
 * expected tab positions, the second one the actual tabl positions
 */


#include <stdio.h>

int main() {
    int tabsize = 8;
    printf("First ruler --> expected tab positions\n");
    printf("Second ruler --> actual tab positions\n");
    /*
     * Print first ruler: we expect a tab stop every
     * eight characters
     */
    for (int i = 0; i < 10; i++) {
        printf("|");
        for (int j = 0; j < (tabsize-1); j++)
            printf("-");
    }
    printf("\n");
    /*
     * Now print the actual tab positions
     */
    for (int i = 0; i < 10; i++)
        printf("|\t");
    printf("\n");
}