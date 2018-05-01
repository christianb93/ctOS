/*
 * cli.c
 * This is the ctOS command line interface
 *
 *
 */


/************************************************
 * Standard header files                        *
 ***********************************************/

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <sys/wait.h>
#include <signal.h>
#include <dirent.h>
#include <sys/times.h>
#include <assert.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <os/if.h>
#include <os/route.h>
#include <netdb.h>
#include <pwd.h>

/****************************************************************************************
 * Some type declarations for networking                                                *
 ***************************************************************************************/

/*
 * This is an ICMP header
 */
typedef struct {
    unsigned char type;                // Type of message
    unsigned char code;                // Message code
    unsigned short checksum;           // Header checksum
} __attribute__ ((packed)) icmp_hdr_t;

/*
 * The body of an ECHO request message
 */
typedef struct {
    unsigned short id;                 // Identifier
    unsigned short seq_no;             // Sequence number
} __attribute__ ((packed)) icmp_echo_request_t;

/*
 * ICMP message types
 */
#define ICMP_ECHO_REPLY 0
#define ICMP_ECHO_REQUEST 8

/************************************************
 * Helper macros                                *
 ***********************************************/

/*
 * Execute a function and exit if it returns -1
 */
#define CHECK(x)   do { if (-1==(x)) { printf("Error %s at line %d\n", strerror(errno), __LINE__);  _exit(1); } } while (0);


/*
 * Maximum number of arguments that we expect
 */
#define MAX_ARGS 32

/************************************************
 * Structures                                   *
 ***********************************************/


/*
 * Forward declaration for job structure
 */
struct _job_t;

/*
 * This structure describes a process. It stores the PID of the process as
 * well as the job to which the process belongs. In addition, it contains the
 * command line used to start the process and the following filedescriptors:
 *
 * infd - used as input for the process
 * outfd - used as output for the process
 * auxfd - this file descriptor will be closed before the process executes, useful to close
 *         remaining end of a pipe
 *
 * As a job is a collection of processes,
 * a process has a next field to be usable in linked lists.
 */
#define CMD_CHARS 256
typedef struct _process_t {
    pid_t pid;
    struct _job_t* job;
    char cmd[CMD_CHARS];
    int infd;
    int outfd;
    int auxfd;
    struct _process_t* next;
} process_t;

/*
 * A job is a collection of processes with a common
 * process group id. It has a status, an ID (jobid) by which it is known to
 * the user and a command which is the command which has been used to create it
 */
typedef struct _job_t {
    pid_t pgid;
    int status;
    int jobid;
    char cmd[CMD_CHARS];
    process_t* process_list_head;
    struct _job_t* next;
} job_t;

/*
 * A list of known jobs
 */
static job_t* jobs_head = 0;

/*
 * Valid values for job->status. A job is considered
 * running if all processes it contains are running. As soon as one
 * process within the job is stopped, the entire job is considered stopped.
 * As jobs are usually stopped via job control signals which are sent to the
 * entire process group, this typically means that all other processes are stopped as well
 * or will be stopped in an instant
 */
#define JOB_STATUS_RUNNING 1
#define JOB_STATUS_STOPPED 2


/*
 * This is a command callback
 */
typedef void (*cmd_callback_t)(char* parms);

/*
 * This structure describes a command
 */
typedef struct {
    char* token;
    cmd_callback_t callback;
} cmd_t;



/*
 * Forward declarations of our commands
 */
void cmd_help(char* parms);
void cmd_exit(char* line);
void cmd_dump(char* line);
void cmd_dir(char* line);
void cmd_write(char* line);
void cmd_append(char* line);
void cmd_create(char* line);
void cmd_rm(char* line);
void cmd_rmdir(char* line);
void cmd_mkdir(char* line);
void cmd_test(char* line);
void cmd_spawn(char* line);
void cmd_run(char* line);
void cmd_kill(char* line);
void cmd_cd(char* line);
void cmd_pipe(char* line);
void cmd_jobs(char* line);
void cmd_fg(char* line);
void cmd_bg(char* line);
void cmd_times(char* line);
void cmd_pwd(char* line);
void cmd_date(char* line);
void cmd_net(char* line);
void cmd_route(char* line);
void cmd_ping(char* line);
void cmd_dns(char* line);
void cmd_whoami(char* line);
void cmd_host(char* line);
void cmd_http(char* line);

/*
 * A list of supported commands
 */
static cmd_t cmds[] = { { "help", cmd_help },
        { "exit", cmd_exit },
        { "dump", cmd_dump },
        { "dir", cmd_dir },
        { "append", cmd_append },
        { "write", cmd_write },
        { "create", cmd_create },
        { "mkdir", cmd_mkdir },
        { "rm", cmd_rm },
        { "rmdir", cmd_rmdir},
        { "spawn", cmd_spawn },
        { "run", cmd_run },
        { "kill", cmd_kill },
        { "test", cmd_test },
        { "cd", cmd_cd},
        { "pipe", cmd_pipe},
        { "jobs", cmd_jobs},
        { "fg", cmd_fg},
        { "bg", cmd_bg},
        { "times", cmd_times},
        { "pwd", cmd_pwd},
        { "date", cmd_date},
        { "net", cmd_net},
        { "route", cmd_route},
        { "ping", cmd_ping},
        { "dns", cmd_dns},
        { "whoami", cmd_whoami},
        { "host", cmd_host},
        { "http", cmd_http},
        { 0, 0 } };




/************************************************
 * The next few functions are used to work with *
 * jobs                                         *
 ***********************************************/


/*
 * Remove a job from the list of jobs. Note that
 * this function does not deallocate any memory
 * Parameters:
 * @job - the job to be removed
 */
static void remove_job(job_t* job) {
    job_t* job_iter;
    if (0==job)
        return;
    /*
     * First one in the queue?
     */
    if (job==jobs_head) {
        jobs_head = job->next;
    }
    /*
     * Not the first one. Navigate to predecessor
     * and adapt next pointer
     */
    else {
        job_iter = jobs_head;
        while (job_iter) {
            if (job_iter->next==job)
                break;
            job_iter = job_iter->next;
        }
        if(job_iter) {
            /*
             * Adapt pointer
             */
            job_iter->next = job->next;
        }
    }
}

/*
 * Remove a process from the list of processes
 * within the corresponding job without decallocating
 * any memory.
 * Parameters:
 * @proc - the process to be removed
 */
static void remove_proc(job_t* job, process_t* proc) {
    process_t* proc_iter;
    if (0==proc)
        return;
    /*
     * First one in the queue?
     */
    if (proc==job->process_list_head) {
        job->process_list_head = proc->next;
    }
    /*
     * Not the first one. Navigate to predecessor
     * and adapt next pointer
     */
    else {
        proc_iter = job->process_list_head;
        while (proc_iter) {
            if (proc_iter->next==proc)
                break;
            proc_iter = proc_iter->next;
        }
        if(proc_iter) {
            /*
             * Adapt pointer
             */
            proc_iter->next = proc->next;
        }
    }
}

/*
 * Given a process ID, return the respective process or NULL if there is no such process
 * Parameter:
 * @pid - the pid of the process we are looking for
 * Return value:
 * a pointer to the process if it could be found, 0 otherwise
 */
static process_t* get_proc(pid_t pid) {
    job_t* job = jobs_head;
    process_t* proc;
    while (job) {
        proc = job->process_list_head;
        while(proc) {
            if (proc->pid==pid) {
                return proc;
            }
            proc = proc->next;
        }
        job = job->next;
    }
    return 0;
}

/*
 * Given a jobid, return the respective job or NULL
 * Parameter:
 * @jobid - id of the job (field jobid in job_t structure) to look for
 * Return value:
 * a pointer to the job or 0 if no matching job could be found
 */
static job_t* get_job(pid_t jobid) {
    job_t* job = jobs_head;
    while (job) {
        if (job->jobid==jobid)
            return job;
        job = job->next;
    }
    return 0;
}

/*
 * Create and return a job structure
 * Parameter:
 * @cmd_line - the command to be stored in the job structure
 * Return value:
 * a pointer to the newly created job
 */
