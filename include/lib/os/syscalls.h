/*
 * syscalls.h
 *
 * This header file contains the system call numbers for all registered
 * system calls
 */

#ifndef __SYSCALLS_H_
#define __SYSCALLS_H_


#define __SYSNO_FORK 0
#define __SYSNO_PTHREAD_CREATE 1
#define __SYSNO_WRITE 2
#define __SYSNO_EXECV 3
#define __SYSNO_READ 4
#define __SYSNO_EXIT 5
#define __SYSNO_OPEN 6
#define __SYSNO_READDIR 7
#define __SYSNO_CLOSE 8
#define __SYSNO_UNLINK 9
#define __SYSNO_SBRK 10
#define __SYSNO_LSEEK 11
#define __SYSNO_SLEEP 12
#define __SYSNO_WAITPID 13
#define __SYSNO_KILL 14
#define __SYSNO_SIGACTION 15
#define __SYSNO_SIGRETURN 16
#define __SYSNO_SIGWAIT 17
#define __SYSNO_QUIT 18
#define __SYSNO_PAUSE 19
#define __SYSNO_SIGPROCMASK 20
#define __SYSNO_GETPID 21
#define __SYSNO_SIGPENDING 22
#define __SYSNO_CHDIR 23
#define __SYSNO_FCNTL 24
#define __SYSNO_STAT 25
#define __SYSNO_SETEUID 26
#define __SYSNO_GETEUID 27
#define __SYSNO_SETUID 28
#define __SYSNO_GETUID 29
#define __SYSNO_GETEGID 30
#define __SYSNO_DUP 31
#define __SYSNO_ISATTY 32
#define __SYSNO_GETPPID 33
#define __SYSNO_UMASK 34
#define __SYSNO_PIPE 35
#define __SYSNO_GETPGRP 36
#define __SYSNO_SETPGID 37
#define __SYSNO_IOCTL 38
#define __SYSNO_GETGID 39
#define __SYSNO_DUP2 40
#define __SYSNO_FSTAT 41
#define __SYSNO_TIMES 42
#define __SYSNO_GETCWD 43
#define __SYSNO_TCGETATTR 44
#define __SYSNO_TIME 45
#define __SYSNO_TCSETATTR 46
#define __SYSNO_SOCKET 47
#define __SYSNO_CONNECT 48
#define __SYSNO_SEND 49
#define __SYSNO_RECV 50
#define __SYSNO_LISTEN 51
#define __SYSNO_BIND 52
#define __SYSNO_ACCEPT 53
#define __SYSNO_SELECT 54
#define __SYSNO_ALARM 55
#define __SYSNO_SENDTO 56
#define __SYSNO_RECVFROM 57
#define __SYSNO_SETSOOPT 58
#define __SYSNO_UTIME 59
#define __SYSNO_CHMOD 60
#define __SYSNO_GETSOCKADDR 61
#define __SYSNO_MKDIR 62
#define __SYSNO_SIGSUSPEND 63
#define __SYSNO_RENAME 64
#define __SYSNO_SETSID 65
#define __SYSNO_GETSID 66
#define __SYSNO_LINK 67
#define __SYSNO_FTRUNCATE 68


unsigned int __ctOS_syscall (unsigned int __sysno, int argc, ...);

#endif /* __SYSCALLS_H_ */
