/*
 * testtty.c
 *
 */

#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

int main() {
    int rc;
    char c;
    unsigned int tflags;
    char buffer[128];
    /*
     * Testcase 1: have the user enter three characters followed by RETURN and do a read
     * for five characters. This read call should return 4
     */
    memset(buffer, 0, 128);
    printf("Testcase 1: please enter abc, then hit RETURN\n");
    rc = read(STDIN_FILENO, buffer, 5);
    if (rc != 4) {
        printf("Testcase failed, rc = %d, should be 4\n", rc);
        _exit(1);
    }
    if (strcmp(buffer, "abc\n")) {
        printf("Testcase failed, expected abc<NL>, got %s\n", buffer);
        _exit(1);
    }
    printf("Testcase 1 successful\n");
    /*
     * Testcase 2: have the user enter three characters followed by RETURN and do a read
     * for two characters. This read call should return 2. A subsequent read should return
     * c and \n
     */
    memset(buffer, 0, 128);
    printf("Testcase 2: please enter abc, then hit RETURN\n");
    rc = read(STDIN_FILENO, buffer, 2);
    if (rc != 2) {
        printf("Testcase failed, rc = %d, should be 2\n", rc);
        _exit(1);
    }
    if (strcmp(buffer, "ab")) {
        printf("Testcase failed, expected ab, got %s\n", buffer);
        _exit(1);
    }
    memset(buffer, 0, 128);
    rc = read(STDIN_FILENO, buffer, 2);
    if (rc != 2) {
        printf("Testcase failed, rc = %d, should be 2\n", rc);
        _exit(1);
    }
    if (strcmp(buffer, "c\n")) {
        printf("Testcase failed, expected c and NL, got %s\n", buffer);
        _exit(1);
    }
    printf("Testcase 2 successful\n");
    /*
     * Testcase 3: have the user hit Ctrl-D
     */
    printf("Testcase 3: please hit Ctrl-D\n");
    rc = read(STDIN_FILENO, buffer, 16);
    if (rc) {
        printf("Testcase 3 failed, expected rc 0, got %d\n", rc);
        _exit(1);
    }
    printf("Testcase 3 successful\n");
    /*
     * Testcase 4: have the user hit Ctrl-D after characters have been entered
     */
    printf("Testcase 3: please enter abc and then hit Ctrl-D\n");
    memset(buffer, 0, 128);
    rc = read(STDIN_FILENO, buffer, 2);
    if (rc!=2) {
        printf("Testcase 4 failed, expected rc 2, got %d\n", rc);
        _exit(1);
    }
    if (strcmp(buffer, "ab")) {
        printf("Testcase failed, expected ab, got %s\n", buffer);
        _exit(1);
    }
    memset(buffer, 0, 128);
    rc = read(STDIN_FILENO, buffer, 2);
    if (rc!=1) {
        printf("Testcase 4 failed, expected rc 1, got %d\n", rc);
        _exit(1);
    }
    if (strcmp(buffer, "c")) {
        printf("Testcase failed, expected c, got %s\n", buffer);
        _exit(1);
    }
    printf("Testcase 4 successful\n");
    printf("Testcase 5: please enter abc and press RETURN, then hit Ctrl-D\n");
    rc = 0;
    while ((c=getc(stdin))!=EOF) {
        rc++;
    }
    if (4!=rc) {
        printf("Testcase 5 failed, expected rc 4, got %d\n", rc);
        _exit(1);
    }
    printf("Testcase 5 successful\n");
    /*
     * Set terminal to non-blocking mode
     */
    tflags = fcntl(STDIN_FILENO, F_GETFL);
    fcntl(STDIN_FILENO, F_SETFL, tflags | O_NONBLOCK);
    /*
     * Now do a non-blocking read
     */
    printf("Testcase 6: doing non-blocking read\n");
    rc = read(STDIN_FILENO, buffer, 1);
    /*
     * and set it back to original settings
     */
    fcntl(STDIN_FILENO, F_SETFL, tflags);
    if ((rc == -1) && (errno==EAGAIN))
        printf("Testcase 6 successful\n");
    else {
        printf("Testcase 6 failed\n");
        _exit(1);
    }
    /*
      * Now do a non-blocking read which returns data
      */
     printf("Testcase 7: doing non-blocking read, please enter a and hit RETURN within the next five seconds\n");
     sleep(5);
     fcntl(STDIN_FILENO, F_SETFL, tflags | O_NONBLOCK);
     rc = read(STDIN_FILENO, buffer, 1);
     fcntl(STDIN_FILENO, F_SETFL, tflags);
     if ((rc == 1) && (buffer[0]='a'))
         printf("Testcase 7 successful\n");
     else {
         printf("Testcase 7 failed, expected return code 1, got %d with errno %d\n", rc, errno);
         _exit(1);
     }
    return 0;
}
