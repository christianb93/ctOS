/*
 * exec.c
 *
 */

#include "lib/os/syscalls.h"
#include "lib/errno.h"

/*
 * Execute a program image
 */
int __ctOS_execve(const char *path, char *const argv[], char *const envp[]) {
   int rc =  __ctOS_syscall(__SYSNO_EXECV, 3, path, argv, envp);
   /*
    * Whenever this returns, it is an error
    */
   errno = -rc;
   return -1;
}
