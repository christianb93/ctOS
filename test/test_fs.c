/*
 * test_fs.c
 *
 * Here we simulate the following file system layout
 * On device (0,0), there is a FAT16 file system (yeah, I know, this is not a good example...)
 * Inode 1 on this file system is the directory /tmp
 * Inode 2 on the file system is a file called /hello
 * Inode 3 on the file system is a file called /tmp/hidden
 * Inode 4 on the file system is a directory called /usr
 * Inode 5 on the file system is a directory called /dev
 * Inode 6 on the file system is a special character device called /dev/tty
 * Inode 7 on the file system is a directory called /usr/local
 *
 * In addition, there is an ext2 file system with the following inodes on it
 * Inode 0 (the root inode)
 * Inode 1 (a file called test)
 * Inode 2 (a directory called dir)
 */

#include "kunit.h"
#include "fs.h"
#include "vga.h"
#include "locks.h"
#include "pm.h"
#include "drivers.h"
#include <stdio.h>
#include <sys/stat.h>
#include <stdint.h>
#include "sys/select.h"
#include "lib/time.h"



#define F_GETFD 1
#define F_SETFD 2
#define F_GETFL 3
#define F_SETFL 4
#define F_DUPFD 5

#define O_RDONLY 0x0
#define O_WRONLY 0x1
#define O_RDWR 0x2

#define O_ACCMODE 0x3

#define O_CREAT 0x40
#define O_EXCL 0x80
#define O_TRUNC 0x200
#define O_APPEND 0x400
#define O_NONBLOCK 0x800

extern int __fs_loglevel;
int __ext2_loglevel;

/*
 * This needs to match the value defined in timer.h
 */
#define HZ 100


/*
 * Stub for do_time
 */
time_t do_time(struct tm* t) {
    return time(0);
}
/*
 * Given a timeval structure, convert its value into ticks or return the maximum in case
 * of an overflow
 */
unsigned int timer_convert_timeval(struct timeval* time) {
    unsigned int ticks;
    unsigned int ticks_usec;
    /*
      * First compute contribution of tv_sev field
      */
     if (time->tv_sec > (UINT_MAX / HZ)) {
         ticks = UINT_MAX;
     }
     else {
         ticks = time->tv_sec * HZ;
     }
     /*
      * then add contribution of tv_usec field
      */
     ticks_usec = time->tv_usec / (1000000 / HZ);
     if (ticks_usec >  ~ticks) {
         ticks = UINT_MAX / HZ;
     }
     else {
         ticks += ticks_usec;
     }
     return ticks;
}

u32 atomic_load(u32* ptr) {
    return *ptr;
}

void atomic_store(u32* ptr, u32 value) {
    *ptr = value;
}


void spinlock_get(spinlock_t* lock, u32* flags) {
}
void spinlock_release(spinlock_t* lock, u32* flags) {
}
void spinlock_init(spinlock_t* lock) {
}
void sem_init(semaphore_t* sem, u32 value) {

}
void sem_up(semaphore_t* sem) {

}
void __sem_down(semaphore_t* sem, char* file, int line) {

}

int __sem_down_intr(semaphore_t* sem, char* file, int line) {
    return 0;
}

static int sem_down_timed_called = 0;
static unsigned int last_timeout;
int __sem_down_timed(semaphore_t* sem, char* file, int line, u32 timeout) {
    sem_down_timed_called++;
    last_timeout = timeout;
    return 0;
}

void __rw_lock_get_read_lock(rw_lock_t* rw_lock, char* file, int line) {

}

void rw_lock_release_read_lock(rw_lock_t* rw_lock) {

}

void __rw_lock_get_write_lock(rw_lock_t* rw_lock, char* file, int line) {

}

void rw_lock_release_write_lock(rw_lock_t* rw_lock) {

}

int tty_getpgrp(minor_dev_t minor) {
    return 1;
}

int tty_setpgrp(minor_dev_t minor, pid_t pgrp) {
    return 0;
}

void pm_attach_tty(dev_t tty) {

}

dev_t pm_get_cterm() {
    return 0;
}

int net_socket_connect(socket_t* socket, struct sockaddr* addr, int addrlen) {
    return 0;
};

int net_socket_bind(socket_t* socket, struct sockaddr* address, int addrlen) {
    return 0;
}

int net_socket_accept(socket_t* socket, struct sockaddr* addr, socklen_t* addrlen, socket_t** new_socket) {
    return 0;
}

int net_ioctl(socket_t* socket, unsigned int cmd, void* arg) {
    return 0;
}

void net_socket_close(socket_t* socket) {

}

int net_socket_getaddr(socket_t* socket, struct sockaddr* laddr, struct sockaddr* faddr, unsigned int* addrlen) {
    return 0;
}

int net_socket_cancel_select(socket_t* socket, semaphore_t* sem) {
    return 0;
}

int net_socket_select(socket_t* socket, int read, int write, semaphore_t* sem) {
    return 0;
}

int net_socket_setoption(socket_t* socket, int level, int option, void* option_value, unsigned int option_len) {
    return 0;
}

static int pid = 0;
int pm_get_pid() {
    return pid;
}

int pm_get_task_id() {
    return 0;
}

int do_pthread_kill(u32 task_id, int sig_no) {
    return 0;
}

uid_t do_geteuid() {
    return 0;
}

gid_t do_getegid() {
    return 0;
}

void cond_init(cond_t* cond) {

}

static int cond_broadcast_called = 0;
void cond_broadcast(cond_t* cond) {
    cond_broadcast_called = 1;
}

int pm_pgrp_in_session(int pid, int pgrp) {
    return 1;
}

int tty_tcgetattr(minor_dev_t minor, struct termios* termios_p) {
    return 0;
}

int tty_tcsetattr(minor_dev_t minor, struct termios* termios_p) {
    return 0;
}

/*
 * Dummy for cond_wait_intr. As we cannot really wait in a single-threaded
 * unit test, we always return -1 here, i.e. we simulate the case that we
 * were interrupted
 */
int cond_wait_intr(cond_t* cond, spinlock_t* lock, u32* eflags) {
    spinlock_release(lock, eflags);
    return -1;
}

void rw_lock_init(rw_lock_t* rw_lock) {

}

socket_t* net_socket_create(int domain, int type, int proto) {
    return (socket_t*) malloc(sizeof(socket_t));
}

ssize_t net_socket_send(socket_t* socket, void* buffer, size_t len, int flags, struct sockaddr* addr, u32 addrlen, int sendto) {
    return 0;
}

ssize_t net_socket_recv(socket_t* socket, void* buffer, size_t len, int flags, struct sockaddr* addr, u32* addrlen, int recvfrom) {
    return 0;
}

int net_socket_listen(socket_t* socket, int backlog) {
    return 0;
}

/*
 * Reference counts for fat16 inodes
 */
static int ref_count[10];

/*
 * fat16 superblock
 */
static superblock_t fat16_superblock;

/*
 * Ext2 superblocks
 */
static superblock_t ext2_superblock;
static superblock_t ext2_second_superblock;

/*
 * fat16 root inode
 */
static inode_t fat16_root_inode;

/*
 * Stub for inode_get_direntry
 */
int fat16_inode_get_direntry(inode_t* inode, off_t index, direntry_t* direntry) {
    /*
     * Simulated root directory
     */
    if (0 == inode->inode_nr) {
        if ((0 == index) || (1 == index)) {
            direntry->inode_nr = 0;
            if (0 == index)
                strcpy(direntry->name, ".");
            else
                strcpy(direntry->name, "..");
            return 0;
        }
        if (2 == index) {
            direntry->inode_nr = 1;
            strcpy(direntry->name, "tmp");
            return 0;
        }
        if (3 == index) {
            direntry->inode_nr = 2;
            strcpy(direntry->name, "hello");
            return 0;
        }
        if (4 == index) {
            direntry->inode_nr = 5;
            strcpy(direntry->name, "dev");
            return 0;
        }
        if (5 == index) {
            direntry->inode_nr = 4;
            strcpy(direntry->name, "usr");
            return 0;
        }
        return -1;
    }
    if (1 == inode->inode_nr) {
        if (0 == index)  {
            direntry->inode_nr = 1;
            strcpy(direntry->name, ".");
            return 0;
        }
        if (1 == index)  {
            direntry->inode_nr = 0;
            strcpy(direntry->name, "..");
            return 0;
        }
        if (2 == index) {
            direntry->inode_nr = 3;
            strcpy(direntry->name, "hidden");
            return 0;
        }
    }
    if (4 == inode->inode_nr) {
        if (0 == index)  {
            direntry->inode_nr = 4;
            strcpy(direntry->name, ".");
            return 0;
        }
        if (1 == index)  {
            direntry->inode_nr = 0;
            strcpy(direntry->name, "..");
            return 0;
        }
        if (2 == index)  {
            direntry->inode_nr = 7;
            strcpy(direntry->name, "local");
            return 0;
        }
    }
    if (5 == inode->inode_nr) {
        if (0 == index)  {
            direntry->inode_nr = 5;
            strcpy(direntry->name, ".");
            return 0;
        }
        if (1 == index)  {
            direntry->inode_nr = 0;
            strcpy(direntry->name, "..");
            return 0;
        }
        if (2 == index) {
            strcpy(direntry->name, "tty");
            direntry->inode_nr = 6;
            return 0;
        }
    }
    if (7 == inode->inode_nr) {
        if (0 == index)  {
            direntry->inode_nr = 7;
            strcpy(direntry->name, ".");
            return 0;
        }
        if (1 == index)  {
            direntry->inode_nr = 4;
            strcpy(direntry->name, "..");
            return 0;
        }
    }
    return -1;
}

/*
 * Inode for file hello
 */
static inode_t fat16_hello_inode;

/*
 * Inode for directory tmp
 */
static inode_t fat16_tmp_inode;

/*
 * Inode for /tmp/hidden
 */
static inode_t fat16_hidden_inode;

/*
 * Inode for /usr
 */
static inode_t fat16_usr_inode;

/*
 * Inode for /dev
 */
static inode_t fat16_dev_inode;

/*
 * Inode for /dev/tty
 */

static inode_t fat16_dev_tty_inode;

/*
 * Inode for /usr/local
 */
static inode_t fat16_usr_local_inode;


/*
 * Inode for new file
 */
static inode_t new_file_inode;

/*
 * Stub for putchar
 */
static int do_putchar = 0;
void win_putchar(win_t* win, u8 c) {
    if (do_putchar)
        printf("%c", c);
}

/*
 * Stub for trap
 */
void trap() {

}

/*
 * Stubs for device manager - used for character devices
 */
ssize_t tty_read(minor_dev_t minor, ssize_t size, void* buffer, unsigned int flags) {
    int i;
    for (i = 0; i < size; i++)
        ((char*) buffer)[i] = 'x';
    return size;
}

static char tty_buffer[5];
ssize_t tty_write(minor_dev_t minor, ssize_t size, void* buffer) {
    int i;
    if (size > 5)
        size = 5;
    for (i = 0; i < size; i++)
        ((char*) tty_buffer)[i] = ((char*) buffer)[i];
    return size;
}

static int tty_open(minor_dev_t minor) {
    return 0;
}

static int tty_close(minor_dev_t minor) {
    return 0;
}

static char_dev_ops_t tty_ops = { tty_open, tty_close, tty_read, tty_write, 0 };
char_dev_ops_t* dm_get_char_dev_ops(major_dev_t major) {
    if (major == 2) {
        return &tty_ops;
    }
    return 0;
}

/*
 * Stub for kmalloc/kfree
 */
u32 kmalloc(size_t size) {
    return malloc(size);
}
void kfree(u32 addr) {
    free((void*) addr);
}

