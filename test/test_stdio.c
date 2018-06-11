/*
 * test_stdio.c
 *
 * NOTE: this test suite assumes presence of a file called "hello" in the current directory which contains
 * the string "Hello World!"
 */

#include "stdio.h"
#include "unistd.h"
#include "limits.h"
#include "stdint.h"
#include "vga.h"
#include "errno.h"

extern void kprintf(char* template, ...);
extern int do_test_case(int x, int(*testcase)());
// extern int __scanf_loglevel;

char** environ;

int __ctOS_rename(char* old, char* new) {
    return -1;
}

int __ctOS_link(const char *path1, const char *path2) {
    return -ENOENT;
}

int __ctOS_ftruncate(int fd, off_t size) {
    return -1;
}

int __ctOS_openat(int dirfd, char* path, int flags, int mode) {
    return -1;
}

int __ctOS_fchdir(int fd) {
    return -1;
}

/*
 * Implementation of kputchar so that we can use kprintf
 */
void win_putchar(win_t* win, u8 c) {
    write(1, &c, 1);
}

/*
 * Macros from kunit.h, adapted for use with kprintf
 */
/*
 * Macro for assertions in unit test cases
 */
#define ASSERT(x)  do { if (!(x)) { \
                          kprintf("Assertion %s failed at line %d in %s..", #x, __LINE__, __FILE__ ); \
                          return 1 ;   \
                        } \
                   } while (0)

/*
 * Set up statistics
 */
#define INIT  int __failed=0; int __passed=0; int __rc=0 ; \
              kprintf("------------------------------------------\n"); \
              kprintf("Starting unit test %s\n", __FILE__); \
              kprintf("------------------------------------------\n");

/*
 * Print statistic and return
 */
#define END kprintf("------------------------------------------\n"); \
            kprintf("Overall test results (%s):\n", __FILE__); \
            kprintf("------------------------------------------\n"); \
            kprintf("Failed: %d  Passed:  %d\n", __failed, __passed); \
            kprintf("------------------------------------------\n"); \
            _exit(__failed);

/* Run a test case
 */
int do_test_case(int x, int (*testcase)()) {
    int rc;
    kprintf("Running testcase %d...", x);
    rc = testcase();
    if (0==rc)
        kprintf("ok\n");
    else
        kprintf("failure\n");
    return rc;
}


/*
 * Execute a test case
 */
#define RUN_CASE(x) do { __rc= do_test_case(x, testcase##x);  \
                         if (__rc) __failed++; else __passed++;} while (0)



/*
 * Testcase 1: fopen a file
 */
int testcase1() {
    ASSERT(fopen("hello", "r+"));
    return 0;
}

/*
 * Testcase 2: fclose a file
 */
int testcase2() {
    FILE* file;
    file = fopen("hello", "r+");
    ASSERT(file);
    ASSERT(0==fclose(file));
    return 0;
}

/*
 * Testcase 3: feof - test case that file has not reached EOF
 */
int testcase3() {
    FILE* file;
    file = fopen("hello", "r+");
    ASSERT(file);
    ASSERT(0==feof(file));
    ASSERT(0==fclose(file));
    return 0;
}

/*
 * Testcase 4: fgetc
 */
int testcase4() {
    FILE* file;
    file = fopen("hello", "r");
    ASSERT(file);
    ASSERT('H'==fgetc(file));
    ASSERT(0==fclose(file));
    return 0;
}

/*
 * Testcase 5: feof - test case that file has reached EOF
 */
int testcase5() {
    int c = 0;
    FILE* file;
    file = fopen("hello", "r+");
    ASSERT(file);
    while (c != EOF) {
        c = fgetc(file);
    }
    ASSERT(feof(file));
    ASSERT(0==fclose(file));
    return 0;
}

/*
 * Testcase 6: ferror
 */
int testcase6() {
    int c = 0;
    FILE* file;
    file = fopen("hello", "r+");
    ASSERT(file);
    c = fgetc(file);
    ASSERT(c=='H');
    ASSERT(0==ferror(file));
    ASSERT(0==fclose(file));
    return 0;
}

/*
 * Testcase 7: fputc and fflush
 * In this testcase, we write one character to a file and flush. We then open the file again to
 * check that the character has been written
 * NOTE: this testcase will create a new file "dummy" which is removed by a later testcase!
 */
int testcase7() {
    int c = 0;
    FILE* file;
    FILE* check;
    file = fopen("dummy", "w+");
    ASSERT(file);
    ASSERT('x'==fputc('x', file));
    ASSERT(0==fflush(file));
    check = fopen("dummy", "r");
    ASSERT(check);
    ASSERT('x'==fgetc(check));
    ASSERT(0==fclose(check));
    ASSERT(0==fclose(file));
    return 0;
}

/*
 * Testcase 8: fseek
 * Read two characters from file hello, then reposition at beginning of file
 */
int testcase8() {
    int c = 0;
    FILE* file;
    file = fopen("hello", "r");
    ASSERT(file);
    ASSERT('H'==fgetc(file));
    ASSERT('e'==fgetc(file));
    ASSERT(0==fseek(file, 0, SEEK_SET));
    ASSERT('H'==fgetc(file));
    ASSERT(0==fclose(file));
    return 0;
}

/*
 * Testcase 9: ftell
 * Check that calling ftell on a file immediately after opening it returns 0
 */
int testcase9() {
    int c = 0;
    FILE* file;
    file = fopen("hello", "r");
    ASSERT(file);
    ASSERT(0==ftell(file));
    ASSERT(0==fclose(file));
    return 0;
}

/*
 * Testcase 10: ftell
 * Check that calling ftell on a file after reading one character returns 1
 */
int testcase10() {
    int c = 0;
    FILE* file;
    file = fopen("hello", "r");
    ASSERT(file);
    ASSERT(0==ftell(file));
    ASSERT('H'==fgetc(file));
    ASSERT(1==ftell(file));
    ASSERT(0==fclose(file));
    return 0;
}

/*
 * Testcase 11: ftell
 * Use ftell to re-read
 */
