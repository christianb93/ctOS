/*
 * net.h
 */

#ifndef __NET_H_
#define __NET_H_

#include "ktypes.h"
#include "locks.h"
#include "pci.h"
#include "eth.h"
#include "lib/sys/socket.h"
#include "locks.h"
#include "lib/os/if.h"

/*
 * This structure describes a network card
 */
typedef struct _nic_t {
    pci_dev_t* pci_dev;                        // the PCI device representing the card
    int irq_vector;                            // the vector which is tied to the card
    u32 base_address;                          // the I/O base address of the network card
    u8 hw_type;                                // HW type
    u8 mac_address[6];                         // the MAC address
    u32 tx_queued;                             // ring buffer pointer, pointing to first free tx descriptor
    u32 tx_sent;                               // ring buffer pointer, pointing to first used tx descriptor
    u32 rx_read;                               // ring buffer pointer, pointing to next free position in receive buffer
    spinlock_t tx_lock;                        // lock to protect tx_* fields, used to serialize transmission
    spinlock_t rx_lock;                        // lock to protect rx_* fields, used to serialize reception
    u32 ip_addr;                               // IP address of interface
    u32 ip_netmask;                            // IP netmask of the interface
    int ip_addr_assigned;                      // has the interface a valid IP address?
    int mtu;                                   // maximum transfer unit (including IP header, but not link layer header)
    char name[IFNAMSIZ];                       // interface name
    struct _nic_t* next;
    struct _nic_t* prev;
} nic_t;

/*
 * A network stack message
 */
typedef struct _net_msg_t {
    u8* data;                            // Start of the buffer
    u8* start;                           // Pointer to first used byte within buffer
    u8* end;                             // Pointer to first unused byte within buffer
    u32 length;                          // Size of buffer
    nic_t* nic;                          // Network card associated with message
    void* eth_hdr;                       // Pointer to Ethernet header
    void* arp_hdr;                       // Pointer to ARP header
    void* ip_hdr;                        // Pointer to IP header
    void* icmp_hdr;                      // Pointer to ICMP header
    void* tcp_hdr;                       // Pointer to TCP header
    void* udp_hdr;                       // Pointer to UDP header
    mac_address_t hw_dest;               // Destination hardware address
    u16 ethertype;                       // Ethertype
    u16 ip_length;                       // Total length of IP data (i.e. not including the header)
    u8 ip_proto;                         // IP protocol
    u32 ip_dest;                         // IP destination
    u32 ip_src;                          // IP source
    int ip_df;                           // DF (Dont't fragment) IP flag
    struct _net_msg_t* next;
    struct _net_msg_t* prev;
} net_msg_t;

/*
 * The size of a socket send buffer and a receive buffer
 */
#define SND_BUFFER_SIZE 65536
#define RCV_BUFFER_SIZE 8192

/*
 * A TCP timer
 */
typedef struct {
    u32 time;                            // Number of TCP ticks left
    u32 backoff;                         // Used to compute the "exponential backoff" when timer is set again
} tcp_timer_t;

/*
 * This is the tcp specific part of a socket
 */
typedef struct _tcp_socket_t {
    int status;                          // Socket status (closed, connected, ...)
    u32 isn;                             // Initial sequence number
    u32 snd_nxt;                         // sequence number for sending - offset of next byte to be sent
    u32 snd_max;                         // Highest value of snd_nxt so far - used to keep track of old snd_nxt during retranmissions
    u32 snd_una;                         // last unacknowledged sequence number
    u32 rcv_nxt;                         // next expected byte
    u32 snd_wnd;                         // send window as advertised by peer
    u32 rcv_wnd;                         // our current receive window as previously advertised to the peer
    int ref_count;                       // Reference count
    spinlock_t ref_count_lock;           // Lock to protect reference count
    int fin_sent;                        // FIN has been sent to peer
    u8 snd_buffer[SND_BUFFER_SIZE];      // Send buffer
    u32 snd_buffer_head;                 // Head of send buffer
    u32 snd_buffer_tail;                 // Tail of send buffer
    u8 rcv_buffer[RCV_BUFFER_SIZE];      // Receive buffer
    u32 rcv_buffer_head;                 // Head of receive buffer
    u32 rcv_buffer_tail;                 // Tail of receive buffer
    u32 smss;                            // effective maximum segment size when sending
    u32 rmss;                            // effective maximum segment size when receiving
    u32 max_wnd;                         // the maximum window size ever advertised by the peer
    u32 cwnd;                            // congestion window
    u32 right_win_edge;                  // right edge of window as advertised to the peer
    u32 rto;                             // retransmission timeout
    tcp_timer_t rtx_timer;               // retransmission timer
    tcp_timer_t delack_timer;            // Timer for delayed ACKs
    tcp_timer_t persist_timer;           // Persist timer
    tcp_timer_t time_wait_timer;         // Time wait timer
    u32 timed_segment;                   // segment currently timed for RTT estimation
    u32 srtt;                            // current value of smoothed RTT, in units of ticks / ( 1 << SRTT_SHIFT)
    u32 rttvar;                          // RTT variance, in units of ticks / (1 << SRTT_SHIFT)
    int current_rtt;                     // RTT (in ticks) of currently timed segment of -1 if no segment is timed
    int first_rtt;                       // set to 1 if no RTT sample has been taken yet
    u32 ack_count;                       // Acknowledged bytes since last update of congestion window
    u32 ssthresh;                        // Slow start threshold
    u32 tcp_options;                     // Options
    u32 dupacks;                         // counter for duplicate acks - used for fast retransmit
    u32 rtx_count;                       // number of times a segment is retransmitted
    u32 snd_wl1;                         // sequence number of last window update
    u32 snd_wl2;                         // acknowledgement number of last window update
    u32 fin_seq_no;                      // sequence number of FIN
    int timeout;                         // Socket operation has timed out
    int closed;                          // user has issued close operation on the socket
    int epipe;                           // connection has been shutdown and no more data can be sent
    int eof;                             // no more data can be received via this connection (but there might be data in the recv buffer)
    struct _tcp_socket_t* next;
    struct _tcp_socket_t* prev;
} tcp_socket_t;


