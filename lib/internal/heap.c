/*
 * heap.c
 *
 * This module implements a heap
 */


#include "lib/os/heap.h"
#include "lib/sys/types.h"
#include "debug.h"

/*
 * Return the next header in the chain
 * Parameter:
 * @chunk - current chunk
 * Returns:
 * a pointer to the next chunk in the chain or 0
 * if the end of the chain is reached
 */
static heap_chunk_header_t* heap_next_chunk(heap_chunk_header_t* chunk) {
    if (chunk->last)
        return 0;
    return (heap_chunk_header_t*)(((u32) chunk->footer)+4);
}

/*
 * Get the size of a chunk
 * excluding header and footer,
 * i.e. the number of freely available bytes
 * Parameters:
 * @chunk - the address of the chunk (i.e. of its header)
 * Return value:
 * size of a chunk
 */
static int heap_chunk_get_size(heap_chunk_header_t* chunk) {
    return (int) (((u32) chunk->footer) - ((u32) chunk)
            - sizeof(heap_chunk_header_t));
}

/*
 * Utility function to validate a chunk
 * Checks that footer of chunk actually points to header
 * Parameters:
 * @header - the chunk to be validated
 * Return value:
 * 0 if chunk is valid
 * ENOFOOTER if the footer is missing
 * ENOHEADER if the header is missing
 * EFOOTER if the footer address does not match the actual footer
 * ESIZE if the chunk size is negative
 */
static int heap_validate_chunk(heap_chunk_header_t* header) {
    if (header == 0)
        return __HEAP_ENOHEADER;
    if (header->footer == 0) {
        return __HEAP_ENOFOOTER;
    }
    if (*((u32*) header->footer) != ((u32) header)) {
        return __HEAP_EFOOTER;
    }
    if (heap_chunk_get_size(header) < 0) {
        return __HEAP_ESIZE;
    }
    return 0;
}

/*
 * Utility function to check an entire heap for consistency
 * Parameters:
 * @heap - the heap to be checked
 * Return value:
 * 0 if all checks are succesful
 * ERANGE if a chunk starts or extends beyond the top of the heap or starts below the heap
 * ENOFOOTER if the footer of a chunk is missing
 * ENOHEADER if the header of a chunk is missing
 * EFOOTER if the footer address of a chunk does not match the actual footer
 * ESIZE if the chunk size is negative for one chunk
 *
 */
static int heap_validate(heap_t* heap) {
    int result;
    int i = 0;
    if (heap->validate==0)
        return 0;
    heap_chunk_header_t* current = (heap_chunk_header_t*) heap->start;
    while (current) {
        i++;
        /* Verify that chunk stays with range given by heap */
        if ((u32) current > heap->current_top) {
            return __HEAP_ECHUNKRANGE;
        }
        if ((current->footer + 3) > (void*) heap->current_top) {
            return __HEAP_ECHUNKRANGE;
        }
        if ((u32) current < heap->start) {
            return __HEAP_ECHUNKRANGE;
        }
        /* Now validate chunk for internal consistency */
        result = heap_validate_chunk((void*) current);
        if (result) {
            return result;
        }
        current = heap_next_chunk(current);
    }
    return 0;
}

/*
 * Get the previous chunk
 * Parameters:
 * @chunk - the address of the chunks header
 * Return value:
 * a pointer to the previous chunk or 0 if there is no previous chunk
 */
static heap_chunk_header_t* heap_previous_chunk(heap_chunk_header_t* chunk) {
    u32* footer;
    if (0 == chunk)
        return 0;
    footer = ((u32*) chunk) - 1;
    if (0 == footer)
        return 0;
    return (heap_chunk_header_t*) (*footer);
}

/*
 * Set up a chunk in a specific area of memory ranging from @first to @last
 * Return a pointer to the header of the newly created chunk
 * The function will initialize a header structure at @first and a corresponding
 * footer at @last-3
 * The chunk will be marked as unused
 * The function will not check that between @first and @last, there is sufficient space
 * for header and footer
 * Parameters:
 * @first - first address of chunk area
 * @last - last address of chunk area
 * Return value:
 * a pointer to the newly created chunk
 */
static heap_chunk_header_t* heap_init_chunk(u32 first, u32 last) {
    heap_chunk_header_t* header = (heap_chunk_header_t*) first;
    header->footer = (void*) (last - 3);
    *((u32*) (header->footer)) = (u32) header;
    header->used = 0;
    return header;
}

