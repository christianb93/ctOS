/*
 * This is a very minimal crti.S. It only provides labels.
 * During linking, the order is
 * 
 * crti.o crtbegin.o <<actual program>> crtend.o crtn.o
 * 
 * Therefore the prologues for both _init and _fini are
 * taken from this file. We use the GCC standard prologue
 *
 */

.section .init
.globl	_init
.type	_init,@function
_init:
	push   %ebp
    mov    %esp,%ebp
    sub    $0x18,%esp    
    
.section .fini
.globl	_fini
.type	_fini,@function
_fini:
    push   %ebp
    mov    %esp,%ebp
    sub    $0x18,%esp    
