/*
 * test_fs_stack.c
 * Automated assembly test for the file system stack
 *
 * This test assumes that the directory contains test images rdimage0 and rdimage1 which have been prepared using the following
 * commands:
 *
 * bximage rdimage0   --> enter hd, flat, 10,
 * sudo losetup /dev/loop0 rdimage0
 * sudo fdisk /dev/loop0   --> create a new partition starting at sector 2048 of type 83
 * sudo losetup /dev/loop1 -o $((512*2048)) rdimage0
 * sudo mkfs -t ext2 -b 1024 -O none /dev/loop1
 * sudo losetup -d /dev/loop1
 * sudo losetup -d /dev/loop0
 * cp rdimage0 tmp
 * dd if=tmp of=rdimage0 ibs=512 obs=512 skip=2048
 * sudo mount rdimage0 /mnt -o loop
 * sudo mkdir /mnt/tmp
 * sudo -s
 * echo "hello" > /mnt/hello
 * umount /mnt
 * exit
 *
 * dd if=tmp of=rdimage1 ibs=512 obs=512 skip=2048
 * sudo mount rdimage1 /mnt -o loop
 * sudo -s
 * echo "hello" > /mnt/mounted
 * mkdir /mnt/dir
 * umount /mnt
 * exit
 *
 *
 * Also make sure to adapt the constants TEST_IMAGE0_SIZE and TEST_IMAGE1_SIZE below
 */

#include "mm.h"
#include "pm.h"
#include "vga.h"
#include "kunit.h"
#include "lib/unistd.h"
#include "drivers.h"
#include "lib/fcntl.h"
#include "lib/sys/stat.h"
#include "fs.h"

extern int __fs_loglevel;
extern int __ext2_loglevel;

/*
 * Size of the test hd image
 */
#define TEST_IMAGE0_SIZE 9273344
#define TEST_IMAGE1_SIZE 9273344


/*
 * Some definitions for POSIX open/read
 */
extern int open(const char *__file, int __oflag, ...);
extern ssize_t read(int fd, void *buf, size_t count);


/*
 * Test images. Image 0 is for the ram disk with minor device number
 * 0, image 1 is the ram disk with minor number 1
 */
static void* image0;
static void* image1;

/*
 * This needs to match the value defined in timer.h
 */
#define HZ 100

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

int net_socket_setoption(socket_t* socket, int level, int option, void* option_value, unsigned int option_len) {
    return 0;
}

socket_t* net_socket_create(int domain, int type, int proto) {
    return 0;
}

int net_socket_bind(socket_t* socket, struct sockaddr* address, int addrlen) {
    return 0;
}

int net_socket_accept(socket_t* socket, struct sockaddr* addr, socklen_t* addrlen, socket_t** new_socket) {
    return 0;
}

