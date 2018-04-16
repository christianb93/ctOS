/*
 * test_tcp.c
 *
 * Related RFCs:
 * -------------
 *
 *
 * 1) Some of the test cases in this unit test suite cover a subset of the processing algorithm for incoming segments
 * described in RFC 793, starting at page 6:
 *
 *   SEGMENT ARRIVES - state CLOSED:
 *            TC 6, TC 7
 *   SEGMENT ARRIVES - state SYN-SENT:
 *            Acceptable SYN-ACK - TC 5
 *            ACK with wrong ACK_NO - TC 88, 90
 *            RST - TC 89, TC 106
 *            SYN - TC 91, TC 92
 *   SEGMENT ARRIVES - state ESTABLISHED
 *            Entire segment within window - TC 16, TC 17
 *            Segment starts left of window - TC 18
 *            Segment starts in the middle receive window - TC 19
 *            Segment acknowledges data not yet sent - TC 51
 *            Segment is not an ACK - TC 93
 *            Segment is a FIN - TC 96
 *            Segment is an acceptable RST - TC 114
 *            Segment is a SYN - TC 117
 *   SEGMENT ARRIVES - state LISTEN
 *            SYN arrives - TC 74, TC 75
 *            RST arrives - TC 86
 *            ACK arrives - TC 87
 *   SEGMENT ARRIVES - state SYN_RCVD
 *            Acceptable ACK arrives - TC 76, TC 77, TC 78, TC 79
 *            Wrong ACK arrives - TC 94
 *            FIN arrives - TC 97
 *            Acceptable RST arrives - TC 115, TC 116
 *   SEGMENT ARRIVES - state CLOSE_WAIT
 *            FIN arrives - TC 96
 *   SEGMENT ARRIVES - state LAST_ACK
 *            Valid ACK - TC 101
 *   SEGMENT ARRIVES - state FIN_WAIT_1
 *            Valid ACK - TC 102, TC 109
 *            Valid FIN-ACK - TC 103
 *            Text segment - TC 108, TC 110
 *            FIN - TC 118
 *   SEGMENT ARRIVES - state FIN_WAIT2
 *            Valid FIN - TC 105
 *            Text segment - TC 111, TC 112
 *   SEGMENT ARRIVES - state TIME_WAIT
 *            Retransmitted FIN - TC 107
 *   SEGMENT ARRIVES - state CLOSING
 *            ACK - TC 119
 *   TIMEOUT - connection not yet established
 *            Active connection times out - TC 55 (not fully specified in RFC 793)
 *            Passive connection times out - TC 95
 *   TIMEOUT - FIN times out
 *            FIN_WAIT_1 - TC 99
 *            CLOSE_WAIT - TC 100
 *   TIME_WAIT timeout
 *            TC 104
 *
 *
 * 2) The following test cases relate to section 4.2.3.4 ("When to send data") of RFC 1122 dealing with Nagle's algorithm and
 * the SWS avoidance algorithm for the sender:
 *
 *    TC 12: min(D,U) >= MSS, D <= U
 *    TC 15: min(D,U) >= MSS, D > U
 *    TC 15: min(D,U) < MSS, SND_NXT != SND_UNA
 *    TC 11: min(D,U) < MSS, SND_NXT = SND_UNA, D <= U
 *    TC 13: min(D,U) < MSS, SND_NXT = SND_UNA, D > U, U >= 0,5 * Max window
 *    TC 14: min(D,U) < MSS, SND_NXT = SND_UNA, D > U, U < 0,5 * Max window
 *
 *
 * 3) Some more test cases relate to RFC 1122 and RFC 793
 *
 *    TC 19, TC 20: RFC 1122 Section 4.2.2.21 (send immediate ACK if an out-of-order segment is received to support fast retransmission)
 *    TC 22: RFC 1122 Section 4.2.3.3 (SWS avoidance on the receivers side)
 *    TC 21: RFC 1122 4.2.3.4 (When to send data)
 *    TC 29, TC 30: RFC 1122 4.2.3.2 (delayed ACK)
 *    TC 31, TC 32: RFC 1122 4.2.3.4 (persist timer)
 *    TC 32, TC 34, TC 66, TC 67: RFC 1122 4.2.2.17 (zero window probes)
 *    TC 33: RFC 793, page 42 (send ACK for a segment when own window is zero)
 *    TC 56,57, 58: use of MSS option during connection establishment
 *    TC 59: effective MSS takes interface into account
 *    TC 60: unknown options are ignored
 *    TC 63: handling of shrinking window
 *    TC 76: RFC 1122 Section 4.2.4.4 Multihoming - select local IP address when listening socket was bound to INADDR_ANY
 *    TC 5: RFC 1122 Section 4.2.4.4 Multihoming - select local IP address when actively connecting a socket
 *
 * 4) Test cases related to system calls
 *
 *    TC 8,9,10: send
 *    TC 22, 23, 24: recv
 *    TC 125: recvfrom
 *    TC 68, 69, 70, 71: bind
 *    TC 72,73, 84, 85: listen
 *    TC 80, TC 81, TC 82, TC 83: select
 *    TC 98, TC 99: close socket in state ESTABLISHED
 *    TC 100: close socket in state CLOSE_WAIT
 *    TC 120: close socket in state SYN_RECEIVED
 *    TC 121: close socket while there is still data in the send buffer
 *    TC 123: close socket in state SYN_SENT
 *    TC 124: close socket in state LISTEN
 *
 * 5) Test cases related to management of retransmission timer as specified in RFC 2988, section 5:
 *
 *    TC 25: new data sent, timer not running
 *    TC 25: retransmission timer expires - retransmit, apply backoff, reset timer
 *    TC 26: new data sent, timer running
 *    TC 26: ACK received, no more data outstanding
 *    TC 26: ACK received, data outstanding
 *    TC 61: ACK received for cached segments
 *    TC 62: timeout while connection is established
 *
 * 6) Test cases related to RTO calculations:
 *
 *    TC 25: initial RTO is 3 seconds
 *    TC 27: RTO after first RTT measurement has been taken
 *    TC 26: RTO after first RTT measurement has been taken, minimum RTO used
 *    TC 28: RTO updated with new RTT sample
 *
 * 7) Test cases related to congestion control (RFC 2581, RFC 5681):
 *
 *    TC 41: slow start - initial size of congestion window
 *    TC 42: slow start - increase congestion window with each ACK (one ACK per segment)
 *    TC 43: slow start - increase congestion window with each ACK (cumulative ACK)
 *    TC 44: congestion avoidance - do not increase window if less than cwnd bytes acknowledged
 *    TC 45: congestion avoidance - increase window once cwnd bytes have been acknowledged
 *    TC 46: slow start - enter slow start again after a timer based retransmission
 *    TC 47: congestion avoidance - enter congestion avoidance again after timer based retransmission
 *    TC 48: fast retransmit and fast recovery - recovery successful
 *    TC 49: fast retransmit and fast recovery - retransmission times out
 *    TC 50: fast retransmit and fast recovery - do not retransmit window probe
 *
 * 8) Reference counting
 *
 *    TC 1: reference count of new socket is two
 *    TC 1: closing a socket in status CLOSED reduces reference count to one again
 *    TC 16: multiplexing does not increase reference count
 *    TC 104: if TIME_WAIT timer expires, reference to socket is dropped
 */



#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "kunit.h"
#include "vga.h"
#include "tcp.h"
#include "net.h"
#include "lib/os/if.h"
#include "lib/os/route.h"
#include "lib/netinet/in.h"
#include <string.h>
#include <unistd.h>

/*
 * Make sure that this matches the definitions im timer.h
 */
#define HZ 100
#define TCP_HZ 4

extern int __net_loglevel;

#define IPPROTO_TCP 6
#define MIN(x,y)  (((x) < (y) ? (x) : (y)))

void trap() {
    printf("------------------ PANIC !! -----------------------\n");
}

int do_kill(int pid, int sig_no) {
    return 0;
}

int pm_get_pid() {
    return 1;
}

/*
 * This needs to match the value defined in timer.h
 */
#define HZ 100

/*
 * Given a timeval structure, convert its value into ticks or return the maximum in case
 * of an overflow
 */
unsigned int timer_convert_timeval(struct timeval* time) {
    unsigned int ticks;
    unsigned int ticks_usec;
    /*
      * First compute contribution of tv_sev field
      */
     if (time->tv_sec > (UINT_MAX / HZ)) {
         ticks = UINT_MAX;
     }
     else {
         ticks = time->tv_sec * HZ;
     }
     /*
      * then add contribution of tv_usec field
      */
     ticks_usec = time->tv_usec / (1000000 / HZ);
     if (ticks_usec >  ~ticks) {
         ticks = UINT_MAX / HZ;
     }
     else {
         ticks += ticks_usec;
     }
     return ticks;
}


static int mtu = 576;
int ip_get_mtu(u32 ip_src) {
    if (0 == ip_src)
        return 576;
    return mtu;
}

void sem_up(semaphore_t* sem) {

}

int cond_wait_intr_timed(cond_t* cond, spinlock_t* lock, u32* eflags, unsigned int timeout) {
    spinlock_release(lock, eflags);
    return -1;
}

void atomic_incr(u32* reg) {
    (*reg)++;
}

void ip_init() {

}

int mm_validate_buffer(u32 buffer, u32 len, int rw) {
    return 0;
}

void udp_init() {

}

int net_if_get_ifconf(struct ifconf* ifc) {
    return -1;
}

int net_if_set_addr(struct ifreq* ifr) {
    return 0;
}

int net_if_get_addr(struct ifreq* ifr) {
    return 0;
}

int net_if_get_netmask(struct ifreq* ifr) {
    return 0;
}

int net_if_set_netmask(struct ifreq* ifr) {
    return 0;
}

int ip_add_route(struct rtentry* rt_entry) {
    return 0;
}

int ip_del_route(struct rtentry* rt_entry) {
    return 0;
}

int udp_create_socket(socket_t* socket, int type, int proto) {
    return 0;
}

/*
 * Compute tcp checksum: given IP header and tcp segment
 */
u16 validate_tcp_checksum(unsigned short tcpLen, u16* ip_payload, u32 ip_src, u32 ip_dst) {
    unsigned long sum = 0;
    /*
     * add the pseudo header
     */
    /*
     * the source ip and destination ip
     */
    sum += (ip_src >> 16) & 0xFFFF;
    sum += (ip_src) & 0xFFFF;
    sum += (ip_dst >> 16) & 0xFFFF;
    sum += (ip_dst) & 0xFFFF;

    /*
     * protocol and reserved: 6
     */
    sum += htons(0x6);
    /*
     * the length
     */
    sum += htons(tcpLen);
    /*
     * and the IP payload, including the TCP header itself
     */
    while (tcpLen > 1) {
        sum += * ip_payload++;
        tcpLen -= 2;
    }
    if(tcpLen > 0) {
        sum += ((*ip_payload) & htons(0xFF00));

    }
    /*
     * Fold 32-bit sum to 16 bits: add carrier to result
     */
    while (sum>>16) {
          sum = (sum & 0xffff) + (sum >> 16);
    }
    sum = ~sum;
    return htons(sum);
}

/*
 * Create a SYN
 * Parameter:
 * @ip_src - IP source address (network byte order)
 * @ip_dst - IP destination address (network byte order)
 * @src_port - source port (host byte order)
 * @dst_port - destination port (host byte order)
 * @seq_no - sequence number (host byte order)
 * @wnd - window to be advertised
 * @mss - MSS to be advertised
 */
static net_msg_t* create_syn(u32 ip_src, u32 ip_dst, u16 src_port, u16 dst_port, u32 seq_no,  u32 wnd, u16 mss) {
    net_msg_t* net_msg = 0;
    u8* options;
    int headroom = 14 + 20;
    tcp_hdr_t* tcp_hdr;
    int size = 128;
    if (0 == (net_msg = net_msg_create(size, headroom)))
        return 0;
    net_msg->start = net_msg->data + MIN(headroom, size);
    net_msg->end = net_msg->start;
    net_msg->nic = 0;
    net_msg->length = size;
    /*
     * Set IP src, IP destination and IP payload length
     */
    net_msg->ip_src = ip_src;
    net_msg->ip_dest = ip_dst;
    net_msg->ip_length = 24;
    /*
     * Set TCP header pointer and construct TCP header
     */
    net_msg->tcp_hdr = net_msg->start + 20;
    tcp_hdr = (tcp_hdr_t*) net_msg->tcp_hdr;
    memset((void*) tcp_hdr, 0, sizeof(tcp_hdr_t));
    tcp_hdr->ack = 0;
    tcp_hdr->syn = 1;
    tcp_hdr->dst_port = htons(dst_port);
    tcp_hdr->src_port = htons(src_port);
    tcp_hdr->hlength = 6;
    tcp_hdr->checksum = 0;
    tcp_hdr->seq_no = htonl(seq_no);
    tcp_hdr->ack_no = 0;
    tcp_hdr->window = htons(wnd);
    /*
     * Add MSS option
     */
    options = (u8*) (net_msg->tcp_hdr + sizeof(tcp_hdr_t));
    options[0] = 2;
    options[1] = 4;
    options[2] = (mss >> 8);
    options[3] = (mss & 0xFF);
    tcp_hdr->checksum = htons(validate_tcp_checksum(24, (u16*) tcp_hdr, ip_src, ip_dst));
    return net_msg;
}

/*
 * Create a RST
 * Parameter:
 * @ip_src - IP source address (network byte order)
 * @ip_dst - IP destination address (network byte order)
 * @src_port - source port (host byte order)
 * @dst_port - destination port (host byte order)
 * @seq_no - sequence number (host byte order)
 * @ack_no - acknowledgement number
 */
static net_msg_t* create_rst(u32 ip_src, u32 ip_dst, u16 src_port, u16 dst_port, u32 seq_no,  u32 ack_no) {
    net_msg_t* net_msg = 0;
    u8* options;
    int headroom = 14 + 20;
    tcp_hdr_t* tcp_hdr;
    int size = 128;
    if (0 == (net_msg = net_msg_create(size, headroom)))
        return 0;
    net_msg->start = net_msg->data + MIN(headroom, size);
    net_msg->end = net_msg->start;
    net_msg->nic = 0;
    net_msg->length = size;
    /*
     * Set IP src, IP destination and IP payload length
     */
    net_msg->ip_src = ip_src;
    net_msg->ip_dest = ip_dst;
    net_msg->ip_length = 20;
    /*
     * Set TCP header pointer and construct TCP header
     */
    net_msg->tcp_hdr = net_msg->start + 20;
    tcp_hdr = (tcp_hdr_t*) net_msg->tcp_hdr;
    memset((void*) tcp_hdr, 0, sizeof(tcp_hdr_t));
    tcp_hdr->ack = 0;
    tcp_hdr->rst = 1;
    tcp_hdr->syn = 0;
    tcp_hdr->dst_port = htons(dst_port);
    tcp_hdr->src_port = htons(src_port);
    tcp_hdr->hlength = 5;
    tcp_hdr->checksum = 0;
    tcp_hdr->seq_no = htonl(seq_no);
    tcp_hdr->ack_no = htonl(ack_no);
    tcp_hdr->window = htons(8192);
    tcp_hdr->checksum = htons(validate_tcp_checksum(20, (u16*) tcp_hdr, ip_src, ip_dst));
    return net_msg;
}

/*
 * Create a SYN-ACK
 * Parameter:
 * @ip_src - IP source address (network byte order)
 * @ip_dst - IP destination address (network byte order)
 * @src_port - source port (host byte order)
 * @dst_port - destination port (host byte order)
 * @seq_no - sequence number (host byte order)
 * @ack_no - acknowledgement number (host byte order)
 * @wnd - window to be advertised
 */
static net_msg_t* create_syn_ack(u32 ip_src, u32 ip_dst, u16 src_port, u16 dst_port, u32 seq_no, u32 ack_no, u32 wnd) {
    net_msg_t* net_msg = 0;
    int headroom = 14 + 20;
    tcp_hdr_t* tcp_hdr;
    int size = 128;
    if (0 == (net_msg = net_msg_create(size, headroom)))
        return 0;
    net_msg->start = net_msg->data + MIN(headroom, size);
    net_msg->end = net_msg->start;
    net_msg->nic = 0;
    net_msg->length = size;
    /*
     * Set IP src, IP destination and IP payload length
     */
    net_msg->ip_src = ip_src;
    net_msg->ip_dest = ip_dst;
    net_msg->ip_length = 20;
    /*
     * Set TCP header pointer and construct TCP header
     */
    net_msg->tcp_hdr = net_msg->start + 20;
    tcp_hdr = (tcp_hdr_t*) net_msg->tcp_hdr;
    memset((void*) tcp_hdr, 0, sizeof(tcp_hdr_t));
    tcp_hdr->ack = 1;
    tcp_hdr->syn = 1;
    tcp_hdr->dst_port = htons(dst_port);
    tcp_hdr->src_port = htons(src_port);
    tcp_hdr->hlength = 5;
    tcp_hdr->checksum = 0;
    tcp_hdr->seq_no = htonl(seq_no);
    tcp_hdr->ack_no = htonl(ack_no);
    tcp_hdr->window = htons(wnd);
    tcp_hdr->checksum = htons(validate_tcp_checksum(20, (u16*) tcp_hdr, ip_src, ip_dst));
    return net_msg;
}

/*
 * Create a FIN-ACK
 * Parameter:
 * @ip_src - IP source address (network byte order)
 * @ip_dst - IP destination address (network byte order)
 * @src_port - source port (host byte order)
 * @dst_port - destination port (host byte order)
 * @seq_no - sequence number (host byte order)
 * @ack_no - acknowledgement number (host byte order)
 * @wnd - window to be advertised
 */
static net_msg_t* create_fin_ack(u32 ip_src, u32 ip_dst, u16 src_port, u16 dst_port, u32 seq_no, u32 ack_no, u32 wnd) {
    net_msg_t* net_msg = 0;
    int headroom = 14 + 20;
    tcp_hdr_t* tcp_hdr;
    int size = 128;
    if (0 == (net_msg = net_msg_create(size, headroom)))
        return 0;
    net_msg->start = net_msg->data + MIN(headroom, size);
    net_msg->end = net_msg->start;
    net_msg->nic = 0;
    net_msg->length = size;
    /*
     * Set IP src, IP destination and IP payload length
     */
    net_msg->ip_src = ip_src;
    net_msg->ip_dest = ip_dst;
    net_msg->ip_length = 20;
    /*
     * Set TCP header pointer and construct TCP header
     */
    net_msg->tcp_hdr = net_msg->start + 20;
    tcp_hdr = (tcp_hdr_t*) net_msg->tcp_hdr;
    memset((void*) tcp_hdr, 0, sizeof(tcp_hdr_t));
    tcp_hdr->ack = 1;
    tcp_hdr->syn = 0;
    tcp_hdr->fin = 1;
    tcp_hdr->dst_port = htons(dst_port);
    tcp_hdr->src_port = htons(src_port);
    tcp_hdr->hlength = 5;
    tcp_hdr->checksum = 0;
    tcp_hdr->seq_no = htonl(seq_no);
    tcp_hdr->ack_no = htonl(ack_no);
    tcp_hdr->window = htons(wnd);
    tcp_hdr->checksum = htons(validate_tcp_checksum(20, (u16*) tcp_hdr, ip_src, ip_dst));
    return net_msg;
}

/*
 * Create a FIN-ACK embedded in a text segment
 * Parameter:
 * @ip_src - IP source address (network byte order)
 * @ip_dst - IP destination address (network byte order)
 * @src_port - source port (host byte order)
 * @dst_port - destination port (host byte order)
 * @seq_no - sequence number (host byte order)
 * @ack_no - acknowledgement number (host byte order)
 * @wnd - window to be advertised
 * @data - data
 * @size - bytes to be sent
 */
static net_msg_t* create_fin_text(u32 ip_src, u32 ip_dst, u16 src_port, u16 dst_port, u32 seq_no, u32 ack_no, u32 wnd,
        u8* data, u32 size) {
    net_msg_t* net_msg = 0;
    int i;
    int headroom = 14 + 20;
    tcp_hdr_t* tcp_hdr;
    if (0 == (net_msg = net_msg_create(size, headroom)))
        return 0;
    if (0 == (net_msg->data = (u8*) malloc(size + headroom + sizeof(tcp_hdr_t)))) {
        free((void*) net_msg);
        return 0;
    }
    net_msg->start = net_msg->data + headroom;
    net_msg->end = net_msg->start;
    net_msg->nic = 0;
    net_msg->length = size + headroom + sizeof(tcp_hdr_t);
    /*
     * Set IP src, IP destination and IP payload length
     */
    net_msg->ip_src = ip_src;
    net_msg->ip_dest = ip_dst;
    net_msg->ip_length = sizeof(tcp_hdr_t) + size;
    /*
     * Set TCP header pointer and construct TCP header
     */
    net_msg->tcp_hdr = net_msg->start;
    tcp_hdr = (tcp_hdr_t*) net_msg->tcp_hdr;
    memset((void*) tcp_hdr, 0, sizeof(tcp_hdr_t));
    if (ack_no)
        tcp_hdr->ack = 1;
    else
        tcp_hdr->ack = 0;
    tcp_hdr->syn = 0;
    tcp_hdr->fin = 1;
    tcp_hdr->dst_port = htons(dst_port);
    tcp_hdr->src_port = htons(src_port);
    tcp_hdr->hlength = 5;
    tcp_hdr->checksum = 0;
    tcp_hdr->seq_no = htonl(seq_no);
    tcp_hdr->ack_no = htonl(ack_no);
    tcp_hdr->window = htons(wnd);
    /*
     * add data
     */
    for (i = 0; i < size; i++) {
        ((u8*)(net_msg->tcp_hdr + sizeof(tcp_hdr_t)))[i] = data[i];
    }
    /*
     * and compute checksum
     */
    tcp_hdr->checksum = htons(validate_tcp_checksum(20 + size, (u16*) tcp_hdr, ip_src, ip_dst));
    return net_msg;
}


/*
 * Create a RST_ACK
 * Parameter:
 * @ip_src - IP source address (network byte order)
 * @ip_dst - IP destination address (network byte order)
 * @src_port - source port (host byte order)
 * @dst_port - destination port (host byte order)
 * @seq_no - sequence number (host byte order)
 * @ack_no - acknowledgement number (host byte order)
 */
static net_msg_t* create_rst_ack(u32 ip_src, u32 ip_dst, u16 src_port, u16 dst_port, u32 seq_no, u32 ack_no) {
    net_msg_t* net_msg = 0;
    int headroom = 14 + 20;
    tcp_hdr_t* tcp_hdr;
    int size = 128;
    if (0 == (net_msg = net_msg_create(size, headroom)))
        return 0;
    if (0 == (net_msg->data = (u8*) malloc(size))) {
        free((void*) net_msg);
        return 0;
    }
    net_msg->start = net_msg->data + MIN(headroom, size);
    net_msg->end = net_msg->start;
    net_msg->nic = 0;
    net_msg->length = size;
    /*
     * Set IP src, IP destination and IP payload length
     */
    net_msg->ip_src = ip_src;
    net_msg->ip_dest = ip_dst;
    net_msg->ip_length = 20;
    /*
     * Set TCP header pointer and construct TCP header
     */
    net_msg->tcp_hdr = net_msg->start + 20;
    tcp_hdr = (tcp_hdr_t*) net_msg->tcp_hdr;
    memset((void*) tcp_hdr, 0, sizeof(tcp_hdr_t));
    tcp_hdr->ack = 1;
    tcp_hdr->rst = 1;
    tcp_hdr->syn = 0;
    tcp_hdr->dst_port = htons(dst_port);
    tcp_hdr->src_port = htons(src_port);
    tcp_hdr->hlength = 5;
    tcp_hdr->checksum = 0;
    tcp_hdr->seq_no = htonl(seq_no);
    tcp_hdr->ack_no = htonl(ack_no);
    tcp_hdr->window = htons(8192);
    tcp_hdr->checksum = htons(validate_tcp_checksum(20, (u16*) tcp_hdr, ip_src, ip_dst));
    return net_msg;
}


/*
 * Create a SYN-ACK with an MSS option
 * Parameter:
 * @ip_src - IP source address (network byte order)
 * @ip_dst - IP destination address (network byte order)
 * @src_port - source port (host byte order)
 * @dst_port - destination port (host byte order)
 * @seq_no - sequence number (host byte order)
 * @ack_no - acknowledgement number (host byte order)
 * @wnd - window to be advertised
 * @mss - MSS to be advertised
 */
static net_msg_t* create_syn_ack_mss(u32 ip_src, u32 ip_dst, u16 src_port, u16 dst_port, u32 seq_no, u32 ack_no, u32 wnd, int mss) {
    net_msg_t* net_msg = 0;
    int headroom = 14 + 20;
    tcp_hdr_t* tcp_hdr;
    int size = 128;
    u8* options;
    if (0 == (net_msg = net_msg_create(size, headroom)))
        return 0;
    net_msg->start = net_msg->data + MIN(headroom, size);
    net_msg->end = net_msg->start;
    net_msg->nic = 0;
    net_msg->length = size;
    /*
     * Set IP src, IP destination and IP payload length
     */
    net_msg->ip_src = ip_src;
    net_msg->ip_dest = ip_dst;
    net_msg->ip_length = 24;
    /*
     * Set TCP header pointer and construct TCP header
     */
    net_msg->tcp_hdr = net_msg->start + 20;
    tcp_hdr = (tcp_hdr_t*) net_msg->tcp_hdr;
    memset((void*) tcp_hdr, 0, sizeof(tcp_hdr_t));
    tcp_hdr->ack = 1;
    tcp_hdr->syn = 1;
    tcp_hdr->dst_port = htons(dst_port);
    tcp_hdr->src_port = htons(src_port);
    tcp_hdr->hlength = 6;
    tcp_hdr->checksum = 0;
    tcp_hdr->seq_no = htonl(seq_no);
    tcp_hdr->ack_no = htonl(ack_no);
    tcp_hdr->window = htons(wnd);
    /*
     * Add MSS option
     */
    options = (u8*) (net_msg->tcp_hdr + sizeof(tcp_hdr_t));
    options[0] = 2;
    options[1] = 4;
    options[2] = (mss >> 8);
    options[3] = (mss & 0xFF);
    /*
     * and compute checksum
     */
    tcp_hdr->checksum = htons(validate_tcp_checksum(24, (u16*) tcp_hdr, ip_src, ip_dst));
    return net_msg;
}

/*
 * Create a text segment
 * Parameter:
 * @ip_src - IP source address (network byte order)
 * @ip_dst - IP destination address (network byte order)
 * @src_port - source port (host byte order)
 * @dst_port - destination port (host byte order)
 * @seq_no - sequence number (host byte order)
 * @ack_no - acknowledgement number (host byte order) - 0 -> no ACK
 * @wnd - window to be advertised
 * @buffer - data
 * @size - number of bytes
 */
static net_msg_t* create_text(u32 ip_src, u32 ip_dst, u16 src_port, u16 dst_port, u32 seq_no, u32 ack_no, u32 wnd, u8* data, u32 size) {
    net_msg_t* net_msg = 0;
    int i;
    int headroom = 14 + 20;
    tcp_hdr_t* tcp_hdr;
    u8*  msg_data;
    if (0 == (net_msg = net_msg_create(size + headroom + sizeof(tcp_hdr_t), headroom)))
        return 0;
    /*
     * Set IP src, IP destination and IP payload length
     */
    net_msg->ip_src = ip_src;
    net_msg->ip_dest = ip_dst;
    net_msg->ip_length = sizeof(tcp_hdr_t) + size;
    /*
     * Set TCP header pointer and construct TCP header
     */
    net_msg->tcp_hdr = net_msg_append(net_msg, sizeof(tcp_hdr_t));
    tcp_hdr = (tcp_hdr_t*) net_msg->tcp_hdr;
    if (0 == tcp_hdr) {
        printf("Could not allocate memory for TCP header\n");
        _exit(1);
    }
    memset((void*) tcp_hdr, 0, sizeof(tcp_hdr_t));
    if (ack_no)
        tcp_hdr->ack = 1;
    else
        tcp_hdr->ack = 0;
    tcp_hdr->syn = 0;
    tcp_hdr->dst_port = htons(dst_port);
    tcp_hdr->src_port = htons(src_port);
    tcp_hdr->hlength = 5;
    tcp_hdr->checksum = 0;
    tcp_hdr->seq_no = htonl(seq_no);
    tcp_hdr->ack_no = htonl(ack_no);
    tcp_hdr->window = htons(wnd);
    /*
     * add data
     */
    msg_data = net_msg_append(net_msg, size);
    if (0 == msg_data) {
        printf("Could not allocate memory for TCP payload\n");
        _exit(1);
    }
    for (i = 0; i < size; i++) {
        msg_data[i] = data[i];
    }
    /*
     * and compute checksum
     */
    tcp_hdr->checksum = htons(validate_tcp_checksum(20 + size, (u16*) tcp_hdr, ip_src, ip_dst));
    return net_msg;
}




/*
 * Stubs
 */
static int do_putchar = 1;
void win_putchar(win_t* win, u8 c) {
    if (do_putchar)
        printf("%c", c);
}

static u32 __useconds = 100;
int do_gettimeofday(u32* seconds, u32* useconds) {
    *useconds = __useconds;
    return 0;
}

/*
 * Stub for kmalloc/kfree
 */
u32 kmalloc(size_t size) {
    return (u32) malloc(size);
}
void kfree(u32 addr) {
    free((void*) addr);
}

static int tcp_disable_cc = 0;
int params_get_int(char* param) {
    if (0 == strcmp(param, "tcp_disable_cc"))
        return tcp_disable_cc;
    return 0;
}

void arp_init() {

}

void net_if_init() {

}

void spinlock_get(spinlock_t* lock, u32* flags) {
    /*
     * Abort if lock is already owned
     */
    if (*lock) {
        printf("-------------- Trying to request lock which is already owned by thread!! ---------------- \n");
        _exit(1);
    }
    *lock = 1;
}


void spinlock_release(spinlock_t* lock, u32* flags) {
    *lock = 0;
}

void spinlock_init(spinlock_t* lock) {
    *lock = 0;
}

void cond_init(cond_t* cond) {

}

void ip_create_socket(socket_t* socket) {

}

static int cond_broadcast_called = 0;
static cond_t* last_cond;
void cond_broadcast(cond_t* cond) {
    cond_broadcast_called++;
    last_cond = cond;
}

/*
 * Dummy for cond_wait_intr. As we cannot really wait in a single-threaded
 * unit test, we always return -1 here, i.e. we simulate the case that we
 * were interrupted
 */
int cond_wait_intr(cond_t* cond, spinlock_t* lock, u32* eflags) {
    return -1;
}
/*
 * Stub for ip_tx_msg
 */
static u8 payload[1024];
static u32 ip_src;
static u32 ip_dst;
static int ip_tx_msg_called = 0;
static int ip_payload_len = 0;
void ip_tx_msg(net_msg_t* net_msg) {
    int i;
    ip_tx_msg_called++;
    ip_src = net_msg->ip_src;
    ip_dst = net_msg->ip_dest;
    ip_payload_len = net_msg->ip_length;
    if (net_msg->end - net_msg->start < 1024) {
        for (i = 0; i < net_msg->end - net_msg->start; i++)
            payload[i] = net_msg->start[i];
    }
    /*
     * Destroy network message as the real IP layer would do it
     */
    net_msg_destroy(net_msg);
}

/*
 * Stub for IP routing
 */
u32 ip_get_src_addr(u32 ip_dst) {
    return 0x1402000a;
}

int ip_get_rtconf(struct rtconf* rtc) {
    return 0;
}



/*
 * Testcase 1:
 * Create a new TCP socket and verify that
 * all required fields are filled
 */
int testcase1() {
    u32 eflags;
    /*
     * Do basic initialization of socket
     */
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Validate that the operations structure is filled
     */
    ASSERT(socket->ops);
    ASSERT(socket->ops->connect);
    ASSERT(socket->ops->close);
    /*
     * and that the initial state is CLOSED
     */
    ASSERT(socket->proto.tcp.status == TCP_STATUS_CLOSED);
    /*
     * Reference count should be two
     */
    ASSERT(2 == socket->proto.tcp.ref_count);
    /*
     * Now close socket - reference count should drop by one
     */
    socket->ops->close(socket, &eflags);
    ASSERT(1 == socket->proto.tcp.ref_count);
    return 0;
}

/*
 * Testcase 2:
 * Connect a new socket and verify that the address length is validated
 */
int testcase2() {
    struct sockaddr_in in;
    /*
     * Do basic initialization of socket
     */
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect
     */
    ASSERT(-107 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)+1));
    return 0;
}

/*
 * Testcase 3:
 * Create a new socket and try to connect it. Verify that -EAGAIN is returned
 */
int testcase3() {
    struct sockaddr_in in;
    /*
     * Do basic initialization of socket
     */
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect
     */
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    return 0;
}

/*
 * Testcase 4:
 * Create a new socket and try to connect it. Verify that SYN is sent and that the checksum
 * is correct
 */
int testcase4() {
    struct sockaddr_in in;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u16* mss;
    /*
     * Do basic initialization of socket
     */
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Calculate checksum
     */
    chksum = validate_tcp_checksum(24, (u16*) payload, ip_src, ip_dst);
    ASSERT(0 == chksum);
    /*
     * Verify a few fields in the header resp. message passed to ip_tx_msg
     */
    ASSERT(ip_dst == in.sin_addr.s_addr);
    ASSERT(ip_src == 0x1402000a);
    /*
     * Header length byte contains four reserved bits
     */
    hdr_length = *((u8*) (payload + 12)) >> 4;
    /*
     * We expect 6 dwords (20 bytes TCP header and 4 bytes for MSS option)
     */
    ASSERT(6 == hdr_length);
    /*
     * Is SYN bit set?
     */
    ctrl_flags = *((u8*) (payload + 13));
    ASSERT(0x2 == ctrl_flags);
    /*
     * Verify destination port
     */
    dst_port = (u16*) (payload + 2);
    ASSERT(ntohs(*dst_port) == 30000);
    /*
     * Verify that MSS options are sent. Thus first byte after header is 2, second byte is 4,
     * third and fourth byte are 536 in network byte order
     */
    ASSERT(*(payload + sizeof(tcp_hdr_t)) == 2);
    ASSERT(*(payload + sizeof(tcp_hdr_t) + 1) == 4);
    mss = (u16*)(payload + sizeof(tcp_hdr_t) + 2);
    ASSERT(ntohs(*mss) == 536);
    /*
     * Check for memory leaks
     */
    unsigned int created;
    unsigned int destroyed;
    net_get_counters(&created, &destroyed);
    ASSERT(created == destroyed);
    return 0;
}

/*
 * Testcase 5:
 * Receive a SYN-ACK for a socket in state SYN-SENT and verify that the socket goes to the state ESTABLISHED and
 * sends an ACK
 */
int testcase5() {
    u32 eflags;
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32 syn_seq_no;
    u32* ack_seq_no;
    u32* ack_ack_no;
    net_msg_t* syn_ack;
    /*
     * Do basic initialization of socket
     */
    net_init();
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    ASSERT(0 ==  ((struct sockaddr_in*) &socket->laddr)->sin_addr.s_addr);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * and that the local IP address of the socket has been set
     */
    ASSERT(inet_addr("10.0.2.20") ==  ((struct sockaddr_in*) &socket->laddr)->sin_addr.s_addr);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 2048);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    __net_loglevel = 0;
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * that the window size has been updated
     */
    ASSERT(2048 == socket->proto.tcp.snd_wnd);
    /*
     * and that an ACK has been sent
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Check that
     * 1) the sequence number of the ACK is the sequence number of the syn + 1
     * 2) the acknowledgement number of the ACK is the sequence number of the SYN-ACK + 1, i.e. 2
     * 3) the ACK has the ACK flag set and SYN not set
     * 4) the TCP checksum is correct
     * 5) IP source and IP destination are correct
     */
    ack_seq_no = (u32*) (payload + 4);
    ack_ack_no = (u32*) (payload + 8);
    ctrl_flags = *((u8*)(payload + 13));
    ASSERT(ntohl(*ack_ack_no) == 2);
    ASSERT(ntohl(*ack_seq_no) == (syn_seq_no + 1));
    ASSERT(ctrl_flags == (1 << 4));
    ASSERT(0 == validate_tcp_checksum(20, (u16*) payload, ip_src, ip_dst));
    ASSERT(ip_src == 0x1402000a);
    ASSERT(ip_dst == 0x1502000a);
    /*
     * Assert that the connected flag in the socket is set
     */
    ASSERT(socket->connected == 1);
    ASSERT(socket->bound == 1);
    /*
     * Finally check SND_NXT and SND_UNA
     */
    ASSERT(syn_seq_no + 1 == socket->proto.tcp.snd_una);
    ASSERT(syn_seq_no + 1 == socket->proto.tcp.snd_nxt);
    /*
     * Check reference count
     */
    ASSERT(2 == socket->proto.tcp.ref_count);
    /*
     * Check for memory leaks
     */
    unsigned int created;
    unsigned int destroyed;
    net_get_counters(&created, &destroyed);
    ASSERT(created == destroyed);
    /*
     * close socket
     */
    __net_loglevel = 0;
    socket->ops->close(socket, &eflags);
    __net_loglevel = 0;
    return 0;
}

