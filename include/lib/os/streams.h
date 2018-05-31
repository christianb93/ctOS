/*
 * streams.h
 *
 */

#ifndef __STREAMS_H_
#define __STREAMS_H_

#include "types.h"

/*
 * This structure represents a stream
 */
typedef struct ___ctOS_stream_t {
    unsigned char* buffer;                      // the buffer used by this stream
    unsigned int buf_size;                      // the total size of the buffer
    int fd;                                     // the file descriptor associated with the stream - -1 means no associated file
    int buf_index;                              // the position within the buffer, i.e. the next byte received by getc
    unsigned int filpos;                        // the position within the file which corresponds to the first byte in the buffer
    int buf_end;                                // the index of the last byte within the buffer which is filled
    int buf_mode;                               // buffering mode
    unsigned char ungetc_buffer;                // buffer used to hold a character which has been put back into the stream
    int ungetc_flag;                            // this is set if a character has been put back but not read again
    int error;                                  // error number - caller needs to set this to zero before invoking any method on the stream
    int dirty;                                  // set by a write operation, cleared by a read
    int eof;                                    // set if the EOF has been reached
    int private_buffer;                         // set to indicate that the buffer is private and needs to be freed upon close
    struct ___ctOS_stream_t* next;
    struct ___ctOS_stream_t* prev;
} __ctOS_stream_t;




int __ctOS_stream_open(__ctOS_stream_t* stream, int fd);
int __ctOS_stream_close(__ctOS_stream_t* stream);
int __ctOS_stream_flush(__ctOS_stream_t* stream);
int __ctOS_stream_putc(__ctOS_stream_t* stream, int c);
int __ctOS_stream_getc(__ctOS_stream_t* stream);
int __ctOS_stream_ungetc(__ctOS_stream_t* stream, int c);
int __ctOS_stream_setvbuf(__ctOS_stream_t* stream, char* buffer, int type, int size);
int __ctOS_stream_close(__ctOS_stream_t* stream);
int __ctOS_stream_flush_all();
void __ctOS_stream_seek(__ctOS_stream_t* stream, unsigned int filpos);
unsigned int __ctOS_stream_tell(__ctOS_stream_t* stream);
void __ctOS_stream_clearerr(__ctOS_stream_t* stream);
int __ctOS_stream_geteof(__ctOS_stream_t* stream);
int __ctOS_stream_geterror(__ctOS_stream_t* stream);
int __ctOS_stream_seterror(__ctOS_stream_t* stream);
int __ctOS_stream_freadahead(__ctOS_stream_t* stream);
int __ctOS_stream_freading(__ctOS_stream_t* stream);
const char*  __ctOS_stream_freadptr(__ctOS_stream_t* stream, size_t* sizep);
void __ctOS_stream_freadptrinc(__ctOS_stream_t* stream, size_t increment);
int __ctOS_stream_fpurge(__ctOS_stream_t* stream);

#endif /* __STREAMS_H_ */