static job_t* create_job(char* cmd_line) {
    job_t* job;
    job = (job_t*) malloc(sizeof(job_t));
    assert(job);
    job->process_list_head = 0;
    job->next = 0;
    job->pgid = 0;
    memset(job->cmd, 0, CMD_CHARS);
    if (cmd_line) {
        strncpy(job->cmd, cmd_line, CMD_CHARS);
        job->cmd[CMD_CHARS-1]=0;
    }
    return job;
}

/*
 * Add a job to the job list and set
 * its jobid to the largest jobid in use plus 1
 * Parameter:
 * @job - the job to be added to the list
 */
static void add_job(job_t* job) {
    job_t* tmp;
    if (0==job)
        return;
    if (0==jobs_head) {
        jobs_head = job;
        job->jobid = 1;
    }
    else {
        job->jobid = 1;
        tmp = jobs_head;
        if (tmp->jobid>=1)
            job->jobid = tmp->jobid+1;
        while (tmp->next) {
            tmp = tmp->next;
            if (tmp->jobid>=job->jobid)
                job->jobid = tmp->jobid + 1;
        }
        tmp->next = job;
    }
    job->next = 0;
}

/*
 * Create a process and add it to the job
 * Parameter:
 * @job - the job to which the new process is to be added
 * @infd - use as infd
 * @outfd - use as outfd
 * @auxfd - use as auxfd
 *
 */
static process_t* add_proc(job_t* job, int infd, int outfd, int auxfd) {
    if (0==job) {
        printf("Invalid argument in add_proc - job must never be NULL\n");
        _exit(1);
    }
    process_t*proc = (process_t*) malloc(sizeof(process_t));
    process_t* tmp;
    if (0==proc) {
        printf("Could not get memory for proc structure\n");
        _exit(1);
    }
    proc->job = job;
    proc->infd = infd;
    proc->outfd = outfd;
    proc->auxfd = auxfd;
    if (0==job->process_list_head) {
        job->process_list_head=proc;
    }
    else {
        tmp = job->process_list_head;
        while (tmp->next) {
            tmp = tmp->next;
        }
        tmp->next = proc;
    }
    proc->next = 0;
    return proc;
}


/************************************************
 * Various utility functions                    *
 ***********************************************/

/*
 * Block all job control signals, i.e:
 * SIGCHLD
 * SIGTTOU
 * SIGTTIN
 * SIGTSTP
 */
static void block_signals() {
    sigset_t sigmask;
    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGCHLD);
    sigaddset(&sigmask, SIGTTIN);
    sigaddset(&sigmask, SIGTTOU);
    sigaddset(&sigmask, SIGTSTP);
    CHECK(sigprocmask(SIG_BLOCK, &sigmask, 0));
}

/*
 * Unblock all job control signals, i.e:
 * SIGCHLD
 * SIGTTOU
 * SIGTTIN
 * SIGTSTP
 */
static void unblock_signals() {
    sigset_t sigmask;
    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGCHLD);
    sigaddset(&sigmask, SIGTTIN);
    sigaddset(&sigmask, SIGTTOU);
    sigaddset(&sigmask, SIGTSTP);
    CHECK(sigprocmask(SIG_UNBLOCK, &sigmask, 0));
}

/*
 * Signal handler for SIGCHLD. This signal handler will use the waitpid system call with the parameters
 * WUNTRACED and WNOHANG to wait for status changes for any child processes of the shell. If a status change
 * occurs, the status of the corresponding job is updated.
 *
 * If a process is terminated, it is removed from the corresponding job. When the last process within a job
 * has been removed in this way, the entire job is removed from the queues.
 */
static void handle_sigchld(int signo) {
    int rc;
    int status;
    job_t* job;
    process_t* proc;
    while (1) {
        rc = waitpid(-1, &status, WNOHANG | WUNTRACED);
        /*
         * If no status is available for any child or if there are no
         * children left, exit
         */
        if ((0==rc) || (-1==rc))
            break;
        /*
         * See whether we can find the process in one of our jobs
         */
        proc = get_proc(rc);
        job = (proc ? proc->job : 0);
        if (WIFSTOPPED(status)) {
            if (job) {
                if (JOB_STATUS_RUNNING==job->status)
                    printf("Job %d stopped due to signal %d\n", job->jobid, WSTOPSIG(status));
                proc->job->status = JOB_STATUS_STOPPED;
            }
        }
        /*
         * A process has terminated
         */
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            /*
             * Remove process from queue of processes in the respective job match.
             */
            if (proc) {
                remove_proc(proc->job, proc);
                free(proc);
            }
            /*
             * Now check whether queue is now empty. If yes, remove job match from queue of jobs as
             * well
             */
            if (job) {
                if (0==job->process_list_head) {
                    if (WIFEXITED(status))
                        printf("Job %d completed\n", job->jobid);
                    if (WIFSIGNALED(status))
                        printf("Job %d terminated due to signal\n", job->jobid, WTERMSIG(status));
                    remove_job(job);
                    free(job);
                }
            }
        }
    }
}

/*
 * Wait for a foreground job, i.e. wait in a loop until all processes in
 * the job have either terminated or the first process has stopped.
 *
 * This function assumes that SIGCHLD is blocked
 *
 */
static void wait_fg_job(job_t* job) {
    int rc;
    int status;
    process_t* proc;
    while(1) {
        rc = waitpid(-job->pgid, &status, WUNTRACED);
        /*
         * If there are no more children in this process group, exit
         */
        if ((-1==rc) && (EINTR!=errno))
            return;
        /*
         * If status information has been reported,
         * update status of job
         */
        if (rc >0) {
            proc = get_proc(rc);
            /*
             * If a process has been stopped, update the status of the job and return.
             * All subsequent events will be passed by the signal handler once we
             * unblock signal again
             */
            if (WIFSTOPPED(status)) {
                if (job)
                    printf("Job %d stopped due to signal %d\n", job->jobid, WSTOPSIG(status));
                job->status = JOB_STATUS_STOPPED;
                return;
            }
            /*
             * A process has terminated. Remove it from the job. If the process was the
             * last within the job, remove the job as well and return
             */
            if (WIFSIGNALED(status) || WIFEXITED(status)) {
                if (proc) {
                    remove_proc(job, proc);
                    free(proc);
                }
                if (0==job->process_list_head) {
                    if (WIFSIGNALED(status))
                        printf("Job %d terminated due to signal %d\n", job->jobid, WTERMSIG(status));
                    remove_job(job);
                    free(job);
                    return;
                }
            }
        }
    }
}

/*
 * Start a job. This function will loop through all processes which are part of a job
 * and use their cmd field to determine the name of the program image to be started. It
 * will start this image in the background or foreground, depending on the second parameter,
 * and fill in the pid field of the processes as well as the pgid field of the job
 * Parameters:
 * @job - the job to be started
 * @foreground - start job in foreground (1) or background (0)
 */
