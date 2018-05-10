#include "gdt_const.h"
#include "smp_const.h"

/*
 * Magic cookie according to multiboot specification
 */
#define MULTIBOOT_HEADER_MAGIC 	0x1BADB002

	.section .text
	.global _start
	.global stack

_start:
	cli
	/* Skip over multiboot header */
	jmp go
	/****************************************
	* This is the multiboot header which    *
	* the bootloader (GRUB) will search for *
	****************************************/
	.align 4
    .long MULTIBOOT_HEADER_MAGIC
    # Set bit 0 (aligment), bit 1 (memory map) and bit 2 (graphics)
    .long 0x7
    .long -(MULTIBOOT_HEADER_MAGIC + 0x7)
    # Header address - not used
    .long 0x0
    # Load address - not used
    .long 0x0
    # Load end address - not used
    .long 0x0
    # BSS end address - not used
    .long 0x0
    # Address of entry point - not used
    .long 0x0
    # VBE mode type requested, 0 for graphics, 1 for text
    # we ask GRUB to start us in text mode and switch ourselves
    .long 1
    # Requested width
    .long 80
    # Requested height
    .long 25
    # VBE requested depth
    .long 0
go:
	/*******************************************
	* According to the multiboot specification *
	* we cannot assume %ESP to be valid, so    *
	* we set up our own stack                  *
	*******************************************/
	mov $(stack + STACK_SIZE-4), %esp
	/*******************************************
	* Save pointer to multiboot information    *
	* structure                                *
	*******************************************/
	push %ebx
	
    /*******************************************
	* Save magic value of EAX                  *
	*******************************************/
	push %eax

    /*******************************************
	* Call C function to setup GDT             *
	* this will return the address of          *
	* the GDT ptr in eax                       *
	********************************************/
	call gdt_get_table
	/*******************************************
    * Load 48 bit GDT register                 *
    ********************************************/
	lgdt %ds:(%eax)
	/*******************************************
	* We need to do a long jump                *
	* so that our new CS becomes effective     *
	*******************************************/
	jmpl $8, $next
next:
    /*******************************************
	* Set up remaining segment registers       *
	*******************************************/
	movw $16, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs
	movw %ax, %ds
	movw $24, %ax
	movw %ax, %ss
	/******************************************
	* Load GS with CPU id of BSP              *
	******************************************/
	movw $SMP_CPUID_TO_GS(0), %ax
	mov %ax, %gs


    /*******************************************
    * Call C function to setup IDT             *
    * this will return the address of          *
    * the IDT ptr in eax                       *
    *******************************************/
    call idt_create_table
    lidt (%eax)

    /*******************************************
    * Init FPU                                 *
    *******************************************/
    finit

    /*******************************************
    * Set TS bit (bit 3) and MP bit (bit 1)    *
    *******************************************/
    mov %cr0, %eax
    or $0xa, %eax
    mov %eax, %cr0


	/*******************************************
	* Now call run                             *
	*******************************************/
	call run

	/*******************************************
	* When main returns, bring the system to   *
	* a halt state                             *
	*******************************************/
	cli
	hlt

	/*******************************************
	* Reserve space for our stack              *
	*******************************************/
	.comm stack, STACK_SIZE



