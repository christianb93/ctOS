/*
 * udp.h
 *
 */

#ifndef _UDP_H_
#define _UDP_H_

#include "net.h"

/*
 * This is a UDP header
 */
typedef struct {
    u16 src_port;        // Source port
    u16 dst_port;        // Destination port
    u16 length;          // Length in bytes, including header
    u16 chksum;          // checksum, competed over pseudo-header, header and payload
} __attribute__ ((packed)) udp_hdr_t;

/*
 * Start of ephemeral port range
 */
#define UDP_EPHEMERAL_PORT 49152

/*
 * Upper limit on receive buffer
 */
#define UDP_RECVBUFFER_SIZE 65536

void udp_init();
int udp_create_socket(socket_t* socket, int domain, int proto);
void udp_rx_msg(net_msg_t* net_msg);

#endif /* _UDP_H_ */
