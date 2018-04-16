/*
 * test_streams.c
 */

#include "kunit.h"
#include "vga.h"
#include "lib/os/streams.h"
#include <stdio.h>
#include "lib/unistd.h"

void win_putchar(win_t* win, u8 c) {
    write(1, &c, 1);
}


/***************************************
 * Simulated read/write operations     *
 **************************************/

#define TEST_FILE_SIZE 256
static int test_file_size;
/*
 * This array holds the simulated file
 */
static unsigned char test_file[TEST_FILE_SIZE];

/*
 * Position within test file
 */
static int filpos = 0;

/*
 * Set up the test file
 */
void setup_testfile() {
    int i;
    test_file_size = TEST_FILE_SIZE;
    for (i=0;i<test_file_size;i++) {
        test_file[i]='a'+(i%10);
    }
}

/*
 * Dummy for read function. We simulate a file which contains 10 characters, namely the first ten characters of the alphabet
 */
static int read_called = 0;
int __ctOS_read(int fd, char* buffer, unsigned int bytes) {
    int i;
    read_called++;
    if (filpos >= test_file_size)
        return 0;
    if (filpos + bytes > test_file_size)
        bytes = test_file_size - filpos;
    for (i=0;i<bytes;i++)
        buffer[i]=test_file[filpos+i];
    filpos += bytes;
    return bytes;
}

/*
 * Dummy for write function
 */
static int write_called = 0;
int __ctOS_write(int fd, char* buffer, unsigned int bytes) {
    int i;
    write_called++;
    if (filpos >= test_file_size)
        return 0;
    if (filpos + bytes > test_file_size)
        test_file_size = filpos + bytes;
    if (test_file_size > TEST_FILE_SIZE) {
        printf("Maximum file size exceeded, check test setup (filpos=%d, bytes=%d)\n", filpos, bytes);
        _exit(1);
    }
    for (i=0;i<bytes;i++)
        test_file[filpos+i] = buffer[i];
    filpos += bytes;
    return bytes;
}

/*
 * Dummy for lseek
 */
int __ctOS_lseek(int fd, off_t pos, int whence) {
    if ((SEEK_CUR==whence) && (0==pos)) {
        return filpos;
    }
    printf("lseek called with unexpected parameters\n");
    _exit(1);
    return 0;
}

/***************************************
 * Actual testcases start here         *
 **************************************/


/*
 * Testcase 1: open a stream referring to a file
 */
int testcase1() {
    __ctOS_stream_t stream;
    setup_testfile();
    ASSERT(0==__ctOS_stream_open(&stream, 0));
    return 0;
}

/*
 * Testcase 2: open a stream which is not associated with a file
 */
int testcase2() {
    __ctOS_stream_t stream;
    setup_testfile();
    ASSERT(0==__ctOS_stream_open(&stream, -1));
    return 0;
}

/*
 * Testcase 3: read 1 byte from a stream which has just been opened
 */
int testcase3() {
    __ctOS_stream_t stream;
    int rc;
    setup_testfile();
    ASSERT(0==__ctOS_stream_open(&stream, 9));
    rc = __ctOS_stream_getc(&stream);
    ASSERT(rc=='a');
    return 0;
}

/*
 * Testcase 4: read 1 byte and one more byte from a stream which has just been opened
 */
int testcase4() {
    __ctOS_stream_t stream;
    int rc;
    filpos = 0;
    setup_testfile();
    ASSERT(0==__ctOS_stream_open(&stream, 9));
    rc = __ctOS_stream_getc(&stream);
    ASSERT(rc=='a');
    rc = __ctOS_stream_getc(&stream);
    ASSERT(rc=='b');
    return 0;
}

/*
 * Testcase 5: read 10 bytes
 */
int testcase5() {
    __ctOS_stream_t stream;
    int rc;
    int i;
    filpos = 0;
    setup_testfile();
    test_file_size=10;
    ASSERT(0==__ctOS_stream_open(&stream, 9));
    for (i=0;i<10;i++) {
        rc = __ctOS_stream_getc(&stream);
        ASSERT(rc=='a'+i);
    }
    return 0;
}

/*
 * Testcase 6: read 11 bytes and verify that last read returns EOF
 */
int testcase6() {
    __ctOS_stream_t stream;
    int rc;
    int i;
    filpos = 0;
    setup_testfile();
    test_file_size=10;
    ASSERT(0==__ctOS_stream_open(&stream, 9));
    for (i=0;i<10;i++) {
        rc = __ctOS_stream_getc(&stream);
        ASSERT(rc=='a'+i);
    }
    ASSERT(-1==__ctOS_stream_getc(&stream));
    return 0;
}

