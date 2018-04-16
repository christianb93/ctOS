/*
 * unlink.c
 */

#include "lib/os/oscalls.h"

#define __NR_unlink      10

#define _syscall1(name,a) \
{ \
__asm__ volatile ("int $0x80" \
    : "=a" (__res) \
    : "a" (__NR_##name),"b" (a)); \
}

int __ctOS_unlink(char* path) {
    int __res;
    _syscall1(unlink, path);
    return __res;
}
