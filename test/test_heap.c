/*
 * test_heap.c
 */

#include <stdio.h>
#include "kunit.h"
#include "vga.h"
#include "ktypes.h"
#include "lib/os/heap.h"
#include <stdlib.h>

static int extension_requested;
static u32 new_top;

typedef struct {
    void* start;
    void* end;
} mem_range_t;

/*
 * Stub for extension function
 */
u32 extension_func(u32 size, u32 current_top) {
    extension_requested = 1;
    if (new_top == current_top)
        return 0;
    if (new_top >= current_top + size)
        return current_top + size;
    else
        return 0;
}

/*
 * Some standard stubs
 */

void win_putchar(win_t* win, u8 c) {
    printf("%c", c);
}

u8 early_getchar() {
    return 'a';
}

void trap() {
    printf("Trap called\n");
}

/*
 * Get the size of a chunk
 * excluding header and footer,
 * i.e. the number of freely available bytes
 */
static u32 heap_chunk_get_size(u32 chunk) {
    return (u32) (((heap_chunk_header_t*) chunk)->footer - chunk
            - sizeof(heap_chunk_header_t));
}

/*
 * Utility function to validate that a given set of memory
 * ranges does not contain any mutually overlapping ranges
 * Returns 1 upon success
 */
int validate_overlaps(mem_range_t* ranges, int nr_of_entries) {
    int i;
    int j;
    for (i = 0; i < nr_of_entries; i++)
        for (j = i + 1; j < nr_of_entries; j++) {
            if (ranges[i].start == ranges[j].start)
                return 0;
            if (ranges[i].end == ranges[j].end)
                return 0;
            if ((ranges[i].start < ranges[j].start) && (ranges[i].end
                    >= ranges[j].end))
                return 0;
            if ((ranges[j].start < ranges[i].start) && (ranges[j].end
                    >= ranges[i].end))
                return 0;
        }
    return 1;
}



/*
 * Testcase 1:
 * Tested function: heap/__ctOS_heap_init
 * Testcase: set up a 4096 byte heap
 */
int testcase1() {
    void* page;
    heap_t heap;
    heap_chunk_header_t* header;
    page = malloc(4096);
    ASSERT(page);
    ASSERT(0==__ctOS_heap_init(&heap, (u32) page, (u32)(page+4095), extension_func));
    header = (heap_chunk_header_t*) page;
    ASSERT(header->used==0);
    ASSERT(header->last==1);
    ASSERT(header->footer);
    ASSERT(((heap_chunk_header_t*) *((unsigned int*)(header->footer)))==header);
    free(page);
    return 0;
}

/*
 * Testcase 2:
 * Tested function: heap/__ctOS_heap_malloc
 * Testcase: set up a 100 byte heap
 * and then request a 16 byte block.
 * We should get a pointer into the first
 * chunk and create a split, i.e.
 * the returned piece of memory
 * should fit the request exactly
 */
int testcase2() {
    char  __attribute__ ((aligned(256))) page[100];
    heap_t heap;
    heap_chunk_header_t* header;
    void* ptr;
    ASSERT(0 == __ctOS_heap_init(&heap, (u32) page, (u32) page + 99, extension_func));
    heap.validate = 1;
    header = (heap_chunk_header_t*) page;
    extension_requested = 0;
    /* Now call malloc */
    ptr = (void*) __ctOS_heap_malloc(&heap, 16);
    ASSERT(0 == extension_requested);
    ASSERT(ptr);
    header = (heap_chunk_header_t*) (ptr - sizeof(heap_chunk_header_t));
    ASSERT(heap_chunk_get_size((u32) header) == 16);
    ASSERT(header->used == 1);
    ASSERT(header->last==0);
    return 0;
}

/*
 * Testcase 3:
 * Tested function: heap/__ctOS_heap_malloc
 * Testcase: set up a 100 byte heap
 * and then request a 160 byte block
 * This should invoke the extension function -
 * we simulate the case that the extension fails
 */
