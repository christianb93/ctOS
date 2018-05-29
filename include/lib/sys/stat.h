/*
 * stat.h
 */

#ifndef _STAT_H_
#define _STAT_H_


#include "types.h"

/*
 * File status information. Be careful when changing this - 
 * it needs to stay in sync with the corresponding structure
 * in os/stat.h
 * Note that st_dev is the device on which the file is located,
 * whereas st_rdev is the device that an inode (if it is a special
 * file) represents
 */
struct stat {
    dev_t st_dev;
    ino_t st_ino;
    mode_t st_mode;
    nlink_t st_nlink;
    uid_t st_uid;
    gid_t st_gid;
    off_t st_size;
    dev_t  st_rdev;            
    time_t st_atime;
    time_t st_mtime;
    time_t st_ctime;
};

/*
 * Bit mask for all bits which
 * contain the file type within the mode
 */
#define S_IFMT    0170000

/*
 * These are the bit masks for the
 * file types
 */
#define S_IFDIR   0040000   // Directory
#define S_IFCHR   0020000   // Character device
#define S_IFBLK   0060000   // Block device
#define S_IFREG   0100000   // Regular file
#define S_IFIFO   0010000   // FIFO
#define S_IFLNK   0120000   // Symbolic link
#define S_IFSOCK  0140000   // Socket

/*
 * Some file access modes
 */
#define S_IRWXU 0700
#define S_IRUSR 0400
#define S_IWUSR 0200
#define S_IXUSR 0100
#define S_IRWXG 0070
#define S_IWGRP 0020
#define S_IRGRP 0040
#define S_IXGRP 0010
#define S_IRWXO 0007
#define S_IROTH 0004
#define S_IWOTH 0002
#define S_IXOTH 0001
#define S_ISUID 04000
#define S_ISGID 02000
#define S_ISVTX 01000

/*
 * Test for file types
 */
#define S_ISDIR(m)   ((m & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)   ((m & S_IFMT) == S_IFCHR)
#define S_ISREG(m)   ((m & S_IFMT) == S_IFREG)
#define S_ISFIFO(m)   ((m & S_IFMT) == S_IFIFO)
#define S_ISLNK(m)   ((m & S_IFMT) == S_IFLNK)
#define S_ISSOCK(m)   ((m & S_IFMT) == S_IFSOCK)
#define S_ISBLK(m)   ((m & S_IFMT) == S_IFBLK)

int stat(const char* path, struct stat* buf);
int lstat(const char* path, struct stat* buf);
int fstat(int fd, struct stat* buf);
mode_t umask(mode_t cmask);
int chmod(const char* path, mode_t mode);
int mkdir(const char *path, mode_t mode);
int mknod(const char *path, mode_t mode, dev_t dev);

#endif /* _STAT_H_ */