/*
 * Testcase 6:
 * Receive a segment not containing a reset for a non-existing socket
 * Verify that a RST is sent in response
 * Case A: ACK bit set in offending segment
 */
int testcase6() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn_ack;
    /*
     * Do basic initialization
     */
    tcp_init();
    __net_loglevel = 0;
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to port 1, using seq_no 200 and acknowledgement number 300
     */
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, 1, 200, 300, 2048);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Validate that a RST has been sent
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Check that
     * 1) the RST bit is set
     * 2) the TCP checksum is correct
     * 3) IP source and IP destination are correct
     * 4) the sequence number is the sequence number of the SYN-ACK
     * 5) the ACK-flag is not set
     */
    rst_seq_no = (u32*) (payload + 4);
    rst_ack_no = (u32*) (payload + 8);
    ctrl_flags = *((u8*)(payload + 13));
    ASSERT(ctrl_flags == (1 << 2));
    ASSERT(0 == validate_tcp_checksum(20, (u16*) payload, ip_src, ip_dst));
    ASSERT(ip_src == 0x1402000a);
    ASSERT(ip_dst == 0x1502000a);
    ASSERT(*rst_seq_no == ntohl(300));
    return 0;
}

/*
 * Testcase 7:
 * Receive a segment not containing a reset for a non-existing socket
 * Verify that a RST is sent in response
 * Case B: ACK bit not set in offending segment
 */
int testcase7() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    /*
     * Do basic initialization
     */
    tcp_init();
    /*
     * Assemble a SYN message from 10.0.2.21:30000 to port 1, using seq_no 200 and acknowledgement number 300
     */
    syn = create_syn(0x1502000a, 0x1402000a, 30000, 1, 200, 2048, 800);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn);
    /*
     * Validate that a RST has been sent
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Check that
     * 1) the RST bit is set
     * 2) the TCP checksum is correct
     * 3) IP source and IP destination are correct
     * 4) the sequence number is 0
     * 5) the ACK-flag is set
     * 6) the acknowledgement number is the sequence number of the offending segment plus its length
     */
    rst_seq_no = (u32*) (payload + 4);
    rst_ack_no = (u32*) (payload + 8);
    ctrl_flags = *((u8*)(payload + 13));
    ASSERT(ctrl_flags == (1 << 2) + (1 << 4));
    ASSERT(0 == validate_tcp_checksum(20, (u16*) payload, ip_src, ip_dst));
    ASSERT(ip_src == 0x1402000a);
    ASSERT(ip_dst == 0x1502000a);
    ASSERT(*rst_seq_no == 0);
    ASSERT(*rst_ack_no == htonl(200));
    return 0;
}

/*
 * Testcase 8:
 * Call tcp_send with an empty buffer and verify that all bytes are taken over
 */
int testcase8() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    unsigned char buffer[SND_BUFFER_SIZE];
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    ASSERT(2 == socket->proto.tcp.ref_count);
    /*
     * Now try to transmit SND_BUFFER_SIZE bytes
     */
    ASSERT(SND_BUFFER_SIZE == socket->ops->send(socket, (void*) buffer, SND_BUFFER_SIZE, 0));
    ASSERT(2 == socket->proto.tcp.ref_count);
    return 0;
}

/*
 * Testcase 9:
 * Call tcp_send with an empty buffer twice so that the byte counts add up to the SND_BUFFER_SIZE
 */
int testcase9() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    unsigned char buffer[SND_BUFFER_SIZE];
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to transmit SND_BUFFER_SIZE - 100 bytes
     */
    ASSERT(SND_BUFFER_SIZE - 100 == socket->ops->send(socket, (void*) buffer, SND_BUFFER_SIZE - 100, 0));
    /*
     * and 100 bytes
     */
    ASSERT(100 == socket->ops->send(socket, (void*) buffer, 100, 0));
    return 0;
}

/*
 * Testcase 10:
 * Call tcp_send with an empty buffer twice so that the byte counts add up to the SND_BUFFER_SIZE. Verify that
 * next call returns -EAGAIN
 */
int testcase10() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    unsigned char buffer[SND_BUFFER_SIZE];
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to transmit SND_BUFFER_SIZE - 100 bytes
     */
    ASSERT(SND_BUFFER_SIZE - 100 == socket->ops->send(socket, (void*) buffer, SND_BUFFER_SIZE - 100, 0));
    /*
     * and 100 bytes
     */
    ASSERT(100 == socket->ops->send(socket, (void*) buffer, 100, 0));
    /*
     * Now buffer is full
     */
    ASSERT(-106 == socket->ops->send(socket, (void*) buffer, 1, 0));
    return 0;
}

/*
 * Testcase 11:
 * Create a socket connection with a send window of 2048. Then send 512 bytes and verify that exactly one segment is sent because
 * we can flush the send buffer and the entire data fits into one buffer
 */
int testcase11() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    int i;
    unsigned char buffer[8192];
    /*
     * Fill buffer
     */
    for (i = 0; i < 512; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 2048);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that the window size has been updated
     */
    ASSERT(2048 == socket->proto.tcp.snd_wnd);
    /*
     * Set congestion window size to a large value in order to avoid that "slow start" makes our
     * test scenario pointless
     */
    socket->proto.tcp.cwnd = 65536;
    /*
     * Now try to transmit 512 bytes
     */
    ip_tx_msg_called = 0;
    ASSERT(512 == socket->ops->send(socket, (void*) buffer, 512, 0));
    /*
     * and verify that a segment has been sent
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Look at data of the sent segment
     */
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(5 == tcp_hdr->hlength);
    segment_data = payload + sizeof(u32) * tcp_hdr->hlength;
    /*
     * as we can empty the buffer, push flag should be set
     */
    ASSERT(tcp_hdr->psh);
    /*
     * and verify that the data is equal to the content of our buffer
     */
    for (i = 0; i < 512; i++)
        ASSERT(segment_data[i] == buffer[i]);
    /*
     * Persist timer should not be set
     */
    ASSERT(0 == socket->proto.tcp.persist_timer.time);
    return 0;
}

/*
 * Testcase 12:
 * Create a socket connection with a send window of 2048. Then send 1024 bytes. As this exceeds the MSS, this will
 * create a message with MSS bytes, and the remainder will not be sent due to Nagle's algorithm
 */
int testcase12() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    int i;
    unsigned char buffer[8192];
    /*
     * Fill buffer
     */
    for (i = 0; i < 1024; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 2048);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that the window size has been updated
     */
    ASSERT(2048 == socket->proto.tcp.snd_wnd);
    /*
     * Set congestion window size to a large value in order to avoid that "slow start" makes our
     * test scenario pointless
     */
    socket->proto.tcp.cwnd = 65536;
    /*
     * Now try to transmit 512 bytes
     */
    ip_tx_msg_called = 0;
    cond_broadcast_called = 0;
    ASSERT(1024 == socket->ops->send(socket, (void*) buffer, 1024, 0));
    /*
     * and verify that only one segment has been sent
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * As there is data left in buffer, this should not have the push flag set
     */
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(0 == tcp_hdr->psh);
    return 0;
}

/*
 * Testcase 13:
 * Create a socket connection with a send window of 128, and a maximum window size of 200 bytes.
 * Then send 256 bytes. Even though the buffer cannot be flushed with this send
 * and we do not have enough data to fill a segment, a segment will be sent as we exceed one half of the maximum window size
 */
int testcase13() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    int i;
    unsigned char buffer[8192];
    /*
     * Fill buffer
     */
    for (i = 0; i < 256; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window 128
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 128);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that the window size has been updated
     */
    ASSERT(128 == socket->proto.tcp.snd_wnd);
    /*
     * Fake maximum window size
     */
    socket->proto.tcp.max_wnd = 200;
    /*
     * Now try to transmit 256 bytes
     */
    ip_tx_msg_called = 0;
    ASSERT(256 == socket->ops->send(socket, (void*) buffer, 256, 0));
    /*
     * and verify that a segment has been sent
     */
    ASSERT( 1 == ip_tx_msg_called);
    /*
     * Validate data
     */
    tcp_hdr = (tcp_hdr_t*) payload;
    segment_data = payload + sizeof(u32) * tcp_hdr->hlength;
    for (i = 0; i < 256; i++)
        ASSERT(segment_data[i] == buffer[i]);
    /*
     * Verify that ACK bit is set (RFC 793:  "Once in the established state, all segments must carry current
     * acknowledgement information") and that the ACK_NO is 2
     */
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(2 == ntohl(tcp_hdr->ack_no));
    /*
     * push bit should not be set
     */
    ASSERT(0 == tcp_hdr->psh);
    return 0;
}

/*
 * Testcase 14:
 * Create a socket connection with a send window of 128, but a maximum window size of 8192.
 * Then send 256 bytes. As the buffer cannot be flushed with this send
 * and we do not have enough data to fill a segment, no data will be sent
 */
int testcase14() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    int i;
    unsigned char buffer[8192];
    /*
     * Fill buffer
     */
    for (i = 0; i < 256; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window 128
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 128);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that the window size has been updated
     */
    ASSERT(128 == socket->proto.tcp.snd_wnd);
    /*
     * Set congestion window size to a large value in order to avoid that "slow start" makes our
     * test scenario pointless
     */
    socket->proto.tcp.cwnd = 65536;
    /*
     * Fake maximum window size
     */
    socket->proto.tcp.max_wnd = 8192;
    /*
     * Now try to transmit 256 bytes
     */
    ip_tx_msg_called = 0;
    cond_broadcast_called = 0;
    ASSERT(256 == socket->ops->send(socket, (void*) buffer, 256, 0));
    /*
     * and verify that no segment has been sent
     */
    ASSERT( 0 == ip_tx_msg_called);
    ASSERT( 0 == cond_broadcast_called);
    return 0;
}

/*
 * Testcase 15:
 * Create a socket connection with a send window U = 600.
 * Then send 700 bytes. This will trigger the transmission of one segment of data with 536 bytes.
 * For the remaining 164 bytes, the decision algorith will be repeated with D = 164,  U = 64. However, this time
 * min(D,U) < MSS and SND_NXT != SND_UNA, so now data will be sent
 */
int testcase15() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    int i;
    unsigned char buffer[8192];
    /*
     * Fill buffer
     */
    for (i = 0; i < 700; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window 600
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 600);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that the window size has been updated
     */
    ASSERT(600 == socket->proto.tcp.snd_wnd);
    /*
     * Set congestion window size to a large value in order to avoid that "slow start" makes our
     * test scenario pointless
     */
    socket->proto.tcp.cwnd = 65536;
    /*
     * Now try to transmit 700 bytes
     */
    ip_tx_msg_called = 0;
    ASSERT(700 == socket->ops->send(socket, (void*) buffer, 700, 0));
    /*
     * and verify that only one segment has been sent
     */
    ASSERT( 1 == ip_tx_msg_called);
    /*
     * Also verify that even though the data has been sent, it is still in the send
     * queue
     */
    ASSERT(0 == socket->proto.tcp.snd_buffer_head);
    return 0;
}

/*
 * Testcase 16:
 * Create a socket and establish a connection. Then simulate receipt of a single segment containing 128 bytes of data
 *
 *  #   Socket under test                                         Peer
 * -------------------------------------------------------------------------------------------------------
 *
 *  1   SYN, SEQ = syn_seq_no, ACK_NO = 0 ----------------------->
 *  2                                                         <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *  3   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 0 ---------->
 *  4                                                         <-- ACK, SEQ = 2, ACK_NO = syn_seq_no + 1, LEN = 128
 */
int testcase16() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    net_msg_t* text;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    int i;
    unsigned char buffer[8192];
    /*
     * Fill buffer
     */
    for (i = 0; i < 128; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    net_init();
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    ASSERT(2 == socket->proto.tcp.ref_count);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window 600
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 600);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Reference count should be unchanged
     */
    ASSERT(2 == socket->proto.tcp.ref_count);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that the window size has been updated
     */
    ASSERT(600 == socket->proto.tcp.snd_wnd);
    /*
     * Put together segment #4
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1, 600, buffer, 128);
    ip_tx_msg_called = 0;
    __net_loglevel = 0;
    tcp_rx_msg(text);
    __net_loglevel = 0;
    /*
     * Verify that no immediate response is sent - the ACK will be delayed!
     */
    ASSERT(0 == ip_tx_msg_called);
    /*
     * and that 128 bytes have been added to the receive queue
     */
    ASSERT(0 == socket->proto.tcp.rcv_buffer_head);
    ASSERT(128 == socket->proto.tcp.rcv_buffer_tail);
    /*
     * Check data
     */
    for (i = 0; i < 128; i++)
        ASSERT(socket->proto.tcp.rcv_buffer[i] == buffer[i]);
    /*
     * Check for memory leaks
     */
    unsigned int created;
    unsigned int destroyed;
    net_get_counters(&created, &destroyed);
    ASSERT(created == destroyed);
    return 0;
}

/*
 * Testcase 17:
 * Create a socket and establish a connection. Then simulate receipt of a single segment containing 128 bytes of data
 * and receipt of a second segment which does not overlap the first segment
 *
 *  #   Socket under test                                         Peer
 * -------------------------------------------------------------------------------------------------------
 *
 *  1   SYN, SEQ = syn_seq_no, ACK_NO = 0 ----------------------->
 *  2                                                         <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *  3   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 0 ---------->
 *  4                                                         <-- ACK, SEQ = 2, ACK_NO = syn_seq_no + 1, LEN = 128
 *  5                                                         <-- ACK, SEQ = 130, ACK_NO = syn_seq_no + 1, LEN = 128
 */
int testcase17() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    net_msg_t* text;
    u32 syn_seq_no = 0;
    u32 syn_win_size = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    int i;
    unsigned char buffer[8192];
    /*
     * Fill buffer
     */
    for (i = 0; i < 256; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number and window size from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    syn_win_size = htons(*((u16*) (payload + 14)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window 600
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 600);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that the window size has been updated
     */
    ASSERT(600 == socket->proto.tcp.snd_wnd);
    /*
     * Put together next segment
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1, 600, buffer, 128);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 130, syn_seq_no + 1, 600, buffer + 128, 128);
    tcp_rx_msg(text);
    /*
     * Verify that no immediate response is sent - the ACK will be delayed!
     */
    ASSERT(0 == ip_tx_msg_called);
    /*
     * and that 256 bytes have been added to the receive queue
     */
    ASSERT(0 == socket->proto.tcp.rcv_buffer_head);
    ASSERT(256 == socket->proto.tcp.rcv_buffer_tail);
    /*
     * Verify that the receive buffer space has been reduced by 256 bytes
     */
    ASSERT(256 == socket->proto.tcp.rcv_buffer_tail);
    /*
     * Check data
     */
    for (i = 0; i < 256; i++)
        ASSERT(socket->proto.tcp.rcv_buffer[i] == buffer[i]);
    return 0;
}

/*
 * Testcase 18:
 * Create a socket and establish a connection. Then simulate receipt of a single segment containing 128 bytes of data
 * and receipt of a second segment which overlaps the first segment on the left
 *
 *  #   Socket under test                                         Peer
 * -------------------------------------------------------------------------------------------------------
 *
 *  1   SYN, SEQ = syn_seq_no, ACK_NO = 0 ----------------------->
 *  2                                                         <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *  3   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 0 ---------->
 *  4                                                         <-- ACK, SEQ = 2, ACK_NO = syn_seq_no + 1, LEN = 128
 *  5                                                         <-- ACK, SEQ = 110, ACK_NO = syn_seq_no + 1, LEN = 128
 */
int testcase18() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    net_msg_t* text;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    int i;
    unsigned char buffer[8192];
    /*
     * Fill buffer
     */
    for (i = 0; i < 256; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window 600
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 600);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that the window size has been updated
     */
    ASSERT(600 == socket->proto.tcp.snd_wnd);
    /*
     * At this point, we have exchanged the first three segments of the simulated
     * session - assemble and send fourth and fifth segment which overlap by 20 bytes
     *
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1, 600, buffer, 128);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 110, syn_seq_no + 1, 600, buffer + 108, 128);
    tcp_rx_msg(text);
    /*
     * Verify that no immediate response is sent - the ACK will be delayed!
     */
    ASSERT(0 == ip_tx_msg_called);
    /*
     * and that 256 - 20 bytes have been added to the receive queue
     */
    ASSERT(0 == socket->proto.tcp.rcv_buffer_head);
    ASSERT(256 - 20 == socket->proto.tcp.rcv_buffer_tail);
    /*
     * Check data
     */
    for (i = 0; i < 256 - 20; i++) {
        if (socket->proto.tcp.rcv_buffer[i] != buffer[i])
            printf("Have wrong data at position %d, have %d, expected %d\n", i, socket->proto.tcp.rcv_buffer[i], buffer[i]);
        ASSERT(socket->proto.tcp.rcv_buffer[i] == buffer[i]);
    }
    return 0;
}

/*
 * Testcase 19:
 * Create a socket and establish a connection. Then simulate receipt of a single segment containing 128 bytes of data
 * and receipt of a second segment which is not located at the left edge of the window. Verify that an immediate duplicate ACK
 * is generated and that the second segment is not copied to the receive buffer
 *
 *  #   Socket under test                                         Peer
 * -------------------------------------------------------------------------------------------------------
 *
 *  1   SYN, SEQ = syn_seq_no, ACK_NO = 0 ----------------------->
 *  2                                                         <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *  3   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 0 ---------->
 *  4                                                         <-- ACK, SEQ = 2, ACK_NO = syn_seq_no + 1, LEN = 128
 *  5                                                         <-- ACK, SEQ = 514, ACK_NO = syn_seq_no + 1, LEN = 128
 *  6   SEQ = syn_seq_no + 1, ACK_NO = 130, LEN = 0 ------------->
 */
int testcase19() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    net_msg_t* text;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u16 syn_win_size;
    u8* segment_data;
    int i;
    unsigned char buffer[8192];
    /*
     * Fill buffer
     */
    for (i = 0; i < 1024; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    net_init();
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number and window size from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    syn_win_size = htons(*((u16*) (payload + 14)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window 600
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 600);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that the window size has been updated
     */
    ASSERT(600 == socket->proto.tcp.snd_wnd);
    /*
     * Assemble next segment
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1, 600, buffer, 128);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    /*
     * This should not create any response
     */
    ASSERT(0 == ip_tx_msg_called);
    /*
     * but should have added 128 bytes to the receive queue
     */
    ASSERT(0 == socket->proto.tcp.rcv_buffer_head);
    ASSERT(128 == socket->proto.tcp.rcv_buffer_tail);
    /*
     * Check data
     */
    for (i = 0; i < 128; i++) {
        if (socket->proto.tcp.rcv_buffer[i] != buffer[i])
            printf("Have wrong data at position %d, have %d, expected %d\n", i, socket->proto.tcp.rcv_buffer[i], buffer[i]);
        ASSERT(socket->proto.tcp.rcv_buffer[i] == buffer[i]);
    }
    /*
     * Now simulate receipt of out-of-order segment. Before we do this, we simulate a non-empty send buffer
     */
    socket->proto.tcp.snd_buffer_tail = 128;
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 514, syn_seq_no + 1, 600, buffer + 512, 128);
    tcp_rx_msg(text);
    /*
     * Verify that an immediate ACK has been sent
     */
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(tcp_hdr->ack);
    ASSERT(130 == ntohl(tcp_hdr->ack_no));
    ASSERT(20 == ip_payload_len);
    /*
     * sequence number in ACK should be our SND.NXT
     */
    ASSERT(ntohl(tcp_hdr->seq_no) == socket->proto.tcp.snd_nxt);
    /*
     * Window size should be the initial window size reduced by 128
     */
    ASSERT(syn_win_size - 128 == htons(*((u16*) (payload + 14))));
    /*
     * Check for memory leaks
     */
    unsigned int created;
    unsigned int destroyed;
    net_get_counters(&created, &destroyed);
    ASSERT(created == destroyed);
    return 0;
}

/*
 * Testcase 20:
 * In this testcase, we look at a more realistic example combining send and receive. First, we establish a connection
 * and set the window size of the local socket to 600. The three-way connection handshake is formed by the messages 1- 3.
 *
 * We then call send with 1024 bytes. This will trigger the creation
 * of a first segment with 536 bytes. The remaining 488 bytes will be kept in the send buffer of the local socket due
 * to Nagle's algorithm.
 *
 * Next we simulate receipt of a message from the peer which contains 128 bytes of data and at the same time acknowledges
 * our previous message. When this segment is received, the data will be copied to the receive buffer and the ACK will
 * trigger the send of the remaining data. Piggybacked on this segment, we will also transfer the ACK for the first 128 bytes received
 * (this is packet 6)
 *
 * Then we simulate receipt of an out-of-order message, i.e. we simulate the case that a segment our peer has emitted is lost.
 * This should trigger a duplicate ACK
 *
 *  #   Socket under test                                         Peer
 * -------------------------------------------------------------------------------------------------------
 *
 *  1   SYN, SEQ = syn_seq_no, ACK_NO = 0 ----------------------->
 *  2                                                         <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *  3   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 0 ---------->
 *  4   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 536 -------->
 *  5                                                         <-- ACK, SEQ = 2, ACK_NO = syn_seq_no + 537, LEN = 128
 *  6   ACK, SEQ = syn_seq_no + 537, ACK_NO = 130, LEN = 488 ---->
 *  7                                                         <-- ACK, SEQ = 514, ACK_NO = syn_seq_no + 1025, LEN = 128
 *  8   SEQ = syn_seq_no + 129, ACK_NO = 130, LEN = 0 ----------->
 *
 */
int testcase20() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    net_msg_t* text;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    int i;
    u32 old_win;
    unsigned char buffer[8192];
    /*
     * Fill buffer
     */
    for (i = 0; i < 1024; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    tcp_init();
    net_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window 600
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 600);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that the window size has been updated
     */
    ASSERT(600 == socket->proto.tcp.snd_wnd);
    /*
     * Now try to transmit 1024 bytes
     */
    ip_tx_msg_called = 0;
    ASSERT(1024 == socket->ops->send(socket, (void*) buffer, 1024, 0));
    /*
     * and verify that a segment containing 536 data bytes has been sent
     */
    ASSERT( 1 == ip_tx_msg_called);
    ASSERT (20 + 536 == ip_payload_len);
    /*
     * At this point, we have exchanged the first four segments of the simulated
     * session - assemble and send fifth segment
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 537, 600, buffer, 128);
    ip_tx_msg_called = 0;
    cond_broadcast_called = 0;
    tcp_rx_msg(text);
    /*
     * Verify that cond_broadcast has been called on socket->buffer_change
     * as the acknowledgement has updated the head of the send buffer
     * (we will also do a broadcast on rcv_buffer_change, thus we cannot
     * check last_cond)
     */
    ASSERT(cond_broadcast_called);
    /*
     * This should have created message 6 - validate it
     */
    ASSERT(1 == ip_tx_msg_called);
    ASSERT(20 + 488 == ip_payload_len);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(ntohl(tcp_hdr->seq_no) == syn_seq_no + 537);
    ASSERT(ntohl(tcp_hdr->ack_no) == 130);
    ASSERT(tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->syn);
    old_win = ntohs(tcp_hdr->window);
    /*
     * Build and receive message 7
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 514, syn_seq_no + 1025, 600, buffer, 128);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    /*
     * This should create an empty duplicate ACK, using the same window size as the previous ACK
     */
    ASSERT(1 == ip_tx_msg_called);
    ASSERT(20 == ip_payload_len);
    ASSERT(130 == ntohl(tcp_hdr->ack_no));
    ASSERT(ntohl(tcp_hdr->seq_no) == syn_seq_no + 1025);
    ASSERT(tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->syn);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(old_win == ntohs(tcp_hdr->window));
    /*
     * Check for memory leaks
     */
    unsigned int created;
    unsigned int destroyed;
    net_get_counters(&created, &destroyed);
    ASSERT(created == destroyed);
    return 0;
}

/*
 * Testcase 21:
 *
 * Test correct processing of cumulative ACKs
 *
 * After setting up the connection, simulating a peer with window size 14600, we call send once with 100 bytes which
 * will force delivery of a first segment (as there is no data in flight yet)
 *
 * Then we call send another 6 times, each times with 100 byte of data. When the sixth packet has been added to the send
 * queue, this will create another segment as now more than 536 bytes are unsent.
 *
 * We then simulate a cumulative ACK for these two segments.
 *
 *  #   Socket under test                                         Peer
 * -------------------------------------------------------------------------------------------------------
 *
 *  1   SYN, SEQ = syn_seq_no, ACK_NO = 0 ----------------------->
 *  2                                                         <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *  3   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 0 ---------->
 *  4   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 100 -------->
 *  5   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 536 -------->
 *  5                                                         <-- ACK, SEQ = 2, ACK_NO = syn_seq_no + 637, LEN = 0
 *  6   SEQ = syn_seq_no + 129, ACK_NO = 130, LEN = 0 ----------->
 *
 */
int testcase21() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    net_msg_t* text;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    int i;
    unsigned char buffer[8192];
    /*
     * Fill buffer
     */
    for (i = 0; i < 1024; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window 14600
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 14600);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that the window size has been updated
     */
    ASSERT(14600 == socket->proto.tcp.snd_wnd);
    /*
     * Now try to transmit 100 bytes
     */
    ip_tx_msg_called = 0;
    ASSERT(100 == socket->ops->send(socket, (void*) buffer, 100, 0));
    /*
     * and verify that a segment containing 100 data bytes has been sent
     */
    ASSERT( 1 == ip_tx_msg_called);
    ASSERT (20 + 100 == ip_payload_len);
    /*
     * In a loop, send another 500 bytes
     */
    ip_tx_msg_called = 0;
    for (i = 0; i < 5; i++) {
        ASSERT(100 == socket->ops->send(socket, (void*) buffer + i*100 + 100, 100, 0));
        ASSERT(0 == ip_tx_msg_called);
    }
    /*
     * The next send should create a segment again
     */
    ASSERT(100 == socket->ops->send(socket, (void*) buffer + 600, 100, 0));
    ASSERT( 1 == ip_tx_msg_called);
    ASSERT (20 + 536 == ip_payload_len);
    /*
     * Simulate ACK
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 637, 14600, buffer, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    /*
     * We should now see the last segment, containing 700 - 536 - 100 = 64 bytes
     */
    ASSERT( 1 == ip_tx_msg_called);
    ASSERT (20 + 64 == ip_payload_len);
    return 0;
}

/*
 * Testcase 22:
 * Create a socket and establish a connection. Then simulate receipt of two full segments of data. Read data from the receive
 * buffer and verify that this forces sending of an ACK
 *
 *  #   Socket under test                                         Peer
 * -------------------------------------------------------------------------------------------------------
 *
 *  1   SYN, SEQ = syn_seq_no, ACK_NO = 0 ----------------------->
 *  2                                                         <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *  3   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 0 ---------->
 *  4                                                         <-- ACK, SEQ = 2, ACK_NO = syn_seq_no + 1, LEN = 536
 *  5                                                         <-- ACK, SEQ = 538, ACK_NO = syn_seq_no + 1, LEN = 536
 */
int testcase22() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    net_msg_t* text;
    u32 syn_seq_no = 0;
    u32 syn_win_size = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    int i;
    unsigned char buffer[2048];
    unsigned char rcv_buffer[2048];
    /*
     * Fill buffer
     */
    for (i = 0; i < 536*2; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number and window size from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    syn_win_size = htons(*((u16*) (payload + 14)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window 600
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 600);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that the window size has been updated
     */
    ASSERT(600 == socket->proto.tcp.snd_wnd);
    /*
     * Call recv and verify that no message is generated and that -EAGAIN is returned
     */
    ip_tx_msg_called = 0;
    ASSERT(-106 == socket->ops->recv(socket, buffer, 512, 0));
    ASSERT(0 == ip_tx_msg_called);
    /*
     * Put together next segments
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1, 600, buffer, 536);
    ip_tx_msg_called = 0;
    cond_broadcast_called = 0;
    tcp_rx_msg(text);
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 536 + 2, syn_seq_no + 1, 600, buffer + 536, 536);
    tcp_rx_msg(text);
    /*
     * Verify that no immediate response is sent - the ACK will be delayed!
     */
    ASSERT(0 == ip_tx_msg_called);
    /*
     * and that 536*2 bytes have been added to the receive queue
     */
    ASSERT(0 == socket->proto.tcp.rcv_buffer_head);
    ASSERT(536*2 == socket->proto.tcp.rcv_buffer_tail);
    /*
     * and a broadcast has been issued on buffer_change
     */
    ASSERT(2 == cond_broadcast_called);
    ASSERT(last_cond == &socket->rcv_buffer_change);
    /*
     * Check data
     */
    for (i = 0; i < 536*2; i++)
        ASSERT(socket->proto.tcp.rcv_buffer[i] == buffer[i]);
    /*
     * Now read 536*2 bytes
     */
    ip_tx_msg_called = 0;
    ASSERT(2*536 == socket->ops->recv(socket, rcv_buffer, 2*536, 0));
    /*
     * and verify that an ACK has been sent
     */
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->syn);
    ASSERT(0 == tcp_hdr->fin);
    /*
     * As we have read all data, the window size should be the original size again
     */
    ASSERT(8192 == ntohs(tcp_hdr->window));
    /*
     * Verify data
     */
    for (i = 0; i < 2*536;i++)
        ASSERT(buffer[i] == rcv_buffer[i]);
    return 0;
}

/*
 * Testcase 23:
 * Create a socket and try to call recv without having established a connection
 */
int testcase23() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    net_msg_t* text;
    u32 syn_seq_no = 0;
    u32 syn_win_size = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    int i;
    unsigned char buffer[2048];
    unsigned char rcv_buffer[2048];
    /*
     * Fill buffer
     */
    for (i = 0; i < 536*2; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Call recv
     */
    ASSERT(-136 == socket->ops->recv(socket, buffer, 100, 0));
    return 0;
}

/*
 * Testcase 24:
 * Create a socket and establish a connection. Then simulate receipt of a full segment of data. Remove the data from the buffer
 * and verify that no ACK is sent yet. Repeat this and verify that the second time, a ACK is sent
 *
 *  #   Socket under test                                         Peer
 * -------------------------------------------------------------------------------------------------------
 *
 *  1   SYN, SEQ = syn_seq_no, ACK_NO = 0 ----------------------->
 *  2                                                         <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *  3   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 0 ---------->
 *  4                                                         <-- ACK, SEQ = 2, ACK_NO = syn_seq_no + 1, LEN = 536
 *  5                                                         <-- ACK, SEQ = 538, ACK_NO = syn_seq_no + 1, LEN = 536
 */
int testcase24() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    net_msg_t* text;
    u32 syn_seq_no = 0;
    u32 syn_win_size = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    int i;
    unsigned char buffer[2048];
    unsigned char rcv_buffer[2048];
    /*
     * Fill buffer
     */
    for (i = 0; i < 536*2; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number and window size from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    syn_win_size = htons(*((u16*) (payload + 14)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window 600
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 600);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that the window size has been updated
     */
    ASSERT(600 == socket->proto.tcp.snd_wnd);
    /*
     * Call recv and verify that no message is generated and that -EAGAIN
     */
    ip_tx_msg_called = 0;
    ASSERT(-106 == socket->ops->recv(socket, buffer, 512, 0));
    ASSERT(0 == ip_tx_msg_called);
    /*
     * Put together next segment
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1, 600, buffer, 536);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    /*
     * Verify that no immediate response is sent - the ACK will be delayed!
     */
    ASSERT(0 == ip_tx_msg_called);
    /*
     * and that 536 bytes have been added to the receive queue
     */
    ASSERT(0 == socket->proto.tcp.rcv_buffer_head);
    ASSERT(536 == socket->proto.tcp.rcv_buffer_tail);
    /*
     * Now read 536 bytes
     */
    ip_tx_msg_called = 0;
    ASSERT(536 == socket->ops->recv(socket, rcv_buffer, 536, 0));
    /*
     * and verify that no ACK is sent
     */
    ASSERT(0 == ip_tx_msg_called);
    /*
     * Check data
     */
    for (i = 0; i < 536;i++)
        ASSERT(buffer[i] == rcv_buffer[i]);
    /*
     * Send next segment
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 536 + 2, syn_seq_no + 1, 600, buffer + 536, 536);
    tcp_rx_msg(text);
    /*
     * Read more data
     */
    ASSERT(536 == socket->ops->recv(socket, rcv_buffer + 536, 536, 0));
    /*
     * and verify that an ACK has been sent
     */
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->syn);
    ASSERT(0 == tcp_hdr->fin);
    /*
     * As we have read all data, the window size should be the original size again
     */
    ASSERT(8192 == ntohs(tcp_hdr->window));
    /*
     * Verify all data
     */
    for (i = 0; i < 2*536;i++)
        ASSERT(buffer[i] == rcv_buffer[i]);
    return 0;
}

/*
 * Testcase 25:
 *
 * Test retransmission based on retransmission timer - retransmit one segment only
 *
 *
 *  #   Socket under test                                         Peer
 * -------------------------------------------------------------------------------------------------------
 *
 *  1   SYN, SEQ = syn_seq_no, ACK_NO = 0 ----------------------->
 *  2                                                         <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *  3   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 0 ---------->
 *  4   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 100 -------->
 *  5   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 100 -------->
 *
 */
int testcase25() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    net_msg_t* text;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    int i;
    unsigned char buffer[8192];
    /*
     * Fill buffer
     */
    for (i = 0; i < 1024; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    tcp_init();
    net_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window 14600
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 14600);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that the window size has been updated
     */
    ASSERT(14600 == socket->proto.tcp.snd_wnd);
    /*
     * Now  transmit 100 bytes
     */
    ip_tx_msg_called = 0;
    ASSERT(100 == socket->ops->send(socket, (void*) buffer, 100, 0));
    /*
     * and verify that a segment containing 100 data bytes has been sent
     */
    ASSERT( 1 == ip_tx_msg_called);
    ASSERT (20 + 100 == ip_payload_len);
    /*
     * Check that retransmission timer is set to 1 second
     */
    ASSERT(socket->proto.tcp.rtx_timer.time == TCP_HZ);
    /*
     * Now simulate first ticks - should not change anything
     */
    ip_tx_msg_called = 0;
    for (i = 0; i < TCP_HZ - 1; i++)
        tcp_do_tick();
    ASSERT(ip_tx_msg_called == 0);
    /*
     * and simulate tick TCP_HZ - this should initiate the retransmission
     */
    tcp_do_tick();
    ASSERT(1 == ip_tx_msg_called);
    /*
     * and should have set the timer to twice its initial value ("exponential backoff")
     */
    ASSERT(socket->proto.tcp.rtx_timer.time == 2*TCP_HZ);
    /*
     * Check data
     */
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(syn_seq_no + 1 == ntohl(tcp_hdr->seq_no));
    ASSERT(100 == ip_payload_len - tcp_hdr->hlength*sizeof(u32));
    for (i = 0; i < 100; i++) {
        ASSERT(buffer[i] == ((u8*)(payload + tcp_hdr->hlength*sizeof(u32)))[i]);
    }
    /*
     * Check for memory leaks
     */
    unsigned int created;
    unsigned int destroyed;
    net_get_counters(&created, &destroyed);
    ASSERT(created == destroyed);
    return 0;
}

/*
 * Testcase 26:
 *
 * Test retransmission based on retransmission timer - send two segments before retransmission occurs
 *
 *
 *  #   Socket under test                                         Peer
 * -------------------------------------------------------------------------------------------------------
 *
 *  1   SYN, SEQ = syn_seq_no, ACK_NO = 0 ----------------------->
 *  2                                                         <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *  3   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 0 ---------->
 *  4   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 100 -------->
 *  5   ACK, SEQ = syn_seq_no + 101, ACK_NO = 2, LEN = 536 ------>
 *  6                                                         <-- ACK. SEQ = 2, ACK_NO = syn_seq_no + 637, LEN = 0
 *
 */
int testcase26() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    net_msg_t* text;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    int i;
    unsigned char buffer[8192];
    /*
     * Fill buffer
     */
    for (i = 0; i < 1024; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window 14600
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 14600);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that the window size has been updated
     */
    ASSERT(14600 == socket->proto.tcp.snd_wnd);
    /*
     * Now  transmit 100 bytes
     */
    ip_tx_msg_called = 0;
    ASSERT(100 == socket->ops->send(socket, (void*) buffer, 100, 0));
    /*
     * and verify that a segment containing 100 data bytes has been sent
     */
    ASSERT( 1 == ip_tx_msg_called);
    ASSERT (20 + 100 == ip_payload_len);
    /*
     * Check that retransmission timer is set to 1 second,  - this is still the initial value as
     * our RTT sample was zero and thus the RTO calculation used the minimum value
     */
    ASSERT(socket->proto.tcp.rtx_timer.time == TCP_HZ);
    /*
     * Now simulate 2 ticks - should not change anything
     */
    ip_tx_msg_called = 0;
    for (i = 0; i < 2; i++)
        tcp_do_tick();
    ASSERT(ip_tx_msg_called == 0);
    /*
     * Verify that timer has been reduced by two
     */
    ASSERT(socket->proto.tcp.rtx_timer.time == TCP_HZ - 2);
    /*
     * send another packet - need to send 536 bytes to force sending (Nagle)
     */
    ASSERT(536 == socket->ops->send(socket, (void*) buffer, 536, 0));
    ASSERT(1 == ip_tx_msg_called);
    /*
     * and verify that timer has been left alone
     */
    ASSERT(socket->proto.tcp.rtx_timer.time ==TCP_HZ - 2);
    /*
     * Receive ACK for first segment
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 101, 14600, buffer, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    /*
     * and verify that timer is reset
     */
    ASSERT(socket->proto.tcp.rtx_timer.time == TCP_HZ);
    /*
     * Now receive ACK for second segment
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 637, 14600, buffer, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    /*
     * and verify that retransmission timer has been cancelled
     */
    ASSERT(socket->proto.tcp.rtx_timer.time == 0);
    return 0;
}

