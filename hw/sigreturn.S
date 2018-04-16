/*
 * sigreturn.S
 *
 * This module contains the code which is placed on the stack when a signal
 * handler is installed
 */

.global __sigreturn_start
.global __sigreturn_end

__sigreturn_start:
    # get signal number from stack
    pop %ebx
    # advance stack pointer to consume the two additional
    # arguments of the handler (currently zero)
    pop %eax
    pop %eax
    # Stack pointer now points to sigframe structure
    mov %esp, %ecx
    # prepare invocation of sigreturn system call (get number from syscalls.h)
    mov $16, %eax
    int $0x80
__sigreturn_end:
    nop

