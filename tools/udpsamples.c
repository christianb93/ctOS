/*
 * udpsamples.c
 *
 */

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

void main() {
    int fd;
    int i;
    struct sockaddr_in faddr;
    char buffer[8192];
    /*
     * Create a UDP socket
     */
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Could not create UDP socket");
        _exit(1);
    }
    /*
     * Connect socket - this will define the destination address so
     * that we can use send on the socket
     */
    faddr.sin_addr.s_addr = inet_addr("192.168.178.1");
    faddr.sin_family = AF_INET;
    faddr.sin_port = htons(30000);
    if (connect(fd, (struct sockaddr*) &faddr, sizeof(struct sockaddr_in)) < 0) {
        perror("Could not connect socket");
        _exit(1);
    }
    /*
     * Send a message
     */
    for (i = 0; i < 8192; i++)
        buffer[i] = i;
    if (send(fd, (const char*) buffer, 1480 - 8 + 1, 0) < 0) {
        perror("Could not write into socket");
        _exit(1);
    }
}
