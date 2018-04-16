/*
 * ipsamples.c
 *
 * A few samples used to demonstrate the usage of raw IP sockets
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>

/*
 * An IP header
 */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;


typedef struct {
    u8 version;                         // Header length in dwords (bits 0 - 3) and version (bits 4 - 7)
    u8 priority;                        // Priority
    u16 length;                         // Length of header and data in total
    u16 id;                             // Identifier
    u16 flags;                          // Flags and fragment offset
    u8 ttl;                             // TTL (time to live)
    u8 proto;                           // transport protocol
    u16 checksum;                       // Checksum
    u32 ip_src;                         // IP address of sender
    u32 ip_dest;                        // IP destination address
} __attribute__ ((packed)) ip_hdr_t;


/*
 * This is an ICMP header
 */
typedef struct {
    unsigned char type;                // Type of message
    unsigned char code;                // Message code
    unsigned short checksum;           // Header checksum
} __attribute__ ((packed)) icmp_hdr_t;

/*
 * The body of an ECHO request message
 */
typedef struct {
    unsigned short id;                 // Identifier
    unsigned short seq_no;             // Sequence number
} __attribute__ ((packed)) icmp_echo_request_t;

/*
 * ICMP message types
 */
#define ICMP_ECHO_REPLY 0
#define ICMP_ECHO_REQUEST 8

/*
 * This structure is an IP message header
 */
typedef struct {
    unsigned char version;                         // Header length in dwords (bits 0 - 3) and version (bits 4 - 7)
    unsigned char priority;                        // Priority
    unsigned short length;                         // Length of header and data in total
    unsigned short id;                             // Identifier
    unsigned short flags;                          // Flags and fragment offset
    unsigned char ttl;                             // TTL (time to live)
    unsigned char proto;                           // transport protocol
    unsigned short checksum;                       // Checksum
    unsigned int ip_src;                           // IP address of sender
    unsigned int ip_dest;                          // IP destination address
} __attribute__ ((packed)) ipv4_hdr_t;

/*
 * Some transport protocols
 */
#define IP_PROTO_ICMP 0x1
#define IP_PROTO_UDP 0x11
#define IP_PROTO_TCP 0x6

/*
 * Some flags to determine the behaviour
 */
#undef DO_BIND
#undef DO_CONNECT
#define DO_SENDTO

/*
 * Compute a checksum
 */
unsigned short net_compute_checksum(unsigned short* words, int byte_count) {
    unsigned int sum = 0;
    unsigned short rc;
    int i;
    unsigned short last_byte = 0;
    /*
     * First sum up all words
     */
    for (i = 0; i < byte_count / 2; i++) {
        sum = sum + ntohs(words[i]);
    }
    /*
     * If the number of bytes is odd, add left over byte << 8
     */
    if (1 == (byte_count % 2)) {
        last_byte = ((unsigned char*) words)[byte_count - 1];
        sum = sum + (last_byte << 8);
    }
    /*
     * Repeatedly add carry to LSB until carry is zero
     */
    while (sum >> 16)
        sum = (sum >> 16) + (sum & 0xFFFF);
    rc = sum;
    rc = ~rc;
    return rc;
}


/*
 * Build an ICMP echo request
 * Parameter:
 * @buffer - buffer to use
 * @buf_len - available bytes in buffer
 */
void create_ping(unsigned char* buffer, int buf_len) {
    icmp_hdr_t* hdr;
    icmp_echo_request_t* request;
    unsigned char* data;
    int i;
    unsigned short chksum;
    if (buf_len < sizeof(icmp_hdr_t) + sizeof(icmp_echo_request_t) + 16)
        return;
    /*
     * First create ICMP header, using checksum zero
     */
    hdr = (icmp_hdr_t*) buffer;
    hdr->code = 0;
    hdr->type = ICMP_ECHO_REQUEST;
    hdr->checksum = 0;
    /*
     * Next fill echo request fields
     */
    request = (icmp_echo_request_t*)(buffer + sizeof(icmp_hdr_t));
    request->id = htons(getpid());
    request->seq_no = htons(1);
    /*
     * and data
     */
    data = buffer + sizeof(icmp_hdr_t) + sizeof(icmp_echo_request_t);
    for (i = 0; i < buf_len - sizeof(icmp_hdr_t) - sizeof(icmp_echo_request_t); i++)
        data[i] = i;
    /*
     * Now compute checksum
     */
    chksum = net_compute_checksum((unsigned short*) buffer, buf_len);
    hdr->checksum = htons(chksum);
}

/*
 * Print an IP address (in network byte order) using printf
 */
void net_print_ip(u32 ip_address) {
       printf("%d.%d.%d.%d", ip_address & 0xFF, (ip_address >> 8) & 0xFF, (ip_address >> 16) & 0xFF, (ip_address >> 24) & 0xFF);
}

