/*
 * do_syscall.S
 *
 */

.global __do_syscall

/*******************************************
 * Set up registers and do syscall         *
 * do_syscall(eax, ebx, ecx, edx, esi)     *
 * Arguments:                              *
 * @eax - value to be stored in eax        *
 * @ebx - value to be stored in ebx        *
 * @ecx - value to be stored in ecx        *
 * @edx - value to be stored in edx        *
 * @esi - value to be stored in esi        *
 * @edi - value to be stored in edi        *
 *******************************************/
 __do_syscall:
    # set up stack frame and save used regs
    push %ebp
    mov %esp, %ebp

    push %ebx
    push %ecx
    push %edx
    push %esi
    push %edi

    # set up register and do syscall
    mov 8(%ebp), %eax
    mov 12(%ebp), %ebx
    mov 16(%ebp), %ecx
    mov 20(%ebp), %edx
    mov 24(%ebp), %esi
    mov 28(%ebp), %edi

    int $0x80

    # restore regs and return
    pop %edi
    pop %esi
    pop %edx
    pop %ecx
    pop %ebx
    leave
    ret