/*
 * Initialize a piece of memory, specified
 * by the addresses @first and @last for usage as a heap. This function
 * will define one large chunk which consumes the entire heap
 * It returns zero on success or an error code
 * The size of the heap specified needs to be sufficient to store header and footer
 * Parameters:
 * @heap - heap structure to be initialized
 * @first - first address of heap
 * @last - last address of heap
 * @extension - extension function
 * Return value:
 * 0 if the heap could be succesfully set up, the result of heap_validate otherwise
 *
 */
int __ctOS_heap_init(heap_t* heap, u32 first, u32 last, unsigned int(*extension)(unsigned int, unsigned int)) {
    heap_chunk_header_t* header;
    heap->start = first;
    heap->current_top = last;
    heap->extension = extension;
    heap->validate = 0;
    header = heap_init_chunk(heap->start, heap->current_top);
    header->last = 1;
    return heap_validate(heap);
}

/*
 * Split a chunk at offset @offset by adding a new chunk header and footer
 * @offset is defined with respect to the start of the chunk, i.e. &header
 * Function returns 0 upon success or an error code
 * Note that the header of the newly created chunk (the upper part created by the split) is
 * located at address (u32) chunk + offset
 * Parameters:
 * @chunk - the chunk to be split
 * @offset - the offset at which we split
 * Return value:
 * 0 if split could be done
 * result of heap_validate_chunk otherwise
 */
static int heap_chunk_split(heap_chunk_header_t* chunk, u32 offset) {
    int rc;
    heap_chunk_header_t* header_old = chunk;
    heap_chunk_header_t* header_new = (heap_chunk_header_t*) ((u32) chunk
            + offset);
    if (0 == chunk)
        return 0;
    /* First fill new header structure */
    header_new->footer = header_old->footer;
    header_new->last = header_old->last;
    header_new->used = 0;
    /* Now adapt old header */
    header_old->footer = ((void*) header_new) - 4;
    header_old->last = 0;
    /* Finally adapt footers */
    *((u32*) header_old->footer) = (u32) chunk;
    *((u32*) header_new->footer) = (u32) header_new;
    rc = heap_validate_chunk(header_new);
    if (rc) {
        return rc;
    }
    rc = heap_validate_chunk(header_old);
    if (rc) {
        return rc;
    }
    return 0;
}

/*
 * Given a chunk and an alignment, return the first address above the chunk header
 * which is aligned and sufficiently above the header to be able to split there,
 * i.e. we still need enough space for a header and a footer
 * Parameter:
 * @chunk - chunk
 * @alignment - requested alignment
 * Return value:
 * the first address above the chunk header
 * which is aligned and sufficiently above the header to be able to split there
 */
static u32 heap_get_aligned_address(heap_chunk_header_t* chunk, int alignment) {
    /* This is the first address >= base which is aligned as requested */
    u32 base = ((u32) chunk) + sizeof(heap_chunk_header_t);
    u32 base_aligned = (base / alignment) * alignment + alignment;
    /* Make sure that when splitting here, we do not overlap our
     * new header or footer with the previous header.
     */
    while ((base_aligned - sizeof(heap_chunk_header_t)) < ((u32) chunk
            + sizeof(heap_chunk_header_t)+4)) {
        base_aligned += alignment;
    }
    return base_aligned;
}

/*
 * Consume a free chunk, i.e. mark it as used
 * If alignment is not equal to 1, the chunk will be split into an upper part fulfilling
 * the alignment and a lower part which is not aligned. The upper part will then be split
 * into a part servicing the request and a remainder
 * Parameters:
 * @heap - the heap
 * @chunk - the chunk to be consumed
 * @requested_size - the size of requested memory
 * @alignment - the requested alignment
 * Return value:
 * 0 if the operation failed and a pointer to first usable byte
 * of the chunk (i.e. the first byte after the header) if the operation
 * was successful
 */
