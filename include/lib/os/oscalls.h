/*
 * oscalls.h
 *
 */

#ifndef __OSCALLS_H_
#define __OSCALLS_H_

#include "types.h"
#include "signals.h"
#include "dirstreams.h"
#include "lib/sys/stat.h"
#include "lib/sys/times.h"
#include "lib/sys/resource.h"
#include "lib/termios.h"
#include "lib/sys/socket.h"
#include "lib/utime.h"

ssize_t __ctOS_read(int fd, char* buffer, size_t bytes);
ssize_t __ctOS_write(int fd, char* buffer, size_t bytes);
int __ctOS_open(char* path, int flags, int mode);
int __ctOS_close(int fd);
void __ctOS__exit(int status);
pid_t __ctOS_fork();
int __ctOS_unlink(char* path);
unsigned int __ctOS_sbrk(unsigned int size);
off_t __ctOS_lseek(int fd, off_t offset, int whence);
int __ctOS_execve(const char *path, char *const argv[], char *const envp[]);
int __ctOS_sleep(int seconds);
pid_t __ctOS_waitpid(pid_t pid, int* stat_loc, int options);
pid_t __ctOS_wait3(int* stat_loc, int options, struct rusage* ru);
int __ctOS_kill(pid_t pid, int sig_no);
int __ctOS_sigaction(int sig_no, __ksigaction_t* act,  __ksigaction_t* oldact);
int __ctOS_sigwait(unsigned int set, int* sig);
int __ctOS_pause();
int __ctOS_sigprocmask(int how, unsigned int* set, unsigned int* oset);
int __ctOS_sigsuspend(unsigned int* set, unsigned int* old_set);
pid_t __ctOS_getpid();
int __ctOS_sigpending(unsigned int* mask);
int __ctOS_getdent(int fd, __ctOS_direntry_t* direntry);
int __ctOS_chdir(char* path);
int __ctOS_mkdir(char* path, int mode);
int __ctOS_fcntl(int fd, int cmd, int arg);
int __ctOS_stat(const char* path, struct stat* buf);
int __ctOS_fstat(int fd, struct stat* buf);
int __ctOS_rename(char* old, char* new);
uid_t __ctOS_geteuid();
int __ctOS_seteuid(uid_t);
uid_t __ctOS_getuid();
int __ctOS_setuid(uid_t);
uid_t __ctOS_getegid();
uid_t __ctOS_getgid();
int __ctOS_dup(int);
int __ctOS_dup2(int, int);
int __ctOS_isatty(int);
pid_t __ctOS_getppid();
mode_t __ctOS_umask(mode_t);
int __ctOS_pipe(int*);
int __ctOS_setpgid(pid_t, pid_t);
pid_t __ctOS_getpgrp();
int __ctOS_ioctl(int fd, unsigned int request, unsigned int arg);
clock_t __ctOS_times(struct tms* times);
int __ctOS_getcwd(char* buffer, size_t n);
int __ctOS_tcgetattr(int fd, struct termios* termios_p);
int __ctOS_tcsetattr(int fd, int action, struct termios* termios_p);
time_t __ctOS_time(time_t* tloc);
int __ctOS_socket(int, int, int);
int __ctOS_connect(int, const struct sockaddr*, socklen_t);
ssize_t __ctOS_send(int, void*, size_t, int);
ssize_t __ctOS_sendto(int, void*, size_t, int, struct sockaddr*, socklen_t);
ssize_t __ctOS_recv(int, void*, size_t, int);
ssize_t __ctOS_recvfrom(int, void*, size_t, int, struct sockaddr*, socklen_t*);
int __ctOS_listen(int, int);
struct hostent* __ctOS_gethostbyname(const char* name);
int __ctOS_bind(int fd, const struct sockaddr *address,  socklen_t address_len);
int __ctOS_accept(int fd, struct sockaddr* addr, socklen_t* len);
int __ctOS_select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, struct timeval* timeout);
unsigned int __ctOS_alarm(unsigned int seconds);
int __ctOS_setsockopt(int socket, int level, int option_name, const void *option_value, socklen_t option_len);
int __ctOS_utime(char* path, struct utimbuf* times);
int __ctOS_chmod(char* path, mode_t mode);
int __ctOS_getsockaddr(int fd, struct sockaddr* laddr, struct sockaddr* faddr, socklen_t* addrlen);
int __ctOS_setsid();
pid_t __ctOS_getsid(pid_t pid);
int __ctOS_link(const char *path1, const char *path2);
int __ctOS_ftruncate(int fd, off_t size);

#endif /* __OSCALLS_H_ */