/*
 * Testcase 27:
 *
 * Test RTO calculation with only one RTT sample
 *
 *
 *  #   Socket under test                                         Peer
 * -------------------------------------------------------------------------------------------------------
 *
 *  1   SYN, SEQ = syn_seq_no, ACK_NO = 0 ----------------------->
 *  2                                                         <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *
 */
int testcase27() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    net_msg_t* text;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    int i;
    unsigned char buffer[8192];
    /*
     * Fill buffer
     */
    for (i = 0; i < 1024; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window 14600
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 14600);
    /*
     * and simulate receipt of the message, but only after 37 ticks (ca. 1,5 seconds) have passed
     */
    for (i = 0; i < 37; i++)
        tcp_do_tick();
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that the window size has been updated
     */
    ASSERT(14600 == socket->proto.tcp.snd_wnd);
    /*
     * SRTT should now be the RTT sample, i.e. 37 ticks
     * RTTVAR should be half of that, i.e. 18,5 ticks
     */
    ASSERT(socket->proto.tcp.srtt >> SRTT_SHIFT == 37);
    ASSERT(socket->proto.tcp.rttvar >> SRTT_SHIFT == 18);
    /*
     * and RTO should be SRTT + 4*RTTVAR = 111 ticks, i.e. approx. 4 seconds
     */
    ASSERT(socket->proto.tcp.rto == 111);
    return 0;
}

/*
 * Testcase 28:
 *
 * Test RTO calculation with two RTT samples
 *
 *
 *  #   Socket under test                                         Peer
 * -------------------------------------------------------------------------------------------------------
 *
 *  1   SYN, SEQ = syn_seq_no, ACK_NO = 0 ----------------------->
 *  2                                                         <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *  4   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 100 -------->
 *  5                                                         <-- ACK. SEQ = 2, ACK_NO = syn_seq_no + 101, LEN = 0
 *
 *
 */
int testcase28() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    net_msg_t* text;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    int i;
    unsigned char buffer[8192];
    /*
     * Fill buffer
     */
    for (i = 0; i < 1024; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window 14600
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 14600);
    /*
     * and simulate receipt of the message, but only after 38 ticks
     */
    for (i = 0; i < 38; i++)
        tcp_do_tick();
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that the window size has been updated
     */
    ASSERT(14600 == socket->proto.tcp.snd_wnd);
    /*
     * SRTT should now be the RTT sample, i.e. 38 ticks
     * RTTVAR should be half of that, i.e. 19 ticks
     */
    ASSERT(socket->proto.tcp.srtt >> SRTT_SHIFT == 38);
    ASSERT(socket->proto.tcp.rttvar >> SRTT_SHIFT == 19);
    /*
     * and RTO should be SRTT + 4*RTTVAR = 114 ticks, i.e. 4,5 seconds
     */
    ASSERT(socket->proto.tcp.rto == 114);
    /*
     * Now send 100 bytes
     */
    ASSERT(100 == socket->ops->send(socket, buffer, 100, 0));
    /*
     * this should have set the RTT timer
     */
    ASSERT(-1 != socket->proto.tcp.current_rtt);
    /*
     * and receive ACK after 98 ticks (sligthly less than 4 seconds)
     */
    ip_tx_msg_called = 0;
    for (i = 0; i < 98; i++)
        tcp_do_tick();
    ASSERT(0 == ip_tx_msg_called);
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 101, 14600, buffer, 0);
    tcp_rx_msg(text);
    /*
     * Now we should have
     * DELTA = current sample - SRTT = 98 - 38 = 60
     * RTTVAR <- 3/4 RTTVAR + 1/4 * | DELTA | = 3/4 * 19 + 1/4 * 60 = 117 / 4 = 29,25
     * SRTT = 7/8 * SRTT + 1/8 * current_sample = 7/8 * 38 + 1/8 * 98 = 364 / 8 = 45,5
     */
    ASSERT(socket->proto.tcp.rttvar >> (SRTT_SHIFT - 2) == 117);
    ASSERT(socket->proto.tcp.srtt >> (SRTT_SHIFT - 1) == 91);
    return 0;
}

/*
 * Testcase 29:
 * Create a socket and establish a connection. Then simulate receipt of a single segment containing 128 bytes of data.
 * Verify that as the delayed ACK timer expires, an ACK is sent
 *
 *  #   Socket under test                                         Peer
 * -------------------------------------------------------------------------------------------------------
 *
 *  1   SYN, SEQ = syn_seq_no, ACK_NO = 0 ----------------------->
 *  2                                                         <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *  3   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 0 ---------->
 *  4                                                         <-- ACK, SEQ = 2, ACK_NO = syn_seq_no + 1, LEN = 128
 *  5   ACK, SEQ = syn_seq_no + 1, ACK_NO = 130, LEN = 0
 */
int testcase29() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    net_msg_t* text;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    int i;
    unsigned char buffer[8192];
    /*
     * Fill buffer
     */
    for (i = 0; i < 128; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window 600
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 600);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that the window size has been updated
     */
    ASSERT(600 == socket->proto.tcp.snd_wnd);
    /*
     * Put together segment #4
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1, 600, buffer, 128);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    /*
     * Verify that no immediate response is sent - the ACK will be delayed!
     */
    ASSERT(0 == ip_tx_msg_called);
    /*
     * and that 128 bytes have been added to the receive queue
     */
    ASSERT(0 == socket->proto.tcp.rcv_buffer_head);
    ASSERT(128 == socket->proto.tcp.rcv_buffer_tail);
    /*
     * Check data
     */
    for (i = 0; i < 128; i++)
        ASSERT(socket->proto.tcp.rcv_buffer[i] == buffer[i]);
    /*
     * Now simulate a timer tick
     */
    ip_tx_msg_called = 0;
    tcp_do_tick();
    /*
     * This should have created an ACK
     */
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(ip_payload_len == sizeof(u32)*tcp_hdr->hlength);
    ASSERT(tcp_hdr->ack_no == htonl(130));
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->fin);
    ASSERT(0 == tcp_hdr->syn);
    /*
     * Assert that timer has been cancelled
     */
    ip_tx_msg_called = 0;
    tcp_do_tick();
    ASSERT(0 == ip_tx_msg_called);
    return 0;
}

/*
 * Testcase 30:
 * Create a socket and establish a connection. Then simulate receipt of two consecutive segments
 * Verify that as the delayed ACK timer expires, an ACK is sent
 *
 *  #   Socket under test                                         Peer
 * -------------------------------------------------------------------------------------------------------
 *
 *  1   SYN, SEQ = syn_seq_no, ACK_NO = 0 ----------------------->
 *  2                                                         <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *  3   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 0 ---------->
 *  4                                                         <-- ACK, SEQ = 2, ACK_NO = syn_seq_no + 1, LEN = 128
 *  5                                                         <-- ACK, SEQ = 130, ACK_NO = syn_seq_no + 1, LEN = 128
 *  6   ACK, SEQ = syn_seq_no + 1, ACK_NO = 258, LEN = 0
 */
int testcase30() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    net_msg_t* text;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    int i;
    unsigned char buffer[8192];
    /*
     * Fill buffer
     */
    for (i = 0; i < 128; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    tcp_init();
    net_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window 600
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 600);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that the window size has been updated
     */
    ASSERT(600 == socket->proto.tcp.snd_wnd);
    /*
     * Put together segment #4 and #5
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1, 600, buffer, 128);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 130, syn_seq_no + 1, 600, buffer, 128);
    tcp_rx_msg(text);
    /*
     * Verify that no immediate response is sent - the ACK will be delayed!
     */
    ASSERT(0 == ip_tx_msg_called);
    /*
     * and that 128*2 bytes have been added to the receive queue
     */
    ASSERT(0 == socket->proto.tcp.rcv_buffer_head);
    ASSERT(128*2 == socket->proto.tcp.rcv_buffer_tail);
    /*
     * Now simulate a timer tick
     */
    ip_tx_msg_called = 0;
    tcp_do_tick();
    /*
     * This should have created an ACK
     */
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(ip_payload_len == sizeof(u32)*tcp_hdr->hlength);
    ASSERT(tcp_hdr->ack_no == htonl(2+2*128));
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->fin);
    ASSERT(0 == tcp_hdr->syn);
    /*
     * Assert that timer has been cancelled
     */
    ip_tx_msg_called = 0;
    tcp_do_tick();
    ASSERT(0 == ip_tx_msg_called);
    /*
     * Check for memory leaks
     */
    unsigned int created;
    unsigned int destroyed;
    net_get_counters(&created, &destroyed);
    ASSERT(created == destroyed);
    return 0;
}


/*
 * Testcase 31:
 * Create a socket connection with a send window of 128, but a maximum window size of 8192.
 * Then send 260 bytes. As the buffer cannot be flushed with this send
 * and we do not have enough data to fill a segment, no data will be sent
 * Verify that persist timer is set and simulate that it fires. Also check that as data is
 * forced out, the persist timer is canceled as the "ACK-clock" is going again
 */
int testcase31() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    int i;
    unsigned char buffer[8192];
    /*
     * Fill buffer
     */
    for (i = 0; i < 260; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window 128
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 128);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that the window size has been updated
     */
    ASSERT(128 == socket->proto.tcp.snd_wnd);
    /*
     * Set congestion window size to a large value in order to avoid that "slow start" makes our
     * test scenario pointless
     */
    socket->proto.tcp.cwnd = 65536;
    /*
     * Fake maximum window size
     */
    socket->proto.tcp.max_wnd = 8192;
    /*
     * Now try to transmit 260 bytes
     */
    ip_tx_msg_called = 0;
    cond_broadcast_called = 0;
    ASSERT(260 == socket->ops->send(socket, (void*) buffer, 260, 0));
    /*
     * and verify that no segment has been sent
     */
    ASSERT( 0 == ip_tx_msg_called);
    ASSERT( 0 == cond_broadcast_called);
    /*
     * Verify that persist timer is set
     */
    ASSERT(socket->proto.tcp.persist_timer.time);
    /*
     * and simulate RTO ticks
     */
    for (i = 0; i < socket->proto.tcp.rto; i++) {
        tcp_do_tick();
        if (i < socket->proto.tcp.rto - 1)
            ASSERT(0 == ip_tx_msg_called);
    }
    /*
     * Last tick should have forced out a packet
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * and cleared persist timer
     */
    ASSERT(0 == socket->proto.tcp.persist_timer.time);
    /*
     * Inspect packet - we should have sent 128 bytes, thus we stay within window size
     * of peer
     */
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(128 == ip_payload_len - sizeof(u32)*tcp_hdr->hlength);
    return 0;
}

/*
 * Testcase 32:
 * Create a socket connection with a send window of 536, but a maximum window size of 8192.
 * Then send 1024 bytes. This will send a segment of 536 bytes, filling up the window of the simulated
 * peer completely.
 *
 * When the peer sends an ACK closing the window, the persist timer is set and will force delivery of a window
 * probe after it expires.
 */
int testcase32() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    net_msg_t* text;
    int i;
    unsigned char buffer[8192];
    /*
     * Fill buffer
     */
    for (i = 0; i < 1024; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window  536
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 536);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that the window size has been updated
     */
    ASSERT(536 == socket->proto.tcp.snd_wnd);
    /*
     * Set congestion window size to a large value in order to avoid that "slow start" makes our
     * test scenario pointless
     */
    socket->proto.tcp.cwnd = 65536;
    /*
     * Fake maximum window size
     */
    socket->proto.tcp.max_wnd = 8192;
    /*
     * Now try to transmit 1024 bytes
     */
    ip_tx_msg_called = 0;
    cond_broadcast_called = 0;
    ASSERT(1024 == socket->ops->send(socket, (void*) buffer, 1024, 0));
    /*
     * and verify that one segment has been sent, containing 536 bytes of data
     */
    ASSERT( 1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(536 == ip_payload_len - sizeof(u32)*tcp_hdr->hlength);
    /*
     * Persist timer should not be set, as the ACK we expect will do the job
     * for us
     */
    ASSERT(0 == socket->proto.tcp.persist_timer.time);
    /*
     * Now simulate an ACK closing the window
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1 + 536, 0, buffer, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    /*
     * As we have more data to be sent, this should have set the persist timer - simulate that it
     * fires
     */
    ASSERT(socket->proto.tcp.rto == socket->proto.tcp.persist_timer.time);
    for (i = 0; i < socket->proto.tcp.rto; i++) {
        tcp_do_tick();
        if (i < socket->proto.tcp.rto - 1)
            ASSERT(0 == ip_tx_msg_called);
    }
    /*
     * Last tick should have forced out a packet
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * As we have data to send, this packet - a zero window probe - should contain at least one byte of
     * data
     */
    ASSERT(ip_payload_len > ((tcp_hdr_t*) payload)->hlength*sizeof(u32));
    /*
     * This should have set the retransmission timer
     */
    ASSERT(socket->proto.tcp.rtx_timer.time);
    /*
     * and cleared the persist timer
     */
    ASSERT(0 == socket->proto.tcp.persist_timer.time);
    return 0;
}

/*
 * Testcase 33:
 *
 * Make sure that when receiving data and our own window is closed, we still send an ACK (data could be a window probe)
 *
 *
 *  #   Socket under test                                         Peer
 * -------------------------------------------------------------------------------------------------------
 *  1   SYN, SEQ = syn_seq_no, ACK_NO = 0 ----------------------->
 *  2                                                         <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *  3   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 0   -------->
 *  4                                                         <-- ACK, SEQ = 2, ACK_NO = syn_seq_no + 1, LEN = 8192
 *  5                                                         <-- ACK, SEQ = 8193, ACK_NO = syn_seq_no + 1, LEN = 8192
 */
int testcase33() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    net_msg_t* text;
    int i;
    unsigned char buffer[8192];
    /*
     * Fill buffer
     */
    for (i = 0; i < 8192; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window  536
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 536);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that the window size has been updated
     */
    ASSERT(536 == socket->proto.tcp.snd_wnd);
    /*
     * Set congestion window size to a large value in order to avoid that "slow start" makes our
     * test scenario pointless
     */
    socket->proto.tcp.cwnd = 65536;
    /*
     * Fake maximum window size
     */
    socket->proto.tcp.max_wnd = 8192;
    /*
     * Now simulate receipt of a message closing our own window
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1, 1460, buffer, 8192);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    /*
     * Due to delayed ACK, this should not have created an ACK
     */
    ASSERT(0 == ip_tx_msg_called);
    /*
     * Now send another packet - this should create an ACK announcing our zero window
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2 + 8192, syn_seq_no + 1, 1460, buffer, 8192);
    tcp_rx_msg(text);
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Check that ACK has window zero
     */
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->window);
    /*
     * Simulate receipt of a window probe
     */
    ip_tx_msg_called = 0;
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2 + 8192, syn_seq_no + 1, 1460, buffer, 1);
    tcp_rx_msg(text);
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Check that ACK has window zero
     */
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->window);
    return 0;
}


/*
 * Testcase 34:
 *
 * Test a typical window probing scenario and exponential backoff
 *
 *  #   Socket under test                                         Peer
 * -------------------------------------------------------------------------------------------------------
 *
 *  1   SYN, SEQ = syn_seq_no, ACK_NO = 0 ----------------------->
 *  2                                                         <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *  3   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 0 ---------->
 *  4   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 536 ---------->
 *  5                                                         <-- ACK, SEQ = 1, ACK_NO = syn_seq_no + 537, LEN = 0, WIN = 0
 *  6   ACK, SEQ = syn_seq_no + 537, ACK_NO = 2, LEN = 1 ---------->
 *  7                                                         <-- ACK, SEQ = 1, ACK_NO = syn_seq_no + 537, LEN = 0, WIN = 0
 *  8   ACK, SEQ = syn_seq_no + 537, ACK_NO = 2, LEN = 1 ---------->
 *  9                                                         <-- ACK, SEQ = 1, ACK_NO = syn_seq_no + 537, LEN = 0, WIN = 0
 *  10                                                        <-- ACK, SEQ = 1, ACK_NO = syn_seq_no + 537, LEN = 0, WIN = 8192
 *
 */
int testcase34() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    net_msg_t* text;
    int timer;
    int i;
    unsigned char buffer[8192];
    /*
     * Fill buffer
     */
    for (i = 0; i < 8192; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window  536
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 536);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that the window size has been updated
     */
    ASSERT(536 == socket->proto.tcp.snd_wnd);
    /*
     * Set congestion window size to a large value in order to avoid that "slow start" makes our
     * test scenario pointless
     */
    socket->proto.tcp.cwnd = 65536;
    /*
     * Fake maximum window size
     */
    socket->proto.tcp.max_wnd = 8192;
    /*
     * Now try to transmit 8192 bytes
     */
    ip_tx_msg_called = 0;
    cond_broadcast_called = 0;
    ASSERT(8192 == socket->ops->send(socket, (void*) buffer, 8192, 0));
    /*
     * and verify that one segment has been sent, containing 536 bytes of data
     */
    ASSERT( 1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(536 == ip_payload_len - sizeof(u32)*tcp_hdr->hlength);
    /*
     * Persist timer should not be set, as the ACK we expect will do the job
     * for us
     */
    ASSERT(0 == socket->proto.tcp.persist_timer.time);
    /*
     * Now simulate an ACK closing the window
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1 + 536, 0, buffer, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    /*
     * As we have more data to be sent, this should have set the persist timer - simulate that it
     * fires
     */
    timer = socket->proto.tcp.persist_timer.time;
    ASSERT(socket->proto.tcp.rto == socket->proto.tcp.persist_timer.time);
    for (i = 0; i < socket->proto.tcp.rto; i++) {
        tcp_do_tick();
        if (i < socket->proto.tcp.rto - 1)
            ASSERT(0 == ip_tx_msg_called);
    }
    /*
     * Last tick should have forced out a packet
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * As we have data to send, this packet - a zero window probe - should contain one byte of
     * data
     */
    ASSERT(ip_payload_len == ((tcp_hdr_t*) payload)->hlength*sizeof(u32) + 1);
    /*
     * This should have set the retransmission timer, applying exponential backoff
     */
    ASSERT(socket->proto.tcp.rtx_timer.time == 2*timer);
    /*
     * and cleared the persist timer
     */
    ASSERT(0 == socket->proto.tcp.persist_timer.time);
    /*
     * Now simulate reply - again window is closed
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1 + 536, 0, buffer, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    /*
     * Should not create an immediate reply as window is still closed
     */
    ASSERT(0 == ip_tx_msg_called);
    /*
     * but retransmission timer should be set
     */
    ASSERT(2*timer == socket->proto.tcp.rtx_timer.time);
    /*
     * Now simulate that retransmission timer fires
     */
    timer = socket->proto.tcp.rtx_timer.time;
    for (i = 0; i < timer - 1; i++) {
        tcp_do_tick();
        ASSERT(0 == ip_tx_msg_called);
    }
    tcp_do_tick();
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Check that we have sent another zero window probe
     */
    ASSERT(ip_payload_len == ((tcp_hdr_t*) payload)->hlength*sizeof(u32) + 1);
    /*
     * and that retransmission timer has doubled again
     */
    ASSERT(2*timer == socket->proto.tcp.rtx_timer.time);
    /*
     * Now simulate another ACK - window still closed
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1 + 536, 0, buffer, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    ASSERT(0 == ip_tx_msg_called);
    /*
     * Retransmission timer should still be set
     */
    ASSERT(2*timer == socket->proto.tcp.rtx_timer.time);
    /*
     * Now simulate an ACK opening the window again
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1 + 536, 8192, buffer, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    /*
     * This should trigger more data being sent - in fact we expect all but the last segments to be sent now
     */
    ASSERT(ip_tx_msg_called);
    return 0;
}

/*
 * Testcase 35
 *
 * Timeout and retransmission of a SYN - test that after two retransmission, an ACK is still accepted
 */
int testcase35() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    net_msg_t* text;
    int timer;
    int i;
    unsigned char buffer[8192];
    /*
     * Fill buffer
     */
    for (i = 0; i < 8192; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Simulate first timeout
     */
    ip_tx_msg_called = 0;
    for (i = 0; i < SYN_TIMEOUT - 1; i++) {
        tcp_do_tick();
    }
    ASSERT(0 == ip_tx_msg_called);
    tcp_do_tick();
    ASSERT(1 == ip_tx_msg_called);
    ASSERT(TCP_STATUS_SYN_SENT == socket->proto.tcp.status);
    ASSERT(syn_seq_no == htonl(*((u32*) (payload + 4))));
    /*
     * Simulate next retransmission
     */
    ip_tx_msg_called = 0;
    for (i = 0; i < SYN_TIMEOUT*2 - 1; i++) {
        tcp_do_tick();
    }
    ASSERT(0 == ip_tx_msg_called);
    tcp_do_tick();
    ASSERT(1 == ip_tx_msg_called);
    ASSERT(TCP_STATUS_SYN_SENT == socket->proto.tcp.status);
    ASSERT(syn_seq_no == htonl(*((u32*) (payload + 4))));
    /*
     * Now simulate ACK
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 536);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    return 0;
}

/*
 * Testcase 36:
 * Create a socket connection with a send window of 2048. Then send 512 bytes and verify that exactly one segment is sent because
 * we can flush the send buffer and the entire data fits into one buffer
 *
 * This is testcase 11 with congestion control enabled
 */
int testcase36() {
    return testcase11();
}

/*
 * Testcase 37:
 * Create a socket connection with a send window of 2048. Then send 1024 bytes. As this exceeds the MSS, this will
 * create a message with MSS bytes, and the remainder will not be sent due to Nagle's algorithm
 *
 * This is testcase 12 with congestion control enabled
 */
int testcase37() {
    return testcase12();
}

/*
 * Testcase 38:
 * Create a socket connection with a send window of 128, and a maximum window size of 200 bytes.
 * Then send 256 bytes. Even though the buffer cannot be flushed with this send
 * and we do not have enough data to fill a segment, a segment will be sent as we exceed one half of the maximum window size
 *
 * This is testcase 13 with congestion control enabled
 */
int testcase38() {
    return testcase13();
}

/*
 * Testcase 39:
 * Create a socket connection with a send window of 128, but a maximum window size of 8192.
 * Then send 256 bytes. As the buffer cannot be flushed with this send
 * and we do not have enough data to fill a segment, no data will be sent
 *
 * This is testcase 14 with congestion control enabled
 */
int testcase39() {
    return testcase14();
}

/*
 * Testcase 40:
 * Create a socket connection with a send window U = 600.
 * Then send 700 bytes. This will trigger the transmission of one segment of data with 536 bytes.
 * For the remaining 164 bytes, the decision algorith will be repeated with D = 164,  U = 64. However, this time
 * min(D,U) < MSS and SND_NXT != SND_UNA, so now data will be sent
 *
 * This is testcase 15 with congestion control enabled
 */
int testcase40() {
    return testcase15();
}

/*
 * Testcase 41:
 *
 * Establish a socket connection and send 2048 bytes. Due to congestion control, only one segment is send
 *
 *
 *  #   Socket under test                                         Peer
 * -------------------------------------------------------------------------------------------------------
 *
 *  1   SYN, SEQ = syn_seq_no, ACK_NO = 0 ----------------------->
 *  2                                                         <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *  3   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 0 ---------->
 *  4   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 536 -------->
 *
 */
int testcase41() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    net_msg_t* text;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    int i;
    unsigned char buffer[8192];
    /*
     * Fill buffer
     */
    for (i = 0; i < 2048; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window 8192
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 8192);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that the window size has been updated
     */
    ASSERT(8192 == socket->proto.tcp.snd_wnd);
    /*
     * Now try to transmit 2048 bytes
     */
    ip_tx_msg_called = 0;
    ASSERT(2048 == socket->ops->send(socket, (void*) buffer, 2048, 0));
    /*
     * and verify that a segment containing 536 data bytes has been sent
     */
    ASSERT( 1 == ip_tx_msg_called);
    ASSERT (20 + 536 == ip_payload_len);
    return 0;
}

/*
 * Testcase 42:
 *
 * Establish a socket connection and send 2048 bytes. Due to congestion control, only one segment is send.
 * Then an ACK is received for this segment. The congestion window is increased by one segment and consequently
 * two new segments are sent out
 *
 *
 *  #   Socket under test                                         Peer
 * -------------------------------------------------------------------------------------------------------
 *
 *  1   SYN, SEQ = syn_seq_no, ACK_NO = 0 ----------------------->
 *  2                                                         <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *  3   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 0 ---------->
 *  4   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 536 -------->
 *  5                                                         <-- ACK, SEQ = 2, ACK_NO = syn_seq_no + 537
 *  6   ACK, SEQ = syn_seq_no + 537, ACK_NO = 2, LEN = 536 -------->
 *  7   ACK, SEQ = syn_seq_no + 1073, ACK_NO = 2, LEN = 536 ------->
 *
 */
int testcase42() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    net_msg_t* text;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    int i;
    unsigned char buffer[8192];
    /*
     * Fill buffer
     */
    for (i = 0; i < 2048; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window 8192
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 8192);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that the window size has been updated
     */
    ASSERT(8192 == socket->proto.tcp.snd_wnd);
    /*
     * Now try to transmit 2048 bytes
     */
    ip_tx_msg_called = 0;
    ASSERT(2048 == socket->ops->send(socket, (void*) buffer, 2048, 0));
    /*
     * and verify that a segment containing 536 data bytes has been sent
     */
    ASSERT( 1 == ip_tx_msg_called);
    ASSERT (20 + 536 == ip_payload_len);
    /*
     * Now simulate an ACK for this segment
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1 + 536, 8192, buffer, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    /*
     * As the congestion window increases by SMSS, we should send two segments in reply
     */
    ASSERT(2 == ip_tx_msg_called);
    /*
     * Last sequence number should be 1 + 2*536 + syn_seq_no
     */
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 + 2*536 + syn_seq_no == ntohl(tcp_hdr->seq_no));
    return 0;
}

/*
 * Testcase 43:
 *
 * Establish a socket connection and send 2048 bytes. Due to congestion control, only one segment is send.
 * Then an ACK is received for this segment. The congestion window is increased by one segment and consequently
 * two new segments are sent out. The ACK received for these two segments increases the cwnd again by SMSS
 *
 *
 *  #   Socket under test                                         Peer
 * -------------------------------------------------------------------------------------------------------
 *
 *  1   SYN, SEQ = syn_seq_no, ACK_NO = 0 ----------------------->
 *  2                                                         <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *  3   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 0 ---------->
 *  4   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 536 -------->
 *  5                                                         <-- ACK, SEQ = 2, ACK_NO = syn_seq_no + 537
 *  6   ACK, SEQ = syn_seq_no + 537, ACK_NO = 2, LEN = 536 -------->
 *  7   ACK, SEQ = syn_seq_no + 1073, ACK_NO = 2, LEN = 536 ------->
 *  8                                                         <-- ACK, SEQ = 2, ACK_NO = syn_seq_no + 1609
 *  9   ACK, SEQ = syn_seq_no + 1609, ACK_NO = 2, LEN = 536 ------->
 *  10  ACK, SEQ = syn_seq_no + 2145, ACK_NO = 2, LEN = 536 ------->
 *  11  ACK, SEQ = syn_seq_no + 2681, ACK_NO = 2, LEN = 536 ------->
 */
int testcase43() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    net_msg_t* text;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    int i;
    unsigned char buffer[8192];
    /*
     * Fill buffer
     */
    for (i = 0; i < 2048; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window 8192
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 8192);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that the window size has been updated
     */
    ASSERT(8192 == socket->proto.tcp.snd_wnd);
    /*
     * Now try to transmit 2048 bytes
     */
    ip_tx_msg_called = 0;
    ASSERT(2048 == socket->ops->send(socket, (void*) buffer, 2048, 0));
    /*
     * and verify that a segment containing 536 data bytes has been sent
     */
    ASSERT( 1 == ip_tx_msg_called);
    ASSERT (20 + 536 == ip_payload_len);
    /*
     * Now simulate an ACK for this segment
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1 + 536, 8192, buffer, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    /*
     * As the congestion window increases by SMSS, we should send two segments in reply
     */
    ASSERT(2 == ip_tx_msg_called);
    /*
     * Last sequence number should be 1 + 2*536 + syn_seq_no
     */
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 + 2*536 + syn_seq_no == ntohl(tcp_hdr->seq_no));
    /*
     * We now have transmitted 3*536 bytes. Put more data into send queue
     */
    ip_tx_msg_called = 0;
    ASSERT(2048 == socket->ops->send(socket, (void*) buffer, 2048, 0));
    ASSERT(0 == ip_tx_msg_called);
    /*
     * and receive second ACK. This should increase cwnd from 2*SMSS to 3*SMSS (i.e. only be SMSS, even though
     * we have acknowledged 2*SMSS bytes)
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1 + 3*536, 8192, buffer, 0);
    tcp_rx_msg(text);
    ASSERT(3 == ip_tx_msg_called);
    ASSERT(5*536 + 1 + syn_seq_no == ntohl(tcp_hdr->seq_no));
    return 0;
}

/*
 * Testcase 44:
 *
 * Simulate the case that the slow start threshold is reached and slow start turns into congestion avoidance
 *
 *
 *
 *
 *  #   Socket under test                                         Peer
 * -------------------------------------------------------------------------------------------------------
 *
 *  1   SYN, SEQ = syn_seq_no, ACK_NO = 0 ----------------------->
 *  2                                                         <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *  3   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 0 ---------->
 *  4   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 536 -------->
 *  5                                                         <-- ACK, SEQ = 2, ACK_NO = syn_seq_no + 537
 *  6   ACK, SEQ = syn_seq_no + 537, ACK_NO = 2, LEN = 536 -------->
 *  7   ACK, SEQ = syn_seq_no + 1073, ACK_NO = 2, LEN = 536 ------->
 *  8                                                         <-- ACK, SEQ = 2, ACK_NO = syn_seq_no + 1073
 *  9   ACK, SEQ = syn_seq_no + 1609, ACK_NO = 2, LEN = 536 ------->
 *  11  ACK, SEQ = syn_seq_no + 2145, ACK_NO = 2, LEN = 536 ------->
 *  12                                                        <-- ACK, SEQ = 2, ACK_NO = syn_seq_no + 1609
 *  13  ACK, SEQ = syn_seq_no + 2681, ACK_NO = 2, LEN = 536 ------->
 */
int testcase44() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    net_msg_t* text;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    int i;
    unsigned char buffer[8192];
    /*
     * Fill buffer
     */
    for (i = 0; i < 2048; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window 8192
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 8192);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that the window size has been updated
     */
    ASSERT(8192 == socket->proto.tcp.snd_wnd);
    /*
     * Set sshtresh to 3*SMSS
     */
    socket->proto.tcp.ssthresh = 536*3;
    /*
     * Now try to transmit 2048 bytes
     */
    ip_tx_msg_called = 0;
    ASSERT(2048 == socket->ops->send(socket, (void*) buffer, 2048, 0));
    /*
     * and verify that a segment containing 536 data bytes has been sent
     */
    ASSERT( 1 == ip_tx_msg_called);
    ASSERT (20 + 536 == ip_payload_len);
    /*
     * Now simulate an ACK for this segment
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1 + 536, 8192, buffer, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    /*
     * As the congestion window increases by SMSS, we should send two segments in reply
     */
    ASSERT(2 == ip_tx_msg_called);
    /*
     * Last sequence number should be 1 + 2*536 + syn_seq_no
     */
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 + 2*536 + syn_seq_no == ntohl(tcp_hdr->seq_no));
    /*
     * We now have transmitted 3*536 bytes. Put more data into send queue
     */
    ip_tx_msg_called = 0;
    ASSERT(4096 == socket->ops->send(socket, (void*) buffer, 4096, 0));
    ASSERT(0 == ip_tx_msg_called);
    /*
     * and receive second ACK. This should inflate the congestion window from 2*SMSS to 3*SMSS. As there is still one
     * segment in flight, we should send out two additional segments
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1 + 2*536, 8192, buffer, 0);
    tcp_rx_msg(text);
    ASSERT(2 == ip_tx_msg_called);
    /*
     * Now acknowledge the next segment. Having reached the slow start threshold, the congestion window will not be opened
     * any further. However, as one segment has left the network, we send one more segment
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1 + 3*536, 8192, buffer, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    ASSERT(1 == ip_tx_msg_called);
    return 0;
}

/*
 * Testcase 45:
 *
 * Simulate the case that the slow start threshold is reached and slow start turns into congestion avoidance
 *
 *
 *
 *  #   Socket under test                                         Peer
 * -------------------------------------------------------------------------------------------------------
 *
 *  1   SYN, SEQ = syn_seq_no, ACK_NO = 0 ----------------------->
 *  2                                                         <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *  3   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 0 ---------->
 *  4   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 536 -------->
 *  5                                                         <-- ACK, SEQ = 2, ACK_NO = syn_seq_no + 537
 *  6   ACK, SEQ = syn_seq_no + 537, ACK_NO = 2, LEN = 536 -------->
 *  7   ACK, SEQ = syn_seq_no + 1073, ACK_NO = 2, LEN = 536 ------->
 *  8                                                         <-- ACK, SEQ = 2, ACK_NO = syn_seq_no + 1073
 *  9   ACK, SEQ = syn_seq_no + 1609, ACK_NO = 2, LEN = 536 ------->
 *  11  ACK, SEQ = syn_seq_no + 2145, ACK_NO = 2, LEN = 536 ------->
 *  12                                                        <-- ACK, SEQ = 2, ACK_NO = syn_seq_no + 1609
 *  13  ACK, SEQ = syn_seq_no + 2681, ACK_NO = 2, LEN = 536 ------->
 *  14                                                        <-- ACK, SEQ = 2, ACK_NO = syn_seq_no + 2781
 *  15  ACK, SEQ = syn_seq_no + 3217, ACK_NO = 2, LEN = 536 ------->
 *  16  ACK, SEQ = syn_seq_no + 3753, ACK_NO = 2, LEN = 536 ------->
 *  17  ACK, SEQ = syn_seq_no + 4289, ACK_NO = 2, LEN = 536 -------> *
 *
 */
int testcase45() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    net_msg_t* text;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    int i;
    unsigned char buffer[8192];
    /*
     * Fill buffer
     */
    for (i = 0; i < 2048; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window 8192
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 8192);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that the window size has been updated
     */
    ASSERT(8192 == socket->proto.tcp.snd_wnd);
    /*
     * Set sshtresh to 3*SMSS
     */
    socket->proto.tcp.ssthresh = 536*3;
    /*
     * Now try to transmit 2048 bytes
     */
    ip_tx_msg_called = 0;
    ASSERT(2048 == socket->ops->send(socket, (void*) buffer, 2048, 0));
    /*
     * and verify that a segment containing 536 data bytes has been sent
     */
    ASSERT( 1 == ip_tx_msg_called);
    ASSERT (20 + 536 == ip_payload_len);
    /*
     * Now simulate an ACK for this segment
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1 + 536, 8192, buffer, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    /*
     * As the congestion window increases by SMSS, we should send two segments in reply
     */
    ASSERT(2 == ip_tx_msg_called);
    /*
     * Last sequence number should be 1 + 2*536 + syn_seq_no
     */
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 + 2*536 + syn_seq_no == ntohl(tcp_hdr->seq_no));
    /*
     * We now have transmitted 3*536 bytes. Put more data into send queue
     */
    ip_tx_msg_called = 0;
    ASSERT(4096 == socket->ops->send(socket, (void*) buffer, 4096, 0));
    ASSERT(0 == ip_tx_msg_called);
    /*
     * and receive second ACK. This should inflate the congestion window from 2*SMSS to 3*SMSS. As there is still one
     * segment in flight, we should send out two additional segments
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1 + 2*536, 8192, buffer, 0);
    tcp_rx_msg(text);
    ASSERT(2 == ip_tx_msg_called);
    /*
     * Now acknowledge the next segment. Having reached the slow start threshold, the congestion window will not be opened
     * any further. However, as one segment has left the network, we send one more segment
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1 + 3*536, 8192, buffer, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    ASSERT(1 == ip_tx_msg_called);
    /*
     * We now have SMSS bytes acknowledged out of a window of 3*SMSS. Send an ACK for two more segments - this should now
     * increase the congestion window by SMSS. Thus we should send three segments in return
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1 + 5*536, 8192, buffer, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    ASSERT(3 == ip_tx_msg_called);
    return 0;
}

/*
 * Testcase 46:
 *
 * Enter congestion avoidance, then simulate a timer based retransmission
 *
 *  #   Socket under test                                         Peer
 * -------------------------------------------------------------------------------------------------------
 *
 *  1   SYN, SEQ = syn_seq_no, ACK_NO = 0 ----------------------->
 *  2                                                         <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *  3   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 0 ---------->
 *  4   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 536 -------->
 *  5                                                         <-- ACK, SEQ = 2, ACK_NO = syn_seq_no + 537
 *  6   ACK, SEQ = syn_seq_no + 537, ACK_NO = 2, LEN = 536 -------->
 *  7   ACK, SEQ = syn_seq_no + 1073, ACK_NO = 2, LEN = 536 ------->
 *  8                                                         <-- ACK, SEQ = 2, ACK_NO = syn_seq_no + 1073
 *  9   ACK, SEQ = syn_seq_no + 1609, ACK_NO = 2, LEN = 536 ------->
 *  11  ACK, SEQ = syn_seq_no + 2145, ACK_NO = 2, LEN = 536 ------->                                <---------
 *  12                                                        <-- ACK, SEQ = 2, ACK_NO = syn_seq_no + 1609   | acks
 *  13  ACK, SEQ = syn_seq_no + 2681, ACK_NO = 2, LEN = 536 ------->                                         | this segment
 *  14                                                        <-- ACK, SEQ = 2, ACK_NO = syn_seq_no + 2681  --
 *  15  ACK, SEQ = syn_seq_no + 3217, ACK_NO = 2, LEN = 536 ------->
 *  16  ACK, SEQ = syn_seq_no + 3753, ACK_NO = 2, LEN = 536 ------->
 *  17  ACK, SEQ = syn_seq_no + 4289, ACK_NO = 2, LEN = 536 ------->
 *  18  ACK, SEQ = syn_seq_no + 2681, ACK_NO = 2, LEN = 536 ------->
 *
 */
