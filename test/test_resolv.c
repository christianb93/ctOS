/*
 * test_resolv.c
 *
 */

#include <sys/socket.h>
#include <arpa/inet.h>
#include "kunit.h"
#include "lib/os/resolv.h"

int h_errno;

/*
 * Testcase 1: send a DNS resolution request for www.kernel.org requesting recursion
 * Verify correct layout of generated message
 */
int testcase1() {
    int fd;
    int rc;
    int i;
    unsigned char buffer[512];
    struct sockaddr_in dest;
    /*
     * Put together destination
     */
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = inet_addr("127.0.0.1");
    dest.sin_port = ntohs(30000);
    /*
     * Open UDP socket
     */
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        return -1;
    }
    /*
     * And bind it to local address
     */
    ASSERT(0 == bind(fd, (struct sockaddr*) &dest, sizeof(struct sockaddr_in)));
    /*
     * Send request
     */
    __ctOS_dns_send_request(fd, "www.kernel.org", &dest, 1, getpid());
    /*
     * We should now be able to read the request from the socket
     */
    rc = recv(fd, buffer, 512, 0);
    ASSERT(32 == rc);
    /*
     * Parse data. First part is the header. The first two bytes should be our ID in network byte order
     */
    ASSERT(buffer[0]*256 + buffer[1] == getpid());
    /*
     * The next byte contains the following bits:
     * Bit 0 - RD - should be 1
     * Bit 1 - TC - should be 0
     * Bit 2 - AA - should be 0
     * Bit 3,4,5,6 - OPCODE - should be 0
     * Bit 7 - QR - should be zero
     */
    ASSERT(0x1 == buffer[2]);
    /*
     * Next byte:
     * Bits 0 - 3 - RCODE - should be 0 for a request
     * Bits 4 - 6 - Z - should be zero
     * Bit 7 - RA - should be cleared in a request
     */
    ASSERT(0x0 == buffer[3]);
    /*
     * Next two octets are QDCOUNT - should be 1
     */
    ASSERT(buffer[4]*256 + buffer[5] == 1);
    /*
     * ANCOUNT
     */
    ASSERT(buffer[6]*256 + buffer[7] == 0);
    /*
     * NSCOUNT
     */
    ASSERT(buffer[8]*256 + buffer[9] == 0);
    /*
     * ARCOUNT
     */
    ASSERT(buffer[10]*256 + buffer[11] == 0);
    /*
     * Starting at byte 12, there should be the QNAME of the query, stored as follows:
     * 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27
     * 3  w  w  w  6  k  e  r  n  e  l  3  o  r  g  0
     */
    ASSERT(3 == buffer[12]);
    ASSERT(0 == strncmp("www", buffer + 13, 3));
    ASSERT(6 == buffer[16]);
    ASSERT(0 == strncmp("kernel", buffer + 17, 6));
    ASSERT(3 == buffer[23]);
    ASSERT(0 == strncmp(buffer + 24, "org", 3));
    ASSERT(0 == buffer[27]);
    /*
     * Next two bytes are QTYPE - should be A
     */
    ASSERT(buffer[28]*256 + buffer[29] == 1);
    /*
     * and QCLASS - should be IN
     */
    ASSERT(buffer[30]*256 + buffer[31] == 1);
    /*
     * close socket
     */
    close(fd);
    return 0;
}

/*
 * Testcase 2: send a DNS resolution request for www.kernel.org. not requesting recursion
 * Verify correct layout of generated message
 */