inode_t* inode_clone(inode_t* inode) {
    if (inode->dev == 0)
        ref_count[inode->inode_nr]++;
    return inode;
}

void inode_release(inode_t* inode) {
    if (inode->dev == 0)
        ref_count[inode->inode_nr]--;

}

/*
 * Simulated fat16 FS
 */
ssize_t fat16_read(inode_t* inode, ssize_t size, off_t offset, void* data) {
    int bytes = 0;
    if (inode->dev != 0)
        return 0;
    if (inode->inode_nr == 2) {
        if (size == 0)
            return 0;
        if (offset > 4)
            return 0;
        bytes = 5 - offset;
        if (size < bytes)
            bytes = size;
        strncpy((char*) data, "hello" + offset, bytes);
        return bytes;
    }
    return 0;
}

ssize_t fat16_write(inode_t* inode, ssize_t size, off_t offset, void* data) {
    return size;
}



static inode_t* fat16_create_inode() {
    return &new_file_inode;
}

static int unlink_inode(inode_t* dir, char* name, int flags) {
    return 0;
}

struct _inode_t* dummy_inode_create(struct _inode_t* parent, char* name, int mode) {
    return 0;
}

int __fat16_trunc_called = 0;
int fat_16_trunc(inode_t* inode, u32 size) {
    __fat16_trunc_called = 1;
    return 0;
}

/*
 * fat16 iops
 */
static inode_ops_t fat16_iops = {
        fat16_read,
        fat16_write,fat_16_trunc,
        fat16_inode_get_direntry, fat16_create_inode, unlink_inode,
        inode_clone,
        inode_release};

static int fat16_probe_result = 0;
int fs_fat16_probe(dev_t device) {
    return fat16_probe_result;
}

static superblock_t* fs_fat16_result = &fat16_superblock;
superblock_t* fs_fat16_get_superblock(dev_t device) {
    if (0 == device)
        return fs_fat16_result;
    return 0;
}

int fs_fat16_init() {
    return 0;
}

void fs_release_superblock(superblock_t* super) {

}

static int fat16_busy = 1;
static int ext2_busy = 1;
int fs_is_busy(superblock_t* super) {
    if (super->device == 0)
        return fat16_busy;
    return ext2_busy;
}

inode_t* fat16_get_inode(dev_t device, ino_t nr) {
    inode_t* rc = 0;
    if ((0 == nr) && (0 == device))
        rc = &fat16_root_inode;
    if ((2 == nr) && (0 == device)) {
        rc = &fat16_hello_inode;
    }
    if ((1 == nr) && (0 == device)) {
        rc = &fat16_tmp_inode;
    }
    if ((3 == nr) && (0 == device)) {
        rc = &fat16_hidden_inode;
    }
    if ((4 == nr) && (0 == device)) {
        rc = &fat16_usr_inode;
    }
    if ((5 == nr) && (0 == device)) {
        rc = &fat16_dev_inode;
    }
    if ((6 == nr) && (0 == device)) {
        rc = &fat16_dev_tty_inode;
    }
    if ((7 == nr) && (0 == device)) {
        rc = &fat16_usr_local_inode;
    }
    if (rc) {
        if (rc->dev == 0)
            ref_count[rc->inode_nr]++;
    }
    return rc;
}

/*
 * Simulated ext2 file system
 */

void fs_ext2_print_cache_info() {

}

static int ext2_probe_result = 0;
int fs_ext2_probe(dev_t device) {
    return ext2_probe_result;
}

static superblock_t* fs_ext2_result = &ext2_superblock;
superblock_t* fs_ext2_get_superblock(dev_t device) {
    if (device == 1)
        return fs_ext2_result;
    if (device == 2)
        return &ext2_second_superblock;
    return 0;
}

static inode_t ext2_root_inode;
static inode_t ext2_test_inode;
static inode_t ext2_dir_inode;


int ext2_inode_get_direntry(inode_t* inode, off_t index, direntry_t* direntry) {
    /*
     * Simulated root directory
     */
    if (0 == inode->inode_nr) {
        if ((0 == index) || (1 == index)) {
            direntry->inode_nr = 0;
            if (0 == index)
                strcpy(direntry->name, ".");
            else
                strcpy(direntry->name, "..");
            return 0;
        }
        if (2 == index) {
            direntry->inode_nr = 1;
            strcpy(direntry->name, "test");
            return 0;
        }
        if (3 == index) {
            direntry->inode_nr = 2;
            strcpy(direntry->name, "dir");
            return 0;
        }
        return -1;
    }
    /*
     * Simulated directory dir
     */
    if (2 == inode->inode_nr) {
        if (0 == index) {
            direntry->inode_nr = 2;
            strcpy(direntry->name, ".");
            return 0;
        }
        if (1 == index) {
            direntry->inode_nr = 0;
            strcpy(direntry->name, "..");
            return 0;
        }
    }
    return -1;
}

static inode_ops_t ext2_iops = {
        fat16_write,
        fat16_write,0,
        ext2_inode_get_direntry, dummy_inode_create, unlink_inode,
        inode_clone,
        inode_release};

inode_t* ext2_get_inode(dev_t device, ino_t nr) {
    if ((0 == nr) && (1 == device))
        return &ext2_root_inode;
    if ((1 == nr) && (1 == device))
        return &ext2_test_inode;
    if ((2 == nr) && (1 == device))
        return &ext2_dir_inode;
    return 0;
}

static fs_implementation_t ext2_impl = {
        fs_ext2_probe,
        fs_ext2_get_superblock,
        0,
        "ext2" };

int fs_ext2_init() {
    return 0;
}

int bc_open(dev_t dev) {
    return 0;
}

/*
 * Common setup function
 */
void setup() {
    int i;
    for (i = 0; i < 10; i++)
        ref_count[i] = 0;
    fat16_superblock.device = 0;
    fat16_superblock.get_inode = fat16_get_inode;
    fat16_superblock.root = 0;
    fat16_superblock.release_superblock = fs_release_superblock;
    fat16_superblock.is_busy = fs_is_busy;
    ext2_superblock.device = 1;
    ext2_superblock.get_inode = ext2_get_inode;
    ext2_superblock.root = 0;
    ext2_superblock.release_superblock = fs_release_superblock;
    ext2_superblock.is_busy = fs_is_busy;
    /*
     * Set up inode for /
     */
    fat16_root_inode.dev = 0;
    fat16_root_inode.inode_nr = 0;
    fat16_root_inode.iops = &fat16_iops;
    fat16_root_inode.mode = S_IFDIR;
    fat16_root_inode.mount_point = 0;
    fat16_root_inode.super = &fat16_superblock;
    /*
     * Set up inode for /hello
     */
    fat16_hello_inode.dev = 0;
    fat16_hello_inode.inode_nr = 2;
    fat16_hello_inode.iops = &fat16_iops;
    fat16_hello_inode.mode = S_IFREG;
    fat16_hello_inode.mount_point = 0;
    fat16_hello_inode.super = &fat16_superblock;
    fat16_hello_inode.size = 1024;
    /*
     * Set up inode for /tmp/hidden
     */
    fat16_hidden_inode.dev = 0;
    fat16_hidden_inode.inode_nr = 3;
    fat16_hidden_inode.iops = &fat16_iops;
    fat16_hidden_inode.mode = S_IFREG;
    fat16_hidden_inode.mount_point = 0;
    fat16_hidden_inode.super = &fat16_superblock;
    /*
     * Set up inode for /tmp
     */
    fat16_tmp_inode.dev = 0;
    fat16_tmp_inode.inode_nr = 1;
    fat16_tmp_inode.iops = &fat16_iops;
    fat16_tmp_inode.mode = S_IFDIR;
    fat16_tmp_inode.mount_point = 0;
    fat16_tmp_inode.super = &fat16_superblock;
    /*
     * Set up inode for /usr
     */
    fat16_usr_inode.dev = 0;
    fat16_usr_inode.inode_nr = 4;
    fat16_usr_inode.iops = &fat16_iops;
    fat16_usr_inode.mode = S_IFDIR;
    fat16_usr_inode.mount_point = 0;
    fat16_usr_inode.super = &fat16_superblock;
    /*
     * Set up inode for /usr/local
     */
    fat16_usr_local_inode.dev = 0;
    fat16_usr_local_inode.inode_nr = 7;
    fat16_usr_local_inode.iops = &fat16_iops;
    fat16_usr_local_inode.mode = S_IFDIR;
    fat16_usr_local_inode.mount_point = 0;
    fat16_usr_local_inode.super = &fat16_superblock;
    /*
     * Set up inode for /dev
     */
    fat16_dev_inode.dev = 0;
    fat16_dev_inode.inode_nr = 5;
    fat16_dev_inode.iops = &fat16_iops;
    fat16_dev_inode.mode = S_IFDIR;
    fat16_dev_inode.mount_point = 0;
    fat16_dev_inode.super = &fat16_superblock;
    /*
     * Set up inode for /dev/tty
     */
    fat16_dev_tty_inode.dev = 0;
    fat16_dev_tty_inode.inode_nr = 6;
    fat16_dev_tty_inode.iops = &fat16_iops;
    fat16_dev_tty_inode.mode = S_IFCHR;
    fat16_dev_tty_inode.mount_point = 0;
    fat16_dev_tty_inode.s_dev = DEVICE(2,0);
    fat16_dev_tty_inode.super = &fat16_superblock;
    /*
     * Set up inode for tests of open (.., O_CREAT)
     */
    new_file_inode.dev = 0;
    new_file_inode.inode_nr = 999;
    new_file_inode.iops = &fat16_iops;
    new_file_inode.mode = S_IFREG;
    new_file_inode.mount_point = 0;
    new_file_inode.super = &fat16_superblock;
    /*
     * Set up inode for ext2 root
     */
    ext2_root_inode.dev = 1;
    ext2_root_inode.inode_nr = 0;
    ext2_root_inode.iops = &ext2_iops;
    ext2_root_inode.mode = S_IFDIR;
    ext2_root_inode.mount_point = 0;
    ext2_root_inode.super = &ext2_superblock;
    ext2_root_inode.size = 1024;
    /*
     * Set up inode for /test on ext2 fs
     */
    ext2_test_inode.dev = 1;
    ext2_test_inode.inode_nr = 1;
    ext2_test_inode.iops = &ext2_iops;
    ext2_test_inode.mode = S_IFREG;
    ext2_test_inode.mount_point = 0;
    ext2_test_inode.super = &ext2_superblock;
    /*
     * Set up inode for ext2  /dir
     */
    ext2_dir_inode.dev = 1;
    ext2_dir_inode.inode_nr = 2;
    ext2_dir_inode.iops = &ext2_iops;
    ext2_dir_inode.mode = S_IFDIR;
    ext2_dir_inode.mount_point = 0;
    ext2_dir_inode.super = &ext2_superblock;
}

/*
 * Testcase 1
 * Tested function: fs_init
 * Testcase: verify that if fs_init is called and
 * no file system accepts the device, an error is returned
 */
int testcase1() {
    setup();
    ASSERT(fs_init(0));
    return 0;
}

/*
 * Testcase 2
 * Tested function: fs_init
 * Testcase: verify that if fs_init is called and the creation
 * of the superblock fails, an error is returned
 */
int testcase2() {
    fat16_probe_result = 1;
    fs_fat16_result = 0;
    setup();
    ASSERT(fs_init(0));
    return 0;
}

/*
 * Testcase 3:
 * Tested function: fs_init
 * Testcase: successful execution
 */
int testcase3() {
    fat16_probe_result = 1;
    fs_fat16_result = &fat16_superblock;
    setup();
    ASSERT(0==fs_init(0));
    return 0;
}

