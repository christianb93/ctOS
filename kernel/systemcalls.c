/*
 * systemcalls.c
 *
 * This module contains the dispatcher for the system calls and all entry points
 * For each system call foo, there is an entry point foo_entry which shows up in the
 * array systemcalls indexed by eax upon execution of int 0x80.
 * foo_entry receives the entire interrupt context, translates the parameters into the parameters
 * used by the actual procedure do_foo and calls do_foo. The return value of foo_entry is put into eax
 * by the dispatcher
 *
 * By convention, the register contents are interpreted as follows:
 *
 * EAX - system call number
 * EBX - parameter #1
 * ECX - parameter #2
 * EDX - parameter #3
 * ESI - parameter #4
 * EDI - parameter #5
 *
 * If more than five parameters are to be passed, EDI points to an array of integers, i.e. parameter five is *EDI and parameter six
 * is *(EDI+4)
 */

#include "systemcalls.h"
#include "pm.h"
#include "mm.h"
#include "debug.h"
#include "util.h"
#include "kerrno.h"
#include "fs.h"
#include "timer.h"
#include "lib/os/syscalls.h"
#include "lib/sys/ioctl.h"

/*
 * Macro to call validation function in memory manager
 */
#define VALIDATE(buffer, len, rw) do {if ((EXECUTION_LEVEL_USER == previous_execution_level) && \
        (mm_validate(buffer, len, rw))) return -EFAULT;} while(0);

/*
 * These are the entry points for all system calls
 * They extract parameters from the interrupt context and
 * call the actual implementation. Their result will be forwarded
 * by the dispatcher to the user space function.
 * Use the macro SYSENTRY to declare these entry points
 */

SYSENTRY(fork) {
    return do_fork(ir_context);
}

/*
 * System call to create a new kernel space thread
 * Parameters:
 * ebx: address of a pointer where the id of the newly created thread is stored
 * ecx: attributes of the new thread
 * edx: address of start routine
 * esi: pointer to argument
 * Currently this is only supported for kernel space threads
 */
SYSENTRY(pthread_create) {
    int rc;
    VALIDATE(ir_context->ebx, sizeof(pthread_t), 1);
    VALIDATE(ir_context->ecx, sizeof(pthread_attr_t), 0);
    VALIDATE(ir_context->edx, sizeof(u32), 0);
    rc = do_pthread_create((pthread_t*) ir_context->ebx,
            (pthread_attr_t*) ir_context->ecx,
            (pthread_start_routine) ir_context->edx, (void*) ir_context->esi,
            ir_context);
    if (0 == rc)
        return 0;
    return -rc;
}

/*
 * System call to write to an open file descriptor
 * Parameters:
 * ebx: file descriptor
 * ecx: pointer to data to be written
 * edx: number of bytes to write
 */
SYSENTRY(write) {
    if (0 == ir_context->edx)
        return 0;
    VALIDATE(ir_context->ecx, ir_context->edx, 0);
    return do_write(ir_context->ebx, (void*) ir_context->ecx, ir_context->edx);
}

/*
 * System call to read from an open file descriptor
 * Parameters:
 * ebx: file descriptor
 * ecx: pointer to buffer for the read data
 * edx: number of bytes to read
 */
SYSENTRY(read) {
    if (0 == ir_context->edx)
        return 0;
    VALIDATE(ir_context->ecx, ir_context->edx, 1);
    return do_read(ir_context->ebx, (void*) ir_context->ecx, ir_context->edx);
}

/*
 * System call to execute a program
 * Parameters:
 * ebx: pointer to file name
 */
SYSENTRY(execv) {
    int rc;
    VALIDATE(ir_context->ebx, 0, 0);
    rc = do_exec((char*) ir_context->ebx, (char**) ir_context->ecx, (char**) ir_context->edx, ir_context);
    if (rc)
        return -rc;
    return 0;
}

/*
 * Systemcall to exit a program
 * Parameters:
 * ebx - exit status
 */
SYSENTRY(exit) {
    do_exit(ir_context->ebx);
    return 0;
}

/*
 * Systemcall to open a file or directory
 * Parameters:
 * ebx - pointer to file name
 * ecx - flags
 */
SYSENTRY(open) {
    VALIDATE(ir_context->ebx, 0, 0);
    return do_open((char*) ir_context->ebx, ir_context->ecx, ir_context->edx);
}

/*
 * Systemcall to read from a directory
 * Parameters:
 * ebx - file descriptor
 * ecx - pointer to direntry structure
 */
