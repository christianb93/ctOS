/*
 * stat.c
 */

#include "lib/sys/stat.h"
#include "lib/os/oscalls.h"
#include "lib/errno.h"

/*
 * Stat
 *
 * The stat() function will obtain information about the named file and write it to the area pointed to by the buf argument. The
 * path argument points to a pathname naming a file. Read, write, or execute permission of the named file is not required.
 *
 * If the named file is a symbolic link, the stat() function will continue pathname resolution using the contents of the symbolic link,
 * and shall return information pertaining to the resulting file if the file exists.
 *
 * The buf argument is a pointer to a stat structure, as defined in the <sys/stat.h> header, into which information is placed
 * concerning the file.
 *
 *
 * Upon successful completion, 0 will be returned. Otherwise, -1 shall be returned and errno set to indicate the error.
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * 1) the time stamps st_atime, st_ctime and st_mtime always contain zero
 * 2) the field st_nlink is always 1
 * 3) as symbolic links are not yet supported, the sentence in the specification above referring to symbolic links is meaningless
 * 4) time-related fields are not updated
 */
int stat(const char* path, struct stat* buf) {
    int rc = __ctOS_stat(path, buf);
    if (rc<0) {
        errno = -rc;
        return -1;
    }
    return 0;
}

int lstat(const char* path, struct stat* buf) {
    return stat(path, buf);
}

/*
 * Fstat
 */
int fstat(int fd, struct stat* buf) {
    int rc = __ctOS_fstat(fd, buf);
    if (rc) {
        errno = rc;
        return -1;
    }
    return 0;
}


/*
 * Umask
 *
 * The umask() function will set the process' file mode creation mask to cmask and return the previous value of the mask.
 * Only the file permission bits of cmask (0-8, see <sys/stat.h>) are used, the other bits are ignored.
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * none
 */
mode_t umask(mode_t cmask) {
    return __ctOS_umask(cmask);
}


/*
 * Utime
 *
 * Set access and modification time of a file. If the second argument of the function is a NULL pointer, the current time
 * will be used for both mtime and atime.
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * - ctime is not updated
 */
int utime(const char *path, const struct utimbuf *times) {
    int res;
    int rc = 0;
    res = __ctOS_utime((char*) path, (struct utimbuf*) times);
    if (res < 0) {
        rc = -1;
        errno = -res;
    }
    return rc;
}

/*
 * Set the file mode bits (i.e. access right bits and SUID, SGID and sticky bit) for a file.
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * - no access rights and privileges are checked
 * - the SGID bit is not automatically cleared in the cases required by POSIX
 *
 */
int chmod(const char* path, mode_t mode) {
    int res;
    int rc = 0;
    res = __ctOS_chmod((char*) path, mode);
    if (res < 0) {
        rc = -1;
        errno = -res;
    }
    return rc;

}

/*
 * Create a directory, special file or regular file
 * 
 * TODO: actually do this
 */
int mknod(const char *path, mode_t mode, dev_t dev) {
    return 0;
}