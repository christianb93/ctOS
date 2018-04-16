/*
 * This module provides a few
 * assembler routines to access
 * special registers in the CPU like the
 * EFLAGS register, to turn on and off
 * interrupts and to perform atomic operations
 */

#include "gdt_const.h"


.global save_eflags
.global restore_eflags
.global get_eflags
.global sti
.global cli
.global xchg
.global atomic_incr
.global atomic_decr
.global get_cr3
.global put_cr3
.global get_cr0
.global put_cr0
.global enable_paging
.global disable_paging
.global reload_cr3
.global invlpg
.global goto_ring3
.global halt
.global reschedule
.global get_gs
.global set_gs
.global rdmsr
.global cpuid
.global load_tss
.global clts
.global setts
.global fpu_restore
.global fpu_save

/*******************************************
 * Save EFLAGS register                    *
 * prototype:                              *
 * save_eflags(u32* flags)                 *
 * Arguments:                              *
 * @flags: a pointer to a dword            *
 * in which we store the flags             *
 *******************************************/
 save_eflags:
    # set up stack frame and save used regs
    push %ebp
    mov %esp, %ebp
    # Save registers
    push %eax
    push %ebx

    # move EFLAGS to @flags
    pushf
    pop %ebx
    mov 8(%ebp),%eax
    mov %ebx, (%eax)

    # restore regs and return
    pop %ebx
    pop %eax
    leave
    ret

/*******************************************
 * Restore EFLAGS register                 *
 * prototype:                              *
 * save_eflags(u32* flags)                 *
 * Arguments:                              *
 * @flags: a pointer to a dword            *
 * in which we have store the flags        *
 *******************************************/
 restore_eflags:
    # set up stack frame and save used regs
    push %ebp
    mov %esp, %ebp
    push %eax
    push %ebx

    # move @flags to EFLAGS
    mov 8(%ebp),%eax
    mov (%eax), %ebx
    push %ebx
    popf

    # restore regs and return
    pop %ebx
    pop %eax
    leave
    ret

/*******************************************
 * Get EFLAGS register                     *
 * prototype:                              *
 * get_eflags()                            *
 *******************************************/
 get_eflags:
    pushf
    pop %eax
    ret

/*******************************************
 * Get GS register                         *
 * prototype:                              *
 * get_gs()                                *
 *******************************************/
 get_gs:
    xor %eax, %eax
    mov %gs, %ax
    ret

/*******************************************
 * Set GS register                         *
 * Prototype:                              *
 * set_gs(u16 gs)                          *
 *******************************************/
set_gs:
    # set up stack
    push %ebp
    mov %esp, %ebp

    # Save eax
    push %eax
    # Move new value of GS into eax
    mov 8(%ebp), %eax
    # and load it
    mov %ax, %gs

    pop %eax
    leave
    ret

/*******************************************
 * Clear CR0.TS                            *
 * Prototype:                              *
 * clts()                                  *
 *******************************************/
clts:
    clts
    ret

/*******************************************
 * Set CR0.TS                              *
 * Prototype:                              *
 * setts()                                 *
 *******************************************/
setts:
    # set up stack
    push %ebp
    mov %esp, %ebp

    # Save eax
    push %eax
    # Clear CR0 bit 3
    mov %cr0, %eax
    or $0x8, %eax
    mov %eax, %cr0

    pop %eax
    leave
    ret


/*******************************************
 * Restore FPU and SSE state               *
 * prototype:                              *
 * fpu_restore(u32 save_area)              *
 * Arguments:                              *
 * @save_area: address of save area        *
 *******************************************/
 fpu_restore:
    # set up stack frame and save used regs
    push %ebp
    mov %esp, %ebp
    # Save registers
    push %eax

    # get FPU state
    mov 8(%ebp),%eax
    fxrstor 0(%eax)

    # restore regs and return
    pop %eax
    leave
    ret


/*******************************************
 * Save FPU and SSE state                  *
 * prototype:                              *
 * fpu_save(u32 save_area)                 *
 * Arguments:                              *
 * @save_area: address of save area        *
 *******************************************/
 fpu_save:
    # set up stack frame and save used regs
    push %ebp
    mov %esp, %ebp
    # Save registers
    push %eax

    # save FPU state
    mov 8(%ebp),%eax
    fxsave 0(%eax)

    # restore regs and return
    pop %eax
    leave
    ret

