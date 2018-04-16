/*
 * rm.S
 *
 * This module contains assembly code to temporarily return to real mode, execute a BIOS function and return to protected mode.
 * To use it, first relocate it to the address 0x7C00, then place the function number describing which function needs to be
 * executed at the linear address 0x10000 and call 0x7C00. Each call will place a return code at 0x10000
 *
 * Supported functions:
 *
 * Value at 0x10000          Performed function
 * -------------------------------------------------------------------
 * 1                         VBE - get controller information
 * 2                         VBE - get video mode
 * 3                         VBE - select video mode
 * 4          ´              VGA BIOS - get font
 *
 *
 * Memory layout:
 *
 * - the code and data is located at 0x7C00
 * - while being in real mode, the stack will be set up at 0x9000
 * - the area starting at 0x10000 is used to communicate with the protected mode code, i.e. the protected mode code needs to
 *   place parameters there and the results of the BIOS calls will be stored there as well
 *
 * Required CPU state when being called:
 *
 * - CPU is in protected mode with valid GDT and IDT
 * - interrupts are disabled
 * - PIC setup has not been done yet, i.e. the PIC is in the state as setup by the BIOS
 * - paging is still disabled
 * - descriptors are flat
 * - the TSS has not yet been loaded (I have experienced issues with the TSS being marked as busy when
     loading the TSS again after having done a switch from PM to RM and back at least once before)
 * - the code segment SELECTOR_CODE_16 is a segment with default operation size bit 0!
 *
 * At the moment, this mechanism is only called at early boot time to set up the VBE video modes. Keep in mind that depending
 * on the boot mechanism used, usage of this module might overwrite other information in memory (for instance, GRUB2 seems to use
 * the area starting at 0x10000 as well to store the multiboot information structure, so make sure to save it before running this code)
 */

 #include "gdt_const.h"

    .section .text
    /*
    * Symbols to mark start and end of the code, this can
    * be used by the protected mode code for relocation
    */
    .global _rm_switch_start
    .global _rm_switch_end
_rm_switch_start:
    .code32
    /************************************************************************************
    * Save registers                                                                    *
    ************************************************************************************/
    push %ebp
    mov %esp, %ebp
    push %ebx
    push %ecx
    push %edx
    push %esi
    push %edi
    pushf
    xor %eax, %eax
    mov %gs, %ax
    push %eax

    /************************************************************************************
    * Save IDT and GTD                                                                  *
    ************************************************************************************/
    sidt (_saved_idtr - _rm_switch_start)
    sgdt (_saved_gdtr - _rm_switch_start)

    /************************************************************************************
    * Switch to a code segment with default operation size 0. Note that this is needed, *
    * as otherwise the code generated for 16 bit code segments using .code16            *
    * will fail because the operand size prefix - or its absence - are misinterpreted   *
    * Also note that due to the relocation, we need to correct the offset of jmp        *
    * to match the actual location of the code in memory at 0x7C00                      *
    ************************************************************************************/
    jmpl $SELECTOR_CODE_16, $(next - _rm_switch_start + 0x7c00)
next:
    .code16

    /************************************************************************************
    * Turn off protected mode bit                                                       *
    ************************************************************************************/
    mov %cr0, %eax
    and $0xFFFFFFFE, %eax
    mov %eax, %cr0

    /************************************************************************************
    * Do a far jump to reload CS with real mode value                                   *
    ************************************************************************************/
    jmpl $0, $(rm - _rm_switch_start + 0x7c00)
rm:

    /************************************************************************************
    * Reload IDT register with real mode interrupt descriptor table                     *
    ************************************************************************************/
    mov $(_idtr - _rm_switch_start + 0x7c00), %eax
    lidt (%eax)

    /************************************************************************************
    * Setup a new stack and data segment. We first save the current value of ESP        *
    * which we then push on the new stack to be able to restore it later                *
    ************************************************************************************/
    mov %esp, %ebx
    mov $0x9000, %eax
    mov %eax, %esp
    xor %eax, %eax
    mov %ax, %ss
    mov $0x1000, %ax
    mov %ax, %ds
    mov %ax, %es
    push %ebx

    /************************************************************************************
    * Now we should be ready to call real mode interrupts. We now get the desired       *
    * function from 0x10000 where the protected mode code is supposed to store it for   *
    * us and switch to the respective subroutine
    ************************************************************************************/
    movw 0x0, %ax
    cmp $1, %ax
    jz call_function1
    cmp $2, %ax
    jz call_function2
    cmp $3, %ax
    jz call_function3
    cmp $4, %ax
    jz call_function4
    jmp done
