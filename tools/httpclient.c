/*
 * A simple HTTP client
 *
 */


#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>

void main(int argc, char** argv) {
    in_addr_t ip_address;
    struct in_addr in;
    struct sockaddr_in faddr;
    int fd;
    char* host = 0;
    char* path = 0;
    struct hostent* he;
    char write_buffer[256];
    char read_buffer[256];
    int b;
    int tries = 0;
    /*
     * Accept arguments
     */
    if (argc < 2) {
        printf("Usage: httpclient <url>\n");
        _exit(1);
    }
    if (argv[1]) {
        /*
         * Strip off leading http://
         */
        host = argv[1];
        if (0 == strcmp(host, "http://")) {
            host = host + 7;
        }
    }
    else {
        printf("ARGV[1] is NULL\n");
        _exit(1);
    }
    host = strtok(host, "/");
    path = strtok(NULL, "/");
    if (0 == path) {
        path = "";
        printf("Using hostname %s\n", host);
    }
    else {
        printf("Using hostname %s, path %s\n", host, path);
    }
    
    /*
     * Resolve hostname
     */
    if (0 == (he = gethostbyname(host))) {
        printf("Could not resolve host name %s\n", host);
        _exit(1);
    }
    ip_address = *((unsigned int *) he->h_addr_list[0]);
    in.s_addr = ip_address;
    printf("Using IP address %s\n", inet_ntoa(in));
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
    faddr.sin_port = htons(80);
    if (-1 == connect(fd, (struct sockaddr*) &faddr, sizeof(struct sockaddr_in))) {
        perror("Could not connect socket");
        _exit(1);
    }
    printf("Connection established, now sending GET request\n");
    b = sprintf(write_buffer, "GET /%s HTTP/1.1\r\n", path);
    b+= sprintf(write_buffer + b, "Host: %s\r\n", host);
    b+= sprintf(write_buffer + b, "User-Agent: ctOS\r\n");
    b+= sprintf(write_buffer + b, "Accept: */*\r\n");
    /*
     * Do not forget to complete request with an empty line
     */
    sprintf(write_buffer + b, "\r\n");
    printf("%s", write_buffer);
    for (b = 0; b < strlen(write_buffer); b++) {
        printf("%.2x ", write_buffer[b]);
        if (15 == (b % 16)) {
            printf("\n");
        }
    }
    b = send(fd, write_buffer, strlen(write_buffer), 0);
    printf("\n\nNow waiting for data to come in\n");
    /*
     *  Put socket into non-blocking mode
     */
    if (fcntl(fd, F_SETFL, O_NONBLOCK)) {
        printf("Warning: could not set socket into non-blocking mode\n");
    }
    
    while (tries < 5) {
        b = read(fd,read_buffer,256);
        if (b > 0) {
            write(1,read_buffer,b);
        }
        else {
            tries += 1;
            sleep(1);
        }
	}
}
