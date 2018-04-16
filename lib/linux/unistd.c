/*
 * unistd.c
 *
 */


#include "lib/os/oscalls.h"

#define __NR_getpid      20
#define __NR_chdir       12
#define __NR_dup         41
#define __NR_pipe        42
#define __NR_getpgrp     65
#define __NR_setpgid     57
#define __NR_dup2        63

#define _syscall0(name) \
{ \
__asm__ volatile ("int $0x80" \
    : "=a" (__res) \
    : "a" (__NR_##name)); \
}

#define _syscall1(name,a) \
{ \
__asm__ volatile ("int $0x80" \
    : "=a" (__res) \
    : "a" (__NR_##name),"b" (a)); \
}


#define _syscall2(name,a, b) \
{ \
__asm__ volatile ("int $0x80" \
    : "=a" (__res) \
    : "a" (__NR_##name),"b" (a), "c" (b)); \
}
pid_t __ctOS_getpid() {
    int __res;
    _syscall0(getpid);
    return __res;
}

int __ctOS_chdir(char* path) {
    int __res;
    _syscall1(chdir, path);
    return __res;
}

int __ctOS_seteuid(uid_t euid) {
    return 1;
}

uid_t __ctOS_geteuid() {
    return 0;
}

int __ctOS_setuid(uid_t euid) {
    return 1;
}

uid_t __ctOS_getuid() {
    return 0;
}

uid_t __ctOS_getegid() {
    return 0;
}

uid_t __ctOS_getgid() {
    return 0;
}

int __ctOS_dup(int fd) {
    int __res;
    _syscall1(dup, fd);
    return __res;
}

int __ctOS_dup2(int fd, int new_fd) {
    int __res;
    _syscall2(dup2, fd, new_fd);
    return __res;
}

int __ctOS_isatty(int fd) {
    return 0;
}

int __ctOS_getppid() {
    return 1;
}

int __ctOS_pipe(int* fd) {
    int __res;
    _syscall1(pipe, fd);
    return __res;
}

pid_t __ctOS_getpgrp() {
    int __res;
    _syscall0(getpgrp);
    return __res;
}

int __ctOS_setpgid(pid_t pid, pid_t pgid) {
    int __res;
    _syscall2(setpgid, pid, pgid);
    return __res;
}

int __ctOS_setsid() {
    return 0;
}

pid_t __ctOS_getsid(pid_t pid) {
    return 0;
}

int __ctOS_getcwd(char* buffer, size_t n) {
    return -1;
}