int net_ioctl(socket_t* socket, unsigned int cmd, void* arg) {
    return 0;
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

int net_socket_select(socket_t* socket, int read, int write, semaphore_t* sem) {
    return 0;
}

int net_socket_getaddr(socket_t* socket, struct sockaddr* laddr, struct sockaddr* faddr, unsigned int* addrlen) {
    return 0;
}

int net_socket_cancel_select(socket_t* socket, semaphore_t* sem) {
    return 0;
}

void* kmalloc_aligned(u32 size, u32 alignment) {
    return 0;
}
/*
 * Stubs for kmalloc/kfree
 */
void* kmalloc(u32 size) {
    return (void*) malloc(size);
}
void kfree(void* addr) {
    free(addr);
}

/*
 * Stub for kputchar
 * Set do_putchar to 1 to see inode
 * cache statistics
 */
static int do_putchar = 0;
void win_putchar(win_t* win, u8 c) {
    if (do_putchar == 1)
        printf("%c", c);
}

/*
 * Stub for do_time in rtc.c
 */
time_t do_time(time_t* ptr) {
    return time(ptr);
}

uid_t do_geteuid() {
    return 0;
}

gid_t do_getegid() {
    return 0;
}

int net_socket_connect(socket_t* socket, struct sockaddr* addr, int addrlen) {
    return 0;
};

/*
 * Stub for trap
 */
void trap() {
    printf("Trap condition occured!\n");
    _exit(1);
}

void cond_init(cond_t* cond) {

}

static int cond_broadcast_called = 0;
void cond_broadcast(cond_t* cond) {
    cond_broadcast_called = 1;
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


/*
 * Stubs for spinlocks
 */
static u32 ie = 1;
void spinlock_init(spinlock_t* lock) {
    *((u32*) lock) = 0;
}

void spinlock_get(spinlock_t* lock, u32* flags) {
    if (*((u32*) lock) == 1) {
        printf(
                "----------- Spinlock requested which is not available! ----------------\n");
        _exit(1);
    }
    *((u32*) lock) = 1;
    *flags = ie;
}
void spinlock_release(spinlock_t* lock, u32* flags) {
    *((u32*) lock) = 0;
    ie = *flags;
}

/*
 * Stubs for semaphores
 */
void sem_init(semaphore_t* sem, u32 value) {
    sem->value = value;
}

void __sem_down(semaphore_t* sem, char* file, int line) {
    if (0 == sem->value) {
        printf(
                "----------- Mutex requested which is not available! ----------------\n");
        _exit(1);
    }
    if (0 == ie) {
        printf(
                "----------- Down operation on semaphore with interrupts disabled! ----------------\n");
        _exit(1);
    }
    sem->value--;
}

void sem_up(semaphore_t* sem) {
    sem->value++;
}

void mutex_up(semaphore_t* mutex)  {
    mutex->value = 1;
}

int __sem_down_intr(semaphore_t* sem, char* file, int line) {
    if (0 == sem->value) {
        printf(
                "----------- Mutex requested which is not available! ----------------\n");
        _exit(1);
    }
    if (0 == ie) {
        printf(
                "----------- Down operation on semaphore with interrupts disabled! ----------------\n");
        _exit(1);
    }
    sem->value--;
    return 0;
}

int __sem_down_timed(semaphore_t* sem, char* file, int line, u32 timeout) {
    if (0 == sem->value) {
        printf(
                "----------- Mutex requested which is not available! ----------------\n");
        _exit(1);
    }
    if (0 == ie) {
        printf(
                "----------- Down operation on semaphore with interrupts disabled! ----------------\n");
        _exit(1);
    }
    sem->value--;
    return 0;
}

/*
 * Stub for 8193 init
 */
void nic_8139_init() {

}

/*
 * Stub for rtc_init
 */
void rtc_init() {

}

int pm_get_task_id() {
    return 0;
}

int do_pthread_kill(u32 task_id, int sig_no) {
    return 0;
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

void pm_attach_tty(dev_t tty)  {

}

dev_t pm_get_cterm() {
    return 0;
}

void net_socket_close(socket_t* socket) {

}

/*
 * Implementation of read/write locks - taken from locks.c
 */

/*
 * Initialize a read-write lock
 * Parameters:
 * @rw_lock - the read-write-lock to use
 */
void rw_lock_init(rw_lock_t* rw_lock) {
    sem_init(&rw_lock->read_count_mutex, 1);
    sem_init(&rw_lock->wrt_mutex, 1);
    rw_lock->readers = 0;
}

/*
 * This function gets a read lock
 * Parameter:
 * @rw_lock - the read write lock to use
 */
void __rw_lock_get_read_lock(rw_lock_t* rw_lock, char* file, int line) {
    sem_down(&rw_lock->read_count_mutex);
    rw_lock->readers++;
    if (1 == rw_lock->readers) {
        sem_down(&rw_lock->wrt_mutex);
    }
    mutex_up(&rw_lock->read_count_mutex);
}


/*
 * This function releases a read lock
 * Parameter:
 * @rw_lock - the read write lock to use
 */
void rw_lock_release_read_lock(rw_lock_t* rw_lock) {
    sem_down(&rw_lock->read_count_mutex);
    rw_lock->readers--;
    if (0 == rw_lock->readers) {
        mutex_up(&rw_lock->wrt_mutex);
    }
    mutex_up(&rw_lock->read_count_mutex);
}

/*
 * Acquire a write lock
 * Parameters:
 * @rw_lock - the lock to be acquired
 */
void __rw_lock_get_write_lock(rw_lock_t* rw_lock, char* file, int line) {
    sem_down(&rw_lock->wrt_mutex);
}

/*
 * Release a write lock
 * Parameters:
 * @rw_lock - the lock to be acquired
 */
void rw_lock_release_write_lock(rw_lock_t* rw_lock) {
    mutex_up(&rw_lock->wrt_mutex);
}


/*
 * Stub for pm_get_pid()
 */
int pm_get_pid() {
    return 0;
}

/*
 * Stub for tty_init
 */
void tty_init() {

}

/*
 * Stub for pci_init
 */
void pci_init() {

}

/*
 * Stub for pata_init
 */
void pata_init() {

}

/*
 * Stub for ahci_init
 */
void ahci_init() {

}

int tty_getpgrp(minor_dev_t minor) {
    return 1;
}

int tty_setpgrp(minor_dev_t minor, pid_t pgrp) {
    return 0;
}

/***************************************
 * Stubs for ramdisk driver start here
 ***************************************/


/*
 * Flag to keep track of whether device is open
 */
static int rd_open0 = 0;
static int rd_open1 = 0;

/*
 * Open ramdisk
 */
int ramdisk_open(minor_dev_t minor) {
    if (minor > 1)
        return -1;
    if (minor==0)
        rd_open0 = 1;
    else
        rd_open1 = 1;
    return 0;
}

/*
 * Close ramdisk
 */
int ramdisk_close(minor_dev_t minor) {
    if (minor > 1)
        return -1;
    if (minor==0)
        rd_open0 = 0;
    else
        rd_open1 = 0;
    return 0;
}
/*
 * Read from ramdisk
 */
ssize_t ramdisk_read(u8 minor, ssize_t blocks, ssize_t first_block, void* buffer) {
    if ((0==rd_open0) && (minor==0)) {
        printf("----- RAM disk not open! ----\n");
        _exit(1);
    }
    if ((0==rd_open1) && (minor==1)) {
        printf("----- RAM disk not open! ----\n");
        _exit(1);
    }
    if (minor==0)
        memcpy(buffer, image0 + first_block*1024, blocks*1024);
    else if (minor==1)
        memcpy(buffer, image1 + first_block*1024, blocks*1024);
    else
        return -EIO;
    return blocks;
}

/*
 * Write to ramdisk
 */
ssize_t ramdisk_write(u8 minor, ssize_t blocks, ssize_t first_block, void* buffer) {
    if ((0==rd_open0) && (minor==0)) {
        printf("----- RAM disk not open! ----\n");
        _exit(1);
    }
    if ((0==rd_open1) && (minor==1)) {
        printf("----- RAM disk not open! ----\n");
        _exit(1);
    }
    if (minor==0)
        memcpy(image0 + first_block*1024, buffer, blocks*1024);
    else if (minor==1)
        memcpy(image1 + first_block*1024, buffer, blocks*1024);
    else
        return -EIO;
    return blocks;
}

static blk_dev_ops_t rd_ops = {ramdisk_open, ramdisk_close, ramdisk_read, ramdisk_write };

void ramdisk_init() {
    int image0_fd;
    int image1_fd;
    image0 = (void*) malloc(TEST_IMAGE0_SIZE);
    if (0 == image0) {
        printf("Could not allocate memory for test image, bailing out\n");
        exit(1);
    }
    image1 = (void*) malloc(TEST_IMAGE1_SIZE);
    if (0 == image1) {
        printf("Could not allocate memory for test image, bailing out\n");
        exit(1);
    }
    image0_fd = open("./rdimage0", O_RDONLY);
    if (image0_fd <= 0) {
        printf("Could not open image file rdimage0 for testing\n");
        exit(1);
    }
    read(image0_fd, image0, TEST_IMAGE0_SIZE);
    image1_fd = open("./rdimage1", O_RDONLY);
    if (image1_fd <= 0) {
        printf("Could not open image file rdimage1 for testing\n");
        exit(1);
    }
    read(image1_fd, image1, TEST_IMAGE0_SIZE);
    /*
     * Register with dm
     */
    dm_register_blk_dev(MAJOR_RAMDISK, &rd_ops);
}

/*
 * Save test images to disk again
 */
void save() {
    int image0_fd;
    int image1_fd;
    image0_fd = open("./rdimage0.new", O_RDWR + O_CREAT);
    if (image0_fd < 0) {
        perror("open failed for rdimage0.new");
        exit(1);
    }
    write(image0_fd, image0, TEST_IMAGE0_SIZE);
    image1_fd = open("./rdimage1.new", O_RDWR + O_CREAT);
    if (image1_fd < 0) {
        perror("open failed for rdimage1.new");
        exit(1);
    }
    write(image1_fd, image1, TEST_IMAGE0_SIZE);
    /*
     * Register with dm
     */
    close(image0_fd);
    close(image1_fd);
}

/*
 * Testcase 1: initialize device driver manager
 */
int testcase1() {
    dm_init();
    return 0;
}

/*
 * Testcase 2: initialize file system
 */
int testcase2() {
    ASSERT(0==fs_init(DEVICE_NONE));
    ASSERT(0==fs_ext2_print_cache_info());
    return 0;
}

/*
 * Testcase 3: mount root file system and stat /tmp and /tmp/.
 */
int testcase3() {
    struct __ctOS_stat mystat;
    ASSERT(0 == do_mount("/", DEVICE(MAJOR_RAMDISK, 0), "ext2"));
    ASSERT(2 == fs_ext2_print_cache_info());
    ASSERT(0 == do_stat("/tmp", &mystat));
    /*
     * the directory /tmp should have link count 2 (entry in / and /tmp/.)
     */
    ASSERT(2 == mystat.st_nlink);
    ASSERT(0 == do_stat("/tmp/.", &mystat));
    return 0;
}

/*
 * Testcase 4: open a file
 */
int testcase4() {
    ASSERT(0 == do_open("/hello", 0, 0));
    ASSERT(4 == fs_ext2_print_cache_info());
    /*
     * It should now not be possible to
     * unmount the root FS
     */
    ASSERT(do_unmount("/"));
    return 0;
}

/*
 * Testcase 5: read from a file. Then seek back, write to it and verify that write was successful.
 * Finally write back original content
 */
int testcase5() {
    char data[6];
    /*
     * First read once
     */
    ASSERT(5 == do_read(0, (void*) data, 5));
    data[5]=0;
    ASSERT(0 == strcmp("hello", data));
    /*
     * Seek and write
     */
    ASSERT(0 == do_lseek(0, 0, 0));
    ASSERT(5 == do_write(0, "aaaaa", 5));
    do_lseek(0, 0, 0);
    ASSERT(5 == do_read(0, data, 5));
    ASSERT(0 == strcmp("aaaaa", data));
    /*
     * Write back original data
     */
    do_lseek(0, 0, 0);
    ASSERT(5 == do_write(0, "hello", 5));
    ASSERT(4 == fs_ext2_print_cache_info());
    return 0;
}

/*
 * Testcase 6: close the file
 */
int testcase6() {
    ASSERT(0 == do_close(0));
    ASSERT(0 == fs_print_open_files());
    ASSERT(2 == fs_ext2_print_cache_info());
    return 0;
}

/*
 * Testcase 7: unmount root file system
 */
int testcase7() {
    ASSERT(0 == do_unmount("/"));
    ASSERT(0 == fs_ext2_print_cache_info());
    return 0;
}

/*
 * Testcase 8: mount root file system again, then mount
 * second instance of ram disk as /tmp. Verify that the mount point /tmp
 * cannot be removed
 */
int testcase8() {
    int rc;
    ASSERT(0 == do_mount("/", DEVICE(MAJOR_RAMDISK, 0), "ext2"));
    ASSERT(2 == fs_ext2_print_cache_info());
    ASSERT(0 == do_mount("/tmp", DEVICE(MAJOR_RAMDISK, 1), "ext2"));
    ASSERT(6 == fs_ext2_print_cache_info());
    /*
     * Should not be possible to unmount root now
     */
    ASSERT(do_unmount("/"));
    /*
     * nor to delete /tmp - this should return EBUSY
     */
    do_putchar = 0;
    __ext2_loglevel = 0;
    rc = do_unlink("/tmp");
    __ext2_loglevel = 0;
    do_putchar = 0;
    ASSERT(rc);
    return 0;
}

/*
 * Testcase 9: open file /tmp/mounted on mounted area of file system
 */
int testcase9() {
    ASSERT(0 == do_open("/tmp/mounted", 0, 0));
    ASSERT(8 == fs_ext2_print_cache_info());
    /*
     * Should not be possible to unmount now
     */
    ASSERT(do_unmount("/tmp"));
    return 0;
}

/*
 * Testcase 10: read from this file
 */
int testcase10() {
    char data[6];
    ASSERT(5 == do_read(0, (void*) data, 5));
    data[5]=0;
    ASSERT(0 == strcmp("hello", data));
    ASSERT(8 == fs_ext2_print_cache_info());
    return 0;
}

/*
 * Testcase 11: add a new file in /tmp by calling open with O_CREAT. Then use stat to verify that
 * file permissions are as expected
 */
int testcase11() {
    int fd;
    struct __ctOS_stat mystat;
    fd = do_open("/tmp/new", O_CREAT, 07777);
    ASSERT(10 == fs_ext2_print_cache_info());
    ASSERT(fd == 1);
    /*
     * As umask is 022, actual permissions should be 07755
     */
    ASSERT(0 == do_stat("/tmp/new", &mystat));
    ASSERT((mystat.st_mode & 07777) == 07755);
    /*
     * Link count for this file should be 1
     */
    ASSERT(1 == mystat.st_nlink);
    do_close(fd);
    return 0;
}

/*
 * Testcase 12: write to this file. We write to the file twice. First we place the string "new file"
 * at the beginning of the file, then we place the string "second write" at block 200 of the file
 * so that we actually create a hole
 */
int testcase12() {
    int fd;
    fd = do_open("/tmp/new", 0, 0);
    ASSERT(fd == 1);
    ASSERT(do_write(fd, "new file", strlen("new file")));
    ASSERT(do_lseek(fd, 200*1024, SEEK_SET));
    ASSERT(do_write(fd, "second write", strlen("second write")));
    do_close(fd);
    ASSERT(8 == fs_ext2_print_cache_info());
    return 0;
}

/*
 * Testcase 13: open file and read from it. Then unlink it and close it. Verify that
 * - we can still read from the file even though it has been removed
 * - the file cannot be opened any more
 */
int testcase13() {
    int fd;
    char buffer[256];
    memset((void*) buffer, 0, 256);
    fd = do_open("/tmp/new", 0, 0);
    ASSERT(fd);
    ASSERT(0 == do_unlink("/tmp/new"));
    ASSERT(strlen("new file")==do_read(fd, buffer, strlen("new file")));
    ASSERT(0 == strncmp("new file", buffer, strlen("new file")));
    memset((void*) buffer, 0, 256);
    ASSERT(do_lseek(fd, 200*1024, SEEK_SET));
    ASSERT(strlen("second write")==do_read(fd, buffer, strlen("second write")));
    ASSERT(0 == strncmp("second write", buffer, strlen("second write")));
    do_close(fd);
    fd = do_open("/tmp/new", 0, 0);
    if (fd >= 0)
        do_close(fd);
    ASSERT(8 == fs_ext2_print_cache_info());
    ASSERT(fd == -116);
    return 0;
}

/*
 * Testcase 14: change to /tmp and make sure that "mounted" can be opened using the relative path
 * name only. Also test getcwd
 */
int testcase14() {
    int fd;
    char buffer[128];
    ASSERT(0 == do_chdir("tmp"));
    /*
     * This should have added a reference to /tmp to our process
     */
    ASSERT(10 == fs_ext2_print_cache_info());
    fd = do_open("mounted", 0, 0);
    ASSERT(fd);
    do_close(fd);
    ASSERT(10 == fs_ext2_print_cache_info());
    /*
     * Get current working directory
     */
    ASSERT(0 == do_getcwd(buffer, 128));
    ASSERT(10 == fs_ext2_print_cache_info());
    ASSERT(0 == strcmp(buffer, "/tmp"));
    ASSERT(0 == do_chdir(".."));
    ASSERT(8 == fs_ext2_print_cache_info());
    return 0;
}

/*
 * Testcase 15: make sure that /tmp/.. is /
 */
int testcase15() {
    int fd;
    inode_t* inode1;
    inode_t* inode2;
    inode1 = fs_get_inode_for_name("/");
    ASSERT(inode1);
    ASSERT(10 == fs_ext2_print_cache_info());
    inode2 = fs_get_inode_for_name("/tmp/..");
    ASSERT(inode2);
    ASSERT(12 == fs_ext2_print_cache_info());
    ASSERT(inode1->inode_nr==inode2->inode_nr);
    ASSERT(inode1->dev==inode2->dev);
    inode1->iops->inode_release(inode1);
    inode2->iops->inode_release(inode2);
    ASSERT(8 == fs_ext2_print_cache_info());
    return 0;
}


/*
 * Testcase 16: unmount both file systems
 */
int testcase16() {
    do_close(0);
    ASSERT(0 == do_unmount("/tmp"));
    ASSERT(0 == do_unmount("/"));
    ASSERT(0 == fs_ext2_print_cache_info());
    return 0;
}

/*
 * Testcase 17: initialize device driver manager, mount root filesystem
 * and open a file. Then close the file twice
 */
int testcase17() {
    dm_init();
    ASSERT(0 == fs_init(DEVICE_NONE));
    ASSERT(0 == do_mount("/", DEVICE(MAJOR_RAMDISK, 0), "ext2"));
    ASSERT(0 == do_open("/hello", 0, 0));
    ASSERT(0 == do_close(0));
    ASSERT(do_close(0)==-EBADF);
    /*
     * Should not be possible to remove /
     */
    ASSERT(130 == do_unlink("/"));
    return 0;
}

/*
 * Testcase 18: open the file hello and verify that it is not empty. Then open with O_TRUNC again and
 * verify that file is empty afterwards
 */
int testcase18() {
    char c;
    ASSERT(0 == do_open("/hello", 0, 0));
    ASSERT(1 == do_read(0, &c, 1));
    ASSERT(0 == do_close(0));
    /*
     * Open with O_TRUNC and close again - should truncate file
     */
    __fs_loglevel = 0;
    ASSERT(0 == do_open("/hello", O_TRUNC + O_WRONLY, 0));
    __fs_loglevel = 0;
    ASSERT(0 == do_close(0));
    /*
     * Check that file is in fact empty
     */
    ASSERT(0 == do_open("/hello", 0, 0));
    ASSERT(0 == do_read(0, &c, 1));
    ASSERT(0 == do_close(0));
    return 0;
}

/*
 * Testcase 19: test writing to a file with O_APPEND. We first write one character to the file hello
 * (which we have emptied in the previous test case). Then we close the file again, open with O_APPEND
 * and write a second character. Finally we close again, open for read and read both bytes
 */
int testcase19() {
    char c[2];
    ASSERT(0 == do_open("/hello", 0, 0));
    c[0]='a';
    ASSERT(1 == do_write(0, c, 1));
    ASSERT(0 == do_close(0));
    ASSERT(0 == do_open("/hello", O_WRONLY+O_APPEND, 0));
    c[0]='b';
    __fs_loglevel = 0;
    ASSERT(1 == do_write(0, c, 1));
    __fs_loglevel = 0;
    ASSERT(0 == do_close(0));
    ASSERT(0 == do_open("/hello", 0, 0));
    ASSERT(2 == do_read(0, c, 2));
    ASSERT('a' == c[0]);
    ASSERT('b' == c[1]);
    ASSERT(0 == do_close(0));
    return 0;

}

/*
 * Testcase 20: open an existing file with O_CREAT and verify that in can be opened as usual
 */
int testcase20() {
    ASSERT(0 == do_open("/hello", O_CREAT, 0));
    ASSERT(0 == do_close(0));
    return 0;
}

/*
 * Testcase 21: open an existing file with O_CREAT + O_EXCL and verify that this returns -EEXIST
 */
int testcase21() {
    ASSERT(-130 == do_open("/hello", O_CREAT + O_EXCL, 0));
   return 0;
}

/*
 * Testcase 22: open a new file with O_CREAT and O_EXCL and close it again. Then stat the file
 */
int testcase22() {
    struct __ctOS_stat mystat;
    ASSERT(0 == do_open("/hello1", O_CREAT + O_EXCL, 0));
    ASSERT(0 == do_close(0));
    ASSERT(0 == do_stat("/hello1", &mystat));
    ASSERT(1 == mystat.st_nlink);
    return 0;
}

/*
 * Testcase 23: open a new file with O_CREAT and close it again. Then stat the file
 */
int testcase23() {
    struct __ctOS_stat mystat;
    ASSERT(0 == do_open("/hello2", O_CREAT, 0));
    ASSERT(0 == do_close(0));
    ASSERT(0 == do_stat("/hello2", &mystat));
    return 0;
}

/*
 * Testcase 24: set the access and modification time of a file with utime and then use
 * stat to verify results
 */
int testcase24() {
    struct __ctOS_stat mystat;
    struct utimbuf times;
    times.actime = 100;
    times.modtime = 200;
    /*
     * do utime
     */
    ASSERT(0 == do_utime("/hello", &times));
    /*
     * followed by stat
     */
    ASSERT(0 == do_stat("/hello", &mystat));
    ASSERT(100 == mystat.st_atime);
    ASSERT(200 == mystat.st_mtime);
    return 0;
}

/*
 * Testcase 25: set the access rights of a file with chmod and verify results using stat
 */
int testcase25() {
    struct __ctOS_stat mystat;
    unsigned short old_mode;
    ASSERT(0 == do_stat("/hello", &mystat));
    old_mode = mystat.st_mode;
    /*
     * do chmod
     */
    ASSERT(0 == do_chmod("/hello", 0111));
    /*
     * followed by another stat
     */
    ASSERT(0 == do_stat("/hello", &mystat));
    ASSERT(0111 == (mystat.st_mode & 0777));
    ASSERT((old_mode & 0170000) == (mystat.st_mode & 0170000));
    return 0;
}

/*
 * Testcase 26: try to create a directory which already exists - should return -EEXIST
 */
int testcase26() {
    ASSERT(-130 == do_mkdir("/tmp", 0));
    return 0;
}

/*
 * Testcase 27: create a new directory and stat it
 */
int testcase27() {
    ino_t new_dir_inode;
    ino_t tmp_inode;
    struct __ctOS_stat mystat;
    int fd;
    int ref_count;
    /*
     * Remember reference count
     */
    ref_count = fs_ext2_print_cache_info();
    ASSERT(0 == do_stat("/tmp", &mystat));
    tmp_inode = mystat.st_ino;
    ASSERT(0 == do_mkdir("/tmp/test", 0));
    ASSERT(0 == do_stat("/tmp/test", &mystat));
    /*
     * Check that file is actually a directory
     */
    ASSERT(S_ISDIR(mystat.st_mode));
    /*
     * and remember inode number
     */
    new_dir_inode = mystat.st_ino;
    /*
     * We expect that /tmp/test is referenced by /test and by /tmp/test/.
     */
    ASSERT(2 == mystat.st_nlink);
    /*
     * Now make sure that there is an entry "."
     */
    ASSERT(0 == do_stat("/tmp/test/.", &mystat));
    ASSERT(mystat.st_ino == new_dir_inode);
    /*
     * and ..
     */
    ASSERT(0 == do_stat("/tmp/test/..", &mystat));
    ASSERT(mystat.st_ino == tmp_inode);
    /*
     * Reference count should not have changed
     */
    ASSERT(ref_count == fs_ext2_print_cache_info());
    /*
     * and adding the same directory again should give an error
     */
    ASSERT(-130 == do_mkdir("/tmp/test", 0));
    /*
     * Add a file within the directory
     */
    ASSERT((fd = do_open("/tmp/test/myfile", O_CREAT, 0777)) >= 0);
    do_close(fd);
    ASSERT(0 == do_stat("/tmp/test/myfile", &mystat));
    /*
     * An attempt to remove the directory should fail now with EEXIST
     */
    ASSERT(130 == do_unlink("/tmp/test"));
    /*
     * Remove file
     */
    ASSERT(0 == do_unlink("/tmp/test/myfile"));
    /*
     * It should not be possible to remove /tmp/test/..
     */
    ASSERT(107 == do_unlink("/tmp/test/.."));
    /*
     * Now remove directory again
     */
    ASSERT(0 == do_unlink("/tmp/test"));
    ASSERT(ref_count == fs_ext2_print_cache_info());
    return 0;
}

/*
 * Testcase 28: create a new directory and stat it - use name with trailing /
 */
int testcase28() {
    ino_t new_dir_inode;
    ino_t tmp_inode;
    struct __ctOS_stat mystat;
    int ref_count;
    /*
     * Remember reference count
     */
    ref_count = fs_ext2_print_cache_info();
    ASSERT(0 == do_stat("/tmp", &mystat));
    tmp_inode = mystat.st_ino;
    ASSERT(0 == do_mkdir("/tmp/test1/", 0));
    ASSERT(0 == do_stat("/tmp/test1", &mystat));
    /*
     * Check that file is actually a directory
     */
    ASSERT(S_ISDIR(mystat.st_mode));
    /*
     * and remember inode number
     */
    new_dir_inode = mystat.st_ino;
    /*
     * Now make sure that there is an entry "."
     */
    ASSERT(0 == do_stat("/tmp/test1/.", &mystat));
    ASSERT(mystat.st_ino == new_dir_inode);
    /*
     * and ..
     */
    ASSERT(0 == do_stat("/tmp/test1/..", &mystat));
    ASSERT(mystat.st_ino == tmp_inode);
    /*
     * Reference count should not have changed
     */
    ASSERT(ref_count == fs_ext2_print_cache_info());
    return 0;
}

/*
 * Testcase 29: rename a file within the same directory
 */
int testcase29() {
    struct __ctOS_stat mystat;
    int ref_count;
    int fd;
    /*
     * Remember reference count
     */
    ref_count = fs_ext2_print_cache_info();
    /*
     * Create test file and verify existence
     */
    ASSERT((fd = do_open("/rename1", O_CREAT + O_EXCL, 0)) >= 0);
    ASSERT(0 == do_close(fd));
    ASSERT(0 == do_stat("/rename1", &mystat));
    /*
     * Rename file
     */
    ASSERT(0 == do_rename("/rename1", "/rename2"));
    ASSERT(0 == do_stat("/rename2", &mystat));
    ASSERT(do_stat("/rename1", &mystat) < 0);
    /*
     * Check reference count
     */
    ASSERT(ref_count == fs_ext2_print_cache_info());
    return 0;
}

/*
 * Testcase 31: rename a file to itself - use file rename2 from testcase 28
 */
int testcase31() {
    struct __ctOS_stat mystat;
    int ref_count;
    int fd;
    /*
     * Remember reference count
     */
    ref_count = fs_ext2_print_cache_info();
    /*
     * Verify existence of test file
     */
    ASSERT(0 == do_stat("/rename2", &mystat));
    /*
     * Rename file
     */
    ASSERT(0 == do_rename("/rename2", "/rename2"));
    ASSERT(0 == do_stat("/rename2", &mystat));
    /*
     * Check reference count
     */
    ASSERT(ref_count == fs_ext2_print_cache_info());
    /*
     * and link count
     */
    ASSERT(1 == mystat.st_nlink);
    return 0;

}

/*
 * Testcase 30: try to rename a file which does not exist
 */
int testcase30() {
    struct __ctOS_stat mystat;
    int ref_count;
    /*
     * Remember reference count
     */
    ref_count = fs_ext2_print_cache_info();
    /*
     * Make sure that file does not exist
     */
    ASSERT(do_stat("youdonotexist", &mystat));
    /*
     * and try to rename it - should return ENOENT
     */
    ASSERT(-116 == do_rename("youdonotexist", "newfile"));
    /*
     * Check reference count
     */
    ASSERT(ref_count == fs_ext2_print_cache_info());
    return 0;
}

/*
 * Testcase 32: try to rename a file to a directory
 */
int testcase32() {
    struct __ctOS_stat mystat;
    int ref_count;
    /*
     * Remember reference count
     */
    ref_count = fs_ext2_print_cache_info();
    /*
     * Make sure that file exists
     */
    ASSERT(0 == do_stat("/rename2", &mystat));
    /*
     * and try to rename it - should return EISDIR
     */
    ASSERT(-133 == do_rename("/rename2", "/tmp"));
    /*
     * Check reference count
     */
    ASSERT(ref_count == fs_ext2_print_cache_info());
    return 0;
}

/*
 * Testcase 33: rename to an existing file
 */
int testcase33() {
    int fd;
    int ref_count;
    char buffer[256];
    /*
     * Write something to rename2 to be able to distinguish it later
     */
    fd = do_open("/rename2", O_RDWR, 0);
    ASSERT(fd >= 0);
    ASSERT(do_write(fd, "hello", strlen("hello") + 1));
    do_close(fd);
    /*
     * Now create a file rename1 and write to it
     */
    ASSERT((fd = do_open("/rename1", O_CREAT + O_EXCL, 0)) >= 0);
    ASSERT(do_write(fd, "rename1", strlen("rename1") + 1));
    ASSERT(0 == do_close(fd));
    /*
     * Remember reference count
     */
    ref_count = fs_ext2_print_cache_info();
    /*
     * Rename
     */
    ASSERT(0 == do_rename("/rename1", "/rename2"));
    /*
     * Check reference count
     */
    ASSERT(ref_count == fs_ext2_print_cache_info());
    /*
     * check that rename2 now contains "rename1"
     */
    memset((void*) buffer, 0, 256);
    ASSERT((fd = do_open("/rename2", O_RDONLY, 0)) >= 0);
    ASSERT(do_read(fd, buffer, 256));
    ASSERT(0 == strcmp(buffer, "rename1"));
    do_close(fd);
    return 0;
}

/*
 * Testcase 34: rename an empty directory within the same parent directory
 */
int testcase34() {
    struct __ctOS_stat mystat;
    int ref_count;
    int parent_inode_nr;
    int fd;
    /*
     * Create test directory structure
     */
    ASSERT(0 == do_mkdir("/mydir", 0777));
    ASSERT(0 == do_stat("/mydir", &mystat));
    parent_inode_nr = mystat.st_ino;
    ASSERT(0 == do_mkdir("/mydir/subdir1", 0777));
    ASSERT(0 == do_stat("/mydir/subdir1", &mystat));
    /*
     * Remember reference count
     */
    ref_count = fs_ext2_print_cache_info();
    /*
     * Rename file
     */
    do_putchar = 1;
    ASSERT(0 == do_rename("/mydir/subdir1", "/mydir/subdir2"));
    do_putchar = 0;
    /*
     * Check reference count
     */
    ASSERT(ref_count == fs_ext2_print_cache_info());
    /*
     * and result of operation
     */
    __fs_loglevel = 0;
    __ext2_loglevel = 0;
    do_putchar = 1;
    ASSERT(0 == do_stat("/mydir/subdir2", &mystat));
    do_putchar = 0;
    ASSERT(do_stat("/mydir/subdir1", &mystat) < 0);
    /*
     * Verify that the parent directory is still correctly linked
     * via the ".." entry
     */
    ASSERT(0 == do_stat("/mydir/subdir2/..", &mystat));
    ASSERT(parent_inode_nr == mystat.st_ino);
    return 0;
}

/*
 * Testcase 35: rename an empty directory - different parent directories
 * We use /mydir/subdir2 from the previous testcase
 */
int testcase35() {
    struct __ctOS_stat mystat;
    int ref_count;
    int parent_inode_nr;
    int fd;
    /*
     * Create test directory structure
     */
    ASSERT(0 == do_mkdir("/myseconddir", 0777));
    ASSERT(0 == do_stat("/myseconddir", &mystat));
    parent_inode_nr = mystat.st_ino;
    ASSERT(0 == do_stat("/mydir/subdir2", &mystat));
    /*
     * Remember reference count
     */
    ref_count = fs_ext2_print_cache_info();
    /*
     * Rename file
     */
    ASSERT(0 == do_rename("/mydir/subdir2", "/myseconddir/subdir1"));
    /*
     * Check reference count
     */
    ASSERT(ref_count == fs_ext2_print_cache_info());
    /*
     * and result of operation
     */
    ASSERT(0 == do_stat("/myseconddir/subdir1", &mystat));
    ASSERT(do_stat("/mydir/subdir2", &mystat) < 0);
    ASSERT(2 == mystat.st_nlink);
    /*
     * Verify that the parent directory is still correctly linked
     * via the ".." entry
     */
    ASSERT(0 == do_stat("/myseconddir/subdir1/..", &mystat));
    ASSERT(parent_inode_nr == mystat.st_ino);
    /*
     * myseconddir is now referenced by its entry in / and the .. entry
     * in subdir1 as well by the . entry in myseconddir, so its link count should be three
     */
    ASSERT(3 == mystat.st_nlink);
    return 0;
}

/*
 * Testcase 36: rename a non-empty directory - different parent directories
 * Target directory exists but is empty.
 * Our target directory is the empty directory /myseconddir/subdir1 from the previous
 * example
 */
int testcase36() {
    struct __ctOS_stat mystat;
    int ref_count;
    int parent_inode_nr;
    int fd;
    /*
     * Create test directory structure
     */
    ASSERT(0 == do_stat("/myseconddir", &mystat));
    parent_inode_nr = mystat.st_ino;
    ASSERT(0 == do_stat("/myseconddir/subdir1", &mystat));
    ASSERT(0 == do_mkdir("/mydir/newsubdir", 0777));
    ASSERT(0 == do_stat("/mydir/newsubdir", &mystat));
    fd = do_open("/mydir/newsubdir/file", O_CREAT, 0777);
    ASSERT(fd >= 0);
    close(fd);
    ASSERT(0 == do_stat("/mydir/newsubdir/file", &mystat));
    /*
     * Remember reference count
     */
    ref_count = fs_ext2_print_cache_info();
    /*
     * Rename file
     */
    ASSERT(0 == do_rename("/mydir/newsubdir", "/myseconddir/subdir1"));
    /*
     * Check reference count
     */
    ASSERT(ref_count == fs_ext2_print_cache_info());
    /*
     * and result of operation
     */
    ASSERT(0 == do_stat("/myseconddir/subdir1", &mystat));
    ASSERT(0 == do_stat("/myseconddir/subdir1/file", &mystat));
    ASSERT(do_stat("/mydir/newsubdir", &mystat) < 0);
    /*
     * Verify that the parent directory is still correctly linked
     * via the ".." entry
     */
    ASSERT(0 == do_stat("/myseconddir/subdir1/..", &mystat));
    ASSERT(parent_inode_nr == mystat.st_ino);
    ASSERT(3 == mystat.st_nlink);
    return 0;
}

/*
 * Testcase 37: rename a non-empty directory - target directory not empty
 */
int testcase37() {
    struct __ctOS_stat mystat;
    int ref_count;
    int parent_inode_nr;
    int fd;
    /*
     * Create test directory structure
     */
    ASSERT(0 == do_stat("/myseconddir", &mystat));
    ASSERT(0 == do_stat("/myseconddir/subdir1", &mystat));
    ASSERT(0 == do_mkdir("/mydir/newsubdir", 0777));
    ASSERT(0 == do_stat("/mydir/newsubdir", &mystat));
    /*
     * Remember reference count
     */
    ref_count = fs_ext2_print_cache_info();
    /*
     * Rename file - should return -EEXIST as the target is not emtpy
     */
    ASSERT(-130 == do_rename("/mydir/newsubdir", "/myseconddir/subdir1"));
    /*
     * Check reference count
     */
    ASSERT(ref_count == fs_ext2_print_cache_info());
    /*
     * and result of operation
     */
    ASSERT(0 == do_stat("/myseconddir/subdir1", &mystat));
    ASSERT(0 == do_stat("/myseconddir/subdir1/file", &mystat));
    ASSERT(0 == do_stat("/mydir/newsubdir", &mystat));
    return 0;
}

/*
 * Testcase 38: verify that a file cannot be renamed to a directory
 */
int testcase38() {
    struct __ctOS_stat mystat;
    int ref_count;
    int parent_inode_nr;
    int fd;
    /*
     * Verify test setup
     */
    ASSERT(0 == do_stat("/myseconddir/subdir1", &mystat));
    ASSERT(0 == do_stat("/myseconddir/subdir1/file", &mystat));
    ASSERT(0 == do_stat("/mydir/newsubdir", &mystat));
    /*
     * Remember reference count
     */
    ref_count = fs_ext2_print_cache_info();
    /*
     * Try to rename file to /mydir/newsubdir - should return -EISDIR
     */
    ASSERT(-133 == do_rename("/myseconddir/subdir1/file", "/mydir/newsubdir"));
    /*
     * Check reference count
     */
    ASSERT(ref_count == fs_ext2_print_cache_info());
    /*
     * and result of operation
     */
    ASSERT(0 == do_stat("/myseconddir/subdir1", &mystat));
    ASSERT(0 == do_stat("/myseconddir/subdir1/file", &mystat));
    ASSERT(0 == do_stat("/mydir/newsubdir", &mystat));
    return 0;
}

/*
 * Testcase 39: verify that a directory cannot be renamed if the target exists and is a file
 */
int testcase39() {
    struct __ctOS_stat mystat;
    int ref_count;
    int parent_inode_nr;
    int fd;
    /*
     * Verify test setup
     */
    ASSERT(0 == do_stat("/mydir/newsubdir", &mystat));
    ASSERT(0 == do_stat("/hello", &mystat));
    /*
     * Remember reference count
     */
    ref_count = fs_ext2_print_cache_info();
    /*
     * Try to rename /mydir/newsubdir to /hello - should fail with error code -ENOTDIR
     */
    ASSERT(-113 == do_rename("/mydir/newsubdir", "/hello"));
    do_putchar = 0;
    /*
     * Check reference count
     */
    ASSERT(ref_count == fs_ext2_print_cache_info());
    /*
     * and result of operation
     */
    ASSERT(0 == do_stat("/mydir/newsubdir", &mystat));
    ASSERT(0 == do_stat("/hello", &mystat));
    return 0;
}

/*
 * Testcase 40: verify that a directory cannot be moved "down the tree"
 */
int testcase40() {
    struct __ctOS_stat mystat;
    int ref_count;
    int parent_inode_nr;
    int fd;
    /*
     * Perform and verify test setup
     */
    ASSERT(0 == do_mkdir("/a", 0777));
    ASSERT(0 == do_mkdir("/a/b", 0777));
    ASSERT(0 == do_mkdir("/a/b/c/", 0777));
    ASSERT(0 == do_stat("/a", &mystat));
    ASSERT(0 == do_stat("/a/b", &mystat));
    ASSERT(0 == do_stat("/a/b/c", &mystat));
    /*
     * Remember reference count
     */
    ref_count = fs_ext2_print_cache_info();
    /*
     * Try to rename /a/b/ to /a/b/c - should fail with error code -EINVAL
     */
    ASSERT(-107 == do_rename("/a/b", "/a/b/c/"));
    /*
     * Check reference count
     */
    ASSERT(ref_count == fs_ext2_print_cache_info());
    /*
     * and result of operation
     */
    ASSERT(0 == do_stat("/a", &mystat));
    ASSERT(0 == do_stat("/a/b", &mystat));
    ASSERT(0 == do_stat("/a/b/c", &mystat));
    return 0;
}

/*
 * Testcase 41: verify that . cannot be renamed
 */
int testcase41() {
    struct __ctOS_stat mystat;
    int ref_count;
    int parent_inode_nr;
    int fd;
    /*
     * Verify test setup
     */
    ASSERT(0 == do_stat("/mydir/newsubdir", &mystat));
    ASSERT(0 == do_stat("/hello", &mystat));
    /*
     * Remember reference count
     */
    ref_count = fs_ext2_print_cache_info();
    /*
     * Try to rename /mydir/newsubdir/. to /mydir/newsubdirX - should return -EINVAL
     */
    ASSERT(-107 == do_rename("/mydir/newsubdir/.", "/mydir/newsubdirX"));
    do_putchar = 0;
    /*
     * Check reference count
     */
    ASSERT(ref_count == fs_ext2_print_cache_info());
    /*
     * and result of operation
     */
    ASSERT(0 == do_stat("/mydir/newsubdir", &mystat));
    ASSERT(0 == do_stat("/hello", &mystat));
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
    // save();
    END;
    return 0;
}
