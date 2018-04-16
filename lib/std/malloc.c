/*
 * malloc.c
 */

#include "lib/os/heap.h"
#include "lib/string.h"

/*
 * The heap
 */
heap_t __ctOS_heap;

/*
 * The malloc() function will allocate unused space for an object whose size in bytes is specified by size and whose value is unspecified.
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * none
 */
void *malloc(size_t size) {
    return __ctOS_heap_malloc(&__ctOS_heap, size);
}

/*
 * The free() function will cause the space pointed to by ptr to be deallocated; that is, made available for further allocation.
 *
 * If ptr is a null pointer, no action shall occur.
 *
 * Otherwise, if the argument does not match a pointer earlier returned by the calloc(), malloc(),
 * realloc(), or strdup() function, or if the space has been deallocated by a call to free() or realloc(), the behavior is undefined.
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * none
 */
void free(void* mem) {
    if (mem)
        __ctOS_heap_free(&__ctOS_heap, mem);
}

/*
 * Realloc library function
 *
 * The realloc() function shall change the size of the memory object pointed to by ptr to the size specified by size.
 * The contents of the object shall remain unchanged up to the lesser of the new and old sizes. If the new size of the
 * memory object would require movement of the object, the space for the previous instantiation of the object is freed.
 * If the new size is larger, the contents of the newly allocated portion of the object are unspecified.
 * If size is 0 and ptr is not a null pointer, the object pointed to is freed.
 * If the space cannot be allocated, the object shall remain unchanged.
 *
 * If ptr is a null pointer, realloc() shall be equivalent to malloc() for the specified size.
 *
 * If ptr does not match a pointer returned earlier by calloc(), malloc(), or realloc() or if the space has previously been
 * deallocated by a call to free() or realloc(), the behavior is undefined.
 *
 * The order and contiguity of storage allocated by successive calls to realloc() is unspecified.
 * The pointer returned if the allocation succeeds shall be suitably aligned so that it may be assigned
 * to a pointer to any type of object and then used to access such an object in the space allocated
 * (until the space is explicitly freed or reallocated). Each such allocation shall yield a pointer to an object
 * disjoint from any other object. The pointer returned shall point to the start (lowest byte address) of the allocated space.
 * If the space cannot be allocated, a null pointer shall be returned.
 *
 * Upon successful completion with a size not equal to 0, realloc() shall return a pointer to the (possibly moved) allocated space.
 * If size is 0, either a null pointer or a unique pointer that can be successfully passed to free() shall be returned.
 * If there is not enough available memory, realloc() shall return a null pointer and set errno to [ENOMEM].
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS: none
 */
void* realloc(void* ptr, size_t size) {
    return __ctOS_heap_realloc(&__ctOS_heap, ptr, size);
}

/*
 * Calloc
 *
 * Allocate space for nelem objects of size elsize
 */
void *calloc(size_t nelem, size_t elsize) {
    void* mem = 0;
    if ((0==nelem) || (0==elsize))
        return 0;
    mem = malloc(nelem*elsize);
    if (mem) {
        memset(mem, 0, nelem*elsize);
    }
    return mem;
}