call_function1:
    call vbe_get_info
    jmp done
call_function2:
    call vbe_get_mode
    jmp done
call_function3:
    call vbe_set_mode
    jmp done
call_function4:
    call vga_get_font
    jmp done
done:
    /************************************************************************************
    * Now we start the return to protected mode. First we restore our flat memory model,*
    * restore IDT and GDT and get the original stack pointer back from the real mode    *
    * stack into edx                                                                    *
    ************************************************************************************/
    xor %eax, %eax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    lidt (_saved_idtr - _rm_switch_start)
    lgdt (_saved_gdtr - _rm_switch_start)
    popl %edx

    /************************************************************************************
    * Next we go back to protected mode, note that this will take us back above the     *
    * 1 MB limit to the original location of the code                                   *
    ************************************************************************************/
    mov %cr0, %eax
    or $0x1, %eax
    mov %eax, %cr0
    jmpl $SELECTOR_CODE_KERNEL, $pm;
pm:
    .code32
    /************************************************************************************
    * Get our stack back and restore original values of segment registers               *
    ************************************************************************************/
    mov $SELECTOR_STACK_KERNEL, %eax
    mov %ax, %ss
    movl %edx, %esp
    mov $SELECTOR_DATA_KERNEL, %eax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs

    /************************************************************************************
    * Restore remaining registers from stack                                            *
    ************************************************************************************/
    pop %eax
    mov %ax, %gs
    popf
    pop %edi
    pop %esi
    pop %edx
    pop %ecx
    pop %ebx
    /************************************************************************************
    * and return                                                                        *
    ************************************************************************************/
    leave
    ret

    /************************************************************************************
    * These are the functions which can be executed                                     *
    ************************************************************************************/
    .code16
vbe_get_info:
    /************************************************************************************
    * Get VBE information block and copy it to address 0x10004                          *
    * Input: 0x1 at 0x10000                                                             *
    * Output:                                                                           *
    * on success, 0 is written to 0x10000, otherwise 1                                  *
    * the VBE information block (512 bytes) is stored at the address 0x10000            *
    ************************************************************************************/
    /************************************************************************************
    * Set up data segment and ES:DI and write 0x0 to 0x10000                            *
    ************************************************************************************/
    mov $0x1000, %ax
    mov %ax, %es
    mov %ax, %ds
    mov $0x4, %di
    mov $0x0, %ax
    mov %ax, 0x0
    /************************************************************************************
    * Preset VBE2 in signature field                                                    *
    ************************************************************************************/
    mov $'V', %al
    mov $'B', %ah
    mov %ax, 0x4
    mov $'E', %al
    mov $'2', %ah
    mov %ax, 0x6
    /************************************************************************************
    * Process interrupt and check return value (0x4F = success)                         *
    * INT 0x10                                                                          *
    * Input:                                                                            *
    * AX = 0x4F00                                                                       *
    * ES:DI = memory area where the info block will be stored                           *
    ************************************************************************************/
    mov $0x4f00, %ax
    int $0x10
    cmp $0x4f, %ax
    jz 0f
    mov $0x1, %ax
    mov %ax, 0x0
0:
    ret

vbe_get_mode:
    /************************************************************************************
    * Get VBE mode information and copy it to address  0x10004                          *
    * Input: 0x2 at 0x10000                                                             *
    *        mode number at 0x10002
    * Output:                                                                           *
    * on success, 0 is written to 0x10000, otherwise 1                                  *
    * the VBE mode info block is stored at address 0x10004                              *
    ************************************************************************************/
    /************************************************************************************
    * Set up data segment and ES:DI and write 0x0 to 0x10000                            *
    ************************************************************************************/
    mov $0x1000, %ax
    mov %ax, %es
    mov %ax, %ds
    mov $0x4, %di
    mov $0x0, %ax
    mov %ax, 0x0
    /************************************************************************************
    * Get requested mode number from address 0x10002                                    *
    ************************************************************************************/
    mov 0x2, %cx
    /************************************************************************************
    * Preset VBE2 in signature field                                                    *
    ************************************************************************************/
    mov $'V', %al
    mov $'B', %ah
    mov %ax, 0x4
    mov $'E', %al
    mov $'2', %ah
    mov %ax, 0x6
    /************************************************************************************
    * Process interrupt and check return value (0x4F = success)                         *
    * INT 0x10                                                                          *
    * Input:                                                                            *
    * AX = 0x4F01                                                                       *
    * CX = mode number (taken from 0x10002)                                             *
    * ES:DI = memory area where the mode info block will be stored                      *
    ************************************************************************************/
    mov $0x4f01, %ax
    int $0x10
    cmp $0x4f, %ax
    jz 0f
    mov $0x1, %ax
    mov %ax, 0x0
