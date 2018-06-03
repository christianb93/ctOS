/*
 * unistd.c
 *
 */

#include "lib/os/oscalls.h"
#include "lib/os/syscalls.h"

pid_t __ctOS_getpid() {
    return __ctOS_syscall(__SYSNO_GETPID, 0);
}

int __ctOS_chdir(char* path) {
    return __ctOS_syscall(__SYSNO_CHDIR, 1, path);
}

int __ctOS_fchdir(int fd) {
    return __ctOS_syscall(__SYSNO_FCHDIR, 1, fd);
}

int __ctOS_seteuid(uid_t euid) {
    return __ctOS_syscall(__SYSNO_SETEUID, 1, euid);
}

uid_t __ctOS_geteuid() {
    return __ctOS_syscall(__SYSNO_GETEUID, 0);
}

int __ctOS_setuid(uid_t uid) {
    return __ctOS_syscall(__SYSNO_SETUID, 1, uid);
}

uid_t __ctOS_getuid() {
    return __ctOS_syscall(__SYSNO_GETUID, 0);
}

uid_t __ctOS_getegid() {
    return __ctOS_syscall(__SYSNO_GETEGID, 0);
}

uid_t __ctOS_getgid() {
    return __ctOS_syscall(__SYSNO_GETGID, 0);
}

int __ctOS_dup(int fd) {
    return __ctOS_syscall(__SYSNO_DUP, 1, fd);
}

int __ctOS_dup2(int fd1, int fd2) {
    return __ctOS_syscall(__SYSNO_DUP2, 2, fd1, fd2);
}

int __ctOS_isatty(int fd) {
    return __ctOS_syscall(__SYSNO_ISATTY, 1, fd);
}

int __ctOS_getppid() {
    return __ctOS_syscall(__SYSNO_GETPPID, 0);
}

int __ctOS_pipe(int* fd) {
    return __ctOS_syscall(__SYSNO_PIPE, 2, fd, 0);
}

pid_t __ctOS_getpgrp() {
    return __ctOS_syscall(__SYSNO_GETPGRP, 0);
}

int __ctOS_setpgid(pid_t pid, pid_t pgid) {
    return __ctOS_syscall(__SYSNO_SETPGID, 2, pid, pgid);
}

int __ctOS_getcwd(char* buffer, size_t n) {
    return __ctOS_syscall(__SYSNO_GETCWD, 2, buffer, n);
}

int __ctOS_setsid() {
    return __ctOS_syscall(__SYSNO_SETSID, 0);
}

pid_t __ctOS_getsid(pid_t pid) {
    return __ctOS_syscall(__SYSNO_GETSID, 1, pid);
}

int __ctOS_ftruncate(int fd, off_t size) {
    return __ctOS_syscall(__SYSNO_FTRUNCATE, 2, fd, size);
}