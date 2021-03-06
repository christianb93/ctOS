/*
 * unistd.c
 */

#include "lib/sys/types.h"
#include "lib/unistd.h"
#include "lib/os/oscalls.h"
#include "lib/errno.h"
#include "lib/stdarg.h"
#include "lib/stdlib.h"
#include "lib/sys/ioctl.h"
#include "lib/sys/stat.h"
#include "lib/string.h"
#include "lib/limits.h"

extern char** environ;

/*
 * Execute a program image
 * Parameters:
 * @path - name of the program image to execute
 * @argv - an array of strings which will be passed as argument to the new program, last entry should be a null pointer
 * @envp - an array of environment strings, last entry in array needs to be a null pointer as well
 * Return value:
 * -1 if the execution fails, otherwise this function does not return
 */
int execve(const char *path, char *const argv[], char *const envp[]) {
    return __ctOS_execve(path, argv, envp);
}


/*
 * Execute a program image
 * Parameters:
 * @path - name of the program image to execute
 * @argv - an array of strings which will be passed as argument to the new program, last entry should be a null pointer
 * Return value:
 * -1 if the execution fails, otherwise this function does not return
 */
int execvp(const char *path, char *const argv[]) {
    return __ctOS_execve(path, argv, environ);
}

/*
 * Execute a program image
 */
int execl(const char* path, const char* arg0, ...) {
    int argc = 1;
    char* arg;
    char** argv;
    va_list ap;
    int i;
    /*
     * First scan list to see how many additional arguments
     * we have after the path (including arg0)
     */
    if (arg0) {
        va_start(ap, arg0);
        while (1) {
            arg = va_arg(ap, char*);
            if (arg) {
                argc++;
            }
            else {
                break;
            }
        }   
    va_end(ap);    
    }
    /*
     * Now build up array and scan list a second time. Note that POSIX makes
     * it the responsibility of the caller to make sure that the first argument
     * is the filename, we do not do this and make no assumptions on the content
     * of arg0 and the other arguments. Thus our array will have argc + 1 elements
     * 
     * argv[0]    <---- arg0
     * argv[argc] <---- 0
     * 
     * Note that POSIX asks the caller to make sure that the variable list
     * of arguments is NULL terminated. However, we do not rely on this and
     * add a trailing NULL to be on the safe side.
     */
    argv = (char**) malloc(sizeof(char*)*(argc+1));
    if (0==argv) {
        errno = ENOMEM;
        return -1;
    }
    if (arg0) {
        va_start(ap, arg0);
        for (i=1;i<argc;i++) {
            arg = va_arg(ap, char*);
            argv[i]=arg;
        }
        va_end(ap);
    }
    argv[0] = (char*) arg0;
    argv[argc]=0;
    return execvp(path, argv);
}

/*
 * Execute a program image, using a filename FILE
 * 
 * The argument file is used to construct a pathname that identifies the new process image file. If the file argument 
 * contains a <slash> character, the file argument is used as the pathname for this file. Otherwise, the path prefix for this file is 
 * obtained by a search of the directories passed as the environment variable PATH. If this environment variable is not set, the default PATH
 * "/bin:/usr/bin" is used
 *
 */
int execlp(const char* file, const char* arg0, ...) {
    /*
     * This is a bit ugly at the moment - we should move the code that this
     * has in common with execl into a separate function at some point...
     */
    int argc = 1;
    char* arg;
    char** argv;
    va_list ap;
    const char* executable = 0;
    char* path;
    char* line;
    char* ptr;
    struct stat mystat;
    char buffer[PATH_MAX+1];
    int i;
    /*
     * We need to determine the full path of the executable. First we check whether the 
     * filename contains a slash, in this case we use it as is
     */
    if (0 == file) {
        return -1;
    }
    if (strrchr(file, '/')) {
        executable = file;
    }
    /*
     * If this did not work, we have to find the path
     */
    if (0 == executable) {
        path = getenv("PATH");
        if (0 == path)
            path = "/bin:/usr/bin";
        /*
         * We need to make a copy as
         * we will use strok
         */
        line = malloc(strlen(path) + 1);
        if (0 == line) {
            return -1;
        }
        strcpy(line, path);
        /*
         * Now walk the path components
         */
        ptr = strtok(line, ":");
        while (ptr) {
            /*
             * Assemble the full file name, ignoring names
             * that exceed the maximum size
             */
            if (strlen(ptr) + strlen(file) < PATH_MAX) {
                memset(buffer, 0, PATH_MAX);
                strcpy(buffer, ptr);
                if (ptr[strlen(ptr)-1] != '/')
                    strcpy(buffer + strlen(buffer), "/");
                strcpy(buffer + strlen(buffer), file);
                if (0 == __ctOS_stat(buffer, &mystat)) {
                    executable = buffer;
                    break;
                }
            }
            ptr = strtok(NULL, ":");
        }
        free(line);
    }
    /*
     * 
     * Scan list to see how many additional arguments
     * we have after the path (including arg0)
     */
    if (arg0) {
        va_start(ap, arg0);
        while (1) {
            arg = va_arg(ap, char*);
            if (arg) {
                argc++;
            }
            else {
                break;
            }
        }   
    va_end(ap);    
    }
    argv = (char**) malloc(sizeof(char*)*(argc+1));
    if (0==argv) {
        errno = ENOMEM;
        return -1;
    }
    if (arg0) {
        va_start(ap, arg0);
        for (i=1;i<argc;i++) {
            arg = va_arg(ap, char*);
            argv[i]=arg;
        }
        va_end(ap);
    }
    argv[0] = (char*) arg0;
    argv[argc]=0;
    return execvp(executable, argv);
}