/*
 * Testcase 7: set up a stream with buffer size 5. Then read 6 bytes in a row
 * so that two read operations are triggered
 */
int testcase7() {
    __ctOS_stream_t stream;
    int rc;
    int i;
    filpos = 0;
    read_called = 0;
    setup_testfile();
    test_file_size=10;
    ASSERT(0==__ctOS_stream_open(&stream, 9));
    stream.buf_size = 5;
    for (i=0;i<6;i++) {
        rc = __ctOS_stream_getc(&stream);
        ASSERT(rc=='a'+i);
    }
    ASSERT(2==read_called);
    return 0;
}

/*
 * Testcase 8: set up a stream and write one character to it. Verify that the character is added to
 * the buffer, but not written to the file immediately
 */
int testcase8() {
    __ctOS_stream_t stream;
    int rc;
    filpos = 0;
    read_called = 0;
    write_called = 0;
    setup_testfile();
    test_file_size=10;
    ASSERT(0==__ctOS_stream_open(&stream, 9));
    ASSERT('x'==__ctOS_stream_putc(&stream, 'x'));
    ASSERT('x'==stream.buffer[0]);
    ASSERT(0==write_called);
    return 0;
}

/*
 * Testcase 9: set up a stream and write BUFSIZE characters to it. Verify that the characters are added to
 * the buffer, but not written to the file immediately
 */
int testcase9() {
    __ctOS_stream_t stream;
    int rc;
    int i;
    filpos = 0;
    read_called = 0;
    write_called = 0;
    setup_testfile();
    ASSERT(0==__ctOS_stream_open(&stream, 9));
    for (i=0;i<stream.buf_size;i++) {
        ASSERT('x'==__ctOS_stream_putc(&stream, 'x'));
        ASSERT('x'==stream.buffer[i]);
    }
    ASSERT(0==write_called);
    return 0;
}

/*
 * Testcase 10: set up a stream and write BUFSIZE+1 characters to it. Verify that
 * - one write operation is done
 * - the first BUFSIZE characters in the file have been overwritten
 * - the buffer contains one additional character
 */
int testcase10() {
    __ctOS_stream_t stream;
    int rc;
    int i;
    filpos = 0;
    read_called = 0;
    write_called = 0;
    setup_testfile();
    ASSERT(0==__ctOS_stream_open(&stream, 9));
    for (i=0;i<stream.buf_size;i++) {
        ASSERT('x'==__ctOS_stream_putc(&stream, 'x'));
        ASSERT('x'==stream.buffer[i]);
    }
    ASSERT(0==write_called);
    ASSERT('y'==__ctOS_stream_putc(&stream, 'y'));
    ASSERT(1==write_called);
    ASSERT('y'==stream.buffer[0]);
    for (i=0;i<stream.buf_size;i++) {
        ASSERT(test_file[i]=='x');
    }
    return 0;
}

/*
 * Testcase 11: set up a stream with buf_mode = _IONBF and write one character. Verify that
 * - a write operation has been performed
 * - the byte has been written to the file
 */
int testcase11() {
    __ctOS_stream_t stream;
    int rc;
    int i;
    filpos = 0;
    read_called = 0;
    write_called = 0;
    setup_testfile();
    ASSERT(0==__ctOS_stream_open(&stream, 9));
    stream.buf_mode = _IONBF;
    ASSERT(0==write_called);
    ASSERT('y'==__ctOS_stream_putc(&stream, 'y'));
    ASSERT(1==write_called);
    ASSERT(test_file[0]=='y');
    return 0;
}

/*
 * Testcase 12: set up a stream and write 1 character to it. Then flush Verify that
 * - one write operation is done, but only after the flush
 * - the first character of the file is overwritten as expected.
 */
int testcase12() {
    __ctOS_stream_t stream;
    int rc;
    int i;
    filpos = 0;
    read_called = 0;
    write_called = 0;
    setup_testfile();
    ASSERT(0==__ctOS_stream_open(&stream, 9));
    ASSERT('y'==__ctOS_stream_putc(&stream, 'y'));
    ASSERT(0==write_called);
    ASSERT(0==__ctOS_stream_flush(&stream));
    ASSERT(1==write_called);
    ASSERT('y'==test_file[0]);
    return 0;
}

/*
 * Testcase 13: set up a stream and write 1 character to it. Then flush Verify that
 * - one write operation is done, but only after the flush
 * - the first character of the file is overwritten as expected.
 * Then write another character to the stream and verify that no additional write takes place
 */