int testcase2() {
    int fd;
    int rc;
    int i;
    unsigned char buffer[512];
    struct sockaddr_in dest;
    /*
     * Put together destination
     */
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = inet_addr("127.0.0.1");
    dest.sin_port = ntohs(30000);
    /*
     * Open UDP socket
     */
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        return -1;
    }
    /*
     * And bind it to local address
     */
    ASSERT(0 == bind(fd, (struct sockaddr*) &dest, sizeof(struct sockaddr_in)));
    /*
     * Send request
     */
    __ctOS_dns_send_request(fd, "www.kernel.org.", &dest, 0, getpid());
    /*
     * We should now be able to read the request from the socket
     */
    rc = recv(fd, buffer, 512, 0);
    ASSERT(32 == rc);
    /*
     * Parse data. First part is the header. The first two bytes should be our ID in network byte order
     */
    ASSERT(buffer[0]*256 + buffer[1] == getpid());
    /*
     * The next byte contains the following bits:
     * Bit 0 - RD - should be 0
     * Bit 1 - TC - should be 0
     * Bit 2 - AA - should be 0
     * Bit 3,4,5,6 - OPCODE - should be 0
     * Bit 7 - QR - should be zero
     */
    ASSERT(0x0 == buffer[2]);
    /*
     * Next byte:
     * Bits 0 - 3 - RCODE - should be 0 for a request
     * Bits 4 - 6 - Z - should be zero
     * Bit 7 - RA - should be cleared in a request
     */
    ASSERT(0x0 == buffer[3]);
    /*
     * Next two octets are QDCOUNT - should be 1
     */
    ASSERT(buffer[4]*256 + buffer[5] == 1);
    /*
     * ANCOUNT
     */
    ASSERT(buffer[6]*256 + buffer[7] == 0);
    /*
     * NSCOUNT
     */
    ASSERT(buffer[8]*256 + buffer[9] == 0);
    /*
     * ARCOUNT
     */
    ASSERT(buffer[10]*256 + buffer[11] == 0);
    /*
     * Starting at byte 12, there should be the QNAME of the query, stored as follows:
     * 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27
     * 3  w  w  w  6  k  e  r  n  e  l  3  o  r  g  0
     */
    ASSERT(3 == buffer[12]);
    ASSERT(0 == strncmp("www", buffer + 13, 3));
    ASSERT(6 == buffer[16]);
    ASSERT(0 == strncmp("kernel", buffer + 17, 6));
    ASSERT(3 == buffer[23]);
    ASSERT(0 == strncmp(buffer + 24, "org", 3));
    ASSERT(0 == buffer[27]);
    /*
     * Next two bytes are QTYPE - should be A
     */
    ASSERT(buffer[28]*256 + buffer[29] == 1);
    /*
     * and QCLASS - should be IN
     */
    ASSERT(buffer[30]*256 + buffer[31] == 1);
    /*
     * close socket
     */
    close(fd);
    return 0;
}

/*
 * Testcase 3: send a DNS resolution request for www.kernel.org requesting recursion to the local
 * nameserver - this does not do any validations, but can be used to check the request via Wireshark
 */
int testcase3() {
    int fd;
    int rc;
    int i;
    unsigned char buffer[512];
    struct sockaddr_in dest;
    /*
     * Put together destination
     */
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = inet_addr("127.0.0.1");
    dest.sin_port = ntohs(53);
    /*
     * Open UDP socket
     */
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        return -1;
    }
    /*
     * Send request
     */
    __ctOS_dns_send_request(fd, "www.kernel.org", &dest, 1, getpid());
    /*
     * close socket
     */
    close(fd);
    return 0;
}

/*
 * Testcase 4: test parsing of labels
 */
int testcase4() {
    unsigned char msg[512];
    unsigned char domain[256];
    memset((void*) msg, 0, 512);
    /*
     * We store berkeley.edu
     */
    msg[12] = strlen("berkeley");
    strncpy(msg + 13, "berkeley", strlen("berkeley"));
    msg[13 + strlen("berkeley")] = strlen("edu");
    strncpy(msg + 12 + strlen("berkeley") + 2, "edu", strlen("edu"));
    /*
     * Now parse this. Note that we expect the label to be 14 bytes long:
     * 8 = strlen("berkeley")
     * 3 = strlen("edu")
     * 1 for the trailing space
     * 1 for the length field preceding "berkeley"
     * 1 for the length field preceding "edu"
     */
    memset((void*) domain, 0, 256);
    ASSERT(14 == __ctOS_dns_parse_name(msg, 12, 512, domain));
    ASSERT(0 == strcmp("berkeley.edu", domain));
    return 0;
}

/*
 * Testcase 5: parse a resource record (RR) of type A
 */