/*
 * The IP specific part of a socket
 */
typedef struct _ip_socket_t {
    u8 ip_proto;                         // IP proto used when the socket was created
    net_msg_t* rcv_buffer_head;          // Head of receive buffer
    net_msg_t* rcv_buffer_tail;          // Tail of receive buffer
    u32 pending_bytes;                   // Bytes in buffer
    int ref_count;                       // Reference count
    spinlock_t ref_count_lock;           // lock to protect reference count
    struct _ip_socket_t* next;
    struct _ip_socket_t* prev;
} ip_socket_t;

/*
 * The UDP specific part of a socket
 */
typedef struct _udp_socket_t {
    net_msg_t* rcv_buffer_head;          // Head of receive buffer
    net_msg_t* rcv_buffer_tail;          // Tail of receive buffer
    u32 pending_bytes;                   // Bytes in buffer
    int ref_count;                       // Reference count
    spinlock_t ref_count_lock;           // lock to protect reference count
    struct _udp_socket_t* next;
    struct _udp_socket_t* prev;
} udp_socket_t;

/*
 * This structure records a select request
 */
typedef struct _select_req_t {
    semaphore_t* sem;                    // semaphore on which we perform an UP if event occurs
    int event;                           // type of event we are waiting for (NET_EVENT_*)
    int actual_event;                    // event which actually occurred
    struct _select_req_t* next;
    struct _select_req_t* prev;
} select_req_t;

/*
 * A socket
 */
typedef struct _socket_t {
    int bound;                           // Socket has been bound to a local address
    int connected;                       // Connect has been called for this socket and a foreign address has been specified
    int error;                           // last error recorded for this socket (negative  error code)
    struct sockaddr laddr;               // the local address
    struct sockaddr faddr;               // the foreign address
    struct _socket_ops_t* ops;           // operations on the socket
    spinlock_t lock;                     // lock the socket state
    cond_t snd_buffer_change;            // used to inform waiting threads when the buffer state of the socket has changed
    cond_t rcv_buffer_change;            // same for receive buffer
    union {
        tcp_socket_t tcp;                // TCP socket
        ip_socket_t ip;                  // IP socket
        udp_socket_t udp;                // UDP socket
    } proto;
    struct _socket_t* so_queue_head;     // Queue of incoming connections
    struct _socket_t* so_queue_tail;
    u32 max_connection_backlog;          // Maximum number of queued connections
    struct _socket_t* parent;            // If we are on the connection queue of a socket, this is a pointer to it
    select_req_t* select_queue_head;     // select table, i.e. list of select operations waiting for this socket
    select_req_t* select_queue_tail;
    unsigned int so_sndtimeout;          // send timeout in ticks
    unsigned int so_rcvtimeout;          // receive timeout in ticks
    struct _socket_t* next;
    struct _socket_t* prev;
} socket_t;

/*
 * Given a pointer to a TCP socket, get the corresponding generic socket and vice versa
 */
#define TCB2SOCK(tcb)  ((socket_t*)(((void*)(tcb)) - offsetof(socket_t, proto)))
#define SOCK2TCB(socket) (&((socket)->proto.tcp))

/*
 * Same thing for IP socket
 */
#define IPCB2SOCK(ipcb)  ((socket_t*)(((void*)(ipcb)) - offsetof(socket_t, proto)))
#define SOCK2IPCB(socket) (&((socket)->proto.ip))

