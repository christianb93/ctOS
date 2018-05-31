/*
 * Used for testing - create a file called hello in the current working 
 * directory and dump the arguments into it, separated by a newline
 */
 
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h> 
 
int main(int argc, char** args) {
    int fd;
    fd = open("hello", O_CREAT + O_RDWR, S_IRWXU);
    for (int i = 0; i < argc; i++) {
        if (args[i]) {
            write(fd, args[i], strlen(args[i]));
            write(fd, "\n", 1);
        }
    }
    close(fd);
    return 0;
}