int testcase5() {
    int rc;
    dns_rr_t* result_list = 0;
    unsigned short* type;
    unsigned short* class;
    unsigned int* ttl;
    unsigned int* rdata;
    unsigned short* rdlength;
    unsigned char msg[512];
    unsigned char* section = msg + sizeof(dns_header_t);
    memset((void*) msg, 0, 512);
    /*
     * First the name - we store berkeley.edu
     */
    section[0] = strlen("berkeley");
    strncpy(section + 1, "berkeley", strlen("berkeley"));
    section[1 + strlen("berkeley")] = strlen("edu");
    strncpy(section +  strlen("berkeley") + 2, "edu", strlen("edu"));
    /*
     * The NAME takes up 8 + 3 + 3 = 14 bytes. Following the name, there is the
     * the TYPE and the CLASS --> 18 bytes
     */
    type = (unsigned short*) (section + 14);
    *type = ntohs(1);
    class = (unsigned short*) (section + 16);
    *class = ntohs(1);
    /*
     * 4 byte TTL --> 22 bytes
     */
    ttl = (unsigned int*) (section + 18);
    *ttl = ntohl(64);
    /*
     * Now rdlength --> 24 bytes
     */
    rdlength = (unsigned short*)(section + 22);
    *rdlength = ntohs(4);
    /*
     * and data --> 28 bytes
     */
    rdata = (unsigned int*) (section + 24);
    *rdata = inet_addr("10.0.2.21");
    /*
     * Now try to parse
     */
    ASSERT(28 == __ctOS_dns_parse_rr_section(section, 28, 0, 1, &result_list));
    /*
     * result_list should now point to a one-element list
     */
    ASSERT(result_list);
    ASSERT(0 == result_list->next);
    /*
     * Check type and class
     */
    ASSERT(1 == result_list->class);
    ASSERT(1 == result_list->type);
    /*
     * Check address and name
     */
    ASSERT(inet_addr("10.0.2.21") == result_list->address);
    ASSERT(0 == strcmp("berkeley.edu", result_list->owner));
    return 0;
}

/*
 * Testcase 6: parse a section containing two resource records (RR) of type A
 */
int testcase6() {
    int rc;
    dns_rr_t* result_list = 0;
    unsigned short* type;
    unsigned short* class;
    unsigned int* ttl;
    unsigned int* rdata;
    unsigned short* rdlength;
    unsigned char msg[512];
    unsigned char* section = msg + sizeof(dns_header_t);
    unsigned char* rr = section;
    memset((void*) msg, 0, 512);
    /*
     * First the of the first RR name - we store berkeley.edu
     */
    rr[0] = strlen("berkeley");
    strncpy(rr + 1, "berkeley", strlen("berkeley"));
    rr[1 + strlen("berkeley")] = strlen("edu");
    strncpy(rr +  strlen("berkeley") + 2, "edu", strlen("edu"));
    /*
     * The NAME takes up 8 + 3 + 3 = 14 bytes. Following the name, there is the
     * the TYPE and the CLASS --> 18 bytes
     */
    type = (unsigned short*) (rr + 14);
    *type = ntohs(1);
    class = (unsigned short*) (rr + 16);
    *class = ntohs(1);
    /*
     * 4 byte TTL --> 22 bytes
     */
    ttl = (unsigned int*) (rr + 18);
    *ttl = ntohl(64);
    /*
     * Now rdlength --> 24 bytes
     */
    rdlength = (unsigned short*)(rr + 22);
    *rdlength = ntohs(4);
    /*
     * and data --> 28 bytes
     */
    rdata = (unsigned int*) (rr + 24);
    *rdata = inet_addr("10.0.2.21");
    /*
     * Now we are done with the first RR and to the second one. Again we start with the name, this
     * time we use ucla.org
     */
    rr = section + 28;
    rr[0] = strlen("ucla");
    strncpy(rr + 1, "ucla", strlen("ucla"));
    rr[1 + strlen("ucla")] = strlen("edu");
    strncpy(rr +  strlen("ucla") + 2, "edu", strlen("edu"));
    /*
      * The NAME takes up 4 + 3 + 3 = 10 bytes. Following the name, there is the
      * the TYPE and the CLASS --> 14 bytes.
      */
     type = (unsigned short*) (rr + 10);
     *type = ntohs(1);
     class = (unsigned short*) (rr + 12);
     *class = ntohs(1);
     /*
      * 4 byte TTL --> 18 bytes
      */
     ttl = (unsigned int*) (rr + 14);
     *ttl = ntohl(64);
     /*
      * Now rdlength --> 20 bytes
      */
     rdlength = (unsigned short*)(rr + 18);
     *rdlength = ntohs(4);
     /*
      * and data --> 24 bytes
      */
     rdata = (unsigned int*) (rr + 20);
     *rdata = inet_addr("10.0.2.22");
    /*
     * Now try to parse. Our section now has 28 + 24 = 52 octets
     */
     result_list = 0;
     ASSERT(52 == __ctOS_dns_parse_rr_section(section, 52, 0, 2, &result_list));
     /*
      * result_list should now point to a two-element list
      */
     ASSERT(result_list);
     ASSERT(result_list->next);
     /*
      * Check type and class
      */
     ASSERT(1 == result_list->class);
     ASSERT(1 == result_list->type);
     /*
      * Check address and name
      */
     ASSERT(inet_addr("10.0.2.21") == result_list->address);
     ASSERT(0 == strcmp("berkeley.edu", result_list->owner));
     /*
      * Now check second entry
      */
     ASSERT(0 == result_list->next->next);
     ASSERT(1 == result_list->next->class);
     ASSERT(1 == result_list->next->type);
     ASSERT(inet_addr("10.0.2.22") == result_list->next->address);
     ASSERT(0 == strcmp("ucla.edu", result_list->next->owner));
     return 0;
}