int testcase11() {
    int c = 0;
    FILE* file;
    int pos;
    file = fopen("hello", "r");
    ASSERT(file);
    ASSERT('H'==fgetc(file));
    pos = ftell(file);
    ASSERT('e'==fgetc(file));
    ASSERT(0==fseek(file, pos, SEEK_SET));
    ASSERT('e'==fgetc(file));
    ASSERT(0==fclose(file));
    return 0;
}

/*
 * Testcase 12: ftell
 * Verify that after writing one character, ftell returns one
 */
int testcase12() {
    int c = 0;
    FILE* file;
    int pos;
    file = fopen("dummy", "w+");
    ASSERT(file);
    ASSERT(fputc('d', file));
    ASSERT(1==ftell(file));
    ASSERT(0==fclose(file));
    return 0;
}

/*
 * Testcase 13: ftell
 * Verify that flushing an output stream does not change the file position
 */
int testcase13() {
    int c = 0;
    FILE* file;
    int pos;
    file = fopen("dummy", "w+");
    ASSERT(file);
    ASSERT(fputc('d', file));
    ASSERT(1==ftell(file));
    ASSERT(0==fflush(file));
    ASSERT(1==ftell(file));
    ASSERT(0==fclose(file));
    return 0;
}

/*
 * Testcase 14: fseek
 * Use ftell to write to byte 5 of a file
 */
int testcase14() {
    int i;
    int c = 0;
    FILE* file;
    int pos;
    file = fopen("dummy", "w+");
    ASSERT(file);
    for (i = 0; i < 10; i++)
        fputc('a', file);
    ASSERT(0==fseek(file, 5, SEEK_SET));
    ASSERT(fputc('x', file));
    ASSERT(0==fclose(file));
    file = fopen("dummy", "r");
    ASSERT(file);
    ASSERT(0==fseek(file, 5, SEEK_SET));
    ASSERT('x'==fgetc(file));
    ASSERT(0==fclose(file));
    return 0;
}

/*
 * Testcase 15: fgets
 * Use fgets to get "Hello World!\n" from the file hello.
 */
int testcase15() {
    FILE* file;
    char buffer[256];
    memset(buffer, 0, 256);
    file = fopen("hello", "r");
    ASSERT(file);
    ASSERT(buffer==fgets(buffer, 256, file));
    ASSERT(0==strcmp(buffer, "Hello World!\n"));
    ASSERT(0==fclose(file));
    return 0;
}

/*
 * Testcase 16: fgets
 * Use fgets to get "aaaaaxaaaa" from the file dummy written above. We read until we hit upon the EOF.
 * Note that fgets returns NULL only when no character has been read at all, so we expect the return value to be
 * the buffer even in this case
 */
int testcase16() {
    FILE* file;
    char* rc;
    char buffer[256];
    memset(buffer, 0, 256);
    /*
     * Set "guard"
     */
    buffer[11] = 0xf;
    file = fopen("dummy", "r");
    ASSERT(file);
    rc = fgets(buffer, 256, file);
    ASSERT(buffer==rc);
    ASSERT(0==strcmp(buffer, "aaaaaxaaaa"));
    ASSERT(0==fclose(file));
    ASSERT(0x0==buffer[10]);
    ASSERT(0xf==buffer[11]);
    return 0;
}

/*
 * Testcase 17: fgets
 * Use fgets to get "aa" from the file dummy written above. We read until we have read 2 bytes
 */
int testcase17() {
    FILE* file;
    char buffer[256];
    memset(buffer, 0, 256);
    /*
     * Set "guard"
     */
    buffer[3] = 0xf;
    file = fopen("dummy", "r");
    ASSERT(file);
    ASSERT(buffer==fgets(buffer, 3, file));
    ASSERT(0==strcmp(buffer, "aa"));
    ASSERT(0==fclose(file));
    ASSERT(0x0==buffer[2]);
    ASSERT(0xf==buffer[3]);
    return 0;
}

/*
 * Testcase 18: remove file again and verify that it cannot be opened again for reading
 */
int testcase18() {
    int rc = remove("dummy");
    ASSERT(0 == rc);
    ASSERT(0==fopen("dummy", "r"));
    return 0;
}

/*
 * Testcase 19: fputs
 * Write a string to a new file using fputs, read it using fgets and compare the results. Then remove the file again
 */
int testcase19() {
    char buffer[256];
    FILE* file = fopen("dummy", "w+");
    ASSERT(file);
    ASSERT(fputs("test", file));
    ASSERT(0==fclose(file));
    file = fopen("dummy", "r");
    ASSERT(file);
    fgets(buffer, 5, file);
    ASSERT(0==strcmp("test", buffer));
    ASSERT(0==fclose(file));
    ASSERT(0==remove("dummy"));
    return 0;
}

/*
 * Testcase 20: fread
 * Read 5 items of size 1 from file hello
 */
int testcase20() {
    unsigned char buffer[256];
    FILE* file = fopen("hello", "r");
    ASSERT(file);
    memset(buffer, 0, 256);
    ASSERT(5==fread(buffer, 1, 5, file));
    ASSERT('H'==buffer[0]);
    ASSERT('o'==buffer[4]);
    fclose(file);
    return 0;
}

/*
 * Testcase 21: fread
 * Read 2 items of size 2 from file hello
 */
int testcase21() {
    unsigned char buffer[256];
    FILE* file = fopen("hello", "r");
    ASSERT(file);
    memset(buffer, 0, 256);
    ASSERT(2==fread(buffer, 2, 2, file));
    ASSERT('H'==buffer[0]);
    ASSERT('l'==buffer[3]);
    fclose(file);
    return 0;
}

/*
 * Testcase 22: freopen.
 * Open a stream for reading first. Then re-open with a different filename for writing and
 * check that a write is successful
 */
int testcase22() {
    FILE* file = fopen("hello", "r");
    ASSERT(file);
    ASSERT(file==freopen("dummy", "w+", file));
    fputc('a', file);
    ASSERT(0==fclose(file));
    file = fopen("dummy", "r");
    ASSERT(file);
    ASSERT('a'==fgetc(file));
    ASSERT(0==fclose(file));
    remove("dummy");
    return 0;
}

/*
 * Testcase 23: fwrite.
 * Write two integers using fwrite and read them again
 */
