/*
 * exit.c
 *
 */

extern void _fini();

#include "lib/os/oscalls.h"
#include "lib/os/streams.h"
#include "lib/limits.h"

/*
 * A list of exit handlers 
 */
typedef void (*exit_handler_t)(void);
static exit_handler_t exit_handler[ATEXIT_MAX];
static int last_exit_handler = 0;

/*
 * Register the function pointed to be handler to be called at program
 * termination. The functions are called in reverse order of their registration
 * 
 * This function returns 0 upon successful completion 
 */
int atexit(void (*handler)(void)) {
    if (last_exit_handler >= ATEXIT_MAX) {
        return 1;
    }
    exit_handler[last_exit_handler] = handler;
    last_exit_handler++;
    return 0;
}

/*
 * Invoke all previously registered exit handler
 */
void __exit_run_handlers() {
    while (last_exit_handler > 0) {
        last_exit_handler--;
        exit_handler[last_exit_handler]();
    }
    
}

/*
 * Clean up all handlers
 */
void __exit_init_handlers() {
    last_exit_handler = 0;
}

/*
 * Exit a process
 *
 * The value of status may be 0, EXIT_SUCCESS, EXIT_FAILURE,  or any other value,
 * though only the least significant 8 bits (that is, status & 0377) shall be available to a waiting parent process.
 *
 * Exit shall terminate the calling processes, with the following consequences:
 *
 * All of the file descriptors open in the calling process shall be closed.
 * If the parent process of the calling process is executing a wait() or waitpid() it shall be notified of the calling process' termination
 * and the low-order eight bits (that is, bits 0377) of status shall be made available to it. If the parent is not waiting, the child's
 * status shall be made available to it when the parent subsequently executes wait() or waitpid().
 *
 * If the parent process of the calling process is not executing a wait() or waitpid(), the calling process shall be transformed into a zombie process.
 * A zombie process is an inactive process and it shall be deleted at some later time when its parent process executes wait() or waitpid().
 *
 * A SIGCHLD shall be sent to the parent process.
 *
 * The parent process ID of all of the calling process' existing child processes and zombie processes shall be set to the process ID of the
 * INIT process, i.e. to PID 1
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * 1) No SIGHUP is sent if the process is a controlling process, and the terminal is not disassociated from the session
 * 2) No SIGHUP is sent if the exit causes a process group to become orphaned
 */
void _exit(int status) {
    /*
     * We always call _fini here, but this is either provided as a dummy by
     * crt1.o or by the actual GCC crt stuff when we build using the 
     * toolchain
     */
    _fini();
    __ctOS__exit(status);
}

/*
 * Exit a process
 *
 * The value of status may be 0, EXIT_SUCCESS, EXIT_FAILURE,  or any other value,
 * though only the least significant 8 bits (that is, status & 0377) shall be available to a waiting parent process.
 *
 * Exit shall terminate the calling processes, with the following consequences:
 *
 * All of the file descriptors open in the calling process shall be closed.
 * If the parent process of the calling process is executing a wait() or waitpid() it shall be notified of the calling process' termination
 * and the low-order eight bits (that is, bits 0377) of status shall be made available to it. If the parent is not waiting, the child's
 * status shall be made available to it when the parent subsequently executes wait() or waitpid().
 *
 * If the parent process of the calling process is not executing a wait() or waitpid(), the calling process shall be transformed into a zombie process.
 * A zombie process is an inactive process and it shall be deleted at some later time when its parent process executes wait() or waitpid().
 *
 * A SIGCHLD shall be sent to the parent process.
 *
 * The parent process ID of all of the calling process' existing child processes and zombie processes shall be set to the process ID of the
 * INIT process, i.e. to PID 1
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * 2) No SIGHUP is sent if the process is a controlling process, and the terminal is not disassociated from the session
 * 3) No SIGHUP is sent if the exit causes a process group to become orphaned
 */
void exit(int status) {
    /*
     * Call all exit handlers. We do this now before any other cleanup 
     * is performed
     */
    __exit_run_handlers();
    /*
     * Flush all open streams. As _exit will close the file
     * descriptors as well and release all memory, this amounts to
     * closing all open streams and directory streams
     */
    __ctOS_stream_flush_all();
    /*
     * Call _exit to complete exit processing
     */
    _exit(status);
}