static void start_job(job_t* job, int foreground) {
    process_t* proc;
    int pid;
    char* argv[MAX_ARGS+1];
    char* env[3];
    char* ptr;
    int i;
    char delimiter[] = " ";
    env[0]="HOME=/";
    env[1]="TERM=ctos";
    env[2]=0;
    if (0==job)
        return;
    /*
     * Block signals
     */
    block_signals();
    job->pgid = 0;
    /*
     * Walk list of processes
     */
    proc = job->process_list_head;
    while(proc) {
        /*
         * Fork off child process
         */
        CHECK(pid=fork());
        /*
         * This code is executed by the child
         */
        if (0==pid) {
            /*
             * Put the child into the new process group. If we are the first child to
             * execute, our copy of the job structure will have pgid zero, so we start a new
             * process group. Otherwise, pgid will already have been filled by the parent
             * and we join the group.
             */
            CHECK(setpgid(0, job->pgid));
            /*
             * and grab controlling terminal if this is a foreground process and we are the first child
             * Note that we have inherited the controlling terminal from the parent
             */
            if ((foreground) && (0==job->pgid)) {
                CHECK(tcsetpgrp(STDIN_FILENO, getpgrp()));
            }
            /*
             * Do redirection if needed and close auxfd
             */
            if (-1 != proc->auxfd) {
                close(proc->auxfd);
            }
            if (proc->infd != STDIN_FILENO) {
                close(STDIN_FILENO);
                dup(proc->infd);
                /*
                 * It is important to close the now duplicated file descriptor, otherwise
                 * a broken pipe will not be detected properly
                 */
                close(proc->infd);
            }
            if (proc->outfd != STDOUT_FILENO) {
                close(STDOUT_FILENO);
                dup(proc->outfd);
                /*
                 * See above
                 */
                close(proc->outfd);
            }
            /*
             * Now prepare arguments, unblock job control signals and
             * run the actual program image
             */
            i = 0;
            ptr = strtok(proc->cmd, delimiter);
            while ((ptr != NULL) && (i < MAX_ARGS)) {
                argv[i] = ptr;
                i++;
                ptr = strtok(NULL, delimiter);
            }
            argv[i] = 0;
            unblock_signals();
            execve(proc->cmd, argv, env);
            fprintf(stderr, "Execution failed\n");
            _exit(1);
        }
        /*
         * This code is executed by the parent
         */
        else {
            /*
             * Store pid
             */
            proc->pid = pid;
            /*
             * If pgid is still 0, set it to pid of child so that the second child will already find
             * the process group id in its copy of the job structure
             */
            if (0==job->pgid)
                job->pgid = pid;
            /*
             * We want to make sure that the process has really been put in the foreground process group before we continue.
             * As we do not know how far the child got with its preparations, we do the same thing to avoid races and
             * to have a defined state
             */
            CHECK(setpgid(pid, job->pgid));
        }
        proc = proc->next;
    }
    /*
     * At this point all processes have been started
     */
    job->status = JOB_STATUS_RUNNING;
    /*
     * Close unneeded file descriptors to allow the children to properly detect a broken pipe.
     * Doing this here as both childs are already running looks like a potential race - need to think about this...
     */
    proc = job->process_list_head;
    while(proc) {
        if (-1 != proc->auxfd)
            close(proc->auxfd);
        proc = proc->next;
    }
    if (foreground) {
        /*
         * Again we also claim the terminal to avoid races
         */
        CHECK(tcsetpgrp(STDIN_FILENO, job->pgid));
        /*
         * We now wait for the process and then claim the terminal
         * again. Note that wait_fg_job might clean up the job
         */
        wait_fg_job(job);
        CHECK(tcsetpgrp(STDIN_FILENO, getpgrp()));
    }
    else {
        /*
         * Make sure to unblock job control signals again so
         * that we learn about a status change
         */
        unblock_signals();
    }
}

/*
 * Get a command callback. This function will scan the
 * list of callback functions and return a match
 * Parameters:
 * @cmd - the command string to search for
 * @cmd_list - list of commands
 * Return value:
 * callback function if the command is in the list, 0 otherwise
 */
static cmd_callback_t get_callback(char* cmd, cmd_t* cmd_list) {
    cmd_callback_t cmd_callback = 0;
    int i = 0;
    if ((0 == cmd) || (0==cmd_list))
        return 0;
    while (cmd_list[i].token) {
        if (0 == strcmp(cmd_list[i].token, cmd))
            cmd_callback = cmd_list[i].callback;
        i++;
    }
    return cmd_callback;
}

/*
 * Utility function to convert a 3-bit file permission mode
 * to a string like "rwx" which is printed to stdout
 * Parameter:
 * @mode - the file access bits
 */
static void convert_mode(int mode) {
    int flags = mode & 07;
    if (flags & 4) {
        printf("r");
    }
    else {
        printf("-");
    }
    if (flags & 2) {
        printf("w");
    }
    else {
        printf("-");
    }
    if (flags & 1) {
        printf("x");
    }
    else {
        printf("-");
    }
}


/************************************************
 * The actual commands start here. All commands *
 * accept the parameter string as only argument *
 ***********************************************/

/*
 * Print usage information
 */
void cmd_help(char* line) {
    printf("Available commands:\n");
    printf("help -  print this screen\n");
    printf("dump - create ASCII dump of a file\n");
    printf("dir -  list contents of current directory\n");
    printf("write <file> -  write data to a file \n");
    printf("append <file> -  append data to file\n");
    printf("create -  create an empty file if it does not exist\n");
    printf("mkdir - create an empty directory\n");
    printf("spawn - spawn a new process image\n");
    printf("run - run a new process image\n");
    printf("kill <pid> <sig_no> - send a signal to a process\n");
    printf("bg <jobid> - continue a stopped job in the background\n");
    printf("fg <jobid> - continue a stopped job in the foreground\n");
    printf("jobs - list all jobs\n");
    printf("cd - change current working directory\n");
    printf("pwd - show current working directory\n");
    printf("rm <file> - remove a file\n");
    printf("rmdir <file> - remove a directory\n");
    printf("pipe <prg1> <prg2> - run program 1 with its output piped into program 2\n");
    printf("date - print current date\n");
    printf("times - show CPU usage of current process\n");
    printf("net - network interface configuration, type net help for list\n");
    printf("route - routing table configuration, type route help for more\n");
    printf("dns - manage DNS server configuration\n");
    printf("whoami - print user information\n");
    printf("ping <ip_address> - ping a remote host\n");
    printf("host <hostname> - resolve hostname\n");
    printf("http <URL> - get and dump web page\n");
    printf("exit - leave current instance of CLI\n");
}

/*
 * Get CPU accounting information
 */
void cmd_times(char* line) {
    struct tms mytimes;
    clock_t uptime = times(&mytimes);
    printf("CPU accounting information of current process (%d) in ticks:\n", getpid());
    printf("------------------------------------------------------------\n");
    printf("User space time:                %d\n", mytimes.tms_utime);
    printf("Kernel space time:              %d\n", mytimes.tms_stime);
    printf("User space time of children:    %d\n", mytimes.tms_cutime);
    printf("Kernel space time of children:  %d\n", mytimes.tms_cstime);
    printf("Uptime:                         %d\n", uptime);
}

/*
 * Print user information
 */
void cmd_whoami(char* line) {
    struct passwd* mypwd;
    uid_t me;
    me = getuid();
    mypwd = getpwuid(me);
    if (0 == mypwd) {
        printf("Could not get record for UID %d from user database\n", me);
        return;
    }
    printf("UID:            %d\n", me);
    printf("User:           %s\n", mypwd->pw_name);
    printf("GID:            %d\n", mypwd->pw_gid);
    printf("Home directory: %s\n", mypwd->pw_dir);
    printf("Shell:          %s\n", mypwd->pw_shell);
    return;
}

/*
 * Exit. This will exit from the shell
 */
void cmd_exit(char* line) {
    int status = 0;
    char* token = strtok(line, " \n");
    if (token)
        status = strtol(token, 0, 10);
    _exit(status);
}

/*
 * Create a pipe
 */
void cmd_pipe(char* line) {
    int fd[2];
    job_t* job;
    process_t* proc;
    char* pgm1;
    char* pgm2;
    pgm1 = strtok(line, " \n");
    if (0==pgm1) {
        printf("Missing parameter\n");
    }
    pgm2 = strtok(0, " \n");
    if (0==pgm2) {
        printf("Missing parameter\n");
    }
    if (pipe(fd)) {
        printf("Could not create pipe\n");
        return;
    }
    /*
     * Create job
     */
    job = create_job(line);
    add_job(job);
    /*
     * and add two processes to it. The first process will read
     * from stdin and write to fd[1], so it can close fd[0]
     */
    proc = add_proc(job, STDIN_FILENO, fd[1], fd[0]);
    memset(proc->cmd, 0, CMD_CHARS);
    strncpy(proc->cmd, pgm1, CMD_CHARS);
    proc->cmd[CMD_CHARS-1]=0;
    /*
     * The second process reads from fd[0] and writes to stdout,
     * so it can close fd[1]
     */
    proc = add_proc(job, fd[0], STDOUT_FILENO, fd[1]);
    memset(proc->cmd, 0, CMD_CHARS);
    strncpy(proc->cmd, pgm2, CMD_CHARS);
    proc->cmd[CMD_CHARS-1]=0;
    /*
     * Start job
     */
    start_job(job, 1);
}