int testcase46() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    net_msg_t* text;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    int i;
    unsigned char buffer[8192];
    /*
     * Fill buffer
     */
    for (i = 0; i < 2048; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window 8192
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 8192);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that the window size has been updated
     */
    ASSERT(8192 == socket->proto.tcp.snd_wnd);
    /*
     * Set sshtresh to 3*SMSS
     */
    socket->proto.tcp.ssthresh = 536*3;
    /*
     * Now try to transmit 2048 bytes
     */
    ip_tx_msg_called = 0;
    ASSERT(2048 == socket->ops->send(socket, (void*) buffer, 2048, 0));
    /*
     * and verify that a segment containing 536 data bytes has been sent
     */
    ASSERT( 1 == ip_tx_msg_called);
    ASSERT (20 + 536 == ip_payload_len);
    /*
     * Now simulate an ACK for this segment
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1 + 536, 8192, buffer, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    /*
     * As the congestion window increases by SMSS, we should send two segments in reply
     */
    ASSERT(2 == ip_tx_msg_called);
    /*
     * Last sequence number should be 1 + 2*536 + syn_seq_no
     */
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 + 2*536 + syn_seq_no == ntohl(tcp_hdr->seq_no));
    /*
     * We now have transmitted 3*536 bytes. Put more data into send queue
     */
    ip_tx_msg_called = 0;
    ASSERT(4096 == socket->ops->send(socket, (void*) buffer, 4096, 0));
    ASSERT(0 == ip_tx_msg_called);
    /*
     * and receive second ACK. This should inflate the congestion window from 2*SMSS to 3*SMSS. As there is still one
     * segment in flight, we should send out two additional segments
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1 + 2*536, 8192, buffer, 0);
    tcp_rx_msg(text);
    ASSERT(2 == ip_tx_msg_called);
    /*
     * Now acknowledge the next segment. Having reached the slow start threshold, the congestion window will not be opened
     * any further. However, as one segment has left the network, we send one more segment
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1 + 3*536, 8192, buffer, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    ASSERT(1 == ip_tx_msg_called);
    /*
     * We now have SMSS bytes acknowledged out of a window of 3*SMSS. Send an ACK for two more segments - this should now
     * increase the congestion window by SMSS to 4*SMSS. Thus we should send three segments in return
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1 + 5*536, 8192, buffer, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    ASSERT(3 == ip_tx_msg_called);
    /*
     * We now have four segments outstanding, the first one being the one with sequence number syn_seq_no + 2681.
     * Simulate that retransmission timer fires. This should put us back into slow start, i.e. we should only
     * retransmit one packet
     */
    ip_tx_msg_called = 0;
    for (i = 0; i < socket->proto.tcp.rto; i++)
        tcp_do_tick();
    ASSERT(1 == ip_tx_msg_called);
    ASSERT(syn_seq_no + 2681 == ntohl(tcp_hdr->seq_no));
    return 0;
}

/*
 * Testcase 47:
 *
 * Enter congestion avoidance, then simulate a retransmission. Receive more ACKs and verify that
 * ssthresh has been reduced
 *
 *
 *
 *  #   Socket under test                                         Peer
 * -------------------------------------------------------------------------------------------------------
 *
 *  1   SYN, SEQ = syn_seq_no, ACK_NO = 0 ----------------------->
 *  2                                                         <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *  3   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 0 ---------->
 *  4   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 536 -------->
 *  5                                                         <-- ACK, SEQ = 2, ACK_NO = syn_seq_no + 537
 *  6   ACK, SEQ = syn_seq_no + 537, ACK_NO = 2, LEN = 536 -------->
 *  7   ACK, SEQ = syn_seq_no + 1073, ACK_NO = 2, LEN = 536 ------->
 *  8                                                         <-- ACK, SEQ = 2, ACK_NO = syn_seq_no + 1073
 *  9   ACK, SEQ = syn_seq_no + 1609, ACK_NO = 2, LEN = 536 ------->
 *  11  ACK, SEQ = syn_seq_no + 2145, ACK_NO = 2, LEN = 536 ------->                                <---------
 *  12                                                        <-- ACK, SEQ = 2, ACK_NO = syn_seq_no + 1609   | acks
 *  13  ACK, SEQ = syn_seq_no + 2681, ACK_NO = 2, LEN = 536 ------->                                         | this segment
 *  14                                                        <-- ACK, SEQ = 2, ACK_NO = syn_seq_no + 2681  --
 *  15  ACK, SEQ = syn_seq_no + 3217, ACK_NO = 2, LEN = 536 ------->
 *  16  ACK, SEQ = syn_seq_no + 3753, ACK_NO = 2, LEN = 536 ------->
 *  17  ACK, SEQ = syn_seq_no + 4289, ACK_NO = 2, LEN = 536 ------->
 *  18  ACK, SEQ = syn_seq_no + 2681, ACK_NO = 2, LEN = 536 ------->
 *  19                                                        <-- ACK, SEQ = 2, ACK_NO = syn_seq_no + 3217
 *  20  ACK, SEQ = syn_seq_no + 3217, ACK_NO = 2, LEN = 536 ------->
 *  21  ACK, SEQ = syn_seq_no + 3753, ACK_NO = 2, LEN = 536 ------->
 *  22                                                        <-- ACK, SEQ = 2, ACK_NO = syn_seq_no + 3753
 *  23  ACK, SEQ = syn_seq_no + 4289, ACK_NO = 2, LEN = 536 ------->
 */

int testcase47() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    net_msg_t* text;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    int i;
    unsigned char buffer[8192];
    /*
     * Fill buffer
     */
    for (i = 0; i < 2048; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window 8192
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 8192);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that the window size has been updated
     */
    ASSERT(8192 == socket->proto.tcp.snd_wnd);
    /*
     * Set sshtresh to 3*SMSS
     */
    socket->proto.tcp.ssthresh = 536*3;
    /*
     * Now try to transmit 2048 bytes
     */
    ip_tx_msg_called = 0;
    ASSERT(2048 == socket->ops->send(socket, (void*) buffer, 2048, 0));
    /*
     * and verify that a segment containing 536 data bytes has been sent
     */
    ASSERT( 1 == ip_tx_msg_called);
    ASSERT (20 + 536 == ip_payload_len);
    /*
     * Now simulate an ACK for this segment
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1 + 536, 8192, buffer, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    /*
     * As the congestion window increases by SMSS, we should send two segments in reply
     */
    ASSERT(2 == ip_tx_msg_called);
    /*
     * Last sequence number should be 1 + 2*536 + syn_seq_no
     */
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 + 2*536 + syn_seq_no == ntohl(tcp_hdr->seq_no));
    /*
     * We now have transmitted 3*536 bytes. Put more data into send queue
     */
    ip_tx_msg_called = 0;
    ASSERT(4096 == socket->ops->send(socket, (void*) buffer, 4096, 0));
    ASSERT(0 == ip_tx_msg_called);
    /*
     * and receive second ACK. This should inflate the congestion window from 2*SMSS to 3*SMSS. As there is still one
     * segment in flight, we should send out two additional segments
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1 + 2*536, 8192, buffer, 0);
    tcp_rx_msg(text);
    ASSERT(2 == ip_tx_msg_called);
    /*
     * Now acknowledge the next segment. Having reached the slow start threshold, the congestion window will not be opened
     * any further. However, as one segment has left the network, we send one more segment
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1 + 3*536, 8192, buffer, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    ASSERT(1 == ip_tx_msg_called);
    /*
     * We now have SMSS bytes acknowledged out of a window of 3*SMSS. Send an ACK for two more segments - this should now
     * increase the congestion window by SMSS to 4*SMSS. Thus we should send three segments in return
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1 + 5*536, 8192, buffer, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    ASSERT(3 == ip_tx_msg_called);
    /*
     * We now have four segments outstanding, the first one being the one with sequence number syn_seq_no + 2681.
     * Simulate that retransmission timer fires. This should put us back into slow start, i.e. we should only
     * retransmit one packet
     */
    ip_tx_msg_called = 0;
    for (i = 0; i < socket->proto.tcp.rto; i++)
        tcp_do_tick();
    ASSERT(1 == ip_tx_msg_called);
    ASSERT(syn_seq_no + 2681 == ntohl(tcp_hdr->seq_no));
    /*
     * As the flight size was 4*SMSS at the time when the retransmission occured, this should have set our
     * slow start threshold back to 2*SMSS
     */
    ASSERT(2*536 == socket->proto.tcp.ssthresh);
    /*
     * Now ACK this segment. This should increase the congestion window by SMSS as we are back in slow start, so
     * it is now 2*SMSS. We should therefore retransmit two more segments
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 2681 + 536, 8192, buffer, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    ASSERT(2 == ip_tx_msg_called);
    ASSERT(syn_seq_no + 3753 == ntohl(tcp_hdr->seq_no));
    /*
     * Being in slow start again, the next ACK should not increase SMSS as it does not ACK the entire
     * window. Thus we only send one more segment in response which completes recovery
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no +  2681 + 2*536, 8192, buffer, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    ASSERT(1 == ip_tx_msg_called);
    ASSERT(syn_seq_no + 3753 + 536 == ntohl(tcp_hdr->seq_no));
    return 0;
}

/*
 * Testcase 48
 *
 * Simulate fast retransmission and fast recovery
 *
 *  #   Socket under test                                         Peer
 * -------------------------------------------------------------------------------------------------------
 *
 *  1   SYN, SEQ = syn_seq_no, ACK_NO = 0 ----------------------->
 *  2                                                         <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *  3   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 0 ---------->
 *  4   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 536 ---------->
 *  5   ACK, SEQ = syn_seq_no + 537, ACK_NO = 2, LEN = 536 -------->
 *  6   ACK, SEQ = syn_seq_no + 1073, ACK_NO = 2, LEN = 536 ------->
 *  7   ACK, SEQ = syn_seq_no + 1609, ACK_NO = 2, LEN = 536 ------->
 *  8   ACK, SEQ = syn_seq_no + 2145, ACK_NO = 2, LEN = 536 ------->
 *  9   ACK, SEQ = syn_seq_no + 2681, ACK_NO = 2, LEN = 536 ------->
 *  10                                                        <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *  11                                                        <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *  12                                                        <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *  13   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 536 --------->
 *  14                                                        <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *  15   ACK, SEQ = syn_seq_no + 3217, ACK_NO = 2, LEN = 536 ------>
 *  16                                                        <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *  17   ACK, SEQ = syn_seq_no + 3753, ACK_NO = 2, LEN = 536 ------>
 *  18                                                        <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 3217
 *  19   ACK, SEQ = syn_seq_no + 4289, ACK_NO = 2, LEN = 536 ------>
 */
int testcase48() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    net_msg_t* text;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    int i;
    unsigned char buffer[8192];
    /*
     * Fill buffer
     */
    for (i = 0; i < 2048; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window 8192
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 8192);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that the window size has been updated
     */
    ASSERT(8192 == socket->proto.tcp.snd_wnd);
    /*
     * Set congestion window to 6*SMSS
     */
    socket->proto.tcp.cwnd = 6*536;
    /*
     * Now write 8192 bytes to the send queue - this should transmit six segments
     */
    ip_tx_msg_called = 0;
    ASSERT(8192 == socket->ops->send(socket, buffer, 8192, 0));
    ASSERT(6 == ip_tx_msg_called);
    /*
     * We now simulate the case that segment #1 is lost, but segments #2, #3 and #4 arrive ok. When
     * segment #2 is received by the peer, it will send a duplicate ACK and acknowledge our SYN again
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1, 8192, buffer, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    /*
     * As this is the first duplicate ACK, it should not trigger any retransmission
     */
    ASSERT(0 == ip_tx_msg_called);
    /*
     * Now simulate two more duplicate ACKS - one initiated by segment #3, one initiated by segment #4
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1, 8192, buffer, 0);
    tcp_rx_msg(text);
    ASSERT(0 == ip_tx_msg_called);
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1, 8192, buffer, 0);
    tcp_rx_msg(text);
    /*
     * The third duplicate ACK should have forced a retransmission
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * of segment #1
     */
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(syn_seq_no + 1 == ntohl(tcp_hdr->seq_no));
    /*
     * Slow start threshold should have been set to
     * MAX(2*SMSS, (snd_nxt - snd_una) / 2) = MAX(2*SMSS, 6*SMSS / 2) = 3*SMSS
     */
    ASSERT(3*536 == socket->proto.tcp.ssthresh);
    /*
     * and congestion window should be 6*SMSS
     */
    ASSERT(6*536 == socket->proto.tcp.cwnd);
    /*
     * Retransmission timer should be set, but segment should not be timed
     */
    ASSERT(socket->proto.tcp.rtx_timer.time);
    ASSERT(-1 == socket->proto.tcp.current_rtt);
    /*
     * Now simulate the duplicate ACK for segment #5. This should inflate the congestion window by one additional segment
     * and therefore inject one more segment into the network (this should be segment #7)
     */
    ip_tx_msg_called = 0;
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1, 8192, buffer, 0);
    tcp_rx_msg(text);
    ASSERT(7*536 == socket->proto.tcp.cwnd);
    ASSERT(1 == ip_tx_msg_called);
    ASSERT(syn_seq_no + 1 + 6*536 == ntohl(tcp_hdr->seq_no));
    /*
     * Finally simulate that segment #6 has been added to the out-of-order queue of our peer, giving us room for one more segment
     * (segment #8)
     */
    ip_tx_msg_called = 0;
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1, 8192, buffer, 0);
    tcp_rx_msg(text);
    ASSERT(8*536 == socket->proto.tcp.cwnd);
    ASSERT(1 == ip_tx_msg_called);
    ASSERT(syn_seq_no + 1 + 7*536 == ntohl(tcp_hdr->seq_no));
    /*
     * Now assume that our retransmission was successful and the peer acknowledges all data up to segment #6
     */
    ip_tx_msg_called = 0;
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1 + 6*536, 8192, buffer, 0);
    tcp_rx_msg(text);
    /*
     * The congestion window should now be 3*SMSS
     */
    ASSERT(3*536 == socket->proto.tcp.cwnd);
    /*
     * As we have already sent segments #7 and #8, we have room for one more segment
     */
    ASSERT(1 == ip_tx_msg_called);
    ASSERT(syn_seq_no + 4289 == ntohl(tcp_hdr->seq_no));
    return 0;
}

/*
 * Testcase 49
 *
 * Simulate fast retransmission and a timeout during fast recovery
 *
 *  #   Socket under test                                         Peer
 * -------------------------------------------------------------------------------------------------------
 *
 *  1   SYN, SEQ = syn_seq_no, ACK_NO = 0 ----------------------->
 *  2                                                         <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *  3   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 0 ---------->
 *  4   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 536 ---------->
 *  5   ACK, SEQ = syn_seq_no + 537, ACK_NO = 2, LEN = 536 -------->
 *  6   ACK, SEQ = syn_seq_no + 1073, ACK_NO = 2, LEN = 536 ------->
 *  7   ACK, SEQ = syn_seq_no + 1609, ACK_NO = 2, LEN = 536 ------->
 *  8   ACK, SEQ = syn_seq_no + 2145, ACK_NO = 2, LEN = 536 ------->
 *  9   ACK, SEQ = syn_seq_no + 2681, ACK_NO = 2, LEN = 536 ------->
 *  10                                                        <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *  11                                                        <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *  12                                                        <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *  13   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 536 --------->
 *  14                                                        <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *  15   ACK, SEQ = syn_seq_no + 3217, ACK_NO = 2, LEN = 536 ------>
 *  16                                                        <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *  17   ACK, SEQ = syn_seq_no + 3753, ACK_NO = 2, LEN = 536 ------>
 *  18   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 536 --------->
 */
int testcase49() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    net_msg_t* text;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    int i;
    int timer;
    unsigned char buffer[8192];
    /*
     * Fill buffer
     */
    for (i = 0; i < 2048; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window 8192
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 8192);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that the window size has been updated
     */
    ASSERT(8192 == socket->proto.tcp.snd_wnd);
    /*
     * Set congestion window to 6*SMSS
     */
    socket->proto.tcp.cwnd = 6*536;
    /*
     * Now write 8192 bytes to the send queue - this should transmit six segments
     */
    ip_tx_msg_called = 0;
    ASSERT(8192 == socket->ops->send(socket, buffer, 8192, 0));
    ASSERT(6 == ip_tx_msg_called);
    /*
     * We now simulate the case that segment #1 is lost, but segments #2, #3 and #4 arrive ok. When
     * segment #2 is received by the peer, it will send a duplicate ACK and acknowledge our SYN again
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1, 8192, buffer, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    /*
     * As this is the first duplicate ACK, it should not trigger any retransmission
     */
    ASSERT(0 == ip_tx_msg_called);
    /*
     * Now simulate two more duplicate ACKS - one initiated by segment #3, one initiated by segment #4
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1, 8192, buffer, 0);
    tcp_rx_msg(text);
    ASSERT(0 == ip_tx_msg_called);
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1, 8192, buffer, 0);
    tcp_rx_msg(text);
    /*
     * The third duplicate ACK should have forced a retransmission
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * of segment #1
     */
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(syn_seq_no + 1 == ntohl(tcp_hdr->seq_no));
    /*
     * Slow start threshold should have been set to
     * MAX(2*SMSS, (snd_nxt - snd_una) / 2) = MAX(2*SMSS, 6*SMSS / 2) = 3*SMSS
     */
    ASSERT(3*536 == socket->proto.tcp.ssthresh);
    /*
     * and congestion window should be 6*SMSS
     */
    ASSERT(6*536 == socket->proto.tcp.cwnd);
    /*
     * Retransmission timer should be set, but segment should not be timed
     */
    ASSERT(socket->proto.tcp.rtx_timer.time);
    ASSERT(-1 == socket->proto.tcp.current_rtt);
    /*
     * Now simulate the duplicate ACK for segment #5. This should inflate the congestion window by one additional segment
     * and therefore inject one more segment into the network (this should be segment #7)
     */
    ip_tx_msg_called = 0;
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1, 8192, buffer, 0);
    tcp_rx_msg(text);
    ASSERT(7*536 == socket->proto.tcp.cwnd);
    ASSERT(1 == ip_tx_msg_called);
    ASSERT(syn_seq_no + 1 + 6*536 == ntohl(tcp_hdr->seq_no));
    /*
     * Finally simulate that segment #6 has been added to the out-of-order queue of our peer, giving us room for one more segment
     * (segment #8)
     */
    ip_tx_msg_called = 0;
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1, 8192, buffer, 0);
    tcp_rx_msg(text);
    ASSERT(8*536 == socket->proto.tcp.cwnd);
    ASSERT(1 == ip_tx_msg_called);
    ASSERT(syn_seq_no + 1 + 7*536 == ntohl(tcp_hdr->seq_no));
    /*
     * Now assume that our retransmission was not successful and the retransmission timer fires
     */
    ASSERT(socket->proto.tcp.rtx_timer.time);
    timer = socket->proto.tcp.rtx_timer.time;
    ip_tx_msg_called = 0;
    for (i = 0; i < timer - 1; i++) {
        tcp_do_tick();
        ASSERT(0 == ip_tx_msg_called);
    }
    tcp_do_tick();
    /*
     * We should now see a retransmission of the first outstanding segment
     */
    ASSERT(1 == ip_tx_msg_called);
    ASSERT(syn_seq_no + 1 == ntohl(tcp_hdr->seq_no));
    /*
     * and should have left fast recovery mode
     */
    ASSERT(socket->proto.tcp.cwnd == 536);
    ASSERT(0 == socket->proto.tcp.dupacks);
    return 0;
}

/*
 * Testcase 50:
 * Create a socket connection with a send window of 536, but a maximum window size of 8192.
 * Then send 1024 bytes. This will send a segment of 536 bytes, filling up the window of the simulated
 * peer completely.
 *
 * When the peer sends an ACK closing the window, the persist timer is set and will force delivery of a window
 * probe after it expires.
 *
 * Make sure that an ACK received in response to this window probe does not trigger fast retransmit
 */
int testcase50() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    net_msg_t* text;
    int i;
    unsigned char buffer[8192];
    /*
     * Fill buffer
     */
    for (i = 0; i < 1024; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window  536
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 536);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that the window size has been updated
     */
    ASSERT(536 == socket->proto.tcp.snd_wnd);
    /*
     * Set congestion window size to a large value in order to avoid that "slow start" makes our
     * test scenario pointless
     */
    socket->proto.tcp.cwnd = 65536;
    /*
     * Fake maximum window size
     */
    socket->proto.tcp.max_wnd = 8192;
    /*
     * Now try to transmit 1024 bytes
     */
    ip_tx_msg_called = 0;
    cond_broadcast_called = 0;
    ASSERT(1024 == socket->ops->send(socket, (void*) buffer, 1024, 0));
    /*
     * and verify that one segment has been sent, containing 536 bytes of data
     */
    ASSERT( 1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(536 == ip_payload_len - sizeof(u32)*tcp_hdr->hlength);
    /*
     * Persist timer should not be set, as the ACK we expect will do the job
     * for us
     */
    ASSERT(0 == socket->proto.tcp.persist_timer.time);
    /*
     * Now simulate an ACK closing the window
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1 + 536, 0, buffer, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    /*
     * As we have more data to be sent, this should have set the persist timer - simulate that it
     * fires
     */
    ASSERT(socket->proto.tcp.rto == socket->proto.tcp.persist_timer.time);
    for (i = 0; i < socket->proto.tcp.rto; i++) {
        tcp_do_tick();
        if (i < socket->proto.tcp.rto - 1)
            ASSERT(0 == ip_tx_msg_called);
    }
    /*
     * Last tick should have forced out a packet
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * As we have data to send, this packet - a zero window probe - should contain at least one byte of
     * data
     */
    ASSERT(ip_payload_len > ((tcp_hdr_t*) payload)->hlength*sizeof(u32));
    /*
     * This should have set the retransmission timer
     */
    ASSERT(socket->proto.tcp.rtx_timer.time);
    /*
     * and cleared the persist timer
     */
    ASSERT(0 == socket->proto.tcp.persist_timer.time);
    /*
     * Now send ACK again and make sure that this does not trigger a retransmit
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1 + 536, 0, buffer, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    ASSERT(0 == ip_tx_msg_called);
    return 0;
}

/*
 * Testcase 51:
 * Create a socket and establish a connection. Then simulate receipt of an ACK for data which we have not sent yet
 * and verify that an ACK is sent in return
 *
 *  #   Socket under test                                         Peer
 * -------------------------------------------------------------------------------------------------------
 *
 *  1   SYN, SEQ = syn_seq_no, ACK_NO = 0 ----------------------->
 *  2                                                         <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *  3   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 0 ---------->
 *  4                                                         <-- ACK, SEQ = 2, ACK_NO = syn_seq_no + 50, LEN = 128
 */
int testcase51() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    net_msg_t* text;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    int i;
    unsigned char buffer[8192];
    /*
     * Fill buffer
     */
    for (i = 0; i < 128; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window 600
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 600);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that the window size has been updated
     */
    ASSERT(600 == socket->proto.tcp.snd_wnd);
    /*
     * Put together ACK
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 50, 600, buffer, 128);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    /*
     * Verify that an ACK is sent immediately
     */
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->syn);
    ASSERT(0 == tcp_hdr->fin);
    ASSERT(sizeof(u32)*tcp_hdr->hlength == sizeof(tcp_hdr_t));
    /*
     * and that no data has been added to the receive queue
     */
    ASSERT(0 == socket->proto.tcp.rcv_buffer_head);
    ASSERT(0 == socket->proto.tcp.rcv_buffer_tail);
    return 0;
}

/*
 * Test macros for sequence number comparison
 * Case 1: b > a
 */
int testcase52() {
    u32 b;
    u32 a;
    b = 0xFFFFFFFF;
    a = 0xFFFFFFF0;
    ASSERT(TCP_LT(a,b));
    ASSERT(TCP_GT(b,a));
    return 0;
}

/*
 * Test macros for sequence number comparison
 * Case 2: b > a, but b has already wrapped around
 */
int testcase53() {
    u32 b;
    u32 a;
    b = 1;
    a = 0xFFFFFFF0;
    ASSERT(TCP_LT(a,b));
    ASSERT(TCP_GT(b,a));
    return 0;
}

/*
 * Test macros for sequence number comparison
 * Case 3: b < a
 */
int testcase54() {
    u32 b;
    u32 a;
    a = 0xFFFFFFF0;
    b = a - (1 << 31) + 1;
    ASSERT(!(TCP_LT(a,b)));
    ASSERT(TCP_LT(b,a));
    ASSERT(TCP_GT(a,b));
    return 0;
}

/*
 * Testcase 55:
 * Timeout during active connect
 *
 * For a SYN, we have a timeout of 15 seconds.
 * As we try five retransmissions in total, we spent about
 * 15 + 30 + 60 + 120 + 240 + 480 = 945
 * seconds in total with the connection attempt and time out.
 */
int testcase55() {
    struct sockaddr_in in;
    int i;
    tcp_hdr_t* tcp_hdr;
    int attempt;
    int j;
    int ticks = 0;
    /*
     * Do basic initialization of socket
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    cond_broadcast_called = 0;
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect
     */
    ip_tx_msg_called = 0;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    ASSERT(0 == cond_broadcast_called);
    /*
     * This should have produced a SYN
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * As the RTO is 15 seconds for a SYN, simulate 15*(HZ / TCP_HZ) ticks
     */
    ASSERT(socket->proto.tcp.rtx_timer.time == 15*TCP_HZ);
    ip_tx_msg_called = 0;
    for (i = 0; i < 15*TCP_HZ - 1; i++) {
        ticks++;
        tcp_do_tick();
        ASSERT(0 == ip_tx_msg_called);
    }
    tcp_do_tick();
    ticks++;
    attempt = 1;
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(0 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(1 == tcp_hdr->syn);
    /*
     * Now repeat this - timer should double with each attempt
     */
    for (j = 0; j < 5; j++) {
        ip_tx_msg_called = 0;
        for (i = 0; i < (15*TCP_HZ << attempt) - 1; i++) {
            tcp_do_tick();
            ticks++;
            ASSERT(0 == ip_tx_msg_called);
        }
        ticks++;
        tcp_do_tick();
        attempt++;
        ASSERT(1 == ip_tx_msg_called);
        ASSERT(0 == tcp_hdr->ack);
        if (4 == j) {
            /*
             * Last attempt should result in a reset
             */
            ASSERT(1 == tcp_hdr->rst);
            ASSERT(0 == tcp_hdr->syn);
            ASSERT(-137 == socket->error);
            ASSERT(cond_broadcast_called);
        }
        else {
            /*
             * Retransmit SYN
             */
            ASSERT(0 == tcp_hdr->rst);
            ASSERT(1 == tcp_hdr->syn);
        }
    }
    return 0;
}

/*
 * Testcase 56:
 * Create a new socket and try to connect it. Verify that the MSS is transmitted as option along with the SYN
 */
int testcase56() {
    struct sockaddr_in in;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u16* mss;
    /*
     * Do basic initialization of socket
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Set MTU to 1500
     */
    mtu = 1500;
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Calculate checksum
     */
    chksum = validate_tcp_checksum(24, (u16*) payload, ip_src, ip_dst);
    ASSERT(0 == chksum);
    /*
     * Verify a few fields in the header resp. message passed to ip_tx_msg
     */
    ASSERT(ip_dst == in.sin_addr.s_addr);
    ASSERT(ip_src == 0x1402000a);
    /*
     * Header length byte contains four reserved bits
     */
    hdr_length = *((u8*) (payload + 12)) >> 4;
    /*
     * We expect 6 dwords (20 bytes TCP header and 4 bytes for MSS option)
     */
    ASSERT(6 == hdr_length);
    /*
     * Is SYN bit set?
     */
    ctrl_flags = *((u8*) (payload + 13));
    ASSERT(0x2 == ctrl_flags);
    /*
     * Verify destination port
     */
    dst_port = (u16*) (payload + 2);
    ASSERT(ntohs(*dst_port) == 30000);
    /*
     * Verify that MSS options are sent. Thus first byte after header is 2, second byte is 4,
     * third and fourth byte are 1460 in network byte order
     */
    ASSERT(*(payload + sizeof(tcp_hdr_t)) == 2);
    ASSERT(*(payload + sizeof(tcp_hdr_t) + 1) == 4);
    mss = (u16*)(payload + sizeof(tcp_hdr_t) + 2);
    ASSERT(ntohs(*mss) == 1460);
    /*
     * Reset MTU
     */
    mtu = 576;
    /*
     * "Close" socket so that is does not interfere with later test cases
     */
    socket->ops->close(socket, 0);
    return 0;
}

/*
 * Testcase 57:
 * Create a new socket and try to connect it. Verify that the MSS is transmitted as option along with the SYN
 * and that the MSS received in the response is correctly handled
 */
int testcase57() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u16* mss;
    net_msg_t* syn_ack;
    u32 syn_seq_no;
    /*
     * Do basic initialization of socket
     */
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    tcp_init();
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Set MTU to 1500
     */
    mtu = 1500;
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
     syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Calculate checksum
     */
    chksum = validate_tcp_checksum(24, (u16*) payload, ip_src, ip_dst);
    ASSERT(0 == chksum);
    /*
     * Verify a few fields in the header resp. message passed to ip_tx_msg
     */
    ASSERT(ip_dst == in.sin_addr.s_addr);
    ASSERT(ip_src == 0x1402000a);
    /*
     * Header length byte contains four reserved bits
     */
    hdr_length = *((u8*) (payload + 12)) >> 4;
    /*
     * We expect 6 dwords (20 bytes TCP header and 4 bytes for MSS option)
     */
    ASSERT(6 == hdr_length);
    /*
     * Is SYN bit set?
     */
    ctrl_flags = *((u8*) (payload + 13));
    ASSERT(0x2 == ctrl_flags);
    /*
     * Verify destination port
     */
    dst_port = (u16*) (payload + 2);
    ASSERT(ntohs(*dst_port) == 30000);
    /*
     * Verify that MSS options are sent. Thus first byte after header is 2, second byte is 4,
     * third and fourth byte are 1460 in network byte order
     */
    ASSERT(*(payload + sizeof(tcp_hdr_t)) == 2);
    ASSERT(*(payload + sizeof(tcp_hdr_t) + 1) == 4);
    mss = (u16*)(payload + sizeof(tcp_hdr_t) + 2);
    ASSERT(ntohs(*mss) == 1460);
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window 600
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack_mss(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 600, 800);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    __net_loglevel = 0;
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that SMSS has been set to 800
     */
    ASSERT(800 == socket->proto.tcp.smss);
    /*
    /*
     * Reset MTU
     */
    mtu = 576;
    return 0;
}

/*
 * Testcase 58
 * Create a new socket and try to connect it. Verify that the MSS is transmitted as option along with the SYN
 * even if the SYN is retransmitted
 */
int testcase58() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u16* mss;
    net_msg_t* syn_ack;
    u32 syn_seq_no;
    int i;
    /*
     * Do basic initialization of socket
     */
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    tcp_init();
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Set MTU to 1500
     */
    mtu = 1500;
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
     syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Calculate checksum
     */
    chksum = validate_tcp_checksum(24, (u16*) payload, ip_src, ip_dst);
    ASSERT(0 == chksum);
    /*
     * Verify a few fields in the header resp. message passed to ip_tx_msg
     */
    ASSERT(ip_dst == in.sin_addr.s_addr);
    ASSERT(ip_src == 0x1402000a);
    /*
     * Header length byte contains four reserved bits
     */
    hdr_length = *((u8*) (payload + 12)) >> 4;
    /*
     * We expect 6 dwords (20 bytes TCP header and 4 bytes for MSS option)
     */
    ASSERT(6 == hdr_length);
    /*
     * Is SYN bit set?
     */
    ctrl_flags = *((u8*) (payload + 13));
    ASSERT(0x2 == ctrl_flags);
    /*
     * Verify destination port
     */
    dst_port = (u16*) (payload + 2);
    ASSERT(ntohs(*dst_port) == 30000);
    /*
     * Verify that MSS options are sent. Thus first byte after header is 2, second byte is 4,
     * third and fourth byte are 1460 in network byte order
     */
    ASSERT(*(payload + sizeof(tcp_hdr_t)) == 2);
    ASSERT(*(payload + sizeof(tcp_hdr_t) + 1) == 4);
    mss = (u16*)(payload + sizeof(tcp_hdr_t) + 2);
    ASSERT(ntohs(*mss) == 1460);
    /*
     * Now simulate that 15 seconds pass and we do a retransmission
     */
    ip_tx_msg_called = 0;
    ASSERT(socket->proto.tcp.rtx_timer.time == 15*TCP_HZ);
    for (i = 0; i < 15*TCP_HZ - 1; i++) {
        tcp_do_tick();
        ASSERT(0 == ip_tx_msg_called);
    }
    /*
     * Next tick should retransmit SYN
     */
    ASSERT(socket->proto.tcp.rtx_timer.time == 1);
    tcp_do_tick();
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Repeat checks
     */
    ASSERT(syn_seq_no == htonl(*((u32*) (payload + 4))));
    hdr_length = *((u8*) (payload + 12)) >> 4;
    ASSERT(6 == hdr_length);
    chksum = validate_tcp_checksum(24, (u16*) payload, ip_src, ip_dst);
    ASSERT(0 == chksum);
    ASSERT(ip_dst == in.sin_addr.s_addr);
    ASSERT(ip_src == 0x1402000a);
    ctrl_flags = *((u8*) (payload + 13));
    ASSERT(0x2 == ctrl_flags);
    dst_port = (u16*) (payload + 2);
    ASSERT(ntohs(*dst_port) == 30000);
    ASSERT(*(payload + sizeof(tcp_hdr_t)) == 2);
    ASSERT(*(payload + sizeof(tcp_hdr_t) + 1) == 4);
    mss = (u16*)(payload + sizeof(tcp_hdr_t) + 2);
    ASSERT(ntohs(*mss) == 1460);
    /*
     * Reset MTU
     */
    mtu = 576;
    return 0;
}

/*
 * Testcase 59:
 * Create a new socket and try to connect it. Verify that if the MSS in the SYN-ACK which we receive exceeds the
 * maximum determined by the local MTU, the local MTU minus the header bytes is used
 */
