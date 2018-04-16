/*
 * rm.h
 *
 * This header file contains some macros used to work in real mode
 */

#ifndef _RM_H_
#define _RM_H_

#include "ktypes.h"

/*
 * A real mode far pointer consists of two 16-bit words. The first word is
 * the offset, the second the segment
 */
typedef u32 far_ptr_t;

/*
 * Extract segment and offset from a far pointer
 */
#define FAR_PTR_SEGMENT(x) ((x) >> 16)
#define FAR_PTR_OFFSET(x) ((x) & 0xFFFF)

/*
 * Convert a far pointer to a linear address. We need to move the segment by
 * 4 bits to the left and
 */
#define FAR_PTR_TO_ADDR(ptr) ((((ptr) >> 12) & ~0xF) + ((ptr) & 0xFFFF))


/*
 * Functions currently supported by our real mode - protected mode interface
 */
#define BIOS_VBE_GET_INFO 1
#define BIOS_VBE_GET_MODE 2
#define BIOS_VBE_SELECT_MODE 3
#define BIOS_VGA_GET_FONT 4

#endif /* _RM_H_ */
