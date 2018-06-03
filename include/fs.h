/*
 * fs.h
 */

#ifndef _FS_H_
#define _FS_H_

#include "lib/sys/types.h"
#include "lib/unistd.h"
#include "lib/os/stat.h"
#include "lib/termios.h"
#include "locks.h"
#include "pm.h"
#include "fs_pipe.h"
#include "net.h"
#include "lib/netinet/in.h"
#include "lib/utime.h"

/*
 * Maximum numbers of characters
 * for a file name including trailing 0
 */
#define FILE_NAME_MAX 256


typedef struct {
    ino_t inode_nr;
    char name[FILE_NAME_MAX];
} direntry_t;

/*
 * Forward declarations
 */
struct _inode_t;
struct _superblock_t;

/*
 * This structure describes the part of the public interface of a file
 * system implementation which operates on an individual inode
 */
typedef struct {
    ssize_t (*inode_read)(struct _inode_t* inode, ssize_t bytes, off_t offset, void* data);
    ssize_t (*inode_write)(struct _inode_t* inode, ssize_t bytes, off_t offset, void* data);
    int (*inode_trunc)(struct _inode_t* inode, u32 new_size);
    int (*inode_get_direntry)(struct _inode_t* inode, off_t index, direntry_t* direntry);
    struct _inode_t* (*inode_create)(struct _inode_t* parent, char* name, int mode);
    int (*inode_unlink)(struct _inode_t* parent, char* name, int flags);
    struct _inode_t* (*inode_clone)(struct _inode_t* inode);
    void (*inode_release)(struct _inode_t* inode);
    int (*inode_flush)(struct _inode_t* inode);
    int (*inode_link)(struct _inode_t* dir, char* name, struct _inode_t* inode);
} inode_ops_t;

/*
 * Flags for the unlink operation
 * FORCE - force removal of a directory even if there are additional hard links
 * NOTRUNC - do not truncate a directory which is unlinked
 */
#define FS_UNLINK_FORCE 0x1
#define FS_UNLINK_NOTRUNC 0x2

/*
 * An inode
 */
typedef struct _inode_t {
    off_t size;                   // Size of the underlying file
    ino_t inode_nr;               // Number of the inode
    dev_t dev;                    // Device on which the inode lives
    mode_t mode;                  // File mode
    uid_t owner;                  // owner of the file
    int link_count;               // link count
    gid_t group;                  // group of the file
    dev_t s_dev;                  // if the inode represents a special file, this is the device
    time_t mtime;                 // modification time
    time_t atime;                 // access time
    void* data;                   // opaque pointer to link low-level data into inode
    inode_ops_t* iops;            // inode operations
    int mount_point;              // set to 1 if there is another file system mounted here
    rw_lock_t rw_lock;            // lock to protect inode
    struct _superblock_t* super;  // the superblock
} inode_t;

/*
 * A superblock
 */
typedef struct _superblock_t{
    dev_t device;                 // Device on which file system is mounted
    ino_t root;                   // Number of root inode of the file system
    void* data;                   // for free use by file system implementation
    inode_t* (*get_inode)(dev_t, ino_t);
    void (*release_superblock)(struct _superblock_t* superblock);
    int (*is_busy)(struct _superblock_t* superblock);
} superblock_t;

/*
 * This structure describes a mount point
 */
typedef struct _mount_point_t {
    dev_t device;                 // Device which is mounted
    inode_t* mounted_on;          // where are we mounted on
    inode_t* root;                // root inode of the mounted file system
    struct _mount_point_t* next;  // next mount point
    struct _mount_point_t* prev;  // previous mount point
} mount_point_t;

/*
 * This is the initial interface for a file system. The generic
 * file system uses this to get the superblock
 */
typedef struct {
    int (*probe)(dev_t);
    superblock_t* (*get_superblock)(dev_t);
    int (*init)();
    char* fs_name;
} fs_implementation_t;

/*
 * An open file
 */
typedef struct _open_file_t {
    off_t cursor;           // the current position within the file
    inode_t* inode;         // the inode of the file
    pipe_t* pipe;           // used to connect an open file to a pipe
    socket_t* socket;       // a socket associated with the file
    int ref_count;          // reference count
    semaphore_t sem;        // Semaphore to protect access to inner state of file
    spinlock_t lock;        // spinlock to protect reference count
    u32 flags;              // flags which have been used to open the file
    struct _open_file_t* next;
    struct _open_file_t* prev;
} open_file_t;

/*
 * Maximum number of file descriptor per process
 */