/*
 * Put a job into the foreground
 */
void cmd_fg(char* line) {
    int jobid;
    job_t* job;
    /*
     * Parse command line
     */
    char* token = strtok(line, " \n");
    if (token)
         jobid = strtol(token, 0, 10);
     if (0 == jobid) {
         printf("Usage: fg <jobid> where jobid needs to be different from zero\n");
         return;
     }
     /*
      * Locate job matching jobid
      */
     job = get_job(jobid);
     if (0==job) {
         printf("No job found with id %d\n", jobid);
         return;
     }
     /*
      * Now put job in foreground, i.e.
      * 1) block signals
      * 2) set foreground process group of terminal to pgid of job
      * 3) send SIGCONT
      * 4) wait for job to terminate or exit
      * 5) claim terminal again
      * 6) unblock signals
      */
     block_signals();
     if(-1==tcsetpgrp(STDIN_FILENO, job->pgid)) {
         printf("Could not attach job to terminal, error: %s\n", strerror(errno));
     }
     killpg(job->pgid, SIGCONT);
     wait_fg_job(job);
     if(-1==tcsetpgrp(STDIN_FILENO, getpgrp())) {
              printf("Could not reclaim terminal, error: %s\n", strerror(errno));
     }
     unblock_signals();
}

/*
 * Continue a stopped job in the background, i.e. locate the
 * job in the queue and send all processes within the job a SIGCONT signal
 */
void cmd_bg(char* line) {
    int jobid;
    job_t* job;
    /*
     * Parse command line
     */
    char* token = strtok(line, " \n");
    if (token)
         jobid = strtol(token, 0, 10);
     if (0 == jobid) {
         printf("Usage: fg <jobid> where jobid needs to be different from zero\n");
         return;
     }
     /*
      * Locate job matching jobid
      */
     job = get_job(jobid);
     if (0==job) {
         printf("No job found with id %d\n", jobid);
         return;
     }
     /*
      * Continue job
      */
     job->status = JOB_STATUS_RUNNING;
     killpg(job->pgid, SIGCONT);
}

/*
 * Run an executable.
 */
static void run_process(char* line, int foreground) {
    job_t* job;
    process_t* proc;
    /*
     * Block job control signals
     */
    block_signals();
    /*
     * Set up a new job structure and a process structure
     */
    job = create_job(line);
    add_job(job);
    proc = add_proc(job, STDIN_FILENO, STDOUT_FILENO, -1);
    memset(proc->cmd, 0, CMD_CHARS);
    strncpy(proc->cmd, line, CMD_CHARS);
    proc->cmd[CMD_CHARS-1]=0;
    start_job(job, foreground);
    return;
}

/*
 * Run a program in the foreground
 */
void cmd_run(char* line) {
    run_process(line, 1);
}

/*
 * Spawn an executable, i.e. start it without waiting for it
 */
void cmd_spawn(char* line) {
    run_process(line, 0);
}

/*
 * Print a list of jobs
 *
 */
void cmd_jobs(char* line) {
    job_t* job;
    process_t* proc;
    if (0==jobs_head) {
        printf("No jobs\n");
        return;
    }
    block_signals();
    job = jobs_head;
    while(job) {
        printf("[%d]  ", job->jobid);
        switch(job->status) {
            case JOB_STATUS_RUNNING:
                printf("Running ");
                break;
            case JOB_STATUS_STOPPED:
                printf("Stopped ");
                break;
            default:
                printf("Unknown ");
                break;
        }
        proc = job->process_list_head;
        printf("%s ", job->cmd);
        while (proc) {
            printf("<%d> ", proc->pid);
            proc = proc->next;
        }
        printf("\n");
        job = job->next;
    }
    unblock_signals();
}

/*
 * Print contents of a file
 */
void cmd_dump(char* line) {
    int fd;
    char ch;
    printf("Dumping content of file %s\n", line);
    fd = open(line, 0);
    if (fd < 0) {
        printf("Error: could not open file\n");
        return;
    }
    while (read(fd, &ch, 1)) {
        printf("%c", ch);
    }
    printf("\n");
    close(fd);
    return;
}

/*
 * Change working directory
 */
void cmd_cd(char* line) {
    if (chdir(line)) {
        printf("Error, cd failed\n");
    }
    return;
}


/*
 * List the current directory
 */
void cmd_dir(char* line) {
    struct dirent* direntry;
    struct stat mystat;
    int rc;
    DIR* dirp;
    dirp = opendir("./");
    if (0==dirp) {
        printf("Error while opening directory\n");
        return;
    }
    direntry = readdir(dirp);
    while (direntry) {
        rc = stat(direntry->d_name, &mystat);
        if (rc<0) {
            printf("<Could not stat file, skipping directory entry>\n");
        }
        else {
            convert_mode(mystat.st_mode >> 6);
            convert_mode(mystat.st_mode >> 3);
            convert_mode(mystat.st_mode);
            printf("%4o  ", mystat.st_mode & 07777);
            printf("%10d  ", (int) mystat.st_size);
            printf("%s", direntry->d_name);
            if (S_ISDIR(mystat.st_mode))
                printf("/");
            printf("\n");
        }
        direntry = readdir(dirp);
    }
    closedir(dirp);
    return;
}

/*
 * Read from the keyboard and write to a file
 */
void cmd_write(char* line) {
    FILE* stream;
    char* file;
    int rc;
    char c;
    block_signals();
    file = strtok(line, " \n");
    if (0 == file) {
        printf("Not all necessary arguments supplied\n");
        unblock_signals();
        return;
    }
    printf("Please enter data to be written to file %s, hit Ctrl-D when you are done\n", file);
    stream = fopen(file, "w+");
    if (0 == stream) {
        printf("Could not fopen file %s for writing\n", file);
    }
    while ((c = getc(stdin)) != EOF) {
        rc = fputc(c, stream);
        if (rc < 0) {
            printf("Writing failed, errno = %d\n", errno);
            c = EOF;
        }
    }
    fclose(stream);
    unblock_signals();
    return;
}

/*
 * Append to a file
 */
void cmd_append(char* line) {
    FILE* stream;
    char* file;
    int rc;
    char c;
    file = strtok(line, " \n");
    if (0 == file) {
        printf("Not all necessary arguments supplied\n");
        return;
    }
    block_signals();
    printf("Please enter data to be written to file %s, hit Ctrl-D when you are done\n", file);
    stream = fopen(file, "a+");
    if (0 == stream) {
        printf("Could not fopen file %s for writing\n", file);
    }
    while ((c = getc(stdin)) != EOF) {
        rc = fputc(c, stream);
        if (rc < 0) {
            printf("Writing failed, errno = %d\n", errno);
            c = EOF;
        }
    }
    fclose(stream);
    unblock_signals();
    return;
}

/*
 * Create an empty file if it does not exist yet
 */
void cmd_create(char* line) {
    char* file;
    int fd;
    file = strtok(line, " \n");
    printf("Creating file %s\n", file);
    fd = open(file, O_CREAT, S_IRWXU);
    if (fd < 0) {
        printf("Could not create file %s", file);
        return;
    }
    close(fd);
    return;
}

/*
 * Create a directory
 */
void cmd_mkdir(char* line) {
    char* file;
    int rc;
    file = strtok(line, " \n");
    printf("Creating directory %s\n", file);
    rc = mkdir(file, 0777);
    if (rc < 0) {
        printf("Could not create directory %s, errno = %d\n", file, errno);
        return;
    }
    return;
}


/*
 * Remove a file
 */
void cmd_rm(char* line) {
    char* file;
    int rc;
    file = strtok(line, " \n");
    printf("Removing file %s\n", file);
    rc = unlink(file);
    if (rc < 0) {
        printf("Could not remove file %s, error code is %d", file, errno );
    }
    return;
}

/*
 * Remove a directory
 */
void cmd_rmdir(char* line) {
    char* file;
    int rc;
    file = strtok(line, " \n");
    printf("Removing directory %s\n", file);
    rc = rmdir(file);
    if (rc < 0) {
        printf("Could not remove directory %s, error code is %d", file, errno );
    }
    return;
}