/*
 * Testcase 7: parse a DNS reply
 */
int testcase7() {
    int rc;
    dns_header_t* hdr;
    dns_rr_t* result_list = 0;
    unsigned short* type;
    unsigned short* class;
    unsigned int* ttl;
    unsigned int* rdata;
    unsigned short* rdlength;
    unsigned char msg[512];
    unsigned char* section;
    memset((void*) msg, 0, 512);
    /*
     * Prepare header
     */
    hdr = (dns_header_t*) msg;
    hdr->aa = 0;
    hdr->ancount = htons(1);
    hdr->arcount = 0;
    hdr->id = ntohs(1);
    hdr->nscount = 0;
    hdr->opcode = 0;
    hdr->qdcount = htons(1);
    hdr->qr = 1;
    hdr->ra = 0;
    hdr->rd = 1;
    hdr->rcode = 0;
    hdr->tc = 0;
    hdr->z = 0;
    /*
     * Now assemble question
     */
    msg[12] = strlen("berkeley");
    strncpy(msg + 13, "berkeley", strlen("berkeley"));
    msg[13 + strlen("berkeley")] = strlen("edu");
    strncpy(msg + 12 + strlen("berkeley") + 2, "edu", strlen("edu"));
    /*
     * Finally store the TYPE and CLASS, starting at position 12 + 14 = 26
     */
    *((unsigned short*)(msg + 26)) = htons(1);
    *((unsigned short*)(msg + 28)) = htons(1);
    /*
     * Now construction answer section, starting at offset 30
     */
    section = msg + 30;
    /*
     * First the name - we store berkeley.edu
     */
    section[0] = strlen("berkeley");
    strncpy(section + 1, "berkeley", strlen("berkeley"));
    section[1 + strlen("berkeley")] = strlen("edu");
    strncpy(section +  strlen("berkeley") + 2, "edu", strlen("edu"));
    /*
     * The NAME takes up 8 + 3 + 3 = 14 bytes. Following the name, there is the
     * the TYPE and the CLASS --> 18 bytes
     */
    type = (unsigned short*) (section + 14);
    *type = ntohs(1);
    class = (unsigned short*) (section + 16);
    *class = ntohs(1);
    /*
     * 4 byte TTL --> 22 bytes
     */
    ttl = (unsigned int*) (section + 18);
    *ttl = ntohl(64);
    /*
     * Now rdlength --> 24 bytes
     */
    rdlength = (unsigned short*)(section + 22);
    *rdlength = ntohs(4);
    /*
     * and data --> 28 bytes
     */
    rdata = (unsigned int*) (section + 24);
    *rdata = 0x1502000a;
    /*
     * Now try to parse
     */
    ASSERT( 0 == __ctOS_dns_parse_reply(msg, 512, &result_list));
    /*
     * Result list should be one element
     */
    ASSERT(result_list);
    ASSERT(0 == result_list->next);
    /*
     * This element should have type A, class IN and address "10.0.2.21"
     */
    ASSERT(1 == result_list->type);
    ASSERT(1 == result_list->class);
    ASSERT(inet_addr("10.0.2.21") == result_list->address);
    ASSERT(0 == strcmp(result_list->owner, "berkeley.edu"));
    return 0;
}

/*
 * Testcase 8: parse a DNS reply using compression
 */