#define FS_MAX_FD 128

/*
 * This data structure contains all data items which represent
 * a process within the file system
 */
typedef struct _fs_process_t {
    inode_t* cwd;                        // Current working directory (NULL = /)
    open_file_t* fd_tables[FS_MAX_FD];   // File descriptors
    u32 fd_flags[FS_MAX_FD];             // file descriptor flags
    mode_t umask;                        // umask
    spinlock_t fd_table_lock;            // Lock to protect file descriptor table
    spinlock_t spinlock;                 // lock to protect all fields of the structure except the fd table
} fs_process_t;


/*
 * Compare two inodes
 */
#define INODE_EQUAL(x,y)  ((((x)->inode_nr==(y)->inode_nr) && ((x)->dev==(y)->dev)))

/*
 * Some constants
 */
#define FS_READ 0
#define FS_WRITE 1

/*
 * The public interface of the file system is split in two parts. The first part consists
 * of functions which operate directly on the level of inodes.
 */
int fs_init(dev_t root);
inode_t* fs_get_inode_for_name(char* path, inode_t* inode_at);
int fs_mount(inode_t* mount_point, dev_t device, fs_implementation_t* fs);
int fs_unmount(inode_t* mounted_on);
ssize_t fs_read(open_file_t* file, size_t bytes, void* buffer);
ssize_t fs_write(open_file_t* file, size_t bytes, void* buffer);
ssize_t fs_lseek(open_file_t* file, off_t offset, int whence);
ssize_t fs_readdir(open_file_t* file, direntry_t* direntry);
open_file_t* fs_open(inode_t* inode, int flags);
int fs_close(open_file_t* file);
void fs_close_all();
void fs_on_exec(int);
void fs_clone(u32 source_pid, u32 target_pid);
int fs_print_open_files();
int fs_get_dirname(inode_t* inode, char* buffer, size_t n);
ssize_t fs_ftruncate(open_file_t* file, off_t size);

/*
 * The public interface below this line corresponds to system calls
 * and does not know anything about inodes
 */
int do_mount(char* path, dev_t dev, char* fs_name);
int do_unmount(char* path);
int do_open(char* path, int flags, int mode);
int do_openat(char* path, int flags, int mode, int at);
int do_close(int fd);
ssize_t do_read(int fd, void* buffer, size_t bytes);
ssize_t do_write(int fd, void* buffer, size_t bytes);
ssize_t do_readdir(int fd, direntry_t* direntry);
ssize_t do_lseek(int fd, off_t offset, int whence);
int do_utime(char* file, struct utimbuf* times);
int do_unlink(char* path);
int do_link(char* path1, char* path2);
int do_chdir(char* path);
int do_fchdir(int fd);
int do_mkdir(char* path, int mode);
int do_fcntl(int fd, int cmd, int arg);
int do_stat(char* path, struct __ctOS_stat*  buffer);
int do_chmod(char* path, mode_t mode);
int do_fstat(int fd, struct __ctOS_stat*  buffer);
int do_rename(char* old, char* new);
int do_dup(int, int);
int do_isatty(int fd);
mode_t do_umask(mode_t umask);
int do_pipe(int fd[], int);
int fs_sgpgrp(int fd, u32* pgrp, int mode);
int do_tcgetattr(int fd, struct termios* termios_p);
int do_tcsetattr(int fd, int action, struct termios* termios_p);
int do_getcwd(char* buffer, size_t n);
int do_socket(int domain, int type, int proto);
int do_connect(int fd, struct sockaddr * sockaddr, int addrlen);
ssize_t do_send(int fd, void* buffer, size_t len, int flags);
ssize_t do_sendto(int fd, void* buffer, size_t len, int flags, struct sockaddr* addr, int addrlen);
ssize_t do_recv(int fd, void* buffer, size_t len, int flags);
ssize_t do_recvfrom(int fd, void* buffer, size_t len, int flags, struct sockaddr* addr, u32* addrlen);
int do_listen(int fd, int backlog);
int do_bind(int fd, struct sockaddr* address, int addrlen);
int do_accept(int fd, struct sockaddr* addr, socklen_t* len);
int do_select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* errorfds, struct timeval* timeout);
int do_ioctl(int fd, unsigned int cmd, void* arg);
int do_setsockopt(int fd, int level, int option, void* option_value, unsigned int option_len);
int do_getsockaddr(int fd, struct sockaddr* laddr, struct sockaddr* faddr, socklen_t* addrlen);
int do_ftruncate(int fd, off_t size);

#endif /* _FS_H_ */