/*
 * Testcase 4
 * Tested function: fs_get_inode_for_name
 * Testcase: get directory root
 */
int testcase4() {
    inode_t* inode;
    fat16_probe_result = 1;
    fs_fat16_result = &fat16_superblock;
    setup();
    ASSERT(0==fs_init(0));
    inode = fs_get_inode_for_name("/", 0);
    ASSERT(inode);
    ASSERT(inode->inode_nr==0);
    ASSERT(inode->dev==0);
    return 0;
}

/*
 * Testcase 5
 * Tested function: fs_get_inode_for_name
 * Testcase: get inode for non-existing path
 */
int testcase5() {
    inode_t* inode;
    fat16_probe_result = 1;
    fs_fat16_result = &fat16_superblock;
    setup();
    ASSERT(0==fs_init(0));
    inode = fs_get_inode_for_name("/not_there", 0);
    ASSERT(0==inode);
    return 0;
}

/*
 * Testcase 6
 * Tested function: fs_get_inode_for_name
 * Testcase: get inode for /hello
 */
int testcase6() {
    inode_t* inode;
    fat16_probe_result = 1;
    fs_fat16_result = &fat16_superblock;
    setup();
    ASSERT(0==fs_init(0));
    inode = fs_get_inode_for_name("/hello", 0);
    ASSERT(inode);
    ASSERT(inode->inode_nr==2);
    ASSERT(inode->dev==0);
    return 0;
}

/*
 * Testcase 7
 * Tested function: fs_get_inode_for_name
 * Testcase: get inode for /tmp
 */
int testcase7() {
    inode_t* inode;
    fat16_probe_result = 1;
    fs_fat16_result = &fat16_superblock;
    setup();
    ASSERT(0==fs_init(0));
    inode = fs_get_inode_for_name("/tmp", 0);
    ASSERT(inode);
    ASSERT(inode->inode_nr==1);
    ASSERT(inode->dev==0);
    return 0;
}

/*
 * Testcase 8
 * Tested function: fs_get_inode_for_name
 * Testcase: get inode for /tmp/
 */
int testcase8() {
    inode_t* inode;
    fat16_probe_result = 1;
    fs_fat16_result = &fat16_superblock;
    setup();
    ASSERT(0==fs_init(0));
    inode = fs_get_inode_for_name("/tmp/", 0);
    ASSERT(inode);
    ASSERT(inode->inode_nr==1);
    ASSERT(inode->dev==0);
    return 0;
}

/*
 * Testcase 9
 * Tested function: fs_get_inode_for_name
 * Testcase: get inode for /hello/
 * should return 0
 */
int testcase9() {
    inode_t* inode;
    fat16_probe_result = 1;
    fs_fat16_result = &fat16_superblock;
    setup();
    ASSERT(0==fs_init(0));
    inode = fs_get_inode_for_name("/hello/", 0);
    ASSERT(0==inode);
    return 0;
}

/*
 * Testcase 10
 * Tested function: fs_get_inode_for_name
 * Testcase: get inode for //
 * should return root directory
 */
int testcase10() {
    inode_t* inode;
    fat16_probe_result = 1;
    fs_fat16_result = &fat16_superblock;
    setup();
    ASSERT(0==fs_init(0));
    inode = fs_get_inode_for_name("//", 0);
    ASSERT(inode);
    return 0;
}

/*
 * Testcase 11
 * Tested function: fs_get_inode_for_name
 * Testcase: get inode for /tmp/hidden before mounting
 * should return inode 3
 */
int testcase11() {
    inode_t* inode;
    fat16_probe_result = 1;
    fs_fat16_result = &fat16_superblock;
    setup();
    ASSERT(0==fs_init(0));
    inode = fs_get_inode_for_name("/tmp/hidden", 0);
    ASSERT(inode);
    ASSERT(inode->inode_nr==3);
    ASSERT(inode->dev==0);
    return 0;
}

/*
 * Testcase 12
 * Tested function: fs_get_inode_for_name
 * Testcase: get inode for /tmp/hidden after mounting
 * should return 0
 */
int testcase12() {
    inode_t* inode;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(0==fs_mount(&fat16_tmp_inode, 1, &ext2_impl));
    inode = fs_get_inode_for_name("/tmp/hidden", 0);
    ASSERT(0==inode);
    return 0;
}

/*
 * Testcase 13
 * Tested function: fs_mount
 * Testcase: mount on unsupported file system
 */
int testcase13() {
    inode_t* inode;
    fat16_probe_result = 1;
    ext2_probe_result = 0;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(fs_mount(&fat16_tmp_inode, 1, &ext2_impl));
    return 0;
}

/*
 * Testcase 14
 * Tested function: fs_get_inode_for_name
 * Testcase: get inode for /tmp/test after mounting
 * should return inode 1 on device 1
 */
int testcase14() {
    inode_t* inode;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(0==fs_mount(&fat16_tmp_inode, 1, &ext2_impl));
    inode = fs_get_inode_for_name("/tmp/test", 0);
    ASSERT(inode);
    ASSERT(inode->inode_nr==1);
    ASSERT(inode->dev==1);
    return 0;
}

/*
 * Testcase 15
 * Tested function: fs_get_inode_for_name
 * Testcase: get inode for /tmp/test before mounting
 * should return 0
 */
int testcase15() {
    inode_t* inode;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    inode = fs_get_inode_for_name("/tmp/test", 0);
    ASSERT(0==inode);
    return 0;
}

/*
 * Testcase 16
 * Tested function: fs_mount
 * Testcase: check that we cannot mount the same device twice - non-root device
 */
int testcase16() {
    inode_t* inode;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(0==fs_mount(&fat16_tmp_inode, 1, &ext2_impl));
    ASSERT(fs_mount(&fat16_usr_inode, 1, &ext2_impl));
    return 0;
}

/*
 * Testcase 17
 * Tested function: fs_mount
 * Testcase: check that we cannot mount the same device twice - root-device
 */
int testcase17() {
    inode_t* inode;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(fs_mount(&fat16_tmp_inode, 0, &ext2_impl));
    return 0;
}

/*
 * Testcase 18
 * Tested function: fs_mount
 * Testcase: check that we cannot mount on an already used mount point
 */
int testcase18() {
    inode_t* inode;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(0==fs_mount(&fat16_tmp_inode, 1, &ext2_impl));
    ASSERT(fs_mount(&fat16_tmp_inode, 2, &ext2_impl));
    return 0;
}

/*
 * Testcase 19
 * Tested function: fs_mount
 * Testcase: check that we cannot mount on an ordinary file
 */
int testcase19() {
    inode_t* inode;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(fs_mount(&fat16_hello_inode, 1, &ext2_impl));
    return 0;
}

/*
 * Testcase 20
 * Tested function: fs_open
 * Testcase: open a new file
 */
int testcase20() {
    inode_t* inode;
    fat16_probe_result = 1;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(fs_open(&fat16_tmp_inode, 0));
    return 0;
}

/*
 * Testcase 21
 * Tested function: fs_read
 * Testcase: open a file and read from it
 */
int testcase21() {
    inode_t* inode;
    fat16_probe_result = 1;
    open_file_t* of;
    char data[6];
    data[5] = 0;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    of = fs_open(&fat16_hello_inode, 0);
    ASSERT(of);
    ASSERT(fs_read(of, 1, data));
    ASSERT(data[0]=='h');
    ASSERT(fs_read(of, 4, data+1));
    ASSERT(0==strncmp("hello", data, 5));
    ASSERT(0==fs_read(of, 1, data));
    return 0;
}

/*
 * Testcase 22
 * Tested function: do_open
 * Testcase: open two files and make sure that the file
 * descriptors are not the same
 */
int testcase22() {
    inode_t* inode;
    fat16_probe_result = 1;
    int fd1;
    int fd2;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    fd1 = do_open("/hello", 0, 0);
    ASSERT(fd1==0);
    fd2 = do_open("/tmp/hidden", 0, 0);
    ASSERT(fd2==1);
    ASSERT(1==ref_count[2]);
    ASSERT(1==ref_count[3]);
    return 0;
}

/*
 * Testcase 23
 * Tested function: do_close
 * Testcase: open a file, close it again and verify
 * that the reference count of the inode is zero
 */
int testcase23() {
    inode_t* inode;
    fat16_probe_result = 1;
    int fd;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    fd = do_open("/hello", 0, 0);
    ASSERT(fd==0);
    ASSERT(1==ref_count[2]);
    ASSERT(0==do_close(0));
    ASSERT(0==ref_count[2]);
    return 0;
}

/*
 * Testcase 24
 * Tested function: do_close
 * Testcase: open a file which is not open
 */
int testcase24() {
    inode_t* inode;
    fat16_probe_result = 1;
    int fd;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(do_close(0));
    return 0;
}

/*
 * Testcase 25
 * Tested function: do_close
 * Testcase: open a file, close it again and call open again
 * to validate that the fd becomes available
 */
int testcase25() {
    inode_t* inode;
    fat16_probe_result = 1;
    int fd;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    fd = do_open("/hello", 0, 0);
    ASSERT(0==fd);
    ASSERT(0==do_close(fd));
    fd = do_open("/tmp/hidden", 0, 0);
    ASSERT(0==fd);
    return 0;
}

/*
 * Testcase 26
 * Tested function: do_read
 * Testcase: open a file and read from it
 */
int testcase26() {
    int fd;
    fat16_probe_result = 1;
    char data[6];
    data[5] = 0;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    fd = do_open("/hello", 0, 0);
    ASSERT(fd==0);
    ASSERT(do_read(fd, data, 1));
    ASSERT(data[0]=='h');
    ASSERT(do_read(fd, data+1,4));
    ASSERT(0==strncmp("hello", data, 5));
    ASSERT(0==do_read(fd, data, 1));
    return 0;
}

/*
 * Testcase 27
 * Tested function: fs_mount
 * Testcase: try to mount root filesystem while it is still mounted
 */
int testcase27() {
    int fd;
    fat16_probe_result = 1;
    char data[6];
    data[5] = 0;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(fs_mount(0, 1, &ext2_impl));
    return 0;
}

/*
 * Testcase 28
 * Tested function: fs_mount
 * Testcase: call fs_init with DEVICE_NONE, then call fs_mount to mount root fs
 */
int testcase28() {
    int fd;
    fat16_probe_result = 1;
    char data[6];
    data[5] = 0;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(DEVICE_NONE));
    ASSERT(0==fs_mount(0, 1, &ext2_impl));
    ASSERT(0==do_open("/test",0, 0));
    return 0;
}

/*
 * Testcase 29
 * Tested function: fs_get_inode_for_name
 * Testcase: call fs_get_inode_for_name when no root fs is mounted
 */
int testcase29() {
    int fd;
    fat16_probe_result = 1;
    char data[6];
    data[5] = 0;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(DEVICE_NONE));
    ASSERT(0==fs_get_inode_for_name("/", 0));
    return 0;
}

/*
 * Testcase 30
 * Tested function: do_open
 * Testcase: call do_open when no root fs is mounted
 */
int testcase30() {
    int fd;
    fat16_probe_result = 1;
    char data[6];
    data[5] = 0;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(DEVICE_NONE));
    ASSERT(0>=do_open("/test", 0, 0));
    return 0;
}

/*
 * Testcase 31
 * Tested function: fs_unmount
 * Testcase: call fs_unmount with an inode on which nothing is mounted
 */
int testcase31() {
    inode_t* tmp_inode;
    fat16_probe_result = 1;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    tmp_inode = fs_get_inode_for_name("/tmp", 0);
    ASSERT(tmp_inode);
    ASSERT(fs_unmount(tmp_inode));
    return 0;
}

