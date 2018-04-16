/*
 * read.c
 */


#include "lib/sys/types.h"

/*
 * Taken from /usr/include/i386-linux-gnu/asm/unistd_32.h
 */
#define __NR_read         3

#define _syscall3(name,a,b,c) \
{ \
__asm__ volatile ("int $0x80" \
    : "=a" (__res) \
    : "a" (__NR_##name),"b" (a),"c" (b),"d" (c)); \
}


ssize_t __ctOS_read(int fd, char* buffer, size_t bytes) {
    ssize_t __res;
    _syscall3(read, fd, buffer, bytes);
    return __res;
}