/*
 * Put the process to sleep for a given number of seconds
 * Parameters:
 * @seconds - seconds to wait
 */
unsigned int sleep(unsigned int seconds) {
    return __ctOS_sleep(seconds);
}

/*
 * Cause a SIGALRM signal to be sent to the process in a given number of seconds
 *
 * If there is already a pending alarm for the process, the alarm is canceled and reset with the new value. If 0 seconds
 * are requested, any pending alarm is canceled, but no now alarm is set. A process can only have at most one pending alarm
 * at any point in time
 *
 * If a previous alarm is canceled by the call, alarm returns the number of seconds left for the previous alarm to expire. Otherwise,
 * 0 is returned
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * none
 */
unsigned int alarm(unsigned int seconds) {
    return __ctOS_alarm(seconds);
}

/*
 * Get PID of the currently executing process
 */
pid_t getpid() {
    return (pid_t) __ctOS_getpid();
}


/*
 * Change working directory
 *
 * The chdir() function will cause the directory named by the pathname pointed to by the path argument to become
 * the current working directory; that is, the starting point for path searches for pathnames not beginning with '/'.
 *
 * Upon successful completion, 0 will be returned. Otherwise, -1 will be returned, the current working directory
 * will remain unchanged, and errno will be set to indicate the error.
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * none
 */
int chdir(const char* path) {
    int rc = __ctOS_chdir((char*) path);
    if (rc) {
        errno = rc;
        return -1;
    }
    return 0;
}

/*
 * This is like fchdir, but the target directory is specified by an open file
 * descriptor that needs to refer to a directory
 */
int fchdir(int fildes) {
    int rc = __ctOS_fchdir(fildes);
    if (rc) {
        errno = rc;
        return -1;
    }
    return 0;
}


/*
 * Get the effective user ID of the currently active process
 *
 * The geteuid() function will return the effective user ID of the calling process.
 *
 * The geteuid() function will always be successful and no return value is reserved to indicate an error.
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * none
 *
 */
uid_t geteuid() {
    return __ctOS_geteuid();
}

/*
 * Set the effective user ID
 *
 * If uid is equal to the real user ID or the saved set-user-ID, or if the process is running with effective user id 0, seteuid()
 * will set the effective user ID of the calling process to uid; the real user ID and saved set-user-ID will remain unchanged.
 *
 * Upon successful completion, 0 will be returned; otherwise, -1 will be returned and errno set to indicate the error.
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * none
 *
 */
int seteuid(uid_t euid) {
    int rc = __ctOS_seteuid(euid);
    if (rc) {
        errno = -rc;
        return -1;
    }
    return 0;
}

/*
 * Get the user ID of a process
 *
 * The getuid() function will return the real user ID of the calling process.
 *
 * The getuid() function will always be successful and no return value is reserved to indicate the error.
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * none
 */
uid_t getuid() {
    return __ctOS_getuid();
}

/*
 * Set the user ID of a process
 *
 * If the process has effective user ID 0,  setuid() will set the real user ID, effective user ID, and the saved set-user-ID
 * of the calling process to uid.
 *
 * If the process is not running with effective user ID 0, but uid is equal to the real user ID or the saved set-user-ID,
 * setuid() will set the effective user ID to uid; the real user ID and saved set-user-ID will remain unchanged.
 *
 * Upon successful completion, 0 will be returned. Otherwise, -1 will be returned and errno set to indicate the error.
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * none
 */
int setuid(uid_t uid) {
    int rc = __ctOS_setuid(uid);
    if (rc) {
        errno = rc;
        return -1;
    }
    return 0;
}

/*
 * Get the effective group ID of a process
 *
 * The getegid() function will return the effective group ID of the calling process.
 *
 * The getegid() function will always be successful and no return value is reserved to indicate an error.
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * none
 */
