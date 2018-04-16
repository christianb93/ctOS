/*
 * fs_pipe.c
 *
 * This file system implements pipes
 */

#include "fs.h"
#include "fs_pipe.h"
#include "kerrno.h"
#include "debug.h"
#include "mm.h"
#include "pm.h"
#include "util.h"
#include "lib/string.h"

/*
 * Create a pipe and return a pointer to it.
 * Return value:
 * a pointer to the newly created pipe if the operation was successful
 * 0 if no memory was available
 */
pipe_t* fs_pipe_create() {
    pipe_t* pipe;
    /*
     * Allocate memory for the pipe on the kernel heap
     */
    if (0==(pipe=(pipe_t*)kmalloc(sizeof(pipe_t)))) {
        return 0;
    }
    /*
     * Set reference counts
     */
    pipe->readers = 0;
    pipe->writers = 0;
    /*
     * Set up condition variables and lock
     */
    spinlock_init(&pipe->lock);
    cond_init(&pipe->written);
    cond_init(&pipe->read);
    /*
     * Initialize circular buffer
     */
    pipe->head = 0;
    pipe->tail = 0;
    return pipe;
}

/*
 * Connect an open file to a pipe
 * Parameters:
 * @pipe - the pipe to which we connect
 * @mode - PIPE_READ or PIPE_WRITE
 * Return value:
 * 0 if operation is successful
 * EINVAL if the mode or the pipe is invalid
 */
int fs_pipe_connect(pipe_t* pipe, int mode) {
    u32 eflags;
    if ((mode!=PIPE_READ) && (mode!=PIPE_WRITE)) {
        return EINVAL;
    }
    if (0==pipe)
        return EINVAL;
    spinlock_get(&pipe->lock, &eflags);
    if (mode==PIPE_READ) {
        pipe->readers++;
    }
    else {
        pipe->writers++;
    }
    spinlock_release(&pipe->lock, &eflags);
    return 0;
}

/*
 * Disconnect from a pipe.
 * Return value:
 * 0 if the operation completed and there are still other files connected to the pipe
 * 1 if the operation completed and there are no more files connected to the pipe
 * -EINVAL if one of the arguments is not valid
 */
int fs_pipe_disconnect(pipe_t* pipe, int mode) {
    int rc = 0;
    u32 eflags;
    if ((mode!=PIPE_READ) && (mode!=PIPE_WRITE)) {
        return EINVAL;
    }
    if (0==pipe)
        return EINVAL;
    spinlock_get(&pipe->lock, &eflags);
    if (mode==PIPE_READ) {
        if (pipe->readers)
            pipe->readers--;
        /*
         * If last reader has disconnect, inform writers
         */
        if (0==pipe->readers)
            cond_broadcast(&pipe->read);
    }
    else {
        if (pipe->writers)
            pipe->writers--;
        /*
         * If the last writer has disconnected, inform readers
         */
        if (0==pipe->writers)
            cond_broadcast(&pipe->written);
    }
    rc = (pipe->writers+pipe->readers > 0) ? 0 : -1;
    spinlock_release(&pipe->lock, &eflags);
    return rc;
}

/*
 * Write data to a pipe
 * Parameters:
 * @pipe - the pipe to which we write
 * @bytes - number of bytes to be written
 * @buffer - data which we write
 * @nowait - set this to 1 if the thread should not block, corresponds to O_NONBLOCK
 * Return value:
 * number of bytes actually written upon success
 * -EPIPE if there are no readers connected to the pipe
 * -EPAUSE if the write operation was interrupted by a signal and no data was written
 *
 */