void wait_for_echo(int fd) {
    /*
     * The answer will contain the IP header as well. So reserve an additional 20 bytes
     */
    unsigned char buffer[276];
    icmp_hdr_t* hdr;
    ipv4_hdr_t* ip_hdr;
    icmp_echo_request_t* reply;
    int rc;
    int i;
    int done = 0;
    int valid = 1;
    unsigned short chksum;
    while (0 == done) {
        errno = 0;
        for (i = 0; i < 276; i++)
            buffer[i] = 0;
        if ((rc = recv(fd, buffer, 276, 0)) < 0) {
            perror("Could not receive data");
        }
        /*
         * Check whether the message is a valid reply
         */
        printf("Received ICMP message (rc = %d, errno = %d), validating content\n", rc, errno);
        if (rc < 276) {
            printf("Packet too short (rc = %d, errno = %d)\n", rc, errno);
            valid = 0;
        }
        if (valid) {
            ip_hdr = (ipv4_hdr_t*) buffer;
            if (ip_hdr->version != 0x45)  {
                printf("Invalid IP version\n");
                valid = 0;
            }
            if (ntohs(ip_hdr->length) != 276) {
                printf("IP data length too short (got %d, expected 276)\n", ntohs(ip_hdr->length));
                valid = 0;
            }
            if (0 == valid) {
                /*
                 * Print some fields from the IP header
                 */
                printf("IP protocol: %d\n", ip_hdr->proto);
                printf("IP destination address: ");
                net_print_ip(ip_hdr->ip_dest);
                printf("\n");
                printf("IP source address: ");
                net_print_ip(ip_hdr->ip_src);
                printf("\n");
                /*
                 * Might be destination unreachable ICMP message (type 0x3)
                 */
                if (ip_hdr->proto == 0x1) {
                    hdr = (icmp_hdr_t*) (buffer + sizeof(ipv4_hdr_t));
                    if (0x3 == hdr->type) {
                        printf("ICMP message type: Destination unreachable\n");
                    }
                }
            }
        }
        if (valid) {
            hdr = (icmp_hdr_t*) (buffer + 20);
            chksum = net_compute_checksum((unsigned short*) (buffer+20), 256);
            if (chksum != 0) {
                printf("Invalid checksum (have %x, expected 0)\n", chksum);
                valid = 0;
            }
        }
        if (valid) {
            if ((hdr->code) || (hdr->type != ICMP_ECHO_REPLY)) {
                printf("Invalid combination of code / type\n");
                valid = 0;
            }
        }
        if (valid) {
            reply = (icmp_echo_request_t*) (buffer + 20 + sizeof(icmp_hdr_t));
            if (reply->id != htons(getpid())) {
                valid = 0;
                printf("Invalid ID number\n");
            }
            if (reply->seq_no != htons(1)) {
                valid = 0;
                printf("Invalid sequence number\n");
            }
        }
        if (valid) {
            for (i = 0; i < 256 - sizeof(icmp_hdr_t) - sizeof(icmp_echo_request_t); i++) {
                if (i != buffer[i + sizeof(ipv4_hdr_t) + sizeof(icmp_hdr_t) + sizeof(icmp_echo_request_t)]) {
                    valid = 0;
                    printf("Invalid byte at position %d\n", i);
                    break;
                }
            }
        }
        if (valid == 1) {
            printf("Message valid\n");
            done = 1;
        }
        else {
            printf("Message not valid\n");
        }
    }
}


void main(int argc, char** argv) {
    in_addr_t dest_addr;
    unsigned char buffer[256];
    int fd;
    struct sockaddr_in dest;
    struct sockaddr_in src;
    /*
     * Convert the argument into an IP address
     */
    printf("Checking arguments\n");
    if (argc <= 1) {
        printf("Usage: ipsamples  <dst_address>\n");
        _exit(0);
    }
    dest_addr = inet_addr((const char*) argv[1]);
    dest.sin_addr.s_addr = dest_addr;
    dest.sin_family = AF_INET;
    dest.sin_port = 0;
    /*
     * Now open a raw IP socket for the address family AF_INET. Using IPPROTO_ICMP
     * implies that we will receive only packets with IP protocol type ICMP
     */
    printf("Trying to open raw IP socket\n");
    fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (fd < 0) {
        perror("Could not open raw socket");
    }
    printf("Successfully opened socket\n");
    /*
     * Build ICMP packet
     */
    printf("Building ICMP packet\n");
    create_ping(buffer, 256);
    /*
     * Bind socket to a local address
     */
#ifdef DO_BIND
    src.sin_addr.s_addr = inet_addr("192.168.178.20");
    src.sin_family = AF_INET;
    src.sin_port = 0;
    if (bind(fd, (struct sockaddr*)&src, sizeof(struct sockaddr_in)) < 0) {
        perror("Could not bind socket to local address");
        _exit(1);
    }
#endif
    /*
     * Connect socket
     */
#ifdef DO_CONNECT
    if (connect(fd, (struct sockaddr*)&dest, sizeof(struct sockaddr_in)) < 0) {
        perror("Could not connect socket");
        _exit(1);
    }
#endif
    /*
     * send packet - this will add the IP header as we have not set IP_HDRINCL
     */
    printf("Sending ICMP ECHO request\n");
#ifndef DO_SENDTO
    if (send(fd, buffer, 256, 0) < 0) {
        perror("Could not send data");
    }
#endif
#ifdef DO_SENDTO
    if (sendto(fd, buffer, 256, 0, (struct sockaddr*)&dest, sizeof(struct sockaddr_in)) < 0) {
        perror("Could not send data using sendto");
    }
#endif
    /*
     *
     * Try to receive an answer
     */
    printf("Waiting for ECHO reply\n");
    wait_for_echo(fd);
    /*
     * Loop to give us some time to look at netstat output
     */
    while(1) {
        sleep(1);
    }
}