SYSENTRY(readdir) {
    VALIDATE(ir_context->ecx, sizeof(direntry_t), 1);
    return do_readdir(ir_context->ebx, (direntry_t*) ir_context->ecx);
}

/*
 * Systemcall to close a file or directory
 * Parameters:
 * ebx - file descriptor
 */
SYSENTRY(close) {
    return do_close(ir_context->ebx);
}

/*
 * Systemcall to remove a file
 * Parameters:
 * ebx - file name
 */
SYSENTRY(unlink) {
    VALIDATE(ir_context->ebx, 0, 0);
    int rc = do_unlink((char*) ir_context->ebx);
    if (rc) {
        return -rc;
    }
    return 0;
}

/*
 * Systemcall to change the break (i.e. first unallocated byte above heap
 * in user space) of a process. Returns the new program break or 0 if
 * no extension was possible
 * Parameters:
 * ebx - number of bytes requested
 */
SYSENTRY(sbrk) {
    return do_sbrk(ir_context->ebx);
}

/*
 * Adapt the position within a file (lseek)
 * Parameters:
 * ebx - file descriptor
 * ecx - offset
 * edx - mode
 */
SYSENTRY(lseek) {
    return do_lseek(ir_context->ebx, ir_context->ecx, ir_context->edx);
}

/*
 * System call to put a process to sleep for the specified number of seconds
 * Parameters:
 * ebx - number of seconds
 */
SYSENTRY(sleep) {
    return do_sleep(ir_context->ebx);
}

/*
 * Wait for process completion
 * Parameters:
 * ebx - pid
 * ecx - pointer to an unsigned int where the status will be stored
 * edx - options
 * esi - rusage structure
 */
SYSENTRY(waitpid) {
    VALIDATE(ir_context->ecx, sizeof(int), 1);
    VALIDATE(ir_context->esi, sizeof(struct rusage), 1);
    return do_waitpid(ir_context->ebx, (int*) ir_context->ecx, ir_context->edx, (struct rusage*) ir_context->esi);
}

/*
 * Send a signal to a process
 * Parameters:
 * ebx - pid
 * ecx - signal number
 */
SYSENTRY(kill) {
    return do_kill(ir_context->ebx, ir_context->ecx);
}

/*
 * Get or set action associated with a signal
 * Parameters:
 * ebx - signal number
 * ecx - new action
 * edx - old action will be stored there
 */
SYSENTRY(sigaction) {
    VALIDATE(ir_context->ecx, sizeof(__ksigaction_t), 0);
    VALIDATE(ir_context->edx, sizeof(__ksigaction_t), 1);
    return do_sigaction(ir_context->ebx, (__ksigaction_t*) ir_context->ecx, (__ksigaction_t*) ir_context->edx);
}

/*
 * Complete a signal handler
 * Parameters:
 * ebx - signal number
 * ecx - pointer to saved signal frame
 */
SYSENTRY(sigreturn) {
    VALIDATE(ir_context->edx, sizeof(sig_frame_t), 0);
    return do_sigreturn(ir_context->ebx, (sig_frame_t*) ir_context->ecx, ir_context);
}

/*
 * Wait for a signal
 * Parameters:
 * ebx - signal masker
 * ecx - signal received will be stored there
 */
SYSENTRY(sigwait) {
    VALIDATE(ir_context->ecx, sizeof(int), 1);
    return do_sigwait(ir_context->ebx, (int*) ir_context->ecx);
}

/*
 * Quit the currently running task
 */
SYSENTRY(quit) {
    return do_quit();
}

/*
 * Pause until an interrupt has been delivered
 */
SYSENTRY(pause) {
    return do_pause();
}


/*
 * Change the signal mask
 * Parameters:
 * ebx - mode
 * ecx - signal mask to be applied
 * edx - old signal mask will be stored there
 */
SYSENTRY(sigprocmask) {
    VALIDATE(ir_context->ecx, sizeof(u32), 0);
    VALIDATE(ir_context->edx, sizeof(u32), 1);
    return do_sigprocmask(ir_context->ebx, (u32*) ir_context->ecx, (u32*) ir_context->edx);
}

/*
 * Get PID of the currently running process
 */
SYSENTRY(getpid) {
    return do_getpid();
}

/*
 * Get bitmask of pending signals
 * Parameters:
 * ebx - bitmask will be stored there
 */
SYSENTRY(sigpending) {
    VALIDATE(ir_context->ebx, sizeof(u32), 1);
    return do_sigpending((u32*) ir_context->ebx);
}

