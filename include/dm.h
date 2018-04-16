/*
 * dm.h
 * Header file for the driver manager
 */

#ifndef _DM_H_
#define _DM_H_

#include "drivers.h"

typedef struct _driver_t {
    major_dev_t major;                     // major number of device
    int type;                               // type (DRIVER_TYPE_CHAR or DRIVER_TYPE_BLK)
    blk_dev_ops_t* blk_dev_ops;             // pointer to block device driver operations
    char_dev_ops_t* char_dev_ops;           // pointer to character device driver operations
} driver_t;

/*
 * Driver type
 */
#define DRIVER_TYPE_NONE 0
#define DRIVER_TYPE_CHAR 1
#define DRIVER_TYPE_BLK 2


int dm_register_blk_dev(major_dev_t major, blk_dev_ops_t* ops);
int dm_register_char_dev(major_dev_t major, char_dev_ops_t* ops);
blk_dev_ops_t* dm_get_blk_dev_ops(major_dev_t major);
char_dev_ops_t* dm_get_char_dev_ops(major_dev_t major);
void dm_init();

#endif /* _DM_H_ */