gid_t getegid() {
    return __ctOS_getegid();
}

/*
 * Get the real group ID of a process
 *
 * The getgid() function will return the real group ID of the calling process.
 *
 * The getgid() function will always be successful and no return value is reserved to indicate an error.
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * none
 */
gid_t getgid() {
    return __ctOS_getgid();
}


/*
 * Duplicate a file descriptor
 *
 * The dup() function will create a copy of the file descriptor fd, using the lowest free file descriptor for the
 * new file descriptor.
 *
 * After a successful return, the old and the new file descriptor refer to the same open file description, or example,
 * if the file offset is modified by using lseek(2)  on one of the descriptors, the offset is also changed for the other.
 *
 * The file descriptor flags of the new file descriptor are set to zero and not shared.
 *
 * On success, dup() returns the new file descriptor. Otherwise, -1 is returned and errno is set to indicate the error.
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * none
 */
int dup(int fd) {
    int new_fd = __ctOS_dup(fd);
    if (new_fd < 0) {
        errno = -new_fd;
        return -1;
    }
    return new_fd;
}

/*
 * Determine whether a file descriptor is associated with a terminal
 *
 * The isatty() function will test whether fildes, an open file descriptor, is associated with a terminal device.
 *
 * It will return 1 if this is the case and 0 otherwise
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * none
 *
 *
 */
int isatty(int fildes) {
    return __ctOS_isatty(fildes);
}

/*
 * Get parent process ID
 *
 * The getppid() function will return the parent process ID of the calling process.
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * none
 */
pid_t getppid() {
    return __ctOS_getppid();
}


/*
 * Create a pipe
 *
 * The pipe() function will create a pipe and place two file descriptors, one each into the arguments fd[0] and fd[1], that refer
 * to the open file descriptions for the read and write ends of the pipe. Their integer values will be the two lowest available
 * at the time of the pipe() call. The FD_CLOEXEC flag will be clear on both file descriptors.
 *
 *  Data can be written to the file descriptor fd[1] and read from the file descriptor fd[0]. A read on the file descriptor
 *  fd[0] will access data written to the file descriptor fd[1] on a first-in-first-out basis.
 *
 *
 *  Upon successful completion, 0 will be returned; otherwise, -1 will be returned and errno set to indicate the error.
 *
 *  BASED ON: POSIX 2004
 *
 *  LIMITATIONS:
 *
 *  1) Upon successful completion, pipe() does not yet mark for update the st_atime, st_ctime, and st_mtime fields of the pipe.
 *
 */
int pipe(int* fd) {
    int rc = __ctOS_pipe(fd);
    if (rc) {
        errno = ENFILE;
        rc = -1;
    }
    return rc;
}


/*
 * Get the process group of a process
 *
 * The getpgrp() function will return the process group ID of the calling process.
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * none
 */
pid_t getpgrp() {
    return __ctOS_getpgrp();
}

/*
 * Set the process group of a process
 *
 * The setpgid() function will either join an existing process group within the session of the calling process or create a new
 * process group within the session of the calling process. The process group ID of a session leader will not change.
 *
 * Upon successful completion, the process group ID of the process with a process ID that matches pid will be set to pgid.
 * As a special case, if pid is 0, the process ID of the calling process will be used. Also, if pgid is 0, the
 * process ID of the indicated process will be used.
 *
 * Using this call, only the process group of the calling process or any of its children in the same session can be modified.
 * An attempt to change the process group of any other process will result in EPERM.
 *
 * Upon successful completion, setpgid() will return 0; otherwise, -1 will be returned and errno will be set to indicate the error.
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * none
 */
int setpgid(pid_t pid, pid_t pgid) {
    int rc;
    rc = __ctOS_setpgid(pid, pgid);
    if (0 != rc) {
        errno = rc;
        rc = -1;
    }
    return rc;
}

/*
 * Create a new session with the calling process as the session lead. The new session will contain a single process group
 * with the calling process as the process group lead and only member. The controlling terminal of the calling
 * process will be set to NONE so that the calling process does not have a controlling terminal upon return.
 *
 * This call will fail with EPERM if the calling process is already a process group lead
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * none
 */
int setsid() {
    int rc;
    rc = __ctOS_setsid();
    if (rc) {
        errno = -rc;
        return -1;
    }
    return 0;
}

/*
 * Get the session ID of the process pid. Note that as a session lead cannot change its process group and setsid() will establish
 * the caller as process group lead, this is the same as the process group ID of the session lead.
 *
 * If the pid argument is 0, the session ID of the calling process will be returned.
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * none
 */
pid_t getsid(pid_t pid) {
    pid_t rc;
    rc = __ctOS_getsid(pid);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    return rc;
}


