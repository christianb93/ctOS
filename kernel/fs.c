/*
 * fs.c
 *
 * This is the ctOS generic file system layer. It offers an abstraction for a specific file system like ext2 or
 * FAT16 which can be used by other parts of the kernel to access a file system in a way which is independent from
 * a specific implementation.
 */

#include "fs.h"
#include "fs_fat16.h"
#include "fs_ext2.h"
#include "kerrno.h"
#include "debug.h"
#include "lib/string.h"
#include "lib/sys/stat.h"
#include "mm.h"
#include "pm.h"
#include "lists.h"
#include "locks.h"
#include "dm.h"
#include "drivers.h"
#include "blockcache.h"
#include "lib/fcntl.h"
#include "lib/os/stat.h"
#include "tty.h"
#include "lib/stdint.h"
#include "lib/termios.h"
#include "net.h"
#include "lib/sys/select.h"
#include "lib/limits.h"
#include "timer.h"
#include "util.h"

/*
 * Local loglevel
 */
int __fs_loglevel = 0;
#define FS_DEBUG(...) do {if (__fs_loglevel > 0 ) { kprintf("DEBUG at %s@%d (%s): ", __FILE__, __LINE__, __FUNCTION__); \
        kprintf(__VA_ARGS__); }} while (0)


/*
 * Release an inode if we hold a reference
 */
#define INODE_RELEASE(x) if ((x)) { (x)->iops->inode_release((x)); }

/*
 * This table holds a list of all file system implementations known
 * to the generic FS layer
 */
static fs_implementation_t known_fs[] = {
        {fs_fat16_probe, fs_fat16_get_superblock, fs_fat16_init, "fat16" },
        {fs_ext2_probe, fs_ext2_get_superblock, fs_ext2_init, "ext2" }
};

#define NR_KNOWN_FS ((sizeof(known_fs) / sizeof(fs_implementation_t)))


/*
 * This is a linked list of all mount points in the system and a spinlock to protect it.
 * The superblock of the root file system itself is not in this list
 * All functions which change the list or manipulate the flag
 * mount_point of an inode need to get a write lock on mount_point_lock!
 */
static mount_point_t* mount_points_head;
static mount_point_t* mount_points_tail;
static rw_lock_t mount_point_lock;

/*
 * This linked list holds all open files and is protected by the lock open_files_lock
 */
static open_file_t* open_files_head;
static open_file_t* open_files_tail;
static spinlock_t open_files_lock;

/*
 * Process level data within the file system
 */
static fs_process_t fs_process[PM_MAX_PROCESS];

/*
 * This is the root inode of the root file system
 */
static inode_t* root_inode;

/*
 * Forward declarations
 */
static void validate_superblock(superblock_t* super);
static void validate_inode(inode_t* inode);
static inode_t* check_inode(int create, int excl, char* path, int mode, int* status);


/*
 * Locking strategy:
 *
 * In this module, the following locks are used:
 * 1) the list of mount points is protected by the r/w lock mount_point_lock
 * 2) the list of open files is protected by the spinlock open_files_lock
 * 3) within each process, there are two spinlocks:
 *    a) fd_table_lock protects the table of file descriptors within this process and their flags
 *    b) spinlock protects all other attributes of the process
 * 4) within each inode, there is a read/write lock rw_lock which protects the content of the inode
 * 5) within each open file, there are the following locks:
 *    a) a semaphore which protects the cursor
 *    b) a spinlock used to protect the reference count
 *
 * Care needs to be taken to always acquire locks in the same order to avoid deadlocks. Currently only the following
 * locking paths / cross-monitor function calls are allowed
 *
 *             ----- mount_point_lock ------                                  sem on open file
 *             |                           |                                          |
 *             |                           |                                          |
 *             V                           V                                          V
 *         spinlock on                 read/write lock                        read/write lock on
 *           process                      on inode                             underlying inode
 *
 *
 *             Lock on file descriptor table                                 read/write lock on an
 *                          |                                                        inode
 *                          |                                                          |
 *                          V                                                          V
 *                 spinlock on open file                                       read/write lock on
 *                                                                              parent directory
 *
 *
 * In particular, read/write locks on inodes are taken "upwards", i.e. if you need to get a lock on an inode and
 * a lock on the parent directory at the same time, always get the lock on the inode first, then the lock on the
 * parent
 *
 * Reference counting:
 *
 * Reference counts are used at two points in this module.
 *
 * First, it is expected that every file system driver implements a reference count on inodes. Thus functions
 * in this module need to keep track of the reference counts of inodes which the acquire from the low-level file
 * system driver
 *
 * Second, each open file has a reference count which is initially one
 * Whenever a reference to an open file is dropped, fs_close() needs to be called. This will decrease the reference count. If
 * the reference count reaches zero, the inode reference within the open file will be dropped and the reference count of the
 * inode will be decreased (which can lead to deletion of the underlying physical file, so do not do this if you hold a spinlock!)
 * Then the open file is removed from the list of open files and the memory associated with it is freed.
 *
 *
 */




/****************************************************************************************
 * The following functions deal with mount points                                       *
 ***************************************************************************************/

/*
 * Set up a new mount point and add it to the list of mount points.
 * Parameter:
 * @device - the device which is mounted
 * @mounted_on - the inode on which the device is mounted
 * @super - the superblock of the mounted file system
 * Return value:
 * 0 upon success
 * ENOMEM if memory allocation for the mount point entry failed
 * Reference counts:
 * - increase reference count of @mounted_on
 * - reference count of @super is incremented by one
 * - reference count of root inode of @super is incremented by one
*  Note that the function does not place any locks, this needs to
 * be done by the caller
 *
 */
static int add_mount_point(dev_t device, inode_t* mounted_on,
        superblock_t* super) {
    mount_point_t* mount_point =
            (mount_point_t*) kmalloc(sizeof(mount_point_t));
    if (0 == mount_point) {
        ERROR("No memory available for mount point\n");
        return ENOMEM;
    }
    LIST_ADD_END(mount_points_head, mount_points_tail, mount_point);
    mount_point->device = device;
    mount_point->mounted_on = mounted_on->iops->inode_clone(mounted_on);
    mount_point->root = super->get_inode(super->device, super->root);
    KASSERT(mount_point->root);
    if (0 == mount_point->root->size) {
        PANIC("This does not look right - size of root inode is zero!\n");
    }
    FS_DEBUG("Mount point: root inode nr is %d@%x, mounted on %d@%x\n", mount_point->root->inode_nr, mount_point->root->dev,
            mount_point->mounted_on->inode_nr, mount_point->mounted_on->dev);
    return 0;
}


/*
 * Mount a given device and file system as root device.
 * Parameter:
 * @root - the root device
 * @fs_impl - the file system to use
 * Return value:
 * 0 upon success
 * EBUSY if the device to be mounted is busy
 * EINVAL if the superblock is not valid
 * Reference counts:
 * - reference count of the root inode of @root_superblock is incremented by one
 * - reference count of @root_superblock is incremented by one
 */
static int mount_root(dev_t root, fs_implementation_t* fs_impl) {
    superblock_t* root_superblock;
    if (root_inode)
        return EBUSY;
    root_superblock = fs_impl->get_superblock(root);
    if (0 == root_superblock)
        return EINVAL;
    validate_superblock(root_superblock);
    root_inode = root_superblock->get_inode(root_superblock->device,
            root_superblock->root);

    /*
     * As we have an indirect reference via the inode, drop direct reference again
     */
    root_superblock->release_superblock(root_superblock);
    if (0 == root_inode)
        return EINVAL;
    validate_inode(root_inode);
    return 0;
}

/*
 * Validate that the new device is supported and not
 * already mounted and that the mount point is not
 * in use
 * Parameter:
 * @device - the new device
 * @fs - the file system on the device
 * @mounted_on - the new mount point
 * Return value:
 * 0 upon success
 * EIO if the file system on the device is not supported
 * ENOTDIR if the mount point is not a directory
 * EBUSY if the device is already mounted
 */
static int validate_mount_point(dev_t device, fs_implementation_t* fs,
        inode_t* mounted_on) {
    mount_point_t* mount_point;
    KASSERT(fs);
    KASSERT(fs->probe);
    if (!fs->probe(device)) {
        DEBUG("Probing device %x failed\n", device);
        return EIO;
    }
    if (mounted_on) {
        if (!S_ISDIR(mounted_on->mode)) {
            return ENOTDIR;
        }
    }
    if (root_inode)
        if (root_inode->dev == device)
            return EBUSY;
    LIST_FOREACH(mount_points_head, mount_point) {
        if ((mount_point->device == device)
                || (INODE_EQUAL(mount_point->mounted_on,mounted_on)))
            return EBUSY;
    }
    return 0;
}


/*
 * Given an inode on which another device is mounted,
 * return the superblock of the mounted file system
 * Note that the reference count of the superblock is
 * not increased, therefore the result of this function
 * must not be stored permanently
 * Parameter:
 * @mounted_on - the inode on which the device is mounted
 * Return value:
 * superblock of mounted file system on success
 * 0 otherwise
 */
static superblock_t* get_mounted_superblock(inode_t* mounted_on) {
    mount_point_t* mount_point;
    LIST_FOREACH(mount_points_head, mount_point) {
        if ((mount_point->mounted_on->dev == mounted_on->dev)
                && (mount_point->mounted_on->inode_nr == mounted_on->inode_nr)) {
            return mount_point->root->super;
        }
    }
    return 0;
}


/*
 * Given an inode of a mounted file system
 * return the inode on which we are mounted
 * Note that the reference count of the inode is
 * not increased, therefore the result of this function
 * must not be stored permanently
 * Parameter:
 * @root_inode - the inode on which the device is mounted
 * Return value:
 * inode on which the file system is mounted or 0 if the root inode is
 * not the root inode of a mounted file system
 */
static inode_t* get_mounted_on_inode(inode_t* root_inode) {
    mount_point_t* mount_point;
    LIST_FOREACH(mount_points_head, mount_point) {
        if ((mount_point->root->dev == root_inode->dev)
                && (mount_point->root->inode_nr == root_inode->inode_nr)) {
            return mount_point->mounted_on;
        }
    }
    return 0;
}

/*
 * Mount a new file system onto a mount point on the existing root file system
 * Parameters:
 * @mounted_on - the mount point on the global file system where the new fs is to be attached (0 --> /)
 * @device - the device which needs to be mounted
 * @fs - the public interface of the file system which is present on the new device
 * Return value:
 * 0 if the operation was successful
 * EIO if the device could not be opened or another error occurred
 * during the mount operation
 * EBUSY if the device is already mounted
 * ENOTDIR if the mount point is not a directory
 * ENOMEM if no memory could be allocated to store the mount point
 * Locks:
 * mount_point_lock - semaphore on mount point table
 * Reference count:
 * - reference count of superblock of new file system will be one
 * - reference count of root inode of new file system will be one
 *
 */
int fs_mount(inode_t* mounted_on, dev_t device, fs_implementation_t* fs) {
    superblock_t* super;
    int rc = 0;
    DEBUG("Trying to mount device %x on mount point (inode_nr = %d)\n", device, mounted_on->inode_nr);
    rw_lock_get_write_lock(&mount_point_lock);
    if ((rc = validate_mount_point(device, fs, mounted_on))) {
        rw_lock_release_write_lock(&mount_point_lock);
        ERROR("validate_mount_point_failed with return code %d\n", rc);
        return rc;
    }
    /*
     * Open device and get superblock
     */
    if ((rc = bc_open(device))) {
        ERROR("Could not open device\n");
        rw_lock_release_write_lock(&mount_point_lock);
        return EIO;
    }
    KASSERT(fs->get_superblock);
    /*
     * Get superblock - reference count of superblock is one once this
     * operation completes
     */
    super = fs->get_superblock(device);
    if (0 == super) {
        ERROR("Ups, file system does not return a superblock\n");
        rw_lock_release_write_lock(&mount_point_lock);
        return EIO;
    }
    DEBUG("Got superblock from device %x, root inode is %d\n", device, super->root);
    /*
     * Do actual mount
     */
    if (0 == mounted_on) {
        rc = mount_root(device, fs);
    }
    else {
        rc = add_mount_point(device, mounted_on, super);
        mounted_on->mount_point = 1;
    }
    /*
     * Mounting will have increase the reference count of the superblock
     * by one, so call release_superblock to decrement it again. Once
     * this completes, the reference count of the superblock and of its
     * root inode will both be one
     */
    super->release_superblock(super);
    rw_lock_release_write_lock(&mount_point_lock);
    return rc;
}


/****************************************************************************************
 * Validation functions which are used to validate the integrity of the in-memory       *
 * file system data structures                                                          *
 ****************************************************************************************/

/*
 * Utility function to verify the integrity of a superblock
 * This function verifies that the function pointers of the
 * superblock are valid and panics if this is not the case
 * Parameter:
 * @super - the superblock to be validated
 */
static void validate_superblock(superblock_t* super) {
    KASSERT(super);
    KASSERT(super->release_superblock);
    KASSERT(super->release_superblock);
    KASSERT(super->is_busy);
}

/*
 * Utility function to verify the integrity of an inode
 * This function verifies that the function pointers of the
 * inode are valid and panics if this is not the case
 * Parameter:
 * @super - the inode to be validated
 */
static void validate_inode(inode_t* inode) {
    KASSERT(inode);
    KASSERT(inode->iops);
    KASSERT(inode->iops->inode_clone);
    KASSERT(inode->iops->inode_get_direntry);
    KASSERT(inode->iops->inode_read);
    KASSERT(inode->iops->inode_release);
    KASSERT(inode->iops->inode_write);
    KASSERT(inode->iops->inode_create);
    KASSERT(inode->iops->inode_unlink);
    KASSERT(inode->super);
}

/****************************************************************************************
 * Initialization                                                                       *
 ****************************************************************************************/

/*
 * Initialize the file system using the given device as root device
 * Parameter:
 * @root - the root device (DEVICE_NONE means no device)
 * Return value:
 * 0 upon success
 * EBUSY if the root device is busy
 * EINVAL if the root device is not valid
 */
int fs_init(dev_t root) {
    int i;
    int j;
    int rc = EINVAL;
    int mounted = 0;
    rw_lock_init(&mount_point_lock);
    open_files_head = 0;
    open_files_tail = 0;
    spinlock_init(&open_files_lock);
    /*
     * Initialize file descriptor tables
     */
    for (i = 0; i < PM_MAX_PROCESS; i++) {
        spinlock_init(&(fs_process[i].fd_table_lock));
        for (j = 0; j < FS_MAX_FD; j++) {
            fs_process[i].fd_tables[j] = 0;
            fs_process[i].fd_flags[j] = 0;
        }
        fs_process[i].cwd = 0;
        fs_process[i].umask = S_IWOTH | S_IWGRP;
    }
    root_inode = 0;
    /*
     * Set up linked lists of mount points
     */
    mount_points_head = 0;
    mount_points_tail = 0;
    /*
     * Init all file systems. If a device is specified, find the first
     * file system which "understands" the device and use it to mount the
     * root file system
     */
    mounted = 0;
    for (i = 0; i < NR_KNOWN_FS; i++) {
        known_fs[i].init();
        if ((root != DEVICE_NONE) && known_fs[i].probe(root) && (0 == mounted)) {
            /*
             * Mount root device
             */
            rc = mount_root(root, known_fs + i);
            mounted = 1;
        }
    }
    if (DEVICE_NONE == root)
        return 0;
    return rc;
}


/*
 * Unmount the root file system
 * Return value:
 * EBUSY if the device is busy
 * 0 if the operation is successful
 * Locks:
 * mount_point_lock - protect mount point list
 * Reference counts:
 * - reference count of root inode of root file system is decreased by one
 * - reference count of root superblock is decreased by one
 * Notes: this function is vulnerable to race conditions if another process
 * concurrently opens new files on the file system. So only use it during
 * kernel shutdown once all user space processes have been stopped!
 */
