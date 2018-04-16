/*
 * systemcalls.h
 */

#ifndef _SYSTEMCALLS_H_
#define _SYSTEMCALLS_H_

#include "irq.h"

/*
 * Typedef for a systemcall
 */
typedef int (*st_handler_t)(ir_context_t*, int);



/*
 * Macro to make definition of system call handlers easier
 */
#define SYSENTRY(x) int x##_entry(ir_context_t* ir_context, int previous_execution_level)

void syscall_dispatch(ir_context_t* ir_context, int previous_execution_level);

#endif /* _SYSTEMCALLS_H_ */
