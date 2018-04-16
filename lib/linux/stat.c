/*
 * stat.c
 *
 */

#include "lib/sys/stat.h"
#include "lib/utime.h"

int __ctOS_stat(const char* path, struct stat* buf) {
    return 0;
}

int __ctOS_fstat(int fd, struct stat* buf) {
    return -1;
}

mode_t __ctOS_umask(mode_t cmask) {
    return 0;
}

int __ctOS_utime(char* path, struct utimbuf* times) {
    return 0;
}

int __ctOS_chmod(char* path, mode_t mode) {
    return 0;
}