/*******************************************
 * XCHG:                                   *
 * Prototype:                              *
 * xchg (int arg1, int *arg2)              *
 * This function will perform              *
 * the following operation:                *
 * temp = *arg2                            *
 * *arg2 = arg1                            *
 * return temp                             *
 * As the XCHG instruction is used,        *
 * this is atomic with respect to          *
 * the memory location address arg2        *
 * Some people would probably call         *
 * this test and set...                    *
 *******************************************/
 xchg:
    # set up stack
    push %ebp
    mov %esp, %ebp
    push %ebx

    mov 12(%ebp), %ebx
    mov 8(%ebp), %eax
    xchg %eax, (%ebx)

    pop %ebx
    leave
    ret

/*******************************************
 * atomic increment:                       *
 * Prototype:                              *
 * atomic_incr(reg_t* mem)                 *
 * This function will perform              *
 * an atomic increment of *mem             *
 *******************************************/
atomic_incr:
    # set up stack
    push %ebp
    mov %esp, %ebp

    # Save eax
    push %eax
    # Move address of mem into eax
    mov 8(%ebp), %eax
    lock incl (%eax)

    pop %eax
    leave
    ret

/*******************************************
 * atomic decrement:                       *
 * Prototype:                              *
 * atomic_decr(int* mem)                   *
 * This function will perform              *
 * an atomic decrement of *mem             *
 *******************************************/
atomic_decr:
    # set up stack
    push %ebp
    mov %esp, %ebp

    # Save eax
    push %eax
    # Move address of mem into eax
    mov 8(%ebp), %eax
    lock decl (%eax)

    pop %eax
    leave
    ret

 /******************************************
 * Enable hardware interrupts              *
 *******************************************/
 sti:
    sti
    ret

/*******************************************
 * Disable hardware interrupts             *
 *******************************************/
cli:
    cli
    ret

/*******************************************
 * Read CR3 register                       *
 *******************************************/
 get_cr3:
    mov %cr3, %eax
    ret

/*******************************************
 * Write CR3 register                      *
 * Parameter:                              *
 * @cr3 - the value to be written to CR3   *
 *******************************************/
 put_cr3:
    push %ebp
    mov %esp, %ebp
    push %eax
    mov 8(%ebp), %eax
    mov %eax,%cr3
    pop %eax
    leave
    ret

/*******************************************
 * Turn on paging and  set WP              *
 * Returns the new value of CR0            *
 *******************************************/
enable_paging:
    mov %cr0, %eax
    or $0x80000000, %eax
    or $0x10000, %eax
    mov %eax, %cr0
    ret

/*******************************************
 * Turn off paging                         *
 * Returns the new value of CR0            *
 *******************************************/
disable_paging:
    mov %cr0, %eax
    and $0x7fffffff, %eax
    ret


/*******************************************
 * Reload CR3 to flush TLB                 *
 * Return old (and new) value of CR3       *
 *******************************************/
 reload_cr3:
    mov %cr3, %eax
    mov %eax, %cr3
    ret

/*******************************************
 * Read CR0 register                       *
 *******************************************/
 get_cr0:
    mov %cr0, %eax
    ret


/*******************************************
 * Write CR0 register                      *
 * Parameter:                              *
 * @cr0 - the value to be written to CR0   *
 *******************************************/
 put_cr0:
    push %ebp
    mov %esp, %ebp
    push %eax
    mov 8(%ebp), %eax
    mov %eax,%cr0
    pop %eax
    leave
    ret

/*******************************************
 * Invalidate a TLB entry                  *
 * @page - virtual address within the      *
 * page for which the entry should be      *
 * invalidated
 *******************************************/
 invlpg:
    push %ebp
    mov %esp, %ebp
    push %eax

    mov 8(%ebp), %eax
    invlpg (%eax)

    pop %eax
    leave
    ret


