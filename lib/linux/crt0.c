/*
 * crt0.c
 *
 */

#include "lib/os/heap.h"
#include "lib/os/oscalls.h"
#include "lib/stdio.h"
#include "lib/unistd.h"
#include "lib/stdlib.h"
#include "lib/string.h"


extern int main(int argc, char** argv);
extern void _exit(int status);

void _fini() {};

extern heap_t __ctOS_heap;


/*
 * The heap extension function
 */
static unsigned int __ctOS_extend_heap(unsigned int size, unsigned int current_top) {
    unsigned int new_brk;
    /*
     * Verify that current top is last byte of page
     */
    if ((current_top+1) % 4096)
        return 0;
    /*
     * Increase heap area using sbrk system call
     */
    new_brk = __ctOS_sbrk(size);
    if (0==new_brk)
        return 0;
    return new_brk-1;
}

/*
 * This is the entry point of each executable linked against the library
 */
void _start(int argc, char** argv, char** envp) {
    unsigned int current_brk;
    unsigned int new_brk;
    /*
     * Allocate heap. First we get the current break, i.e. the first
     * unallocated byte
     */
    current_brk = __ctOS_sbrk(0);
    if (0==current_brk) {
        _exit(1);
    }
    /*
     * Now allocate one additional page and set up heap
     * within that page.
     */
    new_brk = __ctOS_sbrk(4096);
    if (0==new_brk) {
        _exit(1);
    }
    __ctOS_heap_init(&__ctOS_heap,current_brk, new_brk-1, __ctOS_extend_heap);
    /*
     * Initialize the pre-defined streams
     */
    if (__ctOS_stream_open(stdin,STDIN_FILENO )) {
        _exit(2);
    }
    __ctOS_stream_setvbuf(stdin, 0, _IOLBF, 0);
    if(__ctOS_stream_open(stdout, STDOUT_FILENO)) {
        _exit(3);
    }
    __ctOS_stream_setvbuf(stdout, 0, _IOLBF, 0);
    if (__ctOS_stream_open(stderr, STDERR_FILENO)) {
        _exit(4);
    }
    __ctOS_stream_setvbuf(stderr, 0, _IONBF, 0);
    /*
     * Finally invoke main and exit
     */
    int res = main(argc, argv);
    _exit(res);
    while(1);
}
