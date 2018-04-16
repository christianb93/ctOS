/*
 * ext2samples.c
 *
 * Create various samples
 *
 * Available samples:
 *
 * A - create a file with a hole in the second direct block, i.e. first and third direct block are allocated, second direct block is 0
 * B - create a file with a hole in the indirect block, i.e. some direct blocks and entries in the double indirect block are allocated
 * C - create a file with a hole in the double indirect block, i.e. the first direct blocks and entries in the triple indirect block are used
 * D - create a file with a length of 10 bytes
 * E - create a file of original length 2048 bytes which is then truncated to 5 bytes
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

/*
 * Print usage
 */
static void print_usage() {
    printf("Usage: ext2samples <sample> <file>\n");
    printf(
            "where <file> is the file to be created and <sample> is one of the following letters:\n");
    printf("A - create a file with one hole in the second direct block\n");
    printf("B - create a file with one hole spanning the entire indirect block\n");
    printf("C - create a file with one hole spanning the entire double indirect block\n");
    printf("D - create a file with length 10 bytes\n");
    printf("E - create a file with original length 2048 bytes which is then truncated to 5 bytes\n");
}

/*
 * Create sample A
 * This function will create a file which has an allocated first and third direct
 * block and an unallocated second direct block ("hole")
 */
static void create_sampleA(char* file) {
    char* msg = "abcde";
    int fd;
    int rc;
    printf("Creating sample file %s of type A\n", file);
    fd = open(file, O_RDWR + O_CREAT, S_IRWXU);
    if (fd < 0) {
        printf("Could not open file %s\n", file);
        _exit(1);
    }
    rc = write(fd, (void*) msg, 5);
    if (rc < 0) {
        printf("Could not write to file\n");
        _exit(1);
    }
    rc = lseek(fd, 2048, SEEK_SET);
    if (rc < 0) {
        printf("Could not seek file to position 2048\n");
    }
    rc = write(fd, (void*) msg, 5);
    if (rc < 0) {
        printf("Could not write to file\n");
        _exit(1);
    }
    close(fd);
}

/*
 * Create sample B
 * This function will create a file for which all direct blocks and some
 * double indirect blocks are allocated, but the entire indirect block is
 * empty and therefore unallocated
 */
static void create_sampleB(char* file) {
    char* msg = "abcde";
    int fd;
    int rc;
    unsigned int block;
    printf("Creating sample file %s of type B\n", file);
    fd = open(file, O_RDWR + O_CREAT, S_IRWXU);
    if (fd < 0) {
        printf("Could not open file %s\n", file);
        _exit(1);
    }
    /*
     * Write first block
     */
    rc = write(fd, (void*) msg, 5);
    if (rc < 0) {
        printf("Could not write to file\n");
        _exit(1);
    }
    /*
     * Seek to first double indirect block
     */
    block = 12+(1024/sizeof(int));
    rc = lseek(fd, block*1024, SEEK_SET);
    if (rc < 0) {
        printf("Could not seek file to position %d\n", block*1024);
    }
    rc = write(fd, (void*) msg, 5);
    if (rc < 0) {
        printf("Could not write to file\n");
        _exit(1);
    }
    close(fd);
}



/*
 * Create sample C
 * This function will create a file for which all direct and indirect blocks and some
 * triple indirect blocks are allocated, but the entire double indirect block is
 * empty and therefore unallocated
 */
static void create_sampleC(char* file) {
    char* msg = "abcde";
    int fd;
    int rc;
    unsigned int block;
    printf("Creating sample file %s of type C\n", file);
    fd = open(file, O_RDWR + O_CREAT, S_IRWXU);
    if (fd < 0) {
        printf("Could not open file %s\n", file);
        _exit(1);
    }
    /*
     * Write first block
     */
    rc = write(fd, (void*) msg, 5);
    if (rc < 0) {
        printf("Could not write to file\n");
        _exit(1);
    }
    /*
     * Seek to first triple indirect block
     */
    block = 12+256+256*256;
    rc = lseek(fd, block*1024, SEEK_SET);
    if (rc < 0) {
        printf("Could not seek file to position %d\n", block*1024);
    }
    rc = write(fd, (void*) msg, 5);
    if (rc < 0) {
        printf("Could not write to file\n");
        _exit(1);
    }
    close(fd);
}

/*
 * Create a file with length 10 bytes
 */
static void create_sampleD(char* file) {
    char* msg = "0123456789";
    int fd;
    int rc;
    unsigned int block;
    printf("Creating sample file %s of type D\n", file);
    fd = open(file, O_RDWR + O_CREAT, S_IRWXU);
    if (fd < 0) {
        printf("Could not open file %s\n", file);
        _exit(1);
    }
    /*
     * Write first block
     */
    rc = write(fd, (void*) msg, 10);
    if (rc < 0) {
        printf("Could not write to file\n");
        _exit(1);
    }
    close(fd);
}

/*
 * Create a file with length 2048 bytes and truncate
 * it to 5 bytes
 */
static void create_sampleE(char* file) {
    char* buffer;
    int fd;
    int rc;
    unsigned int block;
    printf("Creating sample file %s of type E\n", file);
    fd = open(file, O_RDWR + O_CREAT, S_IRWXU);
    if (fd < 0) {
        printf("Could not open file %s\n", file);
        _exit(1);
    }
    /*
     * Write first block
     */
    buffer = (char*) malloc(2048);
    memset((void*) buffer, 0xff, 2048);
    rc = write(fd, (void*) buffer, 2048);
    if (rc < 0) {
        printf("Could not write to file\n");
        _exit(1);
    }
    close(fd);
    /*
     * Now truncate
     */
    truncate(file, 5);
}

void main(int argc, char** argv) {
    if (argc != 3) {
        print_usage();
        _exit(1);
    }
    switch (argv[1][0]) {
        case 'A':
            create_sampleA(argv[2]);
            break;
        case 'B':
            create_sampleB(argv[2]);
            break;
        case 'C':
            create_sampleC(argv[2]);
            break;
        case 'D':
            create_sampleD(argv[2]);
            break;
        case 'E':
            create_sampleE(argv[2]);
            break;
        default:
            print_usage();
            break;
    }
}

