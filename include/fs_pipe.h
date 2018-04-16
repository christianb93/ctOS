/*
 * fs_pipe.h
 *
 */

#ifndef _FS_PIPE_H_
#define _FS_PIPE_H_

#include "lib/sys/types.h"
#include "lib/limits.h"
#include "locks.h"


typedef struct {
    u32 readers;                  // how many readers are connected to the pipe
    u32 writers;                  // how many writers are connected to the pipe
    cond_t written;               // used to signal readers that we have written to the pipe
    cond_t read;                  // used to signal writers that we have read from the pipe
    spinlock_t lock;              // protect pipe
    u32 head;                     // head of buffer
    u32 tail;                     // tail of buffer
    u8 buffer[PIPE_BUF];          // buffer
} pipe_t;

/*
 * Access modes for pipes
 */
#define PIPE_READ 0
#define PIPE_WRITE 1


pipe_t* fs_pipe_create();
int fs_pipe_connect(pipe_t* pipe, int mode);
int fs_pipe_disconnect(pipe_t* pipe, int mode);
int fs_pipe_write(pipe_t* pipe, size_t bytes, void* buffer, int nowait);
int fs_pipe_read(pipe_t* pipe, size_t bytes, void* buffer, int nowait);


#endif /* _FS_PIPE_H_ */
