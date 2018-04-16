/*
 * streams.c
 *
 * This module contains the implementation of streams for the C library
 */

#include "lib/os/streams.h"
#include "lib/stdio.h"
#include "lib/stdlib.h"
#include "lib/os/errors.h"
#include "lib/os/oscalls.h"
#include "lists.h"

/*
 * This list contains the currently open streams and is used to close or flush all streams
 */
static __ctOS_stream_t* stream_list_head = 0;
static __ctOS_stream_t* stream_list_tail = 0;


/*
 * Open a stream and prepare it for usage.
 * Parameter:
 * @stream - the stream to be initialized
 * @fd - the file descriptor to be used by the stream, -1 for none
 * Return value:
 * 0 if the operation was successful
 * ENOMEM if the allocation of the buffer failed
 */
int __ctOS_stream_open(__ctOS_stream_t* stream, int fd) {
    stream->fd = fd;
    stream->buf_index = 0;
    stream->buf_end = -1;
    stream->buf_size = BUFSIZ;
    stream->buffer = (unsigned char*) malloc(BUFSIZ);
    stream->buf_mode = _IOFBF;
    stream->ungetc_flag = 0;
    stream->dirty = 0;
    stream->eof = 0;
    stream->error = 0;
    stream->filpos = 0;
    if (0==stream->buffer)
        return ENOMEM;
    stream->private_buffer = 1;
    LIST_ADD_END(stream_list_head, stream_list_tail, stream);
    return 0;
}

/*
 * Read a byte directly from the file associated with a stream
 * Parameter:
 * @stream - the stream to which we write
 * Return value:
 * the character itself upon success
 * EOF otherwise
 */
static int __ctOS_stream_read_direct(__ctOS_stream_t* stream) {
    int rc;
    /*
     * Make sure that c is zero as the read operation will
     * only update the LSB of it...
     */
    int c = 0;
    if (-1==stream->fd) {
        stream->error = EBADF;
        return EOF;
    }
    /*
     * Read data from file
     */
    rc = __ctOS_read(stream->fd, (char*) &c, 1);
    if (0==rc) {
        stream->eof = 1;
        return EOF;
    }
    if (rc!=1) {
        stream->error = EIO;
        return EOF;
    }
    stream->filpos++;
    return c;
}

/*
 * Read a byte from a stream. This function will get a byte from the buffer. If the end
 * of the buffer has already been reached, it will clear the buffer and read at most
 * stream->buf_size bytes into the buffer, starting at the current position within the
 * file
 * Parameter:
 * @stream - the stream to be read from
 * Return value
 * EOF if the read operation failed or if the end of the file has been reached
 * the read character, converted into an integer
 */
int __ctOS_stream_getc(__ctOS_stream_t* stream) {
    int rc;
    stream->dirty = 0;
    /*
     * If we have pushed back a character previously, use it
     */
    if (1==stream->ungetc_flag)  {
        stream->ungetc_flag = 0;
        return stream->ungetc_buffer;
    }
    /*
     * Do direct read if the stream is not buffered
     */
    if (_IONBF==stream->buf_mode)
        return __ctOS_stream_read_direct(stream);
    /*
     * If the index of the next character to be read is outside of the buffer,
     * this means that we need to read a new chunk of data into the buffer first
     */
    if (stream->buf_index > stream->buf_end) {
        /*
         * Read the next block from the file. If the read fails, return EOF
         * and update errno. If we have reached the end of the file, return
         * EOF
         */
        if (-1!=stream->fd) {
            rc = __ctOS_read(stream->fd, (char*) stream->buffer, stream->buf_size);
            if (rc <= 0) {
                stream->error = rc;
                if (0==rc)
                    stream->eof = 1;
                return EOF;
            }
            stream->filpos = stream->buf_end + 1;
            stream->buf_end = rc - 1;
            stream->buf_index = 0;
        }
    }
    rc = (int) stream->buffer[stream->buf_index];
    if ((0==rc) && (-1==stream->fd)) {
        stream->eof = 1;
        return EOF;
    }
    stream->buf_index++;
    return rc;
}

