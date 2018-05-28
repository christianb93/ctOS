/*
 * stat.h
 *
 *  Created on: Jan 23, 2012
 *      Author: chr
 */

#ifndef _OS_STAT_H_
#define _OS_STAT_H_

#include "types.h"

/*
 * This needs to stay in sync with 
 * sys/stat.h
 */

typedef struct __ctOS_stat {
    dev_t st_dev;
    ino_t st_ino;
    mode_t st_mode;
    nlink_t st_nlink;
    uid_t st_uid;
    gid_t st_gid;
    off_t st_size;
    dev_t  st_rdev;        
    time_t st_atime;
    time_t st_mtime;
    time_t st_ctime;
} ctOS_stat_t;

#endif /* _OS_STAT_H_ */
