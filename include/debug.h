/*
 * debug.h
 */

#ifndef _DEBUG_H_
#define _DEBUG_H_

#include "irq.h"
#include "kprintf.h"

void debug_main(ir_context_t* ir_context);
void debug_add_cpu(int cpuid);
void trap();

void debug_lock_wait(u32 lock, int type, int rw, char* file, int line);
void debug_lock_acquired(u32 lock, int rw);
void debug_lock_released(u32 lock, int rw);
void debug_lock_cancel(u32 lock_addr, int rw);
int debug_running();

#endif /* _DEBUG_H_ */
