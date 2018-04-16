/*
 * testclient.c
 *
 */


#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>

void main(int argc, char** argv) {
    in_addr_t ip_address;
    in_port_t port;
    struct in_addr in;
    struct sockaddr_in faddr;
    int fd;
    /*
     * Accept arguments
     */
    if (argc < 2) {
        printf("Usage: testclient <ip-address> <port>\n");
        _exit(1);
    }
    if (argv[1]) {
        ip_address = inet_addr(argv[1]);
    }
    else {
        printf("ARGV[1] is NULL\n");
        _exit(1);
    }
    in.s_addr = ip_address;
    printf("Using IP address %s\n", inet_ntoa(in));
    if (argv[2]) {
        port = strtoul(argv[2], 0, 10);
    }
    else {
        printf("ARGV[2] is NULL\n");
        _exit(1);
    }
    /*
     * Create socket
     */
    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) {
        perror("Could not create socket");
        _exit(1);
    }
    /*
     * Connect socket
     */
    faddr.sin_addr = in;
    faddr.sin_family = AF_INET;
    faddr.sin_port = htons(port);
    if (-1 == connect(fd, (struct sockaddr*) &faddr, sizeof(struct sockaddr_in))) {
        perror("Could not connect socket");
        _exit(1);
    }
    while (1);
}
