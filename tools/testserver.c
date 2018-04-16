/*
 * testserver.c
 *
 * This server is the counterpart for the test client in commands/testnet.c. Each testcase in the test client will open a new
 * connection. This server accepts incoming connection and uses a connection counter to identify the test case to which the
 * connection belongs. It then branches into a test case specific routine.
 *
 * Note that the mapping between the test cases in server and client is not one to one, as not each TC in the client opens
 * a socket connection. In fact, the mapping is as follows.
 *
 * TC in client        Connection count    TC in server
 * -----------------------------------------------------
 * 1                   n/a                 n/a
 * 2                   1                   n/a
 * 3                   2                   1
 * 4                   3                   2
 * 5                   4                   3
 * 6                   5                   4
 * 7                   6                   5
 * 8                   7                   6
 * 9                   8                   7
 */

#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

/*
 * Connection counter
 */
static int connection_count = 0;

/*
 * Number of test cases
 */
#define TC_COUNT 9

/*
 * This is the main loop of the UDP part of the testserver. We read messages from the UDP port specified
 * on the command line which are then reflected back to the sender
 */
static void udp_server(unsigned int ip_addr, unsigned short port) {
    unsigned char* buffer;
    struct sockaddr_in in;
    unsigned int req_ip_addr;
    unsigned short req_port;
    unsigned int addrlen;
    int fd;
    int i;
    int rc;
    buffer = (unsigned char*) malloc(16384);
    if (0 == buffer) {
        printf("UDP server: could not allocate memory for buffer\n");
        _exit(1);
    }
    /*
     * Create a UDP server socket and bind it to local address
     */
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("UDP server: socket");
        _exit(1);
    }
    in.sin_addr.s_addr = ip_addr;
    in.sin_port = ntohs(port);
    in.sin_family = AF_INET;
    if (bind(fd, (struct sockaddr*) &in, sizeof(struct sockaddr_in))) {
        perror("UDP server: bind");
        _exit(1);
    }
    /*
     * Now wait in a loop for incoming data
     */
    while(1) {
        addrlen = sizeof(struct sockaddr_in);
        rc = recvfrom(fd, buffer, 16384, 0, (struct sockaddr*) &in, &addrlen);
        if ((rc > 0) && (addrlen == sizeof(struct sockaddr_in))) {
            /*
             * Got data. Wait for one second and process data
             */
            req_ip_addr = in.sin_addr.s_addr;
            req_port = in.sin_port;
            printf("Got request from IP address %x, port number %d, %d bytes\n", req_ip_addr, ntohs(req_port), rc);
            sleep(1);
            /*
             * Convert buffer
             */
            for (i = 0; i < rc; i++) {
                buffer[i] = ~buffer[i];
            }
            sendto(fd, buffer, rc, 0, (struct sockaddr*) &in, sizeof(struct sockaddr_in));
        }
        else {
            printf("rc = %d, addrlen = %d\n", rc, addrlen);
        }
    }
    free((void*) buffer);
}


/*
 * Testcase 1
 */
static void testcase1(int connection) {
    /*
     * Do nothing so that data adds up in the receive queue
     */
    printf("Server: doing testcase 1\n");
    while(1) {
        sleep(1);
    }
}

/*
 * Testcase 2
 */
static void testcase2(int connection) {
    unsigned char c[100];
    int count = 0;
    int bytes;
    int i;
    /*
     * Read data in a loop, 100 bytes at a time
     */
    count = 0;
    while ((bytes = (recv(connection, c, 100, 0))) > 0) {
        printf("Echoing back %d bytes\n", bytes);
       /*
         * and echo data back to sender
         */
        send(connection, c, bytes, 0);
    }
}

/*
 * Testcase 3
 */
static void testcase3(int connection) {
    unsigned char c[16384];
    int bytes;
    int total = 0;
    int i;
    /*
     * Wait 2 seconds before reading any data
     */
    printf("Testcase 3: waiting\n");
    sleep(5);
    /*
     * and read as long data is available
     */
    printf("Testcase 3: starting to read data from socket\n");
    while ((bytes = (recv(connection, c, 16384, 0))) > 0) {
        total += bytes;
        printf("Have %d bytes in total (%d kB) \n", total, total / 1024);
    }
}

/*
 * Testcase 4
 */