int testcase8() {
    int rc;
    dns_header_t* hdr;
    dns_rr_t* result_list = 0;
    unsigned short* type;
    unsigned short* class;
    unsigned int* ttl;
    unsigned int* rdata;
    unsigned short* rdlength;
    unsigned char msg[512];
    unsigned char* section;
    memset((void*) msg, 0, 512);
    /*
     * Prepare header
     */
    hdr = (dns_header_t*) msg;
    hdr->aa = 0;
    hdr->ancount = htons(1);
    hdr->arcount = 0;
    hdr->id = ntohs(1);
    hdr->nscount = 0;
    hdr->opcode = 0;
    hdr->qdcount = htons(1);
    hdr->qr = 1;
    hdr->ra = 0;
    hdr->rd = 1;
    hdr->rcode = 0;
    hdr->tc = 0;
    hdr->z = 0;
    /*
     * Now assemble question
     */
    msg[12] = strlen("berkeley");
    strncpy(msg + 13, "berkeley", strlen("berkeley"));
    msg[13 + strlen("berkeley")] = strlen("edu");
    strncpy(msg + 12 + strlen("berkeley") + 2, "edu", strlen("edu"));
    /*
     * Finally store the TYPE and CLASS, starting at position 12 + 14 = 26
     */
    *((unsigned short*)(msg + 26)) = htons(1);
    *((unsigned short*)(msg + 28)) = htons(1);
    /*
     * Now construction answer section, starting at offset 30
     */
    section = msg + 30;
    /*
     * First the name - we store berkeley.edu, but use compression. Recall
     * that the label "berkeley" is contained in the question section, i.e
     * at offset sizeof(dns_header_t) into the message
     */
    section[0] = 192;
    section[1] = sizeof(dns_header_t);
    /*
     * The NAME takes up 2 bytes. Following the name, there is the
     * the TYPE and the CLASS --> 6 bytes
     */
    type = (unsigned short*) (section + 2);
    *type = ntohs(1);
    class = (unsigned short*) (section + 4);
    *class = ntohs(1);
    /*
     * 4 byte TTL --> 10 bytes
     */
    ttl = (unsigned int*) (section + 6);
    *ttl = ntohl(64);
    /*
     * Now rdlength --> 12 bytes
     */
    rdlength = (unsigned short*)(section + 10);
    *rdlength = ntohs(4);
    /*
     * and data --> 14 bytes
     */
    rdata = (unsigned int*) (section + 12);
    *rdata = 0x1502000a;
    /*
     * Now try to parse
     */
#if 0
    int i;
    for (i = 0; i < 64; i++) {
        if (0 == (i % 8))
            printf("\n%.2x  ", i);
        printf("%.2x ", msg[i]);
    }
#endif
    ASSERT( 0 == __ctOS_dns_parse_reply(msg, 512, &result_list));
    /*
     * Result list should be one element
     */
    ASSERT(result_list);
    ASSERT(0 == result_list->next);
    /*
     * This element should have type A, class IN and address "10.0.2.21"
     */
    ASSERT(1 == result_list->type);
    ASSERT(1 == result_list->class);
    ASSERT(inet_addr("10.0.2.21") == result_list->address);
    ASSERT(0 == strcmp(result_list->owner, "berkeley.edu"));
    return 0;
}

/*
 * Testcase 9: send a DNS resolution request for localhost requesting recursion to the local
 * nameserver and validate result.
 * NOTE: this testcase will only work if there is a nameserver running on UDP port 53 on the local
 * machine (should be the case on most flavours of Unix)
 */
int testcase9() {
    int fd;
    int rc;
    int i;
    dns_rr_t* result_list = 0;
    unsigned char msg[512];
    struct sockaddr_in dest;
    struct sockaddr_in src;
    /*
     * Skip this testcase until further notice 
     * */
     return 0;
    /*
     * Put together destination
     */
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = inet_addr("127.0.0.1");
    dest.sin_port = ntohs(53);
    /*
     * Open UDP socket
     */
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        return -1;
    }
    /*
     * Bind so that we can receive the reply
     */
    src.sin_addr.s_addr = 0;
    src.sin_family = AF_INET;
    src.sin_port = 0;
    if (bind(fd, (struct sockaddr*) &src, sizeof(struct sockaddr_in)) < 0) {
        perror("bind");
        return -1;
    }
    /*
     * Send request
     */
    __ctOS_dns_send_request(fd, "localhost", &dest, 1, getpid());
    /*
     * wait for response
     */
    memset((void*) msg, 0, 512);
    if (recv(fd, msg, 512, 0) < 0) {
        perror("recv");
        return -1;
    }
    /*
     * Now parse message
     */
    ASSERT(0 == __ctOS_dns_parse_reply(msg, 512, &result_list));
    /*
     * Result list should be one element
     */
    ASSERT(result_list);
    ASSERT(0 == result_list->next);
    /*
      * This element should have type A, class IN and address "127.0.0.1"
      */
     ASSERT(1 == result_list->type);
     ASSERT(1 == result_list->class);
     ASSERT(inet_addr("127.0.0.1") == result_list->address);
     ASSERT(0 == strcmp(result_list->owner, "localhost"));
    /*
     * close socket
     */
    close(fd);
    return 0;
}

/*
 * Testcase 10: like testcase 9, but use __ctOS_dns_resolv
 */