/*
 * Testcase 32
 * Tested function: fs_unmount
 * Testcase: mount file system on /tmp, open /tmp/test
 * and make sure that when fs_unmount is called, the request
 * is rejected as there is still an open file on the device
 */
int testcase32() {
    inode_t* inode;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(0==fs_mount(&fat16_tmp_inode, 1, &ext2_impl));
    ASSERT(0==do_open("/tmp/test", 0, 0));
    ASSERT(fs_unmount(&ext2_root_inode));
    return 0;
}

/*
 * Testcase 33
 * Tested function: fs_unmount
 * Testcase: mount file system on /tmp, then unmount again
 * and make sure that /tmp/test is not visible, but /tmp/hidden is visible
 */
int testcase33() {
    inode_t* inode;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    setup();
    fs_fat16_result = &fat16_superblock;
    ext2_busy = 0;
    ASSERT(0==fs_init(0));
    ASSERT(0==fs_mount(&fat16_tmp_inode, 1, &ext2_impl));
    ASSERT(0==fs_unmount(&ext2_root_inode));
    ASSERT(-ENOENT==do_open("/tmp/test", 0, 0));
    ASSERT(0==do_open("/tmp/hidden", 0, 0));
    ext2_busy = 1;
    return 0;
}

/*
 * Testcase 34
 * Tested function: fs_unmount
 * Testcase: unmount root file system while there are open files
 */
int testcase34() {
    inode_t* inode;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(0==do_open("/tmp/hidden", 0, 0));
    ASSERT(fs_unmount(0));
    return 0;
}

/*
 * Testcase 35
 * Tested function: fs_unmount
 * Testcase: unmount root file system
 */
int testcase35() {
    inode_t* inode;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(0==fs_unmount(0));
    return 0;
}

/*
 * Testcase 36
 * Tested function: fs_unmount
 * Testcase: unmount root file system, mount again and try to open a file
 */
int testcase36() {
    inode_t* inode;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(0==fs_unmount(0));
    ASSERT(0==fs_mount(0, 1, &ext2_impl));
    ASSERT(0==do_open("/test", 0, 0));
    return 0;
}

/*
 * Testcase 37
 * Tested function: fs_clone
 * Testcase: open a file and read from it. Then call fs_clone, switch pid and read
 * again to verify that the file descriptor is still valid and points to the same file
 */
int testcase37() {
    inode_t* inode;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    char data;
    setup();
    pid = 0;
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(0==do_open("/hello", 0, 0));
    ASSERT(1==do_read(0, &data, 1));
    ASSERT('h'==data);
    fs_clone(0, 1);
    pid = 1;
    ASSERT(1==do_read(0, &data, 1));
    ASSERT('e'==data);
    pid = 0;
    return 0;
}

/*
 * Testcase 38
 * Tested function: do_mount
 * Testcase: mount a file system and open a file on mounted fs
 */
int testcase38() {
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    setup();
    pid = 0;
    ASSERT(0==fs_init(0));
    ASSERT(0==do_mount("/tmp", 1, "ext2"));
    ASSERT(0==do_open("/tmp/test", 0, 0));
    return 0;
}

/*
 * Testcase 39
 * Tested function: do_mount
 * Testcase: verify that root fs cannot be mounted twice
 */
int testcase39() {
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    setup();
    pid = 0;
    ASSERT(0==fs_init(0));
    ASSERT(do_mount("/", 1, "ext2"));
    return 0;
}

/*
 * Testcase 40
 * Tested function: do_mount
 * Testcase: verify that root fs can be mounted if not yet done
 */
int testcase40() {
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    setup();
    pid = 0;
    ASSERT(0==fs_init(DEVICE_NONE));
    ASSERT(0==do_mount("/", 0, "fat16"));
    ASSERT(0==do_open("/tmp", 0, 0));
    return 0;
}

/*
 * Testcase 41
 * Tested function: fs_lseek
 * Testcase: open a file and do lseek with SEEK_SET
 */
int testcase41() {
    inode_t* inode;
    fat16_probe_result = 1;
    open_file_t* of;
    char data[6];
    data[5] = 0;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    of = fs_open(&fat16_hello_inode, 0);
    ASSERT(of);
    ASSERT(fs_read(of, 1, data));
    ASSERT(data[0]=='h');
    ASSERT(0==fs_lseek(of, 0, SEEK_SET));
    ASSERT(fs_read(of, 1, data));
    ASSERT(data[0]=='h');
    return 0;
}

/*
 * Testcase 42
 * Tested function: fs_lseek
 * Testcase: open a file and do lseek with SEEK_CUR
 */
int testcase42() {
    inode_t* inode;
    fat16_probe_result = 1;
    open_file_t* of;
    char data[6];
    data[5] = 0;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    of = fs_open(&fat16_hello_inode, 0);
    ASSERT(of);
    ASSERT(fs_read(of, 1, data));
    ASSERT(data[0]=='h');
    ASSERT(2==fs_lseek(of, 1, SEEK_CUR));
    ASSERT(fs_read(of, 1, data));
    ASSERT(data[0]=='l');
    return 0;
}

/*
 * Testcase 43
 * Tested function: do_lseek
 * Testcase: open a file and seek, then read from it
 */
int testcase43() {
    int fd;
    fat16_probe_result = 1;
    char data[6];
    data[5] = 0;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    fd = do_open("/hello", 0, 0);
    ASSERT(fd==0);
    ASSERT(1==do_lseek(fd, 1, SEEK_SET));
    ASSERT(do_read(fd, data, 1));
    ASSERT(data[0]=='e');
    return 0;
}

/*
 * Testcase 44
 * Tested function: do_read
 * Testcase: open a file representing a tty and read from it
 */
int testcase44() {
    int fd;
    fat16_probe_result = 1;
    char data[6];
    data[0] = 0;
    data[5] = 0;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    fd = do_open("/dev/tty", 0, 0);
    ASSERT(fd==0);
    ASSERT(do_read(fd, data, 1));
    ASSERT(data[0]=='x');
    return 0;
}

/*
 * Testcase 45
 * Tested function: do_write
 * Testcase: open a file representing a tty and write to it
 */
int testcase45() {
    int fd;
    int i;
    fat16_probe_result = 1;
    char data[5];
    for (i = 0; i < 5; i++) {
        data[i] = i;
        tty_buffer[i] = 0;
    }
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    fd = do_open("/dev/tty", 0, 0);
    ASSERT(fd==0);
    ASSERT(do_write(fd, data, 5));
    for (i = 0; i < 5; i++)
        ASSERT(data[i]==tty_buffer[i]);
    return 0;
}

/*
 * Testcase 46
 * Tested function: fs_close_all
 * Testcase: close all open files of a process
 */
int testcase46() {
    inode_t* inode;
    fat16_probe_result = 1;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    /*
     * First open a file
     */
    ASSERT(0==do_open("/hello", 0, 0));
    /*
     * Then call fs_close_all
     */
    fs_close_all();
    /*
     * and open another file to make sure
     * that FD 0 is again available
     */
    ASSERT(0==do_open("/hello", 0, 0));
    return 0;
}

/*
 * Testcase 47
 * Tested function: fs_readdir
 * Testcase: get directory root and read one directory entry
 */
int testcase47() {
    int rc;
    direntry_t direntry;
    inode_t* inode;
    open_file_t* dir;
    fat16_probe_result = 1;
    fs_fat16_result = &fat16_superblock;
    setup();
    ASSERT(0==fs_init(0));
    inode = fs_get_inode_for_name("/", 0);
    ASSERT(inode);
    ASSERT(inode->inode_nr==0);
    ASSERT(inode->dev==0);
    dir = fs_open(inode, 0);
    ASSERT(dir);
    rc = fs_readdir(dir, &direntry);
    ASSERT(0==rc);
    ASSERT(0==strncmp(".", direntry.name, 1));
    return 0;
}

/*
 * Testcase 48
 * Tested function: fs_readdir
 * Testcase: get directory root and read three directory entries.
 * Verify that third entry is "tmp"
 */
int testcase48() {
    int rc;
    direntry_t direntry;
    inode_t* inode;
    open_file_t* dir;
    fat16_probe_result = 1;
    fs_fat16_result = &fat16_superblock;
    setup();
    ASSERT(0==fs_init(0));
    inode = fs_get_inode_for_name("/", 0);
    ASSERT(inode);
    ASSERT(inode->inode_nr==0);
    ASSERT(inode->dev==0);
    dir = fs_open(inode, 0);
    ASSERT(dir);
    rc = fs_readdir(dir, &direntry);
    ASSERT(0==rc);
    rc = fs_readdir(dir, &direntry);
    ASSERT(0==rc);
    rc = fs_readdir(dir, &direntry);
    ASSERT(0==rc);
    ASSERT(0==strncmp("tmp", direntry.name, 3));
    return 0;
}

/*
 * Testcase 49
 * Tested function: fs_readdir
 * Testcase: get directory root and read six directory entries.
 * Verify that next read returns -1
 */
int testcase49() {
    int rc;
    int i;
    direntry_t direntry;
    inode_t* inode;
    open_file_t* dir;
    fat16_probe_result = 1;
    fs_fat16_result = &fat16_superblock;
    setup();
    ASSERT(0==fs_init(0));
    inode = fs_get_inode_for_name("/", 0);
    ASSERT(inode);
    ASSERT(inode->inode_nr==0);
    ASSERT(inode->dev==0);
    dir = fs_open(inode, 0);
    ASSERT(dir);
    for (i=1;i<=6;i++) {
        rc = fs_readdir(dir, &direntry);
        ASSERT(0==rc);
    }
    rc = fs_readdir(dir, &direntry);
    ASSERT(-1==rc);
    return 0;
}

/*
 * Testcase 50
 * Tested function: do_readdir
 * Testcase: open a file representing a directory and read from it
 */
int testcase50() {
    int fd;
    fat16_probe_result = 1;
    direntry_t direntry;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    fd = do_open("/", 0, 0);
    ASSERT(fd==0);
    ASSERT(0==do_readdir(fd, &direntry));
    ASSERT(direntry.name[0]='.');
    return 0;
}


/*
 * Testcase 51
 * Tested function: do_open
 * Testcase: open a file which does not yet exist without the flag O_CREAT
 * Verify that an error code is returned
 */
int testcase51() {
    inode_t* inode;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    char data;
    setup();
    pid = 0;
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(-ENOENT==do_open("/blabla", 0, 0));
    return 0;
}

/*
 * Testcase 52
 * Tested function: do_open
 * Testcase: open a file which does not yet exist with the flag O_CREAT set
 * Verify that return code is zero
 */
int testcase52() {
    inode_t* inode;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    char data;
    setup();
    pid = 0;
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(0==do_open("/blabla", O_CREAT, 0));
    return 0;
}

/*
 * Testcase 53
 * Tested function: fs_get_inode_for_name
 * Testcase: get inode with relative path name tmp while cwd = /
 */
int testcase53() {
    inode_t* inode;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    inode = fs_get_inode_for_name("tmp", 0);
    ASSERT(inode);
    return 0;
}

/*
 * Testcase 54
 * Tested function: fs_get_inode_for_name
 * Testcase: get inode with relative path name dev/tty while cwd = /
 */
int testcase54() {
    inode_t* inode;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    inode = fs_get_inode_for_name("dev/tty", 0);
    ASSERT(inode);
    ASSERT(inode->inode_nr == 6);
    return 0;
}

/*
 * Testcase 55
 * Tested function: do_chdir
 * Testcase: change to /dev and get inode with relative path name tty
 */
int testcase55() {
    inode_t* inode;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(0==do_chdir("/dev"));
    inode = fs_get_inode_for_name("tty", 0);
    ASSERT(inode);
    ASSERT(inode->inode_nr == 6);
    return 0;
}

