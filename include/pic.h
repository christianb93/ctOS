/*
 * pic.h
 */

#ifndef _PIC_H_
#define _PIC_H_

#include "ktypes.h"

#define PIC_MASTER_CMD 0x20
#define PIC_MASTER_DATA 0x21
#define PIC_SLAVE_CMD 0xa0
#define PIC_SLAVE_DATA 0xa1


void pic_init(u32 pic_vector_base);
void pic_disable();
void pic_eoi(u32 vector, int pic_vector_base);

#endif /* _PIC_H_ */
