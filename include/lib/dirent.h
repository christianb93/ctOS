/*
 * dirent.h
 */

#ifndef _DIRENT_H_
#define _DIRENT_H_

#include "sys/types.h"
#include "os/dirstreams.h"

struct dirent {
    ino_t d_ino;
    char d_name[];
};

typedef __ctOS_dirstream_t DIR;

DIR* opendir(const char* dirname);
struct dirent* readdir(DIR* dirp);
int closedir(DIR* dirp);

#endif /* _DIRENT_H_ */
