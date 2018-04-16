/*
 * tcpsamples.c
 *
 */

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <unistd.h>

void main() {
    int fd;
    int rc;
    struct sockaddr_in laddr;
    struct sockaddr_in saddr;
    socklen_t socklen;
    fd_set fdset;
    /*
     * Print some values
     */
    printf("Size of fd_set: %d\n", sizeof(fd_set));
    printf("FD_SETSIZE: %d\n", FD_SETSIZE);
    printf("NFDBITS: %d\n", NFDBITS);
    fdset.__fds_bits[0] = 10;
    /*
     * First create socket and try to bind it twice
     */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("Could not create socket");
        _exit(1);
    }
    laddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    laddr.sin_port = htons(30000);
    laddr.sin_family = AF_INET;
    rc = bind(fd, (struct sockaddr*) &laddr,sizeof(struct sockaddr_in));
    if (rc < 0) {
        perror("Could not bind socket");
        _exit(1);
    }
    laddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    laddr.sin_port = htons(30000);
    laddr.sin_family = AF_INET;
    rc = bind(fd, (struct sockaddr*) &laddr, sizeof(struct sockaddr_in));
    if (rc < 0) {
        perror("Could not bind socket in second attempt");
    }
    close(fd);
    printf("First sample completed\n");
    /*
     * Now repeat socket creation and try to listen on the socket before binding it
     * This will implicitly bind the socket to an ephemeral port with INADDR_ANY
     */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("Could not create socket");
        _exit(1);
    }
    rc = listen(fd, 5);
    if (rc < 0)
        perror("Could not listen on socket");
    else
        printf("Successfully listened on socket\n");
    socklen = sizeof(struct sockaddr_in);
    if (getsockname(fd, (struct sockaddr*) &saddr, &socklen) < 0)
        perror("Could not get socket address");
    printf("Port number: %d\n", ntohs(saddr.sin_port));
    printf("IP Address: %d\n", ntohl(saddr.sin_addr.s_addr));
    close(fd);
    printf("Second sample completed\n");
    /*
     * Bind a socket to port number 0. In this case, the kernel will chose
     * an ephemeral port and bind to this port with the given IP address
     */
    laddr.sin_port = 0;
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("Could not create socket");
        _exit(1);
    }
    rc = bind(fd, (struct sockaddr*) &laddr, sizeof(struct sockaddr_in));
    if (rc < 0) {
        perror("Could not bind socket in second attempt");
    }
    socklen = sizeof(struct sockaddr_in);
    if (getsockname(fd, (struct sockaddr*) &saddr, &socklen) < 0)
        perror("Could not get socket address");
    printf("Port number: %d\n", ntohs(saddr.sin_port));
    printf("IP Address: %d\n", ntohl(saddr.sin_addr.s_addr));
    printf("Third sample completed\n");
}