int testcase3() {
    void* page;
    heap_t heap;
    heap_chunk_header_t* header;
    void* ptr;
    page = malloc(200);
    ASSERT(page);
    ASSERT(0 == __ctOS_heap_init(&heap, (u32) page, (u32) page + 99, extension_func));
    heap.validate = 1;
    extension_requested = 0;
    new_top = heap.current_top;
    /* Now call malloc */
    ptr = (void*) __ctOS_heap_malloc(&heap, 160);
    ASSERT(1 == extension_requested);
    ASSERT(0 == ptr);
    free(page);
    return 0;
}

/*
 * Testcase 4:
 * Tested function: heap/__ctOS_heap_malloc
 * Testcase: set up a 100 byte heap
 * and then request a 160 byte block
 * This should invoke the extension function -
 * we simulate the case that the extension is successful
 */
int testcase4() {
    void* page;
    heap_t heap;
    heap_chunk_header_t* header;
    void* ptr;
    page = malloc(400);
    ASSERT(page);
    ASSERT(0 == __ctOS_heap_init(&heap, (u32) page, (u32) page + 99, extension_func));
    heap.validate = 1;
    extension_requested = 0;
    new_top = (u32) page + 399;
    /* Now call malloc */
    ptr = (void*) __ctOS_heap_malloc(&heap, 160);
    ASSERT(1 == extension_requested);
    ASSERT(ptr);
    free(page);
    return 0;
}

/*
 * Testcase 5:
 * Tested function: heap/free
 * Testcase: set up a 100 byte heap
 * (i.e. 84 usable bytes)
 * and request a 60 byte block
 * This will create a split
 * and thus consume 16 more bytes
 * for header and footer
 * Free it again and check that
 * free was successful by requesting
 * 30 bytes
 */
int testcase5() {
    void* page;
    heap_t heap;
    heap_chunk_header_t* header;
    void* ptr;
    page = malloc(100);
    ASSERT(page);
    heap.start = (u32) page;
    ASSERT(0 == __ctOS_heap_init(&heap, (u32) page, (u32) page + 99, extension_func));
    heap.validate = 1;
    extension_requested = 0;
    new_top = (u32) page + 99;
    /* Now call malloc */
    ptr = (void*) __ctOS_heap_malloc(&heap, 60);
    ASSERT(0 == extension_requested);
    ASSERT(ptr);
    /* Free */
    __ctOS_heap_free(&heap, ptr);
    /* and allocate again */
    ptr = (void*) __ctOS_heap_malloc(&heap, 30);
    ASSERT(ptr);
    ASSERT(0 == extension_requested);
    free(page);
    return 0;
}

/*
 * Testcase 6:
 * Tested function: heap/free
 * Testcase: verify that merging of
 * freed chunks works, using the following
 * scenario
 * - first create a heap of 4096 bytes
 * - then allocate two pieces of 2000 bytes each
 * - free both of them
 * - verify that a request for 3000 bytes is succesful
 */
int testcase6() {
    void* page;
    heap_t heap;
    heap_chunk_header_t* header;
    void* ptr1;
    void* ptr2;
    page = malloc(4096);
    ASSERT(page);
    ASSERT(0 == __ctOS_heap_init(&heap, (u32) page, (u32) page + 4095, extension_func));
    heap.validate = 1;
    extension_requested = 0;
    new_top = (u32) page + 4095;
    /* Now call malloc two times */
    ptr1 = __ctOS_heap_malloc(&heap, 2000);
    ASSERT(0 == extension_requested);
    ASSERT(ptr1);
    ptr2 = __ctOS_heap_malloc(&heap, 2000);
    ASSERT(0 == extension_requested);
    ASSERT(ptr2);
    /* Verify that heap is full */
    ASSERT(0 == __ctOS_heap_malloc(&heap, 3000));
    /* Free */
    __ctOS_heap_free(&heap, ptr1);
    __ctOS_heap_free(&heap, ptr2);
    /* and allocate again */
    ptr1 = __ctOS_heap_malloc(&heap, 3000);
    ASSERT(ptr1);
    free(page);
    return 0;
}

/*
 * Testcase 7:
 * Tested function: heap/__ctOS_heap_malloc
 * Testcase: allocate 10 blocks and verify
 * that they do not overlap
 */
