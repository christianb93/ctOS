/*
 * smp.h
 *
 *  Created on: Apr 1, 2012
 *      Author: chr
 */

#ifndef _SMP_H_
#define _SMP_H_

#include "smp_const.h"

/*
 * This is where the trampoline code will
 * be placed. This needs to be below 1 MB as the AP
 * will start up in real mode
 */
#define TRAMPOLINE 0x80000


/*
 * BIOS definitions for warm reset
 */
#define WARM_RESET_VECTOR 0x467
#define RESET_ACTION_JMP 0xa

/*
 * Memory barriers for x86
 *
 * Note that we do NOT provide compiler memory barriers here yet!!!
 */

/*
 * A general memory barrier for x86. We could use mfence here but do not
 * want to assume that our machine has SSE2
 */
#define smp_mb()   do {asm("lock addl $0, 0(%esp)"); } while (0)

/*
 * For x86, smp_rmb() and smp_wmb() are noops at the moment as, with a few exceptions listed below,
 * the x86 memory model does not allow read-read re-ordering or store-store reordering. BUT there are exceptions
 *
 * 1) some older Intel-clones like IDT Winchip do seem to have out-of-order reads, thus these CPUs are currently
 * not supported by ctOS (as I do not have access to a box running one of this CPUs I would not be able to test it
 * anyway)
 * 2) the same applies to Pentium Pros which seem to have a few bugs wrt to memory ordering
 * 3) x86 has a weak memory model for write-combined memory - however we assume that only stuff like VGA framebuffers
 * are set up as wc and all the memory with which we actually deal is set up as wb
 */
#define smp_rmb()
#define smp_wmb()



/*
 * Number of BSP. Note that this is NOT the local APIC id which is not guaranteed
 * to be zero for the BSP, but the ctOS internal numbering where by definition, the
 * BSP has ID zero
 */
#define SMP_BSP_ID 0

void smp_start_aps();
int smp_get_cpu();
void smp_start_main(int cpuid);
void smp_wait_idle(int cpuid);
int smp_enabled();
int smp_get_cpu();
int smp_get_cpu_count();

#endif /* _SMP_H_ */