int testcase10() {
    struct sockaddr_in dest;
    unsigned int addr;
       /*
     * Skip this testcase until further notice 
     * */
     return 0;
    /*
     * Put together destination
     */
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = inet_addr("127.0.0.1");
    dest.sin_port = ntohs(53);
    /*
     * and resolve
     */
    ASSERT(0 == __ctOS_dns_resolv("localhost", &addr, &dest));
    ASSERT(inet_addr("127.0.0.1") == addr);
    return 0;
}

/*
 * Testcase 11: send a DNS resolution request for a real host requesting recursion to the local
 * nameserver and validate result.
 * NOTE: this testcase will only work if there is a nameserver running on UDP port 53 on the local
 * machine (should be the case on most flavours of Unix)
 */
int testcase11() {
    int fd;
    int rc;
    int i;
    dns_rr_t* result_list = 0;
    unsigned char msg[512];
    struct sockaddr_in dest;
    struct sockaddr_in src;
   /*
     * Skip this testcase until further notice 
     * */
     return 0;    
    /*
     * Put together destination
     */
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = inet_addr("127.0.0.1");
    dest.sin_port = ntohs(53);
    /*
     * Open UDP socket
     */
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        return -1;
    }
    /*
     * Bind so that we can receive the reply
     */
    src.sin_addr.s_addr = 0;
    src.sin_family = AF_INET;
    src.sin_port = 0;
    if (bind(fd, (struct sockaddr*) &src, sizeof(struct sockaddr_in)) < 0) {
        perror("bind");
        return -1;
    }
    /*
     * Send request
     */
    __ctOS_dns_send_request(fd, "www.google.de", &dest, 1, getpid());
    /*
     * wait for response
     */
    memset((void*) msg, 0, 512);
    if (recv(fd, msg, 512, 0) < 0) {
        perror("recv");
        return -1;
    }
    /*
     * Now parse message
     */
    ASSERT(0 == __ctOS_dns_parse_reply(msg, 512, &result_list));
    /*
     * close socket
     */
    close(fd);
    return 0;
}

/*
 * Testcase 12: parse a DNS reply using compression - only a part of a domain name
 * is compressed
 */
int testcase12() {
    int rc;
    dns_header_t* hdr;
    dns_rr_t* result_list = 0;
    unsigned short* type;
    unsigned short* class;
    unsigned int* ttl;
    unsigned int* rdata;
    unsigned short* rdlength;
    unsigned char msg[512];
    unsigned char* section;
    memset((void*) msg, 0, 512);
    /*
     * Prepare header
     */
    hdr = (dns_header_t*) msg;
    hdr->aa = 0;
    hdr->ancount = htons(1);
    hdr->arcount = 0;
    hdr->id = ntohs(1);
    hdr->nscount = 0;
    hdr->opcode = 0;
    hdr->qdcount = htons(1);
    hdr->qr = 1;
    hdr->ra = 0;
    hdr->rd = 1;
    hdr->rcode = 0;
    hdr->tc = 0;
    hdr->z = 0;
    /*
     * Now assemble question
     */
    msg[12] = strlen("berkeley");
    strncpy(msg + 13, "berkeley", strlen("berkeley"));
    msg[13 + strlen("berkeley")] = strlen("edu");
    strncpy(msg + 12 + strlen("berkeley") + 2, "edu", strlen("edu"));
    /*
     * Finally store the TYPE and CLASS, starting at position 12 + 14 = 26
     */
    *((unsigned short*)(msg + 26)) = htons(1);
    *((unsigned short*)(msg + 28)) = htons(1);
    /*
     * Now construction answer section, starting at offset 30
     */
    section = msg + 30;
    /*
     * First the name - we store www.berkeley.edu, and compression for domain and TLD. Recall
     * that the label "berkeley" is contained in the question section, i.e
     * at offset sizeof(dns_header_t) into the message
     */
    section[0] = 3;
    section[1] = 'w';
    section[2] = 'w';
    section[3] = 'w';
    section[4] = 192;
    section[5] = sizeof(dns_header_t);
    /*
     * The NAME takes up 6 bytes. Following the name, there is the
     * the TYPE and the CLASS --> 10 bytes
     */
    type = (unsigned short*) (section + 6);
    *type = ntohs(1);
    class = (unsigned short*) (section + 8);
    *class = ntohs(1);
    /*
     * 4 byte TTL --> 14 bytes
     */
    ttl = (unsigned int*) (section + 10);
    *ttl = ntohl(64);
    /*
     * Now rdlength --> 16 bytes
     */
    rdlength = (unsigned short*)(section + 14);
    *rdlength = ntohs(4);
    /*
     * and data --> 20 bytes
     */
    rdata = (unsigned int*) (section + 16);
    *rdata = 0x1502000a;
    /*
     * Now try to parse
     */
#if 0
    int i;
    for (i = 0; i < 64; i++) {
        if (0 == (i % 8))
            printf("\n%.2x  ", i);
        printf("%.2x ", msg[i]);
    }
#endif
    ASSERT( 0 == __ctOS_dns_parse_reply(msg, 512, &result_list));
    /*
     * Result list should be one element
     */
    ASSERT(result_list);
    ASSERT(0 == result_list->next);
    /*
     * This element should have type A, class IN and address "10.0.2.21"
     */
    ASSERT(1 == result_list->type);
    ASSERT(1 == result_list->class);
    ASSERT(inet_addr("10.0.2.21") == result_list->address);
    ASSERT(0 == strcmp(result_list->owner, "www.berkeley.edu"));
    return 0;
}

