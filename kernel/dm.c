/*
 * dm.c
 * The device driver manager
 *
 * The device driver manager is a layer above the device drivers for
 * block and character devices. It offers a standardized interface to
 * other layers of the kernel which allows access to a device driver if
 * the device ID (major and minor device number) is known
 *
 * At startup, devices need to register their interfaces with the device
 * driver manager to be accessible
 */

#include "dm.h"
#include "kerrno.h"
#include "debug.h"
#include "tty.h"
#include "pci.h"
#include "ramdisk.h"
#include "pata.h"
#include "ahci.h"
#include "rtc.h"
#include "8139.h"

/*
 * This is table of initialization routines
 * called at boot-time
 */
typedef void (*driver_init_t)();
static driver_init_t built_in_drivers[] = {&pci_init, &tty_init, &ramdisk_init, &pata_init, &ahci_init, &nic_8139_init};

/*
 * This is a table of pointers to driver structures
 */
static driver_t drivers[(sizeof(major_dev_t) << 8)];

/*
 * Initialize the driver manager. This function will built up the
 * internal data structures of the device driver manager and will
 * call the initialization functions stored in the table built_in_drivers
 */
void dm_init() {
    int i;
    for (i=0;i<(sizeof(major_dev_t) << 8);i++)
        drivers[i].type=DRIVER_TYPE_NONE;
    for (i=0;i<(sizeof(built_in_drivers)/sizeof(driver_init_t)); i++)
        built_in_drivers[i]();
}

/*
 * Given a major node, return the block
 * device operations structure registered for
 * this major number or NULL if the major node is not used
 * Parameters:
 * @major - the major device number of the device
 * Return value:
 * the block device operations structure of the device or
 * 0 if no device is registered for this major number
 */
blk_dev_ops_t* dm_get_blk_dev_ops(major_dev_t major) {
    if (drivers[major].type!=DRIVER_TYPE_BLK)
        return 0;
    return drivers[major].blk_dev_ops;
}

/*
 * Given a major node, return the character device
 * operations structure for this node or NULL if no
 * device is registered for this node
 * Parameters:
 * @major - the major device number of the device
 * Return value:
 * the character device operations structure of the device or
 * 0 if no device is registered for this major number
 */
char_dev_ops_t* dm_get_char_dev_ops(major_dev_t major) {
    if (drivers[major].type!=DRIVER_TYPE_CHAR)
        return 0;
    return drivers[major].char_dev_ops;
}

/*
 * Register a block device with the driver manager
 * Parameter:
 * @major - the major node for the driver
 * @ops - a pointer to the interface of the driver
 * Return value:
 * EALREADY if the major device number is in use
 * 0 upon success
 */
int dm_register_blk_dev(major_dev_t major, blk_dev_ops_t* ops) {
    if (drivers[major].type != DRIVER_TYPE_NONE) {
        ERROR("Major number already in use\n");
        return EALREADY;
    }
    drivers[major].blk_dev_ops = ops;
    drivers[major].char_dev_ops = 0;
    drivers[major].major = major;
    drivers[major].type = DRIVER_TYPE_BLK;
    return 0;
}


/*
 * Register a character device with the driver manager
 * Parameter:
 * @major - the major node for the driver
 * @ops - a pointer to the interface of the driver
 * Return value:
 * EALREADY if the major device number is in use
 * 0 upon success
 */
int dm_register_char_dev(major_dev_t major, char_dev_ops_t* ops) {
    if (drivers[major].type != DRIVER_TYPE_NONE) {
        ERROR("Major number already in use\n");
        return EALREADY;
    }
    drivers[major].char_dev_ops = ops;
    drivers[major].blk_dev_ops = 0;
    drivers[major].major = major;
    drivers[major].type = DRIVER_TYPE_CHAR;
    return 0;
}

