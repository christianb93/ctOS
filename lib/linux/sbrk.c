/*
 * sbrk.c
 *
 */

#include "lib/os/oscalls.h"

#define __NR_brk         45


#define _syscall1(name,a) \
{ \
__asm__ volatile ("int $0x80" \
    : "=a" (__res) \
    : "a" (__NR_##name),"b" (a)); \
}

/*
 * Here we save the initial program break
 */
static unsigned int current_brk = 0;

/*
 * This is set by the linker
 */
extern unsigned int _end;

/*
 * Note that the brk system call in Linux returns the new system break
 */
unsigned int __ctOS_sbrk(unsigned int size) {
    unsigned int new_brk;
    unsigned int __res;
    /*
     * If this is the first call, set initial break
     * from linker symbol _end
     */
    if (0==current_brk) {
        current_brk = ((unsigned int) &_end);
        if (current_brk % 4096)
            current_brk = (current_brk / 4096)*4096 + 4096;
        _syscall1(brk, current_brk);
        current_brk = __res;
    }
    if (0==size)
        return current_brk;
    new_brk = current_brk + size;
    if (new_brk % 4096)
        new_brk = (new_brk / 4096)*4096+4096;
    _syscall1(brk, new_brk);
    if (__res>current_brk)
        return __res;
    else
        return 0;
}