int testcase59() {
   struct sockaddr_in in;
   struct sockaddr_in* in_ptr;
   u16 chksum;
   u16 actual_chksum;
   u8 hdr_length;
   u8 ctrl_flags;
   u16* dst_port;
   u16* mss;
   net_msg_t* syn_ack;
   u32 syn_seq_no;
   /*
    * Do basic initialization of socket
    */
   socket_t* socket;
   socket = (socket_t*) malloc(sizeof(socket_t));
   ASSERT(socket);
   socket->bound = 0;
   socket->connected = 0;
   tcp_init();
   /*
    * and call tcp socket creation
    */
   tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
   /*
    * Set MTU to 1500
    */
   mtu = 1500;
   /*
    * Now try to connect to 10.0.2.21 / port 30000
    */
   ip_tx_msg_called = 0;
   in.sin_family =AF_INET;
   in.sin_port = htons(30000);
   in.sin_addr.s_addr = 0x1502000a;
   ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
   /*
    * and verify that ip_tx_msg has been called
    */
   ASSERT(1 == ip_tx_msg_called);
   /*
    * Extract sequence number from SYN
    */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
   /*
    * Calculate checksum
    */
   chksum = validate_tcp_checksum(24, (u16*) payload, ip_src, ip_dst);
   ASSERT(0 == chksum);
   /*
    * Verify a few fields in the header resp. message passed to ip_tx_msg
    */
   ASSERT(ip_dst == in.sin_addr.s_addr);
   ASSERT(ip_src == 0x1402000a);
   /*
    * Header length byte contains four reserved bits
    */
   hdr_length = *((u8*) (payload + 12)) >> 4;
   /*
    * We expect 6 dwords (20 bytes TCP header and 4 bytes for MSS option)
    */
   ASSERT(6 == hdr_length);
   /*
    * Is SYN bit set?
    */
   ctrl_flags = *((u8*) (payload + 13));
   ASSERT(0x2 == ctrl_flags);
   /*
    * Verify destination port
    */
   dst_port = (u16*) (payload + 2);
   ASSERT(ntohs(*dst_port) == 30000);
   /*
    * Verify that MSS options are sent. Thus first byte after header is 2, second byte is 4,
    * third and fourth byte are 1460 in network byte order
    */
   ASSERT(*(payload + sizeof(tcp_hdr_t)) == 2);
   ASSERT(*(payload + sizeof(tcp_hdr_t) + 1) == 4);
   mss = (u16*)(payload + sizeof(tcp_hdr_t) + 2);
   ASSERT(ntohs(*mss) == 1460);
   /*
    * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window 600 - use MSS 2048
    */
   in_ptr = (struct sockaddr_in*) &socket->laddr;
   syn_ack = create_syn_ack_mss(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 600, 2048);
   /*
    * and simulate receipt of the message
    */
   ip_tx_msg_called = 0;
   tcp_rx_msg(syn_ack);
   /*
    * Now validate that status of socket is ESTABLISHED
    */
   ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
   /*
    * and that SMSS has been set to 1460 as the received MSS is not allowed
    * by our own MTU
    */
   ASSERT(1460 == socket->proto.tcp.smss);
   /*
   /*
    * Reset MTU
    */
   mtu = 576;
   return 0;
}

/*
 * Testcase 60:
 * Verify that unknown options contained in a SYN-ACK are correctly processed
 */
int testcase60() {
   struct sockaddr_in in;
   struct sockaddr_in* in_ptr;
   tcp_hdr_t* tcp_hdr;
   u16 chksum;
   u16 actual_chksum;
   u8 hdr_length;
   u8 ctrl_flags;
   u16* dst_port;
   u16* mss;
   net_msg_t* syn_ack;
   u32 syn_seq_no;
   u8* option;
   /*
    * Do basic initialization of socket
    */
   socket_t* socket;
   socket = (socket_t*) malloc(sizeof(socket_t));
   ASSERT(socket);
   socket->bound = 0;
   socket->connected = 0;
   tcp_init();
   /*
    * and call tcp socket creation
    */
   tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
   /*
    * Set MTU to 1500
    */
   mtu = 1500;
   /*
    * Now try to connect to 10.0.2.21 / port 30000
    */
   ip_tx_msg_called = 0;
   in.sin_family =AF_INET;
   in.sin_port = htons(30000);
   in.sin_addr.s_addr = 0x1502000a;
   ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
   /*
    * and verify that ip_tx_msg has been called
    */
   ASSERT(1 == ip_tx_msg_called);
   /*
    * Extract sequence number from SYN
    */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
   /*
    * Calculate checksum
    */
   chksum = validate_tcp_checksum(24, (u16*) payload, ip_src, ip_dst);
   ASSERT(0 == chksum);
   /*
    * Verify a few fields in the header resp. message passed to ip_tx_msg
    */
   ASSERT(ip_dst == in.sin_addr.s_addr);
   ASSERT(ip_src == 0x1402000a);
   /*
    * Header length byte contains four reserved bits
    */
   hdr_length = *((u8*) (payload + 12)) >> 4;
   /*
    * We expect 6 dwords (20 bytes TCP header and 4 bytes for MSS option)
    */
   ASSERT(6 == hdr_length);
   /*
    * Is SYN bit set?
    */
   ctrl_flags = *((u8*) (payload + 13));
   ASSERT(0x2 == ctrl_flags);
   /*
    * Verify destination port
    */
   dst_port = (u16*) (payload + 2);
   ASSERT(ntohs(*dst_port) == 30000);
   /*
    * Verify that MSS options are sent. Thus first byte after header is 2, second byte is 4,
    * third and fourth byte are 1460 in network byte order
    */
   ASSERT(*(payload + sizeof(tcp_hdr_t)) == 2);
   ASSERT(*(payload + sizeof(tcp_hdr_t) + 1) == 4);
   mss = (u16*)(payload + sizeof(tcp_hdr_t) + 2);
   ASSERT(ntohs(*mss) == 1460);
   /*
    * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window 600 - use window 1460
    */
   in_ptr = (struct sockaddr_in*) &socket->laddr;
   syn_ack = create_syn_ack_mss(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 600, 2048);
   /*
    * Now add a second option - add four NOPs
    */
   option = ((u8*)syn_ack->tcp_hdr) + sizeof(tcp_hdr_t) + 4;
   option[0] = 1;
   option[1] = 1;
   option[2] = 1;
   option[3] = 1;
   tcp_hdr = (tcp_hdr_t*) (syn_ack->tcp_hdr);
   tcp_hdr->hlength = 7;
   tcp_hdr->checksum = 0;
   chksum = validate_tcp_checksum(28, (u16*) tcp_hdr, ip_src, ip_dst);
   tcp_hdr->checksum = htons(chksum);
   syn_ack->ip_length += 4;
   /*
    * and simulate receipt of the message
    */
   ip_tx_msg_called = 0;
   tcp_rx_msg(syn_ack);
   /*
    * Now validate that status of socket is ESTABLISHED
    */
   ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
   /*
    * and that SMSS has been set to 1460
    */
   ASSERT(1460 == socket->proto.tcp.smss);
   /*
   /*
    * Reset MTU
    */
   mtu = 576;
   return 0;
}

/*
 * Testcase 61:
 *
 * Test retransmission based on retransmission timer - retransmit one segment only and simulate the
 * case that peer reassembles
 *
 *
 *  #   Socket under test                                         Peer
 * -------------------------------------------------------------------------------------------------------
 *
 *  1   SYN, SEQ = syn_seq_no, ACK_NO = 0 ----------------------->
 *  2                                                         <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *  3   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 0 ---------->
 *  4   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 536 -------->
 *  5   ACK, SEQ = syn_seq_no + 537, ACK_NO = 2, LEN = 536 ------>
 *  6   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 536 -------->
 *  7                                                         <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1073
 *
 */
int testcase61() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    net_msg_t* text;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    int i;
    unsigned char buffer[8192];
    /*
     * Fill buffer
     */
    for (i = 0; i < 1024; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window 14600
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 14600);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that the window size has been updated
     */
    ASSERT(14600 == socket->proto.tcp.snd_wnd);
    /*
     * Fake congestion window to make this testcase work
     */
    socket->proto.tcp.cwnd = 536*2;
    /*
     * Now send three segments
     */
    ip_tx_msg_called = 0;
    ASSERT(536*3 == socket->ops->send(socket, (void*) buffer, 536*3, 0));
    /*
     * and verify that two segments have been sent
     */
    ASSERT( 2 == ip_tx_msg_called);
    /*
     * Check that retransmission timer is set to 1 second
     */
    ASSERT(socket->proto.tcp.rtx_timer.time == TCP_HZ);
    /*
     * Now simulate first ticks - should not change anything
     */
    ip_tx_msg_called = 0;
    for (i = 0; i < TCP_HZ - 1; i++)
        tcp_do_tick();
    ASSERT(ip_tx_msg_called == 0);
    /*
     * and simulate tick TCP_HZ - this should initiate the retransmission
     */
    tcp_do_tick();
    ASSERT(1 == ip_tx_msg_called);
    /*
     * and should have set the timer to twice its initial value ("exponential backoff")
     */
    ASSERT(socket->proto.tcp.rtx_timer.time == 2*TCP_HZ);
    /*
     * Check data
     */
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(syn_seq_no + 1 == ntohl(tcp_hdr->seq_no));
    ASSERT(536 == ip_payload_len - tcp_hdr->hlength*sizeof(u32));
    for (i = 0; i < 536; i++) {
        ASSERT(buffer[i] == ((u8*)(payload + tcp_hdr->hlength*sizeof(u32)))[i]);
    }
    /*
     * Now assume that the peer has received the outstanding message and reassembled our segments. It will
     * now send a cumulative ACK for both
     */
    ip_tx_msg_called = 0;
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1073, 8192, buffer, 0);
    tcp_rx_msg(text);
    /*
     * We now should have sent the third segment
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Check SND_UNA, SND_NXT and SND_MAX
     */
    ASSERT(socket->proto.tcp.snd_una == syn_seq_no + 1073);
    ASSERT(socket->proto.tcp.snd_max == socket->proto.tcp.snd_nxt);
    ASSERT(socket->proto.tcp.snd_una + 536 == socket->proto.tcp.snd_nxt);
    /*
     * Timer should be set again
     */
    ASSERT(socket->proto.tcp.rtx_timer.time);
    /*
     * and this segment should be timed
     */
    ASSERT(socket->proto.tcp.timed_segment == syn_seq_no + 1073);
    /*
     * Close socket
     */
    socket->ops->close(socket, 0);
    return 0;
}

/*
 * Testcase 62:
 * Timeout after connection has been established
 *
 * For data, we have a timeout of at least 1 second
 * As we try five retransmissions in total, we spent about
 * 1 + 2 + 4 + 8 + 16 + 32 = 63
 * seconds in total with the connection attempt and time out.
 */
int testcase62() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    net_msg_t* syn_ack;
    int i;
    tcp_hdr_t* tcp_hdr;
    int attempt;
    int j;
    u32 syn_seq_no;
    int ticks = 0;
    unsigned char buffer[8192];
    /*
     * Do basic initialization of socket
     */
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    tcp_init();
    /*
     * and call tcp socket creation
     */
    cond_broadcast_called = 0;
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    ASSERT(0 == cond_broadcast_called);
    /*
     * This should have produced a SYN
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window 14600
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 14600);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * Send data. As our congestion window is SMSS, this should produce one
     * segment only
     */
    ip_tx_msg_called = 0;
    ASSERT(1024 == socket->ops->send(socket, buffer, 1024,0));
    ASSERT(1 == ip_tx_msg_called);
    /*
     * As the RTO is 1 second for data, simulate TCP_HZ ticks
     */
    ASSERT(socket->proto.tcp.rtx_timer.time == RTO_INIT);
    ip_tx_msg_called = 0;
    for (i = 0; i < RTO_INIT - 1; i++) {
        ticks++;
        tcp_do_tick();
        ASSERT(0 == ip_tx_msg_called);
    }
    tcp_do_tick();
    ticks++;
    attempt = 1;
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->syn);
    /*
     * Now repeat this - timer should double with each attempt
     */
    for (j = 0; j < 5; j++) {
        ip_tx_msg_called = 0;
        for (i = 0; i < (RTO_INIT << attempt) - 1; i++) {
            tcp_do_tick();
            ticks++;
            ASSERT(0 == ip_tx_msg_called);
        }
        ticks++;
        cond_broadcast_called = 0;
        tcp_do_tick();
        attempt++;
        ASSERT(1 == ip_tx_msg_called);
        if (4 == j) {
            /*
             * Last attempt should result in a reset
             */
            ASSERT(1 == tcp_hdr->rst);
            ASSERT(1 == tcp_hdr->ack);
            ASSERT(0 == tcp_hdr->syn);
            ASSERT(-137 == socket->error);
            ASSERT(cond_broadcast_called == 2);
        }
        else {
            /*
             * Retransmit
             */
            ASSERT(0 == tcp_hdr->rst);
            ASSERT(1 == tcp_hdr->ack);
            ASSERT(0 == tcp_hdr->syn);
        }
    }
    return 0;
}

/*
 * Testcase 63:
 *
 * Window probes do not time out
 *
 * In this case, we make sure that window probes are sent longer
 */
int testcase63() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    net_msg_t* syn_ack;
    int i;
    tcp_hdr_t* tcp_hdr;
    int attempt;
    int j;
    int rto;
    u32 syn_seq_no;
    int ticks = 0;
    unsigned char buffer[8192];
    /*
     * Do basic initialization of socket
     */
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    tcp_init();
    /*
     * and call tcp socket creation
     */
    cond_broadcast_called = 0;
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    ASSERT(0 == cond_broadcast_called);
    /*
     * This should have produced a SYN
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window 14600
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 14600);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * Simulate the case that our peer has announced a zero window
     */
    socket->proto.tcp.snd_wnd = 0;
    /*
     * Send data. This should not give any output, but set the persist timer
     */
    ip_tx_msg_called = 0;
    ASSERT(1024 == socket->ops->send(socket, buffer, 1024,0));
    ASSERT(0 == ip_tx_msg_called);
    ASSERT(socket->proto.tcp.persist_timer.time == RTO_INIT);
    /*
     * As the RTO is 1 second for data, simulate TCP_HZ ticks
     */
    ip_tx_msg_called = 0;
    for (i = 0; i < RTO_INIT - 1; i++) {
        ticks++;
        tcp_do_tick();
        ASSERT(0 == ip_tx_msg_called);
    }
    tcp_do_tick();
    ticks++;
    /*
     * Persist timer should have fired now
     */
    ASSERT(1 == ip_tx_msg_called);
    attempt = 1;
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->syn);
    /*
     * Now repeat this 16 times - timer should double with each attempt, but we should not time out
     */
    for (j = 0; j < 16; j++) {
        ip_tx_msg_called = 0;
        rto = (RTO_INIT << attempt);
        if (rto > RTO_MAX)
            rto = RTO_MAX;
        for (i = 0; i < rto - 1; i++) {
            tcp_do_tick();
            ticks++;
            if (ip_tx_msg_called) {
                printf("Received unexpted message for j = %d, ticks = %d, attempt = %d\n", j, ticks, attempt);
            }
            ASSERT(0 == ip_tx_msg_called);
        }
        ticks++;
        cond_broadcast_called = 0;
        tcp_do_tick();
        attempt++;
        ASSERT(1 == ip_tx_msg_called);
        ASSERT(0 == tcp_hdr->rst);
        ASSERT(1 == tcp_hdr->ack);
        ASSERT(0 == tcp_hdr->syn);
    }
    return 0;
}

/*
 * Testcase 64:
 *
 * After establishing the connection, send a few segments. Then simulate the case that the receiver shrinks
 * the window on us.
 *
 *
 *  #   Socket under test                                         Peer
 * -------------------------------------------------------------------------------------------------------
 *
 *  1   SYN, SEQ = syn_seq_no, ACK_NO = 0 ----------------------->
 *  2                                                         <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *  3   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 0 ---------->
 *  4   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 536 -------->
 *  5                                                         <-- ACK, SEQ = 2, ACK_NO = syn_seq_no + 537
 *  6   ACK, SEQ = syn_seq_no + 537, ACK_NO = 2, LEN = 536 -------->
 *  7   ACK, SEQ = syn_seq_no + 1073, ACK_NO = 2, LEN = 536 ------->
 *  8                                                         <-- ACK, SEQ = 2, ACK_NO = syn_seq_no + 1073, WIN = 100
 *
 */
int testcase64() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    net_msg_t* text;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    int i;
    unsigned char buffer[8192];
    /*
     * Fill buffer
     */
    for (i = 0; i < 2048; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    tcp_init();
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window 8192
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 8192);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that the window size has been updated
     */
    ASSERT(8192 == socket->proto.tcp.snd_wnd);
    /*
     * Now try to transmit 2048 bytes
     */
    ip_tx_msg_called = 0;
    ASSERT(2048 == socket->ops->send(socket, (void*) buffer, 2048, 0));
    /*
     * and verify that a segment containing 536 data bytes has been sent
     */
    ASSERT( 1 == ip_tx_msg_called);
    ASSERT (20 + 536 == ip_payload_len);
    /*
     * Now simulate an ACK for this segment
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1 + 536, 8192, buffer, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    /*
     * As the congestion window increases by SMSS, we should send two segments in reply
     */
    ASSERT(2 == ip_tx_msg_called);
    /*
     * Last sequence number should be 1 + 2*536 + syn_seq_no
     */
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 + 2*536 + syn_seq_no == ntohl(tcp_hdr->seq_no));
    /*
     * We now have two segments outstanding. Now simulate receipt of an ACK which acknowledges the first segment,
     * but at the same time reduces the window to 100 bytes, i.e. the left edge of the window moves to the left
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1073, 100, buffer, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    /*
     * We should not have transmitted any new data
     */
    ASSERT(0 == ip_tx_msg_called);
    /*
     * When the retransmission timer now expires, we expect that the window is filled up, i.e. we send 100 bytes of
     * data again
     */
    for (i = 0; i < RTO_INIT; i++)
        tcp_do_tick();
    ASSERT(1 == ip_tx_msg_called);
    ASSERT(ip_payload_len == sizeof(tcp_hdr_t) + 100);
    /*
     * When this segment is acknowledged, the retransmission timer will be set again as there is still data outstanding,
     * but the persist timer should not be set (even though the window is now zero)
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1073 + 100, 0, buffer, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    ASSERT(0 == ip_tx_msg_called);
    ASSERT(socket->proto.tcp.rtx_timer.time);
    ASSERT(0 == socket->proto.tcp.persist_timer.time);
    ASSERT(socket->proto.tcp.snd_wnd == 0);
    return 0;
}

/*
 * Testcase 65
 *
 * Repeat test case 34 with an initial sequence number which forces wrap-around
 */
int testcase65() {
    __useconds = 0xFFFFFFFF - 4;
    tcp_disable_cc = 1;
    return testcase34();
}

/*
 * Testcase 66
 *
 * Repeat test case 34 with congestion control enabled. This tests that when the window of the peer
 * opens up again, we send a segment immediately, thereby retransmitting the byte which we have used to
 * probe the window
 */
int testcase66() {
    tcp_disable_cc = 0;
    return testcase34();
}

/*
 * Testcase 67:
 *
 * Test a typical window probing scenario and exponential backoff - here we test the case that the byte which we use for
 * the window probe is acknowledged
 *
 *  #   Socket under test                                         Peer
 * -------------------------------------------------------------------------------------------------------
 *
 *  1   SYN, SEQ = syn_seq_no, ACK_NO = 0 ----------------------->
 *  2                                                         <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *  3   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 0 ---------->
 *  4   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 536 ---------->
 *  5                                                         <-- ACK, SEQ = 1, ACK_NO = syn_seq_no + 537, LEN = 0, WIN = 0
 *  6   ACK, SEQ = syn_seq_no + 537, ACK_NO = 2, LEN = 1 ---------->
 *  7                                                         <-- ACK, SEQ = 1, ACK_NO = syn_seq_no + 537, LEN = 0, WIN = 0
 *  8   ACK, SEQ = syn_seq_no + 537, ACK_NO = 2, LEN = 1 ---------->
 *  9                                                         <-- ACK, SEQ = 1, ACK_NO = syn_seq_no + 538, LEN = 0, WIN = 8192
 *
 */
int testcase67() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    net_msg_t* text;
    int timer;
    int i;
    unsigned char buffer[8192];
    /*
     * Fill buffer
     */
    for (i = 0; i < 8192; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    tcp_init();
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window  536
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 536);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that the window size has been updated
     */
    ASSERT(536 == socket->proto.tcp.snd_wnd);
    /*
     * Set congestion window size to a large value in order to avoid that "slow start" makes our
     * test scenario pointless
     */
    socket->proto.tcp.cwnd = 65536;
    /*
     * Fake maximum window size
     */
    socket->proto.tcp.max_wnd = 8192;
    /*
     * Now try to transmit 8192 bytes
     */
    ip_tx_msg_called = 0;
    cond_broadcast_called = 0;
    ASSERT(8192 == socket->ops->send(socket, (void*) buffer, 8192, 0));
    /*
     * and verify that one segment has been sent, containing 536 bytes of data
     */
    ASSERT( 1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(536 == ip_payload_len - sizeof(u32)*tcp_hdr->hlength);
    /*
     * Persist timer should not be set, as the ACK we expect will do the job
     * for us
     */
    ASSERT(0 == socket->proto.tcp.persist_timer.time);
    /*
     * Now simulate an ACK closing the window
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1 + 536, 0, buffer, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    /*
     * As we have more data to be sent, this should have set the persist timer - simulate that it
     * fires
     */
    timer = socket->proto.tcp.persist_timer.time;
    ASSERT(socket->proto.tcp.rto == socket->proto.tcp.persist_timer.time);
    for (i = 0; i < socket->proto.tcp.rto; i++) {
        tcp_do_tick();
        if (i < socket->proto.tcp.rto - 1)
            ASSERT(0 == ip_tx_msg_called);
    }
    /*
     * Last tick should have forced out a packet
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * As we have data to send, this packet - a zero window probe - should contain one byte of
     * data
     */
    ASSERT(ip_payload_len == ((tcp_hdr_t*) payload)->hlength*sizeof(u32) + 1);
    /*
     * This should have set the retransmission timer, applying exponential backoff
     */
    ASSERT(socket->proto.tcp.rtx_timer.time == 2*timer);
    /*
     * and cleared the persist timer
     */
    ASSERT(0 == socket->proto.tcp.persist_timer.time);
    /*
     * Now simulate reply - again window is closed
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1 + 536, 0, buffer, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    /*
     * Should not create an immediate reply as window is still closed
     */
    ASSERT(0 == ip_tx_msg_called);
    /*
     * but retransmission timer should be set
     */
    ASSERT(2*timer == socket->proto.tcp.rtx_timer.time);
    /*
     * Now simulate that retransmission timer fires
     */
    timer = socket->proto.tcp.rtx_timer.time;
    for (i = 0; i < timer - 1; i++) {
        tcp_do_tick();
        ASSERT(0 == ip_tx_msg_called);
    }
    tcp_do_tick();
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Check that we have sent another zero window probe
     */
    ASSERT(ip_payload_len == ((tcp_hdr_t*) payload)->hlength*sizeof(u32) + 1);
    /*
     * and that retransmission timer has doubled again
     */
    ASSERT(2*timer == socket->proto.tcp.rtx_timer.time);
    /*
     * Now simulate another ACK - window opens again and our window probe byte is acknowledged
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1 + 537, 8192, buffer, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    /*
     * This should trigger more data being sent - in fact we expect a full segment, starting with segment number syn_seq_no + 538
     */
    ASSERT(1 == ip_tx_msg_called);
    ASSERT(ntohl(tcp_hdr->seq_no) == syn_seq_no + 538);
    return 0;
}

/*
 * Testcase 68: bind a socket to a fully qualified local address
 */
int testcase68() {
    socket_t socket;
    struct sockaddr_in laddr;
    struct sockaddr_in* saddr;
    tcp_init();
    tcp_create_socket(&socket, AF_INET, 0);
    laddr.sin_family = AF_INET;
    laddr.sin_port = htons(30000);
    laddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    ASSERT(0 == socket.ops->bind(&socket, (struct sockaddr*) &laddr, sizeof(struct sockaddr_in)));
    /*
     * Verify that socket has actually been bound
     */
    ASSERT(1 == socket.bound);
    saddr = (struct sockaddr_in*) &socket.laddr;
    ASSERT(ntohs(saddr->sin_port) == 30000);
    ASSERT(saddr->sin_addr.s_addr == inet_addr("127.0.0.1"));
    ASSERT(saddr->sin_family == AF_INET);
    return 0;
}

/*
 * Testcase 69: bind a socket to a fully qualified local address and then try
 * to bind it once more to a different address - should fail with -EINVAL
 */
int testcase69() {
    socket_t socket;
    struct sockaddr_in laddr;
    struct sockaddr_in* saddr;
    tcp_init();
    tcp_create_socket(&socket, AF_INET, 0);
    laddr.sin_family = AF_INET;
    laddr.sin_port = htons(30000);
    laddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    ASSERT(0 == socket.ops->bind(&socket, (struct sockaddr*) &laddr, sizeof(struct sockaddr_in)));
    /*
     * Verify that socket has actually been bound
     */
    ASSERT(1 == socket.bound);
    saddr = (struct sockaddr_in*) &socket.laddr;
    ASSERT(ntohs(saddr->sin_port) == 30000);
    ASSERT(saddr->sin_addr.s_addr == inet_addr("127.0.0.1"));
    ASSERT(saddr->sin_family == AF_INET);
    /*
     * Now try to bind again
     */
    laddr.sin_port = htons(30000);
    ASSERT(-107 == socket.ops->bind(&socket, (struct sockaddr*) &laddr, sizeof(struct sockaddr_in)));
    return 0;
}

/*
 * Testcase 70: bind a socket to a fully qualified local address already in use
 */
int testcase70() {
    socket_t socket;
    socket_t second_socket;
    struct sockaddr_in laddr;
    struct sockaddr_in* saddr;
    tcp_init();
    tcp_create_socket(&socket, AF_INET, 0);
    laddr.sin_family = AF_INET;
    laddr.sin_port = htons(30000);
    laddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    ASSERT(0 == socket.ops->bind(&socket, (struct sockaddr*) &laddr, sizeof(struct sockaddr_in)));
    /*
     * Verify that socket has actually been bound
     */
    ASSERT(1 == socket.bound);
    saddr = (struct sockaddr_in*) &socket.laddr;
    ASSERT(ntohs(saddr->sin_port) == 30000);
    ASSERT(saddr->sin_addr.s_addr == inet_addr("127.0.0.1"));
    ASSERT(saddr->sin_family == AF_INET);
    /*
     * Now try to bind another socket to the same address - this should return -EADDRINUSE
     */
    tcp_create_socket(&second_socket, AF_INET, 0);
    ASSERT(-135 == second_socket.ops->bind(&second_socket, (struct sockaddr*) &laddr, sizeof(struct sockaddr_in)));
    return 0;
}

/*
 * Testcase 71: bind a socket with port number 0 and verify that kernel choses an ephemeral port
 */
int testcase71() {
    socket_t socket;
    socket_t second_socket;
    struct sockaddr_in laddr;
    struct sockaddr_in* saddr;
    tcp_init();
    tcp_create_socket(&socket, AF_INET, 0);
    laddr.sin_family = AF_INET;
    laddr.sin_port = 0;
    laddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    ASSERT(0 == socket.ops->bind(&socket, (struct sockaddr*) &laddr, sizeof(struct sockaddr_in)));
    /*
     * Verify that socket has actually been bound
     */
    ASSERT(1 == socket.bound);
    saddr = (struct sockaddr_in*) &socket.laddr;
    ASSERT(ntohs(saddr->sin_port) == 49152);
    ASSERT(saddr->sin_addr.s_addr == inet_addr("127.0.0.1"));
    ASSERT(saddr->sin_family == AF_INET);
    /*
     * Now create and bind another socket and check that the port number is
     * raised by one
     */
    tcp_create_socket(&second_socket, AF_INET, 0);
    ASSERT(0 == socket.ops->bind(&second_socket, (struct sockaddr*) &laddr, sizeof(struct sockaddr_in)));
    ASSERT(1 == second_socket.bound);
    saddr = (struct sockaddr_in*) &second_socket.laddr;
    ASSERT(ntohs(saddr->sin_port) == 49153);
    ASSERT(saddr->sin_addr.s_addr == inet_addr("127.0.0.1"));
    ASSERT(saddr->sin_family == AF_INET);
    return 0;
}

/*
 * Testcase 72: bind a socket, then listen on it. Verify that the status of the socket
 * changes to LISTEN
 */
int testcase72() {
    socket_t socket;
    socket_t second_socket;
    struct sockaddr_in laddr;
    struct sockaddr_in* saddr;
    tcp_init();
    tcp_create_socket(&socket, AF_INET, 0);
    laddr.sin_family = AF_INET;
    laddr.sin_port = htons(30000);
    laddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    ASSERT(0 == socket.ops->bind(&socket, (struct sockaddr*) &laddr, sizeof(struct sockaddr_in)));
    /*
     * Now listen on socket
     */
    socket.ops->listen(&socket);
    ASSERT(socket.proto.tcp.status == TCP_STATUS_LISTEN);
    return 0;
}

/*
 * Testcase 73: Listen on a socket which has not yet been bound. Verify that the socket is
 * bound to IPADDR_ANY with an ephemeral port
 */
int testcase73() {
    socket_t socket;
    struct sockaddr_in* saddr;
    tcp_init();
    tcp_create_socket(&socket, AF_INET, 0);
    /*
     * Now listen on socket
     */
    ASSERT(0 == socket.ops->listen(&socket));
    ASSERT(socket.proto.tcp.status == TCP_STATUS_LISTEN);
    saddr = (struct sockaddr_in*) &socket.laddr;
    ASSERT(INADDR_ANY == saddr->sin_addr.s_addr);
    ASSERT(49152 == ntohs(saddr->sin_port));
    return 0;
}

/*
 * Testcase 74: bind a socket, then listen on it. Verify that the status of the socket
 * changes to LISTEN. Simulate receipt of a SYN and verify that a SYN-ACK is sent in response
 * and a new socket is created
 */
int testcase74() {
    socket_t socket;
    socket_t* new_socket;
    struct sockaddr_in laddr;
    struct sockaddr_in* saddr;
    net_msg_t* syn;
    tcp_hdr_t* tcp_hdr;
    u16* mss;
    tcp_init();
    mtu = 1500;
    tcp_create_socket(&socket, AF_INET, 0);
    laddr.sin_family = AF_INET;
    laddr.sin_port = htons(30000);
    laddr.sin_addr.s_addr = inet_addr("10.0.2.20");
    ASSERT(0 == socket.ops->bind(&socket, (struct sockaddr*) &laddr, sizeof(struct sockaddr_in)));
    /*
     * Now listen on socket
     */
    socket.max_connection_backlog = 15;
    socket.ops->listen(&socket);
    ASSERT(socket.proto.tcp.status == TCP_STATUS_LISTEN);
    /*
     * Simulate receipt of SYN
     */
    syn = create_syn(inet_addr("10.0.2.21"), inet_addr("10.0.2.20"), 1024, 30000, 100, 8192, 800);
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn);
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->syn);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(101 == ntohl(tcp_hdr->ack_no));
    /*
     * Locate new socket on queue
     */
    new_socket = socket.so_queue_head;
    ASSERT(new_socket);
    ASSERT(new_socket->proto.tcp.status == TCP_STATUS_SYN_RCVD);
    ASSERT(new_socket->proto.tcp.isn == ntohl(tcp_hdr->seq_no));
    ASSERT(new_socket->proto.tcp.isn + 1 == new_socket->proto.tcp.snd_nxt);
    /*
     * Verify that our SYN_ACK contains the MSS 1460
     */
    ASSERT(6 == tcp_hdr->hlength);
    ASSERT(*(payload + sizeof(tcp_hdr_t)) == 2);
    ASSERT(*(payload + sizeof(tcp_hdr_t) + 1) == 4);
    mss = (u16*)(payload + sizeof(tcp_hdr_t) + 2);
    ASSERT(ntohs(*mss) == 1460);
    /*
     * and that our own SMSS has been set to 800
     */
    ASSERT(new_socket->proto.tcp.smss == 800);
    /*
     * Check that foreign address of the socket has been updated
     */
    saddr = (struct sockaddr_in*) &new_socket->faddr;
    ASSERT(saddr->sin_addr.s_addr == inet_addr("10.0.2.21"));
    ASSERT(saddr->sin_family == AF_INET);
    ASSERT(saddr->sin_port == ntohs(1024));
    /*
     * but the foreign address of the original socket remains unchanged
     */
    saddr = (struct sockaddr_in*) &socket.faddr;
    ASSERT(0 == saddr->sin_addr.s_addr);
    ASSERT(0 == saddr->sin_port);
    ASSERT(TCP_STATUS_LISTEN == socket.proto.tcp.status);
    return 0;
}

/*
 * Testcase 75: bind a socket to a non-fully qualified address, then listen on it. Verify that the status of the socket
 * changes to LISTEN. Simulate receipt of a SYN and verify that a SYN-ACK is sent in response
 * and the status of the new socket changes to SYN-RCVD. Also check that the local address of the new socket is updated
 */
int testcase75() {
    socket_t socket;
    socket_t* new_socket;
    struct sockaddr_in laddr;
    struct sockaddr_in* saddr;
    net_msg_t* syn;
    tcp_hdr_t* tcp_hdr;
    u16* mss;
    tcp_init();
    mtu = 1500;
    tcp_create_socket(&socket, AF_INET, 0);
    laddr.sin_family = AF_INET;
    laddr.sin_port = htons(30000);
    laddr.sin_addr.s_addr = 0;
    ASSERT(0 == socket.ops->bind(&socket, (struct sockaddr*) &laddr, sizeof(struct sockaddr_in)));
    /*
     * Now listen on socket
     */
    socket.max_connection_backlog = 15;
    socket.ops->listen(&socket);
    ASSERT(socket.proto.tcp.status == TCP_STATUS_LISTEN);
    /*
     * Simulate receipt of SYN
     */
    syn = create_syn(inet_addr("10.0.2.21"), inet_addr("10.0.2.20"), 1024, 30000, 100, 8192, 800);
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn);
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->syn);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(101 == ntohl(tcp_hdr->ack_no));
    new_socket = socket.so_queue_head;
    ASSERT(new_socket);
    ASSERT(new_socket->proto.tcp.status == TCP_STATUS_SYN_RCVD);
    /*
     * Verify that our SYN_ACK contains the MSS 1460
     */
    ASSERT(6 == tcp_hdr->hlength);
    ASSERT(*(payload + sizeof(tcp_hdr_t)) == 2);
    ASSERT(*(payload + sizeof(tcp_hdr_t) + 1) == 4);
    mss = (u16*)(payload + sizeof(tcp_hdr_t) + 2);
    ASSERT(ntohs(*mss) == 1460);
    /*
     * and that our own SMSS has been set to 800
     */
    ASSERT(new_socket->proto.tcp.smss == 800);
    /*
     * Check that local address of the socket has been updated
     */
    saddr = (struct sockaddr_in*) &new_socket->laddr;
    ASSERT(saddr->sin_addr.s_addr == inet_addr("10.0.2.20"));
    ASSERT(saddr->sin_family == AF_INET);
    ASSERT(saddr->sin_port == ntohs(30000));
    /*
     * but the local address of the listening socket remains
     * unchanged
     */
    saddr = (struct sockaddr_in*) &socket.laddr;
    ASSERT(0 == saddr->sin_addr.s_addr);
    return 0;
}

/*
 * Testcase 76: bind a socket to a non-fully qualified address, then listen on it. Verify that the status of the socket
 * changes to LISTEN. Simulate receipt of a SYN to create a new socket in state SYN_RCVD. Then send an ACK to this socket
 *
 */
int testcase76() {
    socket_t socket;
    socket_t* new_socket;
    struct sockaddr_in laddr;
    struct sockaddr_in* saddr;
    net_msg_t* syn;
    net_msg_t* ack;
    tcp_hdr_t* tcp_hdr;
    u16* mss;
    u32 syn_ack_seq_no;
    tcp_init();
    mtu = 1500;
    tcp_create_socket(&socket, AF_INET, 0);
    laddr.sin_family = AF_INET;
    laddr.sin_port = htons(30000);
    /*
     * Bind listening socket to INADDR_ANY
     */
    laddr.sin_addr.s_addr = 0;
    ASSERT(0 == socket.ops->bind(&socket, (struct sockaddr*) &laddr, sizeof(struct sockaddr_in)));
    /*
     * Now listen on socket
     */
    socket.max_connection_backlog = 15;
    socket.ops->listen(&socket);
    ASSERT(socket.proto.tcp.status == TCP_STATUS_LISTEN);
    /*
     * Simulate receipt of SYN
     */
    syn = create_syn(inet_addr("10.0.2.21"), inet_addr("10.0.2.20"), 1024, 30000, 100, 8192, 800);
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn);
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->syn);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(101 == ntohl(tcp_hdr->ack_no));
    new_socket = socket.so_queue_head;
    ASSERT(new_socket);
    ASSERT(new_socket->proto.tcp.status == TCP_STATUS_SYN_RCVD);
    /*
     * Verify that our SYN_ACK contains the MSS 1460
     */
    ASSERT(6 == tcp_hdr->hlength);
    ASSERT(*(payload + sizeof(tcp_hdr_t)) == 2);
    ASSERT(*(payload + sizeof(tcp_hdr_t) + 1) == 4);
    mss = (u16*)(payload + sizeof(tcp_hdr_t) + 2);
    ASSERT(ntohs(*mss) == 1460);
    /*
     * Extract sequence number for later use
     */
    syn_ack_seq_no = ntohl(tcp_hdr->seq_no);
    /*
     * and that our own SMSS has been set to 800
     */
    ASSERT(new_socket->proto.tcp.smss == 800);
    /*
     * Check that local address of the socket has been updated
     */
    saddr = (struct sockaddr_in*) &new_socket->laddr;
    ASSERT(saddr->sin_addr.s_addr == inet_addr("10.0.2.20"));
    ASSERT(saddr->sin_family == AF_INET);
    ASSERT(saddr->sin_port == ntohs(30000));
    /*
     * but the local address of the listening socket remains
     * unchanged
     */
    saddr = (struct sockaddr_in*) &socket.laddr;
    ASSERT(0 == saddr->sin_addr.s_addr);
    /*
     * Now send an ACK to the newly created socket
     */
    ack = create_text(inet_addr("10.0.2.21"), inet_addr("10.0.2.20"), 1024, 30000, 101, syn_ack_seq_no + 1, 8192, 0, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(ack);
    /*
     * Verify that new socket has changed to "established"
     */
    ASSERT(new_socket->proto.tcp.status == TCP_STATUS_ESTABLISHED);
    /*
     * As we have announced a MSS of 800 in the SYN, we expect that the SMSS of the new socket
     * is now 800 (as our own MTU is 1500)
     */
    ASSERT(800 == new_socket->proto.tcp.smss);
    return 0;
}

