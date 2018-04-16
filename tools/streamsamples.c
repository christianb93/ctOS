/*
 * streamsamples.c
 *
 */

#include <stdio.h>

int main() {
    int i;
    int c = 0;
    FILE* file;
    int pos;
    file = fopen("dummy", "w+");
    for (i=0;i<10;i++)
        fputc('a', file);
    fseek(file, 5, SEEK_SET);
    fputc('x', file);
    fclose(file);
    file = fopen("dummy", "r");
    fseek(file, 5, SEEK_SET);
    if ('x' != fgetc(file)) {
        printf("Error!\n");
        return 1;
    }
    fclose(file);
    return 0;
}
