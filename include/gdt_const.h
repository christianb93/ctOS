/*
 * gdt_const.h
 *
 * Common constants needed by startup code in gdt.c and start.S
 */

/*
 * Size of early stack
 */
#define STACK_SIZE      0x1000

/*
 * Magic number pushed on stack to find start of an IRQ frame
 */
 #define DUMMY_ERROR_CODE 0x1234abcd


/*
 * The following constants define the segment selectors used in the GDT. Here is an overview of the segments we
 * use
 *
 * Selector                Segment
 * ------------------------------------
 * 0                       unused
 * 8                       Code segment kernel
 * 16                      Data segment kernel
 * 24                      Stack segment kernel
 * 32                      Code segment user space
 * 40                      Data segment user space
 * 48                      Stack segment user space
 * 56                      Code segment with default operand size 16 bit
 * 64                      TSS for first CPU (CPU #0, i.e. BSP)
 * 72                      TSS for second CPU
 *  .
 *  .
 * 56 + SMP_MAX_CPU*8      TSS for last CPU
 * 56 + SMP_MAX_CPU*8 + 8  CPU-specific segment for first CPU
 * .
 * .
 * 56 + 16*SMP_MAX_CPU     CPU specific segment for last CPU
 *
 * Note that there is one data segment per CPU, starting with selector 48 + SMP_MAX_CPU. This data segment
 * is loaded into the FS register of each CPU at startup time.
 *
 * Including the "zero" segment, we have therefore 7 + 2*#CPUs segments
 */
#define SELECTOR_CODE_KERNEL 8
#define SELECTOR_DATA_KERNEL 16
#define SELECTOR_STACK_KERNEL 24
#define SELECTOR_CODE_USER 32
#define SELECTOR_DATA_USER 40
#define SELECTOR_STACK_USER 48
#define SELECTOR_CODE_16 56
#define SELECTOR_TSS 64



#ifndef _GDT_CONST_H_
#define _GDT_CONST_H_


#endif /* _GDT_CONST_H_ */
