/*
 * strdup.c
 *
 */

#include "lib/string.h"
#include "lib/stdlib.h"
#include "lib/errno.h"

/*
 * ANSI C strdup - duplicate a string
 */
char* strdup(const char* src) {
    char* dest = (char*) malloc(strlen(src)+1);
    if (0==dest) {
        errno = ENOMEM;
        return 0;
    }
    strcpy(dest, src);
    return dest;
}