/*
 * Set foreground process group of controlling terminal
 *
 * If the process has a controlling terminal, tcsetpgrp() will set the foreground process group ID associated with the terminal to
 * pgid_id. The application shall ensure that the file associated with fildes is the controlling terminal of the calling process
 * and the controlling terminal is currently associated with the session of the calling process. The application shall ensure
 * that the value of pgid_id matches a process group ID of a process in the same session as the calling process.
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * 1) Attempts to use tcsetpgrp() from a process which is a member of a background process group on a fildes associated
 * with its controlling terminal does not result in SIGTTOU be sent to the process group
 *
 */
int tcsetpgrp(int fildes, pid_t pgid_id) {
    return ioctl(fildes, TIOCSPGRP, &pgid_id);
}

/*
 * The tcgetpgrp() function will return the value of the process group ID of the foreground process group associated with the terminal.
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * none
 */
pid_t tcgetpgrp(int fildes) {
    pid_t pgid;
    int rc = ioctl(fildes, TIOCGPGRP, &pgid);
    if (0==rc)
        return pgid;
    errno = -rc;
    return -1;
}


/*
 * The getgroups() function will fill in the array grouplist with the current supplementary group IDs of the calling process.
 *
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * 1) As currently, supplementary group lists are not yet supported, this function will always return an empty list
 */
int getgroups(int gidsetsize, gid_t grouplist[]) {
    return 0;
}


/*
 * Dup2
 *
 */
int dup2(int fd1, int fd2) {
    int rc = __ctOS_dup2(fd1, fd2);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    return rc;
}

/*
 * Store the absolute path name of the current working directory in the provided buffer.
 * The second parameter is the size of the buffer. It the buffer space is not sufficient,
 * 0 is returned and errno is set to ERANGE.
 */
char *getcwd(char *buf, size_t size) {
    int rc;
    if (0==buf)
        return 0;
    rc = __ctOS_getcwd(buf, size);
    if (0==rc) {
        return buf;
    }
    errno = -rc;
    return 0;
}

/*
 * Implementation of access. As every file is accessible by root (the only user so far),
 * this is currently still the same as stat - to be updated once file system protection has
 * been implemented in the kernel
 */
int access(const char *path, int amode) {
    struct stat mystat;
    return stat(path, &mystat);
}

/*
 * Sync file system. As currently, the file system does not maintain
 * any caches, this call does not do anything
 */
void sync() {

}

/*
 * Return the current limit on the number of file descriptors
 * that a process can allocate
 * 
 * According to POSIX, this is equivalent to calling 
 * getrlimit() with the RLIMIT_NOFILE option.
 */
int getdtablesize(void) {
    /*
     * This is from fs.h (FS_MAX_FD)
     */
    return 128;
}

/* 
 * Return the memory page size
 * 
 */
int getpagesize() {
  return 4096;  
}


/*
 * Change the owner of a file. As file access rights are not yet supported by ctOS, this is
 * a dummy implementation
 */
int fchown(int fildes, uid_t owner, gid_t group) {
    /*
     * We only support owner 0 and group 0
     */
    if ((owner != 0) || (group != 0)) {
        return EINVAL;
    }
    return 0;
}

/*
 * This function will create a new directory entry for the existing file path1 called path2.
 *
 * Error codes:
 * EPERM - the file path1 is a directory
 * EEXIST - path2 does already exist
 * ENOENT - path1 or a component of path2 do not exist
 * 
 * Based on:
 * 
 * POSIX 2004
 * 
 * Limitations:
 * - st_ctime and st_mtime are not modified
 */
int link(const char *path1, const char *path2) {
    int rc = __ctOS_link(path1, path2);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    return 0;
} 

/*
 * ftruncate - truncate a file to a given length
 * 
 * This function will truncate the file referred to be the file descriptor fildes to the length given by the second parameter. If
 * the file is not a regular file or not open for writing, the operation will fail. 
 * 
 * If the file size is increased by this operation, i.e. if the new length exceeds the current length of the file, the extra bytes
 * will read as zero until the new space is filled by a write operation.
 * 
 * Errors:
 * EIO if an IO error occurred
 * EBADF if the file does not refer to an open file
 * EINVAL if the size is less than zero
 * EINVAL if the file has not been opened for writing
 * EPERM if the file is not a regular file
 * 
 * Returns:
 * 0 upon success
 * -1 if an error occurred (then errno will be set)
 * 
 */
int ftruncate(int fildes, off_t length) {
    int rc = __ctOS_ftruncate(fildes, length);
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    return 0;
}

/*
 * Write buffered contents for a file to disk. As ctOS does not support buffering on the file 
 * system level, this function does nothing
 */
int fsync(int fildes) {
    return 0;
}