static void testcase4(int connection, unsigned int ip_address, int port) {
    int c;
    struct sockaddr_in faddr;
    int fd;
    unsigned char buffer[256];
    int i;
    printf("Testcase 4: got connection\n");
    /*
     * Wait for one byte of data
     */
    recv(connection, &c, 1, 0);
    /*
     * Now wait for one second, then try to establish a connection to port + 1
     * on 10.0.2.20
     */
    sleep(1);
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == fd) {
        printf("Could not create socket\n");
        return;
    }
    faddr.sin_addr.s_addr = inet_addr("10.0.2.20");
    faddr.sin_port = htons(port + 1);
    faddr.sin_family = AF_INET;
    if (-1 == connect(fd, (struct sockaddr*) &faddr, sizeof(struct sockaddr_in))) {
        printf("Could not open connection to IP address %x, port %d, error is ", ip_address, port + 1);
        perror("");
        return;
    }
    /*
     * Now send 256 bytes
     */
    for (i = 0; i < 256; i++) {
        buffer[i] = i;
    }
    send(fd, buffer, 256, 0);
    printf("Sent 256 bytes to client\n");
    /*
     * and close socket
     */
    printf("Now closing socket with foreign port number %d\n", port + 1);
    close(fd);
}

/*
 * Testcase 5
 */
static void testcase5(int connection) {
    unsigned char c[100];
    int count = 0;
    int i;
    /*
     * Wait for three seconds to give client time
     * to sleep in select
     */
    sleep(3);
    /*
     * then send 100 bytes
     */
    printf("Testcase 5: sending 100 bytes\n");
    send(connection, c, 100, 0);
}

/*
 * Testcase 6
 */
static void testcase6(int connection) {
    /*
     * Wait for three seconds to give client time
     * to sleep in select
     */
    sleep(3);
    /*
     * but do not send any data - select should time out
     */
}

/*
 * Testcase 7
 */
static void testcase7(int connection) {
    /*
     * In this testcase, the client will wait in recv until an alarm goes
     * off - so we do nothing here for a few seconds
     */
    sleep(10);
}

void main(int argc, char** argv) {
    in_addr_t ip_address;
    in_port_t port;
    struct in_addr in;
    struct sockaddr_in local;
    int fd;
    int pid;
    int connection;
    int bytes;
    int i;
    int bufsize = 16384;
    int optsize = 4;
    /*
     * Accept arguments
     */
    if (argc < 2) {
        printf("Usage: testserver <ip-address> <port>\n");
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
     * Start UDP process
     */
    pid = fork();
    if (0 == pid) {
        udp_server(ip_address, port);
        _exit(0);
    }
    if (pid < 0 ) {
        perror("fork");
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
     * Set options
     */
    if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize))) {
        perror("Could not set socket options\n");
    }
    optsize = sizeof(bufsize);
    if (getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &bufsize, &optsize)) {
        perror("Could not get socket options\n");
    }
    else
        printf("Receive buffer size: %d\n", bufsize);
    /*
     * Bind socket to local address
     */
    local.sin_addr = in;
    local.sin_family = AF_INET;
    local.sin_port = htons(port);
    if (-1 == bind(fd, (struct sockaddr*) &local, sizeof(struct sockaddr_in))) {
        perror("Could not bind socket to local address");
        _exit(1);
    }
    /*
     * and listen
     */
    if (-1 == listen(fd, 5)) {
        perror("Could not LISTEN on socket");
        _exit(1);
    }
    /*
     * Now accept incoming connections
     */
    while (1) {
        connection = accept(fd, 0, 0);
        if (connection < 0) {
            perror("Could not accept incoming connection\n");
        }
        else {
            printf("New connection created, forking off process\n");
            connection_count++;
            pid = fork();
            if (pid < 0) {
                perror("Could not create child\n");
            }
            if (0 == pid) {
                /*
                 * Child. Branch to test case
                 */
                printf("Doing test case for connection count %d, fd = %d\n", connection_count, connection);
                switch (connection_count % TC_COUNT) {
                    case 2:
                        testcase1(connection);
                        break;
                    case 3:
                        testcase2(connection);
                        break;
                    case 4:
                        testcase3(connection);
                        break;
                    case 5:
                        testcase4(connection, ip_address, port);
                        break;
                    case 6:
                        testcase5(connection);
                        break;
                    case 7:
                        testcase6(connection);
                        break;
                    case 8:
                        testcase7(connection);
                        break;
                    default:
                        break;
                }
                close(connection);
                _exit(0);
            }
            if (pid > 0) {
                /*
                 * Parent
                 */
                printf("Created child\n");
                /*
                 * Close our copy of connection
                 */
                close(connection);
            }
        }
    }
}
