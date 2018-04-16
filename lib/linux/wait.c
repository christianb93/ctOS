/*
 * wait.c
 */

#include "lib/sys/types.h"

#define __NR_waitpid          7

#define _syscall3(name,a,b,c) \
{ \
__asm__ volatile ("int $0x80" \
    : "=a" (__res) \
    : "a" (__NR_##name),"b" (a),"c" (b), "d" (c)); \
}

/*
 * Wait for process completion
 */
pid_t __ctOS_waitpid(pid_t pid, int* stat_loc, int options) {
    pid_t __res;
    _syscall3(waitpid, pid, stat_loc, options);
    return __res;
}
