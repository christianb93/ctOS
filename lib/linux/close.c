/*
 * close.c
 */

#include "lib/os/oscalls.h"

#define __NR_close        6

#define _syscall1(name,a) \
{ \
__asm__ volatile ("int $0x80" \
    : "=a" (__res) \
    : "a" (__NR_##name),"b" (a)); \
}

int __ctOS_close(int fd) {
    int __res;
    _syscall1(close, fd);
    return __res;
}
