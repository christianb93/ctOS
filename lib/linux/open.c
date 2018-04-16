/*
 * open.c
 *
 */

#include "lib/os/oscalls.h"

#define __NR_open         5

#define _syscall3(name,a,b, c) \
{ \
__asm__ volatile ("int $0x80" \
    : "=a" (__res) \
    : "a" (__NR_##name),"b" (a),"c" (b), "d" (c)); \
}

int __ctOS_open(char* path, int flags, int mode) {
    int __res;
    _syscall3(open, path, flags, mode);
    return __res;
}

int __ctOS_mkdir(char* path, int mode) {
    return -1;
}
