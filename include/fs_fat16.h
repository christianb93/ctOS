/*
 * fs_minix.h
 */

#ifndef _FS_FAT16_H_
#define _FS_FAT16_H_

#include "lib/sys/types.h"
#include "fs.h"

int fs_fat16_probe(dev_t device);
superblock_t* fs_fat16_get_superblock(dev_t device);
int fs_fat16_init();


#endif /* _FS_FAT16_H_ */