/*
 * Testcase 13: send a DNS resolution request for a real host requesting recursion to the local
 * nameserver and validate result - chose a host which has a CNAME (www.kernel.org)
 * NOTE: this testcase will only work if there is a nameserver running on UDP port 53 on the local
 * machine (should be the case on most flavours of Unix)
 */
int testcase13() {
    int fd;
    int rc;
    int i;
    dns_rr_t* result_list = 0;
    unsigned char msg[512];
    struct sockaddr_in dest;
    struct sockaddr_in src;
   /*
     * Skip this testcase until further notice 
     * */
     return 0;    
    /*
     * Put together destination
     */
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = inet_addr("127.0.0.1");
    dest.sin_port = ntohs(53);
    /*
     * Open UDP socket
     */
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        return -1;
    }
    /*
     * Bind so that we can receive the reply
     */
    src.sin_addr.s_addr = 0;
    src.sin_family = AF_INET;
    src.sin_port = 0;
    if (bind(fd, (struct sockaddr*) &src, sizeof(struct sockaddr_in)) < 0) {
        perror("bind");
        return -1;
    }
    /*
     * Send request
     */
    __ctOS_dns_send_request(fd, "www.kernel.org", &dest, 1, getpid());
    /*
     * wait for response
     */
    memset((void*) msg, 0, 512);
    if (recv(fd, msg, 512, 0) < 0) {
        perror("recv");
        return -1;
    }
    /*
     * Now parse message
     */
    ASSERT(0 == __ctOS_dns_parse_reply(msg, 512, &result_list));
    /*
     * close socket
     */
    close(fd);
    return 0;
}

/*
 * Testcase 14: parse a DNS reply containing a CNAME record. We simulate the following record
 * QUESTION SECTION:
 * www.kernel.org     IN       A
 * ANSWER SECTION:
 * www.kernel.org     IN       CNAME   pub.us.kernel.org
 * pub.us.kernel.org  IN       A       149.20.20.133
 *
 */