int testcase23() {
    FILE* file;
    unsigned int test[2];
    ASSERT(file=fopen("dummy", "w+"));
    test[0] = 0x11223344;
    test[1] = 0xaabbccdd;
    ASSERT(2==fwrite((void*) test, sizeof(unsigned int), 2, file));
    test[0] = 0;
    test[1] = 0;
    ASSERT(0==fseek(file, 0, SEEK_SET));
    ASSERT(2==fread( (void*) test, sizeof(unsigned int), 2, file));
    ASSERT(test[0] == 0x11223344);
    ASSERT(test[1] == 0xaabbccdd);
    ASSERT(0==fclose(file));
    remove("dummy");
    return 0;
}

/*
 * Testcase 24: snprintf
 * Print a string without any format specifiers. As we limit the size of the buffer to 3 and need
 * one byte for the trailing zero, we expect that only two bytes are written, but the result is the number
 * of bytes that would have been written, i.e. 3
 */
int testcase24() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(3==snprintf(buffer, 3, "abc"));
    ASSERT(buffer[2]==0);
    return 0;
}

/*
 * Testcase 25: snprintf
 * Print a string without any format specifiers.
 */
int testcase25() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(3==snprintf(buffer, 256, "asc"));
    ASSERT(buffer[3]==0);
    ASSERT(0==strcmp(buffer, "asc"));
    return 0;
}

/*
 * Testcase 26: snprintf
 * Print a string with format specifier %s
 */
int testcase26() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(3==snprintf(buffer, 256, "%s", "abc"));
    ASSERT(buffer[3]==0);
    ASSERT(0==strcmp(buffer, "abc"));
    return 0;
}

/*
 * Testcase 27: snprintf
 * Print a string with format specifier %s, mixed with literal data
 */
int testcase27() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(5==snprintf(buffer, 256, "x%sx", "abc"));
    ASSERT(buffer[5]==0);
    ASSERT(0==strcmp(buffer, "xabcx"));
    return 0;
}

/*
 * Testcase 28: snprintf
 * Print a string with format specifier %s and a precision - precision greater than string length
 */
int testcase28() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(3==snprintf(buffer, 256, "%.5s", "abc"));
    ASSERT(buffer[3]==0);
    ASSERT(0==strcmp(buffer, "abc"));
    return 0;
}

/*
 * Testcase 29: snprintf
 * Print a string with format specifier %s and a precision - precision less than string length
 */
int testcase29() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(2==snprintf(buffer, 256, "%.2s", "abc"));
    ASSERT(buffer[2]==0);
    ASSERT(0==strcmp(buffer, "ab"));
    return 0;
}

/*
 * Testcase 30: snprintf
 * Print a string with format specifier %s and a precision - precision equal to string length
 */
int testcase30() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(3==snprintf(buffer, 256, "%.3s", "abc"));
    ASSERT(buffer[3]==0);
    ASSERT(0==strcmp(buffer, "abc"));
    return 0;
}

/*
 * Testcase 31: snprintf
 * Print an integer value with two digits
 */
int testcase31() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(2==snprintf(buffer, 256, "%d", 12));
    ASSERT(buffer[2]==0);
    ASSERT(0==strcmp(buffer, "12"));
    return 0;
}

/*
 * Testcase 32: snprintf
 * Print an integer value with one digit
 */
int testcase32() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(1==snprintf(buffer, 256, "%d", 1));
    ASSERT(buffer[1]==0);
    ASSERT(0==strcmp(buffer, "1"));
    return 0;
}


/*
 * Testcase 33: snprintf
 * Print zero
 */
int testcase33() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(1==snprintf(buffer, 256, "%d", 0));
    ASSERT(buffer[1]==0);
    ASSERT(0==strcmp(buffer, "0"));
    return 0;
}

/*
 * Testcase 34: snprintf
 * Print a negative integer
 */
int testcase34() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(4==snprintf(buffer, 256, "%d", -123));
    ASSERT(buffer[4]==0);
    ASSERT(0==strcmp(buffer, "-123"));
    return 0;
}

/*
 * Testcase 35: snprintf
 * Print a two-digit integer with precision 3
 */
int testcase35() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(3==snprintf(buffer, 256, "%.3d", 12));
    ASSERT(buffer[3]==0);
    ASSERT(0==strcmp(buffer, "012"));
    return 0;
}

/*
 * Testcase 36: snprintf
 * Print a two-digit integer with precision 3 and width 4 --> " 012"
 */
int testcase36() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(4==snprintf(buffer, 256, "%4.3zd", 12));
    ASSERT(buffer[4]==0);
    ASSERT(0==strcmp(buffer, " 012"));
    return 0;
}

/*
 * Testcase 37: snprintf
 * Print zero with precision of zero
 */
int testcase37() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(0==snprintf(buffer, 256, "%.0d", 0));
    ASSERT(buffer[0]==0);
    return 0;
}

/*
 * Testcase 38: snprintf
 * Print a two-digit integer with precision 1
 */
int testcase38() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(2==snprintf(buffer, 256, "%.1d", 12));
    ASSERT(buffer[2]==0);
    ASSERT(0==strcmp(buffer, "12"));
    return 0;
}

/*
 * Testcase 39: snprintf
 * Print a one-digit integer with precision 1
 */
int testcase39() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(1==snprintf(buffer, 256, "%.1d", 1));
    ASSERT(buffer[1]==0);
    ASSERT(0==strcmp(buffer, "1"));
    return 0;
}

/*
 * Testcase 40: snprintf
 * Print a one-digit integer with precision 0
 */
int testcase40() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(1==snprintf(buffer, 256, "%.0d", 1));
    ASSERT(buffer[1]==0);
    ASSERT(0==strcmp(buffer, "1"));
    return 0;
}

/*
 * Testcase 41: snprintf
 * Print an unsigned int
 */
int testcase41() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(strlen("4294967295")==snprintf(buffer, 256, "%u", 0xffffffff));
    ASSERT(buffer[10]==0);
    ASSERT(0==strcmp(buffer, "4294967295"));
    return 0;
}

/*
 * Testcase 42: snprintf
 * Print an unsigned int which is a power of 8 as octal - two digits
 */