static int unmount_root() {
    rw_lock_get_write_lock(&mount_point_lock);
    if (open_files_head) {
        rw_lock_release_write_lock(&mount_point_lock);
        return EBUSY;
    }
    if (0 == root_inode) {
        rw_lock_release_write_lock(&mount_point_lock);
        return 0;
    }
    if (mount_points_head) {
        rw_lock_release_write_lock(&mount_point_lock);
        return EBUSY;
    }
    root_inode->iops->inode_release(root_inode);
    root_inode = 0;
    rw_lock_release_write_lock(&mount_point_lock);
    return 0;
}

/*
 * Mount a file system
 * Parameters:
 * @path - mount point
 * @dev - the device to be mounted
 * @fs_name - a string describing the file system to be used
 * Return value:
 * EINVAL if the device or path is invalid
 * EIO if the mount operation failed
 * EBUSY if the device is busy
 * ENOMEM if no memory could be allocated for the entry in the mount point table
 * 0 upon success
 * Reference counts:
 * - reference count of root inode of mounted file system will be one
 * - reference count of superblock of mounted file system will be one
 */
int do_mount(char* path, dev_t dev, char* fs_name) {
    fs_implementation_t* fs_impl = 0;
    int i;
    int rc = 0;
    inode_t* mount_point = 0;
    if (0 == path)
        return EINVAL;
    /*
     * First verify fs_name and find file system structure
     */
    if (0 == fs_name)
        return EINVAL;
    for (i = 0; i < NR_KNOWN_FS; i++)
        if (0 == strcmp(known_fs[i].fs_name, fs_name))
            fs_impl = known_fs + i;
    if (0 == fs_impl) {
        DEBUG("Could not find file system with that name\n");
        return EINVAL;
    }
    /*
     * Find inode to be mounted. If path is /, set
     * mount point to null to indicate mounting of root directory
     */
    if (strcmp(path, "/")) {
        mount_point = fs_get_inode_for_name(path);
        if (0 == mount_point) {
            return EINVAL;
        }
    }
    /*
     * Do mount and release inode again
     */
    DEBUG("Doing actual mount\n");
    rc = fs_mount(mount_point, dev, fs_impl);
    if (rc) {
        DEBUG("Got non-zero return code %d from fs_mount\n", rc);
    }
    if (mount_point)
        mount_point->iops->inode_release(mount_point);
    return rc;
}

/*
 * Unmount a file system
 * Parameters:
 * @mounted_root - the root inode of the mounted file system
 * Return value:
 * 0 upon success
 * EBUSY if the device is busy
 * EINVAL if the device is invalid
 * Locks:
 * mount_point_lock - protect mount point list
 * Reference counts:
 * - reference count of root inode of mounted file system (and that of its superblock) is decremented by one
 * - reference count of mounted on inode (and that of its superblock) is decremented by one (if not /)
 */
int fs_unmount(inode_t* mounted_root) {
    dev_t mounted_device = DEVICE_NONE;
    mount_point_t* mount_point;
    mount_point_t* this_mount_point = 0;
    /*
     * Special case: unmount the root file system is requested
     */
    if (0 == mounted_root)
        return unmount_root();
    /*
     * Lock list of mount points. Note that this will also make sure
     * that no thread is running fs_get_inode_for_name in parallel
     * Consequently, any other thread which runs do_open in parallel can be in
     * one of two situations:
     * a) the thread has already completed all fs_get_inode operations and holds
     * a reference to the inode - this will be detected in the busy check further below
     * b) the thread has not yet obtained an inode reference - then it needs to wait until
     * we have completed the critical section and will therefore not see the mounted file
     * system any more
     * Note that for this to work, it is essential that within fs_get_inode_for_name,
     * the mount point lock is only released once we have incremented the reference
     * count on the inode
     */
    rw_lock_get_write_lock(&mount_point_lock);
    /*
     * Verify that the passed inode is actually the root inode
     * of a mounted file system
     */
    FS_DEBUG("Checking whether inode is actually a mount point\n");
    LIST_FOREACH(mount_points_head, mount_point) {
        if ((mount_point->device == mounted_root->dev)
                && (mount_point->root->inode_nr == mounted_root->inode_nr)) {
            mounted_device = mount_point->device;
            this_mount_point = mount_point;
        }
    }
    if (DEVICE_NONE == mounted_device) {
        rw_lock_release_write_lock(&mount_point_lock);
        return EINVAL;
    }
    /*
     * Verify that there is no open inode on the file system
     */
    FS_DEBUG("Is there an open inode?\n");
    if (1 == this_mount_point->root->super->is_busy(
            this_mount_point->root->super)) {
        rw_lock_release_write_lock(&mount_point_lock);
        return EBUSY;
    }
    /*
     * Verify that there is no device mounted on the device
     * which we are about to remove from the file system
     */
    FS_DEBUG("Is there a device mounted on the part of the file system which we umount?\n");
    LIST_FOREACH(mount_points_head, mount_point) {
        if (mount_point->mounted_on->dev == mounted_device) {
            rw_lock_release_write_lock(&mount_point_lock);
            return EBUSY;
        }
    }
    /*
     * First reset mount point flag, then
     * remove mount point from list and release root inode
     */
    this_mount_point->mounted_on->mount_point = 0;
    LIST_REMOVE(mount_points_head, mount_points_tail, this_mount_point);
    this_mount_point->mounted_on->iops->inode_release(
            this_mount_point->mounted_on);
    this_mount_point->root->iops->inode_release(this_mount_point->root);
    kfree(this_mount_point);
    rw_lock_release_write_lock(&mount_point_lock);
    return 0;
}

/*
 * Unmount a file system
 * Parameters:
 * @path - mount point
 * Return value:
 * EINVAL if the path is not valid
 * EIO if the unmount failed
 * EBUSY if the device is still busy
 * 0 upon success
  * Reference counts:
 * - reference count of root inode of mounted file system (and that of its superblock) is decremented by one
 * - reference count of mounted on inode (and that of its superblock) is decremented by one (if not /)
 */
int do_unmount(char* path) {
    inode_t* mount_point = 0;
    if (0 == path)
        return EINVAL;
    /*
     * Find inode to be unmounted. If path is /, set
     * mount point to null to indicate mounting of root directory
     * Note that otherwise the returned inode is the root inode
     * of the mounted device!
     */
    if (strcmp(path, "/")) {
        mount_point = fs_get_inode_for_name(path);
        if (0 == mount_point)
            return EINVAL;
    }
    /*
     * Release inode again to avoid wrong reference count
     * during check whether fs is busy and do unmount
     */
    if (mount_point)
        mount_point->iops->inode_release(mount_point);
    FS_DEBUG("Doing actual umount operation\n");
    return fs_unmount(mount_point);
}




/****************************************************************************************
 * Get and set the current working directory                                            *
 ****************************************************************************************/

/*
 * Get a reference to the current working directory. If the working directory returned
 * is NULL, this is to be interpreted as root directory.
 * Return value:
 * the current working directory
 * Locks:
 * spinlock on current process
 * Reference counts:
 * - increase reference count of working directory (if different from NULL) by one
 */
static inode_t* cwd_get() {
    inode_t* cwd = 0;
    u32 eflags;
    int pid = pm_get_pid();
    spinlock_get(&fs_process[pid].spinlock, &eflags);
    if (fs_process[pid].cwd) {
        cwd = fs_process[pid].cwd->iops->inode_clone(fs_process[pid].cwd);
    }
    spinlock_release(&fs_process[pid].spinlock, &eflags);
    return cwd;
}

/*
 * Set a new working directory
 * Parameter:
 * @proc - the process
 * @new_cwd - the new working directory
 * Locks:
 * spinlock on process @proc
 * Reference counts:
 * - decrease reference count of old working directory
 * - reference count of new working directory is NOT increased
 */
static void cwd_set(fs_process_t* proc, inode_t* new_cwd) {
    inode_t* old_cwd;
    u32 eflags;
    spinlock_get(&proc->spinlock, &eflags);
    old_cwd = proc->cwd;
    proc->cwd = new_cwd;
    spinlock_release(&proc->spinlock, &eflags);
    /*
     * Recall that inode_release might trigger an I/O operation and hence
     * we can do this only after releasing the spinlock
     */
    if (old_cwd) {
        old_cwd->iops->inode_release(old_cwd);
    }
}

/*
 * Get the current working directory
 * Parameters:
 * @buffer - a buffer in which we store the current working directories absolute name
 * @n - size of the buffer
 * Return value:
 * 0 upon success
 * -ENOTDIR if the current working directory is not a directory
 * -ENOMEM if no memory could be allocated temporarily
 * -EIO if an I/O error occured
 * -ERANGE if the name does not fit into the provided buffer
 */
int do_getcwd(char* buffer, size_t n) {
    /*
     * Get current working directory from process structure
     */
    inode_t* cwd = 0;
    int rc;
    cwd = cwd_get();
    if (cwd) {
        rc = fs_get_dirname(cwd, buffer, n);
        cwd->iops->inode_release(cwd);
    }
    else {
        /*
         * cwd NULL means root directory
         */
        if (n<2)
            return -ERANGE;
        buffer[0]='/';
        buffer[1]=0;
        rc = 0;
    }
    return rc;
}

/*
 * Change the current working directory
 * Parameters:
 * @path - the name of the new working directory
 * Return values:
 * 0 if the operation was successful
 * ENOENT if the directory does not exist
 * ENOTDIR if the path is not a directory
 * Reference counts:
 * - drop one reference to old working directory
 * - increase reference count of new working directory
 */
int do_chdir(char* path) {
    inode_t* new_cwd;
    u32 pid;
    pid = pm_get_pid();
    if (0==root_inode)
        return ENOENT;
    if (0 == strcmp(path, "/")) {
        new_cwd = 0;
    }
    else {
        new_cwd = fs_get_inode_for_name(path);
        if (0 == new_cwd) {
            return ENOENT;
        }
    }
    /*
     * make sure that this is a directory
     */
    if (new_cwd) {
        if (!S_ISDIR(new_cwd->mode)) {
            return ENOTDIR;
        }
    }
    /*
     * If the new working directory is the root directory,
     * set it to 0
     */
    if (new_cwd) {
        if ((new_cwd->inode_nr == root_inode->inode_nr) && (new_cwd->dev == root_inode->dev)) {
            new_cwd->iops->inode_release(new_cwd);
            new_cwd = 0;
        }
    }
    cwd_set(fs_process+pid, new_cwd);
    return 0;
}

/****************************************************************************************
 * Basic operations on a directory:                                                     *
 * - scan a directory for a given inode                                                 *
 * - scan a directory for a given file name                                             *
 * - return an inode by name                                                            *
 ****************************************************************************************/

/*
 * Utility function to extract the parent directory from a path name
 * Parameters:
 * @parent_dir - the string to which we write the parent directory
 * @name - the string to which we write the file name without leading directory
 * @path - the path name of the file
 * @strip - strip off trailing slash from path if any
 * Note that at least strlen(path)+1 bytes should be reserved in name and parent_dir
 */
static void split_path(char* parent_dir, char* path, char* name, int split) {
    char* ptr;
    /*
     * First copy path to parent_dir
     */
    strcpy(parent_dir, path);
    parent_dir[strlen(path)] = 0;
    /*
     * Strip of trailing slash if any
     */
    if (strlen(path) > 0) {
        if (path[strlen(path) - 1] == '/')
            parent_dir[strlen(path) - 1] = 0;
    }
    /*
     * Now locate the last slash. Note that if the path name
     * is a relative path name, ptr will be parent_dir - 1 after
     * completing this loop
     */
    ptr = parent_dir + strlen(path);
    while ((*ptr != '/') && (ptr >= parent_dir))
        ptr--;
    /*
     * Now copy file name
     */
    if (name) {
        memset(name, 0, strlen(path)+1);
        strcpy(name, ptr+1);
    }
    /*
     * And cut off path name
     */
    *(ptr+1) = 0;
}


/*
 * Given an inode which represents a directory, scan the directory
 * for a given inode and return its name. The returned string is to
 * be freed by the caller
 * Parameter:
 * @dir - the directory inode to read
 * @wanted - the inode number of the entry to look for
 * Return value:
 * the name of the inode or NULL
 */
static char* scan_directory_by_inode(inode_t* dir,
        int wanted) {
    int i = 0;
    direntry_t direntry;
    char* name = 0;
    validate_inode(dir);
    while (0 == dir->iops->inode_get_direntry(dir, i, &direntry)) {
        if (direntry.inode_nr == wanted) {
            if ((name = kmalloc(strlen(direntry.name)+1)))
                strcpy(name, direntry.name);
        }
        i++;
    }
    return name;
}


/*
 * Given an inode which represents a directory, scan the directory
 * for a given inode and return its name, locking the directory
 * The returned string needs to be freed by the caller
 * Parameter:
 * @dir - the directory inode to read
 * @wanted - the inode number of the entry to look for
 * Return value:
 * the name of the inode or NULL
 * Locks:
 * rw_lock on directory inode (read lock)
 */
static char* scan_directory_by_inode_lock(inode_t* dir,
        int wanted) {
    char* name = 0;
    rw_lock_get_read_lock(&dir->rw_lock);
    name = scan_directory_by_inode(dir, wanted);
    rw_lock_release_read_lock(&dir->rw_lock);
    return name;
}

/*
 * Given an inode which represents a directory, scan the directory
 * for a given name. If the name is found, return the inode.
 * Parameter:
 * @dir - the directory inode to read
 * @name - the path component to look for (not necessarily null terminated)
 * @length - length of the path component
 * Return value:
 * the inode if a matching entry was found, 0 otherwise
 * Reference counts:
 * - increase reference count on returned inode
 */
static inode_t* scan_directory_by_name(inode_t* dir,
        char* name, int length) {
    int i = 0;
    direntry_t direntry;
    validate_inode(dir);
    if (0 == length)
        return 0;
    while (0 == dir->iops->inode_get_direntry(dir, i, &direntry)) {
        if ((0 == strncmp(direntry.name, name, length) && (length == strlen(direntry.name)))) {
            return dir->super->get_inode(dir->dev, direntry.inode_nr);
        }
        i++;
    }
    return 0;
}

/*
 * Given an inode which represents a directory, scan the directory
 * for a given name, locking the directory for read during the scan
 * Parameter:
 * @dir - the directory inode to read
 * @name - the path component to look for (not necessarily null terminated)
 * @length - length of the path component
 * Return value:
 * the inode if a matching entry was found, 0 otherwise
 * Locks:
 * rw_lock on directory inode (read lock)
 * Reference counts:
 * - increase reference count of returned inode by one
 */
static inode_t* scan_directory_by_name_lock(inode_t* dir,
        char* name, int length) {
    inode_t* inode = 0;
    rw_lock_get_read_lock(&dir->rw_lock);
    inode = scan_directory_by_name(dir, name, length);
    rw_lock_release_read_lock(&dir->rw_lock);
    return inode;
}


/*
 * Get an inode for a path name relative to the current working directory
 * This function searches the entire file system, starting at the root inode if
 * an absolute path name is provided, or at the current working directory. Mounts
 * are followed during this search, i.e. if you mount a filesystem on /tmp and call this
 * function with path="/tmp", you get the root inode of the mounted file system
 * Parameter:
 * @path - the path name
 * Locks:
 * mount_point_lock - make sure that the mount point structure remains stable while searching
 * Cross-monitor function calls:
 * scan_directory_by_name_lock
 * cwd_get()
 * Reference counts:
 * - increase reference count of returned inode by one
 */
