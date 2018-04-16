/*
 * lseek.c
 *
 */

#include "lib/os/oscalls.h"

#define __NR_lseek       19

#define _syscall3(name,a,b, c) \
{ \
__asm__ volatile ("int $0x80" \
    : "=a" (__res) \
    : "a" (__NR_##name),"b" (a),"c" (b), "d" (c)); \
}

off_t __ctOS_lseek(int fd, off_t offset, int whence) {
    int __res;
    _syscall3(lseek, fd, offset, whence);
    return __res;
}