int testcase42() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(2==snprintf(buffer, 256, "%o", 8));
    ASSERT(buffer[2]==0);
    ASSERT(0==strcmp(buffer, "10"));
    return 0;
}

/*
 * Testcase 43: snprintf
 * Print value zero with precision 2
 */
int testcase43() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(2==snprintf(buffer, 256, "%.2i", 0));
    ASSERT(buffer[2]==0);
    ASSERT(0==strcmp(buffer, "00"));
    return 0;
}

/*
 * Testcase 44: snprintf
 * Print an unsigned int which is a power of 8 as octal - three digits
 */
int testcase44() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(3==snprintf(buffer, 256, "%o", 0100));
    ASSERT(buffer[3]==0);
    ASSERT(0==strcmp(buffer, "100"));
    return 0;
}

/*
 * Testcase 45: snprintf
 * Print an unsigned int which is not a power of 8 as octal - three digits
 */
int testcase45() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(3==snprintf(buffer, 256, "%o", 0123));
    ASSERT(buffer[3]==0);
    ASSERT(0==strcmp(buffer, "123"));
    return 0;
}

/*
 * Testcase 46: snprintf
 * Print an unsigned int which is not a power of 16 as hex - three digits
 */
int testcase46() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(3==snprintf(buffer, 256, "%x", 0x123));
    ASSERT(buffer[3]==0);
    ASSERT(0==strcmp(buffer, "123"));
    return 0;
}

/*
 * Testcase 47: snprintf
 * Print an unsigned int which is not a power of 16 and contains letters as hex - three digits
 */
int testcase47() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(3==snprintf(buffer, 256, "%x", 0xabc));
    ASSERT(buffer[3]==0);
    ASSERT(0==strcmp(buffer, "abc"));
    return 0;
}

/*
 * Testcase 48: snprintf
 * Print an unsigned int which is not a power of 16 and contains letters as hex - three digits
 */
int testcase48() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(3==snprintf(buffer, 256, "%X", 0xa12));
    ASSERT(buffer[3]==0);
    ASSERT(0==strcmp(buffer, "A12"));
    return 0;
}

/*
 * Testcase 49: snprintf
 * Print a character
 */
int testcase49() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(1==snprintf(buffer, 256, "%c", 't'));
    ASSERT(buffer[1]==0);
    ASSERT(0==strcmp(buffer, "t"));
    return 0;
}

/*
 * Testcase 50: snprintf
 * Print a mixture of characters, numbers and a fixed value string
 */
int testcase50() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(6==snprintf(buffer, 256, "%cXXX%d", 't',12));
    ASSERT(buffer[6]==0);
    ASSERT(0==strcmp(buffer, "tXXX12"));
    return 0;
}

/*
 * Testcase 51: snprintf
 * Print a pointer
 */
int testcase51() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(5==snprintf(buffer, 256, "%p", (void*) 0x123));
    ASSERT(buffer[5]==0);
    ASSERT(0==strcmp(buffer, "0x123"));
    return 0;
}

/*
 * Testcase 52: snprintf
 * Print a pointer
 */
int testcase52() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(10==snprintf(buffer, 256, "%p", (void*) 0xabcdffff));
    ASSERT(buffer[10]==0);
    ASSERT(0==strcmp(buffer, "0xabcdffff"));
    return 0;
}

/*
 * Testcase 53: snprintf
 * Print using the n conversion specifier
 */
int testcase53() {
    int count;
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(3==snprintf(buffer, 256, "a%nbc", &count));
    ASSERT(buffer[3]==0);
    ASSERT(0==strcmp(buffer, "abc"));
    ASSERT(count==1);
    return 0;
}

/*
 * Testcase 54: snprintf
 * Print using the % conversion specifier
 */
int testcase54() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(3==snprintf(buffer, 256, "a%%d"));
    ASSERT(buffer[3]==0);
    ASSERT(0==strcmp(buffer, "a%d"));
    return 0;
}

/*
 * Testcase 55: sprintf
 */
int testcase55() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(5==sprintf(buffer,  "abc%d", 55));
    ASSERT(buffer[5]==0);
    ASSERT(0==strcmp(buffer, "abc55"));
    return 0;
}

/*
 * Testcase 56: sprintf
 * Print INT32_MAX as signed integer
 */
int testcase56() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(10==sprintf(buffer,  "%d", INT32_MAX));
    ASSERT(buffer[10]==0);
    ASSERT(0==strcmp(buffer, "2147483647"));
    return 0;
}

/*
 * Testcase 57: sscanf
 * Scan a string consisting of ordinary characters only which match the template
 */
int testcase57() {
    char buffer[256];
    strncpy(buffer, "abcde", 5);
    buffer[5]=0;
    ASSERT(0==sscanf(buffer, "abcde"));
    return 0;
}

/*
 * Testcase 58: sscanf
 * Test usage of whitespace directive - directive matches one space in input
 */
int testcase58() {
    char buffer[256];
    strncpy(buffer, "ab de", 5);
    buffer[5]=0;
    ASSERT(0==sscanf(buffer, "ab de"));
    return 0;
}

/*
 * Testcase 59: sscanf
 * Test usage of whitespace directive - directive matches two spaces in input
 */
int testcase59() {
    char buffer[256];
    strncpy(buffer, "ab  e", 5);
    buffer[5]=0;
    ASSERT(0==sscanf(buffer, "ab e"));
    return 0;
}

/*
 * Testcase 60: sscanf
 * Test usage of whitespace directive - directive consists of two spaces and matches one space in input
 */
int testcase60() {
    char buffer[256];
    strncpy(buffer, "ab de", 5);
    buffer[5]=0;
    ASSERT(0==sscanf(buffer, "ab  de"));
    return 0;
}

/*
 * Testcase 61: sscanf
 * Test usage of whitespace directive - directive consists of one tab and matches two white-space characters in input
 */
int testcase61() {
    char buffer[256];
    strncpy(buffer, "ab\v\te", 5);
    buffer[5]=0;
    ASSERT(0==sscanf(buffer, "ab\te"));
    return 0;
}

/*
 * Testcase 62: sscanf
 * Test scanning of an integer value
 */