/*
 * Change working directory
 * Parameters:
 * ebx - new working directory
 */
SYSENTRY(chdir) {
    VALIDATE(ir_context->ebx, 0, 0);
    return (-1)*do_chdir((char*) ir_context->ebx);
}

/*
 * Fcntl
 * Parameters:
 * ebx - file descriptor
 * ecx - command
 * edx - additional integer arguments
 */
SYSENTRY(fcntl) {
    return do_fcntl(ir_context->ebx, ir_context->ecx, ir_context->edx);
}

/*
 * Stat
 * Parameters:
 * ebx - file name
 * ecx - stat structure to be filled
 */
SYSENTRY(stat) {
    VALIDATE(ir_context->ebx, 0, 0);
    VALIDATE(ir_context->ecx, sizeof(struct __ctOS_stat), 1);
    return do_stat((char*) ir_context->ebx, (struct __ctOS_stat*) ir_context->ecx);
}

/*
 * Seteuid
 * Parameters:
 * ebx - new effective uid
 */
SYSENTRY(seteuid) {
    return (-1)*do_seteuid(ir_context->ebx);
}

/*
 * Geteuid
 */
SYSENTRY(geteuid) {
    return do_geteuid();
}

/*
 * Setuid
 * Parameters:
 * ebx - new uid
 */
SYSENTRY(setuid) {
    return (-1)*do_setuid(ir_context->ebx);
}

/*
 * Getuid
 */
SYSENTRY(getuid) {
    return do_getuid();
}

/*
 * Getegid
 */
SYSENTRY(getegid) {
    return do_getegid();
}

/*
 * Dup
 * Parameters:
 * ebx - file descriptor
 */
SYSENTRY(dup) {
    return do_dup(ir_context->ebx, 0);
}

/*
 * isatty
 * Parameters:
 * ebx - file descriptor
 */
SYSENTRY(isatty) {
    return do_isatty(ir_context->ebx);
}

/*
 * Getppid
 */
SYSENTRY(getppid) {
    return do_getppid();
}

/*
 * Umask
 * Parameters:
 * ebx - new umask
 */
SYSENTRY(umask) {
    return do_umask(ir_context->ebx);
}

/*
 * Pipe
 * Parameters:
 * ebx - array of two file descriptors
 * ecx - flags
 */
SYSENTRY(pipe) {
    VALIDATE(ir_context->ebx, 2*sizeof(int), 1);
    return (-1)*do_pipe((int*) ir_context->ebx, ir_context->ecx);
}

/*
 * Get process group
 */
SYSENTRY(getpgrp) {
    return do_getpgrp();
}

/*
 * Set process group
 * Parameters:
 * ebx - pid
 * ecx - pgid
 */
SYSENTRY(setpgid) {
    return (-1)*do_setpgid(ir_context->ebx, ir_context->ecx);
}

/*
 * IOCTL
 *
 * This system case is a little bit more complex than the other entry points
 * as it needs to forward the call to different modules
 */
SYSENTRY(ioctl) {
    unsigned int request;
    int fd;
    fd = (int) ir_context->ebx;
    request = ir_context->ecx;
    switch (request) {
        case TIOCGPGRP:
            VALIDATE(ir_context->edx, sizeof(u32), 1);
            return fs_sgpgrp(fd, (u32*) ir_context->edx, 0);
        case TIOCSPGRP:
            VALIDATE(ir_context->edx, sizeof(u32), 0);
            return fs_sgpgrp(fd, (u32*) ir_context->edx, 1);
        case TIOCGETD:
            if (0 == ir_context->edx)
                return -EINVAL;
            VALIDATE(ir_context->edx, sizeof(u32), 1);
            *((u32*) ir_context->edx) = NTTYDISC;
            return 0;
        default:
            return do_ioctl(fd, request, (void*) ir_context->edx);
    }
    return -EINVAL;
}

/*
 * Get real group id
 */
SYSENTRY(getgid) {
    return do_getgid();
}

/*
 * Dup2
 * Parameters:
 * ebx - file descriptor
 * ecx - file descriptor where search is started to determine duplicated fd
 * Note that this implementation is not thread safe - if another thread reopens the second
 * file descriptor between the do_close and the do_open, it will select a higher file descriptor
 */
SYSENTRY(dup2) {
    do_close(ir_context->ecx);
    return do_dup(ir_context->ebx, ir_context->ecx);
}

/*
 * fstat
 * Parameters:
 * ebx - file descriptor
 * ecx - pointer to stat structure
 */
