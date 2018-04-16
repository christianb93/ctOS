/*
 * testfiles.c
 *
 */

#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

/*
 * Macro for assertions in unit test cases
 */
#define ASSERT(x)  do { if (!(x)) { \
        printf("Assertion %s failed at line %d in %s..", #x, __LINE__, __FILE__ ); \
        return 1 ;   \
} \
} while (0)

/*
 * Set up statistics
 */
#define INIT  int __failed=0; int __passed=0; int __rc=0 ; \
        printf("------------------------------------------\n"); \
        printf("Starting unit test %s\n", __FILE__); \
        printf("------------------------------------------\n");

/*
 * Print statistic and return
 */
#define END printf("------------------------------------------\n"); \
        printf("Overall test results (%s):\n", __FILE__); \
        printf("------------------------------------------\n"); \
        printf("Failed: %d  Passed:  %d\n", __failed, __passed); \
        printf("------------------------------------------\n"); return __rc;

/*
 * Execute a test case
 */
#define RUN_CASE(x) do { __rc= do_test_case(x, testcase##x);  \
        if (__rc) __failed++; else __passed++;} while (0)

/*
 * Forward declaration - this is in kunit.o
 */
int do_test_case(int x, int (*testcase)());

/*
 * Testcase 1: create a new, empty file testtmp1 and stat it
 */
int testcase1() {
    struct stat mystat;
    int rc;
    int fd;
    /*
     * Make sure that file does not exist yet
     * This assertion will trigger if a previous testrun has been aborted,
     * you need to manually remove the file in this case before being able
     * to proceed
     */
    rc = stat("testtmp1", &mystat);
    ASSERT(-1==rc);
    /*
     * Create it
     */
    fd=open("testtmp1", O_CREAT, S_IRWXU);
    ASSERT(fd);
    close(fd);
    /*
     * and stat again
     */
    rc = stat("testtmp1", &mystat);
    ASSERT(0 == rc);
    /*
     * chmod
     */
    ASSERT(0 == chmod("testtmp1", 0644));
    ASSERT(0 == stat("testtmp1", &mystat));
    ASSERT((mystat.st_mode & 0777) == 0644);
    ASSERT(0 == chmod("testtmp1", 0700));
    ASSERT(0 == stat("testtmp1", &mystat));
    ASSERT((mystat.st_mode & 0777) == 0700);
    return 0;
}

/*
 * Testcase 2: write to the newly created file
 */
int testcase2() {
    char buffer[16];
    int i;
    int fd;
    /*
     * Fill buffer
     */
    for (i=0;i<16;i++)
        buffer[i]='a'+i;
    /*
     * Open file
     */
    fd=open("testtmp1", O_CREAT, S_IRWXU);
    ASSERT(fd);
    /*
     * Write 16 bytes and close file again
     */
    ASSERT(16==write(fd, buffer, 16));
    close(fd);
    return 0;
}

/*
 * Testcase 3: remove file again
 */
int testcase3() {
    struct stat mystat;
    int rc;
    /*
     * Remove file
     */
    ASSERT(0==unlink("testtmp1"));
    /*
     * Make sure that file does not exist any more
     */
    rc = stat("testtmp1", &mystat);
    ASSERT(-1==rc);
    ASSERT(ENOENT==errno);
    return 0;
}

/*
 * Testcase 4: create a file, write to it and read data back
 */
int testcase4() {
    char write_buffer[16];
    char read_buffer[16];
    int i;
    int fd;
    /*
     * Fill buffers
     */
    for (i=0;i<16;i++) {
        write_buffer[i]='a'+i;
        read_buffer[i]=0;
    }
    /*
     * Open file
     */
    fd=open("testtmp1", O_CREAT, S_IRWXU);
    ASSERT(fd);
    /*
     * Write 16 bytes and close file again
     */
    ASSERT(16==write(fd, write_buffer, 16));
    close(fd);
    /*
     * Open file again and read 16 bytes
     */
    fd=open("testtmp1", O_CREAT, S_IRWXU);
    ASSERT(fd);
    ASSERT(16==read(fd, read_buffer, 16));
    close(fd);
    /*
     * Compare
     */
    for (i=0;i<16;i++) {
        ASSERT(read_buffer[i]==write_buffer[i]);
    }
    return 0;
}