int testcase7() {
    void* page;
    heap_t heap;
    heap_chunk_header_t* header;
    int i;
    mem_range_t ranges[10];
    page = malloc(4096);
    ASSERT(page);
    ASSERT(0 == __ctOS_heap_init(&heap, (u32) page, (u32) page + 4095, extension_func));
    heap.validate = 1;
    extension_requested = 0;
    new_top = (u32) page + 4095;
    for (i = 0; i < 10; i++) {
        ranges[i].start = __ctOS_heap_malloc(&heap, 30);
        ASSERT(ranges[i].start);
        ranges[i].end = ranges[i].start + 29;
        ASSERT(ranges[i].end <= page + 4095);
    }
    /* Now make sure that the ranges do not overlap */
    ASSERT(1 == validate_overlaps(ranges, 10));
    return 0;
}

/*
 * Testcase 8:
 * Tested function: heap/__ctOS_heap_malloc
 * Testcase: request aligned memory
 * and check that alignment happens
 * alignment is bigger than sizeof(heap_chunk_header_t)
 */
int testcase8() {
    void* page;
    heap_t heap;
    void* ptr;
    page = malloc(4096);
    ASSERT(page);
    ASSERT(0 == __ctOS_heap_init(&heap, (u32) page, (u32) page + 4095, extension_func));
    heap.validate = 1;
    extension_requested = 0;
    new_top = (u32) page + 4095;
    /* Request memory aligned at 128 bytes */
    ptr = __ctOS_heap_malloc_aligned(&heap, 40, 128);
    ASSERT(ptr);
    ASSERT(0==(((u32)ptr) % 128));
    free(page);
    return 0;
}

/*
 * Testcase 9:
 * Tested function: heap/__ctOS_heap_malloc
 * Testcase: request aligned memory
 * and check that alignment happens
 * alignment is 1
 */
int testcase9() {
    void* page;
    heap_t heap;
    void* ptr;
    page = malloc(4096);
    ASSERT(page);
    ASSERT(0 == __ctOS_heap_init(&heap, (u32) page, (u32) page + 4095, extension_func));
    heap.validate = 1;
    extension_requested = 0;
    new_top = (u32) page + 4095;
    /* Request memory  */
    ptr = __ctOS_heap_malloc_aligned(&heap, 40, 1);
    ASSERT(ptr);
    free(page);
    return 0;
}

/*
 * Testcase 10:
 * Tested function: heap/__ctOS_heap_malloc
 * Testcase: request aligned memory
 * and check that alignment happens
 * Alignment is 27, 5 or 8
 */
int testcase10() {
    void* page;
    heap_t heap;
    void* ptr;
    page = malloc(4096);
    ASSERT(page);
    ASSERT(0 == __ctOS_heap_init(&heap, (u32) page, (u32) page + 4095, extension_func));
    heap.validate = 1;
    extension_requested = 0;
    new_top = (u32) page + 4095;
    /* Request memory aligned at 27 bytes */
    ptr = __ctOS_heap_malloc_aligned(&heap, 30, 27);
    ASSERT(0==extension_requested);
    ASSERT(ptr);
    ASSERT(0==(((u32) ptr) % 27));
    /* Request memory aligned at 8 bytes */
    ptr = __ctOS_heap_malloc_aligned(&heap, 30, 8);
    ASSERT(ptr);
    ASSERT(0==(((u32) ptr) % 8));
    /* Request memory aligned at 5 bytes */
    ptr = __ctOS_heap_malloc_aligned(&heap, 40, 5);
    ASSERT(ptr);
    ASSERT(0==(((u32) ptr) % 5));
    free(page);
    return 0;
}

/*
 * Testcase 11:
 * Tested function: heap/__ctOS_heap_malloc
 * Testcase: allocate 10 blocks and verify
 * that they do not overlap
 * Use different alignments
 */
