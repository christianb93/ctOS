/*
 * io.S
 */
 .global outb
 .global inb
 .global outl
 .global inl
 .global inw
 .global outw

 /*******************************************
 * Write a byte to a port                   *
 * Parameter:                               *
 * @value - value to be written             *
 * @port - port number                      *
 *******************************************/
 outb:
    push %ebp
    mov %esp, %ebp

    push %eax
    push %edx


    mov 8(%ebp), %eax
    mov 12(%ebp), %edx
    outb %al, %dx

    pop %edx
    pop %eax

    leave
    ret

/********************************************
 * Read a byte from a port                  *
 * Parameter:                               *
 * @port - port number                      *
 *******************************************/
 inb:
    push %ebp
    mov %esp, %ebp

    push %edx

    mov 8(%ebp), %edx
    inb %dx, %al

    pop %edx

    leave
    ret

/********************************************
 * Read a word from a port                  *
 * Parameter:                               *
 * @port - port number                      *
 *******************************************/
 inw:
    push %ebp
    mov %esp, %ebp

    push %edx

    mov 8(%ebp), %edx
    inw %dx, %ax

    pop %edx

    leave
    ret

/*******************************************
 * Write a word to a port                  *
 * Parameter:                               *
 * @value - value to be written             *
 * @port - port number                      *
 *******************************************/
 outw:
    push %ebp
    mov %esp, %ebp

    push %eax
    push %edx

    mov 8(%ebp), %eax
    mov 12(%ebp), %edx
    outw %ax, %dx

    pop %edx
    pop %eax

    leave
    ret

 /*******************************************
 * Write a dword to a port                  *
 * Parameter:                               *
 * @value - value to be written             *
 * @port - port number                      *
 *******************************************/
 outl:
    push %ebp
    mov %esp, %ebp

    push %eax
    push %edx

    mov 8(%ebp), %eax
    mov 12(%ebp), %edx
    outl %eax, %dx

    pop %edx
    pop %eax

    leave
    ret

/********************************************
 * Read a dword from a port                 *
 * Parameter:                               *
 * @port - port number                      *
 *******************************************/
 inl:
    push %ebp
    mov %esp, %ebp

    push %edx

    mov 8(%ebp), %edx
    inl %dx, %eax

    pop %edx

    leave
    ret