/*
 * Testcase 77: bind a socket to a fully qualified address, then listen on it. Verify that the status of the socket
 * changes to LISTEN. Simulate receipt of a SYN to create a new socket in state SYN_RCVD. Then send an ACK to this socket
 *
 */
int testcase77() {
    socket_t socket;
    socket_t* new_socket;
    struct sockaddr_in laddr;
    struct sockaddr_in* saddr;
    net_msg_t* syn;
    net_msg_t* ack;
    tcp_hdr_t* tcp_hdr;
    u16* mss;
    u32 syn_ack_seq_no;
    tcp_init();
    mtu = 1500;
    tcp_create_socket(&socket, AF_INET, 0);
    laddr.sin_family = AF_INET;
    laddr.sin_port = htons(30000);
    laddr.sin_addr.s_addr = inet_addr("10.0.2.20");
    ASSERT(0 == socket.ops->bind(&socket, (struct sockaddr*) &laddr, sizeof(struct sockaddr_in)));
    /*
     * Now listen on socket
     */
    socket.max_connection_backlog = 15;
    socket.ops->listen(&socket);
    ASSERT(socket.proto.tcp.status == TCP_STATUS_LISTEN);
    /*
     * Simulate receipt of SYN
     */
    syn = create_syn(inet_addr("10.0.2.21"), inet_addr("10.0.2.20"), 1024, 30000, 100, 8192, 800);
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn);
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->syn);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(101 == ntohl(tcp_hdr->ack_no));
    new_socket = socket.so_queue_head;
    ASSERT(new_socket);
    ASSERT(new_socket->proto.tcp.status == TCP_STATUS_SYN_RCVD);
    /*
     * Verify that our SYN_ACK contains the MSS 1460
     */
    ASSERT(6 == tcp_hdr->hlength);
    ASSERT(*(payload + sizeof(tcp_hdr_t)) == 2);
    ASSERT(*(payload + sizeof(tcp_hdr_t) + 1) == 4);
    mss = (u16*)(payload + sizeof(tcp_hdr_t) + 2);
    ASSERT(ntohs(*mss) == 1460);
    /*
     * Extract sequence number for later use
     */
    syn_ack_seq_no = ntohl(tcp_hdr->seq_no);
    /*
     * and that our own SMSS has been set to 800
     */
    ASSERT(new_socket->proto.tcp.smss == 800);
    /*
     * Check that local address of the socket has been updated
     */
    saddr = (struct sockaddr_in*) &new_socket->laddr;
    ASSERT(saddr->sin_addr.s_addr == inet_addr("10.0.2.20"));
    ASSERT(saddr->sin_family == AF_INET);
    ASSERT(saddr->sin_port == ntohs(30000));
    /*
     * but the local address of the listening socket remains
     * unchanged
     */
    saddr = (struct sockaddr_in*) &socket.laddr;
    ASSERT(inet_addr("10.0.2.20") == saddr->sin_addr.s_addr);
    /*
     * Now send an ACK to the newly created socket
     */
    ack = create_text(inet_addr("10.0.2.21"), inet_addr("10.0.2.20"), 1024, 30000, 101, syn_ack_seq_no + 1, 8192, 0, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(ack);
    /*
     * Verify that new socket has changed to "established"
     */
    ASSERT(new_socket->proto.tcp.status == TCP_STATUS_ESTABLISHED);
    /*
     * and old socket is still listening
     */
    ASSERT(socket.proto.tcp.status == TCP_STATUS_LISTEN);
    /*
     * As we have announced a MSS of 800 in the SYN, we expect that the SMSS of the new socket
     * is now 800 (as our own MTU is 1500)
     */
    ASSERT(800 == new_socket->proto.tcp.smss);
    return 0;
}

/*
 * Testcase 78: create a new socket as passive open and receive data on it. Then simulate a FIN being received
 *
 */
int testcase78() {
    socket_t socket;
    socket_t* new_socket;
    struct sockaddr_in laddr;
    struct sockaddr_in* saddr;
    net_msg_t* syn;
    net_msg_t* ack;
    tcp_hdr_t* tcp_hdr;
    u16* mss;
    u32 syn_ack_seq_no;
    net_msg_t* text;
    net_msg_t* fin;
    unsigned char buffer[256];
    int i;
    tcp_init();
    mtu = 1500;
    tcp_create_socket(&socket, AF_INET, 0);
    laddr.sin_family = AF_INET;
    laddr.sin_port = htons(30000);
    laddr.sin_addr.s_addr = inet_addr("10.0.2.20");
    ASSERT(0 == socket.ops->bind(&socket, (struct sockaddr*) &laddr, sizeof(struct sockaddr_in)));
    /*
     * Now listen on socket
     */
    socket.max_connection_backlog = 15;
    socket.ops->listen(&socket);
    ASSERT(socket.proto.tcp.status == TCP_STATUS_LISTEN);
    /*
     * Simulate receipt of SYN
     */
    syn = create_syn(inet_addr("10.0.2.21"), inet_addr("10.0.2.20"), 1024, 30000, 100, 8192, 800);
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn);
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->syn);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(101 == ntohl(tcp_hdr->ack_no));
    new_socket = socket.so_queue_head;
    ASSERT(new_socket);
    ASSERT(new_socket->proto.tcp.status == TCP_STATUS_SYN_RCVD);
    /*
     * Verify that our SYN_ACK contains the MSS 1460
     */
    ASSERT(6 == tcp_hdr->hlength);
    ASSERT(*(payload + sizeof(tcp_hdr_t)) == 2);
    ASSERT(*(payload + sizeof(tcp_hdr_t) + 1) == 4);
    mss = (u16*)(payload + sizeof(tcp_hdr_t) + 2);
    ASSERT(ntohs(*mss) == 1460);
    /*
     * Extract sequence number for later use
     */
    syn_ack_seq_no = ntohl(tcp_hdr->seq_no);
    /*
     * and that our own SMSS has been set to 800
     */
    ASSERT(new_socket->proto.tcp.smss == 800);
    /*
     * Check that local address of the socket has been updated
     */
    saddr = (struct sockaddr_in*) &new_socket->laddr;
    ASSERT(saddr->sin_addr.s_addr == inet_addr("10.0.2.20"));
    ASSERT(saddr->sin_family == AF_INET);
    ASSERT(saddr->sin_port == ntohs(30000));
    /*
     * but the local address of the listening socket remains
     * unchanged
     */
    saddr = (struct sockaddr_in*) &socket.laddr;
    ASSERT(inet_addr("10.0.2.20") == saddr->sin_addr.s_addr);
    /*
     * Now send an ACK to the newly created socket
     */
    ack = create_text(inet_addr("10.0.2.21"), inet_addr("10.0.2.20"), 1024, 30000, 101, syn_ack_seq_no + 1, 8192, 0, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(ack);
    /*
     * Verify that new socket has changed to "established"
     */
    ASSERT(new_socket->proto.tcp.status == TCP_STATUS_ESTABLISHED);
    /*
     * and old socket is still listening
     */
    ASSERT(socket.proto.tcp.status == TCP_STATUS_LISTEN);
    /*
     * No answer should have been generated
     */
    ASSERT(0 == ip_tx_msg_called);
    /*
     * Now send 256 bytes of data to the newly created socket
     */
    ip_tx_msg_called = 0;
    for (i = 0; i < 256; i++)
        buffer[i] = i;
    text = create_text(inet_addr("10.0.2.21"), inet_addr("10.0.2.20"), 1024, 30000, 101, syn_ack_seq_no + 1, 8192, buffer, 256);
    tcp_rx_msg(text);
    ASSERT(0 == ip_tx_msg_called);
    /*
     * Read 256 bytes
     */
    for (i = 0; i < 256; i++)
        buffer[i] = 0;
    ASSERT(256 == new_socket->ops->recv(new_socket, buffer, 256, 0));
    for (i = 0; i < 256; i++)
        ASSERT(i == buffer[i]);
    /*
     * Now create a FIN
     */
    fin = create_fin_ack(inet_addr("10.0.2.21"), inet_addr("10.0.2.20"), 1024, 30000, 101 + 256, syn_ack_seq_no + 1, 8192);
    ip_tx_msg_called = 0;
    tcp_rx_msg(fin);
    /*
     * Verify that an ACK is sent for the FIN
     */
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->fin);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->syn);
    ASSERT(101 + 256 +1 == ntohl(tcp_hdr->ack_no));
    /*
     * Now close the socket
     */
    ip_tx_msg_called = 0;
    new_socket->ops->close(new_socket, 0);
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Should have sent a FIN
     */
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(1 == tcp_hdr->fin);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->syn);
    /*
     * with sequence number syn_ack_seq_no + 1
     */
    ASSERT(syn_ack_seq_no + 1 == ntohl(tcp_hdr->seq_no));
    return 0;
}

/*
 * Testcase 79: bind a socket to a fully qualified address, then listen on it. Verify that the status of the socket
 * changes to LISTEN. Simulate receipt of a SYN with source IP address INADDR_ANY and verify that no new socket is created
 *
 */
int testcase79() {
    socket_t socket;
    socket_t* new_socket;
    struct sockaddr_in laddr;
    struct sockaddr_in* saddr;
    net_msg_t* syn;
    net_msg_t* ack;
    tcp_hdr_t* tcp_hdr;
    u16* mss;
    u32 syn_ack_seq_no;
    tcp_init();
    mtu = 1500;
    tcp_create_socket(&socket, AF_INET, 0);
    laddr.sin_family = AF_INET;
    laddr.sin_port = htons(30000);
    laddr.sin_addr.s_addr = inet_addr("10.0.2.20");
    ASSERT(0 == socket.ops->bind(&socket, (struct sockaddr*) &laddr, sizeof(struct sockaddr_in)));
    /*
     * Now listen on socket
     */
    socket.ops->listen(&socket);
    ASSERT(socket.proto.tcp.status == TCP_STATUS_LISTEN);
    /*
     * Simulate receipt of SYN
     */
    syn = create_syn(0, inet_addr("10.0.2.20"), 1024, 30000, 100, 8192, 800);
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn);
    ASSERT(0 == ip_tx_msg_called);
    new_socket = socket.so_queue_head;
    ASSERT(0 == new_socket);
    return 0;
}

/*
 * Testcase 80:
 * Call tcp_select while the send buffer is empty and verify that readiness to write is signaled
 */
int testcase80() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    unsigned char buffer[SND_BUFFER_SIZE];
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now call select
     */
    ASSERT(0x2 == socket->ops->select(socket, 0, 1));
    return 0;
}

/*
 * Testcase 81:
 * Call tcp_select while the send buffer is full and verify that return code & 0x2 == 0
 */
int testcase81() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    unsigned char buffer[SND_BUFFER_SIZE];
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Put one byte into send buffer - select should still return readiness to write
     */
    ASSERT(1 == socket->ops->send(socket, buffer, 1, 0));
    ASSERT(0x2 == socket->ops->select(socket, 0, 1));
    /*
     * Now fill send buffer
     */
    ASSERT(SND_BUFFER_SIZE - 1 == socket->ops->send(socket, buffer, SND_BUFFER_SIZE - 1, 0));
    /*
     * and call select again
     */
    ASSERT(0 == socket->ops->select(socket, 0, 1));
    return 0;
}

/*
 * Testcase 82:
 * Call tcp_select while the receive buffer is empty
 */
int testcase82() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    unsigned char buffer[SND_BUFFER_SIZE];
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now call select
     */
    ASSERT(0 == socket->ops->select(socket, 1, 0));
    /*
     * We should however be able to write
     */
    ASSERT(2 == socket->ops->select(socket, 1, 1));
    return 0;
}

/*
 * Testcase 83:
 * Create a socket and establish a connection. Then simulate receipt of a single segment and test tcp_select
 *
 *  #   Socket under test                                         Peer
 * -------------------------------------------------------------------------------------------------------
 *
 *  1   SYN, SEQ = syn_seq_no, ACK_NO = 0 ----------------------->
 *  2                                                         <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *  3   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 0 ---------->
 *  4                                                         <-- ACK, SEQ = 2, ACK_NO = syn_seq_no + 1, LEN = 128
 */
int testcase83() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    net_msg_t* text;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    int i;
    unsigned char buffer[8192];
    /*
     * Fill buffer
     */
    for (i = 0; i < 128; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window 600
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 600);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * there is no data
     */
    ASSERT(0 == socket->ops->select(socket, 1, 0));
    /*
     * and that the window size has been updated
     */
    ASSERT(600 == socket->proto.tcp.snd_wnd);
    /*
     * Put together segment #4
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1, 600, buffer, 128);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    /*
     * Verify that no immediate response is sent - the ACK will be delayed!
     */
    ASSERT(0 == ip_tx_msg_called);
    /*
     * and that 128 bytes have been added to the receive queue
     */
    ASSERT(0 == socket->proto.tcp.rcv_buffer_head);
    ASSERT(128 == socket->proto.tcp.rcv_buffer_tail);
    ASSERT(1 == socket->ops->select(socket, 1, 0));
    return 0;
}

/*
 * Testcase 84: bind a socket to a fully qualified address, then listen on it. Verify that two SYNs lead to two newly created sockets
 *
 */
int testcase84() {
    socket_t socket;
    socket_t* new_socket;
    struct sockaddr_in laddr;
    struct sockaddr_in* saddr;
    net_msg_t* syn;
    net_msg_t* ack;
    tcp_hdr_t* tcp_hdr;
    u16* mss;
    u32 syn_ack_seq_no;
    tcp_init();
    mtu = 1500;
    tcp_create_socket(&socket, AF_INET, 0);
    laddr.sin_family = AF_INET;
    laddr.sin_port = htons(30000);
    laddr.sin_addr.s_addr = inet_addr("10.0.2.20");
    ASSERT(0 == socket.ops->bind(&socket, (struct sockaddr*) &laddr, sizeof(struct sockaddr_in)));
    /*
     * Now listen on socket
     */
    socket.max_connection_backlog = 15;
    socket.ops->listen(&socket);
    ASSERT(socket.proto.tcp.status == TCP_STATUS_LISTEN);
    /*
     * Simulate receipt of SYN
     */
    syn = create_syn(inet_addr("10.0.2.21"), inet_addr("10.0.2.20"), 1024, 30000, 100, 8192, 800);
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn);
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->syn);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(101 == ntohl(tcp_hdr->ack_no));
    new_socket = socket.so_queue_head;
    ASSERT(new_socket);
    ASSERT(new_socket->proto.tcp.status == TCP_STATUS_SYN_RCVD);
    /*
     * Verify that our SYN_ACK contains the MSS 1460
     */
    ASSERT(6 == tcp_hdr->hlength);
    ASSERT(*(payload + sizeof(tcp_hdr_t)) == 2);
    ASSERT(*(payload + sizeof(tcp_hdr_t) + 1) == 4);
    mss = (u16*)(payload + sizeof(tcp_hdr_t) + 2);
    ASSERT(ntohs(*mss) == 1460);
    /*
     * Extract sequence number for later use
     */
    syn_ack_seq_no = ntohl(tcp_hdr->seq_no);
    /*
     * and that our own SMSS has been set to 800
     */
    ASSERT(new_socket->proto.tcp.smss == 800);
    /*
     * Check that local address of the socket has been updated
     */
    saddr = (struct sockaddr_in*) &new_socket->laddr;
    ASSERT(saddr->sin_addr.s_addr == inet_addr("10.0.2.20"));
    ASSERT(saddr->sin_family == AF_INET);
    ASSERT(saddr->sin_port == ntohs(30000));
    /*
     * but the local address of the listening socket remains
     * unchanged
     */
    saddr = (struct sockaddr_in*) &socket.laddr;
    ASSERT(inet_addr("10.0.2.20") == saddr->sin_addr.s_addr);
    /*
     * Now repeat the entire procedure to add a second socket to the queue of connections
     * waiting to be accepted - this time we use a different foreign port number
     */
    syn = create_syn(inet_addr("10.0.2.21"), inet_addr("10.0.2.20"), 1025, 30000, 100, 8192, 800);
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn);
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->syn);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(101 == ntohl(tcp_hdr->ack_no));
    /*
     * We should now have a second socket
     */
    ASSERT(new_socket == socket.so_queue_head);
    ASSERT(new_socket->next == socket.so_queue_tail);
    /*
     * with foreign address 10.0.2.21:1025
     */
    saddr = (struct sockaddr_in*) &new_socket->next->faddr;
    ASSERT(saddr->sin_addr.s_addr == inet_addr("10.0.2.21"));
    ASSERT(saddr->sin_family == AF_INET);
    ASSERT(saddr->sin_port == ntohs(1025));
    return 0;
}

/*
 * Testcase 85: bind a socket to a fully qualified address, then listen on it. Simulate that the maximum connection backlog
 * is reached
 *
 */
int testcase85() {
    socket_t socket;
    socket_t* new_socket;
    struct sockaddr_in laddr;
    struct sockaddr_in* saddr;
    net_msg_t* syn;
    net_msg_t* ack;
    tcp_hdr_t* tcp_hdr;
    u16* mss;
    u32 syn_ack_seq_no;
    tcp_init();
    mtu = 1500;
    tcp_create_socket(&socket, AF_INET, 0);
    laddr.sin_family = AF_INET;
    laddr.sin_port = htons(30000);
    laddr.sin_addr.s_addr = inet_addr("10.0.2.20");
    ASSERT(0 == socket.ops->bind(&socket, (struct sockaddr*) &laddr, sizeof(struct sockaddr_in)));
    /*
     * Now listen on socket, but set backlog to 2
     */
    socket.max_connection_backlog = 2;
    socket.ops->listen(&socket);
    ASSERT(socket.proto.tcp.status == TCP_STATUS_LISTEN);
    /*
     * Simulate receipt of SYN
     */
    syn = create_syn(inet_addr("10.0.2.21"), inet_addr("10.0.2.20"), 1024, 30000, 100, 8192, 800);
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn);
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->syn);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(101 == ntohl(tcp_hdr->ack_no));
    new_socket = socket.so_queue_head;
    ASSERT(new_socket);
    ASSERT(new_socket->proto.tcp.status == TCP_STATUS_SYN_RCVD);
    /*
     * Verify that our SYN_ACK contains the MSS 1460
     */
    ASSERT(6 == tcp_hdr->hlength);
    ASSERT(*(payload + sizeof(tcp_hdr_t)) == 2);
    ASSERT(*(payload + sizeof(tcp_hdr_t) + 1) == 4);
    mss = (u16*)(payload + sizeof(tcp_hdr_t) + 2);
    ASSERT(ntohs(*mss) == 1460);
    /*
     * Extract sequence number for later use
     */
    syn_ack_seq_no = ntohl(tcp_hdr->seq_no);
    /*
     * and that our own SMSS has been set to 800
     */
    ASSERT(new_socket->proto.tcp.smss == 800);
    /*
     * Check that local address of the socket has been updated
     */
    saddr = (struct sockaddr_in*) &new_socket->laddr;
    ASSERT(saddr->sin_addr.s_addr == inet_addr("10.0.2.20"));
    ASSERT(saddr->sin_family == AF_INET);
    ASSERT(saddr->sin_port == ntohs(30000));
    /*
     * but the local address of the listening socket remains
     * unchanged
     */
    saddr = (struct sockaddr_in*) &socket.laddr;
    ASSERT(inet_addr("10.0.2.20") == saddr->sin_addr.s_addr);
    /*
     * Now repeat the entire procedure to add a second socket to the queue of connections
     * waiting to be accepted - this time we use a different foreign port number
     */
    syn = create_syn(inet_addr("10.0.2.21"), inet_addr("10.0.2.20"), 1025, 30000, 100, 8192, 800);
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn);
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->syn);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(101 == ntohl(tcp_hdr->ack_no));
    /*
     * We should now have a second socket
     */
    ASSERT(new_socket == socket.so_queue_head);
    ASSERT(new_socket->next == socket.so_queue_tail);
    /*
     * with foreign address 10.0.2.21:1025
     */
    saddr = (struct sockaddr_in*) &new_socket->next->faddr;
    ASSERT(saddr->sin_addr.s_addr == inet_addr("10.0.2.21"));
    ASSERT(saddr->sin_family == AF_INET);
    ASSERT(saddr->sin_port == ntohs(1025));
    /*
     * Now try a third connection attempt - should be ignored
     */
    syn = create_syn(inet_addr("10.0.2.21"), inet_addr("10.0.2.20"), 1026, 30000, 100, 8192, 800);
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn);
    ASSERT(0 == ip_tx_msg_called);
    ASSERT(new_socket == socket.so_queue_head);
    ASSERT(new_socket->next == socket.so_queue_tail);
    return 0;
}

/*
 * Testcase 86: bind a socket, then listen on it. Simulate receipt of a RST and verify that it is ignored
 */
int testcase86() {
    socket_t socket;
    socket_t* new_socket;
    struct sockaddr_in laddr;
    struct sockaddr_in* saddr;
    net_msg_t* rst;
    tcp_hdr_t* tcp_hdr;
    u16* mss;
    tcp_init();
    mtu = 1500;
    tcp_create_socket(&socket, AF_INET, 0);
    laddr.sin_family = AF_INET;
    laddr.sin_port = htons(30000);
    laddr.sin_addr.s_addr = inet_addr("10.0.2.20");
    ASSERT(0 == socket.ops->bind(&socket, (struct sockaddr*) &laddr, sizeof(struct sockaddr_in)));
    /*
     * Now listen on socket
     */
    socket.max_connection_backlog = 15;
    socket.ops->listen(&socket);
    ASSERT(socket.proto.tcp.status == TCP_STATUS_LISTEN);
    /*
     * Simulate receipt of RST
     */
    rst = create_rst(inet_addr("10.0.2.21"), inet_addr("10.0.2.20"), 1024, 30000, 100, 1000);
    ip_tx_msg_called = 0;
    tcp_rx_msg(rst);
    ASSERT(0 == ip_tx_msg_called);
    return 0;
}

/*
 * Testcase 87: bind a socket, then listen on it. Simulate receipt of an ACK and verify that a RST is sent in response
 */
int testcase87() {
    socket_t socket;
    socket_t* new_socket;
    struct sockaddr_in laddr;
    struct sockaddr_in* saddr;
    net_msg_t* ack;
    tcp_hdr_t* tcp_hdr;
    u16* mss;
    tcp_init();
    mtu = 1500;
    tcp_create_socket(&socket, AF_INET, 0);
    laddr.sin_family = AF_INET;
    laddr.sin_port = htons(30000);
    laddr.sin_addr.s_addr = inet_addr("10.0.2.20");
    ASSERT(0 == socket.ops->bind(&socket, (struct sockaddr*) &laddr, sizeof(struct sockaddr_in)));
    /*
     * Now listen on socket
     */
    socket.max_connection_backlog = 15;
    socket.ops->listen(&socket);
    ASSERT(socket.proto.tcp.status == TCP_STATUS_LISTEN);
    /*
     * Simulate receipt of ACK
     */
    ack = create_text(inet_addr("10.0.2.21"), inet_addr("10.0.2.20"), 1024, 30000, 100, 1, 8192, 0, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(ack);
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->syn);
    ASSERT(1 == ntohl(tcp_hdr->seq_no));
    return 0;
}

/*
 * Testcase 88:
 * Receive a SYN-ACK for a socket in state SYN-SENT for which SEG.ACK_NO <= ISS and verify that a RST is sent
 */
int testcase88() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32 syn_seq_no;
    net_msg_t* syn_ack;
    tcp_hdr_t* tcp_hdr;
    /*
     * Do basic initialization of socket
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    ASSERT(0 ==  ((struct sockaddr_in*) &socket->laddr)->sin_addr.s_addr);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * and that the local IP address of the socket has been set
     */
    ASSERT(inet_addr("10.0.2.20") ==  ((struct sockaddr_in*) &socket->laddr)->sin_addr.s_addr);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1, with wrong ACK_NO
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no, 2048);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    __net_loglevel = 0;
    /*
     * Now validate that status of socket is still SYN_SENT
     */
    ASSERT(TCP_STATUS_SYN_SENT == socket->proto.tcp.status);
    /*
     * and that a RST has been sent
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Check that
     * 1) the sequence number of the RST is the ACK_NO from the invalid segment
     * 3) the RST has the ACK flag not set
     */
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->ack);
    ASSERT(syn_seq_no == ntohl(tcp_hdr->seq_no));
    /*
     * Assert that the connected flag in the socket is not set
     */
    ASSERT(socket->connected == 0);
    return 0;
}

/*
 * Testcase 89:
 * Receive a RST-ACK for a socket in state SYN-SENT for which SEG.ACK_NO <= ISS and verify that no RST is sent
 */
int testcase89() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32 syn_seq_no;
    net_msg_t* rst_ack;
    tcp_hdr_t* tcp_hdr;
    /*
     * Do basic initialization of socket
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    ASSERT(0 ==  ((struct sockaddr_in*) &socket->laddr)->sin_addr.s_addr);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * and that the local IP address of the socket has been set
     */
    ASSERT(inet_addr("10.0.2.20") ==  ((struct sockaddr_in*) &socket->laddr)->sin_addr.s_addr);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a RST-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1, with wrong ACK_NO
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    rst_ack = create_rst_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(rst_ack);
    __net_loglevel = 0;
    /*
     * Now validate that status of socket is still SYN_SENT
     */
    ASSERT(TCP_STATUS_SYN_SENT == socket->proto.tcp.status);
    /*
     * and that no RST has been sent
     */
    ASSERT(0 == ip_tx_msg_called);
    return 0;
}

/*
 * Testcase 90:
 * Receive a SYN-ACK for a socket in state SYN-SENT for which SEG.ACK_NO > ISS + 1 and verify that a RST is sent
 */
int testcase90() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32 syn_seq_no;
    net_msg_t* syn_ack;
    tcp_hdr_t* tcp_hdr;
    /*
     * Do basic initialization of socket
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    ASSERT(0 ==  ((struct sockaddr_in*) &socket->laddr)->sin_addr.s_addr);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * and that the local IP address of the socket has been set
     */
    ASSERT(inet_addr("10.0.2.20") ==  ((struct sockaddr_in*) &socket->laddr)->sin_addr.s_addr);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1, with wrong ACK_NO
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 2, 2048);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    __net_loglevel = 0;
    /*
     * Now validate that status of socket is still SYN_SENT
     */
    ASSERT(TCP_STATUS_SYN_SENT == socket->proto.tcp.status);
    /*
     * and that a RST has been sent
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Check that
     * 1) the sequence number of the RST is the ACK_NO from the invalid segment
     * 3) the RST has the ACK flag not set
     */
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->ack);
    ASSERT(syn_seq_no + 2 == ntohl(tcp_hdr->seq_no));
    /*
     * Assert that the connected flag in the socket is not set
     */
    ASSERT(socket->connected == 0);
    return 0;
}

/*
 * Testcase 91:
 * Receive a SYN while being in state SYN_SENT - simultaneous open. Verify that a SYN-ACK is sent in response and
 * the socket moves to state SYN_RECEIVED
 */
int testcase91() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32 syn_seq_no;
    u32* ack_seq_no;
    u32* ack_ack_no;
    net_msg_t* syn;
    u8* options;
    /*
     * Do basic initialization of socket
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    ASSERT(0 ==  ((struct sockaddr_in*) &socket->laddr)->sin_addr.s_addr);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    mtu = 1500;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * and that the local IP address of the socket has been set
     */
    ASSERT(inet_addr("10.0.2.20") ==  ((struct sockaddr_in*) &socket->laddr)->sin_addr.s_addr);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN message from 10.0.2.21:30000 to our local port, using seq_no 1
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn = create_syn(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, 8192, 1460);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn);
    __net_loglevel = 0;
    /*
     * Now validate that status of socket is SYN_RECEIVED
     */
    ASSERT(TCP_STATUS_SYN_RCVD == socket->proto.tcp.status);
    /*
     * and that a SYN-ACK has been sent
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Check that
     * 1) the sequence number of the SYN-ACK is the sequence number of the syn
     * 2) the acknowledgement number of the ACK is the sequence number of the SYN + 1, i.e. 2
     * 3) the message has the ACK flag set and SYN set
     * 4) the TCP checksum is correct
     * 5) IP source and IP destination are correct
     * 6) segment contains MSS option
     */
    tcp_hdr_t* tcp_hdr = (tcp_hdr_t*) payload;
    ack_seq_no = (u32*) (payload + 4);
    ack_ack_no = (u32*) (payload + 8);
    ctrl_flags = *((u8*)(payload + 13));
    ASSERT(ntohl(*ack_ack_no) == 2);
    ASSERT(ntohl(*ack_seq_no) == (syn_seq_no));
    ASSERT(ctrl_flags == ((1 << 4) | (1 << 1)));
    ASSERT(6 == tcp_hdr->hlength);
    ASSERT(0 == validate_tcp_checksum(24, (u16*) payload, ip_src, ip_dst));
    ASSERT(ip_src == 0x1402000a);
    ASSERT(ip_dst == 0x1502000a);
    options = (u8*)(payload + 20);
    ASSERT(2 == options[0]);
    ASSERT(4 == options[1]);
    ASSERT(1460 == options[2]*256 + options[3]);
    /*
     * Assert that the connected flag in the socket is not set
     */
    ASSERT(socket->connected == 0);
    ASSERT(socket->bound == 1);
    /*
     * Verify RCV_NXT and SND_UNA
     */
    ASSERT(2 == socket->proto.tcp.rcv_nxt);
    ASSERT(syn_seq_no == socket->proto.tcp.snd_una);
    return 0;
}

/*
 * Testcase 92:
 * Receive a SYN while being in state SYN_SENT - simultaneous open. Verify that a SYN-ACK is sent in response and
 * the socket moves to state SYN_RECEIVED. Then sent an ACK and verify that socket goes to ESTABLISHED
 */
int testcase92() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32 syn_seq_no;
    u32* ack_seq_no;
    u32* ack_ack_no;
    net_msg_t* syn;
    net_msg_t* ack;
    u8* options;
    /*
     * Do basic initialization of socket
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    ASSERT(0 ==  ((struct sockaddr_in*) &socket->laddr)->sin_addr.s_addr);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    mtu = 1500;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * and that the local IP address of the socket has been set
     */
    ASSERT(inet_addr("10.0.2.20") ==  ((struct sockaddr_in*) &socket->laddr)->sin_addr.s_addr);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN message from 10.0.2.21:30000 to our local port, using seq_no 1
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn = create_syn(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, 8192, 1460);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn);
    /*
     * Now validate that status of socket is SYN_RECEIVED
     */
    ASSERT(TCP_STATUS_SYN_RCVD == socket->proto.tcp.status);
    /*
     * and that a SYN-ACK has been sent
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Check that
     * 1) the sequence number of the SYN-ACK is the sequence number of the syn
     * 2) the acknowledgement number of the ACK is the sequence number of the SYN + 1, i.e. 2
     * 3) the message has the ACK flag set and SYN set
     * 4) the TCP checksum is correct
     * 5) IP source and IP destination are correct
     * 6) segment contains MSS option
     */
    tcp_hdr_t* tcp_hdr = (tcp_hdr_t*) payload;
    ack_seq_no = (u32*) (payload + 4);
    ack_ack_no = (u32*) (payload + 8);
    ctrl_flags = *((u8*)(payload + 13));
    ASSERT(ntohl(*ack_ack_no) == 2);
    ASSERT(ntohl(*ack_seq_no) == (syn_seq_no));
    ASSERT(ctrl_flags == ((1 << 4) | (1 << 1)));
    ASSERT(6 == tcp_hdr->hlength);
    ASSERT(0 == validate_tcp_checksum(24, (u16*) payload, ip_src, ip_dst));
    ASSERT(ip_src == 0x1402000a);
    ASSERT(ip_dst == 0x1502000a);
    options = (u8*)(payload + 20);
    ASSERT(2 == options[0]);
    ASSERT(4 == options[1]);
    ASSERT(1460 == options[2]*256 + options[3]);
    /*
     * Assert that the connected flag in the socket is not set
     */
    ASSERT(socket->connected == 0);
    ASSERT(socket->bound == 1);
    /*
     * Verify RCV_NXT and SND_UNA
     */
    ASSERT(2 == socket->proto.tcp.rcv_nxt);
    ASSERT(syn_seq_no == socket->proto.tcp.snd_una);
    /*
     * Now simulate receipt of ACK for the SYN
     */
    ack = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1, 8192, 0, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(ack);
    ASSERT(0 == ip_tx_msg_called);
    ASSERT(1 == socket->connected);
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    return 0;
}

/*
 * Testcase 93:
 * Create a socket and establish a connection. Then simulate receipt of a single segment containing 128 bytes of data,
 * but no ACK -> segment should be dropped
 *
 */
int testcase93() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    net_msg_t* text;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    int i;
    unsigned char buffer[8192];
    /*
     * Fill buffer
     */
    for (i = 0; i < 128; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window 600
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 600);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that the window size has been updated
     */
    ASSERT(600 == socket->proto.tcp.snd_wnd);
    /*
     * Put together incoming data segment without an ACK
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, 0, 600, buffer, 128);
    ip_tx_msg_called = 0;
    tcp_rx_msg(text);
    /*
     * Verify that no immediate response is sent - the segment has been dropped
     */
    ASSERT(0 == ip_tx_msg_called);
    /*
     * and no data has been added to the receive queue
     */
    ASSERT(0 == socket->proto.tcp.rcv_buffer_head);
    ASSERT(0 == socket->proto.tcp.rcv_buffer_tail);
    return 0;
}


/*
 * Testcase 94: bind a socket to a fully qualified address, then listen on it. Verify that the status of the socket
 * changes to LISTEN. Simulate receipt of a SYN to create a new socket in state SYN_RCVD. Then send an ACK to this socket
 * which has an unexpected ACK no
 *
 */
int testcase94() {
    socket_t socket;
    socket_t* new_socket;
    struct sockaddr_in laddr;
    struct sockaddr_in* saddr;
    net_msg_t* syn;
    net_msg_t* ack;
    tcp_hdr_t* tcp_hdr;
    u16* mss;
    u32 syn_ack_seq_no;
    tcp_init();
    mtu = 1500;
    tcp_create_socket(&socket, AF_INET, 0);
    laddr.sin_family = AF_INET;
    laddr.sin_port = htons(30000);
    laddr.sin_addr.s_addr = inet_addr("10.0.2.20");
    ASSERT(0 == socket.ops->bind(&socket, (struct sockaddr*) &laddr, sizeof(struct sockaddr_in)));
    /*
     * Now listen on socket
     */
    socket.max_connection_backlog = 15;
    socket.ops->listen(&socket);
    ASSERT(socket.proto.tcp.status == TCP_STATUS_LISTEN);
    /*
     * Simulate receipt of SYN
     */
    syn = create_syn(inet_addr("10.0.2.21"), inet_addr("10.0.2.20"), 1024, 30000, 100, 8192, 800);
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn);
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->syn);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(101 == ntohl(tcp_hdr->ack_no));
    new_socket = socket.so_queue_head;
    ASSERT(new_socket);
    ASSERT(new_socket->proto.tcp.status == TCP_STATUS_SYN_RCVD);
    /*
     * Verify that our SYN_ACK contains the MSS 1460
     */
    ASSERT(6 == tcp_hdr->hlength);
    ASSERT(*(payload + sizeof(tcp_hdr_t)) == 2);
    ASSERT(*(payload + sizeof(tcp_hdr_t) + 1) == 4);
    mss = (u16*)(payload + sizeof(tcp_hdr_t) + 2);
    ASSERT(ntohs(*mss) == 1460);
    /*
     * Extract sequence number for later use
     */
    syn_ack_seq_no = ntohl(tcp_hdr->seq_no);
    /*
     * and that our own SMSS has been set to 800
     */
    ASSERT(new_socket->proto.tcp.smss == 800);
    /*
     * Check that local address of the socket has been updated
     */
    saddr = (struct sockaddr_in*) &new_socket->laddr;
    ASSERT(saddr->sin_addr.s_addr == inet_addr("10.0.2.20"));
    ASSERT(saddr->sin_family == AF_INET);
    ASSERT(saddr->sin_port == ntohs(30000));
    /*
     * but the local address of the listening socket remains
     * unchanged
     */
    saddr = (struct sockaddr_in*) &socket.laddr;
    ASSERT(inet_addr("10.0.2.20") == saddr->sin_addr.s_addr);
    /*
     * Now send an ACK to the newly created socket, but use a wrong ACK no
     */
    ack = create_text(inet_addr("10.0.2.21"), inet_addr("10.0.2.20"), 1024, 30000, 101, syn_ack_seq_no, 8192, 0, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(ack);
    /*
     * Verify that we have sent a RST
     */
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == ip_tx_msg_called);
    ASSERT(syn_ack_seq_no == ntohl(tcp_hdr->seq_no));
    ASSERT(1 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->ack);
    return 0;
}