inode_t* fs_get_inode_for_name(char* path) {
    inode_t* current_inode;
    inode_t* new_inode;
    superblock_t* current_superblock = 0;
    char* ptr = path;
    char* next = 0;
    inode_t* mounted_on;
    KASSERT(ptr);
    if ((0 == root_inode) || (0 == path))
        return 0;
    rw_lock_get_read_lock(&mount_point_lock);
    /*
     * Is this a path name relative to the current working directory?
     */
    if (*ptr != '/') {
        if (0 == (current_inode = cwd_get()))  {
            current_inode = root_inode->iops->inode_clone(root_inode);
        }
    }
    /*
     * It is an absolute path name
     */
    else {
        current_inode = root_inode->iops->inode_clone(root_inode);
    }
    current_superblock = current_inode->super;
    while ((*ptr) && current_inode) {
        FS_DEBUG("Current inode: (%d, %d)\n", current_inode->dev, current_inode->inode_nr);
        /*
         * Move pointer to first character after slash
         * if it currently points to a slash
         */
        while ('/' == *ptr) {
            ptr++;
        }
        if (0 == *ptr)
            break;
        /*
         * Advance to next separator or end of string
         */
        for (next = ptr; (*next) && (*next != '/'); next++)
            ;
        /*
         * If the current inode is the root inode of a mounted file system
         * and we are looking for "..", switch to inode on the file
         * system on which we are mounted
         */
        if ((current_inode->inode_nr == current_superblock->root) && (strncmp("..", ptr, 2) == 0) && (2 == (next - ptr))) {
            /*
             * Is this a mounted file system?
             * If yes, switch to parent file system
             */
            if ((mounted_on = get_mounted_on_inode(current_inode))) {
                current_superblock = mounted_on->super;
                current_inode->iops->inode_release(current_inode);
                current_inode = mounted_on->iops->inode_clone(mounted_on);
            }
        }
        /*
         * Scan the directory for the path component
         */
        new_inode = scan_directory_by_name_lock(current_inode, ptr, next - ptr);
        current_inode->iops->inode_release(current_inode);
        current_inode = new_inode;
        if (current_inode) {
            /*
             * If this is a mount point, switch to a different superblock and a different inode
             */
            if (current_inode->mount_point) {
                current_superblock = get_mounted_superblock(current_inode);
                new_inode = current_superblock->get_inode(
                        current_superblock->device, current_superblock->root);
                current_inode->iops->inode_release(current_inode);
                current_inode = new_inode;
            }
        }
        ptr = next;
    }
    /*
     * If last character in path is a slash, make sure that we return
     * a directory
     */
    if (next && (*next) && current_inode) {
        if (!(S_ISDIR(current_inode->mode))) {
            current_inode->iops->inode_release(current_inode);
            current_inode = 0;
        }
    }
    rw_lock_release_read_lock(&mount_point_lock);
    FS_DEBUG("Returning inode with inode nr %d\n", (current_inode == 0) ? 0 : current_inode->inode_nr);
    return current_inode;
}


/*
 * Given an inode which describes a directory, this function
 * determines the absolute path name of the directory and stores
 * it in the provided buffer.
 * If this function is called with a directory which is the root directory
 * of a mounted file system, it only returns the correct result if the
 * inode passed as parameter is itself already on the mounted file system
 * Return value:
 * 0 upon successful completion
 * -ENOTDIR if the inode does not represent a directory
 * -EIO if an unspecified error occurs
 * -ENOMEM if temporary memory could not be allocated
 * -ERANGE if the size of the buffer is not sufficient
 * -EINVAL if any of the provided parameters is not valid
 * Locks:
 * mount_point_lock
 * Cross-monitor function calls:
 * scan_directory_by_name_lock
 * scan_directory_by_inode_lock
 */
int fs_get_dirname(inode_t* inode, char* buffer, size_t n) {
    inode_t* current_inode;
    inode_t* next_inode;
    inode_t* mounted_on_inode;
    int name_index = n-1;
    int i;
    int error = 0;
    char* name;
    if ((0 == inode) || (0 == n))
        return -EINVAL;
    if (1 == n)
        return -ERANGE;
    if (!S_ISDIR(inode->mode))
        return -ENOTDIR;
    buffer[n-1]=0;
    /*
     * First handle the special case that the inode is the root inode
     */
    if ((inode->inode_nr == root_inode->inode_nr) && (inode->dev == root_inode->dev)) {
        buffer[0]='/';
        buffer[1]=0;
        return 0;
    }
    /*
     * We walk our way upwards through the filesystem, using the
     * .. entries of each directory until we hit upon the root inode. During each step,
     * we will then browse the parent directory until we find an entry matching the current
     * inode and add its name at the end of the supplied buffer, starting from the right to
     * the left and separated by /. Once we are done, we move the entire string to the beginning
     * of the buffer
     */
    rw_lock_get_read_lock(&mount_point_lock);
    current_inode = inode->iops->inode_clone(inode);
    while (0 == error) {
        if ((current_inode->dev == root_inode->dev) && (current_inode->inode_nr == root_inode->inode_nr))
            break;
        /*
         * If current_inode is the root directory of a mounted file system,
         * switch to the inode on which it is mounted now
         */
        if ((mounted_on_inode = get_mounted_on_inode(current_inode))) {
            FS_DEBUG("Switching to mount point %d on %x\n", mounted_on_inode->inode_nr, mounted_on_inode->dev);
            current_inode->iops->inode_release(current_inode);
            current_inode = mounted_on_inode->iops->inode_clone(mounted_on_inode);
        }
        /*
         * Now scan directory to locate entry ..
         */
        if ((next_inode = scan_directory_by_name_lock(current_inode, "..", 2))) {
            /*
             * Now next inode is the directory in which current inode is located. Scan this directory
             * to figure out the name of current_inode
             */
            name = scan_directory_by_inode_lock(next_inode, current_inode->inode_nr);
            if (name) {
                FS_DEBUG("Found name: %s\n", name);
                /*
                 * Add name at current end of buffer. First reserve space for name and separator
                 */
                name_index -= strlen(name)+1;
                if ((name_index >= 0) && (name_index < n)) {
                    buffer[name_index]='/';
                    for (i = 0; i < strlen(name); i++)
                        buffer[name_index+i+1] = name[i];
                }
                else {
                    error = -ERANGE;
                }
                kfree(name);
            }
            else {
                error = -ENOMEM;
            }
            /*
             * Done. Release current inode and replace it by next_inode
             */
            current_inode->iops->inode_release(current_inode);
            current_inode = next_inode;
        }
        else {
            /*
             * Next_inode is NULL
             */
            error = -EIO;
        }
    }
    current_inode->iops->inode_release(current_inode);
    /*
     * Now name_index points to the start of the first path component. Copy this to
     * its final location
     */
    if (0 == error)
        strcpy(buffer, buffer+name_index);
    rw_lock_release_read_lock(&mount_point_lock);
    return error;
}

/*****************************************************************************************
 * These functions deal with controlling terminals                                       *
 *****************************************************************************************/

/*
 * If @inode is a terminal, set the controlling terminal of the
 * calling process to this terminal
 * Do nothing if @inode is not a terminal
 * Parameter:
 * @inode - the inode
 */
static void tty_attach(inode_t* inode) {
    if (MAJOR(inode->s_dev) == MAJOR_TTY)  {
        pm_attach_tty(inode->s_dev);
    }
}




/*****************************************************************************************
 * The following functions implement basic operations on the level of open files         *
 * - add a new open file to cross-process list of open files                             *
 * - remove a file from the list of open files                                           *
 * - increase the reference count on an open file                                        *
 * - decrease the reference count and destroy file it if reaches zero (fs_close)         *
 *****************************************************************************************/

/*
 * Add a new entry to the list of open files
 * Parameter:
 * @of - the new file
 * Locks:
 * open_files_lock
 */
static void add_open_file(open_file_t* of) {
    u32 eflags;
    spinlock_get(&open_files_lock, &eflags);
    LIST_ADD_END(open_files_head, open_files_tail, of);
    spinlock_release(&open_files_lock, &eflags);
}

/*
 * Remove an entry from the list of open files
 * Parameter:
 * @of - the file to be removed
 * Locks:
 * open_files_lock
 */
static void remove_open_file(open_file_t* of) {
    u32 eflags;
    spinlock_get(&open_files_lock, &eflags);
    LIST_REMOVE(open_files_head, open_files_tail, of);
    spinlock_release(&open_files_lock, &eflags);
}

/*
 * Duplicate a reference to an open file by
 * increase the files reference count
 * Parameter:
 * @of - the open file
 * Return value:
 * returns a new reference to the open file
 * Locks:
 * lock on open file
 * Reference counts:
 * - increase reference count of open file by one
 */
static open_file_t* clone_open_file(open_file_t* of) {
    u32 eflags;
    spinlock_get(&of->lock, &eflags);
    of->ref_count++;
    spinlock_release(&of->lock, &eflags);
    return of;
}

/*
 * Close an open file. This function will decrease the reference count of
 * an open file descriptor. If the reference count reaches zero, it will
 * remove the open file from the list of open files, release the respective
 * inode and release any memory allocated for the open file
 * Parameter:
 * @file - the open file to be closed
 * Return code:
 * EBADF if @file is not a valid open file
 * 0 upon success
 * Locks:
 * spinlock on open file
 * Reference counts:
 * - decrease reference count of open file
 * - if that drops to zero, release reference count on associated inode
 */
int fs_close(open_file_t* file) {
    u32 eflags;
    char_dev_ops_t* ops;
    inode_t* inode;
    pipe_t* pipe;
    socket_t* socket;
    int flags;
    dev_t device = file->inode->s_dev;
    int is_chr = S_ISCHR(file->inode->mode);
    FS_DEBUG("Getting spinlock\n");
    spinlock_get(&file->lock, &eflags);
    if (0 == file->ref_count) {
        ERROR("fs_close called on file with reference count zero\n");
        spinlock_release(&file->lock, &eflags);
        return EBADF;
    }
    file->ref_count--;
    if (0 == file->ref_count) {
        FS_DEBUG("Reference counted has reached zero\n");
        /*
         * Make sure that we release the spinlock before calling release on
         * the inode as this might trigger a read/write operation which requires
         * interrupts to work. This is not a race condition, as nobody else has a reference
         * to the file any more and thus nobody else can access it anyway
         */
        inode = file->inode;
        pipe = file->pipe;
        flags = file->flags;
        socket = file->socket;
        spinlock_release(&file->lock, &eflags);
        /*
         * Remove file from list of open files
         */
        remove_open_file(file);
        FS_DEBUG("Freeing file\n");
        kfree(file);
        if (inode) {
            /*
             * For a pipe, no iops structure is defined - in this case we
             * free the inode and the pipe using kfree if needed
             */
            if (0 == inode->iops)  {
                if (S_ISFIFO(inode->mode)) {
                    FS_DEBUG("Calling pipe_disconnect, flags are %d\n", flags);
                    if (1 == fs_pipe_disconnect(pipe, ((flags & O_WRONLY)) ?  PIPE_WRITE : PIPE_READ)) {
                        FS_DEBUG("Freeing pipe\n");
                        kfree(pipe);
                        kfree(inode);
                    }
                }
                else if (S_ISSOCK(inode->mode)) {
                    /*
                     * close socket
                     */
                    if (socket)
                        net_socket_close(socket);
                }
                else {
                    PANIC("Inode is not a pipe nor a socket, but has no iops structure, giving up\n");
                }
            }
            else {
                inode->iops->inode_release(inode);
            }
        }
        /*
         * If the file is a character device, call close on the device
         * We can do this safely now even if close sleeps as we do not
         * hold any spinlock at the moment
         */
        if (is_chr) {
            ops = dm_get_char_dev_ops(MAJOR(device));
            if (ops) {
                ops->close(MINOR(device));
            }
            else {
                ERROR("Could not get operations data structure for major device %d\n", MAJOR(device));
            }
        }
    }
    else
        spinlock_release(&file->lock, &eflags);
    return 0;
}

/****************************************************************************************
 * The following functions use the basic functions operating on open files              *
 * to implement the following functions related to open files:                          *
 * - create a new open file                                                             *
 * - read from an open file or write to an open file                                    *
 * - change the cursor position within an open file (seek)                              *
 ****************************************************************************************/

/*
 * Open a file for a given inode, i.e. create a new entry in the list of
 * open files and return a pointer to this new open file. This function will
 * clone the inode, so the callee needs to release it again
 * Parameter:
 * @inode - the inode representing the file to be opened
 * @flags - flags
 * Return value:
 * a pointer to the new open file or 0 if the operation failed
 * Reference counts:
 * - reference count of return value is one
 * - reference count of inode is increased by one
 */
open_file_t* fs_open(inode_t* inode, int flags) {
    open_file_t* of = 0;
    char_dev_ops_t* ops;
    if (0 == inode)
        return 0;
    if (0 == (of = (open_file_t*) kmalloc(sizeof(open_file_t)))) {
        ERROR("Could not allocate memory for new open file\n");
        return 0;
    }
    /*
     * As a pipe or socket does not have a "real" inode, do not call
     * validate_inode in this case and do not use clone
     */
    if ((!S_ISFIFO(inode->mode)) && (!S_ISSOCK(inode->mode)))
        validate_inode(inode);
    of->cursor = 0;
    of->flags = flags;
    of->pipe = 0;
    of->socket = 0;
    if (inode->iops)
        of->inode = inode->iops->inode_clone(inode);
    else
        of->inode = inode;
    spinlock_init(&of->lock);
    sem_init(&of->sem, 1);
    of->ref_count = 1;
    /*
     * If the file is a character device, call open on the device
     */
    if (S_ISCHR(inode->mode)) {
        if ((ops = (dm_get_char_dev_ops(MAJOR(inode->s_dev))))) {
            ops->open(MINOR(inode->s_dev));
        }
        else {
            ERROR("Could not get operations data structure for major device %d\n", MAJOR(inode->s_dev));
            fs_close(of);
            return 0;
        }
        /*
         * If the file is a tty, and the process does not yet have an associated controlling
         * terminal, set it
         */
        tty_attach(inode);
    }
    /*
     * Add new open file to list
     */
    add_open_file(of);
    return of;
}


/*
 * Close all file descriptors of the current process and
 * set current working directory to null
 * Locks:
 * spinlock in process table entry
 * Reference counts:
 * - decrease reference count of current working directory
 */
void fs_close_all() {
    int i;
    u32 pid = pm_get_pid();
    for (i = 0; i < FS_MAX_FD; i++) {
        do_close(i);
    }
    cwd_set(fs_process + pid, 0);
}

/*
 * Close all file descriptors for which the FD_CLOEXEC flag is
 * set and all open directories for a process
 *
 * Only call this function if all but the active task
 * within this process have been terminated to avoid
 * race conditions on the file descriptor flags!
 *
 * Parameters:
 * @pid - the process id
 */
void fs_on_exec(int pid) {
    int i;
    open_file_t* file;
    /*
     */
    for (i = 0; i < FS_MAX_FD; i++) {
        if (fs_process[pid].fd_tables[i]) {
            file = fs_process[pid].fd_tables[i];
            if ((fs_process[pid].fd_flags[i] & FD_CLOEXEC) || (S_ISDIR(file->inode->mode)))
                do_close(i);
        }
    }
}

/*
 * Implementation of the inode read/write operation for a
 * special file representing a character device
 * Parameter:
 * @inode - the inode from which we read
 * @bytes - bytes to read
 * @data - buffer
 * @rw - operation to be performed (0=read, 1=write)
 * @flags - flags of the open file
 * Return value:
 * -EINVAL if the device was not valid
 * -EIO if the operation failed
 * the number of bytes read or written upon success
 */