int testcase62() {
    int res;
    char buffer[256];
    strncpy(buffer, "12", 2);
    buffer[2]=0;
    ASSERT(1==sscanf(buffer, "%d", &res));
    ASSERT(12==res);
    return 0;
}

/*
 * Testcase 63: sscanf
 * Test scanning of an integer value with leading white space
 */
int testcase63() {
    int res;
    char buffer[256];
    strncpy(buffer, " 12", 3);
    buffer[3]=0;
    ASSERT(1==sscanf(buffer, "%d", &res));
    ASSERT(12==res);
    return 0;
}

/*
 * Testcase 64: sscanf
 * Test scanning of an integer value with trailing white space
 */
int testcase64() {
    int res;
    char buffer[256];
    strncpy(buffer, "10 ", 3);
    buffer[3]=0;
    ASSERT(1==sscanf(buffer, "%d", &res));
    ASSERT(10==res);
    return 0;
}

/*
 * Testcase 65: sscanf
 * Test scanning of a signed integer value
 */
int testcase65() {
    int res;
    char buffer[256];
    strncpy(buffer, "-11", 3);
    buffer[3]=0;
    ASSERT(1==sscanf(buffer, "%d", &res));
    ASSERT(-11==res);
    return 0;
}

/*
 * Testcase 66: sscanf
 * Test scanning of a signed integer value - overflow
 */
int testcase66() {
    int res;
    char buffer[256];
    strncpy(buffer, "11111111111", 11);
    buffer[11]=0;
    ASSERT(1==sscanf(buffer, "%d", &res));
    return 0;
}

/*
 * Testcase 67: sscanf
 * Test scanning of a signed integer value with defined width
 */
int testcase67() {
    int res;
    char buffer[256];
    strncpy(buffer, "-11", 3);
    buffer[3]=0;
    ASSERT(1==sscanf(buffer, "%2d", &res));
    ASSERT(-1==res);
    return 0;
}

/*
 * Testcase 68: sscanf
 * Test scanning of an unsigned integer value with defined width
 */
int testcase68() {
    int res;
    char buffer[256];
    strncpy(buffer, "1234", 4);
    buffer[4]=0;
    ASSERT(1==sscanf(buffer, "%2d", &res));
    ASSERT(12==res);
    return 0;
}

/*
 * Testcase 69: sscanf
 * Test scanning of an unsigned octal integer
 */
int testcase69() {
    int res;
    char buffer[256];
    strncpy(buffer, "12", 3);
    buffer[3]=0;
    ASSERT(1==sscanf(buffer, "%o", &res));
    ASSERT(10==res);
    return 0;
}

/*
 * Testcase 70: sscanf
 * Test scanning of an unsigned hexadecimal integer
 */
int testcase70() {
    unsigned int res;
    char buffer[256];
    strncpy(buffer, "ff", 2);
    buffer[2]=0;
    ASSERT(1==sscanf(buffer, "%x", &res));
    ASSERT(255==res);
    return 0;
}

/*
 * Testcase 71: sscanf
 * Test scanning of an unsigned hexadecimal integer - maximum value
 */
int testcase71() {
    unsigned int res;
    char buffer[256];
    strncpy(buffer, "ffffffff", 8);
    buffer[8]=0;
    ASSERT(1==sscanf(buffer, "%x", &res));
    ASSERT(0xffffffff==res);
    return 0;
}

/*
 * Testcase 72: sscanf
 * Test scanning of a signed hexadecimal integer
 */
int testcase72() {
    int res;
    char buffer[256];
    strncpy(buffer, "-ff", 3);
    buffer[3]=0;
    ASSERT(1==sscanf(buffer, "%x", &res));
    ASSERT(0xff*(-1)==res);
    return 0;
}

/*
 * Testcase 73: sscanf
 * Test scanning of characters with default width 1 - additional input left
 */
int testcase73() {
    char res;
    char buffer[256];
    strncpy(buffer, "ab", 2);
    buffer[2]=0;
    ASSERT(1==sscanf(buffer, "%c", &res));
    ASSERT('a'==res);
    return 0;
}

/*
 * Testcase 74: sscanf
 * Test scanning of characters with default width 1 - no additional input left
 */
int testcase74() {
    char res;
    char buffer[256];
    strncpy(buffer, "b", 1);
    buffer[1]=0;
    ASSERT(1==sscanf(buffer, "%c", &res));
    ASSERT('b'==res);
    return 0;
}

/*
 * Testcase 75: sscanf
 * Test scanning of characters with width 2
 */
int testcase75() {
    char check[256];
    memset(check, 1, 256);
    char buffer[256];
    strncpy(buffer, "ab", 2);
    buffer[2]=0;
    ASSERT(1==sscanf(buffer, "%2c", check));
    ASSERT(check[2]==1);
    ASSERT(0==strncmp("ab", check, 2));
    return 0;
}

/*
 * Testcase 76: sscanf
 * Test scanning of characters with width 3 and white space
 */
int testcase76() {
    char check[256];
    memset(check, 1, 256);
    char buffer[256];
    strncpy(buffer, "a b", 3);
    buffer[3]=0;
    ASSERT(1==sscanf(buffer, "%3c", check));
    ASSERT(check[3]==1);
    ASSERT(0==strncmp("a b", check, 3));
    return 0;
}

/*
 * Testcase 77: sscanf
 * Test scanning of a string
 */
int testcase77() {
    char check[256];
    memset(check, 1, 256);
    char buffer[256];
    strncpy(buffer, "ab ", 3);
    buffer[3]=0;
    ASSERT(1==sscanf(buffer, "%s", check));
    ASSERT(check[2]==0);
    ASSERT(0==strcmp("ab", check));
    return 0;
}

/*
 * Testcase 78: sscanf
 * Test scanning of a string - stop at whitespace
 */
int testcase78() {
    char check[256];
    memset(check, 1, 256);
    char buffer[256];
    strncpy(buffer, "a b", 3);
    buffer[3]=0;
    ASSERT(1==sscanf(buffer, "%s", check));
    ASSERT(check[1]==0);
    ASSERT(0==strcmp("a", check));
    return 0;
}

/*
 * Testcase 79: sscanf
 * Test scanning of a %
 */