int testcase13() {
    __ctOS_stream_t stream;
    int rc;
    int i;
    filpos = 0;
    read_called = 0;
    write_called = 0;
    setup_testfile();
    ASSERT(0==__ctOS_stream_open(&stream, 9));
    ASSERT('y'==__ctOS_stream_putc(&stream, 'y'));
    ASSERT(0==write_called);
    ASSERT(0==__ctOS_stream_flush(&stream));
    ASSERT(1==write_called);
    ASSERT('y'==test_file[0]);
    write_called = 0;
    ASSERT('z'==__ctOS_stream_putc(&stream, 'z'));
    ASSERT(0==write_called);
    ASSERT(stream.buffer[0]=='z');
    return 0;
}

/*
 * Testcase 14: set up a stream in non-buffered mode and read one character from it. Verify that
 * a read operation is performed
 */
int testcase14() {
    __ctOS_stream_t stream;
    int rc;
    int i;
    filpos = 0;
    read_called = 0;
    write_called = 0;
    setup_testfile();
    ASSERT(0==__ctOS_stream_open(&stream, 9));
    stream.buf_mode = _IONBF;
    rc = __ctOS_stream_getc(&stream);
    ASSERT(1==read_called);
    ASSERT('a'==rc);
    ASSERT(0==write_called);
    return 0;
}

/*
 * Testcase 15: read a character from a stream. Then place it back in the stream using ungetc and read again.
 * Finally perform a third read and verify that we get the next byte from the file
 */
int testcase15() {
    __ctOS_stream_t stream;
    filpos = 0;
    read_called = 0;
    write_called = 0;
    setup_testfile();
    ASSERT(0==__ctOS_stream_open(&stream, 9));
    ASSERT('a'==__ctOS_stream_getc(&stream));
    ASSERT('t'==__ctOS_stream_ungetc(&stream, 't'));
    ASSERT('t'==__ctOS_stream_getc(&stream));
    ASSERT('b'==__ctOS_stream_getc(&stream));
    return 0;
}

/*
 * Testcase 16: use setvbuf to force a stream to point to a custom buffer. Verify that the buffer is actually filled
 * if we read from the stream
 */
int testcase16() {
    __ctOS_stream_t stream;
    char mybuffer[5];
    int i;
    filpos = 0;
    read_called = 0;
    write_called = 0;
    setup_testfile();
    ASSERT(0==__ctOS_stream_open(&stream, 9));
    ASSERT(0==__ctOS_stream_setvbuf(&stream, mybuffer,_IOFBF, 5 ));
    for (i=0;i<5;i++) {
        ASSERT(__ctOS_stream_getc(&stream));
        ASSERT(mybuffer[i]==('a'+i));
    }
    return 0;
}

/*
 * Testcase 17: use setvbuf with an invalid buffering mode
 */
int testcase17() {
    __ctOS_stream_t stream;
    char mybuffer[5];
    int i;
    filpos = 0;
    read_called = 0;
    write_called = 0;
    setup_testfile();
    ASSERT(0==__ctOS_stream_open(&stream, 9));
    ASSERT(__ctOS_stream_setvbuf(&stream, mybuffer,_IOFBF+100, 5 ));
    return 0;
}

/*
 * Testcase 18: use setvbuf to set the buffering mode only
 */
int testcase18() {
    __ctOS_stream_t stream;
    int i;
    filpos = 0;
    read_called = 0;
    write_called = 0;
    setup_testfile();
    ASSERT(0==__ctOS_stream_open(&stream, 9));
    ASSERT(0==__ctOS_stream_setvbuf(&stream, 0,_IOFBF, 5 ));
    for (i=0;i<5;i++) {
        ASSERT(__ctOS_stream_getc(&stream));
        ASSERT(stream.buffer[i]==('a'+i));
    }
    return 0;
}

/*
 * Testcase 19: set a stream to line buffered and write a character followed by a newline
 * Verify that data is written
 */
int testcase19() {
    __ctOS_stream_t stream;
    int i;
    filpos = 0;
    read_called = 0;
    write_called = 0;
    setup_testfile();
    ASSERT(0==__ctOS_stream_open(&stream, 9));
    ASSERT(0==__ctOS_stream_setvbuf(&stream, 0,_IOLBF, 0 ));
    ASSERT('z'==__ctOS_stream_putc(&stream, 'z'));
    ASSERT(0==write_called);
    ASSERT('\n'==__ctOS_stream_putc(&stream, '\n'));
    ASSERT(1==write_called);
    ASSERT(test_file[0]=='z');
    ASSERT(test_file[1]=='\n');
    return 0;
}

