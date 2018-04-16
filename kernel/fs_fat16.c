/*
 * fs_minix.c
 * File system specific code for the MINIX file system
 *
 * This is not yet implemented in this release of ctOS
 */

#include "fs.h"
#include "fs_fat16.h"

int fs_fat16_probe(dev_t device) {
    return 0;
}

superblock_t* fs_fat16_get_superblock(dev_t device) {
    return 0;
}

int fs_fat16_init() {
    return 0;
}
