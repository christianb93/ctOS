/*
 * fcntl.h
 *
 *  Created on: Nov 25, 2011
 *      Author: chr
 */

#ifndef _FCNTL_H_
#define _FCNTL_H_

#define O_RDONLY 0x0
#define O_WRONLY 0x1
#define O_RDWR 0x2

#define O_ACCMODE 0x3

#define O_CREAT 0x40
#define O_EXCL 0x80
#define O_TRUNC 0x200
#define O_APPEND 0x400
#define O_NONBLOCK 0x800

#define AT_FDCWD -200

/*
 * Commands for fcntl
 */
#define F_GETFD 1
#define F_SETFD 2
#define F_GETFL 3
#define F_SETFL 4
#define F_DUPFD 5

/*
 * File descriptor flags
 */
#define FD_CLOEXEC 0x1

int fcntl(int fd, int cmd, ...);
int open(const char* path, int flags, ...);
int openat(int dirfd, const char *pathname, int flags, ...);

#endif /* _FCNTL_H_ */
