/*
 * heap.h
 */

#ifndef _HEAP_H_
#define _HEAP_H_

#include "types.h"

/*
 * This data structure defines a heap
 * Note that this heap can be placed
 * anywhere in the virtual address space
 * The function extension is invoked when the heap
 * needs to be extended and is supposed to
 * return the new current top of the heap
 */
typedef struct {
    unsigned int start;
    unsigned int current_top;
    unsigned int (*extension)(unsigned int size, unsigned int current_top);
    int validate;
} heap_t;

/*
 * The heap is divided into chunks. Each
 * chunk is preceded by a header
 * and followed by a footer. The footer is
 * simply a pointer to the corresponding
 * header, whereas the headers form
 * a linked list
 */

typedef struct {
    void* footer;
    unsigned char last :1;
    unsigned char used :1;
} __attribute__ ((packed)) heap_chunk_header_t;

/*
 * Internal error codes
 */
#define __HEAP_ENOHEADER 1
#define __HEAP_ENOFOOTER 2
#define __HEAP_EFOOTER 3
#define __HEAP_ECHUNKRANGE 4
#define __HEAP_ESIZE 5

int __ctOS_heap_init(heap_t* heap, unsigned int first, unsigned int last, unsigned int(*extension)(unsigned int, unsigned int));
void* __ctOS_heap_malloc(heap_t* heap, unsigned int size);
void* __ctOS_heap_malloc_aligned(heap_t* heap, unsigned int size, unsigned int alignment);
void __ctOS_heap_free(heap_t* heap, void* ptr);
void* __ctOS_heap_realloc(heap_t* heap, void* ptr, size_t size);

#endif /* _HEAP_H_ */