int testcase79() {
    char buffer[256];
    strncpy(buffer, "%", 1);
    buffer[1]=0;
    ASSERT(0==sscanf(buffer, "%%"));
    return 0;
}

/*
 * Testcase 80: sscanf
 * Test scanning of a %n - ordinary string read so far only
 */
int testcase80() {
    int count;
    char buffer[256];
    strncpy(buffer, "abc", 3);
    buffer[3]=0;
    ASSERT(0==sscanf(buffer, "ab%n", &count));
    ASSERT(2==count);
    return 0;
}

/*
 * Testcase 81: sscanf
 * Test abort of scan due to insufficient input - at least one conversion done
 */
int testcase81() {
    int res;
    int dummy = 0;
    char buffer[256];
    strncpy(buffer, "111", 3);
    buffer[3]=0;
    ASSERT(1==sscanf(buffer, "%d%d", &res, &dummy));
    ASSERT(111==res);
    ASSERT(0==dummy);
    return 0;
}

/*
 * Testcase 82: sscanf
 * Simulate terminal input and handling of trailing newline
 */
int testcase82() {
    int res;
    char buffer[256];
    strncpy(buffer, "11\n", 3);
    buffer[3]=0;
    ASSERT(1==sscanf(buffer, "%d\n", &res));
    ASSERT(11==res);
    return 0;
}

/*
 * Testcase 83: sscanf
 * Simulate terminal input and handling of trailing newline
 */
int testcase83() {
    int res;
    char buffer[256];
    strncpy(buffer, "11\n", 3);
    buffer[3]=0;
    ASSERT(1==sscanf(buffer, "%d ", &res));
    ASSERT(11==res);
    return 0;
}

/*
 * Testcase 84: sprintf
 * Print an integer containing a zero at a middle position
 */
int testcase84() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(3==snprintf(buffer, 256, "%d", 101));
    ASSERT(buffer[3]==0);
    ASSERT(0==strcmp(buffer, "101"));
    return 0;
}

/*
 * Testcase 85: sprintf
 * Print an integer containing a zero at the end
 */
int testcase85() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(3==snprintf(buffer, 256, "%d", 110));
    ASSERT(buffer[3]==0);
    ASSERT(0==strcmp(buffer, "110"));
    return 0;
}

/*
 * Testcase 86: sprintf
 * Print an integer containing a zero at the end and somewhere in the middle
 */
int testcase86() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(4==snprintf(buffer, 256, "%d", 1010));
    ASSERT(buffer[4]==0);
    ASSERT(0==strcmp(buffer, "1010"));
    return 0;
}

/*
 * Testcase 87: snprintf
 * Print a string with format specifier %s and a dynamic precision - precision less than string length
 */
int testcase87() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(2==snprintf(buffer, 256, "%.*s", 2, "abc"));
    ASSERT(buffer[2]==0);
    ASSERT(0==strcmp(buffer, "ab"));
    return 0;
}

/*
 * Testcase 88: snprintf
 * Print a two-digit integer with dynamic precision 3 and dynamic width 4 --> " 012"
 */
int testcase88() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(4==snprintf(buffer, 256, "%*.*zd", 4, 3,12));
    ASSERT(buffer[4]==0);
    ASSERT(0==strcmp(buffer, " 012"));
    return 0;
}


/*
 * Testcase 89: snprintf
 * Print a string with format specifier %s, but use size 0
 */
int testcase89() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(3==snprintf(buffer, 0, "%s", "abc"));
    ASSERT(buffer[0]==1);
    return 0;
}

/*
 * Testcase 90: snprintf
 * Print a string with format specifier %s, but use size 0 and a NULL string
 */
int testcase90() {
    ASSERT(3==snprintf(0, 0, "%s", "abc"));
    return 0;
}

/*
 * Testcase 91: snprintf
 * Print an integer value with %ld
 */
int testcase91() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(2==snprintf(buffer, 256, "%ld", (long int) 12));
    ASSERT(buffer[2]==0);
    ASSERT(0==strcmp(buffer, "12"));
    return 0;
}

/*
 * Testcase 92: snprintf
 * Print an integer value with one digit and width 2, should give right justified output
 */
int testcase92() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(2 == snprintf(buffer, 256, "%2d", 1));
    ASSERT(buffer[2] == 0);
    ASSERT(0 == strcmp(buffer, " 1"));
    return 0;
}

/*
 * Testcase 93: snprintf
 * Print zero with width 2, should give right justified output
 */
int testcase93() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(2 == snprintf(buffer, 256, "%2d", 0));
    ASSERT(buffer[2] == 0);
    ASSERT(0 == strcmp(buffer, " 0"));
    return 0;
}

/*
 * Testcase 94: snprintf
 * Print zero with width 2 and flag -, should give left justified output
 */
int testcase94() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(2 == snprintf(buffer, 256, "%-2d", 0));
    ASSERT(buffer[2] == 0);
    ASSERT(0 == strcmp(buffer, "0 "));
    return 0;
}


/*
 * Testcase 95: snprintf
 * Print one-digit integer with width 2 and flag -, should give left justified output
 */
int testcase95() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(2 == snprintf(buffer, 256, "%-2d", 1));
    ASSERT(buffer[2] == 0);
    ASSERT(0 == strcmp(buffer, "1 "));
    return 0;
}

/*
 * Testcase 96: snprintf
 * Print three-character string with width 4, should give right-justified output
 */
int testcase96() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(4 == snprintf(buffer, 256, "%4s","abc"));
    ASSERT(buffer[4] == 0);
    ASSERT(0 == strcmp(buffer, " abc"));
    return 0;
}

/*
 * Testcase 97: snprintf
 * Print three-character string with width 4 and flag -, should give left-justified output
 */
int testcase97() {
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(4 == snprintf(buffer, 256, "%-4s","abc"));
    ASSERT(buffer[4] == 0);
    ASSERT(0 == strcmp(buffer, "abc "));
    return 0;
}

/*
 * Testcase 98: snprintf
 * Print a double with width 5 and dynamic precision 2
 */
int testcase98() {
    int rc;
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(5 == snprintf(buffer, 256, "%5.*f", 2, (double) 12.41));
    ASSERT(0 == strcmp(buffer, "12.41"));
    return 0;
}