/*
 * Testcase 56
 * Tested function: do_chdir
 * Testcase: change to dev and get inode with relative path name tty
 */
int testcase56() {
    inode_t* inode;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(0==do_chdir("dev"));
    inode = fs_get_inode_for_name("tty", 0);
    ASSERT(inode);
    ASSERT(inode->inode_nr == 6);
    return 0;
}

/*
 * Testcase 57
 * Tested function: do_chdir
 * Testcase: change to dev and back to /
 */
int testcase57() {
    inode_t* inode;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(0==do_chdir("dev"));
    inode = fs_get_inode_for_name("tty", 0);
    ASSERT(inode);
    ASSERT(inode->inode_nr == 6);
    ASSERT(0==do_chdir("/"));
    ASSERT(fs_get_inode_for_name("dev/tty", 0));
    ASSERT(0==fs_get_inode_for_name("tty", 0));
    return 0;
}

/*
 * Testcase 58
 * Tested function: do_chdir
 * Testcase: change to dev and back to ..
 */
int testcase58() {
    inode_t* inode;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(0==do_chdir("dev"));
    inode = fs_get_inode_for_name("tty", 0);
    ASSERT(inode);
    ASSERT(inode->inode_nr == 6);
    ASSERT(0==do_chdir(".."));
    ASSERT(fs_get_inode_for_name("dev/tty", 0));
    ASSERT(0==fs_get_inode_for_name("tty", 0));
    return 0;
}

/*
 * Testcase 59
 * Tested function: fs_get_inode_for_name
 * Testcase: mount a file system on /tmp and
 * verify that /tmp/. is /tmp
 */
int testcase59() {
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    inode_t* inode1;
    inode_t* inode2;
    setup();
    pid = 0;
    ASSERT(0==fs_init(0));
    ASSERT(0==do_mount("/tmp", 1, "ext2"));
    inode1 = fs_get_inode_for_name("/tmp/.", 0);
    ASSERT(inode1);
    inode2 = fs_get_inode_for_name("/tmp", 0);
    ASSERT(inode2);
    ASSERT(inode1->dev==inode2->dev);
    ASSERT(inode1->inode_nr==inode2->inode_nr);
    return 0;
}

/*
 * Testcase 60
 * Tested function: fs_get_inode_for_name
 * Testcase: mount a file system on /tmp and
 * verify that /tmp/.. is /
 */
int testcase60() {
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    inode_t* inode1;
    inode_t* inode2;
    setup();
    pid = 0;
    ASSERT(0==fs_init(0));
    ASSERT(0==do_mount("/tmp", 1, "ext2"));
    inode1 = fs_get_inode_for_name("/tmp/..", 0);
    ASSERT(inode1);
    inode2 = fs_get_inode_for_name("/", 0);
    ASSERT(inode2);
    ASSERT(inode1->dev==inode2->dev);
    ASSERT(inode1->inode_nr==inode2->inode_nr);
    return 0;
}

/*
 * Testcase 61
 * Tested function: fs_get_inode_for_name
 * Testcase: verify that /tmp/.. is /
 */
int testcase61() {
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    inode_t* inode1;
    inode_t* inode2;
    setup();
    pid = 0;
    ASSERT(0==fs_init(0));
    inode1 = fs_get_inode_for_name("/tmp/..", 0);
    ASSERT(inode1);
    inode2 = fs_get_inode_for_name("/", 0);
    ASSERT(inode2);
    ASSERT(inode1->dev==inode2->dev);
    ASSERT(inode1->inode_nr==inode2->inode_nr);
    return 0;
}

/*
 * Testcase 62
 * Tested function: fs_get_inode_for_name
 * Testcase: mount the ext2 file system on /tmp and
 * verify that /tmp/dir/.. is /tmp
 */
int testcase62() {
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    inode_t* inode1;
    inode_t* inode2;
    setup();
    pid = 0;
    ASSERT(0==fs_init(0));
    ASSERT(0==do_mount("/tmp", 1, "ext2"));
    inode1 = fs_get_inode_for_name("/tmp/dir/..", 0);
    ASSERT(inode1);
    inode2 = fs_get_inode_for_name("/tmp", 0);
    ASSERT(inode2);
    ASSERT(inode1->dev==inode2->dev);
    ASSERT(inode1->inode_nr==inode2->inode_nr);
    return 0;
}

/*
 * Testcase 63
 * Tested function: fs_get_inode_for_name
 * Testcase: get inode for /usr/local/test after mounting
 * the ext2 file system on /usr/local
 */
int testcase63() {
    inode_t* inode;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(0==fs_mount(&fat16_usr_local_inode, 1, &ext2_impl));
    inode = fs_get_inode_for_name("/usr/local/test", 0);
    ASSERT(inode);
    ASSERT(inode->inode_nr==1);
    ASSERT(inode->dev==1);
    return 0;
}

/*
 * Testcase 64
 * Tested function: fs_get_inode_for_name
 * Testcase: get inode for /usr/local/.. after mounting
 * the ext2 file system on /usr/local and verify that this is /usr
 */
int testcase64() {
    inode_t* inode1;
    inode_t* inode2;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(0==fs_mount(&fat16_usr_local_inode, 1, &ext2_impl));
    inode1 = fs_get_inode_for_name("/usr/local/..", 0);
    ASSERT(inode1);
    inode2 = fs_get_inode_for_name("/usr", 0);
    ASSERT(inode2);
    ASSERT(inode2->inode_nr==inode1->inode_nr);
    ASSERT(inode2->dev==inode2->dev);
    return 0;
}

/*
 * Testcase 65
 * Tested function: fs_get_inode_for_name
 * Testcase: get inode for empty string and verify that
 * the current working directory is returned
 */
int testcase65() {
    inode_t* inode;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(0==fs_mount(&fat16_usr_local_inode, 1, &ext2_impl));
    inode = fs_get_inode_for_name("", 0);
    ASSERT(inode);
    ASSERT(inode->inode_nr==fat16_root_inode.inode_nr);
    do_chdir("/usr");
    inode = fs_get_inode_for_name("", 0);
    ASSERT(inode);
    ASSERT(inode->inode_nr==fat16_usr_inode.inode_nr);
    return 0;
}

/*
 * Testcase 66
 * Tested function: fcntl
 * Testcase: open a file and use fcntl with cmd F_GETFD to verify that the
 * file descriptor flag is zero
 */
int testcase66() {
    inode_t* inode;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    char data;
    setup();
    pid = 0;
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(0==do_open("/hello", 0, 0));
    ASSERT(0==do_fcntl(0, 1, 0));
    return 0;
}

/*
 * Testcase 67
 * Tested function: fcntl
 * Testcase: open a file and use fcntl with cmd F_SETFD to set the file descriptor flags
 */
int testcase67() {
    inode_t* inode;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    char data;
    setup();
    pid = 0;
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(0==do_open("/hello", 0, 0));
    ASSERT(0==do_fcntl(0, 2, 1));
    ASSERT(1==do_fcntl(0, 1, 0));
    return 0;
}

/*
 * Testcase 68
 * Tested function: stat
 * Testcase: do stat on an existing file and check fields
 */
int testcase68() {
    struct __ctOS_stat mystat;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    char data;
    setup();
    pid = 0;
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(0==do_stat("/hello", &mystat));
    ASSERT(2==mystat.st_ino);
    ASSERT(0==mystat.st_dev);
    ASSERT(0==mystat.st_rdev);
    ASSERT(S_ISREG(mystat.st_mode));
    ASSERT(mystat.st_size==fat16_hello_inode.size);
    return 0;
}


/*
 * Testcase 69
 * Tested function: do_dup
 * Testcase: open a file and read from it. Then call dup and read
 * again to verify that the new file descriptor is valid and points to the same file
 */
int testcase69() {
    inode_t* inode;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    char data;
    setup();
    pid = 0;
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(0==do_open("/hello", 0, 0));
    ASSERT(1==do_read(0, &data, 1));
    ASSERT('h'==data);
    ASSERT(1==do_dup(0, 0));
    ASSERT(1==do_read(0, &data, 1));
    ASSERT('e'==data);
    ASSERT(1==do_read(1, &data, 1));
    ASSERT('l'==data);
    /*
     * Now close fd 1 and make sure that fd 0 is still readable
     */
    ASSERT(0==do_close(1));
    ASSERT(1==do_read(0, &data, 1));
    ASSERT('l'==data);
    ASSERT(0==do_close(0));
    ASSERT(do_read(0, &data, 1)<0);
    return 0;
}

/*
 * Testcase 70
 * Tested function: isatty and stat
 * Testcase: call isatty and stat on a tty
 */
int testcase70() {
    inode_t* inode;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    char data;
    struct __ctOS_stat _mystat;
    setup();
    pid = 0;
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(0==do_open("/dev/tty", 0, 0));
    ASSERT(1==do_isatty(0));
    /*
     * now call stat and check st_rdev
     */
    ASSERT(0 == do_stat("/dev/tty", &_mystat));
    ASSERT(_mystat.st_rdev == DEVICE(2,0));
    return 0;
}

/*
 * Testcase 71
 * Tested function: isatty
 * Testcase: call isatty on a regular file
 */
int testcase71() {
    inode_t* inode;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    char data;
    setup();
    pid = 0;
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(0==do_open("/hello", 0, 0));
    ASSERT(0==do_isatty(0));
    return 0;
}

/*
 * Testcase 72
 * Tested function: isatty
 * Testcase: call isatty on an invalid file descriptor
 */
int testcase72() {
    inode_t* inode;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    char data;
    setup();
    pid = 0;
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(0==do_isatty(10));
    return 0;
}

/*
 * Testcase 73
 * Tested function: umask
 * Testcase: call umask to retrieve the default umask
 */
int testcase73() {
    inode_t* inode;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    char data;
    setup();
    pid = 0;
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT((S_IWGRP | S_IWOTH)==do_umask(0));
    return 0;
}

/*
 * Testcase 74
 * Tested function: umask
 * Testcase: call umask to retrieve a changed umask
 */
int testcase74() {
    inode_t* inode;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    char data;
    setup();
    pid = 0;
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT((S_IWGRP | S_IWOTH)==do_umask(0777));
    ASSERT(0777==do_umask(0));
    return 0;
}

/*
 * Testcase 75
 * Tested function: pipe
 * Testcase: call pipe to create a pipe
 */
int testcase75() {
    int fd[2];
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    char data;
    setup();
    pid = 0;
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(0==do_pipe(fd, 0));
    ASSERT(0==fd[0]);
    ASSERT(1==fd[1]);
    return 0;
}

/*
 * Testcase 76
 * Tested function: pipe
 * Testcase: call pipe to create a and close both ends
 */
int testcase76() {
    int fd[2];
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    char data;
    setup();
    pid = 0;
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(0==do_pipe(fd, 0));
    ASSERT(0==fd[0]);
    ASSERT(1==fd[1]);
    ASSERT(0==do_close(fd[0]));
    ASSERT(0==do_close(fd[1]));
    return 0;
}

/*
 * Testcase 77
 * Tested function: do_write
 * Testcase: write to a pipe
 */
int testcase77() {
    int fd[2];
    char buffer = 'a';
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    char data;
    setup();
    pid = 0;
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(0==do_pipe(fd, 0));
    ASSERT(0==fd[0]);
    ASSERT(1==fd[1]);
    ASSERT(1==do_write(1, &buffer, 1));
    return 0;
}

/*
 * Testcase 78
 * Tested function: do_read
 * Testcase: write to a pipe and read from it
 */
