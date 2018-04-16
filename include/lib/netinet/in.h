/*
 * in.h
 */

#ifndef _IN_H_
#define _IN_H_

#include "../sys/socket.h"
#include "../inttypes.h"
#include "../arpa/inet.h"

typedef unsigned short in_port_t;
#ifndef _IN_ADDR_DEFINED
#define _IN_ADDR_DEFINED
typedef unsigned int in_addr_t;
#endif

#ifndef _SA_FAMILY_T_DEFINED
#define _SA_FAMILY_T_DEFINED
typedef unsigned short sa_family_t;
#endif

#ifndef _STRUCT_IN_ADDR_DEFINED
#define _STRUCT_IN_ADDR_DEFINED
struct in_addr {
    in_addr_t s_addr;
};
#endif

struct sockaddr_in {
    sa_family_t  sin_family;   // AF_INET.
    in_port_t sin_port;        // Port number - 2 bytes
    struct in_addr sin_addr;   // IP address - 4 bytes
    unsigned char sin_zero[8]; // filler to reach length of 16 bytes in total
};

/*
 * IP protocols
 */
#define IPPROTO_IP 0
#define IPPROTO_ICMP 1
#define IPPROTO_RAW 255
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17

/*
 * Special IP addresses
 */
#define INADDR_ANY ((in_addr_t) 0x00000000)
#define INADDR_BROADCAST ((in_addr_t) 0xffffffff)

/*
 * Internet address string length
 */
#define INET_ADDRSTRLEN 16
#endif /* _IN_H_ */