/*******************************************
 * Switch to ring 3                        *
 * Parameters:                             *
 * @entry_point - entry point to jump to   *
 * @esp - stack pointer to use             *
 * We do the switch by tricking the CPU    *
 * into believing that we return from an   *
 * interrupt handler invoked from ring 3   *
 * Thus we built up a stack as follows:    *
 *       User space stack selector         *
 *       ESP to be used in ring 3          *
 *       EFLAGS                            *
 *       User space code selector          *
 *       EIP                               *
 * Note that the RPL field (bit 0 and 1)   *
 * of stack selector and code selector     *
 * need to be 3 - the CPU will check that  *
 * the RPL is equal to the DPL in the      *
 * segment descriptor and raise a GPF      *
 * if this is not the case                 *
 * Also the CPU will check that the stack  *
 * segment is writable and will raise      *
 * a GPF if this is not the case           *
 * We also load the new data segment       *
 * into ds, es, and fs                     *
 *******************************************/
 goto_ring3:
    push %ebp
    mov %esp, %ebp

    # turn off interrupts - just to be on the save side
    cli
    # Put user space SS and ESP on stack
    mov $SELECTOR_STACK_USER+3, %eax
    push %eax
    mov 12(%ebp), %eax
    push %eax
    # Set IF in eflags and push
    pushf
    pop %eax
    or $0x200, %eax
    push %eax
    # code selector for user space segment
    mov $SELECTOR_CODE_USER+3, %eax
    push %eax
    # and EIP
    mov 8(%ebp), %eax
    push %eax
    # load data segments
    mov $SELECTOR_DATA_USER+3, %eax
    mov %ax, %ds
    mov %ax, %fs
    mov %ax, %es
    # and do the iret
    iret


 /*******************************************
 * Halt the CPU until an interrupt occurs   *
 *******************************************/
halt:
    push %ebp
    mov %esp, %ebp

    hlt

    leave
    ret


/*******************************************
 * Trigger rescheduling using int 0x81     *
 *******************************************/
reschedule:
    push %ebp
    mov %esp, %ebp

    int $0x81

    leave
    ret

/*******************************************
 * Read from an MSR                        *
 * Prototype:                              *
 * rdmsr(u32 msr, u32* low, u32* high)     *
 * @msr - number of msr                    *
 * @low - lower 32 bits are stored there   *
 * @high - higher 32 bits are stored there *
 *******************************************/
rdmsr:
    # set up stack
    push %ebp
    mov %esp, %ebp

    # save ECX, EDX and EBX
    push %ecx
    push %edx
    push %ebx

    # Move MSR number into ECX
    mov 8(%ebp), %ecx
    # and execute rdmsr. Now EDX contains bits 32 - 63, EAX contains
    # bits 0 - 31
    rdmsr
    # store results
    mov %eax, %ebx
    mov 12(%ebp), %eax
    mov %ebx, (%eax)
    mov 16(%ebp), %eax
    mov %edx, (%eax)

    # Restore register
    pop %ebx
    pop %edx
    pop %ecx
    leave
    ret

/*******************************************
 * Execute CPUID                           *
 * prototype:                              *
 * cpuid(u32 eax, u32* ebx, u32* ecx,      *
 *       u32* edx)                         *
 * Return value:                           *
 * EAX after executing cpuid               *
 *******************************************/
 cpuid:
    # set up stack frame and save used regs
    push %ebp
    mov %esp, %ebp
    push %ebx
    push %ecx
    push %edx

    # execute cpuid
    mov 8(%ebp), %eax
    cpuid
    # store results. We
    # first save EAX on the
    # stack and restore it later
    push %eax
    mov 12(%ebp),%eax
    mov %ebx, (%eax)
    mov 16(%ebp),%eax
    mov %ecx, (%eax)
    mov 20(%ebp),%eax
    mov %edx, (%eax)
    pop %eax

    # restore regs and return
    pop %edx
    pop %ecx
    pop %ebx
    leave
    ret


 /******************************************
 * Load TSS                                *
 *******************************************/
load_tss:
    # set up stack frame and save used regs
    push %ebp
    mov %esp, %ebp
    push %eax

    # load TSS
    mov $SELECTOR_TSS, %eax
    ltr %ax

    # restore regs and return
    pop %eax
    leave
    ret