0:
    ret

vbe_set_mode:
    /************************************************************************************
    * Set VBE mode                                                                      *
    * Input: 0x3 at 0x10000                                                             *
    *        mode number at 0x10002
    * Output:                                                                           *
    * on success, 0 is written to 0x10000, otherwise 1                                  *
    ************************************************************************************/
    /************************************************************************************
    * Set up data segment and write 0x0 to 0x10000                                      *
    ************************************************************************************/
    mov $0x1000, %ax
    mov %ax, %ds
    mov $0x0, %ax
    mov %ax, 0x0
    /************************************************************************************
    * Get requested mode number from address 0x10002                                    *
    ************************************************************************************/
    mov 0x2, %bx
    /************************************************************************************
    * Preset VBE2 in signature field                                                    *
    ************************************************************************************/
    mov $'V', %al
    mov $'B', %ah
    mov %ax, 0x4
    mov $'E', %al
    mov $'2', %ah
    mov %ax, 0x6
    /************************************************************************************
    * Process interrupt and check return value (0x4F = success)                         *
    * INT 0x10                                                                          *
    * Input:                                                                            *
    * AX = 0x4F02                                                                       *
    * BX = mode number                                                                  *
    ************************************************************************************/
    mov $0x4f02, %ax
    int $0x10
    cmp $0x4f, %ax
    jz 0f
    mov $0x1, %ax
    mov %ax, 0x0
0:
    ret

vga_get_font:
    /************************************************************************************
    * Get VGA font data                                                                 *
    * Input: 0x4 at 0x10000                                                             *
    * Output:                                                                           *
    * 8x16 VGA BIOS font, stored at address 0x10006                                     *
    * Segment of font table, stored at 0x10002                                          *
    * Offset of font table, stored at 0x10004                                           *
    ************************************************************************************/
    /************************************************************************************
    * Save registers                                                                    *
    ************************************************************************************/
    push %bp
    push %es
    /************************************************************************************
    * Process interrupt and check return value (0x4F = success)                         *
    * INT 0x10  / function 0x1130 - get current character generator info                *
    * Input:                                                                            *
    * AX = 0x1130                                                                       *
    * BH = 0x6 - get address of 8x16 font                                               *
    * Output:                                                                           *
    * ES:BP - address of requested table                                                *
    ************************************************************************************/
    mov $0x1130, %ax
    movb $0x6, %bh
    int $0x10
    /************************************************************************************
    * Copy segment and offset of font table to 0x10002 and 0x10004                      *
    ************************************************************************************/
    mov $0x1000, %ax
    mov %ax, %ds
    mov %es, %ax
    mov %ax, 0x2
    mov %bp, %ax
    mov %ax, 0x4
    /************************************************************************************
    * Load font address into DS:SI and 0x10006 into ES:DI                               *
    ************************************************************************************/
    push %es
    pop %ds
    mov %bp, %si
    mov $0x1000, %ax
    mov %ax, %es
    mov $0x6, %di
    /************************************************************************************
    * and copy font data for 256 characters, i.e. 16*256 dwords                         *
    ************************************************************************************/
    mov $4096, %cx
    rep movsb
    /************************************************************************************
    * Restore registers and return                                                      *
    ************************************************************************************/
    pop %es
    pop %bp
    ret

    /************************************************************************************
    * Temporary IDT for real mode                                                       *
    * Limit = 256, base = 0000:0000                                                     *
    ************************************************************************************/
_idtr:
    .word 256
    .long 0x0
    /************************************************************************************
    * Here we store the contents of the IDT and GDT register, i.e. the 48 bit pointers  *
    ************************************************************************************/
_saved_idtr:
    .long 0x0
    .long 0x0
_saved_gdtr:
    .long 0x0
    .long 0x0
_rm_switch_end:



