/*
 * kprintf.h
 */

#ifndef _KPRINTF_H_
#define _KPRINTF_H_

#include "ktypes.h"
#include "vga.h"
#include "console.h"

extern int __loglevel;

/*
 * Some macros to produce output on the screen.
 * Use those as far as possible
 */

/*
 * Simply print something to the screen
 */
#define PRINT(...) do {kprintf(__VA_ARGS__); } while (0)

/*
 * Similar to PRINT, but output is enhanced by some
 * debugging information
 */
#define DEBUG(...) do {if (__loglevel > 0 ) { kprintf("DEBUG at %s@%d (%s): ", __FILE__, __LINE__, __FUNCTION__); \
                     kprintf(__VA_ARGS__); }} while (0)

/*
 * Similar to PRINT, but output is enhanced by some
 * debugging information and message is preceeded by ERROR
 */
#define ERROR(...) do {kprintf("ERROR at %s@%d (%s): ", __FILE__, __LINE__, __FUNCTION__); \
                     kprintf(__VA_ARGS__); } while (0)

/*
 * Message preceeded by a functionally defined module name
 */
#define MSG(...) do {kprintf ("[%s] ", __module); kprintf(__VA_ARGS__); } while (0)

/*
 * As above, but after printing the message, CPU is halted
 */
#define PANIC(...) do {kprintf("PANIC at %s@%d (%s): ", __FILE__, __LINE__, __FUNCTION__); \
                     kprintf(__VA_ARGS__); \
                     trap(); } while (0)

/*
 * An assertion
 */
#define KASSERT(x) do { if (!(x)) { \
                                      PANIC("Assertion %s failed\n", #x); \
                                  } \
                      } while (0)

void kprintf(char* template, ...);
void wprintf(win_t* win, char* template, ...);

#endif /* _KPRINTF_H_ */