static ssize_t fs_rw_chr(inode_t* inode, ssize_t bytes, void* data, int rw, u32 flags) {
    dev_t device;
    char_dev_ops_t* ops = 0;
    /*
     * Get device information
     */
    device = inode->s_dev;
    /*
     * Locate device
     */
    if (0 == (ops = dm_get_char_dev_ops(MAJOR(device)))) {
        ERROR("Could not get device operations structure for major device %h\n", MAJOR(device));
        return -EINVAL;
    }
    /*
     * Perform operation
     */
    if (FS_READ == rw) {
        return ops->read(MINOR(device), bytes, data, flags);
    }
    else {
        return ops->write(MINOR(device), bytes, data);
    }
}

/*
 * Implementation of the inode read/write operation for a
 * regular file
 * Parameter:
 * @file - the file from which we read or to which we write
 * @bytes - bytes to read/write
 * @data - buffer
 * @rw - operation to be performed (0=read, 1=write)
 * Return value:
 * the number of bytes read or written
 * -EIO if the operation failed
 * Locks:
 * rw_lock on open file
 */
static ssize_t fs_rw_reg(open_file_t* file, ssize_t bytes, void* data, int rw) {
    ssize_t rc = 0;
    if (FS_READ == rw) {
        rw_lock_get_read_lock(&file->inode->rw_lock);
        rc = file->inode->iops->inode_read(file->inode, bytes, file->cursor,
            data);
        rw_lock_release_read_lock(&file->inode->rw_lock);
    }
    else {
        rw_lock_get_write_lock(&file->inode->rw_lock);
        /*
         * If file has been opened with O_APPEND, set cursor to end of file
         */
        if (file->flags & O_APPEND) {
            FS_DEBUG("Setting cursor to %d\n", file->inode->size);
            file->cursor = file->inode->size;
        }
        rc = file->inode->iops->inode_write(file->inode, bytes, file->cursor,
                data);
        rw_lock_release_write_lock(&file->inode->rw_lock);
    }
    return rc;
}

/*
 * Implementation of the inode read/write operation for a
 * directory
 * Parameter:
 * @file - the file from which we read or to which we write
 * @direntry - buffer
 * @rw - operation to be performed (0=read, 1=write)
 * Return value:
 * 0 upon success
 * -1 if the directory could not be found
 * EIO if an error occurred
 * Locks:
 * rw_lock on open file
 */
static ssize_t fs_rw_dir(open_file_t* file, direntry_t* direntry,  int rw) {
    ssize_t rc = 0;
    if (FS_READ == rw) {
        rw_lock_get_read_lock(&file->inode->rw_lock);
        rc = file->inode->iops->inode_get_direntry(file->inode, file->cursor, direntry);
        rw_lock_release_read_lock(&file->inode->rw_lock);
    }
    return rc;
}

/*
 * Read from an open file
 * Parameter:
 * @file - the open file from which to read
 * Return value:
 * the number of bytes read upon success
 * -EIO if an I/O error occured
 * -EINVAL if the file is a character device and the device is not valid
 * -EPAUSE if the read has been interrupted and no data was available
 * -EOVERFLOW if the specified byte count would lead to an overflow of the
 * files cursor position
 * Locks:
 * file->sem - the semaphore protecting the inner state of the file
 * Cross-monitor function calls:
 * fs_rw_reg
 */
ssize_t fs_read(open_file_t* file, size_t bytes, void* buffer) {
    ssize_t rc;
    /*
     * We can read at most up to INT32_MAX bytes
     */
    if (bytes>INT32_MAX)
        return -EOVERFLOW;
    /*
     * Use specific functions if the inode is a character device, a pipe or a socket
     */
    if (S_ISCHR(file->inode->mode)) {
        rc = fs_rw_chr(file->inode, bytes, buffer, FS_READ, file->flags);
    }
    else if (S_ISFIFO(file->inode->mode)) {
        rc = fs_pipe_read(file->pipe, bytes, buffer, (file->flags & O_NONBLOCK) ? 1 : 0);
    }
    else if (S_ISSOCK(file->inode->mode)) {
        rc = net_socket_recv(file->socket, buffer, bytes, 0, 0, 0, 0);
    }
    else {
        /*
         * We get the lock on the cursor position only if we read
         * from a regular file as reading from a terminal or FIFO
         * could block and does not need to know the current cursor
         * position
         */
        sem_down(&file->sem);
        /*
         * Check for overflow of cursor position
         */
        if ((file->cursor + (off_t) bytes) < 0) {
            sem_up(&file->sem);
            return -EOVERFLOW;
        }
        /*
         * Temporarily get read lock and do actual read operation
         */
        rc = fs_rw_reg(file, bytes, buffer, FS_READ);
        if (rc >= 0)
            file->cursor += rc;
        sem_up(&file->sem);
    }
    return rc;
}


/*
 * Read from an open directory
 * Parameter:
 * @file - the open file representing the directory
 * @direntry - the direntry where the result is stored
 * Return value:
 * 0 upon success
 * -1 if no further directory entry could be read
 * -EIO if an I/O error occured
 * -EINVAL if the open file is not a directory
 * -EOVERFLOW if the directory index would overflow
 * Locks:
 * file->sem - the semaphore protecting the inner state of the file
 * Cross-monitor function calls:
 * fs_rw_dir
 */
ssize_t fs_readdir(open_file_t* file, direntry_t* direntry) {
    ssize_t rc;
    sem_down(&file->sem);
    /*
     * Check for overflow
     */
    if (INT32_MAX == file->cursor) {
        sem_up(&file->sem);
        return -EOVERFLOW;
    }
    /*
     * Make sure that this is a directory
     */
    if (!S_ISDIR(file->inode->mode))
        rc = -EINVAL;
    /*
     * Read directory entry
     */
    else {
        rc = fs_rw_dir(file, direntry, FS_READ);
    }
    if (0 == rc) {
        file->cursor++;
    }
    if ((rc < 0) && (rc != -1))
        rc = rc * (-1);
    sem_up(&file->sem);
    return rc;
}

/*
 * Write to an open file
 * Parameter:
 * @file - the open file to which to write
 * Return value:
 * -EINVAL if the device is not valid
 * -EIO if the operation failed
 * -EPAUSE if the write operation was interrupted by a signal
 * -EPIPE if a pipe was broken
 * the number of bytes written upon success
 * Locks:
 * file->sem - the semaphore protecting the inner state of the file
 * Cross-monitor function calls:
 * fs_rw_reg
 */
ssize_t fs_write(open_file_t* file, size_t bytes, void* buffer) {
    ssize_t rc = 0;
    /*
     * We can read at most INT32_MAX bytes
     */
    if (bytes > INT32_MAX)
        return -EOVERFLOW;
    /*
     * Use specific functions if the inode is a character device
     * or a pipe
     */
    if (S_ISCHR(file->inode->mode)) {
        rc = fs_rw_chr(file->inode, bytes, buffer, 1, file->flags);
    }
    else if (S_ISFIFO(file->inode->mode)) {
        rc = fs_pipe_write(file->pipe, bytes, buffer, (file->flags & O_NONBLOCK) ? 1 : 0);
        /*
         * If the pipe was broken, send SIGPIPE to current thread
         */
        if (-EPIPE == rc){
            do_pthread_kill(pm_get_task_id(), __KSIGPIPE);
        }
    }
    else if (S_ISSOCK(file->inode->mode)) {
        rc = net_socket_send(file->socket, buffer, bytes, 0, 0, 0, 0);
    }
    else if (S_ISREG(file->inode->mode)) {
        sem_down(&file->sem);
        if (file->cursor+ (off_t) bytes < 0) {
            sem_up(&file->sem);
            return -EOVERFLOW;
        }
        rc = fs_rw_reg(file, bytes, buffer, FS_WRITE);
        if (rc > 0) {
            file->cursor += rc;
        }
        sem_up(&file->sem);
    }
    return rc;
}

/*
 * Seek position within open file
 * Parameter:
 * @fd - file
 * @offset - the offset
 * @whence - a flag indicating to what the offset refers. Here SEEK_SET means that @offset is
 * the absolute value of the offset to be set. SEEK_CUR means that the offset refers to the
 * current position. SEEK_END means that the offset refers to the file size
 * Returns:
 * -EINVAL if the value of whence is not valid
 * -ESPIPE if we try to apply a seek operation to a pipe
 * the new offset to the start of the file if the operation is successful
 * Locks:
 * file->sem - semaphore used to protect the file offset
 *
 */
ssize_t fs_lseek(open_file_t* file, off_t offset, int whence) {
    off_t res = 0;
    char_dev_ops_t* ops;
    /*
     * If the file descriptor is a pipe, return ESPIPE
     */
    if (file->pipe)
        return -ESPIPE;
    sem_down(&file->sem);
    switch (whence) {
        case SEEK_SET:
            if (offset < 0) {
                res = -EOVERFLOW;
            }
            else {
                file->cursor = offset;
            }
            break;
        case SEEK_CUR:
            /*
             * Check for overflow or negative cursor position
             */
            if (file->cursor + offset <0) {
                res = -EOVERFLOW;
            }
            else {
                file->cursor += offset;
            }
            break;
        case SEEK_END:
            /*
             * Check for overflow or negative cursor position
             */
            if (offset + file->inode->size < 0) {
                res = -EOVERFLOW;
            }
            else {
                file->cursor = offset + file->inode->size;
            }
            break;
        default:
            res = -EINVAL;
            break;
    }
    if (0 == res)
        res = file->cursor;
    if (S_ISCHR(file->inode->mode)) {
        ops = dm_get_char_dev_ops(MAJOR(file->inode->s_dev));
        if (ops) {
            ops->seek(MINOR(file->inode->s_dev), res);
        }
        else {
            ERROR("Could not get valid operations structure for major device %d\n", MAJOR(file->inode->s_dev));
            res = file->cursor;
        }
    }
    sem_up(&file->sem);
    return res;
}


/****************************************************************************************
 * Functions to manage the table of file descriptors per process                        *
 ****************************************************************************************/

/*
 * Given a process and a file descriptor, locate the open file associated with the
 * file descriptor, increase its reference count by one and return it
 * Parameter:
 * @proc - the process
 * @fd - the file descriptor
 * Return value:
 * the open file or NULL
 * Locks:
 * lock on file descriptor table of the process
 * Cross-monitor function calls:
 * clone_open_file
 * Reference counts:
 * - increase reference count of returned file by one
 */
static open_file_t* get_file(fs_process_t* proc, int fd) {
    u32 eflags;
    open_file_t* of = 0;
    KASSERT(proc);
    spinlock_get(&proc->fd_table_lock, &eflags);
    of = proc->fd_tables[fd];
    /*
     * Increase reference count of file
     */
    if (of) {
        clone_open_file(of);
    }
    spinlock_release(&proc->fd_table_lock, &eflags);
    return of;
}

/*
 * Given a pointer to an open file, locate a free entry in the file descriptor
 * table and place the file there.
 * Parameter:
 * @proc - the process
 * @of - the open file
 * @start - the lowest file descriptor to be used if available
 * @flags - FD flags to use
 * Return value:
 * the file descriptor or -1 if no file descriptor is available any more
 * Locks:
 * lock on file descriptor table of process @proc
 */
static int store_file(fs_process_t* proc, open_file_t* of, int start, int flags) {
    int fd = -1;
    int i;
    u32 eflags;
    spinlock_get(&proc->fd_table_lock, &eflags);
    /*
     * Now scan table of file descriptors to find
     * a free slot for the current process
     */
    for (i = start; i < FS_MAX_FD; i++) {
        if (0 == proc->fd_tables[i]) {
            fd = i;
            /*
             * Place pointer to open file in file descriptor table
             * and set flags
             */
            proc->fd_tables[i] = of;
            proc->fd_flags[i] = flags;
            break;
        }
    }
    spinlock_release(&proc->fd_table_lock, &eflags);
    return fd;
}

/*
 * Clone file descriptors, i.e. for all open file descriptors in a source
 * process, add an entry to the file descriptor table of the target process
 * Parameter:
 * @source - the source process
 * @target - the target process
 * Locks:
 * lock on file descriptor table of source process
 * Cross-monitor function call:
 * clone_open_file
 * Reference counts:
 * - for each file open in process @source, increase reference count by one
 */
static void clone_files(fs_process_t* source, fs_process_t* target) {
    int i;
    u32 eflags;
    spinlock_get(&source->fd_table_lock, &eflags);
    for (i = 0; i < FS_MAX_FD; i++) {
        target->fd_tables[i] = source->fd_tables[i];
        target->fd_flags[i] = source->fd_flags[i];
        if (source->fd_tables[i]) {
            clone_open_file(source->fd_tables[i]);
        }
    }
    spinlock_release(&source->fd_table_lock, &eflags);
}

/*
 * Implementation of the close system call
 * Parameter:
 * @fd - the file descriptor to be closed
 * Return value:
 * 0 upon success
 * -EBADF if @fd is not a valid file descriptor
 * Locks:
 * fd_table_lock of process structure
 * Reference counts:
 * - decrease reference count of file to which @fd refers by one
 */
int do_close(int fd) {
    FS_DEBUG("Closing file\n");
    int pid = pm_get_pid();
    open_file_t* of;
    fs_process_t* proc;
    u32 eflags;
    if ((fd < 0) || (fd >= FS_MAX_FD))
        return -EBADF;
    proc = fs_process + pid;
    spinlock_get(&proc->fd_table_lock, &eflags);
    if (0 == proc->fd_tables[fd]) {
        spinlock_release(&proc->fd_table_lock, &eflags);
        return -EBADF;
    }
    /*
     * Call fs_close on open file descriptor and
     * release file descriptor slot. Make sure to release spinlock
     * first as fs_close might invoke inode_release which in turn can
     * trigger an I/O operation
     */
    of = proc->fd_tables[fd];
    proc->fd_tables[fd] = 0;
    proc->fd_flags[fd] = 0;
    spinlock_release(&proc->fd_table_lock, &eflags);
    if (of)
        fs_close(of);
    return 0;
}


/****************************************************************************************
 * The following functions are the system call interface for:                           *
 * - open                                                                               *
 * - read                                                                               *
 * - write                                                                              *
 * - lseek                                                                              *
 * - fcntl                                                                              *
 * - dup                                                                                *
 * - stat and fstat                                                                     *
 * - umask                                                                              *
 * - pipe                                                                               *
 * - unlink                                                                             *
 * - clone                                                                              *
 * - pipe                                                                               *
 ****************************************************************************************/

/*
 * Internal utility function which will check for the existence of an inode and create a new file if requested
 * The directory in which the file is to be created or searched for is scanned with a write lock
 * to make sure that the file has not been created by another thread in the meantime
 * Parameters:
 * @create - set this to 1 if you want the file to be created if it does not yet exist
 * @excl - corresponds to O-EXCL flag
 * @path - name of new file (full path name)
 * @mode - file permissions (only bits 0777 are set)
 * @status - will be set to 1 if the file exists already
 * Return value:
 * a pointer to the newly created inode or null if the inode could not be created
 * Locks:
 * write lock on the directory in which the inode is to be created
 * Reference counts:
 * - the reference count of the inode returned is incremented by one
 * Notes:
 * If the provided path is itself a directory, this function simply delegates the call to
 * fs_get_inode_for_name
 */
