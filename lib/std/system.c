#include "lib/sys/stat.h"
#include "lib/sys/wait.h"
#include "lib/signal.h"
#include "lib/unistd.h"
#include "lib/errno.h"


/*
 * Execute a command
 * 
 * If this is called with command == NULL, it will determine whether a shell exists (/bin/sh or /bin/dash) and return 1 if this is the case
 * 
 * If command is not NULL and a shell could be found, the shell will be invoked, passing command as argument. If command is not NULL and no 
 * shell could be found, 0 is returned
 * 
 */
int system(const char* command) {
    char* path = 0;
    char* prog = 0;
    int pid = 0;
    int result = 0;
    struct stat mystat;
    struct sigaction sa, savintr, savequit;
    sigset_t saveblock;    
    /*
     * First see whether we have a shell
     */
    if (0 == stat("/bin/sh", &mystat)) {
        path = "/bin/sh";
        prog = "sh";
    }
    else if (0 == stat("/bin/dash", &mystat)) {
        path = "/bin/dash";
        prog = "dash";
    }
    if (0 == path) {
        return 0;
    }
    if (0 == command) {
        return 1;
    }
    /*
     * Now we know that we have a shell. From now on we basically follow the reference implementation
     * that the POSIX standard describes. First we set up a sig action structure that ignores a signal
     */
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    /*
     * Next we install this handler for SIGINT
     * and SIGQUIT, so that the parent will ignore
     * these signals from now on. We also store
     * the old handler structures
     */
    sigemptyset(&savintr.sa_mask);
    sigemptyset(&savequit.sa_mask);
    sigaction(SIGINT, &sa, &savintr);
    sigaction(SIGQUIT, &sa, &savequit);
    /*
     * Finally we block SIGCHILD
     */
    sigaddset(&sa.sa_mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &sa.sa_mask, &saveblock);
    /*
     * Now we do the actual fork
     */
    pid = fork();
    if (0 == pid) {
        /*
         * This is the child process. We first restore the old signal handling
         * and then invoke execl to load the shell
         */
        sigaction(SIGINT, &savintr, (struct sigaction *)0);
        sigaction(SIGQUIT, &savequit, (struct sigaction *)0);
        sigprocmask(SIG_SETMASK, &saveblock, (sigset_t *)0);
        execl(path, prog, "-c", command, (char *)0);
        /*
         * We should never get here
         */
        _exit(1);
     }
     if (-1 == pid) {
         /*
          * Something went wrong during exit
          */
        result = -1;
     }
     else {
         /*
          * Valid pid. Wait for child, but retry
          * if the call is interrupted with EINTR
          */
        while (waitpid(pid, &result, 0) == -1) {
            if (errno != EINTR){
                result = -1;
                break;
            }
        }
     }  
     /*
      * Restore old signal processing
      */
    sigaction(SIGINT, &savintr, (struct sigaction *)0);
    sigaction(SIGQUIT, &savequit, (struct sigaction *)0);
    sigprocmask(SIG_SETMASK, &saveblock, (sigset_t *)0); 
    return result;
}