/*
 * syscall.c
 *
 */

#include "lib/sys/types.h"
#include "lib/stdarg.h"

extern unsigned int __do_syscall(unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int);

/*
 * Perform a system call
 * Parameter:
 * @__sysno - number of system call
 * @argc - number of arguments
 * @args - list of arguments
 * Return value:
 * result of system call
 */
static unsigned int syscall_impl(unsigned int __sysno, int argc, va_list args) {
    unsigned int higharg[2];
    unsigned int eax = 0;
    unsigned int ebx= 0;
    unsigned int ecx = 0;
    unsigned int edx = 0;
    unsigned int esi = 0;
    unsigned int edi = 0;
    eax = __sysno;
    if (argc>=1)
        ebx = va_arg(args, unsigned int);
    if (argc>=2)
        ecx = va_arg(args, unsigned int);
    if (argc>=3)
        edx = va_arg(args, unsigned int);
    if (argc>=4)
        esi = va_arg(args, unsigned int);
    /*
     * If we have five arguments, edi is last argument
     */
    if (argc==5) {
        edi = va_arg(args, unsigned int);
    }
    /*
     * else edi needs to point to an array of arguments
     */
    else {
        if (argc > 6)
            return -1;
        higharg[0] = va_arg(args, unsigned int);
        higharg[1] = va_arg(args, unsigned int);
        edi = (unsigned int) higharg;
    }
    return __do_syscall(eax, ebx, ecx, edx, esi, edi);
}

/*
 * Wrapper function to perform a system call
 * Parameters:
 * @__sysno: number of system call
 * @argc: number of parameters the systemcall expects
 * @...: parameters of the system call (up to 5)
 * Return value:
 * result of system call
 */
unsigned int __ctOS_syscall (unsigned int __sysno, int argc, ...) {
    va_list args;
    va_start(args, argc);
    return syscall_impl(__sysno, argc, args);
}