static void* heap_consume_chunk(heap_t* heap, heap_chunk_header_t* chunk,
        u32 requested_size, u32 alignment) {
    u32 offset;
    u32 base;
    u32 base_aligned;
    int rc;
    rc = heap_validate(heap);
    if (rc) {
        return 0;
    }
    /* If splitting is necessary to fulfill alignment, we split first
     * into a lower part which is unaligned and an upper part which is aligned
     * We then continue to work with the upper part only
     */
    base = (u32) chunk + sizeof(heap_chunk_header_t);
    if (base % alignment) {
        base_aligned = heap_get_aligned_address(chunk, alignment);
        offset = base_aligned - (u32) chunk - sizeof(heap_chunk_header_t);
        /* Split sizeof(heap_chunk_header_t) below this address */
        rc = heap_chunk_split(chunk, offset);
        if (rc) {
            return 0;
        }
        chunk = (heap_chunk_header_t*) (base_aligned
                - sizeof(heap_chunk_header_t));
        rc = heap_validate(heap);
        if (rc) {
            return 0;
        }
    }
    /* Check whether splitting the chunk makes sense
     * We only do a split if the chunk is big enough
     * to store the new header and footer
     * and still has a size of more than 4 byte
     */
    if (heap_chunk_get_size(chunk) > (requested_size + 8
            + sizeof(heap_chunk_header_t))) {
        offset = requested_size + sizeof(heap_chunk_header_t) + 4;
        rc = heap_chunk_split(chunk, offset);
        if (rc) {
            return 0;
        }
        rc = heap_validate(heap);
        if (rc) {
            return 0;
        }
    }
    rc = heap_validate(heap);
    if (rc) {
        return 0;
    }
    chunk->used = 1;
    return ((void*) chunk) + sizeof(heap_chunk_header_t);
}

/*
 * Determine whether a given chunk is big enough to service a request, taking alignment into account
 * If the chunk is already in use, false is returned
 * Parameter:
 * @chunk - the chunk to be checked
 * @requested - number of requested bytes
 * @alignment - requested aligment
 * Return value:
 * 1 if check is successful, 0 otherwise
 */
static int heap_chunk_sufficient(heap_chunk_header_t* chunk, u32 requested,
        u32 alignment) {
    if (0 == chunk)
        return 0;
    if (chunk->used)
        return 0;
    /* This is the base address of the usable area within the chunk */
    u32 base = ((u32) chunk) + sizeof(heap_chunk_header_t);
    /* If the base address is already aligned as needed, check whether we have enough
     * space and return
     */
    if (0 == (base % alignment)) {
        return (heap_chunk_get_size(chunk) >= requested);
    }
    /* This is the first address >= base which is aligned as requested */
    u32 base_aligned = heap_get_aligned_address(chunk, alignment);
    /*
     * If we want to return base address to the user, we have to split
     * sizeof(heap_chunk_header_t) bytes below base address. So we need to make
     * sure that the space between base_aligned and footer is still big enough to service the request
     */
    if (((u32) chunk->footer) <= base_aligned + requested)
        return 0;
    return 1;
}

/*
 * Malloc
 * This function will allocate a part of the heap and return a pointer of it to the callee
 * The function will return memory allocated at @alignment bytes, i.e. the address returned
 * to the user will be a multiple of @alignment
 * First the linked list of chunks is scanned for a chunk which is big enough to fulfill the
 * request.
 * If no chunk is found, the extension function defined in the heap data structure is called
 * to enlarge the heap
 * Parameters:
 * @heap - the heap on which we operate
 * @size - number of requested bytes
 * Return value:
 * pointer to allocated memory or 0 if allocation failed
 */
void* __ctOS_heap_malloc_aligned(heap_t* heap, u32 size, u32 alignment) {
    heap_chunk_header_t* current = (heap_chunk_header_t*) heap->start;
    heap_chunk_header_t* last;
    u32 extension;
    u32 extension_size;
    int rc;
    void* ptr;
    if (0==size)
        return 0;
    /* First go through available chunks and search for a free
     * chunk which is big enough, taking alignment into account
     */
    while (0 != current) {
        if (heap_chunk_sufficient(current, size, alignment)) {
            ptr = heap_consume_chunk(heap, current, size, alignment);
            rc=heap_validate(heap);
            if (rc) {
                return 0;
            }
            if (heap_chunk_get_size((heap_chunk_header_t*)(ptr-sizeof(heap_chunk_header_t))) < size) {
                return 0;
            }
            return ptr;
        }
        current = heap_next_chunk(current);
    }
    /* If we got to this point, there is no free chunk
     * so we need to request an extension
     * First figure out how much space we need taking extension into account
     */
    if (1 == alignment)
        extension_size = size + 4 + sizeof(heap_chunk_header_t);
    else {
        extension_size = heap_get_aligned_address(
                (heap_chunk_header_t*) heap->current_top + 1, alignment)
                - heap->current_top + size + 8;
    }
    /*
     * The extension function is supposed to return zero
     * if no extension is possible
     */
    if (0==heap->extension) {
        return 0;
    }
    extension = heap->extension(extension_size, heap->current_top);
    if (0 == extension)
        return 0;
    current = (heap_chunk_header_t*) (heap->current_top + 1);
    /* Get pointer to last chunk in the old heap size */
    last = (heap_chunk_header_t*) *((u32*) (heap->current_top - 3));
    /* Fix references in linked list */
    last->last = 0;
    current->last = 1;
    /* Set up new header and footer */
    heap_init_chunk(heap->current_top + 1, extension);
    /* Adapt heap structure */
    heap->current_top = extension;
    heap_init_chunk((u32) current, heap->current_top);
    /*finally consume and return new chunk */
    ptr = heap_consume_chunk(heap, current, size, alignment);
    rc=heap_validate(heap);
    if (rc) {
        return 0;
    }
    if (heap_chunk_get_size((heap_chunk_header_t*)(ptr-sizeof(heap_chunk_header_t))) < size) {
        return 0;
    }
    return ptr;
}

