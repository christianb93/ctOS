/*
 * tcp.h
 *
 */

#ifndef _TCP_H_
#define _TCP_H_

#include "ktypes.h"
#include "net.h"

typedef struct {
    u16 src_port;               // Source port
    u16 dst_port;               // Destination port
    u32 seq_no;                 // Sequence number
    u32 ack_no;                 // Acknowledge number
    u8 rsv1:4,                  // 4 reserved bits
       hlength:4;               // header length
    u8 fin:1,                   // FIN
       syn:1,                   // SYN
       rst:1,                   // RST
       psh:1,                   // PSH
       ack:1,                   // ACK
       urg:1,                   // URG
       ece:1,                   // ECN Echo
       cwr:1;                   // Congestion window reduced
    u16 window;                 // Receive window
    u16 checksum;               // Checksum
    u16 urgent_ptr;             // Urgent pointer
} __attribute__ ((packed)) tcp_hdr_t;


/*
 * TCP header options
 */
#define TCP_OPT_KIND_EOD 0
#define TCP_OPT_KIND_NOP 1
#define TCP_OPT_KIND_MSS 2
#define TCP_OPT_LEN_MSS 4

/*
 * This structure is used to pass options around
 */
typedef struct {
    u32 mss;                    // Maximum segment size option
} tcp_options_t;

/*
 * Status of a socket
 */
#define TCP_STATUS_CLOSED 1
#define TCP_STATUS_SYN_SENT 2
#define TCP_STATUS_SYN_RCVD 3
#define TCP_STATUS_ESTABLISHED 4
#define TCP_STATUS_LISTEN 5
#define TCP_STATUS_CLOSE_WAIT 6
#define TCP_STATUS_FIN_WAIT_1 7
#define TCP_STATUS_LAST_ACK 8
#define TCP_STATUS_FIN_WAIT_2 9
#define TCP_STATUS_TIME_WAIT 10
#define TCP_STATUS_CLOSING 11

/*
 * Default MSS
 */
#define TCP_DEFAULT_MSS 536

/*
 * Start of ephemeral port range
 */
#define TCP_EPHEMERAL_PORT 49152

/*
 * Initial RTO in units of TCP ticks - use 1 second
 */
#define RTO_INIT (TCP_HZ)

/*
 * Max RTO in units of TCP ticks
 */
#define RTO_MAX (TCP_HZ * 120)
/*
 * Maximum backoff
 */
#define TCP_MAX_BACKOFF 10
/*
 * Value of current_rtt indicating that no RTT sample is in progress
 */
#define RTT_NONE -1

/*
 * Value for delayed ACK timer in ticks
 */
#define DELACK_TO 1
/*
 * Timeout for a SYN in ticks
 */
#define SYN_TIMEOUT (TCP_HZ * 15)
/*
 * Maximum value for SYN timeout
 */
#define SYN_TIMEOUT_MAX (TCP_HZ * 600)
/*
 * Number of attempts we do for retransmitting a SYN
 */
#define SYN_MAX_RTX 5
/*
 * Number of attempts we do for retransmitting data
 */
#define TCP_MAX_RTX 5
/*
 * Assumed value for the MSL in ticks - we assume 30 seconds
 */
#define TCP_MSL (TCP_HZ*30)

/*
 * Number of bits in srtt and rttvar variables which are after the
 * decimal point
 */
#define SRTT_SHIFT 3

/*
 * Initial size of congestion window (segments)
 */
#define CWND_IW 1

/*
 * Initial value of SSTHRESH
 */
#define SSTHRESH_INIT 65536

/*
 * Option flags stored in tcp_options
 */
#define TCP_OPTIONS_CC 0x1


/*
 * Number of duplicate ACKs which trigger fast retransmit
 */
#define DUPACK_TRIGGER 3

/*
 * Number of sockets which we allow
 */
#define MAX_TCP_SOCKETS 256

/*
 * Macros to compare sequence numbers
 *
 * According to RFC 1323, the maximum window size is 2^30 (which corresponds to the
 * window scale factor being limited to 2^14). If we think of sequence numbers as being
 * the mod 2^32 projection of a virtual, unlimited "real sequence number", this implies that
 * real sequence numbers only differ by at most 2^30, i.e. that their difference is small enough
 * to be captured by a signed 32-bit integer. This makes the following macros work
 */
#define TCP_LT(a,b) ( ((int)((b) - (a))) > 0)
#define TCP_LEQ(a,b) ( ((int)((b) - (a))) >= 0)
#define TCP_GT(a,b) ( ((int)((a) - (b))) > 0)
#define TCP_GEQ(a,b) ( ((int)((a) - (b))) >= 0)


void tcp_init();
int tcp_create_socket(socket_t* socket, int domain, int proto);
void tcp_rx_msg(net_msg_t* net_msg);
void tcp_do_tick();

int tcp_print_sockets();
#endif /* _TCP_H_ */