/*
 * Testcase 99: snprintf
 * Print 12.41 with width 6 and dynamic precision 2
 */
int testcase99() {
    int rc;
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(6 == snprintf(buffer, 256, "%6.*f", 2, (double) 12.41));
    ASSERT(0 == strcmp(buffer, " 12.41"));
    return 0;
}

/*
 * Testcase 100: snprintf
 * Print 12.41 with precision 1
 */
int testcase100() {
    int rc;
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(4 == snprintf(buffer, 256, "%.1f", (double) 12.41));
    ASSERT(0 == strcmp(buffer, "12.4"));
    return 0;
}

/*
 * Testcase 101: snprintf
 * Print 12.41 with default precision (6)
 */
int testcase101() {
    int rc;
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(9 == snprintf(buffer, 256, "%f", (double) 12.41));
    ASSERT(0 == strcmp(buffer, "12.410000"));
    return 0;
}

/*
 * Testcase 102: snprintf
 * Print -3.141 with default precision (6)
 */
int testcase102() {
    int rc;
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(9 == snprintf(buffer, 256, "%f", (double) -3.141));
    ASSERT(0 == strcmp(buffer, "-3.141000"));
    return 0;
}


/*
 * Testcase 103: snprintf
 * Print 0 with default precision (6)
 */
int testcase103() {
    int rc;
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(8 == snprintf(buffer, 256, "%f", (double) 0.0));
    ASSERT(0 == strcmp(buffer, "0.000000"));
    return 0;
}

/*
 * Testcase 104: snprintf
 * Print 0.5 with precision 2
 */
int testcase104() {
    int rc;
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(4 == snprintf(buffer, 256, "%.2f", (double) 0.5));
    ASSERT(0 == strcmp(buffer, "0.50"));
    return 0;
}

/*
 * Testcase 105: snprintf
 * Print 12.41 with width 6 and dynamic precision 2, left justified
 */
int testcase105() {
    int rc;
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(6 == snprintf(buffer, 256, "%-6.*f", 2, (double) 12.41));
    ASSERT(0 == strcmp(buffer, "12.41 "));
    return 0;
}

/*
 * Testcase 106: snprintf
 * Print inf
 */
int testcase106() {
    int rc;
    char buffer[256];
    double value = 0;
    memset(buffer, 1, 256);
    value = 1.0 / 0.0;
    ASSERT(3 == snprintf(buffer, 256, "%f", value));
    ASSERT(0 == strcmp(buffer, "inf"));
    return 0;
}

/*
 * Testcase 107: snprintf
 * Print -inf
 */
int testcase107() {
    int rc;
    char buffer[256];
    double value = 0;
    memset(buffer, 1, 256);
    value = -1.0 / 0.0;
    ASSERT(4 == snprintf(buffer, 256, "%f", value));
    ASSERT(0 == strcmp(buffer, "-inf"));
    return 0;
}

/*
 * Testcase 108: snprintf
 * Print nan
 */
int testcase108() {
    int rc;
    char buffer[256];
    double value = 0;
    memset(buffer, 1, 256);
    /*
     * 0.0 / 0.0 gives -NaN
     */
    value = +0.0 / +0.0;
    value = -1*value;
    ASSERT(3 == snprintf(buffer, 256, "%f", value));
    ASSERT(0 == strcmp(buffer, "nan"));
    return 0;
}

/*
 * Testcase 109: snprintf
 * Print -nan
 */
int testcase109() {
    int rc;
    char buffer[256];
    double value = 0;
    memset(buffer, 1, 256);
    /*
     * 0.0 / 0.0 gives -NaN
     */
    value = +0.0 / +0.0;
    ASSERT(4 == snprintf(buffer, 256, "%f", value));
    ASSERT(0 == strcmp(buffer, "-nan"));
    return 0;
}

/*
 * Testcase 110: snprintf
 * Print 0.5 with precision 2, use F
 */
int testcase110() {
    int rc;
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(4 == snprintf(buffer, 256, "%.2F", (double) 0.5));
    ASSERT(0 == strcmp(buffer, "0.50"));
    return 0;
}

/*
 * Testcase 111: snprintf
 * Print -NAN
 */
int testcase111() {
    int rc;
    char buffer[256];
    double value = 0;
    memset(buffer, 1, 256);
    /*
     * 0.0 / 0.0 gives -NaN
     */
    value = +0.0 / +0.0;
    ASSERT(4 == snprintf(buffer, 256, "%F", value));
    ASSERT(0 == strcmp(buffer, "-NAN"));
    return 0;
}

/*
 * Testcase 112: snprintf
 * Print -INF
 */
int testcase112() {
    int rc;
    char buffer[256];
    double value = 0;
    memset(buffer, 1, 256);
    value = -1.0 / 0.0;
    ASSERT(4 == snprintf(buffer, 256, "%F", value));
    ASSERT(0 == strcmp(buffer, "-INF"));
    return 0;
}

/*
 * Testcase 113: snprintf
 * Print 1.3 with precision 0 and verify that radix character is suppressed
 */
int testcase113() {
    int rc;
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(1 == snprintf(buffer, 256, "%.0f", (double) 1.3));
    ASSERT(0 == strcmp(buffer, "1"));
    return 0;
}

/*
 * Testcase 114: snprintf
 * Print 1.5 with precision 0 and verify that radix character is suppressed and 2 is printed (rounding)
 */
int testcase114() {
    int rc;
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(1 == snprintf(buffer, 256, "%.0f", (double) 1.5));
    ASSERT(0 == strcmp(buffer, "2"));
    return 0;
}

/*
 * Testcase 115: snprintf
 * Print 1.46 with precision 1 - should give 1.5
 */
int testcase115() {
    int rc;
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(3 == snprintf(buffer, 256, "%.1f", (double) 1.46));
    ASSERT(0 == strcmp(buffer, "1.5"));
    return 0;
}

/*
 * Testcase 116: snprintf
 * Print -1.46 with precision 1 - should give -1.5
 */
int testcase116() {
    int rc;
    char buffer[256];
    memset(buffer, 1, 256);
    ASSERT(4 == snprintf(buffer, 256, "%.1f", (double) -1.46));
    ASSERT(0 == strcmp(buffer, "-1.5"));
    return 0;
}