int testcase78() {
    int fd[2];
    char buffer = 'a';
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    char data;
    setup();
    pid = 0;
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(0==do_pipe(fd, 0));
    ASSERT(0==fd[0]);
    ASSERT(1==fd[1]);
    ASSERT(1==do_write(1, &buffer, 1));
    buffer = '0';
    ASSERT(1==do_read(0, &buffer, 1));
    ASSERT('a'==buffer);
    return 0;
}

/*
 * Testcase 79
 * Tested function: fstat
 * Testcase: do fstat on an existing file and check fields
 */
int testcase79() {
    struct __ctOS_stat mystat;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    char data;
    setup();
    pid = 0;
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(0==do_open("/hello", 0, 0));
    ASSERT(0==do_fstat(0, &mystat));
    ASSERT(2==mystat.st_ino);
    ASSERT(0==mystat.st_dev);
    ASSERT(S_ISREG(mystat.st_mode));
    ASSERT(mystat.st_size==fat16_hello_inode.size);
    return 0;
}

/*
 * Testcase 80
 * Tested function: fcntl - F_GETFL
 * Testcase: open an existing file and read the flags
 * using fcntl
 */
int testcase80() {
    struct __ctOS_stat mystat;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    char data;
    setup();
    pid = 0;
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(0==do_open("/hello", O_RDWR, 0));
    ASSERT(O_RDWR==do_fcntl(0, F_GETFL, 0));
    return 0;
}

/*
 * Testcase 81
 * Tested function: fcntl - F_SETFL
 * Testcase: open an existing file, change the flags and re-read
 * using fcntl
 */
int testcase81() {
    struct __ctOS_stat mystat;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    char data;
    setup();
    pid = 0;
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(0==do_open("/hello", O_RDWR, 0));
    ASSERT(O_RDWR==do_fcntl(0, F_GETFL, 0));
    ASSERT(0==do_fcntl(0, F_SETFL, O_APPEND));
    ASSERT(O_RDWR+O_APPEND==do_fcntl(0, F_GETFL, 0));
    return 0;
}

/*
 * Testcase 82
 * Tested function: do_open
 * Testcase: open a file which does not yet exist with the flag O_CREAT and O_EXCL set
 * Verify that return code is zero
 */
int testcase82() {
    inode_t* inode;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    char data;
    setup();
    pid = 0;
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(0==do_open("/blabla", O_CREAT | O_EXCL, 0));
    return 0;
}

/*
 * Testcase 83
 * Tested function: do_open
 * Testcase: open a file which does exist with the flag O_CREAT and O_EXCL set
 * Verify that return code is -EEXIST
 */
int testcase83() {
    inode_t* inode;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    char data;
    setup();
    pid = 0;
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(-130==do_open("/hello", O_CREAT | O_EXCL, 0));
    return 0;
}

/*
 * Testcase 84
 * Tested function: fs_get_inode_for_name
 * Testcase: get inode for /tmp after mounting
 * should return inode / on device 1
 */
int testcase84() {
    inode_t* inode;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(0==fs_mount(&fat16_tmp_inode, 1, &ext2_impl));
    inode = fs_get_inode_for_name("/tmp", 0);
    ASSERT(inode);
    ASSERT(inode->dev==1);
    ASSERT(inode->inode_nr==0);
    return 0;
}

/*
 * Testcase 85
 * Tested function: fs_get_dirname
 * Testcase: get directory name for /usr/local
 */
int testcase85() {
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    char buffer[256];
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(0==fs_get_dirname(&fat16_usr_local_inode, buffer, 256));
    ASSERT(0==strcmp("/usr/local", buffer));
    return 0;
}

/*
 * Testcase 86
 * Tested function: fs_get_dirname
 * Testcase: get directory name for /usr
 */
int testcase86() {
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    char buffer[256];
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(0==fs_get_dirname(&fat16_usr_inode, buffer, 256));
    ASSERT(0==strcmp("/usr", buffer));
    return 0;
}

/*
 * Testcase 87
 * Tested function: fs_get_dirname
 * Testcase: get directory name for /
 */
int testcase87() {
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    char buffer[256];
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(0==fs_get_dirname(&fat16_root_inode, buffer, 256));
    ASSERT(0==strcmp("/", buffer));
    return 0;
}

/*
 * Testcase 88
 * Tested function: fs_get_dirname
 * Testcase: get name for /tmp/dir after mounting
 */
int testcase88() {
    inode_t* inode;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    char buffer[256];
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(0==fs_mount(&fat16_tmp_inode, 1, &ext2_impl));
    ASSERT(0==fs_get_dirname(&ext2_dir_inode, buffer, 32));
    ASSERT(0==strcmp("/tmp/dir", buffer));
    return 0;
}

/*
 * Testcase 89
 * Tested function: fs_get_dirname
 * Testcase: get name for /tmp after mounting
 */
int testcase89() {
    inode_t* inode;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    char buffer[256];
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(0==fs_mount(&fat16_tmp_inode, 1, &ext2_impl));
    ASSERT(0==fs_get_dirname(&ext2_root_inode, buffer, 32));
    ASSERT(0==strcmp("/tmp", buffer));
    return 0;
}

/*
 * Testcase 90
 * Tested function: fs_read
 * Testcase: open a file and try to read more than INT32_MAX bytes
 */
int testcase90() {
    inode_t* inode;
    fat16_probe_result = 1;
    open_file_t* of;
    char data[6];
    data[5] = 0;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    of = fs_open(&fat16_hello_inode, 0);
    ASSERT(of);
    ASSERT(-132==fs_read(of, (1 << 31), data));
    return 0;
}

/*
 * Testcase 91
 * Tested function: fs_read
 * Testcase: open a file and read one byte. Then try to read (1 << 31)-1 bytes
 */
int testcase91() {
    inode_t* inode;
    fat16_probe_result = 1;
    open_file_t* of;
    char data[6];
    data[5] = 0;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    of = fs_open(&fat16_hello_inode, 0);
    ASSERT(of);
    ASSERT(1==fs_read(of, 1, data));
    ASSERT(-132==fs_read(of, (1 << 31)- (unsigned int) 1, data));
    return 0;
}

/*
 * Testcase 92
 * Tested function: fs_write
 * Testcase: open a file and try to write more than INT32_MAX bytes
 */
int testcase92() {
    inode_t* inode;
    fat16_probe_result = 1;
    open_file_t* of;
    char data[6];
    data[5] = 0;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    of = fs_open(&fat16_hello_inode, 0);
    ASSERT(of);
    ASSERT(-132==fs_write(of, (1 << 31), data));
    return 0;
}

/*
 * Testcase 93
 * Tested function: fs_write
 * Testcase: open a file and read one byte. Then try to write (1 << 31)-1 bytes
 */
int testcase93() {
    inode_t* inode;
    fat16_probe_result = 1;
    open_file_t* of;
    char data[6];
    data[5] = 0;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    of = fs_open(&fat16_hello_inode, 0);
    ASSERT(of);
    ASSERT(1==fs_write(of, 1, data));
    ASSERT(-132==fs_write(of, (1 << 31)- (unsigned int) 1, data));
    return 0;
}


/*
 * Testcase 94
 * Tested function: fs_lseek
 * Testcase: open a file and read one byte. Then try to seek forward by (1<<31)-1 bytes
 */
int testcase94() {
    inode_t* inode;
    fat16_probe_result = 1;
    open_file_t* of;
    char data[6];
    data[5] = 0;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    of = fs_open(&fat16_hello_inode, 0);
    ASSERT(of);
    ASSERT(1==fs_read(of, 1, data));
    ASSERT(-132==fs_lseek(of, INT32_MAX, SEEK_CUR));
    return 0;
}

/*
 * Testcase 95
 * Tested function: fs_lseek
 * Testcase: open a file and seek to position INT32_MAX+1
 */
int testcase95() {
    inode_t* inode;
    fat16_probe_result = 1;
    open_file_t* of;
    char data[6];
    data[5] = 0;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    of = fs_open(&fat16_hello_inode, 0);
    ASSERT(of);
    ASSERT(-132==fs_lseek(of, (1<<31), SEEK_SET));
    return 0;
}

/*
 * Testcase 96
 * Tested function: fs_get_inode_for_name
 * Testcase: get inode for //tmp
 */
int testcase96() {
    inode_t* inode;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    inode = fs_get_inode_for_name("//tmp", 0);
    ASSERT(inode);
    ASSERT(inode->dev==0);
    ASSERT(inode->inode_nr==1);
    return 0;
}

/*
 * Testcase 97
 * Tested function: stat
 * Testcase: do stat on a non-existing file
 */
int testcase97() {
    struct __ctOS_stat mystat;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    char data;
    setup();
    pid = 0;
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(-ENOENT==do_stat("/hellobla", &mystat));
    return 0;
}

/*
 * Testcase 98
 * Tested functions: FD_* macros to work with fd_set structures
 */
int testcase98() {
    fd_set myset;
    int i;
    int j;
    /*
     * First zero set, then check that all descriptors are clear
     */
    FD_ZERO(&myset);
    for (i = 0; i < FD_SETSIZE; i++) {
        ASSERT(0 == FD_ISSET(i, &myset));
    }
    /*
     * Then, for each possible FD, set file descriptor and make sure
     * sure that it is set, but no other FD is set
     */
    for (i = 0; i < FD_SETSIZE; i++) {
        FD_ZERO(&myset);
        FD_SET(i, &myset);
        ASSERT(1 == FD_ISSET(i, &myset));
        for (j = 0; j < FD_SETSIZE; j++)
            if (j != i)
                ASSERT((0 == FD_ISSET(j, &myset)));
    }
    /*
     * Now repeat the same with FD_CLR
     */
    for (i = 0; i < FD_SETSIZE; i++) {
        FD_ZERO(&myset);
        for (j = 0; j < FD_SETSIZE; j++)
            FD_SET(j, &myset);
        FD_CLR(i, &myset);
        ASSERT(0 == FD_ISSET(i, &myset));
        for (j = 0; j < FD_SETSIZE; j++)
            if (j != i)
                ASSERT((1 == FD_ISSET(j, &myset)));
        }
    return 0;
}

/*
 * Testcase 99:
 * Testcase: call do_select with a timeout. Verify that the timeout used for the semaphore is correct
 * Case 1: only seconds specified. According to POSIX, we should be able to support at least 31 days
 */
int testcase99() {
    int fd;
    fd_set readfds;
    struct timeval timeout;
    /*
     * Initialize file system layer
     */
    ASSERT(0 == fs_init(0));
    /*
     * and open a socket
     */
    fd = do_socket(AF_INET, SOCK_STREAM, 0);
    ASSERT(0 == fd);
    /*
     * Prepare FD set
     */
    FD_ZERO(&readfds);
    FD_SET(0, &readfds);
    /*
     * and timeout
     */
    timeout.tv_sec = (31 * 24 * 60 * 60);
    timeout.tv_usec = 0;
    /*
     * and call do_select
     */
    sem_down_timed_called = 0;
    __fs_loglevel = 0;
    do_putchar = 1;
    do_select(1024, &readfds, 0, 0, &timeout);
    __fs_loglevel = 0;
    ASSERT(1 == sem_down_timed_called);
    ASSERT(last_timeout == (31 * 24 * 60 * 60)*HZ);
    return 0;
}

/*
 * Testcase 100:
 * Testcase: call do_select with a timeout. Verify that the timeout used for the semaphore is correct
 * Case 2: only micro-seconds specified
 */