static inode_t* check_inode(int create, int excl, char* path, int mode, int* status) {
    char* parent_dir;
    char* name;
    inode_t* parent_inode;
    inode_t* inode;
    *status = 0;
    /*
     * If the path refers to a directory, simply call fs_get_inode_for_name on it
     */
    if ('/' == path[strlen(path)-1]) {
        FS_DEBUG("This looks like a directory\n");
        return fs_get_inode_for_name(path);
    }
    /*
     * First determine path name of parent directory and get
     * inode of parent directory
     */
    if (0 == (parent_dir = (char*) kmalloc(strlen(path)+1))) {
        ERROR("Could not allocate buffer for path name\n");
        return 0;
    }
    if (0 == (name = (char*) kmalloc(strlen(path)+1))) {
        ERROR("Could not allocate buffer for file name\n");
        return 0;
    }
    split_path(parent_dir, path, name, 0);
    FS_DEBUG("path = %s, parent_dir = %s, name = %s\n", path, parent_dir, name);
    if (0 == (parent_inode = fs_get_inode_for_name(parent_dir))) {
        FS_DEBUG("Invalid pathname %s (path was %s)\n", parent_dir, path);
        kfree((void*) name);
        kfree((void*) parent_dir);
        return 0;
    }
    /*
     * Lock parent directory and scan for file
     */
    rw_lock_get_write_lock(&parent_inode->rw_lock);
    FS_DEBUG("Scanning parent directory for file\n");
    inode = scan_directory_by_name(parent_inode, name, strlen(name));
    if (0 == inode) {
        FS_DEBUG("File not found, create = %d\n", create);
        if (create) {
            inode = parent_inode->iops->inode_create(parent_inode, name, mode);
            if (0 == inode) {
                ERROR("Could not create new inode\n");
                kfree((void*) name);
                kfree((void*) parent_dir);
                rw_lock_release_write_lock(&parent_inode->rw_lock);
                parent_inode->iops->inode_release(parent_inode);
                return 0;
            }
        }
    }
    else {
        FS_DEBUG("Found file\n");
        *status = 1;
        /*
         * If O_CREAT and O_EXCL are both set, fail
         */
        if ((1 == excl) && (1 == create)) {
            inode->iops->inode_release(inode);
            inode = 0;
        }
    }
    /*
     * Release lock on parent inode and return reference
     */
    FS_DEBUG("Releasing lock on and reference to parent\n");
    rw_lock_release_write_lock(&parent_inode->rw_lock);
    parent_inode->iops->inode_release(parent_inode);
    kfree((void*) name);
    kfree((void*) parent_dir);
    return inode;
}


/*
 * Implementation of the open system call. This function will create a new entry in the
 * table of open files. It will also locate a free file descriptor for the current process
 * and make that file descriptor point to the newly opened file
 * Parameter:
 * @path - the name of the file to be opened
 * @flags - open flags
 * @mode - file mode
 * Return value:
 * -ENOENT if the file does not exist
 * -ENOMEM if insufficient kernel memory was available
 * -EIO if a truncate operation was requested but failed
 * -EEXIST if the file already exists and O_CREAT and O_EXCL where specified
 * -EMFILE if too many open files are open for the process
 * the file descriptor if the operation is successful
 * Locks:
 * - write lock on inode (only if file needs to be truncated)
 * Reference counts:
 * - reference count of inode representing the file is increased by one
 *
 */
int do_open(char* path, int flags, int mode) {
    int pid = pm_get_pid();
    int fd = -1;
    int rc;
    inode_t* inode;
    open_file_t* of;
    fs_process_t* self = fs_process+pid;
    int create = 0;
    int excl = 0;
    int status = 0;
    KASSERT(path);
    FS_DEBUG("Getting inode\n");
    if (flags & O_CREAT)
        create = 1;
    if (flags & O_EXCL)
        excl = 1;
    /*
     * Get inode and create it if O_CREAT is requested and the file does
     * not yet exist. If O_CREAT is not set, this will simply get an inode
     */
    if (0 == (inode = check_inode(create, excl, path, mode & 07777 & ~(self->umask), &status))) {
        /*
         * File does not exist or could not be created. If the file existed already
         * and O_CREAT and O_EXCL are set, return EEXIST else return ENOENT
         */
        if ((1 == status) && (O_CREAT & flags) && (O_EXCL & flags))
            return -EEXIST;
        return -ENOENT;
    }
    validate_inode(inode);
    FS_DEBUG("Opening new file\n");
    /*
     * Open file
     */
    if (0 == (of = fs_open(inode, flags))) {
        ERROR("fs_open returned null, assuming memory problem\n");
        inode->iops->inode_release(inode);
        return -ENOMEM;
    }
    FS_DEBUG("Allocating new file descriptor\n");
    /*
     * Allocate file descriptor and store file in file descriptor table
     * If we have not been able to find an available file descriptor, release open file and inode
     * and exit
     */
    if (-1 == (fd = store_file(self, of, 0, 0))) {
        fs_close(of);
        inode->iops->inode_release(inode);
        return -EMFILE;
    }
    /*
     * Truncate the file if either O_RDWR or O_WRONLY is specified and the file is a regular file
     * and O_TRUNC is specified in the flags
     */
    if ((S_ISREG(inode->mode)) && (flags & (O_RDWR+O_WRONLY)) && (flags & O_TRUNC)) {
        FS_DEBUG("Truncating file\n");
        rw_lock_get_write_lock(&inode->rw_lock);
        if ((rc = inode->iops->inode_trunc(inode))) {
            FS_DEBUG("Truncate failed with rc %d\n", rc);
            rw_lock_release_write_lock(&inode->rw_lock);
            inode->iops->inode_release(inode);
            fs_close(of);
            return -EIO;
        }
        FS_DEBUG("inode->size = %d\n", inode->size);
        rw_lock_release_write_lock(&inode->rw_lock);
    }
    inode->iops->inode_release(inode);
    return fd;
}

/*
 * Implementation of the mkdir system call
 * Parameter:
 * @path - full pathname of new directory
 * @mode - access mode, will be masked with 07777
 * Return value:
 * -ENOMEM if needed temporary memory could not be allocated
 * -ENOENT if the specified path does not exist
 * -EIO if an unexpected I/O error occurred
 * -EEXIST if the specified directory exists
 * Locks:
 * write lock on directory in which dir is to be created
 */
int do_mkdir(char* path, int mode) {
    char* parent_dir;
    char* name;
    inode_t* parent_inode;
    inode_t* inode;
    fs_process_t* self = fs_process + pm_get_pid();
    /*
     * First determine path name of parent directory and get
     * inode of parent directory
     */
    if (0 == (parent_dir = (char*) kmalloc(strlen(path) + 1))) {
        ERROR("Could not allocate buffer for path name\n");
        return -ENOMEM;
    }
    if (0 == (name = (char*) kmalloc(strlen(path) + 1))) {
        ERROR("Could not allocate buffer for file name\n");
        kfree ((void*) parent_dir);
        return -ENOMEM;
    }
    split_path(parent_dir, path, name, 1);
    FS_DEBUG("path = %s, parent_dir = %s, name = %s\n", path, parent_dir, name);
    if (0 == strlen(name))
        return -ENOENT;
    /*
     * Lock parent directory and scan for file
     */
    if (0 == (parent_inode = fs_get_inode_for_name(parent_dir))) {
        FS_DEBUG("Invalid pathname %s (path was %s)\n", parent_dir, path);
        kfree((void*) name);
        kfree((void*) parent_dir);
        return -ENOENT;
    }
    rw_lock_get_write_lock(&parent_inode->rw_lock);
    FS_DEBUG("Scanning parent directory for file\n");
    inode = scan_directory_by_name(parent_inode, name, strlen(name));
    if (0 == inode) {
        /*
         * Directory does not exist yet, proceed and add new directory. Note that inode_create is expected to perform
         * all necessary operations (like handling .. and .)
         */
        if (0 == (inode = parent_inode->iops->inode_create(parent_inode, name, (mode & 07777 & ~(self->umask)) + S_IFDIR))) {
            kfree((void*) parent_dir);
            kfree((void*) name);
            inode->iops->inode_release(inode);
            rw_lock_release_write_lock(&parent_inode->rw_lock);
            parent_inode->iops->inode_release(parent_inode);
            return -EIO;
        }
    }
    else {
        /*
         * Directory exists, return error
         */
        kfree((void*) parent_dir);
        kfree((void*) name);
        inode->iops->inode_release(inode);
        rw_lock_release_write_lock(&parent_inode->rw_lock);
        parent_inode->iops->inode_release(parent_inode);
        return -EEXIST;
    }
    /*
     * Free memory used for path components again
     */
    kfree((void*) parent_dir);
    kfree((void*) name);
    /*
     * and release inodes and lock
     */
    inode->iops->inode_release(inode);
    rw_lock_release_write_lock(&parent_inode->rw_lock);
    parent_inode->iops->inode_release(parent_inode);
    return 0;
}

/*
 * Implementation of the dup system call
 * Parameter:
 * @fd - the file descriptor to be duplicated
 * @start - where to start the search for an open file descriptor
 * Return value:
 * a non-negative file descriptor if the operation was successful
 * -EBADF if the file descriptor is not valid
 * -EMFILE if there is no free file descriptor
 * Reference counts:
 * - increase reference count of existing open file by one
 */
int do_dup(int fd, int start) {
    FS_DEBUG("Dup on file descriptor %d\n", fd);
    int pid = pm_get_pid();
    int new_fd = -1;
    open_file_t* of = 0;
    if ((fd < 0) || (fd >= FS_MAX_FD))
        return -EBADF;
    if ((start < 0) || (start >= FS_MAX_FD))
        return -EBADF;
    if (0 == (of = get_file(fs_process + pid, fd))) {
        return -EBADF;
    }
    /*
     * Locate free file descriptor and store open file there
     */
    if (-1 == (new_fd = store_file(fs_process + pid, of, start, 0))) {
        fs_close(of);
        return -EMFILE;
    }
    return new_fd;
}

/*
 * Implementation of the read system call
 * Parameter:
 * @fd - file descriptor
 * @buffer - buffer to which read data is to be written
 * @bytes - number of bytes to read
 * Return value:
 * -EIO if an I/O error occurred
 * -EINVAL if the file is a character device and the device is not valid
 * -EPAUSE if the read has been interrupted by a signal
 * -EOVERFLOW if the specified byte count would lead to an overflow of the file offset
 * -EBADF if the file descriptor is not valid
 * number of bytes read upon success
 */
ssize_t do_read(int fd, void* buffer, size_t bytes) {
    int pid = pm_get_pid();
    open_file_t* of;
    ssize_t rc;
    if (0 == (of = get_file(fs_process + pid, fd))) {
        return -EBADF;
    }
    rc = fs_read(of, bytes, buffer);
    /*
     * Call fs_close to decrease reference count again
     * which has been increased by get_file
     */
    fs_close(of);
    return rc;
}

/*
 * Implementation of the readdir system call
 * Parameter:
 * @fd - file descriptor
 * @direntry - buffer to which read data is to be written
 * Return value:
 * -1 if no further directory entry could be found
 * -EIO if an I/O error occured
 * -EINVAL if the file descriptor is not a directory
 * -EBADF if the file descriptor is not valid
 * -EOVERFLOW if the read would lead to an overflow of the cursor position
 * 0 upon success
 */
ssize_t do_readdir(int fd, direntry_t* direntry) {
    int pid = pm_get_pid();
    open_file_t* of;
    ssize_t rc;
    if (0 == (of = get_file(fs_process + pid, fd))) {
        return -EBADF;
    }
    rc = fs_readdir(of, direntry);
    /*
     * Call fs_close to decrease reference count again
     */
    fs_close(of);
    return rc;
}

/*
 * Implementation of the write system call
 * Parameter:
 * @fd - file descriptor
 * @buffer - buffer containing data to be written
 * @bytes - number of bytes to write
 * Return value:
 * -EBADF if the file descriptor is not valid
 * -EINVAL if the device is not valid
 * -EIO if the operation failed
 * -EPAUSE if the write was interrupted by a signal
 * -EPIPE if it was tried to write to a broken pipe
 * number of bytes written upon success
 */
ssize_t do_write(int fd, void* buffer, size_t bytes) {
    int pid = pm_get_pid();
    open_file_t* of;
    ssize_t rc;
    if (0 == (of = get_file(fs_process + pid, fd))) {
        return -EBADF;
    }
    rc = fs_write(of, bytes, buffer);
    /*
     * Call close to decrease reference count again
     */
    fs_close(of);
    return rc;
}

/*
 * Implementation of the lseek system call
 * Parameter:
 * @fd - file descriptor
 * @offset - offset
 * @whence - how to interpret offset (see comments on fs_lseek)
 * Return value:
 * -EBADF if the file descriptor is not valid
 * -EINVAL if the argument whence is not valid
 * -ESPIPE if the file descriptor refers to a pipe
 * the new absolute offset into the file upon success
 */
ssize_t do_lseek(int fd, off_t offset, int whence) {
    int pid = pm_get_pid();
    open_file_t* of;
    ssize_t rc;
    if (0 == (of = get_file(fs_process + pid, fd))) {
        return -EBADF;
    }
    rc = fs_lseek(of, offset, whence);
    fs_close(of);
    return rc;
}


/*
 * Implementation of the fcntl system call
 * Parameter:
 * @fd - the file descriptor
 * @cmd - the command to be performed
 * @arg - integer argument needed by some commands
 * Return value:
 * 0 or a positive value if the operation is successful
 * -EBADF if the file descriptor is not valid
 * -EINVAL if the command is not valid
 */
int do_fcntl(int fd, int cmd, int arg) {
    int rc = 0;
    u32 flags;
    FS_DEBUG("Fcntl on file descriptor %d\n", fd);
    int pid = pm_get_pid();
    open_file_t* of = 0;
    if ((fd < 0) || (fd >= FS_MAX_FD)) {
        return -EBADF;
    }
    /*
     * Divert call to do_dup if F_DUPFD is requested
     */
    if (F_DUPFD == cmd)
        return do_dup(fd, arg);
    if (0 == (of = get_file(fs_process + pid, fd)))
        return -EBADF;
    /*
     * Do processing depending on requested operation
     */
    switch (cmd) {
        case F_GETFD:
            rc = atomic_load(fs_process[pid].fd_flags+fd);
            break;
        case F_SETFD:
            atomic_store(fs_process[pid].fd_flags+fd, arg);
            rc = 0;
            break;
        case F_GETFL:
            rc = atomic_load(&(of->flags));
            break;
        case F_SETFL:
            /*
             * Note that we do not need to lock the file flags, as we
             * do not modify the first two bits and simply overwrite the other bits.
             * Using the temporary variable flags makes sure that the assignment is an atomic
             * 32-bit operation
             */
            flags = (atomic_load(&(of->flags)) & O_ACCMODE) + (arg & ~O_ACCMODE);
            atomic_store(&(of->flags), flags);
            rc = 0;
            break;
        default:
            rc = -EINVAL;
            break;
    }
    if (of)
        fs_close(of);
    return rc;
}



/*
 * Utility function to stat an inode
 * Parameter:
 * @inode - the inode
 * @buffer - the struct stat buffer to which we copy the information
 * Locks:
 * rw_lock on inode
 */
static void perform_stat(inode_t* inode, struct __ctOS_stat* buffer) {
    /*
     * Lock inode to make sure that we have a consistent state
     */
    rw_lock_get_read_lock(&inode->rw_lock);
    /*
     * Copy fields from inode to stat structure
     */
    buffer->st_dev = inode->dev;
    buffer->st_ino = inode->inode_nr;
    buffer->st_mode = inode->mode;
    buffer->st_nlink = inode->link_count;
    buffer->st_uid = inode->owner;
    buffer->st_gid = inode->group;
    buffer->st_size = inode->size;
    buffer->st_atime = inode->atime;
    buffer->st_mtime = inode->mtime;
    buffer->st_ctime = 0;
    /*
     * Release lock again
     */
    rw_lock_release_read_lock(&inode->rw_lock);
}

