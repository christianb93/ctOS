/*
 * testargs.c
 *
 */
#include <unistd.h>
#include <stdio.h>

int main() {
    char *arguments[2] = { "a", "b", NULL };    
    if (execv("dumpargs", arguments)) {
        printf("execv returned with error code\n");
    }
}
