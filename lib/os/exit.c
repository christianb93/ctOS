/*
 * exit.c
 */

#include "lib/os/syscalls.h"

void __ctOS__exit(int status) {
    __ctOS_syscall(__SYSNO_EXIT, 1,  status);
}
