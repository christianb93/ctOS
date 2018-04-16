/*
 * setjmp.S
 *
 */

.global __ctOS_setjmp
.global __ctOS_longjmp


/*******************************************
 * Save current CPU state to jmp_buf       *
 * prototype:                              *
 * setjmp(char* jmp_buf)                   *
 * Arguments:                              *
 * @jmp_buf: buffer for CPU state          *
 *******************************************/
 __ctOS_setjmp:
    # set up stack frame
    push %ebp
    mov %esp, %ebp

    # Save ebx
    push %ebx

    # Copy register values to buffer. First move
    # address of buffer to eax
    mov 8(%ebp), %eax

    # Now copy integer registers - do not save EAX
    mov %ebx, 0(%eax)
    mov %ecx, 4(%eax)
    mov %edx, 8(%eax)
    mov %esi, 12(%eax)
    mov %edi, 16(%eax)

    # Store FPU state. First save current value of EAX in EBX,
    # then advance EAX until it points to the next 16 byte boundary
    # after offset 36
    mov %eax, %ebx
    add $35, %eax
align1:
    inc %eax
    mov %eax, %ecx
    shr $4, %ecx
    shl $4, %ecx
    cmp %ecx, %eax
    jne align1
    # now do fxsave
    fxsave 0(%eax)
    # restore EAX so that it points to start of save area again
    mov %ebx, %eax

    # Copy eflags
    pushf
    pop %ebx
    mov %ebx, 32(%eax)

    # As we have already overwritten ebp with esp,
    # we need to get the old value of ebp from
    # the stack.
    mov (%ebp), %ebx
    mov %ebx, 20(%eax)

    # Save stack pointer
    mov %ebp, 24(%eax)

    # Move return address to buffer
    # and set eax to zero
    mov 4(%ebp), %ebx
    mov %ebx, 28(%eax)
    xor %eax, %eax

    # restore regs and return
    pop %ebx
    leave
    ret

/*******************************************
 * Get current CPU state from jmp_buf      *
 * prototype:                              *
 * longjmp(char* jmp_buf, int val)         *
 * Arguments:                              *
 * @jmp_buf: buffer for CPU state          *
 * @val - return value of setjmp           *
 *******************************************/
 __ctOS_longjmp:
    # set up stack frame
    push %ebp
    mov %esp, %ebp


    # Get old stack pointer back
    mov 8(%ebp), %eax
    mov 24(%eax), %esp

    # Make stack pointer point to return address of
    # the setjmp call which created this buffer
    add $4, %esp

    # Overwrite return address with saved EIP so that
    # a ret will take us back to this point
    mov 28(%eax), %ebx
    mov %ebx, (%esp)

    # now restore FPU state saved with fxsave previously. First save
    # current value of EAX in EBX, then locate next point after offset
    # 35 which is 16 byte aligned - this is were we have saved the FPU state
    mov %eax, %ebx
    add $35, %eax
align2:
    inc %eax
    mov %eax, %ecx
    shr $4, %ecx
    shl $4, %ecx
    cmp %ecx, %eax
    jne align2
    # now do fxrstor
    fxrstor 0(%eax)
    # restore EAX so that it points to start of save area again
    mov %ebx, %eax


    # Restore integer registers from
    # buffer. Note that once we have done this,
    # we may no longer change any of them
    mov 0(%eax), %ebx
    mov 4(%eax), %ecx
    mov 8(%eax), %edx
    mov 12(%eax), %esi
    mov 16(%eax), %edi


    # Place second parameter in eax
    mov 12(%ebp), %eax
    # If eax is zero, set it to one
    test %eax, %eax
    jnz 0f
    mov $1, %eax
0:
    push %eax

    # Restore eflags and ebp. As we need ebx for this purpose,
    # we temporarily push it on the stack as well. We need to
    # restore eflags here because doing this earlier would result
    # in its value being overwritten by the test statement above
    # Also note that once we have restored ebp, we can no longer
    # access our arguments and the jump buffer
    push %ebx
    mov 8(%ebp), %eax
    mov 32(%eax), %ebx
    push %ebx
    popf
    pop %ebx
    mov 20(%eax), %ebp

    # As this has overwritten eax, restore it from stack
    pop %eax

    # and do jump
    ret