/*
 * Testcase 95:
 * Timeout during passive connect
 *
 * For a SYN_ACK, we have a timeout of 15 seconds.
 * As we try five retransmissions in total, we spent about
 * 15 + 30 + 60 + 120 + 240 + 480 = 945
 * seconds in total with the connection attempt and time out.
 */
int testcase95() {
    struct sockaddr_in in;
    struct sockaddr_in laddr;
    int i;
    tcp_hdr_t* tcp_hdr;
    int attempt;
    int j;
    int ticks = 0;
    net_msg_t* syn;
    unsigned int syn_ack_seq_no;
    unsigned int syn_ack_ack_no;
    /*
     * Do basic initialization of socket
     */
    tcp_init();
    socket_t* socket;
    socket_t* new_socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    cond_broadcast_called = 0;
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    ASSERT(socket->proto.tcp.ref_count == 2);
    laddr.sin_family = AF_INET;
    /*
     * bind socket to local address
     */
    laddr.sin_port = htons(30000);
    laddr.sin_addr.s_addr = inet_addr("10.0.2.20");
    ASSERT(0 == socket->ops->bind(socket, (struct sockaddr*) &laddr, sizeof(struct sockaddr_in)));
    /*
     * Listen on socket
     */
    ip_tx_msg_called = 0;
    socket->max_connection_backlog = 15;
    ASSERT(0 == socket->ops->listen(socket));
    /*
     * and simulate receipt of a syn
     */
    syn = create_syn(inet_addr("10.0.2.21"), inet_addr("10.0.2.20"), 1024, 30000, 100, 8192, 800);
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn);
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->syn);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(101 == ntohl(tcp_hdr->ack_no));
    new_socket = socket->so_queue_head;
    ASSERT(new_socket);
    ASSERT(new_socket->proto.tcp.status == TCP_STATUS_SYN_RCVD);
    /*
     * This should have produced a SYN_ACK
     */
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->syn);
    ASSERT(1 == tcp_hdr->ack);
    /*
     * Remember SEQ_NO and ACK_NO for later
     */
    syn_ack_seq_no = ntohl(tcp_hdr->seq_no);
    syn_ack_ack_no = ntohl(tcp_hdr->ack_no);
    /*
     * To be able to validate the new socket after the timeout which drops the socket
     * we increase its reference count by one now
     */
    new_socket->proto.tcp.ref_count++;
    /*
     * As the RTO is 15 seconds for a SYN, simulate 15*(HZ / TCP_HZ) ticks
     */
    ASSERT(new_socket->proto.tcp.rtx_timer.time == 15*TCP_HZ);
    ip_tx_msg_called = 0;
    for (i = 0; i < 15*TCP_HZ - 1; i++) {
        ticks++;
        tcp_do_tick();
        ASSERT(0 == ip_tx_msg_called);
    }
    tcp_do_tick();
    ticks++;
    attempt = 1;
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(1 == tcp_hdr->syn);
    ASSERT(syn_ack_seq_no == ntohl(tcp_hdr->seq_no));
    ASSERT(syn_ack_ack_no == ntohl(tcp_hdr->ack_no));
    /*
     * Now repeat this - timer should double with each attempt
     */
    for (j = 0; j < 5; j++) {
        ip_tx_msg_called = 0;
        for (i = 0; i < (15*TCP_HZ << attempt) - 1; i++) {
            tcp_do_tick();
            ticks++;
            ASSERT(0 == ip_tx_msg_called);
        }
        ticks++;
        tcp_do_tick();
        attempt++;
        ASSERT(1 == ip_tx_msg_called);
        ASSERT(1 == tcp_hdr->ack);
        if (4 == j) {
            /*
             * Last attempt should result in a reset
             */
            ASSERT(1 == tcp_hdr->rst);
            ASSERT(0 == tcp_hdr->syn);
            ASSERT(-137 == new_socket->error);
            ASSERT(cond_broadcast_called);
        }
        else {
            /*
             * Retransmit SYN_ACK
             */
            ASSERT(0 == tcp_hdr->rst);
            ASSERT(1 == tcp_hdr->syn);
            ASSERT(1 == tcp_hdr->ack);
        }
    }
    return 0;
}


/*
 * Testcase 96:
 *
 * Receive a FIN for a socket in state ESTABLISHED and verify that state CLOSE_WAIT is entered
 *
 */
int testcase96() {
    socket_t* socket;
    socket_t* new_socket;
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    tcp_hdr_t* tcp_hdr;
    u32 syn_seq_no = 0;
    u32 rcv_nxt;
    net_msg_t* fin;
    unsigned char buffer[256];
    u16* mss;
    tcp_init();
    mtu = 1500;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    tcp_create_socket(socket, AF_INET, 0);
    ip_tx_msg_called = 0;
    in.sin_family = AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * establish connection by receiving a SYN_ACK
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(inet_addr("10.0.2.21"), inet_addr("10.0.2.20"), 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 8192);
    tcp_rx_msg(syn_ack);
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and simulate receipt of FIN
     */
    rcv_nxt = socket->proto.tcp.rcv_nxt;
    fin = create_fin_ack(inet_addr("10.0.2.21"), inet_addr("10.0.2.20"), 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1, 8192);
    ip_tx_msg_called = 0;
    tcp_rx_msg(fin);
    ASSERT(TCP_STATUS_CLOSE_WAIT == socket->proto.tcp.status);
    /*
     * RCV_NXT should have been advanced by one
     */
    ASSERT(rcv_nxt + 1 == socket->proto.tcp.rcv_nxt);
    /*
     * and we should have sent an ACK for the FIN
     */
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->fin);
    ASSERT(0 == tcp_hdr->syn);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(3 == ntohl(tcp_hdr->ack_no));
    ASSERT(syn_seq_no + 1 == ntohl(tcp_hdr->seq_no));
    /*
     * A further read should return 0
     */
    ASSERT(0 == socket->ops->recv(socket, buffer, 10, 0));
    /*
     * Sending another FIN should not change the state
     */
    fin = create_fin_ack(inet_addr("10.0.2.21"), inet_addr("10.0.2.20"), 30000, ntohs(in_ptr->sin_port), 3, syn_seq_no + 1, 8192);
    ip_tx_msg_called = 0;
    tcp_rx_msg(fin);
    ASSERT(TCP_STATUS_CLOSE_WAIT == socket->proto.tcp.status);
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->fin);
    ASSERT(0 == tcp_hdr->syn);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(3 == ntohl(tcp_hdr->ack_no));
    ASSERT(syn_seq_no + 1 == ntohl(tcp_hdr->seq_no));
    return 0;
}

/*
 * Testcase 97:
 *
 * Receive a FIN for a socket in state SYN_RCVD and verify that state CLOSE_WAIT is entered
 *
 */
int testcase97() {
    socket_t* socket;
    socket_t* new_socket;
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    tcp_hdr_t* tcp_hdr;
    u32 syn_seq_no = 0;
    u32 rcv_nxt;
    net_msg_t* fin;
    unsigned char buffer[256];
    u16* mss;
    tcp_init();
    mtu = 1500;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    tcp_create_socket(socket, AF_INET, 0);
    ip_tx_msg_called = 0;
    in.sin_family = AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = inet_addr("10.0.2.20");
    /*
     * Bind to local address
     */
    ASSERT(0 == socket->ops->bind(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and listen
     */
    socket->max_connection_backlog = 15;
    ASSERT(0 == socket->ops->listen(socket));
    /*
     * Now simulate receipt of SYN caused by a connection attempt from port 1024
     */
    syn = create_syn(inet_addr("10.0.2.21"), inet_addr("10.0.2.20"), 1024, 30000, 1, 8192, 1460);
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn);
    /*
     * and verify that we are now in state SYN-RECEIVED
     */
    new_socket = socket->so_queue_head;
    ASSERT(new_socket);
    ASSERT(TCP_STATUS_SYN_RCVD == new_socket->proto.tcp.status);
    /*
     * Extract sequence number from the SYN_ACK
     */
    tcp_hdr = (tcp_hdr_t*) payload;
    syn_seq_no = ntohl(tcp_hdr->seq_no);
    /*
     * Now simulate receipt of FIN
     */
    rcv_nxt = new_socket->proto.tcp.rcv_nxt;
    fin = create_fin_ack(inet_addr("10.0.2.21"), inet_addr("10.0.2.20"), 1024, 30000, 2, syn_seq_no + 1, 8192);
    ip_tx_msg_called = 0;
    tcp_rx_msg(fin);
    ASSERT(TCP_STATUS_CLOSE_WAIT == new_socket->proto.tcp.status);
    /*
     * RCV_NXT should have been advanced by one
     */
    ASSERT(rcv_nxt + 1 == new_socket->proto.tcp.rcv_nxt);
    /*
     * and we should have sent an ACK for the FIN
     */
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->fin);
    ASSERT(0 == tcp_hdr->syn);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(3 == ntohl(tcp_hdr->ack_no));
    ASSERT(syn_seq_no + 1 == ntohl(tcp_hdr->seq_no));
    /*
     * A further read should return 0
     */
    ASSERT(0 == new_socket->ops->recv(new_socket, buffer, 10, 0));
    return 0;
}

/*
 * Testcase 98:
 *
 * Close a socket in state ESTABLISHED and verify that FIN is sent and the socket moves to state FIN_WAIT_1
 *
 */
int testcase98() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    net_msg_t* text;
    u32 snd_nxt;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window  536
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 536);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * Call tcp_close
     */
    snd_nxt = socket->proto.tcp.snd_nxt;
    ip_tx_msg_called = 0;
    socket->ops->close(socket, 0);
    /*
     * and verify that a FIN has been sent
     */
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->fin);
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->syn);
    ASSERT(TCP_STATUS_FIN_WAIT_1 == socket->proto.tcp.status);
    /*
     * SND_NXT and SND_MAX should have been increased
     */
    ASSERT(snd_nxt + 1 == socket->proto.tcp.snd_nxt);
    return 0;
}

/*
 * Testcase 99:
 *
 * Close a socket in state ESTABLISHED and verify that FIN is sent and the socket moves to state FIN_WAIT_1. Then
 * simulate the case that the FIN times out
 *
 */
int testcase99() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    net_msg_t* text;
    u32 fin_seq_no;
    int rto;
    int i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window  536
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 536);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * Call tcp_close
     */
    ip_tx_msg_called = 0;
    socket->ops->close(socket, 0);
    /*
     * and verify that a FIN has been sent
     */
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->fin);
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->syn);
    ASSERT(TCP_STATUS_FIN_WAIT_1 == socket->proto.tcp.status);
    /*
     * Save FIN sequence number
     */
    fin_seq_no = ntohl(tcp_hdr->seq_no);
    /*
     * Now simulate RTO timer ticks
     */
    rto = socket->proto.tcp.rtx_timer.time;
    ASSERT(rto);
    for (i = 0; i < rto; i++) {
        ip_tx_msg_called = 0;
        tcp_do_tick();
        if (i + 1 == rto) {
            ASSERT(1 == ip_tx_msg_called);
        }
        else
            ASSERT(0 == ip_tx_msg_called);
    }
    /*
     * Verify that a FIN has been sent again with the same sequence number
     */
    ASSERT(1 == tcp_hdr->fin);
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->syn);
    ASSERT(fin_seq_no == ntohl(tcp_hdr->seq_no));
    /*
     * and socket state is unchanged
     */
    ASSERT(TCP_STATUS_FIN_WAIT_1 == socket->proto.tcp.status);
    return 0;
}

/*
 * Testcase 100:
 *
 * Receive a FIN for a socket in state ESTABLISHED and verify that state CLOSE_WAIT is entered. Then call close on socket
 * and verify that FIN is sent. Also verify that if FIN times out, it is sent again
 *
 */
int testcase100() {
    socket_t* socket;
    socket_t* new_socket;
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    tcp_hdr_t* tcp_hdr;
    u32 syn_seq_no = 0;
    u32 rcv_nxt;
    net_msg_t* fin;
    unsigned char buffer[256];
    u16* mss;
    int i;
    int timeout;
    tcp_init();
    mtu = 1500;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    tcp_create_socket(socket, AF_INET, 0);
    ip_tx_msg_called = 0;
    in.sin_family = AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * establish connection by receiving a SYN_ACK
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(inet_addr("10.0.2.21"), inet_addr("10.0.2.20"), 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 8192);
    tcp_rx_msg(syn_ack);
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and simulate receipt of FIN
     */
    rcv_nxt = socket->proto.tcp.rcv_nxt;
    fin = create_fin_ack(inet_addr("10.0.2.21"), inet_addr("10.0.2.20"), 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1, 8192);
    ip_tx_msg_called = 0;
    tcp_rx_msg(fin);
    ASSERT(TCP_STATUS_CLOSE_WAIT == socket->proto.tcp.status);
    /*
     * RCV_NXT should have been advanced by one
     */
    ASSERT(rcv_nxt + 1 == socket->proto.tcp.rcv_nxt);
    /*
     * and we should have sent an ACK for the FIN
     */
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->fin);
    ASSERT(0 == tcp_hdr->syn);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(3 == ntohl(tcp_hdr->ack_no));
    ASSERT(syn_seq_no + 1 == ntohl(tcp_hdr->seq_no));
    /*
     * Close socket
     */
    ip_tx_msg_called = 0;
    socket->ops->close(socket, 0);
    ASSERT(1 == ip_tx_msg_called);
    /*
     * This should be a FIN
     */
    ASSERT(1 == tcp_hdr->fin);
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->syn);
    /*
     * with sequence number syn_seq_no + 2
     */
    ASSERT(syn_seq_no + 1 == ntohl(tcp_hdr->seq_no));
    /*
     * Socket should be in state LAST_ACK now
     */
    ASSERT(TCP_STATUS_LAST_ACK == socket->proto.tcp.status);
    /*
     * Now simulate the case the we time out
     */
    timeout = socket->proto.tcp.rtx_timer.time;
    ASSERT(timeout);
    for (i = 0; i < timeout; i++) {
        ip_tx_msg_called = 0;
        tcp_do_tick();
        if (i < timeout -1) {
            ASSERT(0 == ip_tx_msg_called);
        }
        else {
            ASSERT(1 == ip_tx_msg_called);
        }
    }
    /*
     * Should have sent FIN again
     */
    ASSERT(1 == tcp_hdr->fin);
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->syn);
    /*
     * with the same sequence number
     */
    ASSERT(syn_seq_no + 1 == ntohl(tcp_hdr->seq_no));
    /*
     * Socket should still be in state LAST_ACK
     */
    ASSERT(TCP_STATUS_LAST_ACK == socket->proto.tcp.status);
    return 0;
}

/*
 * Testcase 101:
 *
 * Receive a FIN for a socket in state ESTABLISHED and verify that state CLOSE_WAIT is entered. Then call close on socket
 * and verify that FIN is sent. Acknowledge FIN so that socket is closed
 *
 */
int testcase101() {
    socket_t* socket;
    socket_t* new_socket;
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    tcp_hdr_t* tcp_hdr;
    u32 syn_seq_no = 0;
    u32 rcv_nxt;
    net_msg_t* fin;
    net_msg_t* ack;
    unsigned char buffer[256];
    u16* mss;
    tcp_init();
    mtu = 1500;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    tcp_create_socket(socket, AF_INET, 0);
    ip_tx_msg_called = 0;
    in.sin_family = AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * establish connection by receiving a SYN_ACK
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(inet_addr("10.0.2.21"), inet_addr("10.0.2.20"), 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 8192);
    tcp_rx_msg(syn_ack);
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and simulate receipt of FIN
     */
    rcv_nxt = socket->proto.tcp.rcv_nxt;
    fin = create_fin_ack(inet_addr("10.0.2.21"), inet_addr("10.0.2.20"), 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1, 8192);
    ip_tx_msg_called = 0;
    tcp_rx_msg(fin);
    ASSERT(TCP_STATUS_CLOSE_WAIT == socket->proto.tcp.status);
    /*
     * RCV_NXT should have been advanced by one
     */
    ASSERT(rcv_nxt + 1 == socket->proto.tcp.rcv_nxt);
    /*
     * and we should have sent an ACK for the FIN
     */
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->fin);
    ASSERT(0 == tcp_hdr->syn);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(3 == ntohl(tcp_hdr->ack_no));
    ASSERT(syn_seq_no + 1 == ntohl(tcp_hdr->seq_no));
    /*
     * Close socket
     */
    ip_tx_msg_called = 0;
    socket->ops->close(socket, 0);
    ASSERT(1 == ip_tx_msg_called);
    /*
     * This should be a FIN
     */
    ASSERT(1 == tcp_hdr->fin);
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->syn);
    /*
     * with sequence number syn_seq_no + 1
     */
    ASSERT(syn_seq_no + 1 == ntohl(tcp_hdr->seq_no));
    /*
     * Socket should be in state LAST_ACK now
     */
    ASSERT(TCP_STATUS_LAST_ACK == socket->proto.tcp.status);
    /*
     * Acknowledge FIN
     */
    ack = create_text(inet_addr("10.0.2.21"), inet_addr("10.0.2.20"), 30000, ntohs(in_ptr->sin_port), 3, syn_seq_no + 2, 8192, 0, 0);
    tcp_rx_msg(ack);
    __net_loglevel = 0;
    /*
     * Socket should have been gone now
     */
    do_putchar = 0;
    ASSERT(0 == tcp_print_sockets());
    do_putchar = 1;
    return 0;
}

/*
 * Testcase 102:
 *
 * Close a socket in state ESTABLISHED and verify that FIN is sent and the socket moves to state FIN_WAIT_1. Then process
 * ACK and move to FIN_WAIT_2
 *
 */
int testcase102() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    net_msg_t* ack;
    u32 snd_nxt;
    u32 snd_una;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window  536
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 536);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * Call tcp_close
     */
    snd_nxt = socket->proto.tcp.snd_nxt;
    ip_tx_msg_called = 0;
    socket->ops->close(socket, 0);
    /*
     * and verify that a FIN has been sent
     */
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->fin);
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->syn);
    ASSERT(TCP_STATUS_FIN_WAIT_1 == socket->proto.tcp.status);
    /*
     * SND_NXT and SND_MAX should have been increased
     */
    ASSERT(snd_nxt + 1 == socket->proto.tcp.snd_nxt);
    /*
     * Now ACK the FIN
     */
    ack = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 2, 8192, 0, 0);
    ip_tx_msg_called = 0;
    snd_una = socket->proto.tcp.snd_una;
    tcp_rx_msg(ack);
    ASSERT(0 == ip_tx_msg_called);
    ASSERT(TCP_STATUS_FIN_WAIT_2 == socket->proto.tcp.status);
    /*
     * Should have acknowledged one more byte
     */
    ASSERT(snd_una + 1 == socket->proto.tcp.snd_una);
    return 0;
}

/*
 * Testcase 103:
 *
 * Close a socket in state ESTABLISHED and verify that FIN is sent and the socket moves to state FIN_WAIT_1. Then process
 * a FIN-ACK and move to state TIME_WAIT
 *
 */
int testcase103() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    net_msg_t* fin_ack;
    u32 snd_nxt;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window  536
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 536);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * Call tcp_close
     */
    snd_nxt = socket->proto.tcp.snd_nxt;
    ip_tx_msg_called = 0;
    socket->ops->close(socket, 0);
    /*
     * and verify that a FIN has been sent
     */
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->fin);
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->syn);
    ASSERT(TCP_STATUS_FIN_WAIT_1 == socket->proto.tcp.status);
    /*
     * SND_NXT and SND_MAX should have been increased
     */
    ASSERT(snd_nxt + 1 == socket->proto.tcp.snd_nxt);
    /*
     * Now assemble a segment which acknowledges our FIN and contains itself a FIN
     */
    fin_ack = create_fin_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 2, 8192);
    ip_tx_msg_called = 0;
    tcp_rx_msg(fin_ack);
    /*
     * We should now be in TIME_WAIT
     */
    ASSERT(TCP_STATUS_TIME_WAIT == socket->proto.tcp.status);
    /*
     * and should have acknowledged the FIN
     */
    ASSERT(1 == ip_tx_msg_called);
    ASSERT(0 == tcp_hdr->fin);
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->syn);
    ASSERT(3 == ntohl(tcp_hdr->ack_no));
    /*
     * All timers should be turned off
     */
    ASSERT(0 == socket->proto.tcp.rtx_timer.time);
    ASSERT(0 == socket->proto.tcp.delack_timer.time);
    ASSERT(0 == socket->proto.tcp.persist_timer.time);
    /*
     * and time_wait timer should be set
     */
    ASSERT(2*TCP_MSL == socket->proto.tcp.time_wait_timer.time);
    return 0;
}

/*
 * Testcase 104:
 *
 * Move a socket into TIME_WAIT and simulate that the timer expires
 *
 */
int testcase104() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    net_msg_t* fin_ack;
    u32 snd_nxt;
    int i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window  536
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 536);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * Call tcp_close
     */
    snd_nxt = socket->proto.tcp.snd_nxt;
    ip_tx_msg_called = 0;
    /*
     * Simulate that FS layer / net layer closes the socket and
     * releases reference
     */
    socket->ops->close(socket, 0);
    socket->ops->release(socket);
    ASSERT(1 == socket->proto.tcp.ref_count);
    /*
     * and verify that a FIN has been sent
     */
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->fin);
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->syn);
    ASSERT(TCP_STATUS_FIN_WAIT_1 == socket->proto.tcp.status);
    /*
     * SND_NXT and SND_MAX should have been increased
     */
    ASSERT(snd_nxt + 1 == socket->proto.tcp.snd_nxt);
    /*
     * Now assemble a segment which acknowledges our FIN and contains itself a FIN
     */
    fin_ack = create_fin_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 2, 8192);
    ip_tx_msg_called = 0;
    tcp_rx_msg(fin_ack);
    /*
     * We should now be in TIME_WAIT
     */
    ASSERT(TCP_STATUS_TIME_WAIT == socket->proto.tcp.status);
    /*
     * and should have acknowledged the FIN
     */
    ASSERT(1 == ip_tx_msg_called);
    ASSERT(0 == tcp_hdr->fin);
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->syn);
    ASSERT(3 == ntohl(tcp_hdr->ack_no));
    /*
     * All timers should be turned off
     */
    ASSERT(0 == socket->proto.tcp.rtx_timer.time);
    ASSERT(0 == socket->proto.tcp.delack_timer.time);
    ASSERT(0 == socket->proto.tcp.persist_timer.time);
    /*
     * and time_wait timer should be set
     */
    ASSERT(2*TCP_MSL == socket->proto.tcp.time_wait_timer.time);
    /*
     * Simulate that timer expires - at this point, socket will be freed
     */
    for (i = 0; i < 2*TCP_MSL; i++)
        tcp_do_tick();
    do_putchar = 0;
    ASSERT(0 == tcp_print_sockets());
    return 0;
}

/*
 * Testcase 105:
 *
 * Close a socket in state ESTABLISHED and verify that FIN is sent and the socket moves to state FIN_WAIT_1. Then process
 * an FIN-ACK and move to state FIN_WAIT_2. Finally receive FIN so that we move to state TIME_WAIT
 *
 */
int testcase105() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    net_msg_t* fin_ack;
    u32 snd_nxt;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window  536
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 536);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * Call tcp_close
     */
    snd_nxt = socket->proto.tcp.snd_nxt;
    ip_tx_msg_called = 0;
    socket->ops->close(socket, 0);
    /*
     * and verify that a FIN has been sent
     */
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->fin);
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->syn);
    ASSERT(TCP_STATUS_FIN_WAIT_1 == socket->proto.tcp.status);
    /*
     * SND_NXT and SND_MAX should have been increased
     */
    ASSERT(snd_nxt + 1 == socket->proto.tcp.snd_nxt);
    /*
     * Now assemble a segment which acknowledges our FIN
     */
    fin_ack = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 2, 8192, 0, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(fin_ack);
    /*
     * We should now be in FIN_WAIT_2
     */
    ASSERT(TCP_STATUS_FIN_WAIT_2 == socket->proto.tcp.status);
    ASSERT(0 == ip_tx_msg_called);
    /*
     * Now receive FIN_ACK
     */
    fin_ack = create_fin_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 2, 8192);
    tcp_rx_msg(fin_ack);
    /*
     * We should have sent an ACK for the FIN
     */
    ASSERT(1 == ip_tx_msg_called);
    ASSERT(0 == tcp_hdr->fin);
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->syn);
    ASSERT(3 == ntohl(tcp_hdr->ack_no));
    /*
     * and moved to TIME_WAIT
     */
    ASSERT(TCP_STATUS_TIME_WAIT == socket->proto.tcp.status);
    /*
     * All timers should be turned off
     */
    ASSERT(0 == socket->proto.tcp.rtx_timer.time);
    ASSERT(0 == socket->proto.tcp.delack_timer.time);
    ASSERT(0 == socket->proto.tcp.persist_timer.time);
    /*
     * and time_wait timer should be set
     */
    ASSERT(2*TCP_MSL == socket->proto.tcp.time_wait_timer.time);
    return 0;
}



/*
 * Testcase 106:
 * Receive a RST-ACK for a socket in state SYN-SENT which is acceptable and verify that socket is dropped
 */
int testcase106() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32 syn_seq_no;
    net_msg_t* rst_ack;
    tcp_hdr_t* tcp_hdr;
    /*
     * Do basic initialization of socket
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    ASSERT(0 ==  ((struct sockaddr_in*) &socket->laddr)->sin_addr.s_addr);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * that we are in SYN_SENT
     */
    ASSERT(TCP_STATUS_SYN_SENT == socket->proto.tcp.status);
    /*
     * and that the local IP address of the socket has been set
     */
    ASSERT(inet_addr("10.0.2.20") ==  ((struct sockaddr_in*) &socket->laddr)->sin_addr.s_addr);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a RST-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1, with correct ACK_NO
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    rst_ack = create_rst_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1);
    ASSERT(rst_ack);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    do_putchar = 1;
    tcp_rx_msg(rst_ack);
    __net_loglevel = 0;
    /*
     * Now validate that socket has been dropped
     */
    do_putchar = 0;
    ASSERT(0 == tcp_print_sockets());
    /*
     * and that no RST has been sent
     */
    ASSERT(0 == ip_tx_msg_called);
    return 0;
}

/*
 * Testcase 107:
 *
 * Close a socket in state ESTABLISHED and verify that FIN is sent and the socket moves to state FIN_WAIT_1. Then process
 * a FIN-ACK and move to state TIME_WAIT. Simulate that after some time, the remote FIN is retransmitted
 *
 */
int testcase107() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    net_msg_t* fin_ack;
    u32 snd_nxt;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window  536
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 536);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * Call tcp_close
     */
    snd_nxt = socket->proto.tcp.snd_nxt;
    ip_tx_msg_called = 0;
    socket->ops->close(socket, 0);
    /*
     * and verify that a FIN has been sent
     */
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->fin);
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->syn);
    ASSERT(TCP_STATUS_FIN_WAIT_1 == socket->proto.tcp.status);
    /*
     * SND_NXT and SND_MAX should have been increased
     */
    ASSERT(snd_nxt + 1 == socket->proto.tcp.snd_nxt);
    /*
     * Now assemble a segment which acknowledges our FIN and contains itself a FIN
     */
    fin_ack = create_fin_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 2, 8192);
    ip_tx_msg_called = 0;
    tcp_rx_msg(fin_ack);
    /*
     * We should now be in TIME_WAIT
     */
    ASSERT(TCP_STATUS_TIME_WAIT == socket->proto.tcp.status);
    /*
     * and should have acknowledged the FIN
     */
    ASSERT(1 == ip_tx_msg_called);
    ASSERT(0 == tcp_hdr->fin);
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->syn);
    ASSERT(3 == ntohl(tcp_hdr->ack_no));
    /*
     * All timers should be turned off
     */
    ASSERT(0 == socket->proto.tcp.rtx_timer.time);
    ASSERT(0 == socket->proto.tcp.delack_timer.time);
    ASSERT(0 == socket->proto.tcp.persist_timer.time);
    /*
     * and time_wait timer should be set
     */
    ASSERT(2*TCP_MSL == socket->proto.tcp.time_wait_timer.time);
    /*
     * Simulate one tick
     */
    tcp_do_tick();
    ASSERT(2*TCP_MSL == socket->proto.tcp.time_wait_timer.time + 1);
    /*
     * and receive same FIN again
     */
    fin_ack = create_fin_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 2, 8192);
    ip_tx_msg_called = 0;
    do_putchar = 1;
    tcp_rx_msg(fin_ack);
    /*
     * Should still be in time_wait
     */
    ASSERT(TCP_STATUS_TIME_WAIT == socket->proto.tcp.status);
    /*
     * and should have acknowledged the FIN
     */
    ASSERT(1 == ip_tx_msg_called);
    ASSERT(0 == tcp_hdr->fin);
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->syn);
    ASSERT(3 == ntohl(tcp_hdr->ack_no));
    /*
     * All timers should be turned off
     */
    ASSERT(0 == socket->proto.tcp.rtx_timer.time);
    ASSERT(0 == socket->proto.tcp.delack_timer.time);
    ASSERT(0 == socket->proto.tcp.persist_timer.time);
    /*
     * and time_wait timer should have been reset to 2*MSL
     */
    ASSERT(2*TCP_MSL == socket->proto.tcp.time_wait_timer.time);
    return 0;
}

/*
 * Testcase 108:
 *
 * Close a socket in state ESTABLISHED and verify that FIN is sent and the socket moves to state FIN_WAIT_1. Then receive
 * a text segment while socket->eof is not set. Verify that the segment is processed and added to the receive buffer
 *
 */
int testcase108() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    net_msg_t* text;
    net_msg_t* fin_ack;
    u32 snd_nxt;
    u8 buffer[256];
    int i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window  536
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 536);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * Call tcp_close
     */
    snd_nxt = socket->proto.tcp.snd_nxt;
    ip_tx_msg_called = 0;
    socket->ops->close(socket, 0);
    /*
     * and verify that a FIN has been sent
     */
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->fin);
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->syn);
    ASSERT(TCP_STATUS_FIN_WAIT_1 == socket->proto.tcp.status);
    /*
     * SND_NXT and SND_MAX should have been increased
     */
    ASSERT(snd_nxt + 1 == socket->proto.tcp.snd_nxt);
    /*
     * Now send a text segment which does not yet acknowledge our FIN
     */
    for (i = 0; i < 100; i++)
        buffer[i] = i;
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1, 8192, buffer, 100);
    ip_tx_msg_called = 0;
    socket->proto.tcp.eof = 0;
    do_putchar = 1;
    tcp_rx_msg(text);
    ASSERT(0 == ip_tx_msg_called);
    /*
     * Verify that data has been added to receive buffer
     */
    ASSERT(0 == socket->proto.tcp.rcv_buffer_head);
    ASSERT(100 == socket->proto.tcp.rcv_buffer_tail);
    for (i = 0; i < 100; i++)
        ASSERT(buffer[i] == socket->proto.tcp.rcv_buffer[i]);
    return 0;
}

/*
 * Testcase 109:
 *
 * Close a socket in state ESTABLISHED and verify that FIN is sent and the socket moves to state FIN_WAIT_1. Then receive
 * a text segment acknowleding our FIN while socket->eof is not set. Verify that the segment is processed and added to the receive buffer
 *
 */
int testcase109() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    net_msg_t* text;
    net_msg_t* fin_ack;
    u32 snd_nxt;
    u8 buffer[256];
    int i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window  536
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 536);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * Call tcp_close
     */
    snd_nxt = socket->proto.tcp.snd_nxt;
    ip_tx_msg_called = 0;
    socket->ops->close(socket, 0);
    /*
     * and verify that a FIN has been sent
     */
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->fin);
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->syn);
    ASSERT(TCP_STATUS_FIN_WAIT_1 == socket->proto.tcp.status);
    /*
     * SND_NXT and SND_MAX should have been increased
     */
    ASSERT(snd_nxt + 1 == socket->proto.tcp.snd_nxt);
    /*
     * Now send a text segment which does not  acknowledge our FIN
     */
    for (i = 0; i < 100; i++)
        buffer[i] = i;
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 2, 8192, buffer, 100);
    ip_tx_msg_called = 0;
    socket->proto.tcp.eof = 0;
    do_putchar = 1;
    tcp_rx_msg(text);
    ASSERT(0 == ip_tx_msg_called);
    /*
     * Verify that data has been added to receive buffer
     */
    ASSERT(0 == socket->proto.tcp.rcv_buffer_head);
    ASSERT(100 == socket->proto.tcp.rcv_buffer_tail);
    for (i = 0; i < 100; i++)
        ASSERT(buffer[i] == socket->proto.tcp.rcv_buffer[i]);
    /*
     * State should now be FIN_WAIT_2
     */
    ASSERT(TCP_STATUS_FIN_WAIT_2 == socket->proto.tcp.status);
    return 0;
}

/*
 * Testcase 110:
 *
 * Close a socket in state ESTABLISHED and verify that FIN is sent and the socket moves to state FIN_WAIT_1. Then receive
 * a text segment acknowleding our FIN while socket->eof is set. Verify that the segment is processed
 * but not added to the receive buffer
 *
 */
int testcase110() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    net_msg_t* text;
    net_msg_t* fin_ack;
    u32 snd_nxt;
    u32 rcv_nxt;
    u8 buffer[256];
    int i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window  536
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 536);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * Call tcp_close
     */
    snd_nxt = socket->proto.tcp.snd_nxt;
    ip_tx_msg_called = 0;
    socket->ops->close(socket, 0);
    /*
     * and verify that a FIN has been sent
     */
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->fin);
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->syn);
    ASSERT(TCP_STATUS_FIN_WAIT_1 == socket->proto.tcp.status);
    /*
     * SND_NXT and SND_MAX should have been increased
     */
    ASSERT(snd_nxt + 1 == socket->proto.tcp.snd_nxt);
    /*
     * Now send a text segment which does not  acknowledge our FIN
     */
    for (i = 0; i < 100; i++)
        buffer[i] = i;
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 2, 8192, buffer, 100);
    ip_tx_msg_called = 0;
    socket->proto.tcp.eof = 1;
    do_putchar = 1;
    rcv_nxt = socket->proto.tcp.rcv_nxt;
    tcp_rx_msg(text);
    ASSERT(0 == ip_tx_msg_called);
    /*
     * Verify that data has NOT been added to receive buffer
     */
    ASSERT(0 == socket->proto.tcp.rcv_buffer_head);
    ASSERT(0 == socket->proto.tcp.rcv_buffer_tail);
    /*
     * State should now be FIN_WAIT_2
     */
    ASSERT(TCP_STATUS_FIN_WAIT_2 == socket->proto.tcp.status);
    /*
     * We should still have increased RCV_NXT
     */
    ASSERT(rcv_nxt + 100 == socket->proto.tcp.rcv_nxt);
    return 0;
}

/*
 * Testcase 111:
 *
 * Close a socket in state ESTABLISHED and verify that FIN is sent and the socket moves to state FIN_WAIT_1. Then process
 * an FIN-ACK and move to state FIN_WAIT_2. Finally receive a text segment with socket->eof being 0. Verify that segment
 * is fully processed
 *
 */
int testcase111() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    net_msg_t* fin_ack;
    net_msg_t* text;
    u32 snd_nxt;
    int i;
    u8 buffer[256];
    u32 rcv_nxt;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window  536
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 536);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * Call tcp_close
     */
    snd_nxt = socket->proto.tcp.snd_nxt;
    ip_tx_msg_called = 0;
    socket->ops->close(socket, 0);
    /*
     * and verify that a FIN has been sent
     */
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->fin);
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->syn);
    ASSERT(TCP_STATUS_FIN_WAIT_1 == socket->proto.tcp.status);
    /*
     * SND_NXT and SND_MAX should have been increased
     */
    ASSERT(snd_nxt + 1 == socket->proto.tcp.snd_nxt);
    /*
     * Now assemble a segment which acknowledges our FIN
     */
    fin_ack = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 2, 8192, 0, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(fin_ack);
    /*
     * We should now be in FIN_WAIT_2
     */
    ASSERT(TCP_STATUS_FIN_WAIT_2 == socket->proto.tcp.status);
    ASSERT(0 == ip_tx_msg_called);
    /*
     * Now send a text segment
     */
    for (i = 0; i < 100; i++)
        buffer[i] = i;
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 2, 8192, buffer, 100);
    ip_tx_msg_called = 0;
    socket->proto.tcp.eof = 0;
    do_putchar = 1;
    tcp_rx_msg(text);
    ASSERT(0 == ip_tx_msg_called);
    /*
     * Verify that data has been added to receive buffer
     */
    ASSERT(0 == socket->proto.tcp.rcv_buffer_head);
    ASSERT(100 == socket->proto.tcp.rcv_buffer_tail);
    for (i = 0; i < 100; i++)
        ASSERT(buffer[i] == socket->proto.tcp.rcv_buffer[i]);
    /*
     * State should now be FIN_WAIT_2
     */
    ASSERT(TCP_STATUS_FIN_WAIT_2 == socket->proto.tcp.status);
    return 0;
}

