/*
 * trampoline.S
 *
 */

#include "smp_const.h"
#include "gdt_const.h"



    .section .text
    .global _trampoline_start
    .global _trampoline_end
    .code16gcc
_trampoline_start:
    /************************************************************
    * set data segment to 0x10000 - this is where we            *
    * exchange data with the SMP startup control code in smp.c  *
    ************************************************************/
    movw $AP_DS, %bx
    movw %bx, %ds;
    /************************************************************
    * increment status flag at address 0x10000                  *
    ************************************************************/
    lock incb 0
    /************************************************************
    * The caller needs to set up a GDT 48 bit pseudo-descriptor *
    * at physical address 0x10004                               *
    ************************************************************/
    mov $AP_GDTR_LOC, %eax
    lgdt %ds:(%eax)

    /************************************************************
    * Init FPU (before setting TS bit...)                       *
    ************************************************************/
    finit

    /************************************************************
    * Turn on protected mode bit, MP bit and TS bit             *
    ************************************************************/
    mov %cr0, %eax
    or $0xb, %eax
    mov %eax, %cr0

    /************************************************************
    * Clear prefetch queue by doing a far jump                  *
    * Recall that the trampoline code is relocated! This will   *
    * take us to the copy of the code at the original location! *
    ************************************************************/
    jmpl $SELECTOR_CODE_KERNEL, $next
    .code32
next:
    /************************************************************
    * Set up segment registers and stack                        *
    ************************************************************/
    movw $SELECTOR_DATA_KERNEL, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %ds
    movw $SELECTOR_STACK_KERNEL, %ax
    movw %ax, %ss
    mov $(_trampoline_stack + STACK_SIZE - 4), %esp
    /************************************************************
    * incremented protected mode AP counter                     *
    ************************************************************/
    lock incw (AP_DS*0x10 + AP_PM_STATUS_ADDR)
    /************************************************************
    * Get value of CR3 to use from address x010014              *
    ************************************************************/
    mov (AP_DS*0x10 + AP_CR3_ADDR), %eax
    mov %eax, %cr3
    /************************************************************
    * Turn on paging and WP bit                                 *
    ************************************************************/
    mov %cr0, %eax
    or $0x80000000, %eax
    or $0x10000, %eax
    mov %eax, %cr0
   /*************************************************************
    * Call C function to get the address of the IDT ptr in eax  *
    ************************************************************/
    call idt_create_table
    lidt (%eax)
    /************************************************************
    * Load TSS register - the selector is calculated by getting *
    * the logical cpuid from address 0x10018 where smp.c stores *
    * it, multiplying this by 8 and adding it to the TSS        *
    * selector of the BSP                                       *
    ************************************************************/
    mov (AP_DS*0x10 + AP_CPUID_ADDR), %eax
    shl $3, %eax
    add $SELECTOR_TSS, %eax
    ltr %ax

    /************************************************************
    * Call smp_ap_main()                                        *
    ************************************************************/
    call smp_ap_main
    cli
    hlt
_trampoline_end:
    nop
    .comm _trampoline_stack, STACK_SIZE

