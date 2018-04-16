/*
 * resolv.h
 *
 */

#ifndef _RESOLV_H_
#define _RESOLV_H_

#include "../netdb.h"


/*
 * QTYPE values
 */
#define QTYPE_A      1                    // host address
#define QTYPE_NS     2                    // authorative name server
#define QTYPE_CNAME  5                    // the canonical name for an alias
#define QTYPE_ANY    255                  // request all records

/*
 * QCLASS values
 */
#define QCLASS_IN 1                       // internet

/*
 * Size in byte of QTYPE, QCLASS, RDLENGTH and TTL
 */
#define CLASS_BYTES 2
#define TYPE_BYTES 2
#define TTL_BYTES 4
#define RDLENGTH_BYTES 2

/*
 * Maximum size of a domain, including trailing zero
 */
#define MAX_DOMAIN_SIZE 256

/*
 * Maximum size of a DNS message
 */
#define MAX_DNS_MSG_SIZE 512

/*
 * Number of attemps we make when trying to resolve a message
 */
#define DNS_RESOLV_ATTEMPTS 5

/*
 * A DNS header - see RFC 1035. This only works for little-endian machines where GCC places
 * the first bits in the structure into the LSB of the respective byte
 */
typedef struct {
    unsigned short id;                // Identifier used to match request and reply
    unsigned char rd:1;               // recursion desired
    unsigned char tc:1;               // message truncated
    unsigned char aa:1;               // reply is authorative answer
    unsigned char opcode:4;           // type of query
    unsigned char qr:1;               // Message is query (0) or response (1)
    unsigned char rcode:4;            // Response code
    unsigned char z:3;                // reserved
    unsigned char ra:1;               // Recursion available
    unsigned short qdcount;           // number of entries in the question section
    unsigned short ancount;           // number of entries in the answer section
    unsigned short nscount;           // number of entries in the authority record section
    unsigned short arcount;           // number of entries in the additional record section
} dns_header_t;


/*
 * This structure is used to store a ressource record internally. This is not the layout in
 * a DNS message, but the result of parsing the message
 */
typedef struct _dns_rr_t {
    unsigned char owner[MAX_DOMAIN_SIZE];              // owner name of the ressource record
    unsigned short type;                               // type of ressource record in host byte order
    unsigned short class;                              // class of ressource record in host byte order
    unsigned char cname[MAX_DOMAIN_SIZE];              // the CNAME for CNAME RR records
    unsigned char nsdname[MAX_DOMAIN_SIZE];            // the NSDAME for NS RR records
    unsigned int address;                              // the IP address for A RR records in network byte order
    struct _dns_rr_t* next;
} dns_rr_t;

int __ctOS_dns_send_request(int fd, unsigned char* host, struct sockaddr_in* dest, int rd, int id);
int __ctOS_dns_parse_name(unsigned char* msg, int offset, int len, unsigned char* domain);
int __ctOS_dns_parse_rr_section(unsigned char* msg, int len, int offset, int entries, dns_rr_t** result_list);
int __ctOS_dns_parse_reply(unsigned char* msg, int len, dns_rr_t** result_list);
int __ctOS_dns_resolv(unsigned char* host, unsigned int* addr, struct sockaddr_in* ns);


#endif /* _RESOLV_H_ */
