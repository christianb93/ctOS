/*
 * testall.c
 *
 * Run all automated tests
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>

static char* tests[] = {"testwait", "testfiles", "testjc", "testsignals", "testpipes", "testfork", "testmisc"};

#define NR_OF_TESTS  (sizeof(tests) / sizeof(char*))


int main(int argc, char** argv) {
    int status;
    int i;
    int count;
    pid_t pid;
    int iterations = 1;
    int start_time = 0;
    if (argc==2) {
        iterations = strtol(argv[1], 0, 10);
    }
    /*
     * Get current time
     */
    start_time = time(0);
    for (count=0;count<iterations;count++) {
        printf("------------------------------------------\n");
        printf("Starting pass %d\n", count+1);
        printf("------------------------------------------\n");
        for (i=0;i<NR_OF_TESTS;i++) {
             pid = fork();
            if (pid < 0) {
                printf("Could not fork process, giving up\n");
                _exit(1);
            }
            if (0==pid) {
                /*
                 * Child. Make sure that child is running in its own process group
                 * as some of the tests expect this
                 */
                setpgid(0, 0);
                tcsetpgrp(STDIN_FILENO, getpid());
                /*
                 * Run program
                 */
                execl(tests[i], 0);
                printf("Dooh, something went wrong\n");
                _exit(1);
            }
            if (pid>0) {
                /*
                 * Parent. Also put child into foreground, then wait for it to complete
                 */
                tcsetpgrp(STDIN_FILENO, pid);
                if(pid!=waitpid(pid, &status, 0)) {
                    printf("waitpid failed with errno %d\n", errno);
                    _exit(1);
                }
                if (!WIFEXITED(status)) {
                    printf("Strange, child apparently was killed due to a signal\n");
                    _exit(1);
                }
                if (WEXITSTATUS(status)) {
                    printf("Test %s failed, stopping\n", tests[i]);
                    _exit(1);
                }
            }
        }
    }
    printf("-----------------------------------------\n");
    printf("All tests completed successfully\n");
    printf("This test took %d seconds (%d minutes)\n", time(0)-start_time, (time(0)-start_time) / 60);
    printf("-----------------------------------------\n");
    return 0;
}
