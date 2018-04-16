/*
 * smp_const.h
 *
 * This header file contains constants which are required by the code
 * in trampoline.S as well as by the code in smp.c
 */

#ifndef _SMP_CONST_H_
#define _SMP_CONST_H_

/*
 * The following absolute addresses are
 * used to communicate with an AP while it
 * is executing the trampoline code
 * - a status field
 * - location of the GDT pointer
 * - status field used by the AP to signal that it has reached the protected mode
 * - address of CR3 which the AP is supposed to use
 * - address of CPU id which the AP is supposed to use
 */
#define AP_DS 0x1000
#define AP_RM_STATUS_ADDR 0x0
#define AP_GDTR_LOC 0x4
#define AP_PM_STATUS_ADDR 0x10
#define AP_CR3_ADDR 0x14
#define AP_CPUID_ADDR 0x18

/*
 * Number of CPUs which we support
 */
#define SMP_MAX_CPU 8

/*
 * We use the GS register to store the CPU id. As the value in this register
 * needs to be a valid segment, we use the following conversion rules
 */
#define SMP_CPUID_TO_GS(cpuid) (SELECTOR_TSS + SMP_MAX_CPU*8 + 8*cpuid)
#define SMP_GS_TO_CPUID(gs) ((gs - SELECTOR_TSS - SMP_MAX_CPU*8) / 8)

#endif /* _SMP_CONST_H_ */
