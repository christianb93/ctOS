/*
 * drivers.h
* This header file declares structures which are common to all device drivers
* and the driver manager
 */

#ifndef _DRIVERS_H_
#define _DRIVERS_H_

#include "ktypes.h"
#include "lib/unistd.h"
#include "kerrno.h"

typedef u8 major_dev_t;
typedef u8 minor_dev_t;

/*
 * Structure describing the interface for a character device driver
 */
typedef struct {
    int (*open)(minor_dev_t minor);
    int (*close)(minor_dev_t minor);
    ssize_t (*read)(minor_dev_t minor, ssize_t size, void* buffer, u32 flags);
    ssize_t (*write)(minor_dev_t minor, ssize_t size, void* buffer);
    ssize_t (*seek)(minor_dev_t minor, ssize_t pos);
} char_dev_ops_t;

/*
 * This is the interface of a block device driver
 */
typedef struct {
    int (*open)(minor_dev_t minor);
    int (*close)(minor_dev_t minor);
    ssize_t (*read)(minor_dev_t minor, ssize_t blocks, ssize_t first_block, void* buffer);
    ssize_t (*write)(minor_dev_t minor, ssize_t blocks, ssize_t first_block, void* buffer);
} blk_dev_ops_t;

/*
 * Device number for "no device"
 */
#define DEVICE_NONE (0xffff)

/*
 * Major numbers
 */
#define MAJOR_RAMDISK 1
#define MAJOR_TTY 2
#define MAJOR_ATA 3
#define MAJOR_AHCI 4

/*
 * Default block size
 */
#define BLOCK_SIZE 1024
/*
 * Macros to work with major and minor device
 */
#define MAJOR(x) (((major_dev_t) (x >> 8) ))
#define MINOR(x) (((minor_dev_t) x))
#define DEVICE(major,minor) (((dev_t) (major<<8)+minor))




#endif /* _DRIVERS_H_ */