SYSENTRY(fstat) {
    VALIDATE(ir_context->ecx, sizeof(struct __ctOS_stat), 1);
    return do_fstat(ir_context->ebx, (struct __ctOS_stat*) ir_context->ecx);
}

/*
 * Times
 * Parameters:
 * ebx - pointer to accounting information
 */
SYSENTRY(times) {
    VALIDATE(ir_context->ebx, sizeof(struct __ktms), 1);
    return do_times((struct __ktms*) ir_context->ebx);
}

/*
 * Getcwd
 * Parameters:
 * ebx - buffer where current working directory is stored
 * ecx - size of buffer
 */
SYSENTRY(getcwd) {
    if (0 == ir_context->ecx)
        return -EINVAL;
    VALIDATE(ir_context->ebx, ir_context->ecx, 1);
    return do_getcwd((char*) ir_context->ebx, (size_t) ir_context->ecx);
}

/*
 * Tcgetattr
 * Parameters:
 * ebx - file descriptor
 * ecx - pointer to termios structure
 */
SYSENTRY(tcgetattr) {
    VALIDATE(ir_context->ecx, sizeof(struct termios), 1);
    return do_tcgetattr(ir_context->ebx, (struct termios*) ir_context->ecx);
}

/*
 * Time
 * Parameters:
 * ebx - pointer to time_t where result is returned
 */
SYSENTRY(time) {
    VALIDATE(ir_context->ebx, sizeof(time_t), 1);
    return do_time((time_t*) ir_context->ebx);
}

/*
 * tcsetattr
 * Parameters:
 * ebx - file descriptor
 * ecx - action
 * edx - termios structure
 */
SYSENTRY(tcsetattr) {
    VALIDATE(ir_context->edx, sizeof(struct termios), 0);
    return do_tcsetattr(ir_context->ebx, ir_context->ecx, (struct termios*) ir_context->edx);
}

/*
 * Socket
 * Parameters:
 * ebx - domain
 * ecx - type
 * edx - protocol
 */
SYSENTRY(socket) {
    return do_socket(ir_context->ebx, ir_context->ecx, ir_context->edx);
}

/*
 * Connect
 * Parameters:
 * ebx - file descriptor
 * ecx - socket address
 * edx - length of address
 */
SYSENTRY(connect) {
    if (0 == ir_context->edx)
        return -EINVAL;
    VALIDATE(ir_context->ecx, ir_context->edx, 0);
    return do_connect(ir_context->ebx, (struct sockaddr*) ir_context->ecx, ir_context->edx);
}

/*
 * Send
 * Parameters:
 * ebx - file descriptor
 * ecx - buffer
 * edx - length of buffer
 * esi - flags
 */
SYSENTRY(send) {
    if (0 == ir_context->edx)
        return -EINVAL;
    VALIDATE(ir_context->ecx, ir_context->edx, 0);
    return do_send(ir_context->ebx, (u8*) ir_context->ecx, ir_context->edx, ir_context->esi);
}

/*
 * Recv
 * Parameters:
 * ebx - file descriptor
 * ecx - buffer
 * edx - length of buffer
 * esi - flags
 *
 */
SYSENTRY(recv) {
    if (0 == ir_context->edx)
        return 0;
    VALIDATE(ir_context->ecx, ir_context->edx, 1);
    return do_recv(ir_context->ebx, (u8*) ir_context->ecx, ir_context->edx, ir_context->esi);
}

/*
 * Listen
 * Parameters:
 * ebx - file descriptor
 * ecx - backlog
 */
SYSENTRY(listen) {
    return do_listen(ir_context->ebx, ir_context->ecx);
}

/*
 * Bind
 * Parameters:
 * ebx - file descriptor
 * ecx - socket address
 * edx - length of address
 */
SYSENTRY(bind) {
    if (0 == ir_context->edx)
        return -EINVAL;
    VALIDATE(ir_context->ecx, ir_context->edx, 0);
    return do_bind(ir_context->ebx, (struct sockaddr*) ir_context->ecx, ir_context->edx);
}

/*
 * Accept
 * Parameter:
 * ebx - file descriptor
 * ecx - peer address is stored at this address
 * edx - pointer to length of address buffer
 */
SYSENTRY(accept) {
    if (0 == ir_context->edx)
        return -EFAULT;
    VALIDATE(ir_context->edx, sizeof(u32), 1);
    if (0 == *((u32*) ir_context->edx))
        return -EINVAL;
    VALIDATE(ir_context->ecx, *((u32*) ir_context->edx), 1);
    return do_accept(ir_context->ebx, (struct sockaddr*) ir_context->ecx, (u32*) ir_context->edx);
}