/*
 * and UDP socket
 */
#define UCB2SOCK(ucb)  ((socket_t*)(((void*)(ucb)) - offsetof(socket_t, proto)))
#define SOCK2UCB(socket) (&((socket)->proto.udp))


/*
 * This structure defines a set of operations which can be performed on
 * a socket
 */
typedef struct _socket_ops_t {
    int (*connect)(socket_t*, struct sockaddr*, int addrlen);        // this is called whenever a socket is connected
    int (*close)(socket_t*, u32* eflags);                            // close a socket
    int (*send)(socket_t*, void*, unsigned int, int);                // send data
    int (*recv)(socket_t*, void*, unsigned int, int);                // receive data
    int (*listen)(socket_t*);                                        // put socket into listen state
    int (*bind)(socket_t*, struct sockaddr*, int);                   // bind socket to local address
    int (*select)(socket_t*, int, int);                              // non-blocking select
    void (*release)(socket_t*);                                      // release reference obtained via create
    int (*sendto)(socket_t*, void*, unsigned int, int,               // send data to a specific address
            struct sockaddr*, unsigned int);
    int (*recvfrom)(socket_t*, void*, unsigned int, int,             // receive data
            struct sockaddr*, unsigned int*);
} socket_ops_t;

/*
 * Hardware types
 */
#define HW_TYPE_ETH 0

/*
 * Default headroom used for new network messages
 * IP header size: max. 60 bytes
 * Ethernet header size: max 18 bytes
 * To be on the safe side, we use 128 bytes
 */
#define NET_MIN_HEADROOM 128

/*
 * Event types for select
 */
#define NET_EVENT_CAN_READ 1
#define NET_EVENT_CAN_WRITE 2

/*
 * Maximum number of connections a listening socket can queue
 */
#define MAX_LISTEN_BACKLOG 15


void net_init();
net_msg_t* net_msg_create(u32 size, u32 headroom);
net_msg_t* net_msg_new(u32 size);
void net_msg_destroy(net_msg_t* net_msg);
void net_msg_cut_off(net_msg_t* net_msg, u32 offset);
u8* net_msg_append(net_msg_t* net_msg, u32 size);
u8* net_msg_prepend(net_msg_t* net_msg, u32 size);
net_msg_t* net_msg_clone(net_msg_t* net_msg);
void net_msg_set_eth_hdr(net_msg_t* net_msg, u32 offset);
void net_msg_set_arp_hdr(net_msg_t* net_msg, u32 offset);
void net_msg_set_ip_hdr(net_msg_t* net_msg, u32 offset);
void net_msg_set_icmp_hdr(net_msg_t* net_msg, u32 offset);
void net_msg_set_tcp_hdr(net_msg_t* net_msg, u32 offset);
void net_msg_set_udp_hdr(net_msg_t* net_msg, u32 offset);
u32 net_msg_get_size(net_msg_t* net_msg);
u8* net_msg_get_start(net_msg_t* net_msg);
u16 net_compute_checksum(u16* words, int word_count);
socket_t* net_socket_create(int domain, int type, int proto);
int net_socket_connect(socket_t* socket, struct sockaddr* addr, int addrlen);
void net_socket_close(socket_t* socket);
ssize_t net_socket_send(socket_t* socket, void* buffer, size_t len, int flags, struct sockaddr* addr, u32 addrlen, int sendto);
ssize_t net_socket_recv(socket_t* socket, void* buffer, size_t len, int flags, struct sockaddr* addr, u32* addrlen, int recvfrom);
int net_socket_listen(socket_t* socket, int backlog);
int net_socket_bind(socket_t* socket, struct sockaddr* address, int addrlen);
int net_socket_accept(socket_t* socket, struct sockaddr* addr, socklen_t* addrlen, socket_t** new_socket);
void net_post_event(socket_t* socket, int event);

int net_socket_select(socket_t* socket, int read, int write, semaphore_t* sem);
int net_socket_cancel_select(socket_t* socket, semaphore_t* sem);
int net_ioctl(socket_t* socket, unsigned int cmd, void* arg);
int net_socket_setoption(socket_t* socket, int level, int option, void* option_value, unsigned int option_len);
int net_socket_getaddr(socket_t* socket, struct sockaddr* laddr, struct sockaddr* faddr, unsigned int* addrlen);

void net_print_ip(u32 ip_address);
u32 net_str2ip(char* ip_address);
void net_get_counters(int* created, int* destroyed);

/*
 * This is in eth.c
 */
int eth_create_header(net_msg_t* net_msg);
void eth_address_copy(mac_address_t to, mac_address_t from);

#endif /* _NET_H_ */
