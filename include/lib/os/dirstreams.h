/*
 * dirstreams.h
 *
 *  Created on: Jan 20, 2012
 *      Author: chr
 */

#ifndef _DIRSTREAMS_H_
#define _DIRSTREAMS_H_

#include "types.h"

/*
 * Needs to match declaration of direntry_t and
 * FILE_NAME_MAX in fs.h
 */
typedef struct {
    ino_t inode_nr;
    char name[256];
} __ctOS_direntry_t;


/*
 * A directory stream
 */
typedef struct  {
    __ctOS_direntry_t* buffer;                   // buffer used by this stream
    int buf_size;                                // size of the buffer, i.e. number of directory entries which fit into the buffer
    int fd;                                      // open directory associated with this buffer
    int buf_index;                               // position within the buffer, i.e. the index of the next entry received by a read
    unsigned int filpos;                         // position within the file which corresponds to the first entry in the buffer
    int buf_end;                                 // index of the last entry within the buffer which is filled
} __ctOS_dirstream_t;

#endif /* _DIRSTREAMS_H_ */

/*
 * The number of directory entries which we keep in a buffer
 */
#define __DIRSTREAM_BUFSIZE 256

int __ctOS_dirstream_open(__ctOS_dirstream_t* stream, int fd);
__ctOS_direntry_t* __ctOS_dirstream_readdir(__ctOS_dirstream_t* stream);
void __ctOS_dirstream_close(__ctOS_dirstream_t* stream);