/*
 * Flush a stream. This function will write the entire buffer to the associated file
 * and reset the positioning attributes of the stream
 * Parameters:
 * @stream - the stream to be flushed
 * Return value
 * 0 if the operation is successful
 * EBADF if the stream does not have an associated stream
 * EIO if the write operation failed
 */
int __ctOS_stream_flush(__ctOS_stream_t* stream) {
    int rc;
    int new_filpos;
    /*
     * Do not flush an unbuffered stream
     */
    if (_IONBF==stream->buf_mode)
        return 0;
    /*
     * Only flush if the last operation was a write
     */
    if (stream->dirty) {
        if (-1==stream->fd) {
            return EBADF;
        }
        if (-1==stream->buf_end)
            return 0;
        rc = __ctOS_write(stream->fd, (char*) stream->buffer, stream->buf_end + 1);
        if (rc != stream->buf_end + 1 ) {
            return EIO;
        }
        new_filpos = __ctOS_lseek(stream->fd, 0, SEEK_CUR);
        if (new_filpos <0) {
            new_filpos = -1;
        }
        stream->filpos = new_filpos;
        stream->buf_end = -1;
        stream->buf_index = 0;
        stream->dirty = 0;
    }
    return 0;
}

/*
 * Flush all open streams
 * Return value:
 * 0 if all streams could be successfully flushed
 * the error code returned by the first failed flush operation if the operation fails on a stream
 */
int __ctOS_stream_flush_all() {
    __ctOS_stream_t* stream;
    int rc;
    LIST_FOREACH(stream_list_head, stream) {
        rc = __ctOS_stream_flush(stream);
        if (rc)
            return rc;
    }
    return 0;
}

/*
 * Write a byte directly into the file associated with a stream
 * Parameter:
 * @stream - the stream to which we write
 * @c - the character to be written
 * Return value:
 * the character itself upon success
 * EOF otherwise
 */
static int __ctOS_stream_write_direct(__ctOS_stream_t* stream, int c) {
    int rc;
    if (-1==stream->fd) {
        stream->error = EBADF;
        return EOF;
    }
    rc = __ctOS_write(stream->fd, (char*) &c, 1);
    if (rc!=1) {
        stream->error = EIO;
        return EOF;
    }
    stream->filpos++;
    return (int)((unsigned char)c);
}

/*
 * Write a byte into a stream. This function will add a byte to the buffer. If the
 * buffer is full, i.e. if buf_end + 1 >= buf_size, the buffer will be flushed and the
 * byte will be written to offset 0 into the buffer.
 * If no file descriptor is associated with the stream but the buffer is full and needs to
 * be flushed, errno will be set to EBADF and EOF is returned.
 * Parameter:
 * @c - the character to be written
 * @stream - the stream to which we write
 * Return value:
 * the written character upon successful completion
 * EOF otherwise
 */
int __ctOS_stream_putc(__ctOS_stream_t* stream, int c) {
    int rc;
    stream->dirty = 1;
    /*
     * If the stream is not buffered, write the character to the file
     * and return
     */
    if (_IONBF == stream->buf_mode) {
        return __ctOS_stream_write_direct(stream, c);
    }
    /*
     * If the buffer is full, flush the stream
     */
    if (stream->buf_end +1 >= stream->buf_size) {
        if ((rc = __ctOS_stream_flush(stream))) {
            stream->error = rc;
            return EOF;
        }
    }
    /*
     * Now place new character in stream
     */
    stream->buf_end++;
    stream->buffer[stream->buf_end]=(unsigned char) c;
    /*
     * If the last character is a newline and the stream
     * is line buffered, flush it. Mark it as dirty first
     * to make sure that flush is done
     */
    stream->dirty = 1;
    if ((_IOLBF==stream->buf_mode) && (((unsigned char) c)=='\n')) {
        if ((rc = __ctOS_stream_flush(stream))) {
            stream->error = rc;
            return EOF;
        }
    }
    return (int)((unsigned char)c);
}