/*
 * Send a signal to a process
 */
void cmd_kill(char* line) {
    char* token;
    int rc;
    int pid = 0;
    int sig_no = 0;
    token = strtok(line, " \n");
    if (token)
        pid = strtol(token, 0, 10);
    if (0 == pid) {
        printf(
                "Usage: kill <pid> <sig_no>, where pid needs to be different from zero\n");
        return;
    }
    token = strtok(0, " \n");
    if (token)
        sig_no = strtol(token, 0, 10);
    if (0 == sig_no) {
        printf(
                "Usage: kill <pid< <sig_no>, where pid needs to be different from zero\n");
        return;
    }
    printf("Sending signal %d to process %d\n", sig_no, pid);
    rc = kill(pid, sig_no);
    if (rc < 0) {
        printf("Kill failed with error number %d\n", errno);
    }
    else {
        printf("Kill successful\n");
    }
    return;
}

/*
 * Print current working directory
 */
void cmd_pwd(char* line) {
    char buffer[256];
    char* rc;
    rc = getcwd(buffer, 256);
    if (rc) {
        printf("%s\n", rc);
    }
    else {
        printf("Error: could not determine current working directory, errno is %d\n", errno);
    }
}

/*
 * Print current time and date
 */
void cmd_date(char* line) {
    time_t current_time;
    time(&current_time);
    printf("%s", ctime(&current_time));
}

/****************************************************************************************
 * Networking utilities                                                                 *
 ***************************************************************************************/

/*
 * List all devices
 */
void cmd_net_list(char* line) {
    struct ifconf ifc;
    struct ifreq if_req[32];
    struct ifreq ifq;
    char ip_address_string[INET_ADDRSTRLEN];
    char netmask_string[INET_ADDRSTRLEN];
    unsigned int ip_address;
    unsigned int netmask;

    int i;
    struct ifreq* ifr;
    int fd;
    printf("Name   IP Address        Netmask\n");
    printf("--------------------------------\n");
    /*
     * Open a TCP socket to get access to a file descriptor
     */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return;
    }
    /*
     * Do ioctl
     */
    ifc.ifc_len = sizeof(struct ifreq) * 32;
    ifc.ifc_ifcu.ifcu_req = if_req;
    if (ioctl(fd, SIOCGIFCONF, &ifc) < 0) {
        perror("ioctl");
        close(fd);
        return;
    }
    /*
     * Walk through result list, get additional data and display results
     */
    for (i = 0; i < ifc.ifc_len / sizeof(struct ifreq); i++) {
        ifr = if_req + i;
        ip_address = ((struct sockaddr_in*) &ifr->ifr_ifru.ifru_addr)->sin_addr.s_addr;
        if (0 == inet_ntop(AF_INET, (const void*) &ip_address, ip_address_string, INET_ADDRSTRLEN)) {
            perror("inet_ntop");
            close(fd);
            return;
        }
        /*
         * Get netmask
         */
        strncpy(ifq.ifrn_name, ifr->ifrn_name, 4);
        if (ioctl(fd, SIOCGIFNETMASK, &ifq) < 0) {
            perror("netmask ioctl");
            close(fd);
            return;
        }
        netmask = ((struct sockaddr_in*) &ifq.ifr_ifru.ifru_netmask)->sin_addr.s_addr;
        if (0 == inet_ntop(AF_INET, (const void*) &netmask, netmask_string, INET_ADDRSTRLEN)) {
            perror("inet_ntop for netmask");
            close(fd);
            return;
        }
        printf("%4s   %-16s  %-16s\n", ifr->ifrn_name, ip_address_string, netmask_string);
    }
    /*
     * Close fd
     */
    close(fd);
}

/*
 * Assign IP address to device
 */
void cmd_net_addr(char* line) {
    char* dev;
    char* addr;
    unsigned int ip_addr;
    struct ifreq ifr;
    struct sockaddr_in* in;
    int fd;
    dev = strtok(0, " \n\t");
    if (0 == dev) {
        printf("No device specified, syntax is net addr <dev> <addr>\n");
        return;
    }
    addr = strtok(0, " \n\t");
    if (0 == addr) {
        printf("No adress specified, syntax is net addr <dev> <addr>\n");
        return;
    }
    printf("Assigning IP address %s to device %s\n", addr, dev);
    ip_addr = inet_addr(addr);
    if (-1 == ip_addr) {
        printf("Could not parse IP address %s\n", addr);
        return;
    }
    /*
     * Prepare ifreq structure
     */
    in = (struct sockaddr_in*) &ifr.ifr_ifru.ifru_addr;
    in->sin_addr.s_addr = ip_addr;
    in->sin_family = AF_INET;
    strncpy(ifr.ifrn_name, dev, 4);
    /*
     * Get socket
     */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return;
    }
    /*
     * Do ioctl
     */
    if (ioctl(fd, SIOCSIFADDR, &ifr) < 0) {
        perror("ioctl");
        return;
    }
    close(fd);
}

void cmd_net(char* line) {
    char* subcmd;
    /*
     * Get subcommand
     */
    subcmd = strtok(line, " \n\t");
    if (0 == subcmd) {
        printf("No command specified, type net help for list of available commands\n");
        return;
    }
    if (0 == strcmp(subcmd, "list")) {
        cmd_net_list(line);
    }
    else if (0 == strcmp(subcmd, "addr")) {
        cmd_net_addr(line);
    }
    else {
        printf("Type net help for list of available commands\n");
    }
    return;
}

/*
 * List all DNS servers
 */
void cmd_dns_list(char* line) {
    int fd;
    int i;
    struct ifconf ifc;
    struct ifreq if_req[32];
    unsigned int ip_addr;
    unsigned char addr_str[INET_ADDRSTRLEN];
    /*
     * Open a TCP socket to get access to a file descriptor
     */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return;
    }
    /*
     * Do ioctl
     */
    ifc.ifc_len = sizeof(struct ifreq) * 32;
    ifc.ifc_ifcu.ifcu_req = if_req;
    if (ioctl(fd, SIOCGIFCONF, &ifc) < 0) {
        perror("ioctl");
        close(fd);
        return;
    }
    /*
     * List DNS servers
     */
    printf("DNS Server\n");
    printf("-----------\n");
    for (i = 0; i < MAX_DNS_SERVERS; i++) {
        ip_addr = ifc.ifc_dns_servers[i];
        if (0 == inet_ntop(AF_INET, &ip_addr, (char*) addr_str, INET_ADDRSTRLEN)) {
            perror("inet_ntop");
            return;
        }
        printf("%-16s\n", addr_str);
    }
    /*
     * Close file descriptor
     */
    close(fd);
}

void cmd_dns_add(char* line) {
    char* addr;
    unsigned int ip_addr;
    int fd;
    /*
     * Get argument - destination
     */
    addr = strtok(0, " \n\t");
    if (0 == addr) {
        printf("No server address specified, syntax is dns add <server>\n");
        return;
    }
    ip_addr = inet_addr(addr);
    if (-1 == ip_addr) {
        printf("Could not parse IP address %s\n", addr);
        return;
    }
    /*
     * Open a TCP socket to get access to a file descriptor
     */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return;
    }
    /*
     * Do IOCTL
     */
    if (ioctl(fd, SIOCADDNS, (void*) &ip_addr)) {
        perror("ioctl");
    }
    /*
     * Close socket again
     */
    close(fd);
}

void cmd_dns_del(char* line) {
    char* addr;
    unsigned int ip_addr;
    int fd;
    /*
     * Get argument - destination
     */
    addr = strtok(0, " \n\t");
    if (0 == addr) {
        printf("No server address specified, syntax is dns del <server>\n");
        return;
    }
    ip_addr = inet_addr(addr);
    if (-1 == ip_addr) {
        printf("Could not parse IP address %s\n", addr);
        return;
    }
    /*
     * Open a TCP socket to get access to a file descriptor
     */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return;
    }
    /*
     * Do IOCTL
     */
    if (ioctl(fd, SIOCDELNS, (void*) &ip_addr)) {
        perror("ioctl");
    }
    /*
     * Close socket again
     */
    close(fd);
}