/*
 * Interface to malloc function when no alignment
 * is requested
 * Parameter:
 * @heap - heap to use
 * @size - number of bytes requested
 * Return value:
 * pointer to allocated memory or 0 if allocation failed
 */
void* __ctOS_heap_malloc(heap_t* heap, u32 size) {
    return __ctOS_heap_malloc_aligned(heap, size, sizeof(unsigned int));
}

/*
 * Free
 * This function will mark the chunk pointed to by ptr as unused.
 * If possible, it will also try to merge the returned chunk with adjacent chunks
 * to avoid memory fragmentation
 * Parameters:
 * @heap - the heap to operate on
 * @ptr - a pointer to a previously allocated memory
 */
void __ctOS_heap_free(heap_t* heap, void* ptr) {
    heap_chunk_header_t* header = (heap_chunk_header_t*) (ptr
            - sizeof(heap_chunk_header_t));
    heap_chunk_header_t* first_free;
    heap_chunk_header_t* last_free;
    u8 orig_last;
    header->used = 0;
    first_free = header;
    last_free = header;
    /* Advance last_free until it points to the
     * last free header equal to or above the
     * current one
     */
    while (1) {
        if (0 == heap_next_chunk(last_free))
            break;
        if (1 == heap_next_chunk(last_free)->used)
            break;
        last_free = heap_next_chunk(last_free);
    }
    /* Similarly make first_free point
     * to the last chunk equal to or before
     * the current one which is free
     */
    while (1) {
        if (((u32) first_free) <= heap->start)
            break;
        if (1 == heap_previous_chunk(first_free)->used)
            break;
        first_free = heap_previous_chunk((heap_chunk_header_t*) first_free);
    }
    /* Merge the area between first_free and last_free
     * into one big chunk, mark it as unused and
     * fix references in list of chunks
     */
    orig_last = last_free->last;
    heap_init_chunk((u32) first_free, ((u32) last_free->footer) + 3)->used = 0;
    first_free->last = orig_last;
    return;
}

/*
 * Reallocate an object on the the heap.
 * If the new size is less than or equal to the existing size, no operation is performed. Otherwise,
 * it is tried to allocate a new, larger area of the heap.
 *
 * Potential improvement: if the new size is less than the old size and there is enough room to split the chunk,
 * we could do this to save some space...
 *
 * Parameter:
 * @heap - the heap
 * @ptr - pointer to the existing object
 * @size - new size
 */
void* __ctOS_heap_realloc(heap_t* heap, void* ptr, size_t size) {
    size_t i;
    if (0==ptr)
        return __ctOS_heap_malloc(heap, size);
    heap_chunk_header_t* header = (heap_chunk_header_t*) (ptr
                - sizeof(heap_chunk_header_t));
    size_t old_size = heap_chunk_get_size(header);
    if (size <= old_size)
        return ptr;
    /*
     * Need to allocate more memory
     */
    void* new_ptr = __ctOS_heap_malloc(heap, size);
    if (0==new_ptr)
        return 0;
    __ctOS_heap_free(heap, ptr);
    for (i=0;i<old_size;i++)
        ((char*)new_ptr)[i]=((char*)ptr)[i];
    return new_ptr;
}
