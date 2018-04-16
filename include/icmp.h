/*
 * icmp.h
 *
 */

#ifndef _ICMP_H_
#define _ICMP_H_

#include "net.h"

/*
 * This is an ICMP header
 */
typedef struct {
    u8 type;                // Type of message
    u8 code;                // Message code
    u16 checksum;           // Header checksum
} __attribute__ ((packed)) icmp_hdr_t;

/*
 * The body of an ECHO request message
 */
typedef struct {
    u16 id;                 // Identifier
    u16 seq_no;             // Sequence number
} __attribute__ ((packed)) icmp_echo_request_t;


/*
 * Size of ICMP secondary header in bytes - this is the first few
 * bytes after the ICMP header which contains additional data
 */
#define ICMP_SECOND_HDR_SIZE 4

/*
 * ICMP message types
 */
#define ICMP_ECHO_REPLY 0
#define ICMP_DEST_UNREACH 3
#define ICMP_ECHO_REQUEST 8

/*
 * ICMP message codes
 */
#define ICMP_CODE_NONE 0
#define ICMP_CODE_NET_UNREACH 0
#define ICMP_CODE_PORT_UNREACH 3

/*
 * Number of octets after the IP header returned in an ICMP error message
 * According to RFC 1122, this should at least be 8 but can be more
 */
#define ICMP_ERROR_OCTETS 8

void icmp_rx_msg(net_msg_t* net_msg);
void icmp_send_error(net_msg_t* net_msg, int code, int type);

#endif /* _ICMP_H_ */
