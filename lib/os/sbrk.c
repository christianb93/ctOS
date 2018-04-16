/*
 * sbrk.c
 *
 */

#include "lib/os/syscalls.h"

unsigned int __ctOS_sbrk(unsigned int size) {
    return __ctOS_syscall(__SYSNO_SBRK, 1, size);
}