int fs_pipe_write(pipe_t* pipe, size_t bytes, void* buffer, int nowait) {
    size_t bytes_left = bytes;
    u32 bytes_written = 0;
    u32 bytes_to_write = 0;
    u32 eflags;
    u32 elements_in_buffer = 0;
    u32 free_slots = 0;
    if ((0==pipe) || (0==buffer) || (0==bytes))
        return 0;
    while (bytes_left > 0) {
        /*
         * Enter monitor by getting spinlock
         */
        spinlock_get(&pipe->lock, &eflags);
        /*
         * If there are no readers, return -EPIPE or the number of bytes written. The calling function is
         * supposed to signal SIGPIPE to the thread in this case
         */
        if (0==pipe->readers) {
            spinlock_release(&pipe->lock, &eflags);
            if (0==bytes_written)
                return -EPIPE;
            else
                return bytes_written;
        }
        /*
         * Determine number of elements in buffer and free slot
         */
        elements_in_buffer = pipe->tail - pipe->head;
        free_slots = PIPE_BUF - elements_in_buffer;
        /*
         * If we can write to the buffer, either because we have at least n free slots or
         * we have any free slots and non-atomic writes are allowed, place data in buffer
         */
        if (((bytes>PIPE_BUF) && (free_slots > 0)) || (free_slots >=bytes)) {
            bytes_to_write = MIN(free_slots, bytes);
            memcpy(pipe->buffer + (pipe->tail % PIPE_BUF), buffer + bytes_written, bytes_to_write);
            bytes_left -= bytes_to_write;
            bytes_written += bytes_to_write;
            pipe->tail+=bytes_to_write;
            /*
             * Notify readers and leave monitor
             */
            cond_broadcast(&pipe->written);
            spinlock_release(&pipe->lock, &eflags);
        }
        else {
            if (nowait) {
                spinlock_release(&pipe->lock, &eflags);
                return (bytes_written ? bytes_written : -EAGAIN);
            }
            /*
             * Wait until data has been read. If we are interrupted by a signal, return -EPAUSE if
             * no data was written or bytes_written
             */
            if(-1==cond_wait_intr(&pipe->read, &pipe->lock, &eflags)) {
                return (0==bytes_written) ? -EPAUSE : bytes_written;
            }
            spinlock_release(&pipe->lock, &eflags);
        }
    }
    return bytes_written;
}


/*
 * Read data from a pipe
 * Parameters:
 * @pipe - the pipe from which we read
 * @bytes - number of bytes to be read
 * @buffer - output buffer
 * @nowait - set this to 1 to return immediately without waiting (corresponds to O_NONBLOCK)
 * Return value:
 * number of bytes actually read upon success
 * -EPAUSE if the read operation was interrupted by a signal and no data was read
 *
 */
int fs_pipe_read(pipe_t* pipe, size_t bytes, void* buffer, int nowait) {
    u32 bytes_read = 0;
    u32 bytes_to_read = 0;
    u32 eflags;
    u32 elements_in_buffer = 0;
    if ((0==pipe) || (0==buffer) || (0==bytes))
        return 0;
    while (0==bytes_read) {
        /*
         * Enter monitor by getting spinlock
         */
        spinlock_get(&pipe->lock, &eflags);
        /*
         * Determine number of elements in buffer
         */
        elements_in_buffer = pipe->tail - pipe->head;
        /*
         * If there is data in the buffer, read it
         */
        if (elements_in_buffer) {
            bytes_to_read = MIN(elements_in_buffer, bytes);
            memcpy(buffer + bytes_read, pipe->buffer + (pipe->head % PIPE_BUF), bytes_to_read);
            pipe->head += bytes_to_read;
            bytes_read += bytes_to_read;
            /*
             * Notify writers and leave monitor
             */
            cond_broadcast(&pipe->read);
            spinlock_release(&pipe->lock, &eflags);
        }
        /*
         * If there are no writers, leave monitor
         */
        else if (0==pipe->writers) {
            spinlock_release(&pipe->lock, &eflags);
            return 0;
        }
        else {
            /*
             * Return if nowait has been specified
             */
            if (1==nowait) {
                spinlock_release(&pipe->lock, &eflags);
                return -EAGAIN;
            }
            /*
             * Wait until data has been written. If we are interrupted by a signal, return -EPAUSE if
             * no data was read or bytes_read
             */
            if(-1==cond_wait_intr(&pipe->written, &pipe->lock, &eflags)) {
                return (0==bytes_read) ? -EPAUSE : bytes_read;
            }
            /*
             * If we return from wait, we hold the lock, i.e. we are inside the monitor.
             */
            spinlock_release(&pipe->lock, &eflags);
        }
    }
    return bytes_read;
}
