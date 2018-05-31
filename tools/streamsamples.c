/*
 * streamsamples.c
 *
 */

#include <stdio.h>
#include <stdio_ext.h>
#include <assert.h>
#include <unistd.h>

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
    /*
     * Now let us take a look at __fpurge. We first create a file
     * with a defined content
     */
    file = fopen("dummy", "w");
    fwrite("abcdef", 1, 5, file);
    fclose(file);
    /*
     * Now we open the file and read one byte, this 
     * should fill the buffer
     */
    file = fopen("dummy", "r");
    c = fgetc(file);
    assert(__fbufsize(file) > 5);
    assert(c == 'a');
    /*
     * Discard the buffer
     */
    __fpurge(file);
    /*
     * and read again
     */
    c = fgetc(file);
    /*
     * As the buffersize is larger than the full size,
     * this returns -1
     */
    assert(-1 == c);
    unlink("dummy");
    return 0;
}
