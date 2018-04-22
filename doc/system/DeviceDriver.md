# Device driver

## Block devices and character devices

In ctOS, most device drivers implement a standard interface which allows the device driver manager to easily access different device drivers through a uniform set of standard calls. This interface is very much along the lines of block devices and character devices found on most UNIX systems.

A **character device** is a device from which data is read and to which data is written as a stream of bytes. Character devices have a state: each read operation advances a pointer within the byte stream, and the next read or write operation will refer to this location within the byte stream. The current position within the byte stream can be altered by a seek operation, which, however, is not supported by every device. Typical examples for character devices are serial lines, keyboards and consoles.

The second class of devices supported by ctOS is the class of **block devices**. Block devices are read from and written to in units of blocks. Each block on the device is identified by a unique address. Whenever a read or write is performed, the block to which the operation refers has to be passed as parameters, there is no pointer into a current position as it is the case for a character device. Typical examples for block devices are hard disks, where the smallest unit of data which can be accessed is a sector (typically 512 bytes).

## Major and minor nodes

Traditionally, a Unix system identifies devices by a combination of two 8 bit values called **major and minor node**. The major node determines the driver which is used to communicate with the device. The minor node is passed to the device driver when an operation on the device is requested and identifies the actual physical device to be used.

Suppose for instance that the major device number 4 is reserved for TTY devices. A computer might have more than one terminal attached to it, or might have virtual terminals or several serial lines. To distinguish between these devices, the minor number is used. For instance, my Linux system uses major number 4 and minor number 0 for /dev/tty0, major number 4 and minor number 1 for /dev/tty1 and so forth.

In the case of hard disks, we use the minor number to differentiate not only between all hard disks attached to any of the controllers, but also to differentiate between different partitions. Again, on my Linux system, the major number used for all AHCI devices is 8. Minor number one is the device /dev/sda, i.e. the first disk attached to the first controller. Minor number 1 is the first partition on that disk and so forth.

```
ls -l /dev/tty*
crw-rw-rw- 1 root tty     5,  0 Apr 22 17:49 /dev/tty
crw--w---- 1 root tty     4,  0 Apr 22 17:49 /dev/tty0
crw--w---- 1 root tty     4,  1 Apr 22 17:50 /dev/tty1
crw--w---- 1 root tty     4, 10 Apr 22 17:49 /dev/tty10
crw--w---- 1 root tty     4, 11 Apr 22 17:49 /dev/tty11
crw--w---- 1 root tty     4, 12 Apr 22 17:49 /dev/tty12
crw--w---- 1 root tty     4, 13 Apr 22 17:49 /dev/tty13
crw--w---- 1 root tty     4, 14 Apr 22 17:49 /dev/tty14
```

On Linux systems, a textual description of the major device numbers can be found in `/proc/devices`.

In the case of hard disks, we use the minor number to differentiate not only between all hard disks attached to any of the controllers, but also to differentiate between different partitions. Again, on my Linux system, the major number used for all AHCI devices is 8. Minor number one is the device /dev/sda, i.e. the first disk attached to the first controller. Minor number 1 is the first partition on that disk and so forth.


## The interface of a device driver

The following functions are offered by a character device.

| Function |	Arguments |	Description 
|:---|:---|:---|
init|	none|	This is the initialization routine which is called once by the driver manager at boot time. The initialization routine is responsible for scanning the attached devices and setting up the devices as far as needed to be able to serve future requests. It will also call a function within the driver manager to register all other functions of the public interface with the driver manager.
| open |	minor number |	This call is supposed to prepare the device for a subsequent read or write operation. It is up to the driver which operations are performed in the initialization function and which operations are done in open. Typically, a driver will perform device-specific initialization in the open call as this call already refers to an individual minor number
| close |	minor number |	Close a specific device
|read |	minor number |number of bytes to read, pointer to a buffer |	This function reads a specified number of bytes from the device and stores it in the buffer passed as parameter. It will then advance the position within the device depending on the number of bytes actually read. It returns the number of bytes read or a negative error code if an error occurs
|write | 	minor number, number of bytes to write, pointer to data | This function will write the specified number of bytes to the device and advance the position within the device. It will return the number of bytes actually written or a negative error code if an error occurs
| seek  | 	minor number, position| 	Move the position within the device to the specified location


Note that not all devices will implement all of these functions. A serial line driver, for instance, will usually not implement a seek command. Some devices might be read only or write only and only implement either read or write. However, each driver needs to technically implement all of these functions, i.e. at least provide a stub, to avoid null pointers.

The interface of a block driver is very similar, except that there is no seek operation and data size is specified in units of blocks. Each block on the device has a unique block number, ranging from zero to the number of blocks on the device minus 1. This block number is used to address a specific block on the device.


| Function |	Arguments |	Description 
|:---|:---|:---|
init|	none|	This is the initialization routine which is called once by the driver manager at boot time. The initialization routine is responsible for scanning the attached devices and setting up the devices as far as needed to be able to serve future requests. It will also call a function within the driver manager to register all other functions of the public interface with the driver manager.
| open |	minor number |	This call is supposed to prepare the device for a subsequent read or write operation. It is up to the driver which operations are performed in the initialization function and which operations are done in open. Typically, a driver will perform device-specific initialization in the open call as this call already refers to an individual minor number
| close |	minor number |	Close a specific device
|read |	minor number |minor number, number of blocks to read, address of first block to read, pointer to a buffer |	This function reads a specified number of blocks from the device and stores it in the buffer passed as parameter. The additional parameter "first block" specifies where on the device the data is read. The function should return the number of bytes read upon success or a negative error code if the operation failed
|write | minor number, number of blocks to write, address of first block to write, pointer to data | This function will write the specified number of blocks to the device.

This leads to the following definitions in the header file drivers.h. Note that the initialization function of a driver is not a part of these structures but a separate function which needs to be present in a static table in the device driver manager (see below).


```
typedef struct {
    int (*open)(u8 minor);
    int (*close)(u8 minor);
    ssize_t (*read)(u8 minor, ssize_t size, void* buffer);
    ssize_t (*write)(u8 minor, ssize_t size, void* buffer);
    ssize_t (*seek)(u8 minor, ssize_t pos);
} char_dev_ops_t;

typedef struct {
    int (*open)(u8 minor);
    int (*close)(u8 minor);
    ssize_t (*read)(u8 minor, ssize_t blocks, ssize_t first_block, void* buffer);
    ssize_t (*write)(u8 minor, ssize_t blocks, ssize_t first_block, void* buffer);
} blk_dev_ops_t;
```

## The driver manager


|Function | Description
|:---|:---|
| dm_register_blk_dev| 	Register a block device under a specified major number with the driver manager
| dm_register_char_dev |Register a character device under a specified major number with the driver manager
| dm_get_blk_dev_ops|	Given a major number, return a pointer to the blk_dev_ops_t structure for this device or NULL if no device is registered for this major number
|dm_get_char_dev_ops|	Given a major number, return a pointer to the char_dev_ops_t structure for this device or NULL if no device is registered for this major number
|dm_init|Initialize the driver manager. Among other tasks, this will force the device manager to initialize all device drivers it knows of

To be able to initialize all device drivers at startup time, the driver manager contains a hardcoded table of initialization routines. This table is walked at start up and all initialization routines are invoked. At this point, the pointer to the initialization routine is the only information which the driver manager has on a specific driver. When loadable module support is added in later versions of ctOS, the driver manager needs to be invoked each time a new device driver is loaded and will then call the initialization routine of the new driver.


 