int testcase100() {
    int fd;
    fd_set readfds;
    struct timeval timeout;
    /*
     * Initialize file system layer
     */
    ASSERT(0 == fs_init(0));
    /*
     * and open a socket
     */
    fd = do_socket(AF_INET, SOCK_STREAM, 0);
    ASSERT(0 == fd);
    /*
     * Prepare FD set
     */
    FD_ZERO(&readfds);
    FD_SET(0, &readfds);
    /*
     * and timeout - use 10 ms, i.e. 1 tick
     */
    timeout.tv_sec = 0;
    timeout.tv_usec = 1000000 / HZ;
    /*
     * and call do_select
     */
    sem_down_timed_called = 0;
    __fs_loglevel = 0;
    do_putchar = 1;
    do_select(1024, &readfds, 0, 0, &timeout);
    __fs_loglevel = 0;
    ASSERT(1 == sem_down_timed_called);
    ASSERT(last_timeout == 1);
    return 0;
}

/*
 * Testcase 101:
 * Testcase: call do_select with a timeout. Verify that the timeout used for the semaphore is correct
 * Case 3: micro-seconds and seconds specified
 */
int testcase101() {
    int fd;
    fd_set readfds;
    struct timeval timeout;
    /*
     * Initialize file system layer
     */
    ASSERT(0 == fs_init(0));
    /*
     * and open a socket
     */
    fd = do_socket(AF_INET, SOCK_STREAM, 0);
    ASSERT(0 == fd);
    /*
     * Prepare FD set
     */
    FD_ZERO(&readfds);
    FD_SET(0, &readfds);
    /*
     * and timeout - use 10 ms, i.e. 1 tick, plus 2 seconds, i.e. 2*HZ ticks
     */
    timeout.tv_sec = 2;
    timeout.tv_usec = 1000000 / HZ;
    /*
     * and call do_select
     */
    sem_down_timed_called = 0;
    __fs_loglevel = 0;
    do_putchar = 1;
    do_select(1024, &readfds, 0, 0, &timeout);
    __fs_loglevel = 0;
    ASSERT(1 == sem_down_timed_called);
    ASSERT(last_timeout == 1 + 2*HZ);
    return 0;
}

/*
 * Testcase 102:
 * Tested function: fs_ftruncate
 * Testcase: open a file for writing, then truncate
 */
int testcase102() {
    inode_t* inode;
    fat16_probe_result = 1;
    open_file_t* of;
    char data[6];
    data[5] = 0;
    setup();
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    of = fs_open(&fat16_hello_inode, 0);
    ASSERT(of);
    ASSERT(of->inode);
    /*
     * Now do the actual truncate
     */
    __fat16_trunc_called = 0;
    ASSERT(0 == fs_ftruncate(of, 0));
    ASSERT(__fat16_trunc_called);
    return 0;
}

/*
 * Testcase 103
 * Tested function: fs_ftruncate
 * Testcase: call ftruncate on a directory
 */
int testcase103() {
    inode_t* inode;
    open_file_t* of;
    setup();
    ASSERT(0==fs_init(0));
    inode = fs_get_inode_for_name("/tmp", 0);
    of = fs_open(inode, 0);
    __fat16_trunc_called = 0;
    ASSERT(-EPERM == fs_ftruncate(of, 0));
    ASSERT(0 == __fat16_trunc_called);
}

/*
 * Testcase 104
 * Tested function: do_ftruncate
 * Testcase: call ftruncate on a file descriptor open for writing
 */
int testcase104() {
    inode_t* inode;
    int fd;
    setup();
    ASSERT(0==fs_init(0));
    fd = do_open("/hello", O_RDWR, 0);
    __fat16_trunc_called = 0;
    ASSERT(0 == do_ftruncate(fd, 0));
    ASSERT(1 == __fat16_trunc_called);
    return 0;
}

/*
 * Testcase 105
 * Tested function: do_ftruncate
 * Testcase: call ftruncate on a file descriptor open for reading
 */
int testcase105() {
    inode_t* inode;
    int fd;
    setup();
    ASSERT(0==fs_init(0));
    fd = do_open("/hello", O_RDONLY, 0);
    __fat16_trunc_called = 0;
    ASSERT(-EINVAL == do_ftruncate(fd, 0));
    ASSERT(0 == __fat16_trunc_called);
    return 0;
}

/*
 * Testcase 106
 * Tested function: do_ftruncate
 * Testcase: call ftruncate on an invalid file descriptor
 */
int testcase106() {
    inode_t* inode;
    setup();
    ASSERT(0==fs_init(0));
    __fat16_trunc_called = 0;
    ASSERT(-EBADF == do_ftruncate(42, 0));
    ASSERT(0 == __fat16_trunc_called);
    return 0;
}

/*
 * Testcase 107
 * Tested function: do_ftruncate
 * Testcase: call ftruncate on a file descriptor with a negative target size
 */
int testcase107() {
    inode_t* inode;
    int fd;
    setup();
    ASSERT(0==fs_init(0));
    fd = do_open("/hello", O_RDWR, 0);
    __fat16_trunc_called = 0;
    ASSERT(-EINVAL == do_ftruncate(fd, -1));
    ASSERT(0 == __fat16_trunc_called);
    return 0;
}

/*
 * Testcase 108
 * Tested function: fs_get_inode_for_name with at != NULL
 * Testcase: get inode for a relative path name using the at argument
 */
int testcase108() {
    inode_t* inode;
    fat16_probe_result = 1;
    fs_fat16_result = &fat16_superblock;
    setup();
    ASSERT(0==fs_init(0));
    /*
     * We first need to get a reference to /usr
     */
    inode_t* usr_inode = fs_get_inode_for_name("/usr", 0);
    ASSERT(usr_inode);
    /*
     * Now we get /usr/local
     */
    inode = fs_get_inode_for_name("local", usr_inode);
    /*
     * This should be the same as if we were directly using /usr/local
     */
    ASSERT(inode);
    ASSERT(inode->inode_nr == 7);
    return 0;
}


/*
 * Testcase 109
 * Tested function: fs_get_inode_for_name with at != NULL
 * Testcase: get inode for an absolute path name using the at argument
 * which should then be ignored
 */
int testcase109() {
    inode_t* inode;
    fat16_probe_result = 1;
    fs_fat16_result = &fat16_superblock;
    setup();
    ASSERT(0==fs_init(0));
    /*
     * We first need to get a reference to /usr
     */
    inode_t* usr_inode = fs_get_inode_for_name("/usr", 0);
    ASSERT(usr_inode);
    /*
     * Now we get /usr/local
     */
    inode = fs_get_inode_for_name("/usr/local", usr_inode);
    /*
     * This should have ignored the inode_at parameter
     */
    ASSERT(inode);
    ASSERT(inode->inode_nr == 7);
    return 0;
}


/*
 * Testcase 110
 * Tested function: fs_get_inode_for_name with at != NULL
 * Testcase: get inode for a relative path name using the at argument
 * and verify that the reference count of this inode is not changed
 */
int testcase110() {
    inode_t* inode;
    fat16_probe_result = 1;
    fs_fat16_result = &fat16_superblock;
    setup();
    ASSERT(0==fs_init(0));
    int old_ref_count = 0;
    /*
     * We first need to get a reference to /usr
     */
    inode_t* usr_inode = fs_get_inode_for_name("/usr", 0);
    ASSERT(usr_inode);
    old_ref_count = ref_count[usr_inode->inode_nr];
    /*
     * Now we get /usr/local
     */
    inode = fs_get_inode_for_name("local", usr_inode);
    /*
     * This should be the same as if we were directly using /usr/local
     */
    ASSERT(inode);
    ASSERT(inode->inode_nr == 7);
    /*
     * and the reference count should not have changed
     */ 
    ASSERT(old_ref_count == ref_count[usr_inode->inode_nr]);
    return 0;
}

/*
 * Testcase 111
 * Tested function: openat at != NULL
 * Testcase: use openat with a relative path name
 */
int testcase111() {
    inode_t* inode;
    int old_inode_ref_count = 0;
    int old_of_refcounts = 0;
    fat16_probe_result = 1;
    fs_fat16_result = &fat16_superblock;
    setup();
    ASSERT(0==fs_init(0));
    int dirfd = 0;
    int fd;
    /*
     * We first need to get a reference to /usr. We call this
     * twice, the second fd should then be 1
     */
    do_open("/usr", 0, 0);
    dirfd = do_open("/usr", 0, 0);
    ASSERT(dirfd);
    /*
     * Now we get /usr/local
     */
    old_inode_ref_count = ref_count[4];
    old_of_refcounts = fs_get_of_refcounts();
    ASSERT((fd = do_openat("local", 0, 0, dirfd)));
    ASSERT(old_inode_ref_count == ref_count[4]);
    /*
     * We also check that the reference count of the open
     * file has not been increased
     */
    do_close(fd);
    ASSERT(old_of_refcounts == fs_get_of_refcounts());
    return 0;
}

/*
 * Testcase 112
 * Tested function: openat at != NULL
 * Testcase: use openat with a relative path name but with at = AT_FDCWD
 */
int testcase112() {
    inode_t* inode;
    int rc;
    int old_inode_ref_count = 0;
    int old_of_refcounts = 0;
    fat16_probe_result = 1;
    fs_fat16_result = &fat16_superblock;
    setup();
    ASSERT(0==fs_init(0));
    int fd;
    /*
     * Open one file to make fd > 0
     */
    ASSERT(0 == do_open("/", 0, 0));
    old_inode_ref_count = ref_count[4];
    old_of_refcounts = fs_get_of_refcounts();
    /*
     * This call should fail as we are not in /usr
     */
    ASSERT(0 > (fd = do_openat("local", 0, 0, -200)));
    /*
     * We also check that the reference counts have not changed
     */
    ASSERT(old_inode_ref_count == ref_count[4]);
    ASSERT(old_of_refcounts == fs_get_of_refcounts());
    /*
     * This call should succeed
     */
    rc = do_chdir("/usr");
    ASSERT(0 == rc);
    old_inode_ref_count = ref_count[4];
    old_of_refcounts = fs_get_of_refcounts();
    ASSERT((fd = do_openat("local", 0, 0, -200)));
    do_close(fd);
    ASSERT(old_inode_ref_count == ref_count[4]);
    ASSERT(old_of_refcounts == fs_get_of_refcounts());
    return 0;
}


/*
 * Testcase 113
 * Tested function: openat at != NULL
 * Testcase: use openat with a relative path name
 * which does not refer to a directory
 */
int testcase113() {
    inode_t* inode;
    int old_inode_ref_count = 0;
    int old_of_refcounts = 0;
    fat16_probe_result = 1;
    fs_fat16_result = &fat16_superblock;
    setup();
    ASSERT(0==fs_init(0));
    int dirfd = 0;
    int fd;
    /*
     * We first need to get a reference to /hello. We call this
     * twice, the second fd should then be 1
     */
    do_open("/usr", 0, 0);
    dirfd = do_open("/hello", 0, 0);
    ASSERT(dirfd);
    /*
     * Now we get /usr/local. This should with -EBADF
     */
    old_inode_ref_count = ref_count[4];
    old_of_refcounts = fs_get_of_refcounts();
    ASSERT(-115 == (fd = do_openat("local", 0, 0, dirfd)));
    ASSERT(old_inode_ref_count == ref_count[4]);
    /*
     * We also check that the reference count of the open
     * file has not been increased
     */
    ASSERT(old_of_refcounts == fs_get_of_refcounts());
    return 0;
}

/*
 * Testcase 114
 * Tested function: openat at != NULL
 * Testcase: use openat with a relative path name
 * which does not even refer to an open file
 */
int testcase114() {
    inode_t* inode;
    int old_inode_ref_count = 0;
    int old_of_refcounts = 0;
    fat16_probe_result = 1;
    fs_fat16_result = &fat16_superblock;
    setup();
    ASSERT(0==fs_init(0));
    int dirfd = 0;
    int fd;
    dirfd = 1234;
    /*
     * Now we get /usr/local. This should with -EBADF
     */
    old_inode_ref_count = ref_count[4];
    old_of_refcounts = fs_get_of_refcounts();
    ASSERT(-115 == (fd = do_openat("local", 0, 0, dirfd)));
    ASSERT(old_inode_ref_count == ref_count[4]);
    /*
     * We also check that the reference count of the open
     * file has not been increased
     */
    ASSERT(old_of_refcounts == fs_get_of_refcounts());
    return 0;
}