int testcase14() {
    int rc;
    dns_header_t* hdr;
    dns_rr_t* result_list = 0;
    unsigned short* type;
    unsigned short* class;
    unsigned int* ttl;
    unsigned char* rdata;
    unsigned short* rdlength;
    unsigned char msg[512];
    unsigned char* section;
    memset((void*) msg, 0, 512);
    /*
     * Prepare header
     */
    hdr = (dns_header_t*) msg;
    hdr->aa = 0;
    hdr->ancount = htons(2);
    hdr->arcount = 0;
    hdr->id = ntohs(1);
    hdr->nscount = 0;
    hdr->opcode = 0;
    hdr->qdcount = htons(1);
    hdr->qr = 1;
    hdr->ra = 1;
    hdr->rd = 1;
    hdr->rcode = 0;
    hdr->tc = 0;
    hdr->z = 0;
    /*
     * Now assemble question
     */
    msg[12] = 3;
    strncpy(msg + 13, "www", 3);
    msg[16] = 6;
    strncpy(msg + 17, "kernel", 6);
    msg[23] = 3;
    strncpy(msg + 24, "org", 3);
    msg[27] = 0;
    /*
     * Finally store the TYPE and CLASS, starting at position 28
     */
    *((unsigned short*)(msg + 28)) = htons(1);
    *((unsigned short*)(msg + 30)) = htons(1);
    /*
     * Now construction answer section, starting at offset 32
     */
    section = msg + 32;
    /*
     * First the name - we use compression to refer to www.kernel.org
     * that the domain "www.kernel.org" is contained in the question section and located at offset 12
     */
    section[0] = 192;
    section[1] = 12;
    /*
     * The NAME takes up 2 bytes. Following the name, there is the
     * the TYPE (5 = CNAME) and the CLASS
     */
    type = (unsigned short*) (section + 2);
    *type = ntohs(5);
    class = (unsigned short*) (section + 4);
    *class = ntohs(1);
    /*
     * 4 byte TTL
     */
    ttl = (unsigned int*) (section + 6);
    *ttl = ntohl(64);
    /*
     * Now rdlength
     */
    rdlength = (unsigned short*)(section + 10);
    *rdlength = ntohs(9);
    /*
     * and data. As this is a CNAME record, the RDATA is itself a domain. We store
     * pub.us as label and for kernel.org refer to the instance of kernel.org in
     * the question which starts at offset 16
     */
    rdata = section + 12;
    rdata[0] = 3;
    rdata[1] = 'p';
    rdata[2] = 'u';
    rdata[3] = 'b';
    rdata[4] = 2;
    rdata[5] = 'u';
    rdata[6] = 's';
    rdata[7] = 192;
    rdata[8] = 16;
    /*
     * Now we place the second ressource record which is an IN record for pub.us.kernel.org. Again
     * we use compression here and refer to the instance in the RDATA of the CNAME record
     */
    section = rdata + 9;
    section[0] = 192;
    section[1] = rdata - msg;
    /*
     * The NAME takes up 2 bytes. Following the name, there is the
     * the TYPE (1 = IN) and the CLASS
     */
    type = (unsigned short*) (section + 2);
    *type = ntohs(1);
    class = (unsigned short*) (section + 4);
    *class = ntohs(1);
    /*
     * 4 byte TTL
     */
    ttl = (unsigned int*) (section + 6);
    *ttl = ntohl(64);
    /*
     * Now rdlength
     */
    rdlength = (unsigned short*)(section + 10);
    *rdlength = ntohs(4);
    /*
     * and rdata
     */
    rdata = section + 12;
    *((unsigned int*) rdata) = inet_addr("149.20.20.133");
    /*
     * Now try to parse
     */
#if 0
    int i;
    for (i = 0; i < 70; i++) {
        if (0 == (i % 8))
            printf("\n%.2x  ", i);
        printf("%.2x ", msg[i]);
    }
#endif
    ASSERT(0 == __ctOS_dns_parse_reply(msg, 512, &result_list));
    ASSERT(result_list);
    ASSERT(5 == result_list->type);
    ASSERT(0 == strcmp((char*) result_list->cname, "pub.us.kernel.org"));
    ASSERT(result_list->next);
    ASSERT(1 == result_list->next->type);
    ASSERT(0 == strcmp((char*) result_list->next->owner, "pub.us.kernel.org"));
    ASSERT(inet_addr("149.20.20.133") == result_list->next->address);
    return 0;
}

/*
 * Testcase 15: send a DNS resolution request for a real host requesting recursion to the local
 * nameserver and validate result - chose a host which has a CNAME (www.kernel.org) and use
 * upper cases in the query
 * NOTE: this testcase will only work if there is a nameserver running on UDP port 53 on the local
 * machine (should be the case on most flavours of Unix)
 */
int testcase15() {
    int fd;
    int rc;
    int i;
    dns_rr_t* result_list = 0;
    unsigned char msg[512];
    struct sockaddr_in dest;
    struct sockaddr_in src;
   /*
     * Skip this testcase until further notice 
     * */
     return 0;    
    /*
     * Put together destination
     */
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = inet_addr("127.0.0.1");
    dest.sin_port = ntohs(53);
    /*
     * Open UDP socket
     */
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        return -1;
    }
    /*
     * Bind so that we can receive the reply
     */
    src.sin_addr.s_addr = 0;
    src.sin_family = AF_INET;
    src.sin_port = 0;
    if (bind(fd, (struct sockaddr*) &src, sizeof(struct sockaddr_in)) < 0) {
        perror("bind");
        return -1;
    }
    /*
     * Send request
     */
    __ctOS_dns_send_request(fd, "WWW.KERNEL.ORG", &dest, 1, getpid());
    /*
     * wait for response
     */
    memset((void*) msg, 0, 512);
    if (recv(fd, msg, 512, 0) < 0) {
        perror("recv");
        return -1;
    }
    /*
     * Now parse message
     */
    ASSERT(0 == __ctOS_dns_parse_reply(msg, 512, &result_list));
    /*
     * close socket
     */
    close(fd);
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
    RUN_CASE(12);
    RUN_CASE(13);
    RUN_CASE(14);
    RUN_CASE(15);
    END;
    return 0;
}

