/*
 * test_dirstreams.c
 */

#include "kunit.h"
#include "vga.h"
#include "lib/os/dirstreams.h"
#include <stdio.h>
#include "lib/unistd.h"
#include <stdlib.h>


void win_kputchar(win_t* win, char c) {
    write(1, &c, 1);
}


/***************************************
 * Simulated read operation            *
 **************************************/

/*
 * We simulate a directory with ten entries, named 000 to 999
 */
static int filpos = 0;
static int getdent_called = 0;
int __ctOS_getdent(int fd, __ctOS_direntry_t* direntry) {
    getdent_called = 1;
    if (filpos >9)
        return -1;
    direntry->inode_nr = filpos;
    memset((void*) direntry->name, 0, 255);
    memset((void*) direntry->name, '0'+filpos, 3);
    filpos++;
    return 0;
}

/***************************************
 * Actual testcases start here         *
 **************************************/


/*
 * Testcase 1: open a directory stream
 */
int testcase1() {
    __ctOS_dirstream_t stream;
    ASSERT(0==__ctOS_dirstream_open(&stream, 0));
    return 0;
}

/*
 * Testcase 2: read from a directory stream once
 */
int testcase2() {
    __ctOS_dirstream_t stream;
    __ctOS_direntry_t* direntry;
    ASSERT(0==__ctOS_dirstream_open(&stream, 0));
    filpos = 0;
    direntry = __ctOS_dirstream_readdir(&stream);
    ASSERT(direntry);
    ASSERT(direntry->inode_nr==0);
    ASSERT(0==strcmp(direntry->name, "000"));
    return 0;
}

/*
 * Testcase 3: read from a directory stream twice
 */
int testcase3() {
    __ctOS_dirstream_t stream;
    __ctOS_direntry_t* direntry;
    ASSERT(0==__ctOS_dirstream_open(&stream, 0));
    filpos = 0;
    direntry = __ctOS_dirstream_readdir(&stream);
    ASSERT(direntry);
    ASSERT(direntry->inode_nr==0);
    ASSERT(0==strcmp(direntry->name, "000"));
    direntry = __ctOS_dirstream_readdir(&stream);
    ASSERT(direntry);
    ASSERT(direntry->inode_nr==1);
    ASSERT(0==strcmp(direntry->name, "111"));
    return 0;
}

/*
 * Testcase 4: set up a directory stream with buffer size 2. Read once
 * to fill up the buffer, then read a second time to retrieve the second entry as
 * well. Read a third time and verify that another read operation is triggered
 */
int testcase4() {
    __ctOS_dirstream_t stream;
    __ctOS_direntry_t* direntry;
    ASSERT(0==__ctOS_dirstream_open(&stream, 0));
    stream.buf_size=2;
    filpos = 0;
    getdent_called = 0;
    direntry = __ctOS_dirstream_readdir(&stream);
    ASSERT(direntry);
    ASSERT(direntry->inode_nr==0);
    ASSERT(0==strcmp(direntry->name, "000"));
    ASSERT(1==getdent_called);
    getdent_called = 0;
    direntry = __ctOS_dirstream_readdir(&stream);
    ASSERT(direntry);
    ASSERT(direntry->inode_nr==1);
    ASSERT(0==strcmp(direntry->name, "111"));
    ASSERT(0==getdent_called);
    getdent_called = 0;
    direntry = __ctOS_dirstream_readdir(&stream);
    ASSERT(direntry);
    ASSERT(direntry->inode_nr==2);
    ASSERT(0==strcmp(direntry->name, "222"));
    ASSERT(1==getdent_called);
    return 0;
}

/*
 * Testcase 5: read from a directory stream until all directory entries have been read
 */
int testcase5() {
    int i;
    __ctOS_dirstream_t stream;
    __ctOS_direntry_t* direntry;
    ASSERT(0==__ctOS_dirstream_open(&stream, 0));
    filpos = 0;
    for (i=0;i<10;i++) {
        ASSERT(__ctOS_dirstream_readdir(&stream));
    }
    ASSERT(0==__ctOS_dirstream_readdir(&stream));
    return 0;
}

/*
 * Testcase 6: close a stream
 */
int testcase6() {
    int i;
    __ctOS_dirstream_t stream;
    __ctOS_direntry_t* direntry;
    ASSERT(0==__ctOS_dirstream_open(&stream, 0));
    filpos = 0;
    ASSERT(__ctOS_dirstream_readdir(&stream));
    __ctOS_dirstream_close(&stream);
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
    END;
}
