/*
 * setjmp.h
 *
 *  Created on: Jan 20, 2012
 *      Author: chr
 */

#ifndef _SETJMP_H_
#define _SETJMP_H_

/*
 * This buffer is used to store the register state when a setjmp is done. Its contents are
 * as follows (from lower addresses to higher addresses)
 * EBX
 * ECX
 * EDX
 * ESI
 * EDI
 * EBP
 * ESP
 * RET
 * EFLAGS
 * up to 16 bytes to make sure that following FPU state is 16-byte aligned
 * FPU state (512 bytes)
 * Thus the total size of the structure is 9*sizeof(u32) + 512 + 16 = 564 bytes = 141 dwords
 */
typedef unsigned int jmp_buf[141];

void longjmp(jmp_buf, int);
int __ctOS_setjmp(jmp_buf);
#define setjmp(x) __ctOS_setjmp(x)

#endif /* _SETJMP_H_ */
