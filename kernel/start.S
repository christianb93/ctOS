#include "gdt_const.h"
#include "smp_const.h"

/*
 * Magic cookie according to multiboot specification
 * Note that this is the magic cookie for multiboot2!
 */
#define MULTIBOOT2_HEADER_MAGIC  0xe85250d6

	.section .text
	.global _start
	.global stack

_start:
	cli
	/* Skip over multiboot header */
	jmp go
	/****************************************
	* This is the multiboot2 header which   *
	* the bootloader (GRUB) will search for *
	****************************************/
	.align 8
multiboot_header:    
    # The first 4 bytes are the magic number
    .long MULTIBOOT2_HEADER_MAGIC
    # Next field is the architecture field. We ask
    # to be loaded in 32bit protected mode, i.e. 0
    .long 0
    # Next field is the length of the header
    .long go - multiboot_header
    # Next field is the checksum. This should be such that
    # all magic fields (i.e. the first four fields including
    # the checksum itself) sum up to zero
    .long -(MULTIBOOT2_HEADER_MAGIC + (go  - multiboot_header))
    #
    # The second part of the multiboot header (the tags)
    # starts here. It seems that GRUB2 expects ALL tags to be aligned
    # on an 8-byte boundary
    #
    #
    # Module alignment tag. Type = 6, size = 8
    #
    .long 6
    .long 8
    #
    # Multiboot2 information request tag
    # Type = 1
    # We ask for the tag types command line (1),
    # modules (3) and memory map (6)
    # It seems that this needs to end with type 0
    #
mbi_tag_start:
    .long 0x1
    .long mbi_tag_end - mbi_tag_start
    .long 1
    .long 3
    .long 6
    .long 0
mbi_tag_end:
    #
    # Terminating tag - size 8, type 0
    #
    .align 8
    .long 0
    .long 8
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



