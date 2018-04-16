/*
 * init.c
 *
 * This is the program executed by the kernel in user space immediately after setting up
 * the kernel and moving to user space
 */

#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <errno.h>


int main() {
    int pid;
    int status;
    int rc;
    printf("INIT: starting /cli\n");
    pid = fork();
    if (0 == pid) {
        execl("/cli", "myarg", "test", 0);
    }
    if (pid < 0) {
        printf("Error: could not fork!\n");
        _exit(1);
    }
    while (errno!=ECHILD) {
        rc = waitpid(pid, &status, 0);
        if (rc > 0) {
            printf("INIT: Child terminated with status 0%o\n", status);
            printf("INIT: Child exit status: %d\n", WEXITSTATUS(status));
            if (WIFEXITED(status)) {
                printf("INIT: Child termination reason: normal termination\n");
            }
            if (WIFSIGNALED(status)) {
                printf("INIT: Child termination reason: signal %d received\n", WTERMSIG(status));
            }
        }
   }
    printf("INIT: all children completed\n");
    return 0;
}