/*
 * Testcase 112:
 *
 * Close a socket in state ESTABLISHED and verify that FIN is sent and the socket moves to state FIN_WAIT_1. Then process
 * an FIN-ACK and move to state FIN_WAIT_2. Finally receive a text segment with socket->eof being 1. Verify that segment
 * is acknowledged, but not moved to receive buffer
 *
 */
int testcase112() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    net_msg_t* fin_ack;
    net_msg_t* text;
    u32 snd_nxt;
    int i;
    u8 buffer[256];
    u32 rcv_nxt;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window  536
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 536);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * Call tcp_close
     */
    snd_nxt = socket->proto.tcp.snd_nxt;
    ip_tx_msg_called = 0;
    socket->ops->close(socket, 0);
    /*
     * and verify that a FIN has been sent
     */
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->fin);
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->syn);
    ASSERT(TCP_STATUS_FIN_WAIT_1 == socket->proto.tcp.status);
    /*
     * SND_NXT and SND_MAX should have been increased
     */
    ASSERT(snd_nxt + 1 == socket->proto.tcp.snd_nxt);
    /*
     * Now assemble a segment which acknowledges our FIN
     */
    fin_ack = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 2, 8192, 0, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(fin_ack);
    /*
     * We should now be in FIN_WAIT_2
     */
    ASSERT(TCP_STATUS_FIN_WAIT_2 == socket->proto.tcp.status);
    ASSERT(0 == ip_tx_msg_called);
    /*
     * Now send a text segment
     */
    for (i = 0; i < 100; i++)
        buffer[i] = i;
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 2, 8192, buffer, 100);
    ip_tx_msg_called = 0;
    socket->proto.tcp.eof = 1;
    do_putchar = 1;
    rcv_nxt = socket->proto.tcp.rcv_nxt;
    tcp_rx_msg(text);
    ASSERT(0 == ip_tx_msg_called);
    /*
     * Verify that data has NOT been added to receive buffer
     */
    ASSERT(0 == socket->proto.tcp.rcv_buffer_head);
    ASSERT(0 == socket->proto.tcp.rcv_buffer_tail);
    /*
     * but RCV_NXT is advanced
     */
    ASSERT(rcv_nxt + 100 == socket->proto.tcp.rcv_nxt);
    return 0;
}

/*
 * Testcase 113:
 *
 * Close a socket in state ESTABLISHED and verify that FIN is sent and the socket moves to state FIN_WAIT_1. Then process
 * an ACK which acknowledges only data sent earlier and make sure that we stay in FIN_WAIT_1
 *
 */
int testcase113() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    net_msg_t* ack;
    u32 snd_nxt;
    u8 buffer[256];
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window  536
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 536);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * Now send 100 bytes
     */
    ASSERT(100 == socket->ops->send(socket, buffer, 100, 0));
    /*
     * Call tcp_close
     */
    snd_nxt = socket->proto.tcp.snd_nxt;
    ip_tx_msg_called = 0;
    socket->ops->close(socket, 0);
    /*
     * and verify that a FIN has been sent
     */
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->fin);
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->syn);
    ASSERT(TCP_STATUS_FIN_WAIT_1 == socket->proto.tcp.status);
    /*
     * SND_NXT and SND_MAX should have been increased
     */
    ASSERT(snd_nxt + 1 == socket->proto.tcp.snd_nxt);
    /*
     * Now ACK the data which we have sent previously
     */
    ack = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 101, 8192, 0, 0);
    ip_tx_msg_called = 0;
    tcp_rx_msg(ack);
    ASSERT(0 == ip_tx_msg_called);
    ASSERT(TCP_STATUS_FIN_WAIT_1 == socket->proto.tcp.status);
    return 0;
}

/*
 * Testcase 114:
 * Create a socket and establish a connection. Then simulate receipt of an acceptable RST
 */
int testcase114() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    net_msg_t* rst;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    int i;
    unsigned char buffer[8192];
    /*
     * Fill buffer
     */
    for (i = 0; i < 128; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window 600
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 600);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that the window size has been updated
     */
    ASSERT(600 == socket->proto.tcp.snd_wnd);
    /*
     * send acceptable RST
     */
    rst = create_rst_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1);
    ip_tx_msg_called = 0;
    tcp_rx_msg(rst);
    /*
     * and verify that socket has been dropped
     */
    ASSERT(0 == ip_tx_msg_called);
    do_putchar = 0;
    ASSERT(0 == tcp_print_sockets());
    /*
     * with error ECONNRESET
     */
    ASSERT(-141 == socket->error);
    return 0;
}

/*
 * Testcase 115: bind a socket to a non-fully qualified address, then listen on it. Verify that the status of the socket
 * changes to LISTEN. Simulate receipt of a SYN and verify that a SYN-ACK is sent in response
 * and the status of the new socket changes to SYN-RCVD. Then send a valid RST
 */
int testcase115() {
    socket_t socket;
    socket_t* new_socket;
    struct sockaddr_in laddr;
    struct sockaddr_in* saddr;
    u32 syn_ack_seq_no;
    net_msg_t* syn;
    net_msg_t* rst;
    tcp_hdr_t* tcp_hdr;
    u16* mss;
    tcp_init();
    mtu = 1500;
    tcp_create_socket(&socket, AF_INET, 0);
    laddr.sin_family = AF_INET;
    laddr.sin_port = htons(30000);
    laddr.sin_addr.s_addr = 0;
    ASSERT(0 == socket.ops->bind(&socket, (struct sockaddr*) &laddr, sizeof(struct sockaddr_in)));
    /*
     * Now listen on socket
     */
    socket.max_connection_backlog = 15;
    socket.ops->listen(&socket);
    ASSERT(socket.proto.tcp.status == TCP_STATUS_LISTEN);
    /*
     * Simulate receipt of SYN
     */
    syn = create_syn(inet_addr("10.0.2.21"), inet_addr("10.0.2.20"), 1024, 30000, 100, 8192, 800);
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn);
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->syn);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(101 == ntohl(tcp_hdr->ack_no));
    new_socket = socket.so_queue_head;
    ASSERT(new_socket);
    ASSERT(2 == tcp_print_sockets());
    ASSERT(new_socket->proto.tcp.status == TCP_STATUS_SYN_RCVD);
    syn_ack_seq_no = ntohl(tcp_hdr->seq_no);
    /*
     * Verify that our SYN_ACK contains the MSS 1460
     */
    ASSERT(6 == tcp_hdr->hlength);
    ASSERT(*(payload + sizeof(tcp_hdr_t)) == 2);
    ASSERT(*(payload + sizeof(tcp_hdr_t) + 1) == 4);
    mss = (u16*)(payload + sizeof(tcp_hdr_t) + 2);
    ASSERT(ntohs(*mss) == 1460);
    /*
     * and that our own SMSS has been set to 800
     */
    ASSERT(new_socket->proto.tcp.smss == 800);
    /*
     * Check that local address of the socket has been updated
     */
    saddr = (struct sockaddr_in*) &new_socket->laddr;
    ASSERT(saddr->sin_addr.s_addr == inet_addr("10.0.2.20"));
    ASSERT(saddr->sin_family == AF_INET);
    ASSERT(saddr->sin_port == ntohs(30000));
    /*
     * but the local address of the listening socket remains
     * unchanged
     */
    saddr = (struct sockaddr_in*) &socket.laddr;
    ASSERT(0 == saddr->sin_addr.s_addr);
    /*
     * Now send reset
     */
    rst = create_rst_ack(inet_addr("10.0.2.21"), inet_addr("10.0.2.20"), 1024, 30000, 101, syn_ack_seq_no + 1);
    ip_tx_msg_called = 0;
    tcp_rx_msg(rst);
    /*
     * Verify that there is only one socket left
     */
    do_putchar = 0;
    ASSERT(1 == tcp_print_sockets());
    /*
     * and connection queue is empty again
     */
    ASSERT(0 == socket.so_queue_head);
    return 0;
}

/*
 * Testcase 116:
 * Do a simultaneous open, then receive an acceptable RST
 */
int testcase116() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32 syn_seq_no;
    u32* ack_seq_no;
    u32* ack_ack_no;
    net_msg_t* syn;
    net_msg_t* rst;
    u8* options;
    /*
     * Do basic initialization of socket
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    ASSERT(0 ==  ((struct sockaddr_in*) &socket->laddr)->sin_addr.s_addr);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    mtu = 1500;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * and that the local IP address of the socket has been set
     */
    ASSERT(inet_addr("10.0.2.20") ==  ((struct sockaddr_in*) &socket->laddr)->sin_addr.s_addr);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN message from 10.0.2.21:30000 to our local port, using seq_no 1
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn = create_syn(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, 8192, 1460);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn);
    __net_loglevel = 0;
    /*
     * Now validate that status of socket is SYN_RECEIVED
     */
    ASSERT(TCP_STATUS_SYN_RCVD == socket->proto.tcp.status);
    /*
     * and that a SYN-ACK has been sent
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Check that
     * 1) the sequence number of the SYN-ACK is the sequence number of the syn
     * 2) the acknowledgement number of the ACK is the sequence number of the SYN + 1, i.e. 2
     * 3) the message has the ACK flag set and SYN set
     * 4) the TCP checksum is correct
     * 5) IP source and IP destination are correct
     * 6) segment contains MSS option
     */
    tcp_hdr_t* tcp_hdr = (tcp_hdr_t*) payload;
    ack_seq_no = (u32*) (payload + 4);
    ack_ack_no = (u32*) (payload + 8);
    ctrl_flags = *((u8*)(payload + 13));
    ASSERT(ntohl(*ack_ack_no) == 2);
    ASSERT(ntohl(*ack_seq_no) == (syn_seq_no));
    ASSERT(ctrl_flags == ((1 << 4) | (1 << 1)));
    ASSERT(6 == tcp_hdr->hlength);
    ASSERT(0 == validate_tcp_checksum(24, (u16*) payload, ip_src, ip_dst));
    ASSERT(ip_src == 0x1402000a);
    ASSERT(ip_dst == 0x1502000a);
    options = (u8*)(payload + 20);
    ASSERT(2 == options[0]);
    ASSERT(4 == options[1]);
    ASSERT(1460 == options[2]*256 + options[3]);
    /*
     * Assert that the connected flag in the socket is not set
     */
    ASSERT(socket->connected == 0);
    ASSERT(socket->bound == 1);
    /*
     * Verify RCV_NXT and SND_UNA
     */
    ASSERT(2 == socket->proto.tcp.rcv_nxt);
    ASSERT(syn_seq_no == socket->proto.tcp.snd_una);
    /*
     * Now simulate receipt of a RST
     */
    rst = create_rst(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, 0);
    ip_tx_msg_called = 0;
    __net_loglevel = 1;
    tcp_rx_msg(rst);
    /*
     * TCB should have been dropped
     */
    ASSERT(0 == tcp_print_sockets());
    /*
     * with error code ECONNREFUSED
     */
    ASSERT(-142 == socket->error);
    return 0;
}

/*
 * Testcase 117:
 *
 * Receive a SYN for a socket in a state other than LISTEN or SYN_RCVD. Connection should be reset
 *
 */
int testcase117() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    u32 snd_nxt;
    u8 buffer[256];
    int i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window  536
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 536);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * Send SYN
     */
    syn = create_syn(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, 8192, 1460);
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn);
    /*
     * Should have dropped socket
     */
    ASSERT(0 == tcp_print_sockets());
    /*
     * with a RST
     */
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->fin);
    ASSERT(0 == tcp_hdr->syn);
    /*
     * and error code CONNRESET
     */
    ASSERT(-141 == socket->error);
    return 0;
}

/*
 * Testcase 118:
 *
 * Close a socket in state ESTABLISHED and verify that FIN is sent and the socket moves to state FIN_WAIT_1. Then process
 * a FIN-ACK which does not acknowledge our FIN (simultaneous close)
 *
 */
int testcase118() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    net_msg_t* fin_ack;
    u32 snd_nxt;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window  536
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 536);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * Call tcp_close
     */
    snd_nxt = socket->proto.tcp.snd_nxt;
    ip_tx_msg_called = 0;
    socket->ops->close(socket, 0);
    /*
     * and verify that a FIN has been sent
     */
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->fin);
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->syn);
    ASSERT(TCP_STATUS_FIN_WAIT_1 == socket->proto.tcp.status);
    /*
     * SND_NXT and SND_MAX should have been increased
     */
    ASSERT(snd_nxt + 1 == socket->proto.tcp.snd_nxt);
    /*
     * Now assemble a segment which does no acknowledge our FIN and contains itself a FIN
     */
    fin_ack = create_fin_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1, 8192);
    ip_tx_msg_called = 0;
    tcp_rx_msg(fin_ack);
    /*
     * We should now be in CLOSING
     */
    ASSERT(TCP_STATUS_CLOSING == socket->proto.tcp.status);
    /*
     * and should have acknowledged the FIN
     */
    ASSERT(1 == ip_tx_msg_called);
    ASSERT(0 == tcp_hdr->fin);
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->syn);
    ASSERT(3 == ntohl(tcp_hdr->ack_no));
    /*
     * and time_wait timer should not be set
     */
    ASSERT(0 == socket->proto.tcp.time_wait_timer.time);
    return 0;
}

/*
 * Testcase 119:
 *
 * Close a socket in state ESTABLISHED and verify that FIN is sent and the socket moves to state FIN_WAIT_1. Then process
 * a FIN-ACK which does not acknowledge our FIN (simultaneous close). Then move socket to TIME_WAIT by acknowledging the FIN
 *
 */
int testcase119() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    net_msg_t* fin_ack;
    net_msg_t* ack;
    u32 snd_nxt;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window  536
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 536);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * Call tcp_close
     */
    snd_nxt = socket->proto.tcp.snd_nxt;
    ip_tx_msg_called = 0;
    socket->ops->close(socket, 0);
    /*
     * and verify that a FIN has been sent
     */
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->fin);
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->syn);
    ASSERT(TCP_STATUS_FIN_WAIT_1 == socket->proto.tcp.status);
    /*
     * SND_NXT and SND_MAX should have been increased
     */
    ASSERT(snd_nxt + 1 == socket->proto.tcp.snd_nxt);
    /*
     * Now assemble a segment which does no acknowledge our FIN and contains itself a FIN
     */
    fin_ack = create_fin_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1, 8192);
    ip_tx_msg_called = 0;
    tcp_rx_msg(fin_ack);
    /*
     * We should now be in CLOSING
     */
    ASSERT(TCP_STATUS_CLOSING == socket->proto.tcp.status);
    /*
     * and should have acknowledged the FIN
     */
    ASSERT(1 == ip_tx_msg_called);
    ASSERT(0 == tcp_hdr->fin);
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->syn);
    ASSERT(3 == ntohl(tcp_hdr->ack_no));
    /*
     * and time_wait timer should not be set
     */
    ASSERT(0 == socket->proto.tcp.time_wait_timer.time);
    /*
     * Now ACK the FIN
     */
    ip_tx_msg_called = 0;
    ack = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 3, syn_seq_no + 2, 8192, 0, 0);
    tcp_rx_msg(ack);
    ASSERT(0 == ip_tx_msg_called);
    /*
     * Should be in TIME_WAIT now
     */
    ASSERT(TCP_STATUS_TIME_WAIT == socket->proto.tcp.status);
    /*
     * All timers should be turned off
     */
    ASSERT(0 == socket->proto.tcp.rtx_timer.time);
    ASSERT(0 == socket->proto.tcp.delack_timer.time);
    ASSERT(0 == socket->proto.tcp.persist_timer.time);
    /*
     * and time_wait timer should have been reset to 2*MSL
     */
    ASSERT(2*TCP_MSL == socket->proto.tcp.time_wait_timer.time);
    return 0;
}

/*
 * Testcase 120:
 * Receive a SYN while being in state SYN_SENT - simultaneous open. Verify that a SYN-ACK is sent in response and
 * the socket moves to state SYN_RECEIVED. Then close this socket. Check that a FIN is sent and the socket state
 * is FIN_WAIT_1
 */
int testcase120() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32 syn_seq_no;
    u32* ack_seq_no;
    u32* ack_ack_no;
    net_msg_t* syn;
    u8* options;
    /*
     * Do basic initialization of socket
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    ASSERT(0 ==  ((struct sockaddr_in*) &socket->laddr)->sin_addr.s_addr);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    mtu = 1500;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * and that the local IP address of the socket has been set
     */
    ASSERT(inet_addr("10.0.2.20") ==  ((struct sockaddr_in*) &socket->laddr)->sin_addr.s_addr);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * In our socket, we should have advanced SND_NXT to SND_UNA + 1
     */
    ASSERT(socket->proto.tcp.snd_nxt == socket->proto.tcp.snd_una + 1);
    /*
     * Assemble a SYN message from 10.0.2.21:30000 to our local port, using seq_no 1
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn = create_syn(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, 8192, 1460);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn);
    __net_loglevel = 0;
    /*
     * Now validate that status of socket is SYN_RECEIVED
     */
    ASSERT(TCP_STATUS_SYN_RCVD == socket->proto.tcp.status);
    /*
     * and that a SYN-ACK has been sent
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Check that
     * 1) the sequence number of the SYN-ACK is the sequence number of the syn
     * 2) the acknowledgement number of the ACK is the sequence number of the SYN + 1, i.e. 2
     * 3) the message has the ACK flag set and SYN set
     * 4) the TCP checksum is correct
     * 5) IP source and IP destination are correct
     * 6) segment contains MSS option
     */
    tcp_hdr_t* tcp_hdr = (tcp_hdr_t*) payload;
    ack_seq_no = (u32*) (payload + 4);
    ack_ack_no = (u32*) (payload + 8);
    ctrl_flags = *((u8*)(payload + 13));
    ASSERT(ntohl(*ack_ack_no) == 2);
    ASSERT(ntohl(*ack_seq_no) == (syn_seq_no));
    ASSERT(ctrl_flags == ((1 << 4) | (1 << 1)));
    ASSERT(6 == tcp_hdr->hlength);
    ASSERT(0 == validate_tcp_checksum(24, (u16*) payload, ip_src, ip_dst));
    ASSERT(ip_src == 0x1402000a);
    ASSERT(ip_dst == 0x1502000a);
    options = (u8*)(payload + 20);
    ASSERT(2 == options[0]);
    ASSERT(4 == options[1]);
    ASSERT(1460 == options[2]*256 + options[3]);
    /*
     * There should be one unacknowledged octet now, namely our SYN
     */
     ASSERT(socket->proto.tcp.snd_nxt == socket->proto.tcp.snd_una + 1);
    /*
     * Assert that the connected flag in the socket is not set
     */
    ASSERT(socket->connected == 0);
    ASSERT(socket->bound == 1);
    /*
     * Verify RCV_NXT and SND_UNA
     */
    ASSERT(2 == socket->proto.tcp.rcv_nxt);
    ASSERT(syn_seq_no == socket->proto.tcp.snd_una);
    /*
     * Now close this socket
     */
    ip_tx_msg_called = 0;
    do_putchar = 1;
    socket->ops->close(socket, 0);
    /*
     * We should have sent a FIN now
     */
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->fin);
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->syn);
    /*
     * with sequence number syn_seq_no + 1 and ACK_NO 2
     */
    ASSERT(syn_seq_no + 1 == ntohl(tcp_hdr->seq_no));
    ASSERT(2 == ntohl(tcp_hdr->ack_no));
    /*
     * and the socket should be in state FIN_WAIT_1
     */
    ASSERT(TCP_STATUS_FIN_WAIT_1 == socket->proto.tcp.status);
    return 0;
}

/*
 * Testcase 121:
 *
 * Close a socket in state ESTABLISHED while there is still data in the sent buffer. Verify that this data is sent first
 * before the FIN is sent
 *
 */
int testcase121() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    net_msg_t* text;
    u32 snd_nxt;
    u8 buffer[2048];
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window  536
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 536);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * Now put 1024 bytes into the send buffer
     */
    ip_tx_msg_called = 0;
    ASSERT(1024 == socket->ops->send(socket, buffer, 1024, 0));
    /*
     * We should now have sent a first segment with 536 bytes
     */
    ASSERT(1 == ip_tx_msg_called);
    ASSERT(sizeof(tcp_hdr_t) + 536 == ip_payload_len);
    /*
     * Call tcp_close
     */
    snd_nxt = socket->proto.tcp.snd_nxt;
    ip_tx_msg_called = 0;
    socket->ops->close(socket, 0);
    /*
     * and verify that no FIN has been sent yet
     */
    ASSERT(0 == ip_tx_msg_called);
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * Now simulate an ACK from the peer - this will trigger sending
     * the remaining data
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 537, 8192, 0, 0);
    tcp_rx_msg(text);
    ASSERT(1 == ip_tx_msg_called);
    /*
     * This segment should be 1024 - 536 = 488 bytes of data plus a FIN
     */
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(sizeof(tcp_hdr_t) + 488 == ip_payload_len);
    ASSERT(1 == tcp_hdr->fin);
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(0 == tcp_hdr->syn);
    /*
     * and status should now be FIN_WAIT_1
     */
    ASSERT(TCP_STATUS_FIN_WAIT_1 == socket->proto.tcp.status);
    return 0;
}

/*
 * Testcase 122:
 *
 * Receive a FIN for a socket in state ESTABLISHED and verify that state CLOSE_WAIT is entered - FIN is contained in data segment
 *
 */
int testcase122() {
    socket_t* socket;
    socket_t* new_socket;
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    tcp_hdr_t* tcp_hdr;
    u32 syn_seq_no = 0;
    u32 rcv_nxt;
    net_msg_t* fin;
    unsigned char buffer[256];
    u16* mss;
    tcp_init();
    mtu = 1500;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    tcp_create_socket(socket, AF_INET, 0);
    ip_tx_msg_called = 0;
    in.sin_family = AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * establish connection by receiving a SYN_ACK
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(inet_addr("10.0.2.21"), inet_addr("10.0.2.20"), 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 8192);
    tcp_rx_msg(syn_ack);
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and simulate receipt of FIN containing data
     */
    rcv_nxt = socket->proto.tcp.rcv_nxt;
    fin = create_fin_text(inet_addr("10.0.2.21"), inet_addr("10.0.2.20"), 30000, ntohs(in_ptr->sin_port), 2,
            syn_seq_no + 1, 8192, buffer, 100);
    ASSERT(fin);
    ip_tx_msg_called = 0;
    do_putchar = 1;
    tcp_rx_msg(fin);
    ASSERT(TCP_STATUS_CLOSE_WAIT == socket->proto.tcp.status);
    /*
     * RCV_NXT should have been advanced by 101
     */
    ASSERT(rcv_nxt + 101 == socket->proto.tcp.rcv_nxt);
    /*
     * and we should have sent an ACK for the FIN and all the data received
     */
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(0 == tcp_hdr->fin);
    ASSERT(0 == tcp_hdr->syn);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(103 == ntohl(tcp_hdr->ack_no));
    ASSERT(syn_seq_no + 1 == ntohl(tcp_hdr->seq_no));
    /*
     * A further read should return 100 bytes
     */
    ASSERT(100 == socket->ops->recv(socket, buffer, 100, 0));
    /*
     * and then EOF
     */
    ASSERT(0 == socket->ops->recv(socket, buffer, 100, 0));
    return 0;
}

/*
 * Testcase 123:
 * Create a socket in state SYN_SENT and close it
 */
int testcase123() {
    struct sockaddr_in in;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u16* mss;
    tcp_init();
    /*
     * Do basic initialization of socket
     */
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * State should by SYN_SENT
     */
    ASSERT(TCP_STATUS_SYN_SENT == socket->proto.tcp.status);
    /*
     * Now close socket
     */
    ASSERT(0 == socket->ops->close(socket, 0));
    /*
     * and verify that it is gone
     */
    do_putchar = 0;
    ASSERT(0 == tcp_print_sockets());
    do_putchar = 1;
    return 0;
}

/*
 * Testcase 124: bind a socket to a fully qualified address, then listen on it. Verify that the status of the socket
 * changes to LISTEN. Simulate receipt of a SYN to create a new socket in state SYN_RCVD. Then close listening socket
 * and verify that both sockets are cleaned up
 *
 */
int testcase124() {
    socket_t socket;
    socket_t* new_socket;
    struct sockaddr_in laddr;
    struct sockaddr_in* saddr;
    net_msg_t* syn;
    net_msg_t* ack;
    tcp_hdr_t* tcp_hdr;
    u16* mss;
    u32 syn_ack_seq_no;
    u32 eflags;
    tcp_init();
    /*
     * Make sure that there are no open sockets
     */
    do_putchar = 0;
    ASSERT(0 == tcp_print_sockets());
    do_putchar = 1;
    mtu = 1500;
    tcp_create_socket(&socket, AF_INET, 0);
    laddr.sin_family = AF_INET;
    laddr.sin_port = htons(30000);
    laddr.sin_addr.s_addr = inet_addr("10.0.2.20");
    ASSERT(0 == socket.ops->bind(&socket, (struct sockaddr*) &laddr, sizeof(struct sockaddr_in)));
    /*
     * Now listen on socket
     */
    socket.max_connection_backlog = 15;
    socket.ops->listen(&socket);
    ASSERT(socket.proto.tcp.status == TCP_STATUS_LISTEN);
    /*
     * Simulate receipt of SYN
     */
    syn = create_syn(inet_addr("10.0.2.21"), inet_addr("10.0.2.20"), 1024, 30000, 100, 8192, 800);
    ip_tx_msg_called = 0;
    __net_loglevel = 0;
    tcp_rx_msg(syn);
    __net_loglevel = 0;
    ASSERT(1 == ip_tx_msg_called);
    tcp_hdr = (tcp_hdr_t*) payload;
    ASSERT(1 == tcp_hdr->syn);
    ASSERT(0 == tcp_hdr->rst);
    ASSERT(1 == tcp_hdr->ack);
    ASSERT(101 == ntohl(tcp_hdr->ack_no));
    new_socket = socket.so_queue_head;
    ASSERT(new_socket);
    ASSERT(new_socket->proto.tcp.status == TCP_STATUS_SYN_RCVD);
    /*
     * Should have two sockets now
     */
    do_putchar = 0;
    ASSERT(2 == tcp_print_sockets());
    do_putchar = 1;
    /*
     * Now close listening socket
     */
    __net_loglevel = 0;
    ip_tx_msg_called = 0;
    ASSERT(0 == socket.ops->close(&socket, &eflags));
    __net_loglevel = 0;
    /*
     * The socket in state listen should have been dropped immediately, whereas the
     * socket in state SYN_RCVD should be in FIN_WAIT_1 and should have emitted a FIN
     */
    do_putchar = 0;
    ASSERT(1 == tcp_print_sockets());
    do_putchar = 1;
    ASSERT(TCP_STATUS_FIN_WAIT_1 == new_socket->proto.tcp.status);
    ASSERT(TCP_STATUS_CLOSED == socket.proto.tcp.status);
    ASSERT(1 == ip_tx_msg_called);
    return 0;
}

/*
 * Testcase 125:
 * Create a socket and establish a connection. Then simulate receipt of one segment of data. Read data from the receive
 * buffer using recvfrom
 *
 *  #   Socket under test                                         Peer
 * -------------------------------------------------------------------------------------------------------
 *
 *  1   SYN, SEQ = syn_seq_no, ACK_NO = 0 ----------------------->
 *  2                                                         <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *  3   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 0 ---------->
 *  4                                                         <-- ACK, SEQ = 2, ACK_NO = syn_seq_no + 1, LEN = 536
 */
int testcase125() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    struct sockaddr_in msg_addr;
    int addrlen;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    net_msg_t* text;
    u32 syn_seq_no = 0;
    u32 syn_win_size = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    int i;
    unsigned char buffer[2048];
    unsigned char rcv_buffer[2048];
    /*
     * Fill buffer
     */
    for (i = 0; i < 536*2; i++)
        buffer[i] = i;
    /*
     * Do basic initialization
     */
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number and window size from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    syn_win_size = htons(*((u16*) (payload + 14)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window 600
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 600);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that the window size has been updated
     */
    ASSERT(600 == socket->proto.tcp.snd_wnd);
    /*
     * Call recv and verify that no message is generated and that -EAGAIN is returned
     */
    ip_tx_msg_called = 0;
    ASSERT(-106 == socket->ops->recvfrom(socket, buffer, 512, 0, 0, 0));
    ASSERT(0 == ip_tx_msg_called);
    /*
     * Put together next segment
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1, 600, buffer, 536);
    ip_tx_msg_called = 0;
    cond_broadcast_called = 0;
    tcp_rx_msg(text);
    /*
     * Now read 536 bytes
     */
    ip_tx_msg_called = 0;
    addrlen = sizeof(struct sockaddr_in);
    ASSERT(536 == socket->ops->recvfrom(socket, rcv_buffer, 536, 0, (struct sockaddr*) &msg_addr, &addrlen));
    /*
     * Verify data
     */
    for (i = 0; i < 536;i++)
        ASSERT(buffer[i] == rcv_buffer[i]);
    /*
     * and address
     */
    ASSERT(sizeof(struct sockaddr_in) == addrlen);
    ASSERT(msg_addr.sin_family == AF_INET);
    ASSERT(msg_addr.sin_addr.s_addr == inet_addr("10.0.2.21"));
    ASSERT(msg_addr.sin_port == ntohs(30000));
    return 0;
}

/*
 * Testcase 126:
 * Create a socket and establish a connection. Then simulate receipt of a single segment containing 1024 bytes of data
 * Use correspondingly large MSS
 *
 *  #   Socket under test                                         Peer
 * -------------------------------------------------------------------------------------------------------
 *
 *  1   SYN, SEQ = syn_seq_no, ACK_NO = 0 ----------------------->
 *  2                                                         <-- SYN, ACK, SEQ = 1, ACK_NO = syn_seq_no + 1
 *  3   ACK, SEQ = syn_seq_no + 1, ACK_NO = 2, LEN = 0 ---------->
 *  4                                                         <-- ACK, SEQ = 2, ACK_NO = syn_seq_no + 1, LEN = 2048
 */
int testcase126() {
    struct sockaddr_in in;
    struct sockaddr_in* in_ptr;
    u16 chksum;
    u16 actual_chksum;
    u8 hdr_length;
    u8 ctrl_flags;
    u16* dst_port;
    u32* rst_seq_no;
    u32* rst_ack_no;
    net_msg_t* syn;
    net_msg_t* syn_ack;
    net_msg_t* text;
    u32 syn_seq_no = 0;
    tcp_hdr_t* tcp_hdr;
    u8* segment_data;
    int i;
    int old_mtu;
    unsigned char buffer[8192];
    /*
     * Fill buffer
     */
    for (i = 0; i < 128; i++)
        buffer[i] = i;
    /*
     * Do basic initialization - set MTU to 2048 first
     */
    old_mtu = mtu;
    mtu = 2048;
    net_init();
    tcp_init();
    socket_t* socket;
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    socket->bound = 0;
    socket->connected = 0;
    /*
     * and call tcp socket creation
     */
    tcp_create_socket(socket, AF_INET, IPPROTO_TCP);
    ASSERT(2 == socket->proto.tcp.ref_count);
    /*
     * Now try to connect to 10.0.2.21 / port 30000
     */
    ip_tx_msg_called = 0;
    in.sin_family =AF_INET;
    in.sin_port = htons(30000);
    in.sin_addr.s_addr = 0x1502000a;
    ASSERT(-106 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * and verify that ip_tx_msg has been called
     */
    ASSERT(1 == ip_tx_msg_called);
    /*
     * Extract sequence number from SYN
     */
    syn_seq_no = htonl(*((u32*) (payload + 4)));
    /*
     * Assemble a SYN-ACK message from 10.0.2.21:30000 to our local port, using seq_no 1 and window 2048
     */
    in_ptr = (struct sockaddr_in*) &socket->laddr;
    syn_ack = create_syn_ack(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 1, syn_seq_no + 1, 2048);
    /*
     * and simulate receipt of the message
     */
    ip_tx_msg_called = 0;
    tcp_rx_msg(syn_ack);
    /*
     * Reference count should be unchanged
     */
    ASSERT(2 == socket->proto.tcp.ref_count);
    /*
     * Now validate that status of socket is ESTABLISHED
     */
    ASSERT(TCP_STATUS_ESTABLISHED == socket->proto.tcp.status);
    /*
     * and that the window size has been updated
     */
    ASSERT(2048 == socket->proto.tcp.snd_wnd);
    /*
     * Put together segment
     */
    text = create_text(0x1502000a, 0x1402000a, 30000, ntohs(in_ptr->sin_port), 2, syn_seq_no + 1, 600, buffer, 1024);
    ip_tx_msg_called = 0;
    __net_loglevel = 0;
    tcp_rx_msg(text);
    __net_loglevel = 0;
    /*
     * Verify that no immediate response is sent - the ACK will be delayed!
     */
    ASSERT(0 == ip_tx_msg_called);
    /*
     * and that 1024 bytes have been added to the receive queue
     */
    ASSERT(0 == socket->proto.tcp.rcv_buffer_head);
    ASSERT(1024 == socket->proto.tcp.rcv_buffer_tail);
    /*
     * Check data
     */
    for (i = 0; i < 1024; i++)
        ASSERT(socket->proto.tcp.rcv_buffer[i] == buffer[i]);
    /*
     * Check for memory leaks
     */
    unsigned int created;
    unsigned int destroyed;
    net_get_counters(&created, &destroyed);
    ASSERT(created == destroyed);
    /*
     * Finally reset MTU
     */
    mtu = old_mtu;
    return 0;
}

int main() {
    INIT;
    /*
     * Turn off congestion control for first few test cases
     */
    tcp_disable_cc = 1;
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
    RUN_CASE(16);
    RUN_CASE(17);
    RUN_CASE(18);
    RUN_CASE(19);
    RUN_CASE(20);
    RUN_CASE(21);
    RUN_CASE(22);
    RUN_CASE(23);
    RUN_CASE(24);
    RUN_CASE(25);
    RUN_CASE(26);
    RUN_CASE(27);
    RUN_CASE(28);
    RUN_CASE(29);
    RUN_CASE(30);
    RUN_CASE(31);
    RUN_CASE(32);
    RUN_CASE(33);
    RUN_CASE(34);
    RUN_CASE(35);
    /*
     * Now turn on congestion control
     */
    tcp_disable_cc = 0;
    RUN_CASE(36);
    RUN_CASE(37);
    RUN_CASE(38);
    RUN_CASE(39);
    RUN_CASE(40);
    RUN_CASE(41);
    RUN_CASE(42);
    RUN_CASE(43);
    RUN_CASE(44);
    RUN_CASE(45);
    RUN_CASE(46);
    RUN_CASE(47);
    RUN_CASE(48);
    RUN_CASE(49);
    RUN_CASE(50);
    RUN_CASE(51);
    RUN_CASE(52);
    RUN_CASE(53);
    RUN_CASE(54);
    RUN_CASE(55);
    RUN_CASE(56);
    RUN_CASE(57);
    RUN_CASE(58);
    RUN_CASE(59);
    RUN_CASE(60);
    RUN_CASE(61);
    RUN_CASE(62);
    RUN_CASE(63);
    RUN_CASE(64);
    RUN_CASE(65);
    RUN_CASE(66);
    RUN_CASE(67);
    RUN_CASE(68);
    RUN_CASE(69);
    RUN_CASE(70);
    RUN_CASE(71);
    RUN_CASE(72);
    RUN_CASE(73);
    RUN_CASE(74);
    RUN_CASE(75);
    RUN_CASE(76);
    RUN_CASE(77);
    RUN_CASE(78);
    RUN_CASE(79);
    RUN_CASE(80);
    RUN_CASE(81);
    RUN_CASE(82);
    RUN_CASE(83);
    RUN_CASE(84);
    RUN_CASE(85);
    RUN_CASE(86);
    RUN_CASE(87);
    RUN_CASE(88);
    RUN_CASE(89);
    RUN_CASE(90);
    RUN_CASE(91);
    RUN_CASE(92);
    RUN_CASE(93);
    RUN_CASE(94);
    RUN_CASE(95);
    RUN_CASE(96);
    RUN_CASE(97);
    RUN_CASE(98);
    RUN_CASE(99);
    RUN_CASE(100);
    RUN_CASE(101);
    RUN_CASE(102);
    RUN_CASE(103);
    RUN_CASE(104);
    RUN_CASE(105);
    RUN_CASE(106);
    RUN_CASE(107);
    RUN_CASE(108);
    RUN_CASE(109);
    RUN_CASE(110);
    RUN_CASE(111);
    RUN_CASE(112);
    RUN_CASE(113);
    RUN_CASE(114);
    RUN_CASE(115);
    RUN_CASE(116);
    RUN_CASE(117);
    RUN_CASE(118);
    RUN_CASE(119);
    RUN_CASE(120);
    RUN_CASE(121);
    RUN_CASE(122);
    RUN_CASE(123);
    RUN_CASE(124);
    RUN_CASE(125);
    RUN_CASE(126);
    END;
}