/*
 * Testcase 115
 * Tested function: fchdir
 * Testcase: use fchdir to switch to /usr
 */
int testcase115() {
    inode_t* inode;
    int old_inode_ref_count = 0;
    int old_of_refcounts = 0;
    fat16_probe_result = 1;
    fs_fat16_result = &fat16_superblock;
    setup();
    ASSERT(0==fs_init(0));
    int dirfd = 0;
    /*
     * Get /usr. We call open twice, as the first
     * open will return 0 in any case
     */
    do_open("/usr", 0, 0);
    dirfd = do_open("/usr", 0, 0);
    ASSERT(dirfd);
    /*
     * Now we chdir to there
     */
    old_inode_ref_count = ref_count[4];
    old_of_refcounts = fs_get_of_refcounts();
    ASSERT(0 == do_fchdir(dirfd));
    /*
     * This will have increased the reference count of
     * the /usr inode by one
     */
    ASSERT((old_inode_ref_count + 1) == ref_count[4]);
    /*
     * We also check that the reference count of the open
     * file has not been increased
     */
    ASSERT(old_of_refcounts == fs_get_of_refcounts());
    /*
     * Verify that we have actually switched
     */
    char* cwd_buffer = (char*) malloc(512);
    ASSERT(0 == do_getcwd(cwd_buffer, 512));
    ASSERT(0 == strcmp(cwd_buffer, "/usr"));
    free(cwd_buffer);
    return 0;
}

/*
 * Testcase 116
 * Tested function: fchdir
 * Testcase: do fchdir on an invalid file descriptor
 */
int testcase116() {
    inode_t* inode;
    int old_inode_ref_count = 0;
    int old_of_refcounts = 0;
    fat16_probe_result = 1;
    fs_fat16_result = &fat16_superblock;
    setup();
    ASSERT(0==fs_init(0));
    int dirfd = 15;
    /*
     * Now we chdir to there - this should fail
     * with EBADF
     */
    old_inode_ref_count = ref_count[4];
    old_of_refcounts = fs_get_of_refcounts();
    ASSERT(115 == do_fchdir(dirfd));
    /*
     * This will leave the reference counts unchanged
     */
    ASSERT(old_inode_ref_count == ref_count[4]);
    ASSERT(old_of_refcounts == fs_get_of_refcounts());
    /*
     * Verify that we have actually not switched
     */
    char* cwd_buffer = (char*) malloc(512);
    ASSERT(0 == do_getcwd(cwd_buffer, 512));
    ASSERT(0 == strcmp(cwd_buffer, "/"));
    free(cwd_buffer);
    return 0;
}

/*
 * Testcase 117
 * Tested function: fchdir
 * Testcase: do fchdir on an invalid file descriptor
 * that does not refer to a directory
 */
int testcase117() {
    inode_t* inode;
    int old_inode_ref_count = 0;
    int old_of_refcounts = 0;
    int dirfd;
    fat16_probe_result = 1;
    fs_fat16_result = &fat16_superblock;
    setup();
    ASSERT(0==fs_init(0));
    do_open("/hello", 0,0);
    ASSERT((dirfd = do_open("/hello", 0, 0)));
    /*
     * Now we chdir to there - this should fail
     * with ENOTDIR
     */
    old_inode_ref_count = ref_count[4];
    old_of_refcounts = fs_get_of_refcounts();
    ASSERT(113 == do_fchdir(dirfd));
    /*
     * This will leave the reference counts unchanged
     */
    ASSERT(old_inode_ref_count == ref_count[4]);
    ASSERT(old_of_refcounts == fs_get_of_refcounts());
    /*
     * Verify that we have actually not switched
     */
    char* cwd_buffer = (char*) malloc(512);
    ASSERT(0 == do_getcwd(cwd_buffer, 512));
    ASSERT(0 == strcmp(cwd_buffer, "/"));
    free(cwd_buffer);
    return 0;
}

/*
 * Testcase 118
 * Tested function: do_dup2
 * Testcase: open a file and read from it. Then call dup2 and read
 * again to verify that the new file descriptor is valid and points to the same file
 * Assume that fd2 is not used yet
 */
int testcase118() {
    inode_t* inode;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    char data;
    int of_refcounts;
    setup();
    pid = 0;
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(0==do_open("/hello", 0, 0));
    ASSERT(1==do_read(0, &data, 1));
    ASSERT('h'==data);
    /*
     * Now call dup2
     */
    of_refcounts = fs_get_of_refcounts();
    ASSERT(10==do_dup2(0, 10));
    /*
     * As file descriptor 10 was not used, the total
     * number of refcounts should have increased by one
     */
    ASSERT((of_refcounts + 1) == fs_get_of_refcounts());
    /*
     * Verify that the new file descriptor 10 works
     */
    ASSERT(1==do_read(10, &data, 1));
    ASSERT('e'==data);
    ASSERT(1==do_read(0, &data, 1));
    ASSERT('l'==data);
    /*
     * Now close fd 10 and make sure that fd 0 is still readable
     */
    ASSERT(0==do_close(10));
    ASSERT(1==do_read(0, &data, 1));
    ASSERT('l'==data);
    ASSERT(0==do_close(0));
    ASSERT(do_read(0, &data, 1)<0);
    return 0;
}

/*
 * Testcase 119
 * Tested function: do_dup2
 * Testcase: open a file and read from it. Then call dup2 and read
 * again to verify that the new file descriptor is valid and points to the same file
 * Assume that fd2 is already used
 */
int testcase119() {
    inode_t* inode;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    char data;
    int of_refcounts;
    setup();
    pid = 0;
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(0==do_open("/hello", 0, 0));
    ASSERT(1 == do_open("/tmp", 0, 0));
    ASSERT(1==do_read(0, &data, 1));
    ASSERT('h'==data);
    /*
     * Now call dup2(0,1)
     */
    of_refcounts = fs_get_of_refcounts();
    ASSERT(1==do_dup2(0, 1));
    /*
     * As file descriptor 1 was already used, the total
     * number of refcounts should be the same
     */
    ASSERT(of_refcounts  == fs_get_of_refcounts());
    /*
     * Verify that the new file descriptor 1 works
     */
    ASSERT(1==do_read(1, &data, 1));
    ASSERT('e'==data);
    ASSERT(1==do_read(0, &data, 1));
    ASSERT('l'==data);
    /*
     * Now close fd 1 and make sure that fd 0 is still readable
     */
    ASSERT(0==do_close(1));
    ASSERT(1==do_read(0, &data, 1));
    ASSERT('l'==data);
    ASSERT(0==do_close(0));
    ASSERT(do_read(0, &data, 1)<0);
    return 0;
}

/*
 * Testcase 120
 * Tested function: do_dup2
 * Testcase: open a file and read from it. Then call dup2 where both arguments are the same
 */
int testcase120() {
    inode_t* inode;
    fat16_probe_result = 1;
    ext2_probe_result = 1;
    char data;
    int of_refcounts;
    setup();
    pid = 0;
    fs_fat16_result = &fat16_superblock;
    ASSERT(0==fs_init(0));
    ASSERT(0==do_open("/hello", 0, 0));
    ASSERT(1==do_read(0, &data, 1));
    ASSERT('h'==data);
    /*
     * Now call dup2(0,0)
     */
    of_refcounts = fs_get_of_refcounts();
    ASSERT(0==do_dup2(0, 0));
    /*
     * As file descriptor 0 was already used, the total
     * number of refcounts should be the same
     */
    ASSERT(of_refcounts  == fs_get_of_refcounts());
    /*
     * Make sure that fd 0 is still readable 
     */
    ASSERT(1==do_read(0, &data, 1));
    ASSERT('e'==data);
    ASSERT(0==do_close(0));
    ASSERT(do_read(0, &data, 1)<0);
    return 0;
}

int main() {
    INIT;
    RUN_CASE(1);
    RUN_CASE(2);
    RUN_CASE(3);
    RUN_CASE(4);
    RUN_CASE(5);
    RUN_CASE(6);
    RUN_CASE(7);
    RUN_CASE(8);
    RUN_CASE(9);
    RUN_CASE(10);
    RUN_CASE(11);
    RUN_CASE(12);
    RUN_CASE(13);
    RUN_CASE(14);
    RUN_CASE(15);
    RUN_CASE(16);
    RUN_CASE(17);
    RUN_CASE(18);
    RUN_CASE(19);
    RUN_CASE(20);
    RUN_CASE(21);
    RUN_CASE(22);
    RUN_CASE(23);
    RUN_CASE(24);
    RUN_CASE(25);
    RUN_CASE(26);
    RUN_CASE(27);
    RUN_CASE(28);
    RUN_CASE(29);
    RUN_CASE(30);
    RUN_CASE(31);
    RUN_CASE(32);
    RUN_CASE(33);
    RUN_CASE(34);
    RUN_CASE(35);
    RUN_CASE(36);
    RUN_CASE(37);
    RUN_CASE(38);
    RUN_CASE(39);
    RUN_CASE(40);
    RUN_CASE(41);
    RUN_CASE(42);
    RUN_CASE(43);
    RUN_CASE(44);
    RUN_CASE(45);
    RUN_CASE(46);
    RUN_CASE(47);
    RUN_CASE(48);
    RUN_CASE(49);
    RUN_CASE(50);
    RUN_CASE(51);
    RUN_CASE(52);
    RUN_CASE(53);
    RUN_CASE(54);
    RUN_CASE(55);
    RUN_CASE(56);
    RUN_CASE(57);
    RUN_CASE(58);
    RUN_CASE(59);
    RUN_CASE(60);
    RUN_CASE(61);
    RUN_CASE(62);
    RUN_CASE(63);
    RUN_CASE(64);
    RUN_CASE(65);
    RUN_CASE(66);
    RUN_CASE(67);
    RUN_CASE(68);
    RUN_CASE(69);
    RUN_CASE(70);
    RUN_CASE(71);
    RUN_CASE(72);
    RUN_CASE(73);
    RUN_CASE(74);
    RUN_CASE(75);
    RUN_CASE(76);
    RUN_CASE(77);
    RUN_CASE(78);
    RUN_CASE(79);
    RUN_CASE(80);
    RUN_CASE(81);
    RUN_CASE(82);
    RUN_CASE(83);
    RUN_CASE(84);
    RUN_CASE(85);
    RUN_CASE(86);
    RUN_CASE(87);
    RUN_CASE(88);
    RUN_CASE(89);
    RUN_CASE(90);
    RUN_CASE(91);
    RUN_CASE(92);
    RUN_CASE(93);
    RUN_CASE(94);
    RUN_CASE(95);
    RUN_CASE(96);
    RUN_CASE(97);
    RUN_CASE(98);
    RUN_CASE(99);
    RUN_CASE(100);
    RUN_CASE(101);
    RUN_CASE(102);
    RUN_CASE(103);
    RUN_CASE(104);
    RUN_CASE(105);
    RUN_CASE(106);
    RUN_CASE(107);
    RUN_CASE(108);
    RUN_CASE(109);
    RUN_CASE(110);
    RUN_CASE(111);
    RUN_CASE(112);
    RUN_CASE(113);
    RUN_CASE(114);
    RUN_CASE(115);
    RUN_CASE(116);
    RUN_CASE(117);
    RUN_CASE(118);
    RUN_CASE(119);
    RUN_CASE(120);
    END;
}

