/*
 * waitsamples.c
 *
 * Test behaviour of waitpid system call
 */

#include <stdio.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

/*
 * we fork off a new process which exits immediately and wait for it. Exit status is zero
 */
void testcase1() {
    int status;
    int pid;
    int rc;

    pid = fork();
    if (0==pid) {
        _exit(0);
    }
    if (pid<0) {
        printf("fork failed!\n");
        _exit(1);
    }
    if (pid>0) {
        rc= waitpid(pid, &status, 0);
        if (rc!=pid) {
            printf("waitpid failed with rc %d, errno is %d\n", rc, errno);
        }
        else {
            if (WIFEXITED(status) && (WEXITSTATUS(status)==0)) {
                printf("Testcase 1 ok\n");
            }
            else {
                printf("Testcase 1 failed\n");
            }
        }
    }
}

/*
 * we fork off a new process which exits immediately and wait for it. Exit status is different from zero
 */
void testcase2() {
    int status;
    int pid;
    int rc;

    pid = fork();
    if (0==pid) {
        _exit(1);
    }
    if (pid<0) {
        printf("fork failed!\n");
        _exit(1);
    }
    if (pid>0) {
        rc= waitpid(pid, &status, 0);
        if (rc!=pid) {
            printf("waitpid failed with rc %d, errno is %d\n", rc, errno);
        }
        else {
            if (WIFEXITED(status) && (WEXITSTATUS(status)==1)) {
                printf("Testcase 2 ok\n");
            }
            else {
                printf("Testcase 2 failed\n");
            }
        }
    }
}


/*
 *  we fork off a new process which is then killed and wait for it
 */
void testcase3() {
    int status;
    int pid;
    int rc;
    pid = fork();
    if (0==pid) {
        while(1);
    }
    if (pid<0) {
        printf("fork failed!\n");
        _exit(1);
    }
    if (pid>0) {
        kill(pid, SIGTERM);
        rc= waitpid(pid, &status, 0);
        if (rc!=pid) {
            printf("waitpid failed with rc %d, errno is %d\n", rc, errno);
        }
        else {
            if (WIFSIGNALED(status) && (WTERMSIG(status)==SIGTERM)) {
                printf("Testcase 3 ok\n");
            }
            else {
                printf("Testcase 3 failed\n");
            }
        }
    }
}

/*
 *  we fork off a new process which is then suspended and wait for it with WUNTRACED
 */
void testcase4() {
    int status;
    int pid;
    int rc;
    pid = fork();
    if (0==pid) {
        while(1);
    }
    if (pid<0) {
        printf("fork failed!\n");
        _exit(1);
    }
    if (pid>0) {
        kill(pid, SIGSTOP);
        rc= waitpid(pid, &status, WUNTRACED);
        if (rc!=pid) {
            printf("waitpid failed with rc %d, errno is %d\n", rc, errno);
        }
        else {
            if (WIFSTOPPED(status) && (WSTOPSIG(status)==SIGSTOP)) {
                printf("Testcase 4 ok\n");
            }
            else {
                printf("Testcase 4 failed\n");
            }
        }
    }
}

/*
 *  we fork off a new process which is then suspended. Then we kill the process and wait for it. Verify that the error
 *  status which we get corresponds to the exit reason
 */
void testcase5() {
    int status;
    int pid;
    int rc;
    pid = fork();
    if (0==pid) {
        while(1);
    }
    if (pid<0) {
        printf("fork failed!\n");
        _exit(1);
    }
    if (pid>0) {
        kill(pid, SIGSTOP);
        /*
         * Give it some time to handle the signal
         */
        sleep(1);
        kill(pid, SIGKILL);
        sleep(1);
        rc= waitpid(pid, &status, WUNTRACED);
        if (rc!=pid) {
            printf("waitpid failed with rc %d, errno is %d\n", rc, errno);
        }
        else {
            if (WIFSIGNALED(status) && (WTERMSIG(status)==SIGKILL)) {
                printf("Testcase 5 ok\n");
            }
            else {
                printf("Testcase 5 failed\n");
            }
        }
    }
}

static volatile int sigchld = 0;
void myhandler(int sig_no) {
    if (SIGCHLD==sig_no)
        sigchld = 1;
}

/*
 *  we fork off a new process which exits immediately and wait for the SIGCHLD. When this has arrived, we call wait to get the exit
 *  status
 */
void testcase6() {
    int status;
    int pid;
    int rc;
    signal(SIGCHLD, myhandler);
    sigchld = 0;
    pid = fork();
    if (0==pid) {
        _exit(2);
    }
    if (pid<0) {
        printf("fork failed!\n");
        _exit(1);
    }
    if (pid>0) {
        while (0==sigchld);
        rc= waitpid(pid, &status, WNOHANG);
        if (rc!=pid) {
            printf("waitpid failed with rc %d, errno is %d\n", rc, errno);
        }
        else {
            if (WIFEXITED(status) && (WEXITSTATUS(status)==2)) {
                printf("Testcase 6 ok\n");
            }
            else {
                printf("Testcase 6 failed\n");
            }
        }
    }
    signal(SIGCHLD, SIG_DFL);
}

int main()  {
    testcase1();
    testcase2();
    testcase3();
    testcase4();
    testcase5();
    testcase6();
    return 0;
}