int testcase11() {
    void* page;
    heap_t heap;
    heap_chunk_header_t* header;
    int i;
    int alignment;
    mem_range_t ranges[20];
    page = malloc(8192);
    ASSERT(page);
    ASSERT(0 == __ctOS_heap_init(&heap, (u32) page, (u32) page + 4095, extension_func));
    heap.validate = 1;
    extension_requested = 0;
    new_top = (u32) page + 8192;
    for (i = 0; i < 20; i++) {
        alignment = (i % 8) * 2 + 27 + (i % 2);
        ranges[i].start = __ctOS_heap_malloc_aligned(&heap, 30 + (i % 15) * 3,
                alignment);
        ASSERT(ranges[i].start);
        ASSERT(0== ((u32)ranges[i].start % alignment));
        ranges[i].end = ranges[i].start + 29;
        ASSERT(ranges[i].end <= page + 8191);
    }
    /* Now make sure that the ranges do not overlap */
    ASSERT(1 == validate_overlaps(ranges, 20));
    free(page);
    return 0;
}

/*
 * Testcase 12:
 * Tested function: heap/__ctOS_heap_malloc
 * Testcase: request aligned memory
 * and check that alignment happens
 * Alignment is 241
 * Extension necessary to fulfill request
 */
int testcase12() {
    void* page;
    heap_t heap;
    void* ptr;
    page = malloc(4096);
    ASSERT(page);
    ASSERT(0 == __ctOS_heap_init(&heap, (u32) page, (u32) page + 16, extension_func));
    heap.validate = 1;
    extension_requested = 0;
    new_top = (u32) page + 4095;
    /* Request memory aligned at 235 bytes */
    ptr = __ctOS_heap_malloc_aligned(&heap, 130, 241);
    ASSERT(extension_requested);
    ASSERT(ptr);
    ASSERT(0==(((u32) ptr) % 241));
    free(page);
    return 0;
}

/*
 * Testcase 13:
 * Tested function: realloc
 * Testcase: call realloc with a smaller size and verify that the pointer is not changed
 */
int testcase13() {
    void* page;
    heap_t heap;
    void* ptr;
    void* new_ptr;
    page = malloc(4096);
    ASSERT(page);
    ASSERT(0 == __ctOS_heap_init(&heap, (u32) page, (u32) page + 2048, extension_func));
    heap.validate = 1;
    ptr = __ctOS_heap_malloc(&heap, 10);
    ASSERT(ptr);
    new_ptr = __ctOS_heap_realloc(&heap, ptr, 5);
    ASSERT(ptr==new_ptr);
    return 0;
}

/*
 * Testcase 14:
 * Tested function: realloc
 * Testcase: call realloc with a larger size than the original object and verify that the object has been moved
 */
int testcase14() {
    void* page;
    heap_t heap;
    int i;
    void* ptr;
    void* new_ptr;
    page = malloc(4096);
    ASSERT(page);
    ASSERT(0 == __ctOS_heap_init(&heap, (u32) page, (u32) page + 2048, extension_func));
    heap.validate = 1;
    ptr = __ctOS_heap_malloc(&heap, 10);
    ASSERT(ptr);
    for (i=0;i<10;i++)
        ((char*)ptr)[i]=0xa;
    new_ptr = __ctOS_heap_realloc(&heap, ptr, 15);
    for (i=0;i<10;i++)
        ASSERT(((char*)new_ptr)[i]==0xa);
    return 0;
}

/*
 * Testcase 15:
 * Tested function: realloc
 * Testcase: call realloc with a null pointer
 */
int testcase15() {
    void* page;
    heap_t heap;
    void* ptr;
    page = malloc(4096);
    ASSERT(page);
    ASSERT(0 == __ctOS_heap_init(&heap, (u32) page, (u32) page + 2048, extension_func));
    heap.validate = 1;
    ptr = __ctOS_heap_realloc(&heap, 0, 5);
    ASSERT(ptr);
    __ctOS_heap_free(&heap, ptr);
    return 0;
}

int main() {
    INIT;
    RUN_CASE(1);
    RUN_CASE(2);
    RUN_CASE(3);
    RUN_CASE(4);
    RUN_CASE(5);
    RUN_CASE(6);
    RUN_CASE(7);
    RUN_CASE(8);
    RUN_CASE(9);
    RUN_CASE(10);
    RUN_CASE(11);
    RUN_CASE(12);
    RUN_CASE(13);
    RUN_CASE(14);
    RUN_CASE(15);
    END;
}
