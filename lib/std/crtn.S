/*
 * This is a very minimal crtn.S. It only provides labels.
 * During linking, the order is
 * 
 * crti.o crtbegin.o <<actual program>> crtend.o crtn.o
 * 
 * Therefore the epilogues for both _init and _fini are
 * taken from this file. We use the GCC standard epilogues
 *
 */

.section .init
    leave 
    ret
    
.section .fini
    leave
    ret