/*
 * Implementation of the stat system call.
 * Parameter:
 * @path - the name of the file
 * @buffer - struct stat to fill
 * Return value:
 * -ENOENT if the specified path name is not valid
 * 0 upon success
 */
int do_stat(char* path, struct __ctOS_stat*  buffer) {
    inode_t* inode;
    KASSERT(path);
    FS_DEBUG("Getting inode for path name\n");
    if (0 == (inode = fs_get_inode_for_name(path))) {
        return -ENOENT;
    }
    perform_stat(inode, buffer);
    inode->iops->inode_release(inode);
    return 0;
}

/*
 * Implementation of the fstat system call.
 * Parameter:
 * @fd - the file descriptor
 * @buffer - struct stat to fill
 * Return value:
 * -ENOENT if the specified path name is not valid
 * -EBADF if the file descriptor is not valid
 * 0 upon success
 */
int do_fstat(int fd, struct __ctOS_stat*  buffer) {
    int pid = pm_get_pid();
    open_file_t* of = 0;
    if ((fd < 0) || (fd >= FS_MAX_FD))
        return -EBADF;
    /*
     * First deference file descriptor to get a pointer to the inode
     */
    if (0 == (of = get_file(fs_process + pid, fd)))
        return -EBADF;
    if (0 == of->inode) {
        fs_close(of);
        return -EBADF;
    }
    /*
     * Do stat and drop reference again
     */
    perform_stat(of->inode, buffer);
    fs_close(of);
    return 0;
}

/*
 * Update modification and access time for an inode
 * Parameter:
 * @file - the file
 * @times - new access and modification time
 * Locks:
 * write lock on inode
 */
int do_utime(char* file, struct utimbuf* times) {
    time_t atime;
    time_t mtime;
    int rc;
    inode_t* inode;
    /*
     * Get reference to inode
     */
    if (0 == (inode = fs_get_inode_for_name(file))) {
        return -ENOENT;
    }
    /*
     * Get write lock on inode
     */
    rw_lock_get_write_lock(&inode->rw_lock);
    /*
     * Perform update and flush
     */
    if (times) {
        atime = times->actime;
        mtime = times->modtime;
    }
    else {
        atime = do_time(0);
        mtime = atime;
    }
    inode->atime = atime;
    inode->mtime = mtime;
    if (inode->iops->inode_flush) {
        rc = inode->iops->inode_flush(inode);
    }
    else {
        rc = -EINVAL;
    }
    /*
     * Release write lock on inode
     */
    rw_lock_release_write_lock(&inode->rw_lock);
    return rc;
}

/*
 * Change the file mode bits (bits 0 - 11) of the mode field of an
 * inode
 * Parameter:
 * Locks:
 * write lock on the inode
 * Return value:
 * 0 upon success
 * -ENOTENT if the path does not refer to a valid file
 */
int do_chmod(char* path, mode_t mode) {
    int rc;
    inode_t* inode;
    /*
     * Get reference to inode
     */
    if (0 == (inode = fs_get_inode_for_name(path))) {
        return -ENOENT;
    }
    /*
     * Get write lock on inode
     */
    rw_lock_get_write_lock(&inode->rw_lock);
    /*
     * Perform update and flush
     */
    inode->mode = (inode->mode & S_IFMT) + (mode & ~S_IFMT);
    if (inode->iops->inode_flush) {
        rc = inode->iops->inode_flush(inode);
    }
    else {
        rc = -EINVAL;
    }
    /*
     * Release write lock on inode
     */
    rw_lock_release_write_lock(&inode->rw_lock);
    return rc;
}

/*
 * Return the previous umask of a process and set the new umask
 * Parameter:
 * @umask - the new umask
 * Return value:
 * the old umask
 * Locks:
 * lock on file system process structure
 */
mode_t do_umask(mode_t umask) {
    mode_t old_umask;
    u32 eflags;
    pid_t pid = pm_get_pid();
    fs_process_t* self = fs_process+pid;
    mode_t file_permission_bits = S_IRWXU | S_IRWXG | S_IRWXO;
    spinlock_get(&self->spinlock, &eflags);
    old_umask = self->umask;
    self->umask = umask & file_permission_bits;
    spinlock_release(&self->spinlock, &eflags);
    return old_umask;
}

/*
 * Unlink an inode
 * Parameter:
 * @inode - the inode to be unlinked
 * @dir - the directory in which the inode is located
 * @name - the name of the directory entry to be removed
 * Return value:
 * 0 upon success
 * EIO if an I/O error occured
 * ENOENT if the entry could not be found
 * Locks:
 * rw_lock on the directory
 * Only call this function if you hold the lock on the inode
 */
static int unlink_inode(inode_t* inode, inode_t* dir, char* name) {
    int rc;
    /*
     * Get lock on directory
     */
    rw_lock_get_write_lock(&dir->rw_lock);
    /*
     * Call unlink. Note that the function inode_unlink will - per convention - not
     * access the read/write locks in the abstract inode structure, so there is no
     * danger of a deadlock here
     */
    rc = inode->iops->inode_unlink(dir, name, 0);
    /*
     * Release lock again
     */
    rw_lock_release_write_lock(&dir->rw_lock);
    return rc;
}

/*
 * Unlink a file. This function will remove the directory entry for the
 * given file or directory.
 * Parameter:
 * @path - the name of the file
 * Return value:
 * 0 if operation was successful
 * ENOENT if the file could not be found
 * EIO if the unlink operation could not be completed due to an unexpected error
 * ENOMEM if we are running out of memory
 * EBUSY if the file is a directory and a mount point which is in use
 * EEXIST if the file is a directory which is not empty or has additional hard links
 * EEXIST if an attempt is made to remove the root directory
 * EINVAL if the last path component is . or ..
 * Locks:
 * lock on inode
 * Cross-monitor function calls:
 * unlink_inode
 */
int do_unlink(char* path) {
    inode_t* inode = 0;
    char* parent_dir;
    inode_t* parent_inode;
    char* name;
    int rc;
    /*
     * First get a reference to the inode
     */
    if (0 == (inode = fs_get_inode_for_name(path))) {
        FS_DEBUG("Could not get reference to inode\n");
        return ENOENT;
    }
    /*
     * Determine directory in which inode is located
     */
    if (0 == (parent_dir = (char*) kmalloc(strlen(path)+1))) {
        ERROR("Could not allocate buffer for path\n");
        inode->iops->inode_release(inode);
        return ENOMEM;
    }
    if (0 == (name = (char*) kmalloc(strlen(path)+1))) {
        ERROR("Could not allocate buffer for file name\n");
        kfree((void*) parent_dir);
        inode->iops->inode_release(inode);
        return ENOMEM;
    }
    split_path(parent_dir, path, name, 0);
    if (0 == (parent_inode = fs_get_inode_for_name(parent_dir))) {
        FS_DEBUG("Invalid pathname %s for parent directory\n", parent_dir);
        kfree((void*) name);
        kfree((void*) parent_dir);
        inode->iops->inode_release(inode);
        return ENOENT;
    }
    kfree((void*) parent_dir);
    /*
     * If the inode is a directory, the name should not be ".." or "."
     */
    if (S_ISDIR(inode->mode)) {
        if ((0 == strcmp(name, "..")) || (0 == strcmp(name, "."))) {
            FS_DEBUG("Name of directory is .. or .\n");
            kfree((void*) name);
            inode->iops->inode_release(inode);
            parent_inode->iops->inode_release(parent_inode);
            return EINVAL;
        }
    }
    /*
     * If parent dir and inode are equal, we are trying to remove the
     * root directory
     */
    if (parent_inode == inode) {
        FS_DEBUG("Trying to remove /\n");
        kfree((void*) name);
        parent_inode->iops->inode_release(parent_inode);
        inode->iops->inode_release(inode);
        return EEXIST;
    }
    /*
     * Get lock on inode - this is needed as unlink will modify
     * the link count of the inode within the lower level drivers
     */
    rw_lock_get_write_lock(&inode->rw_lock);
    rc = unlink_inode(inode, parent_inode, name);
    FS_DEBUG("RC of unlink inode is %d\n", rc);
    rw_lock_release_write_lock(&inode->rw_lock);
    /*
     * Release reference to inode and parent inode
     */
    parent_inode->iops->inode_release(parent_inode);
    inode->iops->inode_release(inode);
    kfree((void*) name);
    if (rc)
        return rc;
    return 0;
}

/*
 * Given two inodes, return true if the first inode is a direct or indirect
 * parent of the second inode, assuming that both inodes are on the same device
 * Parameter:
 * @parent - parent inode
 * @child - child inode
 * Note that this function calls scan_directory_by_name_lock and thus implicitly
 * locks the directories above child recursively
 */
static int is_parent(inode_t* parent, inode_t* child) {
    inode_t* current_inode = child->iops->inode_clone(child);
    inode_t* next_inode = 0;
    /*
     * Return true if both inodes are equal
     */
    if (INODE_EQUAL(parent, child)) {
        FS_DEBUG("parent and child are equal, returning FALSE\n");
        current_inode->iops->inode_release(current_inode);
        return 1;
    }
    while (1) {
        /*
         * Get parent of current inode
         */
        FS_DEBUG("Scanning directory %d for .. entry\n", current_inode->inode_nr);
        next_inode = scan_directory_by_name_lock(current_inode, "..", 2);
        /*
         * If we cannot get the .. entry, something went wrong
         */
        if (0 == next_inode) {
            current_inode->iops->inode_release(current_inode);
            return 0;
        }
        /*
         * If this matches the parent, we have a match and return
         */
        if (INODE_EQUAL(next_inode, parent)) {
            current_inode->iops->inode_release(current_inode);
            next_inode->iops->inode_release(next_inode);
            return 1;
        }
        /*
         * If the next inode is equal to the current inode, we have reached the top
         * of the mounted file system
         */
        if (INODE_EQUAL(next_inode, current_inode)) {
            current_inode->iops->inode_release(current_inode);
            next_inode->iops->inode_release(next_inode);
            return 0;
        }
        /*
         * In all other cases proceed
         */
        current_inode->iops->inode_release(current_inode);
        current_inode = next_inode;
        next_inode = 0;
    }
    return 0;
}

/*
 * Rename a file
 * Parameter:
 * @old - path to old file
 * @new - path to new file
 * Return values:
 * 0 upon success
 * -ENOENT if the old file does not exist
 * -EISDIR if the new file is a directory, but old is not
 * -ENOMEM if we are running out of memory
 * -EXDEV if old and new file are not located on the same file system
 * -ENOTDIR if the path prefix of new is not an existing directory
 * -ENOTDIR if the old path names a directory but the new path is an existing file
 * -EXDEV if old and new are not on the same file system
 * -EEXIST if the target is a non-empty directory
 * -EINVAL if old is a directory in the path prefix of new
 */
int do_rename(char* old, char* new) {
    inode_t* old_inode = 0;
    inode_t* new_inode = 0;
    char* parent_dir = 0;
    inode_t* new_parent_inode = 0;
    inode_t* old_parent_inode = 0;
    char* old_name = 0;
    char* new_name = 0;
    int rc;
    int result = 0;
    /*
     * Resolve old and new inode
     */
    FS_DEBUG("Getting old and new inode\n");
    old_inode = fs_get_inode_for_name(old);
    new_inode = fs_get_inode_for_name(new);
    /*
     * If the old file does not exist, this is an error
     */
    if (0 == old_inode) {
        FS_DEBUG("Old file does not exist\n");
        INODE_RELEASE(new_inode);
        return -ENOENT;
    }
    /*
     * We can only move directories or regular files
     */
    if ((!S_ISDIR(old_inode->mode)) && (!S_ISREG(old_inode->mode))) {
        result = -EINVAL;
        goto exit;
    }
    /*
     * If both old and new resolve to the same existing file, do nothing. If both exist and
     * one of them is a directory, but the other one not, return error. Also verify that the new file
     * is not a special file
     */
    if (new_inode) {
        if ((old_inode->inode_nr == new_inode->inode_nr) && (old_inode->dev == new_inode->dev)) {
            result = 0;
            goto exit;
        }
        if (S_ISDIR(new_inode->mode) && (!S_ISDIR(old_inode->mode))) {
            result = -EISDIR;
            goto exit;
        }
        if (!S_ISDIR(new_inode->mode) && (S_ISDIR(old_inode->mode))) {
            result = -ENOTDIR;
            goto exit;
        }
        if ((!S_ISDIR(new_inode->mode)) && (!S_ISREG(new_inode->mode))) {
            result = -EINVAL;
            goto exit;
        }
    }
    /*
     * Resolve path components of new
     */
    FS_DEBUG("Resolving path components\n");
    if (0 == (parent_dir = (char*) kmalloc(MAX(strlen(new), strlen(old)) + 1))) {
        ERROR("Could not allocate buffer for path\n");
        result = -ENOMEM;
        goto exit;
    }
    if (0 == (old_name = (char*) kmalloc(MAX(strlen(new), strlen(old)) + 1))) {
        ERROR("Could not allocate buffer for file name\n");
        result = -ENOMEM;
        goto exit;
    }
    if (0 == (new_name = (char*) kmalloc(MAX(strlen(new), strlen(old)) + 1))) {
        ERROR("Could not allocate buffer for file name\n");
        result = -ENOMEM;
        goto exit;
    }
    split_path(parent_dir, new, new_name, 0);
    if (0 == strlen(new_name)) {
        result = -ENOENT;
        goto exit;
    }
    if (S_ISREG(old_inode->mode) || S_ISDIR(old_inode->mode)) {
        /*
         * To perform the actual renaming operation for a file or directory, we proceed as follows
         * a) we first lock the inode old
         * b) we then lock the directory in which new is going to be located
         * c) and add a link to new to this directory
         * d) if that succeeds, we remove the link to old
         */
        FS_DEBUG("Renaming regular file or directory\n");
        /*
         * First get link to new and old parent inode
         */
        if (0 == (new_parent_inode = fs_get_inode_for_name(parent_dir))) {
            result = -ENOTDIR;
            goto exit;
        }
        split_path(parent_dir,old, old_name, 0);
        if (0 == strlen(old_name)) {
            result = -ENOENT;
            goto exit;
        }
        /*
         * It is not allowed to rename . or ..
         */
        if ((0 == strcmp(old_name, ".")) || (0 == strcmp(old_name, ".."))) {
            result = -EINVAL;
            goto exit;
        }
        if (0 == (old_parent_inode = fs_get_inode_for_name(parent_dir))) {
            result =  -ENOTDIR;
            goto exit;
        }
        FS_DEBUG("New parent inode is %d, old parent inode is %d\n", new_parent_inode->inode_nr,
                old_parent_inode->inode_nr);
        /*
         * If the parent inode of new is not on the same device as old, fail
         */
        if (new_parent_inode->dev != old_inode->dev) {
            result = -EXDEV;
            goto exit;
        }
        /*
         * Return an error if the old pathname is a directory which is in the path of
         * the new directory
         */
        if (is_parent(old_inode, new_parent_inode)) {
            result = -EINVAL;
            goto exit;
        }
        /*
         * Lock old inode
         */
        FS_DEBUG("Locking old inode (%d)\n", old_inode->inode_nr);
        rw_lock_get_write_lock(&old_inode->rw_lock);
        /*
         * Now lock new parent inode
         */
        rw_lock_get_write_lock(&new_parent_inode->rw_lock);
        /*
         * If the new file exists, remove it
         */
        if (new_inode) {
            if ((rc = new_parent_inode->iops->inode_unlink(new_parent_inode, new_name, 0))) {
                /*
                 * If entry has been removed by another process, ignore error
                 */
                if (ENOENT != rc) {
                    result = - rc;
                    goto exit;
                }
            }
        }
        /*
         * Add directory entry pointing to old to new directory
         */
        FS_DEBUG("Adding new link for %s to target directory\n", new_name);
        rc = new_parent_inode->iops->inode_link(new_parent_inode, new_name, old_inode);
        /*
         * Release lock on new parent inode again
         */
        rw_lock_release_write_lock(&new_parent_inode->rw_lock);
        /*
         * If the link operation has failed, return
         */
        if (rc) {
            FS_DEBUG("Return code of inode_link is %d\n", rc);
            result = -rc;
            goto exit;
        }
        /*
         * Now get lock on old parent directory
         */
        rw_lock_get_write_lock(&old_parent_inode->rw_lock);
        /*
         * and remove old directory entry
         */
        FS_DEBUG("Removing old directory entry %s\n", old_name);
        rc = old_parent_inode->iops->inode_unlink(old_parent_inode, old_name, FS_UNLINK_FORCE + FS_UNLINK_NOTRUNC);
        if (rc) {
            FS_DEBUG("Return code of unlink: %d\n", rc);
        }
        /*
         * Release lock again
         */
        rw_lock_release_write_lock(&old_parent_inode->rw_lock);
        /*
         * Return error if operation failed
         */
        if (rc) {
            result = - rc;
            goto exit;
        }
        /*
         * Release lock on old inode again
         */
        rw_lock_release_write_lock(&old_inode->rw_lock);

    }
    else {
        /*
         * not allowed
         */
        result = -EINVAL;
    }
exit:
    /*
     * Drop references again
     */
    INODE_RELEASE(old_inode);
    INODE_RELEASE(new_inode);
    INODE_RELEASE(old_parent_inode);
    INODE_RELEASE(new_parent_inode);
    /*
     * and free used memory
     */
    if (old_name)
        kfree((void*) old_name);
    if (new_name)
        kfree((void*) new_name);
    if (parent_dir)
        kfree((void*) parent_dir);
    return result;
}

