/*
 * stat.c
 */

#include "lib/os/oscalls.h"
#include "lib/os/syscalls.h"

/*
 * Stat
 */
int __ctOS_stat(const char* path, struct stat* buf) {
    return __ctOS_syscall(__SYSNO_STAT, 2, path, buf);
}

/*
 * fstat
 */
int __ctOS_fstat(int fd, struct stat* buf) {
    return __ctOS_syscall(__SYSNO_FSTAT, 2, fd, buf);
}


/*
 * Umask
 */
mode_t __ctOS_umask(mode_t cmask) {
    return __ctOS_syscall(__SYSNO_UMASK, 1, cmask);
}

/*
 * Utime
 */
int __ctOS_utime(char* path, struct utimbuf* times) {
    return __ctOS_syscall(__SYSNO_UTIME, 2, path, times);
}

/*
 * Chmod
 */
int __ctOS_chmod(char* path, mode_t mode) {
    return __ctOS_syscall(__SYSNO_CHMOD, 2, path, mode);
}
