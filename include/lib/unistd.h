/*
 * unistd.h
 */

#ifndef _UNISTD_H_
#define _UNISTD_H_

#include "sys/types.h"
#include "getopt.h"

/*
 * Standard file descriptors
 */
#define STDIN_FILENO    0
#define STDOUT_FILENO   1
#define STDERR_FILENO   2

/*
 * Input for lseek
 */
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/*
 * Definitions for access
 */
#define F_OK 1
#define R_OK 2
#define W_OK 3
#define X_OK 4


pid_t fork();
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const char* buffer, size_t bytes);
int open(const char* path, int flags, ...);
int close(int fd);
int unlink(const char* path);
int rmdir(char* path);
off_t lseek(int fd, off_t offset, int whence);
int execve(const char *path, char *const argv[], char *const envp[]);
int execvp(const char *path, char *const argv[]);
int execl(const char* path, const char* arg0, ...);
void _exit(int status);
unsigned int sleep(unsigned int);
pid_t getpid();
int chdir(const char* path);
uid_t geteuid();
int seteuid(uid_t);
uid_t getuid();
int setuid(uid_t);
gid_t getegid();
gid_t getgid(void);
int dup(int fd);
mode_t umask(mode_t);
int pipe(int* fd);
pid_t getppid();
int isatty(int fd);
pid_t getpgrp();
pid_t tcgetpgrp(int fildes);
int setpgid(pid_t pid, pid_t pgid);
int setsid();
pid_t getsid(pid_t pid);
int getgroups(int gidsetsize, gid_t grouplist[]);
int tcsetpgrp(int fildes, pid_t pgid_id);
int dup2(int fd1, int fd2);
char *getcwd(char *buf, size_t size);
int access(const char *path, int amode);
void sync(void);
unsigned int alarm(unsigned int seconds);


#endif /* _UNISTD_H_ */
