/*
 * testenv.c
 */


#include <stdio.h>

extern char** environ;

int main() {
    int i = 0;
    while (environ[i]) {
        printf("%s\n", environ[i]);
        i++;
    }
    printf("Found %d environment entries in total\n", i);
}