/*
 * Clone the process table entry of a given process
 * Parameters:
 * @source_pid - the id of the source process
 * @target_pid - the id of the target process
 * Locks:
 * lock on source process
 */
void fs_clone(u32 source_pid, u32 target_pid) {
    u32 eflags;
    fs_process_t* source;
    fs_process_t* target;
    if ((source_pid >= PM_MAX_PROCESS) || (target_pid >= PM_MAX_PROCESS))
        return;
    source = fs_process + source_pid;
    target = fs_process + target_pid;
    /*
     * Clone file descriptors
     */
    clone_files(source, target);
    /*
     * Clone remaining fields
     */
     spinlock_get(&source->spinlock, &eflags);
     if (source->cwd)
         target->cwd = source->cwd->iops->inode_clone(source->cwd);
     else
         target->cwd=0;
     target->umask = source->umask;
     spinlock_release(&source->spinlock, &eflags);
}

/*
 * Create a pipe and return the file descriptors for the reading and
 * writing end
 * Parameters:
 * @fd - array of two file descriptors. In fd[0], the file descriptor for the reading end will be placed
 * @flags - file descriptor flags
 * Return value:
 * 0 if the operation was successful
 * ENOMEM if no memory could be allocated
 * EMFILE if there are too many open files
 */
int do_pipe(int fd[2], int flags) {
    inode_t* inode = 0;
    pipe_t* pipe = 0;
    open_file_t* reading_end = 0;
    open_file_t* writing_end = 0;
    FS_DEBUG("do_pipe called\n");
    pid_t pid = pm_get_pid();
    fd[0]=-1;
    fd[1]=-1;
    /*
     * First we create an inode
     */
    FS_DEBUG("Creating inode\n");
    if (0 == (inode = (inode_t*) kmalloc(sizeof(inode_t))))
        return ENOMEM;
    /*
     * Set fields applicable for a pipe
     */
    memset((void*) inode, 0, sizeof(inode_t));
    inode->dev = DEVICE_NONE;
    inode->mode = S_IFIFO;
    inode->owner = do_geteuid();
    inode->group = do_getegid();
    inode->s_dev = DEVICE_NONE;
    /*
     * Create a pipe
     */
    FS_DEBUG("Creating pipe\n");
    if (0 == (pipe = fs_pipe_create())) {
        kfree(inode);
        return ENOMEM;
    }
    /*
     * Create an open file for the reading end
     */
    FS_DEBUG("Creating open file\n");
    if (0 == (reading_end = fs_open(inode, O_RDONLY))) {
        kfree(inode);
        kfree(pipe);
        return ENOMEM;
    }
    /*
     * and link it to the pipe
     */
    reading_end->pipe = pipe;
    fs_pipe_connect(pipe, PIPE_READ);
    /*
     * Now scan table of file descriptors to find
     * a free slot for the current process
     */
    FS_DEBUG("Allocating new file descriptor for reading end\n");
    fd[0] = store_file(fs_process + pid, reading_end, 0, flags);
    if (-1 == fd[0]) {
        fs_close(reading_end);
        return EMFILE;
    }
    /*
     * Do the same for the writing end. If this fails, we call
     * fs_close on the pipe which will clean up the pipe and the inode
     */
    if (0 == (writing_end = fs_open(inode, O_WRONLY))) {
        fs_close(reading_end);
        return ENOMEM;
    }
    writing_end->pipe = pipe;
    fs_pipe_connect(pipe, PIPE_WRITE);
    FS_DEBUG("Allocating new file descriptor for writing end\n");
    fd[1] = store_file(fs_process + pid, writing_end, 0, flags);
    if (-1 == fd[1]) {
        fs_close(reading_end);
        do_close(fd[0]);
        return EMFILE;
    }
    return 0;
}

/****************************************************************************************
 * The following functions realize the file system related part of the terminal         *
 * interface                                                                            *
 ****************************************************************************************/

/*
 * Implementation of the isatty system
 * Parameter:
 * @fd - the file descriptor to be duplicated
 * Return value:
 * 1 if the file descriptor refers to a tty
 * 0 otherwise
 */
int do_isatty(int fd) {
    FS_DEBUG("Isatty on file descriptor %d\n", fd);
    int pid = pm_get_pid();
    open_file_t* of;
    dev_t dev;
    if ((fd < 0) || (fd >= FS_MAX_FD))
        return 0;
    if (0 == (of = get_file(fs_process + pid, fd)))
        return 0;
    if (0 == of->inode) {
        fs_close(of);
        return 0;
    }
    if (!S_ISCHR(of->inode->mode)) {
        fs_close(of);
        return 0;
    }
    dev = of->inode->s_dev;
    fs_close(of);
    return (MAJOR_TTY == MAJOR(dev));
}

/*
 *
 * Set/get termios settings of the terminal referred to by the file
 * descriptor @fd
 * Parameters:
 * @fd - the file descriptor
 * @termios_p - settings to be used
 * @action - TCSANOW, TCSADRAIN or TCSAFLUSH, only relevant when @set is 1
 * @set - 0 for get, 1 for set
 * Return value:
 * 0 upon successful completion
 * -ENOTTY if the file descriptor is not a terminal
 * -EBADF if the file descriptor is not valid
 */
static int tcgs_attr(int fd, int action, struct termios* termios_p, int set) {
    int rc;
    open_file_t* of;
    dev_t dev;
    fs_process_t* self = fs_process + pm_get_pid();
    of = get_file(self, fd);
    if (0 == of)
        return -EBADF;
    if (0 == of->inode)
        return -EBADF;
    /*
     * Return -ENOTTY if the device is not a TTY
     */
    dev = of->inode->s_dev;
    if ((!S_ISCHR(of->inode->mode)) || (MAJOR(dev) != MAJOR_TTY))
        return -ENOTTY;
    if (1 == set)
        rc = tty_tcsetattr(MINOR(dev), action, termios_p);
    else
        rc = tty_tcgetattr(MINOR(dev), termios_p);
    fs_close(of);
    return rc;
}


/*
 * tcgetattr system call
 * Get termios settings of the terminal referred to by the file descriptor
 * fd
 * Parameters:
 * @fd - the file descriptor
 * @termios_p - this is where the result is stored
 * Return value:
 * 0 upon successful completion
 * -ENOTTY if the file descriptor is not a terminal
 * -EBADF if the file descriptor is not valid
 */
int do_tcgetattr(int fd, struct termios* termios_p) {
    return tcgs_attr(fd, 0, termios_p, 0);
}

/*
 * tcsetattr system call
 * Set termios settings of the terminal referred to by the filedescriptor
 * fd
 * Parameters:
 * @fd - the file descriptor
 * @action - TCSANOW, TCSADRAIN or TCSAFLUSH
 * @termios_p - settings to be used
 * Return value:
 * 0 upon successful completion
 * -ENOTTY if the file descriptor is not a terminal
 * -EBADF if the file descriptor is not valid
 */
int do_tcsetattr(int fd, int action, struct termios* termios_p) {
    return tcgs_attr(fd, action, termios_p, 1);
}


/*
 * Get or set the process group of a terminal device
 * Parameters:
 * @fd - the file descriptor
 * @pgrp - pointer to an unsigned int where we store the result
 * @mode - 0 = get process group, 1 = set process group
 * Return value:
 * 0 upon success
 * -EBADF if the file descriptor is not valid or does not refer to a terminal device
 */
int fs_sgpgrp(int fd, u32* pgrp, int mode) {
    open_file_t* of;
    int rc = 0;
    if (0 == pgrp)
        return -EINVAL;
    if ((fd < 0) || (fd >= FS_MAX_FD))
        return -EINVAL;
    if (0 == (of = get_file(fs_process + pm_get_pid(), fd)))
        return -EBADF;
    if (0 == of->inode) {
        fs_close(of);
        return -EBADF;
    }
    if (!S_ISCHR(of->inode->mode)) {
        fs_close(of);
        return -EBADF;
    }
    /*
     * Return -ENOTTY if the device is not a TTY
     */
    if (MAJOR(of->inode->s_dev) != MAJOR_TTY) {
        fs_close(of);
        return -ENOTTY;
    }
    /*
     * Return -ENOTTY if the device is not the controlling terminal of the
     * process or the process does not have a controlling terminal
     */
    if ((of->inode->s_dev != pm_get_cterm()) && (mode)) {
        fs_close(of);
        return -ENOTTY;
    }
    /*
     * Return -EPERM if the process group is not a process group within the
     * session of the current process and mode is 1
     */
    if (mode && (0 == pm_pgrp_in_session(pm_get_pid(), *pgrp))) {
        fs_close(of);
        return -EPERM;
    }
    if (0 == mode) {
        *pgrp = tty_getpgrp(MINOR(of->inode->s_dev));
    }
    else {
        rc = tty_setpgrp(MINOR(of->inode->s_dev), *pgrp);
    }
    fs_close(of);
    return rc;
}

/****************************************************************************************
 * The next section contains functions related to sockets. As sockets are addressed via *
 * file descriptors, they are managed by the FS layer, but operations are forwarded to  *
 * the networking stack
 ****************************************************************************************/

/*
 * Create a new socket
 * Parameter:
 * @domain - address family
 * @type - type of socket (stream, datagram, raw)
 * @proto - protocol
 * Return value:
 * a file descriptor if the operation was successful
 * -ENOMEM if no memory could be allocated for the socket
 * -EMFILE if no available file descriptor could be found
 *
 */
int do_socket(int domain, int type, int proto) {
    inode_t* inode = 0;
    open_file_t* file = 0;
    int fd;
    socket_t* socket;
    FS_DEBUG("do_socket called\n");
    pid_t pid = pm_get_pid();
    /*
     * First we create an inode
     */
    FS_DEBUG("Creating inode\n");
    if (0 == (inode = (inode_t*) kmalloc(sizeof(inode_t))))
        return -ENOMEM;
    /*
     * Set fields applicable for a socket
     */
    memset((void*) inode, 0, sizeof(inode_t));
    inode->dev = DEVICE_NONE;
    inode->mode = S_IFSOCK;
    inode->owner = do_geteuid();
    inode->group = do_getegid();
    inode->s_dev = DEVICE_NONE;
    /*
     * Create a socket
     */
    FS_DEBUG("Creating socket\n");
    if (0 == (socket = net_socket_create(domain, type, proto))) {
        kfree(inode);
        return -ENOMEM;
    }
    /*
     * Create an open file
     */
    FS_DEBUG("Creating open file\n");
    if (0 == (file = fs_open(inode, O_RDONLY))) {
        kfree(inode);
        net_socket_close(socket);
        return -ENOMEM;
    }
    /*
     * and link it to the socket
     */
    file->socket = socket;
    /*
     * Now scan table of file descriptors to find
     * a free slot for the current process
     */
    FS_DEBUG("Allocating new file descriptor\n");
    fd = store_file(fs_process + pid, file, 0, 0);
    if (-1 == fd) {
        fs_close(file);
        return -EMFILE;
    }
    return fd;
}

/*
 * Accept a new incoming connection
 * Parameter:
 * @fd - file descriptor of socket
 * @addr - socket address, here the peers address is stored
 * @len - length of address
 */
int do_accept(int fd, struct sockaddr* addr, socklen_t* len) {
    inode_t* inode = 0;
    open_file_t* file = 0;
    open_file_t* of = 0;
    int new_fd;
    int rc;
    socket_t* socket;
    socket_t* new_socket;
    pid_t pid = pm_get_pid();
    FS_DEBUG("do_accept called\n");
    /*
     * Locate original socket
     */
    if ((fd < 0) || (fd >= FS_MAX_FD))
        return -EBADF;
    if (0 == (of = get_file(fs_process + pm_get_pid(), fd)))
        return -EBADF;
    if (0 == of->socket) {
        fs_close(of);
        return -EBADF;
    }
    socket = of->socket;
    /*
     * Create an inode
     */
    FS_DEBUG("Creating inode\n");
    if (0 == (inode = (inode_t*) kmalloc(sizeof(inode_t)))) {
        fs_close(of);
        return -ENOMEM;
    }
    /*
     * Set fields applicable for a socket
     */
    memset((void*) inode, 0, sizeof(inode_t));
    inode->dev = DEVICE_NONE;
    inode->mode = S_IFSOCK;
    inode->owner = do_geteuid();
    inode->group = do_getegid();
    inode->s_dev = DEVICE_NONE;
    /*
     * Create an open file pointing to the inode
     */
    FS_DEBUG("Creating open file\n");
    if (0 == (file = fs_open(inode, O_RDONLY))) {
        fs_close(of);
        kfree(inode);
        return -ENOMEM;
    }
    /*
     * Now scan table of file descriptors to find
     * a free slot for the current process
     */
    FS_DEBUG("Allocating new file descriptor\n");
    new_fd = store_file(fs_process + pid, file, 0, 0);
    if (-1 == new_fd) {
        fs_close(of);
        fs_close(file);
        return -EMFILE;
    }
    fs_close(of);
    /*
     * Get socket
     */
    rc = net_socket_accept(socket, addr, len, &new_socket);
    if (0 != rc) {
        fs_close(of);
        fs_close(file);
        return rc;
    }
    /*
     * and link it to the file
     */
    if (new_socket) {
        file->socket = new_socket;
        return new_fd;
    }
    return -1;
}


/*
 * Connect a socket. Currently only TCP / IP sockets are supported
 *
 * Parameter:
 * @fd - the file descriptor representing the socket
 * @sockaddr - the address to which we connect
 * @addrlen - length of address
 * Return values:
 * 0 upon success
 * -EBADF if the fd is not valid
 */
