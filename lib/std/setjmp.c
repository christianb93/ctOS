/*
 * setjmp.c
 *
 */

#include "lib/setjmp.h"


/*
 * These functions are in setjmp.S
 */
extern void __ctOS_longjmp(jmp_buf, int);


/*
 * The longjmp() function will restore the environment saved by the most recent invocation of setjmp() in the same thread, with
 * the corresponding jmp_buf argument. If there is no such invocation, or if the function containing the invocation of setjmp()
 * has terminated execution in the interim, or if the invocation of setjmp() was within the scope of an identifier with
 * variably modified type and execution has left that scope in the interim, the behavior is undefined.
 *
 * Note that longjmp does not restore the signal mask.
 *
 * BASED ON: POSIX 2004
 *
 *
 */
void longjmp(jmp_buf env, int val) {
    __ctOS_longjmp(env, val);
}