/*
 * Testcase 5: duplicate a file descriptor using dup and verify that the file position is shared
 */
int testcase5() {
    int fd1;
    int fd2;
    char buffer;
    fd1 = open("testtmp1", 0);
    ASSERT(fd1);
    fd2=dup(fd1);
    ASSERT(fd2);
    ASSERT(1==read(fd1, &buffer, 1));
    ASSERT(buffer=='a');
    ASSERT(1==read(fd2, &buffer, 1));
    ASSERT(buffer=='b');
    close(fd1);
    close(fd2);
    return 0;
}

/*
 * Testcase 6: duplicate a file descriptor using fcntl and verify that the file position is shared
 */
int testcase6() {
    int fd1;
    int fd2;
    char buffer;
    fd1 = open("testtmp1", 0);
    ASSERT(fd1);
    fd2=fcntl(fd1, F_DUPFD, 10);
    ASSERT(fd2>=10);
    ASSERT(1==read(fd1, &buffer, 1));
    ASSERT(buffer=='a');
    ASSERT(1==read(fd2, &buffer, 1));
    ASSERT(buffer=='b');
    close(fd1);
    close(fd2);
    return 0;
}

/*
 * Testcase 7: do fstat on the file
 */
int testcase7() {
    int fd;
    struct stat mystat;
    fd = open("testtmp1", 0);
    ASSERT(fd);
    ASSERT(0==fstat(fd, &mystat));
    ASSERT(16==mystat.st_size);
    close(fd);
    return 0;
}


/*
 * Testcase 8: remove file again
 */
int testcase8() {
    struct stat mystat;
    int rc;
    /*
     * Remove file
     */
    ASSERT(0==unlink("testtmp1"));
    /*
     * Make sure that file does not exist any more
     */
    rc = stat("testtmp1", &mystat);
    ASSERT(-1==rc);
    return 0;
}

/*
 * Testcase 9: add a directory
 */
int testcase9() {
    struct stat mystat;
    /*
     * Create a directory
     */
    ASSERT(0 == mkdir("testdir1", 0777));
    ASSERT(0 == stat("testdir1", &mystat));
    ASSERT(S_ISDIR(mystat.st_mode));
    /*
     * and remove it again
     */
    ASSERT(0 == rmdir("testdir1"));
    ASSERT(-1 == stat("testdir1", &mystat));
    return 0;
}

/*
 * Testcase 10: rename a file
 */
int testcase10() {
    struct stat mystat;
    int fd;
    /*
     * Want to use testtmp1 - make sure it does not exist yet
     */
    ASSERT(-1 == stat("testtmp1", &mystat));
    /*
     * same for testtmp2
     */
    ASSERT(-1 == stat("testtmp2", &mystat));
    /*
     * Create testtmp1
     */
    fd = open("testtmp1", O_CREAT, 0777);
    if (fd < 0) {
        perror("open");
    }
    ASSERT(fd >= 0);
    close(fd);
    /*
     * and rename it
     */
    ASSERT(0 == rename("testtmp1", "testtmp2"));
    ASSERT(-1 == stat("testtmp1", &mystat));
    ASSERT(0 == stat("testtmp2", &mystat));
    /*
     * finally remove file again
     */
    ASSERT(0 == unlink("testtmp2"));
    return 0;
}

/*
 * Testcase 11:
 * open a file using fdopen and write to it using fprintf. Then verify result
 */
int testcase11() {
    int fd;
    FILE* file;
    char buffer[128];
    /*
     * Create testtmp1
     */
    fd = open("testtmp1", O_CREAT, 0777);
    if (fd < 0) {
        perror("open");
    }
    ASSERT(fd >= 0);
    /*
     * Open stream
     */
    file = fdopen(fd, "rw");
    ASSERT(file);
    /*
     * and write to it
     */
    fprintf(file, "%s", "hello");
    fclose(file);
    /*
     * Now open file again and read data
     */
    fd = open("testtmp1", O_RDONLY);
    ASSERT(fd >= 0);
    memset((void*) buffer, 0, 128);
    ASSERT(strlen("hello") == read(fd, buffer, strlen("hello")));
    close(fd);
    ASSERT(0 == strcmp("hello", buffer));
    /*
     * and remove file again
     */
    ASSERT(0 == unlink("testtmp1"));
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
    END;
}

