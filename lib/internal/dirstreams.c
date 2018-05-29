/*
 * dirstreams.c
 *
 */

#include "lib/os/dirstreams.h"
#include "lib/os/errors.h"
#include "lib/stdlib.h"
#include "lib/errno.h"
#include "lib/os/oscalls.h"


/*
 * Open a directory stream
 * Parameters:
 * @stream - the stream to be initialized
 * @fd - the file descriptor to use, needs to point to an open directory
 * Return value:
 * 0 upon success
 * ENOMEM if no memory could be allocated for the buffer
 */
int __ctOS_dirstream_open(__ctOS_dirstream_t* stream, int fd) {
    stream->fd = fd;
    stream->buf_index = 0;
    stream->buf_end = -1;
    /*
     * We reserve space for 256 directory entries initially
     */
    stream->buf_size = __DIRSTREAM_BUFSIZE;
    stream->buffer = (__ctOS_direntry_t*) malloc(__DIRSTREAM_BUFSIZE*sizeof(__ctOS_direntry_t));
    stream->filpos = 0;
    if (0==stream->buffer)
        return ENOMEM;
    return 0;
}

/*
 * Read a directory entry from a stream. This function will get an entry from the buffer. If the end
 * of the buffer has already been reached, it will clear the buffer and read at most
 * stream->buf_size directory entries into the buffer, starting at the current position within the
 * directory
 * Parameter:
 * @stream - the stream to be read from
 * Return value
 * 0 if the read operation failed or if the end of the file has been reached
 * a pointer to the read (and buffered) entry if the operation was successful
 */
__ctOS_direntry_t* __ctOS_dirstream_readdir(__ctOS_dirstream_t* stream) {
    int rc = 0;
    /*
     * If the index of the next entry to be read is outside of the buffer,
     * this means that we need to read a new chunk of data into the buffer first
     */
    if (stream->buf_index > stream->buf_end) {
        /*
         * Read the next chunk of data from the file. If the read fails, return NULL
         * and update errno.
         */
        stream->buf_index = 0;
        stream->buf_end = -1;
        while ((0==rc) && (stream->buf_end < stream->buf_size-1)) {
            rc = __ctOS_getdent(stream->fd, (__ctOS_direntry_t*) stream->buffer+stream->buf_end+1);
            if (rc < -1) {
                errno = - rc;
                return 0;
            }
            if (0==rc) {
                stream->buf_end++;
                stream->filpos++;
            }
        }
    }
    /*
     * If we have not been able to fill up the buffer, return 0
     */
    if (stream->buf_index > stream->buf_end)
        return 0;
    stream->buf_index++;
    return &stream->buffer[stream->buf_index-1];
}


/*
 * Close a directory stream
 * Parameter:
 * @stream - stream to close
 */
void __ctOS_dirstream_close(__ctOS_dirstream_t* stream) {
    if (stream->buffer)
        free((void*) stream->buffer);
}

/*
 * Rewind a directory stream. We assume that the stream has been opened 
 * and refers to a valid file descriptor
 * Parameter:
 * @stream - stream to use
 * Returns:
 * 0 upon success
 */
int __ctOS_dirstream_rewind(__ctOS_dirstream_t* stream) {
    /*
     * We give up if there is no buffer allocated yet, in this case
     * the stream has most likely not been opened. Same thing
     * if there is no valid file descriptor
     */
    if (0 == stream->buffer) {
        return ENOBUFS;
    }
    if (-1 == stream->fd) {
        return EINVAL;
    }
    /*
     * Reset the buffer control variables
     */
    stream->buf_index = 0;
    stream->buf_end = -1;
    stream->filpos = 0;    
    /*
     * and rewind the actual file
     */
    __ctOS_lseek(stream->fd, 0, SEEK_SET);
    return 0;
}