int do_connect(int fd, struct sockaddr* sockaddr, int addrlen) {
    open_file_t* of = 0;
    int rc;
    /*
     * Get reference to file
     */
    if ((fd < 0) || (fd >= FS_MAX_FD)) {
        return -EBADF;
    }
    if (0 == (of = get_file(fs_process + pm_get_pid(), fd))) {
        return -EBADF;
    }
    if (0 == of->socket) {
        fs_close(of);
        return -EBADF;
    }
    /*
     * Call net connect
     */
    rc = net_socket_connect(of->socket, sockaddr, addrlen);
    /*
     * Drop reference again
     */
    fs_close(of);
    return rc;
}

/*
 * Send data to a socket
 *
 * Parameter:
 * @fd - the file descriptor representing the socket
 * @buffer - pointer to data
 * @len - length of data
 * @flags - flags
 * Return values:
 * Number of bytes successfully sent
 * -EBADF if the fd is not valid
 * -EOVERFLOW is len is more than INT_MAX
 * additional error codes from network layer
 */
ssize_t do_send(int fd, void* buffer, size_t len, int flags) {
    open_file_t* of = 0;
    int rc;
    /*
     * Can write at most INT_MAX bytes
     */
    if (len > INT_MAX)
        return -EOVERFLOW;
    /*
     * Get reference to file
     */
    if ((fd < 0) || (fd >= FS_MAX_FD))
        return -EBADF;
    if (0 == (of = get_file(fs_process + pm_get_pid(), fd)))
        return -EBADF;
    if (0 == of->socket) {
        fs_close(of);
        return -EBADF;
    }
    /*
     * Call net send
     */
    rc = net_socket_send(of->socket, buffer, len, flags, 0, 0, 0);
    /*
     * Drop reference again
     */
    fs_close(of);
    return rc;
}

/*
 * Send data to a socket (sendto(
 *
 * Parameter:
 * @fd - the file descriptor representing the socket
 * @buffer - pointer to data
 * @len - length of data
 * @flags - flags
 * @addr - destination address
 * @addrlen - length of address field
 * Return values:
 * Number of bytes successfully sent
 * -EBADF if the fd is not valid
 * -EOVERFLOW is len is more than INT_MAX
 * additional error codes from network layer
 */
ssize_t do_sendto(int fd, void* buffer, size_t len, int flags, struct sockaddr* addr, int addrlen) {
    open_file_t* of = 0;
    int rc;
    /*
     * Can write at most INT_MAX bytes
     */
    if (len > INT_MAX)
        return -EOVERFLOW;
    /*
     * Get reference to file
     */
    if ((fd < 0) || (fd >= FS_MAX_FD))
        return -EBADF;
    if (0 == (of = get_file(fs_process + pm_get_pid(), fd)))
        return -EBADF;
    if (0 == of->socket) {
        fs_close(of);
        return -EBADF;
    }
    /*
     * Call net send
     */
    rc = net_socket_send(of->socket, buffer, len, flags, addr, addrlen, 1);
    /*
     * Drop reference again
     */
    fs_close(of);
    return rc;
}

/*
 * Read data from a socket
 *
 * Parameter:
 * @fd - the file descriptor representing the socket
 * @buffer - pointer to data
 * @len - length of data
 * @flags - flags
 * Return values:
 * Number of bytes successfully read
 * -EBADF if the fd is not valid
 * -EINTR if the read request was interrupted
 * -EOVERFLOW if we try to read more than INT_MAX bytes
 * additional error codes from network layer
 */
ssize_t do_recv(int fd, void* buffer, size_t len, int flags) {
    open_file_t* of = 0;
    int rc;
    /*
     * Can read at most INT_MAX bytes
     */
    if (len > INT_MAX)
        return -EOVERFLOW;
    /*
     * Get reference to file
     */
    if ((fd < 0) || (fd >= FS_MAX_FD))
        return -EBADF;
    if (0 == (of = get_file(fs_process + pm_get_pid(), fd)))
        return -EBADF;
    if (0 == of->socket) {
        fs_close(of);
        return -EBADF;
    }
    /*
     * Call net recv
     */
    rc = net_socket_recv(of->socket, buffer, len, flags, 0, 0, 0);
    /*
     * Drop reference again
     */
    fs_close(of);
    return rc;
}

/*
 * Read data from a socket (recvfrom)
 *
 * Parameter:
 * @fd - the file descriptor representing the socket
 * @buffer - pointer to data
 * @len - length of data
 * @flags - flags
 * @addr - the source address of the peer is stored here
 * @addrlen - length of address field
 * Return values:
 * Number of bytes successfully read
 * -EBADF if the fd is not valid
 * -EINTR if the read request was interrupted
 * -EOVERFLOW if we try to read more than INT_MAX bytes
 * additional error codes from network layer
 */
ssize_t do_recvfrom(int fd, void* buffer, size_t len, int flags, struct sockaddr* addr, u32* addrlen) {
    open_file_t* of = 0;
    int rc;
    /*
     * Can read at most INT_MAX bytes
     */
    if (len > INT_MAX)
        return -EOVERFLOW;
    /*
     * Get reference to file
     */
    if ((fd < 0) || (fd >= FS_MAX_FD))
        return -EBADF;
    if (0 == (of = get_file(fs_process + pm_get_pid(), fd)))
        return -EBADF;
    if (0 == of->socket) {
        fs_close(of);
        return -EBADF;
    }
    /*
     * Call net recv
     */
    rc = net_socket_recv(of->socket, buffer, len, flags, addr, addrlen, 1);
    /*
     * Drop reference again
     */
    fs_close(of);
    return rc;
}

/*
 * Bind socket to a local address
 *
 * Parameter:
 * @fd - the file descriptor representing the socket
 * @address - address to use
 * @address_len - length of address argument in bytes
 * Return values:
 * -EBADF if the fd is not valid
 * -ENOTSOCK is this is not a socket
 */
int do_bind(int fd, struct sockaddr* address, int addrlen) {
    open_file_t* of = 0;
    int rc;
    /*
     * Get reference to file
     */
    if ((fd < 0) || (fd >= FS_MAX_FD))
        return -EBADF;
    if (0 == (of = get_file(fs_process + pm_get_pid(), fd)))
        return -EBADF;
    if (0 == of->socket) {
        fs_close(of);
        return -ENOTSOCK;
    }
    /*
     * Call net_socket_bind
     */
    rc = net_socket_bind(of->socket, address, addrlen);
    /*
     * Drop reference again
     */
    fs_close(of);
    return rc;
}

/*
 * Listen on a socket
 *
 * Parameter:
 * @fd - the file descriptor representing the socket
 * @backlog - limit for queue of incoming connections
 * Return values:
 * -EBADF if the fd is not valid
 * -ENOTSOCK is this is not a socket
 */
int do_listen(int fd, int backlog) {
    open_file_t* of = 0;
    int rc;
    /*
     * Get reference to file
     */
    if ((fd < 0) || (fd >= FS_MAX_FD))
        return -EBADF;
    if (0 == (of = get_file(fs_process + pm_get_pid(), fd)))
        return -EBADF;
    if (0 == of->socket) {
        fs_close(of);
        return -ENOTSOCK;
    }
    /*
     * Call net_socket_listen
     */
    rc = net_socket_listen(of->socket, backlog);
    /*
     * Drop reference again
     */
    fs_close(of);
    return rc;
}

/*
 * Select system call
 * Parameter:
 * @nfds - number of file descriptors - only file descriptors with numbers smaller than this will be checked
 * @readfds - an instance of the fd_set bitmask in which the file descriptors to be checked for reading are set
 * @writefds - an instance of the fd_set bitmask in which the file descriptors to be checked for writing are set
 * @errorfds - an instance of the fd_set bitmask in which the file descriptors to be checked for errors are set
 * @timeout - timeout
 * Return value:
 * number of file descriptors for which an event occured upon success
 * -EINVAL if the number of file descriptors is not valid
 * -EBADF if one of the file descriptors is not valid
 * -ENOMEM if there is not enough memory for temporary data structures
 * -EINTR if the operation was interrupted by a signal
 */
int do_select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* errorfds, struct timeval* timeout) {
    int i;
    open_file_t** files;
    int invalid_fd = 0;
    semaphore_t* sem;
    int return_now = 0;
    unsigned int to_ticks;
    int read;
    int write;
    int rc;
    int pid = pm_get_pid();
    int reason;
    /*
     * First validate arguments
     */
    if ((nfds > FD_SETSIZE) || (nfds < 0))
        return -EINVAL;
    /*
     * Compute timeout in ticks
     */
    if (timeout) {
        to_ticks = timer_convert_timeval(timeout);
    }
    else {
        to_ticks = 0;
    }
    /*
     * Now determine open files for all file descriptors and validate the files. Note that currently, we
     * only support select for sockets, so we verify that all file descriptors refer to a socket
     */
    files = (open_file_t**) kmalloc(sizeof(open_file_t*) * nfds);
    if (0 == files)
        return -ENOMEM;
    for (i = 0; i < nfds; i++) {
        files[i] = 0;
        if ((readfds && FD_ISSET(i, readfds)) || (writefds && FD_ISSET(i, writefds))) {
            files[i] = get_file(fs_process + pid, i);
            if (0 == files[i])
                invalid_fd = 1;
            else if (0 == files[i]->socket)
                invalid_fd = 1;
        }
    }
    if (invalid_fd) {
        for (i = 0; i < nfds; i++)
            if (files[i])
                fs_close(files[i]);
        kfree((void*) files);
        return -EBADF;
    }
    /*
     * At this point, we know that all file descriptors are valid. We now create a semaphore and then, for
     * each file descriptor, pass the semaphore to the socket layer specific select function. This function will
     * return with a non-zero return value if the event we are waiting for has already occurred.
     */
    sem = (semaphore_t*) kmalloc(sizeof(semaphore_t));
    if (0 == sem) {
        for (i = 0; i < nfds; i++)
            if (files[i])
                fs_close(files[i]);
        kfree((void*) files);
        return -ENOMEM;
    }
    sem_init(sem, 0);
    for (i = 0; i < nfds; i++) {
        if (readfds && FD_ISSET(i, readfds))
            read = 1;
        else
            read = 0;
        if (writefds && FD_ISSET(i, writefds))
            write = 1;
        else
            write = 0;
        if ((read || write) && (files[i])) {
            rc = net_socket_select(files[i]->socket, read, write, sem);
            if (rc > 0) {
                /*
                 * Bit 0 in rc indicates whether we can read, bit 1 whether we can
                 * write. Clear flags in file descriptor sets to reflect the result
                 */
                if (readfds) {
                    if (0 == (rc & 0x1))
                        FD_CLR(i, readfds);
                    else
                        return_now++;
                }
                if (writefds) {
                    if (0 == (rc & 0x2))
                        FD_CLR(i, writefds);
                    else
                        return_now++;
                }
            }
        }
    }
    /*
     * If needed wait on semaphore
     */
    if (return_now) {
        for (i = 0; i < nfds; i++)
            if (files[i])
                fs_close(files[i]);
        kfree ((void*) files);
        return return_now;
    }
    if (0 == timeout)
        rc = sem_down_intr(sem);
    else
        rc = sem_down_timed(sem, to_ticks);
    /*
     * Cancel any pending select requests and see whether they have fired and
     * if yes, why. Update readfds and writefds accordingly
     */
    return_now = 0;
    for (i = 0; i < nfds; i++) {
        if (files[i]) {
            if (files[i]->socket) {
                if (readfds)
                    FD_CLR(i, readfds);
                if (writefds)
                    FD_CLR(i, writefds);
                reason = net_socket_cancel_select(files[i]->socket, sem);
                if ((reason & 0x1) && (readfds)) {
                    return_now++;
                    FD_SET(i, readfds);
                }
                if ((reason & 0x2) && (writefds)) {
                    return_now++;
                    FD_SET(i, writefds);
                }
            }
        }
    }
    /*
     * Clean up
     */
    for (i = 0; i < nfds; i++)
        if (files[i])
            fs_close(files[i]);
    kfree ((void*) files);
    kfree ((void*) sem);
    /*
     * If we were interrupted and none of the file descriptors
     * is ready, return -EINTR. Note that a restart is not supported for select
     * by ctOS - POSIX says that support for restart is implementation specific
     */
    if ((-1 == rc) && (0 == return_now))
        return -EINTR;
    return return_now;
}


/*
 * ioctl system call. Note that the terminal specific IOCTLs are handled in
 * systemcalls.c, all others are forwarded to this functions
 * Parameter:
 * @fd - the file descriptor
 * @cmd - the IOCTL number
 * @arg - the argument of the IOCTL
 */
int do_ioctl(int fd, unsigned int cmd, void* arg) {
    int rc = -ENOSYS;
    open_file_t* of;
    /*
     * Get reference to file
     */
    if ((fd < 0) || (fd >= FS_MAX_FD))
        return -EBADF;
    if (0 == (of = get_file(fs_process + pm_get_pid(), fd)))
        return -EBADF;
    /*
     * If this is a socket, assume that this is a socket specific ioctl
     */
    if (of->socket) {
        rc = net_ioctl(of->socket, cmd, arg);
    }
    fs_close(of);
    return rc;
}

/*
 * Set socket options
 * Parameter:
 * @fd - the sockets file descriptor
 * @level - level of option
 * @option - option name
 * @option_value - pointer to option value
 * @option_len - length of option in bytes
 * Return value:
 * 0 upon success
 */
int do_setsockopt(int fd, int level, int option, void* option_value, unsigned int option_len) {
    open_file_t* of = 0;
    int rc;
    /*
     * Get reference to file
     */
    if ((fd < 0) || (fd >= FS_MAX_FD))
        return -EBADF;
    if (0 == (of = get_file(fs_process + pm_get_pid(), fd)))
        return -EBADF;
    if (0 == of->socket) {
        fs_close(of);
        return -ENOTSOCK;
    }
    /*
     * Call net_socket_listen
     */
    rc = net_socket_setoption(of->socket, level, option, option_value, option_len);
    /*
     * Drop reference again
     */
    fs_close(of);
    return rc;
}

/*
 * Get foreign and local address of a socket
 * Parameter:
 * @fd - file descriptor for the socket
 * @laddr - local address will be stored here, can be NULL
 * @faddr - foreign address will be stored here, can be NULL
 * @addrlen - length of laddr and faddr, will be updated with actual address length
 * Return values:
 * -EBADF - file descriptor is not valid
 * -ENOTSOCK - file is not a socket
 * -EINVAL - addrlen is 0
 */
int do_getsockaddr(int fd, struct sockaddr* laddr, struct sockaddr* faddr, socklen_t* addrlen) {
    open_file_t* of = 0;
    int rc = 0;
    if (0 == addrlen)
        return -EINVAL;
    /*
     * Get reference to file
     */
    if ((fd < 0) || (fd >= FS_MAX_FD))
        return -EBADF;
    if (0 == (of = get_file(fs_process + pm_get_pid(), fd)))
        return -EBADF;
    if (0 == of->socket) {
        fs_close(of);
        return -ENOTSOCK;
    }
    /*
     * Call net_socket_getaddr
     */
    rc = net_socket_getaddr(of->socket, laddr, faddr, addrlen);
    /*
     * Drop reference again
     */
    fs_close(of);
    return rc;
}

/***************************************************************
 * Everything below this line is for debugging only            *
 **************************************************************/

/*
 * Print a list of all open files.
 * Return value:
 * 0 if output is empty
 * number of open files otherwise
 */
int fs_print_open_files() {
    open_file_t* of;
    int rc = 0;
    PRINT("\nDevice    Inode      Ref.count\n");
    PRINT("---------------------------------\n");
    LIST_FOREACH(open_files_head, of) {
        rc++;
        PRINT("(%h, %h)  %x  %d\n", MAJOR(of->inode->dev), MINOR(of->inode->dev), of->inode->inode_nr, of->ref_count);
    }
    return rc;
}