/*
 * Select
 * Parameters:
 * ebx - number of file descriptors
 * ecx - file descriptor set for read
 * edx - file descriptor set for write
 * esi - ignored
 * edi - timeout
 */
SYSENTRY(select) {
    VALIDATE(ir_context->ecx, sizeof(fd_set), 1);
    VALIDATE(ir_context->edx, sizeof(fd_set), 1);
    VALIDATE(ir_context->edi, sizeof(struct timeval), 0);
    return do_select(ir_context->ebx, (fd_set*) ir_context->ecx, (fd_set*) ir_context->edx, 0, (struct timeval*) ir_context->edi);
}

/*
 * Alarm
 * Parameters:
 * ebx - number of seconds
 */
SYSENTRY(alarm) {
    return do_alarm(ir_context->ebx);
}

/*
 * Sendto
 * Parameters:
 * ebx - file descriptor
 * ecx - buffer
 * edx - length of buffer
 * esi - flags
 * *edi - address
 * *(edi + 4) - address length
 */
SYSENTRY(sendto) {
    struct sockaddr* addr;
    int addrlen;
    if (0 == ir_context->edi)
        return -EFAULT;
    VALIDATE(ir_context->edi, 2*sizeof(u32), 0);
    addr = (struct sockaddr*) (*((u32*)ir_context->edi));
    if (0 == addr)
        return -EFAULT;
    addrlen = *((u32*)(ir_context->edi + 4));
    if (0 == addrlen)
        return -EINVAL;
    VALIDATE(addr, addrlen, 0);
    if (0 == ir_context->edx)
        return 0;
    VALIDATE(ir_context->ecx, ir_context->edx, 0);
    return do_sendto(ir_context->ebx, (void*) ir_context->ecx, ir_context->edx, ir_context->esi,
            addr, addrlen);
}

/*
 * Recvfrom
 * Parameters:
 * ebx - file descriptor
 * ecx - buffer
 * edx - length of buffer
 * esi - flags
 * *edi - address
 * *(edi + 4) - pointer to address length
 */
SYSENTRY(recvfrom) {
    struct sockaddr* addr;
    u32* addrlen;
    if (0 == ir_context->edi)
        return -EFAULT;
    VALIDATE(ir_context->edi, 2*sizeof(u32), 0);
    addr = (struct sockaddr*) (*((u32*)ir_context->edi));
    addrlen = (u32*) (*((u32*)(ir_context->edi + 4)));
    if ((0 == addr) || (0 == addrlen))
        return -EFAULT;
    VALIDATE(addrlen, sizeof(u32), 1);
    if (0 == *addrlen)
        return -EINVAL;
    VALIDATE(addr, *addrlen, 1);
    if (0 == ir_context->edx)
        return 0;
    VALIDATE(ir_context->ecx, ir_context->edx, 1);
    return do_recvfrom(ir_context->ebx, (void*) ir_context->ecx, ir_context->edx, ir_context->esi,
            addr, addrlen);
}

/*
 * Setsockopt
 * Parameters:
 * ebx - file descriptor
 * ecx - level
 * edx - option
 * esi - pointer to option value
 * edi - length of option value
 */
SYSENTRY(setsockopt) {
    if (0 == ir_context->edi)
        return -EINVAL;
    VALIDATE(ir_context->esi, ir_context->edi, 0);
    return do_setsockopt(ir_context->ebx, ir_context->ecx, ir_context->edx, (void*) ir_context->esi, ir_context->edi);
}

/*
 * utime
 * Parameter:
 * ebx - file name
 * ecx - new time
 */
SYSENTRY(utime) {
    VALIDATE(ir_context->ebx, 0, 0);
    VALIDATE(ir_context->ecx, sizeof(struct utimbuf), 0);
    return do_utime((char*) ir_context->ebx, (struct utimbuf*) ir_context->ecx);
}

/*
 * chmod
 * Parameter:
 * ebx - file name
 * ecx - new mode
 */
SYSENTRY(chmod) {
    VALIDATE(ir_context->ebx, 0, 0);
    return do_chmod((char*) ir_context->ebx, ir_context->ecx);
}

/*
 * Get local and foreign address of a socket
 * Parameter:
 * ebx - file descriptor
 * ecx - local address
 * edx - foreign address
 * esi - address length
 */
