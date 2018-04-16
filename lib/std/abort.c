/*
 * abort.c
 *
 */

#include "lib/ctype.h"
#include "lib/string.h"
#include "lib/signal.h"
#include "lib/os/streams.h"

/*
 * Abort the calling process
 *
 * The abort() function will cause abnormal process termination to occur, unless the signal SIGABRT is being caught and the
 * signal handler does not return.
 *
 * The abnormal termination processing will include the default actions defined for SIGABRT and an attempt to effect fclose()
 * on all open streams.
 *
 * The SIGABRT signal will be sent to the calling process as if by means of raise() with the argument SIGABRT. The status made
 * available to wait() or waitpid() by abort() will be that of a process terminated by the SIGABRT signal.
 *
 * The abort() function will override blocking or ignoring the SIGABRT signal.
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * none
 */
void abort() {
    sigset_t sigset;
    struct sigaction sa;
    /*
     * First we make sure that SIGABRT is not blocked
     * and not ignored
     */
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGABRT);
    sigprocmask(SIG_UNBLOCK, &sigset, 0);
    sigaction(SIGABRT, 0, &sa);
    if (SIG_IGN==sa.sa_handler) {
        sa.sa_handler = SIG_DFL;
        sigaction(SIGABRT, &sa, 0);
    }
    /*
     * If a user-defined signal handler is
     * set up, raise the signal to make sure that this
     * handler is processed
     */
    if (sa.sa_handler != SIG_DFL) {
        raise(SIGABRT);
    }
    /*
     * When we get to this point, either the user defined signal handler returned
     * or no signal handler will installed. Thus we now start the actual abnormal termination
     */
    __ctOS_stream_flush_all();
    raise(SIGABRT);
    /*
     * Should never get here
     */
    _exit(1);
}
