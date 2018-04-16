/*
 * dumpchars.c
 *
 *  Created on: Feb 19, 2012
 *      Author: chr
 */

#include <stdio.h>
#include <ctype.h>

int main() {
    int i;
    printf("Printing punctuation characters\n");
    for (i=0;i<256;i++) {
        if (ispunct(i)) {
            printf("%d  -->  %c\n", i, i);
        }
    }
    printf("Now printing a list of all characters which are in cntrl\n");
    for (i=0;i<256;i++) {
        if (iscntrl(i)) {
            printf("%d\n", i);
        }
    }
    printf("Now printing a list of all ASCII characters which are not in alpha, digit, punct and not a space\n");
    for (i=0;i<=127;i++) {
        if (!isalpha(i) && !isdigit(i) && !ispunct(i) && (i != ' ')) {
            printf("%d\n", i);
        }
    }
}