SYSENTRY(getsockaddr) {
    struct sockaddr* laddr = (struct sockaddr*) ir_context->ecx;
    struct sockaddr* faddr = (struct sockaddr*) ir_context->edx;
    u32* addrlen = (u32*) ir_context->esi;
    if (0 == addrlen)
        return -EINVAL;
    VALIDATE(addrlen, sizeof(u32), 1);
    if (0 == *addrlen)
        return 0;
    VALIDATE(laddr, *addrlen, 1);
    VALIDATE(faddr, *addrlen, 1);
    return do_getsockaddr(ir_context->ebx, laddr, faddr, addrlen);
}

/*
 * mkdir
 * Parameter:
 * ebx - file name
 * ecx - access mode
 */
SYSENTRY(mkdir) {
    VALIDATE(ir_context->ebx, 0, 0);
    return do_mkdir((char*) ir_context->ebx, ir_context->ecx);
}

/*
 * sigsuspend
 * Parameter:
 * ebx - signal mask
 * ecx - old signal mask
 */
SYSENTRY(sigsuspend) {
    VALIDATE(ir_context->ebx, sizeof(u32), 0);
    VALIDATE(ir_context->ecx, sizeof(u32), 1);
    return do_sigsuspend((u32*) ir_context->ebx, (u32*) ir_context->ecx);
}

/*
 * rename
 * Parameter:
 * ebx - old name
 * ecx - new name
 */
SYSENTRY(rename) {
    VALIDATE(ir_context->ebx, 0, 0);
    VALIDATE(ir_context->ecx, 0, 0);
    return do_rename((char*) ir_context->ebx, (char*) ir_context->ecx);
}

/*
 * link
 * Parameter:
 * ebx - old name
 * ecx - new name
 */
SYSENTRY(link) {
    VALIDATE(ir_context->ebx, 0, 0);
    VALIDATE(ir_context->ecx, 0, 0);
    return do_link((char*) ir_context->ebx, (char*) ir_context->ecx);
}


/*
 * setsid
 */
SYSENTRY(setsid) {
    return do_setsid();
}

/*
 * getsid
 * Parameter:
 * ebx - pid
 */
SYSENTRY(getsid) {
    return do_getsid(ir_context->ebx);
}
/*
 * This array contains all system call entry points and defines the mapping of
 * system call numbers to functions
 */

static st_handler_t systemcalls[] = { fork_entry,
        pthread_create_entry, write_entry, execv_entry, read_entry, exit_entry, open_entry,
        readdir_entry, close_entry, unlink_entry, sbrk_entry , lseek_entry, sleep_entry, waitpid_entry,
        kill_entry, sigaction_entry, sigreturn_entry, sigwait_entry, quit_entry, pause_entry,
        sigprocmask_entry, getpid_entry, sigpending_entry, chdir_entry, fcntl_entry, stat_entry,
        seteuid_entry, geteuid_entry, setuid_entry, getuid_entry, getegid_entry, dup_entry, isatty_entry,
        getppid_entry, umask_entry, pipe_entry, getpgrp_entry, setpgid_entry, ioctl_entry, getgid_entry,
        dup2_entry, fstat_entry, times_entry, getcwd_entry, tcgetattr_entry, time_entry, tcsetattr_entry, socket_entry,
        connect_entry, send_entry, recv_entry, listen_entry, bind_entry, accept_entry, select_entry, alarm_entry,
        sendto_entry, recvfrom_entry, setsockopt_entry, utime_entry, chmod_entry, getsockaddr_entry, mkdir_entry,
        sigsuspend_entry, rename_entry, setsid_entry, getsid_entry, link_entry};

#define SYSTEM_CALL_ENTRIES (sizeof(systemcalls) / sizeof(st_handler_t))



/*
 * Dispatcher for system calls
 * This dispatcher gets the entry points from the array
 * systemcalls based on the value of eax at the time when
 * int 0x80 was issued and calls the respective handler
 */
void syscall_dispatch(ir_context_t* ir_context, int previous_execution_level) {
    /*
     * Call number out of range?
     */
    if (ir_context->eax >= SYSTEM_CALL_ENTRIES) {
        ir_context->eax = -ENOSYS;
        return;
    }
    /*
     * Call number reserved?
     */
    if (0 == systemcalls[ir_context->eax]) {
        ir_context->eax = -ENOSYS;
        return;
    }
    ir_context->eax = (systemcalls[ir_context->eax])(ir_context, previous_execution_level);
}