/*
 * Testcase 20: set a stream to line buffered and write a character followed by a newline followed by another character
 * Verify that data is written once - after the first newline
 */
int testcase20() {
    __ctOS_stream_t stream;
    int i;
    filpos = 0;
    read_called = 0;
    write_called = 0;
    setup_testfile();
    ASSERT(0==__ctOS_stream_open(&stream, 9));
    ASSERT(0==__ctOS_stream_setvbuf(&stream, 0,_IOLBF, 0 ));
    ASSERT('z'==__ctOS_stream_putc(&stream, 'z'));
    ASSERT(0==write_called);
    ASSERT('\n'==__ctOS_stream_putc(&stream, '\n'));
    ASSERT(1==write_called);
    ASSERT(test_file[0]=='z');
    ASSERT(test_file[1]=='\n');
    write_called = 0;
    ASSERT('t'==__ctOS_stream_putc(&stream, 't'));
    ASSERT(0==write_called);
    ASSERT(stream.buffer[0]=='t');
    return 0;
}

/*
 * Testcase 21: set a stream to line buffered, then write into it until the buffer is full. Put an additional newline
 * into the buffer - this should involve two flush operations
 */
int testcase21() {
    __ctOS_stream_t stream;
    int i;
    filpos = 0;
    read_called = 0;
    write_called = 0;
    setup_testfile();
    ASSERT(0==__ctOS_stream_open(&stream, 9));
    stream.buf_size = 10;
    ASSERT(0==__ctOS_stream_setvbuf(&stream, 0,_IOLBF, 0 ));
    for (i=0;i<10;i++) {
        ASSERT('t'==__ctOS_stream_putc(&stream, 't'));
    }
    ASSERT(0==write_called);
    ASSERT('\n'==__ctOS_stream_putc(&stream, '\n'));
    ASSERT(2==write_called);
    for (i=0;i<10;i++) {
        ASSERT('t'==test_file[i]);
    }
    ASSERT('\n'==test_file[10]);
    return 0;
}

/*
 * Testcase 22: close a buffered stream and make sure that its contents are flushed
 */
int testcase22() {
    __ctOS_stream_t stream;
    int i;
    filpos = 0;
    read_called = 0;
    write_called = 0;
    setup_testfile();
    ASSERT(0==__ctOS_stream_open(&stream, 9));
    stream.buf_size = 10;
    ASSERT('z'==__ctOS_stream_putc(&stream, 'z'));
    ASSERT(0==write_called);
    ASSERT(0==__ctOS_stream_close(&stream));
    ASSERT(1==write_called);
    ASSERT('z'==test_file[0]);
    return 0;
}

/*
 * Testcase 23: close a buffered stream which has never been used
 */
int testcase23() {
    __ctOS_stream_t stream;
    int i;
    filpos = 0;
    read_called = 0;
    write_called = 0;
    setup_testfile();
    ASSERT(0==__ctOS_stream_open(&stream, 9));
    ASSERT(0==__ctOS_stream_close(&stream));
    ASSERT(0==write_called);
    ASSERT(0==read_called);
    return 0;
}


/*
 * Testcase 24: close an unbuffered stream
 */
int testcase24() {
    __ctOS_stream_t stream;
    int i;
    filpos = 0;
    read_called = 0;
    write_called = 0;
    setup_testfile();
    ASSERT(0==__ctOS_stream_open(&stream, 9));
    stream.buf_size = 10;
    __ctOS_stream_setvbuf(&stream, 0, _IONBF, 0);
    ASSERT('z'==__ctOS_stream_putc(&stream, 'z'));
    ASSERT('z'==test_file[0]);
    ASSERT(1==write_called);
    ASSERT(0==__ctOS_stream_close(&stream));
    ASSERT(1==write_called);
    ASSERT('z'==test_file[0]);
    return 0;
}

/*
 * Testcase 25: use setvbuf to force a stream to point to a custom buffer. Then close stream again
 */
int testcase25() {
    __ctOS_stream_t stream;
    char mybuffer[5];
    int i;
    filpos = 0;
    read_called = 0;
    write_called = 0;
    setup_testfile();
    ASSERT(0==__ctOS_stream_open(&stream, 9));
    ASSERT(0==__ctOS_stream_setvbuf(&stream, mybuffer,_IOFBF, 5 ));
    ASSERT(0==__ctOS_stream_close(&stream));
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
    RUN_CASE(16);
    RUN_CASE(17);
    RUN_CASE(19);
    RUN_CASE(20);
    RUN_CASE(21);
    RUN_CASE(22);
    RUN_CASE(23);
    RUN_CASE(24);
    RUN_CASE(25);
    END;
}
