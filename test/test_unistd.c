#include "kunit.h"
#include "lib/sys/stat.h"

/*
 * We need a lot of stubs for this one 
 */
static const char* __last_path = 0;
static char** __last_argv = 0;
int __ctOS_execve(const char *path, char *const argv[], char *const envp[]) {
    __last_path = path;
    __last_argv = (char**) argv;
    return -1;
}

int __ctOS_sleep(int seconds) {
    return 0;
}

int __ctOS_alarm(int seconds) {
    return 0;
}

int __ctOS_getpid() {
    return 0;
}

int __ctOS_chdir(char* path) {
    return 0;
}


int __ctOS_getuid() {
    return 0;
}

int __ctOS_setuid(int uid) {
    return 0;
}

int __ctOS_geteuid() {
    return 0;
}

int __ctOS_seteuid(int uid) {
    return 0;
}

int __ctOS_getegid() {
    return 0;
}

int __ctOS_setegid(int gid) {
    return 0;
}

int __ctOS_getgid() {
    return 0;
}

int __ctOS_setgid(int gid) {
    return 0;
}

int __ctOS_dup(int fd) {
    return 0;
}

int __ctOS_isatty(int fd) {
    return 0;
}

int __ctOS_getppid() {
    return 0;
}

int __ctOS_pipe(int* fd) {
    return 0;
}

int __ctOS_getpgrp() {
    return 0;
}


int __ctOS_setpgid(int id) {
    return 0;
}

int __ctOS_setsid(int id) {
    return 0;
}

int __ctOS_getsid() {
    return 0;
}

int __ctOS_dup2(int fd1, int fd2) {
    return 0;
}

int __ctOS_getcwd(char* buffer, size_t n) {
    return 0;
}

int __ctOS_link(char* path1, char* path2) {
    return 0;
}

int __ctOS_ftruncate(int fd, int size) {
    return 0;
}

int __ctOS_stat(char* path, struct stat* mystat) {
    if (0 == strcmp("/bin/myfile", path)) {
        return 0;
    }
    return 1;
}

/*******************************************
 * Actual testcases start here             *
 ******************************************/

/*
 * Call execl without any additional arguments. 
 */
int testcase1() {
    __last_path = 0;
    __last_argv = 0;
    execl("myimage", 0);
    /*
     * Check __last_path
     */
    ASSERT(__last_path);
    ASSERT(0 == strcmp(__last_path, "myimage"));
    ASSERT(0 == __last_path[strlen("myimage")]);
    /*
     * Check the argv array. The array should have one entry only,
     * namely a null pointer
     */
    ASSERT(__last_argv);
    ASSERT(0 == __last_argv[0]);
    return 0;
}

/*
 * Call execl with an additional argument
 */
int testcase2() {
    __last_path = 0;
    __last_argv = 0;
    execl("myimage", "a", 0);
    /*
     * Check __last_path
     */
    ASSERT(__last_path);
    ASSERT(0 == strcmp(__last_path, "myimage"));
    ASSERT(0 == __last_path[strlen("myimage")]);
    /*
     * Check the argv array. The array should have two entries,
     * namely the string "a" and a null pointer
     */
    ASSERT(__last_argv);
    ASSERT(__last_argv[0]);
    ASSERT(0 == __last_argv[1]);
    ASSERT(0 == strcmp("a", __last_argv[0]));
    return 0;
}


/*
 * Call execlp without any additional arguments and with a
 * path containing a slash
 */
int testcase3() {
    __last_path = 0;
    __last_argv = 0;
    execlp("/myimage", 0);
    /*
     * Check __last_path
     */
    ASSERT(__last_path);
    ASSERT(0 == strcmp(__last_path, "/myimage"));
    ASSERT(0 == __last_path[strlen("/myimage")]);
    /*
     * Check the argv array. The array should have one entry only,
     * namely a null pointer
     */
    ASSERT(__last_argv);
    ASSERT(0 == __last_argv[0]);
    return 0;
}


/*
 * Call execlp without any additional arguments and with a
 * path that does not contain a slash
 */
int testcase4() {
    __last_path = 0;
    __last_argv = 0;
    execlp("myfile", 0);
    /*
     * Check __last_path
     */
    ASSERT(__last_path);
    ASSERT(0 == strcmp(__last_path, "/bin/myfile"));
    ASSERT(0 == __last_path[strlen("/bin/myfile")]);
    /*
     * Check the argv array. The array should have one entry only,
     * namely a null pointer
     */
    ASSERT(__last_argv);
    ASSERT(0 == __last_argv[0]);
    return 0;
}

int main() {
    INIT;
    RUN_CASE(1);
    RUN_CASE(2);
    RUN_CASE(3);
    RUN_CASE(4);
    END;
}