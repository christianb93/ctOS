/*
 * ip.h
 *
 */

#ifndef _IP_H_
#define _IP_H_

#include "lib/os/route.h"

/*
 * This structure is an IP message header
 */
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
 * This structure defines a hole in an IP datagram undergoing reassembly
 * (see RFC 815)
 */
typedef struct _ip_datagram_hole_t {
    unsigned short first;               // First octet in hole
    unsigned short last;                // Last octet in hole (-1 <-> infinity)
    unsigned short prev;                // Offset of previous hole
    unsigned short next;                // Offset of next hole
} __attribute__ ((packed)) hole_t;

/*
 * Size of reassembly buffer. As the maximum size of an IP message is limited to 65535 bytes by the
 * fact that the length field in the header is only 2 bytes, a datagram can be at most 65535 - 20 = 65515
 * bytes long. As our implementation passes a valid IP header to the transport layer at all times, this
 * is therefore the limit which we need for the fragment buffer
 */
#define IP_FRAGMENT_MAX_SIZE 65515

/*
 * This is a reassembly slot which contains all the data for reassembly of
 * a particular datagram
 */
typedef struct {
    hole_t* hole_list_head;                        // Head of hole list
    hole_t* hole_list_tail;                        // Tail of hole list
    unsigned int ip_src;                           // IP source address (in network byte order)
    unsigned int ip_dst;                           // IP destination address (in network byte order)
    unsigned int id;                               // ID field
    unsigned char ip_proto;                        // IP proto
    unsigned char buffer[IP_FRAGMENT_MAX_SIZE];    // Reassembly buffer
    int payload_length;                            // Payload length of reassembled message
    unsigned int used;                             // Is slot in use?
    int timeout;                                   // Timeout
} reassembly_slot_t;

/*
 * A routing table entry
 */
typedef struct _route_t {
    struct rtentry  rt_entry;                      // this is the part of the routing table entry visible to applications
    nic_t* nic;                                    // outgoing interface
} route_t;

/*
 * Number of available reassembly slots. We use 16 slots at the moment, i.e. our buffers consume
 * 16*64k = 1M of memory, as every buffer is designed for reassembly of a maximum size IP datagram
 */
#define NR_REASSEMBLY_SLOTS 16

/*
 * Reassembly timeout (seconds) - as suggested in RFC 791
 */
#define REASSEMBLY_TIMEOUT 15
/*
 * Value used to indicate "infinity" for a hole
 */
#define HOLE_INF (IP_FRAGMENT_MAX_SIZE - 1)

/*
 * Some transport protocols
 */
#define IP_PROTO_ICMP 0x1
#define IP_PROTO_UDP 0x11
#define IP_PROTO_TCP 0x6

/*
 * Default netmask for standard network classes
 */
#define NETMASK_CLASS_A 0xff
#define NETMASK_CLASS_B 0xffff
#define NETMASK_CLASS_C 0xffffff

/*
 * Default IP TTL
 */
#define IP_DEFAULT_TTL 64

/*
 * Size of receive buffer for raw IP sockets
 */
#define IP_RCV_BUFFER_SIZE ((16*65536))

/*
 * IPv4 version number
 */
#define IPV4_VERSION 0x4

/*
 * Default header length in dwords
 */
#define IPV4_HDR_LENGTH 5

void ip_init();
void ip_do_tick();
void ip_rx_msg(net_msg_t* net_msg);
int ip_tx_msg(net_msg_t* net_msg);
u32 ip_get_src_addr(u32 ip_dst);
int ip_get_mtu(u32 ip_src);
int ip_add_route(struct rtentry* rt_entry);
int ip_del_route(struct rtentry* rt_entry);
void ip_purge_nic(nic_t* nic);
nic_t* ip_get_route(u32 ip_src, u32 ip_dst, u32* next_hop);
int ip_get_rtconf(struct rtconf* rtc);
void ip_print_routing_table();
int ip_create_socket(socket_t* socket, int domain, int proto);
void ip_test();

#endif /* _IP_H_ */