void cmd_dns(char* line) {
    char* subcmd;
    /*
     * Get subcommand
     */
    subcmd = strtok(line, " \n\t");
    if (0 == subcmd) {
        printf("No command specified, type dns help for list of available commands\n");
        return;
    }
    if (0 == strcmp(subcmd, "list")) {
        cmd_dns_list(line);
    }
    else if (0 == strcmp(subcmd, "add")) {
        cmd_dns_add(line);
    }
    else if (0 == strcmp(subcmd, "del")) {
        cmd_dns_del(line);
    }
    else {
        printf("Type dns help for list of available commands\n");
    }
    return;
}

/*
 * List all routes
 */
void cmd_route_list(char* line) {
    struct rtconf rtc;
    struct rtentry rt_entries[32];
    char gateway_string[INET_ADDRSTRLEN];
    char dest_string[INET_ADDRSTRLEN];
    char genmask_string[INET_ADDRSTRLEN];
    unsigned int gateway;
    unsigned int genmask;
    unsigned int dest;
    int i;
    struct rtentry* rt_entry;
    int fd;
    printf("Destination        Netmask           Gateway           Device   Flags\n");
    printf("---------------------------------------------------------------------\n");
    /*
     * Open a TCP socket to get access to a file descriptor
     */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return;
    }
    /*
     * Do ioctl
     */
    rtc.rtc_len = sizeof(struct rtentry) * 32;
    rtc.rtc_rtcu.rtcu_req = rt_entries;
    if (ioctl(fd, SIOCGRTCONF, &rtc) < 0) {
        perror("ioctl");
        close(fd);
        return;
    }
    /*
     * Walk through result list and display results
     */
    for (i = 0; i < rtc.rtc_len / sizeof(struct rtentry); i++) {
        rt_entry = rt_entries + i;
        /*
         * Get destination
         */
        dest = ((struct sockaddr_in*) &rt_entry->rt_dst)->sin_addr.s_addr;
        if (0 == inet_ntop(AF_INET, (const void*) &dest, dest_string, INET_ADDRSTRLEN)) {
            perror("inet_ntop - destination");
            close(fd);
            return;
        }
        gateway = ((struct sockaddr_in*) &rt_entry->rt_gateway)->sin_addr.s_addr;
        if (0 == inet_ntop(AF_INET, (const void*) &gateway, gateway_string, INET_ADDRSTRLEN)) {
            perror("inet_ntop - gateway");
            close(fd);
            return;
        }
        genmask = ((struct sockaddr_in*) &rt_entry->rt_genmask)->sin_addr.s_addr;
        if (0 == inet_ntop(AF_INET, (const void*) &genmask, genmask_string, INET_ADDRSTRLEN)) {
            perror("inet_ntop - genmask");
            close(fd);
            return;
        }
        /*
         * And print
         */
        printf("%-16s   %-16s  %-16s  %4s     ", dest_string, genmask_string, gateway_string, rt_entry->dev);
        if (rt_entry->rt_flags & RT_FLAGS_UP) {
            printf("U");
        }
        if (rt_entry->rt_flags & RT_FLAGS_GW) {
            printf("G");
        }
        printf("\n");
    }
    /*
     * Close fd
     */
    close(fd);
}

/*
 * Add a route to the kernel routing table
 */
void cmd_route_add(char* line) {
    char* dev;
    char* addr;
    unsigned int dest;
    unsigned int genmask;
    unsigned int gateway;
    struct rtentry rt_entry;
    struct sockaddr_in* in;
    int fd;
    /*
     * Get first argument - destination
     */
    addr = strtok(0, " \n\t");
    if (0 == addr) {
        printf("No destination address specified, syntax is route add <dest> <genmask> <gateway> <dev>\n");
        return;
    }
    dest = inet_addr(addr);
    if (-1 == dest) {
        printf("Could not parse IP destination address %s\n", addr);
        return;
    }
    /*
     * Get next argument - genmask
     */
    addr = strtok(0, " \n\t");
    if (0 == addr) {
        printf("No mask specified, syntax is route add <dest> <genmask> <gateway> <dev>\n");
        return;
    }
    genmask = inet_addr(addr);
    if (-1 == dest) {
        printf("Could not parse netmask %s\n", addr);
        return;
    }
    /*
     * Get gateway
     */
    addr = strtok(0, " \n\t");
    if (0 == addr) {
        printf("No gateway specified, syntax is route add <dest> <genmask> <gateway> <dev>\n");
        return;
    }
    gateway = inet_addr(addr);
    if (-1 == gateway) {
        printf("Could not parse gateway %s\n", addr);
        return;
    }
    /*
     * Get device
     */
    dev = strtok(0, " \n\t");
    if (0 == dev) {
        printf("No device specified, syntax is route add <dest> <genmask> <gateway> <dev>\n");
        return;
    }
    /*
     * Prepare rtentry structure. Note that we assume a local route if the gateway is 0
     */
    in = (struct sockaddr_in*) &rt_entry.rt_dst;
    in->sin_addr.s_addr = dest;
    in->sin_family = AF_INET;
    in = (struct sockaddr_in*) &rt_entry.rt_genmask;
    in->sin_addr.s_addr = genmask;
    in->sin_family = AF_INET;
    in = (struct sockaddr_in*) &rt_entry.rt_gateway;
    in->sin_addr.s_addr = gateway;
    in->sin_family = AF_INET;
    strncpy(rt_entry.dev, dev, 4);
    rt_entry.rt_flags = RT_FLAGS_UP;
    if (gateway) {
        rt_entry.rt_flags |= RT_FLAGS_GW;
        printf("Assuming gateway\n");
    }
    else {
        printf("Assuming local route\n");
    }
    /*
     * Get socket
     */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return;
    }
    /*
     * Do ioctl
     */
    if (ioctl(fd, SIOCADDRT, &rt_entry) < 0) {
        perror("ioctl");
        return;
    }
    close(fd);
}

/*
 * Delete a route from the kernel routing table
 */
void cmd_route_del(char* line) {
    char* dev;
    char* addr;
    unsigned int dest;
    unsigned int genmask;
    unsigned int gateway;
    struct rtentry rt_entry;
    struct sockaddr_in* in;
    int fd;
    /*
     * Get first argument - destination
     */
    addr = strtok(0, " \n\t");
    if (0 == addr) {
        printf("No destination address specified, syntax is route del <dest> <genmask> <gateway> <dev>\n");
        return;
    }
    dest = inet_addr(addr);
    if (-1 == dest) {
        printf("Could not parse IP destination address %s\n", addr);
        return;
    }
    /*
     * Get next argument - genmask
     */
    addr = strtok(0, " \n\t");
    if (0 == addr) {
        printf("No mask specified, syntax is route del <dest> <genmask> <gateway> <dev>\n");
        return;
    }
    genmask = inet_addr(addr);
    if (-1 == dest) {
        printf("Could not parse netmask %s\n", addr);
        return;
    }
    /*
     * Get gateway
     */
    addr = strtok(0, " \n\t");
    if (0 == addr) {
        printf("No gateway specified, syntax is route del <dest> <genmask> <gateway> <dev>\n");
        return;
    }
    gateway = inet_addr(addr);
    if (-1 == gateway) {
        printf("Could not parse gateway %s\n", addr);
        return;
    }
    /*
     * Get device
     */
    dev = strtok(0, " \n\t");
    if (0 == dev) {
        printf("No device specified, syntax is route del <dest> <genmask> <gateway> <dev>\n");
        return;
    }
    /*
     * Prepare rtentry structure.
     */
    in = (struct sockaddr_in*) &rt_entry.rt_dst;
    in->sin_addr.s_addr = dest;
    in->sin_family = AF_INET;
    in = (struct sockaddr_in*) &rt_entry.rt_genmask;
    in->sin_addr.s_addr = genmask;
    in->sin_family = AF_INET;
    in = (struct sockaddr_in*) &rt_entry.rt_gateway;
    in->sin_addr.s_addr = gateway;
    in->sin_family = AF_INET;
    strncpy(rt_entry.dev, dev, 4);
    /*
     * Get socket
     */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return;
    }
    /*
     * Do ioctl
     */
    if (ioctl(fd, SIOCDELRT, &rt_entry) < 0) {
        perror("ioctl");
        return;
    }
    close(fd);
}