/*
 * Testcase 117: sscanf
 * Test parsing of a float (f, e, E, g, a)
 * Parse an integer
 */
int testcase117() {
    float x;
    ASSERT(sscanf("1", "%f", &x));
    ASSERT(x == 1.0);
    return 0;
} 

/*
 * Testcase 118: sscanf
 * Test parsing of a float (f, e, E, g, a)
 * Parse a floating point number
 */
int testcase118() {
    float x;
    float error;
    ASSERT(sscanf("1.5", "%f", &x));
    ASSERT(x == 1.5);
    ASSERT(sscanf("3.141", "%f", &x));
    error = 3.141 - x;
    if (error < 0)
        error = -1.0 * error;
    ASSERT(error < 10e-5);
    return 0;
} 

/*
 * Testcase 119: sscanf
 * Test parsing of a float (f, e, E, g, a)
 * Parse a floating point number with a sign
 */
int testcase119() {
    float x;
    float error;
    ASSERT(sscanf("-3.141", "%f", &x));
    error = -3.141 - x;
    if (error < 0)
        error = -1.0 * error;
    ASSERT(error < 10e-5);
    return 0;
} 

/*
 * Testcase 120: sscanf
 * Test parsing of a float (f, e, E, g, a)
 * Parse a floating point number with a trailing e-expression
 */
int testcase120() {
    float x;
    float error;
    ASSERT(sscanf("3.141e5", "%f", &x));
    error = 3.141e5 - x;
    if (error < 0)
        error = -1.0 * error;
    ASSERT(error < 10e-5);
    return 0;
} 

/*
 * Testcase 121: sscanf
 * Test parsing of a float (f, e, E, g, a)
 * Parse a floating point number as a double
 */
int testcase121() {
    double x;
    double error;
    ASSERT(sscanf("3.141e5", "%lf", &x));
    error = 3.141e5 - x;
    if (error < 0)
        error = -1.0 * error;
    ASSERT(error < 10e-5);
    return 0;
} 

/*
 * Testcase 122: sscanf
 * Test parsing of a float (f, e, E, g, a) followed by n
 * Parse a floating point number as a double
 */
int testcase122() {
    double x;
    int n;
    double error;
    ASSERT(sscanf("3.141e5", "%lg%n", &x, &n));
    error = 3.141e5 - x;
    if (error < 0)
        error = -1.0 * error;
    ASSERT(error < 10e-5);
    ASSERT(n == strlen("3.141e5"));
    return 0;
} 

/*
 * Testcase 123: sscanf
 * Test parsing of a float (f, e, E, g, a) followed by n
 * Parse a floating point number as a double, use a negative exponent
 */
int testcase123() {
    double x;
    int n;
    double error;
    ASSERT(sscanf("3.141e-2", "%lg%n", &x, &n));
    error = 3.141e-2 - x;
    if (error < 0)
        error = -1.0 * error;
    ASSERT(error < 10e-5);
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
    RUN_CASE(18);
    RUN_CASE(19);
    RUN_CASE(20);
    RUN_CASE(21);
    RUN_CASE(22);
    RUN_CASE(23);
    RUN_CASE(24);
    RUN_CASE(25);
    RUN_CASE(26);
    RUN_CASE(27);
    RUN_CASE(28);
    RUN_CASE(29);
    RUN_CASE(30);
    RUN_CASE(31);
    RUN_CASE(32);
    RUN_CASE(33);
    RUN_CASE(34);
    RUN_CASE(35);
    RUN_CASE(36);
    RUN_CASE(37);
    RUN_CASE(38);
    RUN_CASE(39);
    RUN_CASE(40);
    RUN_CASE(41);
    RUN_CASE(42);
    RUN_CASE(43);
    RUN_CASE(44);
    RUN_CASE(45);
    RUN_CASE(46);
    RUN_CASE(47);
    RUN_CASE(48);
    RUN_CASE(49);
    RUN_CASE(50);
    RUN_CASE(51);
    RUN_CASE(52);
    RUN_CASE(53);
    RUN_CASE(54);
    RUN_CASE(55);
    RUN_CASE(56);
    RUN_CASE(57);
    RUN_CASE(58);
    RUN_CASE(59);
    RUN_CASE(60);
    RUN_CASE(61);
    RUN_CASE(62);
    RUN_CASE(63);
    RUN_CASE(64);
    RUN_CASE(65);
    RUN_CASE(66);
    RUN_CASE(67);
    RUN_CASE(68);
    RUN_CASE(69);
    RUN_CASE(70);
    RUN_CASE(71);
    RUN_CASE(72);
    RUN_CASE(73);
    RUN_CASE(74);
    RUN_CASE(75);
    RUN_CASE(76);
    RUN_CASE(77);
    RUN_CASE(78);
    RUN_CASE(79);
    RUN_CASE(80);
    RUN_CASE(81);
    RUN_CASE(82);
    RUN_CASE(83);
    RUN_CASE(84);
    RUN_CASE(85);
    RUN_CASE(86);
    RUN_CASE(87);
    RUN_CASE(88);
    RUN_CASE(89);
    RUN_CASE(90);
    RUN_CASE(91);
    RUN_CASE(92);
    RUN_CASE(93);
    RUN_CASE(94);
    RUN_CASE(95);
    RUN_CASE(96);
    RUN_CASE(97);
    RUN_CASE(98);
    RUN_CASE(99);
    RUN_CASE(100);
    RUN_CASE(101);
    RUN_CASE(102);
    RUN_CASE(103);
    RUN_CASE(104);
    RUN_CASE(105);
    RUN_CASE(106);
    RUN_CASE(107);
    RUN_CASE(108);
    RUN_CASE(109);
    RUN_CASE(110);
    RUN_CASE(111);
    RUN_CASE(112);
    RUN_CASE(113);
    RUN_CASE(114);
    RUN_CASE(115);
    RUN_CASE(116);
    RUN_CASE(117);
    RUN_CASE(118);
    RUN_CASE(119);
    RUN_CASE(120);
    RUN_CASE(121);
    RUN_CASE(122);
    RUN_CASE(123);
    END;
}