/*
 * Put a character back into the stream
 * Parameter:
 * @stream - the stream to which the unget operation is to be applied
 * @c - the character
 */
int __ctOS_stream_ungetc(__ctOS_stream_t* stream, int c) {
    /*
     * We allow only one byte to be pushed back
     */
    if (stream->ungetc_flag)
        return EOF;
    /*
     * Required by the POSIX specification
     */
    if (EOF==c)
        return EOF;
    stream->ungetc_flag = 1;
    stream->ungetc_buffer = (unsigned char) c;
    stream->eof = 0;
    return (int)((unsigned char)c);
}

/*
 * Set buffer and buffer mode for a stream
 * Parameter:
 * @stream - the stream
 * @buffer - the buffer to use (NULL if no buffer is provided)
 * @type - type (_IONBF, IOLBF, _IOFBF)
 * @size - the size of the buffer
 * Return value:
 * 0 upon successful completion
 * EINVAL - type is not valid
 */
int __ctOS_stream_setvbuf(__ctOS_stream_t* stream, char* buffer, int type, int size) {
    if (!((_IONBF==type) || (_IOLBF==type) || (_IOFBF==type)))
        return EINVAL;
    stream->buf_mode = type;
    if (buffer) {
        if ((stream->buffer) && (stream->private_buffer))
            free(stream->buffer);
        stream->buffer = (unsigned char*) buffer;
        stream->private_buffer = 0;
        stream->buf_size = size;
    }
    stream->buf_index = 0;
    stream->buf_end = -1;
    stream->filpos = 0;
    stream->dirty = 0;
    return 0;
}


/*
 * Close a stream
 * Parameter:
 * @stream - the stream to be closed
 * Return value:
 * 0 if operation was successful
 * EOF if an error occurs
 */
int __ctOS_stream_close(__ctOS_stream_t* stream) {
    int rc;
    rc = __ctOS_stream_flush(stream);
    if ((stream->buffer) && (stream->private_buffer))
        free(stream->buffer);
    stream->buffer = 0;
    stream->buf_end = -1;
    stream->buf_index = 0;
    stream->buf_size = 0;
    LIST_REMOVE(stream_list_head, stream_list_tail, stream);
    if (rc) {
        stream->error = rc;
        return EOF;
    }
    return 0;
}


/*
 * Reset stream attributes without performing any actual operation
 * The filpos attribute of the stream will be set to the value passed as argument,
 * the read index buf_index and the buffer end marker buf_end will be reset to their
 * initial value. The end-of-file indicator will be reset and any characters in the "ungetc"
 * buffer will be discarded. The dirty flag is reset and the error flag is cleared
 */
void __ctOS_stream_seek(__ctOS_stream_t* stream, unsigned int filpos) {
    stream->filpos = filpos;
    stream->buf_end = -1;
    stream->buf_index = 0;
    stream->eof = 0;
    stream->ungetc_flag = 0;
    stream->dirty = 0;
    stream->error = 0;
}

/*
 * Get file position
 */
unsigned int __ctOS_stream_tell(__ctOS_stream_t* stream) {
    if (stream->dirty) {
        return stream->filpos + stream->buf_end +1;
    }
    return stream->filpos + stream->buf_index - stream->ungetc_flag;
}

/*
 * Clear the error indicator and EOF marker for a file
 */
void __ctOS_stream_clearerr(__ctOS_stream_t* stream) {
    stream->eof = 0;
    stream->error = 0;
}

/*
 * Get the eof-indicator of a stream
 */
int __ctOS_stream_geteof(__ctOS_stream_t* stream) {
    return stream->eof;
}


/*
 * Get the error code of a stream
 */
int __ctOS_stream_geterror(__ctOS_stream_t* stream) {
    return stream->error;
}