void cmd_route(char* line) {
    char* subcmd;
    /*
     * Get subcommand
     */
    subcmd = strtok(line, " \n\t");
    if (0 == subcmd) {
        printf("No command specified, type route help for list of available commands\n");
        return;
    }
    if (0 == strcmp(subcmd, "list")) {
        cmd_route_list(line);
    }
    else if (0 == strcmp(subcmd, "add")) {
        cmd_route_add(line);
    }
    else if (0 == strcmp(subcmd, "del")) {
        cmd_route_del(line);
    }
    else {
        printf("Type route help for list of available commands\n");
    }
    return;
}



/****************************************************************************************
 * The following functions are used for the integrated ping utility                     *
 ***************************************************************************************/

/*
 * Compute the IP checksum of a word array. The elements within the
 * array are assumed to be stored in network byte order
 * Parameter:
 * @words - pointer to array
 * @byte_count - number of bytes in array
 */
static unsigned short compute_checksum(unsigned short* words, int byte_count) {
    unsigned int sum = 0;
    unsigned short rc;
    int i;
    unsigned short last_byte = 0;
    /*
     * First sum up all words. We do all the sums in network byte order
     * and only convert the result
     */
    for (i = 0; i < byte_count / 2; i++) {
        sum = sum + words[i];
    }
    /*
     * If the number of bytes is odd, add left over byte << 8
     */
    if (1 == (byte_count % 2)) {
        last_byte = ((unsigned char*) words)[byte_count - 1];
        sum = sum + last_byte;
    }
    /*
     * Repeatedly add carry to LSB until carry is zero
     */
    while (sum >> 16)
        sum = (sum >> 16) + (sum & 0xFFFF);
    rc = sum;
    rc = ntohs(~rc);
    return rc;
}


/*
 * Some constants
 * NR_OF_PINGS - number of echo requests which we send
 * WAIT_TIME - seconds to wait after all requests have been sent
 */
#define NR_OF_PINGS 5
#define WAIT_TIME 2


/*
 * ICMP types
 */
#define ICMP_TYPE_ECHO_REPLY 0
#define ICMP_TYPE_DEST_UNREACHABLE 3
#define ICMP_TYPE_TIME_EXCEEDED 11

/*
 * Counters
 */
volatile static int requests;
volatile static int replies;
volatile static int seconds;

/*
 * Signal handler for alarm
 */
static void handle_alarm(int sig_no) {
    if (SIGALRM == sig_no) {
        seconds++;
    }
}


/*
 * Send an ICMP ECHO request to a remote host
 * Parameter:
 * @fd - the file descriptor of a raw IP socket
 * The function will create an ICMP echo request with
 * ID = PID of current process
 * SEQ_NO = requests + 1
 * and will increase requests
 */
static void send_ping(int fd) {
    unsigned char buffer[256];
    unsigned char* request_data;
    unsigned short* id;
    unsigned short* seq_no;
    unsigned short chksum;
    int pid = getpid();
    int i;
    icmp_hdr_t* request_hdr;
    request_hdr = (icmp_hdr_t*) buffer;
    request_hdr->code = 0;
    request_hdr->type = ICMP_ECHO_REQUEST;
    request_hdr->checksum = 0;
    /*
      * Fill ICMP data area. The first two bytes are an identifier, the next two bytes the sequence number
      * We use 100 bytes for ICMP header and data
      */
    request_data = ((void*) request_hdr) + sizeof(icmp_hdr_t);
    id = (unsigned short*)(request_data);
    seq_no = id + 1;
    *id = htons(pid);
    *seq_no = htons(requests + 1);
    requests++;
    /*
     * Fill up remaining bytes. We have an IP payload of 100 bytes in total
     * ID and SEQ_NO consume four bytes
     */
    for (i = 0; i < 100 - 4 - sizeof(icmp_hdr_t); i++) {
        ((unsigned char*)(request_data + 4))[i] = i;
    }
    /*
     * Compute checksum over entire IP payload
     */
    chksum = compute_checksum((unsigned short*) request_hdr, 100);
    request_hdr->checksum = htons(chksum);
    /*
     * Finally send message
     */
    if (100 != send(fd, buffer, 100, 0)) {
        perror("send");
        return;
    }
}

/*
 * Set up a signal handler for SIGALRM
 */
static void set_signal() {
    sigset_t set;
    struct sigaction sa;
    sigemptyset(&set);
    sigprocmask(SIG_UNBLOCK, &set, 0);
    sigaddset(&set, SIGALRM);
    sa.sa_flags = 0;
    sa.sa_handler = handle_alarm;
    sigemptyset(&set);
    sa.sa_mask = set;
    sigaction(SIGALRM, &sa, 0);
}

/*
 * Open a raw IP socket and connect it
 * Parameter:
 * @dest - IP destination address (in network byte order)
 * Return value:
 * -1 if an error occured
 * file descriptor of socket otherwise
 */
static int open_socket(unsigned int dest) {
    struct sockaddr_in in;
    int fd;
    fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (fd < 0) {
        perror("socket");
        return -1;
    }
    /*
     * and connect it to remote host
     */
    in.sin_family = AF_INET;
    in.sin_addr.s_addr = dest;
    if (connect(fd, (struct sockaddr*) &in, sizeof(struct sockaddr_in))) {
        perror("connect");
        return -1;
    }
    return fd;
}

/*
 * Process ICMP reply message
 */
static void process_reply(unsigned char* in_buffer, int len, unsigned int expected_src) {
    unsigned int src;
    int ip_hdr_length;
    int ip_payload_length;
    icmp_hdr_t* reply_hdr;
    char addr_str[16];
    unsigned char* reply_data;
    char* conv;
    unsigned short* id;
    unsigned short* seq_no;
    int data_ok;
    int i;
    /*
     * Get IP header length and parse ICMP header
     * 1) hdr_type should be 0
     * 2) code should be 0
     */
    ip_hdr_length = (in_buffer[0] & 0xF)*sizeof(unsigned int);
    ip_payload_length = (in_buffer[2] << 8) + in_buffer[3] - ip_hdr_length;
    reply_hdr = (icmp_hdr_t*) (in_buffer + ip_hdr_length);
    /*
     * Check that IP source address (located at offset 12 in IP header)
     * is our target address
     */
    src = *((unsigned int*) (in_buffer + 12));
    if (src != expected_src) {
        return;
    }
    /*
     * Verify checksum
     */
    if (compute_checksum((unsigned short*) reply_hdr, ip_payload_length) != 0)
        return;
    /*
     * Extract ID and SEQ_NO
     */
    reply_data = ((void*) reply_hdr) + sizeof(icmp_hdr_t);
    id = (unsigned short*)(reply_data);
    seq_no = id + 1;
    /*
     * Print message information
     */
    printf("Got ICMP ");
    switch(reply_hdr->type) {
        case ICMP_TYPE_ECHO_REPLY:
            if (*id == ntohs(getpid()))
                printf("echo reply (SEQ_NO = %d) ", ntohs(*seq_no));
            else
                printf("echo reply (ID not matching) ");
            break;
        case ICMP_TYPE_DEST_UNREACHABLE:
            printf("destination unreachable message ");
            break;
        case ICMP_TYPE_TIME_EXCEEDED:
            printf("time exceeded notification ");
            break;
        default:
            printf("unknown type %d ", reply_hdr->type);
            break;
    }
    conv = (char*) inet_ntop(AF_INET, (const void*) &src, addr_str, 16);
    if (conv)
        printf("from host %s\n", conv);
    else
        printf("\n");
    /*
     * Check code
     */
    if ((ICMP_ECHO_REPLY != reply_hdr->type) || (0 != reply_hdr->code))
        return;
    /*
     * Length of entire packet should be 20 + 100
     */
    if (ip_payload_length != 100)
        return;
    /*
     * Check ID and SEQ_NO
     */
    if (*id != htons(getpid()))
        return;
    if ((ntohs(*seq_no) > requests) || (ntohs(*seq_no) < 1))
        return;
    /*
     * Finally check remaining data
     */
    data_ok = 1;
    for (i = 0; i < 100 - 4 - sizeof(icmp_hdr_t); i++) {
        if (((unsigned char*) reply_data + 4)[i] != (i % 256))
            data_ok = 0;
    }
    if (1 == data_ok)
        replies++;
}

