/*
 * exit.c
 */

/*
 * Taken from /usr/include/i386-linux-gnu/asm/unistd_32.h
 */
#define __NR_exit 1

#define _syscall1(name,a) \
{ \
__asm__ volatile ("int $0x80" \
    : "=a" (__res) \
    : "a" (__NR_##name),"b" (a)); \
}

void __ctOS__exit(int status) {
    int __res;
    _syscall1(exit, status);
}