/*
 * Ping a remote host
 * To ping a host, we send NR_OF_PINGS ICMP echo requests, one per second, and then wait
 * WAIT_TIME seconds for the replies. Each reply message is printed on the screen.
 */
void cmd_ping(char* line) {
    char* addr;
    unsigned int dest;
    unsigned char in_buffer[256];
    struct hostent* he;
    int fd;
    int rc;
    /*
     * Reset counter
     */
    requests = 0;
    replies = 0;
    seconds = 0;
    /*
     * Set up signal handler for SIGALRM
     */
    set_signal();
    /*
      * Get first argument - destination
      */
     if (0 == (addr = strtok(line, " \n\t"))) {
         printf("No destination address specified, syntax is ping <dest>\n");
         return;
     }
     if (-1 == (dest = inet_addr(addr))) {
         /*
          * addr seems to be a hostname - try to resolv it
          */
         if (0 == (he = gethostbyname(addr))) {
             printf("Could not resolve host name %s\n", addr);
             return;
         }
         dest = *((unsigned int *) he->h_addr_list[0]);
     }
     printf("Pinging %s\n", addr);
     /*
      * Open raw IP socket
      */
     if ((fd = open_socket(dest)) < 0) {
         return;
     }
     /*
      * Now prepare and send first ICMP message
      */
     send_ping(fd);
     /*
      * We now enter a loop, waiting for either replies or signals. When we
      * receive a reply, we print out the reply. When we receive a signal, we
      * reset the alarm and send another ping
      */
     while (1) {
         /*
          * Set alarm and wait for answer
          */
         alarm(1);
         rc = recv(fd, in_buffer, 256, 0);
         /*
          * If we were interrupted, send new packet if needed and set new alarm
          * unless we have reached the timeout limit
          */
         if (-1 == rc) {
             if (EINTR == errno) {
                 if (requests < NR_OF_PINGS) {
                     send_ping(fd);
                 }
                 else if (seconds > WAIT_TIME + NR_OF_PINGS) {
                     printf("Timeout reached\n");
                     break;
                 }
                 /*
                  * Set new alarm
                  */
                 alarm(1);
             }
             else {
                 perror("recv");
                 return;
             }
         }
         else if (rc > 0) {
             /*
              * We got new data, check and print it
              */
             process_reply(in_buffer, 100, dest);
             /*
              * If we are done, cancel alarm and exit
              */
             if (replies >= NR_OF_PINGS) {
                 printf("All replies received\n");
                 alarm(0);
                 break;
             }
         }
         else {
             printf("Got negative return code - %d\n", (-1)*rc);
         }
     }
     /*
      * Close socket again
      */
     close(fd);
     /*
      * and print statistics
      */
     printf("Sent %d packets, got %d replies, %d packets lost\n", requests, replies, requests - replies);
}

/*
 * Resolve a hostname
 */
void cmd_host(char* line) {
    char* addr;
    unsigned int dest;
    struct hostent* he;
    char addr_str[16];
    const char* conv;
    /*
      * Get first argument - hostname
      */
    if (0 == (addr = strtok(line, " \n\t"))) {
         printf("No host name specified, syntax is host <hostname>\n");
         return;
    }
    /*
     * addr seems to be a hostname - try to resolv it
     */
    if (0 == (he = gethostbyname(addr))) {
        printf("Could not resolve host name %s\n", addr);
        return;
    }
    dest = *((unsigned int *) he->h_addr_list[0]);
    conv = inet_ntop(AF_INET, (void*) &dest, addr_str, 16);
    if (conv) {
        printf("Address: %s\n", conv);
        return;
    }
    printf("Hostname could not be resolved\n");
}

/*
 * Get HTTP data from a remote host
 * Parameter:
 * hostname
 * path (optional)
 */
void cmd_http(char* line) {
    in_addr_t ip_address;
    struct in_addr in;
    struct sockaddr_in faddr;    
    struct hostent* he;
    char* host;
    char* path;
    char* url;
    int fd;
    char write_buffer[256];
    char read_buffer[256];
    int b;
    int tries = 0;
    /*
      * Get  argument - url
      */
    if (0 == (url = strtok(line, " \n\t"))) {
         printf("No host name specified, syntax is http <URL> \n");
         return;
    }
    /*
     * Strip off leading http://
     */
    if (0 == strcmp(url, "http://")) {
        url = url + 7;
    }    
    host = strtok(url, "/");
    path = strtok(NULL, "/");
    /*
     * Create socket
     */
    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) {
        printf("Could not create socket");
        return;
    }
    /*
     * Resolve hostname
     */
    if (0 == (he = gethostbyname(host))) {
        printf("Could not resolve host name %s\n", host);
        return;
    }
    ip_address = *((unsigned int *) he->h_addr_list[0]);
    in.s_addr = ip_address;    
     /*
     * Connect socket
     */
    faddr.sin_addr = in;
    faddr.sin_family = AF_INET;
    faddr.sin_port = htons(80);
    if (-1 == connect(fd, (struct sockaddr*) &faddr, sizeof(struct sockaddr_in))) {
        perror("Could not connect socket");
        return;
    }
    printf("Connection established, now sending GET request\n");
    b = sprintf(write_buffer, "GET /%s HTTP/1.1\r\n", path);
    b+= sprintf(write_buffer + b, "Host: %s\r\n", host);
    b+= sprintf(write_buffer + b, "User-Agent: ctOS\r\n");
    b+= sprintf(write_buffer + b, "Accept: */*\r\n");
    /*
     * Do not forget to complete request with an empty line
     */
    sprintf(write_buffer + b, "\r\n");
    printf("%s", write_buffer);    
    b = send(fd, write_buffer, strlen(write_buffer), 0);
    printf("\n\nNow waiting for data to come in\n");
    /*
     *  Put socket into non-blocking mode
     */
    if (fcntl(fd, F_SETFL, O_NONBLOCK)) {
        printf("Warning: could not set socket into non-blocking mode\n");
    }
    while (tries < 5) {
        b = read(fd,read_buffer,256);
        if (b > 0) {
            write(1,read_buffer,b);
        }
        else {
            tries += 1;
            sleep(1);
        }
	}
}


/*
 * Do some tests
 */
void cmd_test(char* line) {
    void* mem = 0;
    unsigned char buffer[16384];
    int i;
    printf("Trying to collect some user space time\n");
    for (i=0;i<1000000000;i++) {
        buffer[i % 16384] = i;
        buffer[i % 16384]++;
    }
    printf("Testing malloc...");
    fflush(stdout);
    mem = malloc(100);
    if (0 == mem) {
        printf("failed\n");
    }
    else {
        printf("ok\n");
    }
    printf("Now freeing memory again...");
    free(mem);
    printf("ok\n");
    printf("Now I will take a little nap and sleep for two seconds...\n");
    sleep(2);
    printf("Done\n");
    printf("Please enter a decimal integer: \n");
    scanf("%d", &i);
    printf("You entered %d\n", i);
    return;
}

int main(int argc, char** argv) {
    char buffer[64];
    char* token;
    cmd_callback_t cmd_callback = 0;
    signal(SIGCHLD, handle_sigchld);
    while (1) {
        cmd_callback = 0;
        /*
         * Block job control signals
         */
        block_signals();
        printf("@>");
        fflush(stdout);
        memset(buffer, 0, 64);
        read(STDIN_FILENO, buffer, 64);
        /*
         * Process any events now
         */
        unblock_signals();
        if (buffer[0]) {
            token = strtok(buffer, " \n");
            if (token) {
                cmd_callback = get_callback(token, cmds);
                if (0 == cmd_callback)
                    printf("Unknown command:%s\n", token);
                else {
                    cmd_callback(strtok(0, "\n"));
                }
            }
        }
    }
    return 0;
}
