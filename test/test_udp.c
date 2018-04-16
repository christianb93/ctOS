/*
 * test_udp.c
 */

#include "kunit.h"
#include "ktypes.h"
#include "udp.h"
#include "locks.h"
#include "vga.h"
#include "net.h"
#include "eth.h"
#include "lib/os/route.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

/****************************************************************************************
 * Stubs                                                                                *
 ***************************************************************************************/

extern int __net_loglevel;

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


/*
 * Stub for putchar
 */
static int do_putchar = 1;
void win_putchar(win_t* win, u8 c) {
    if (do_putchar)
        printf("%c", c);
}


/*
 * Stubs for synchronization primitives
 */
void cond_init(cond_t* cond) {

}

int cond_wait_intr_timed(cond_t* cond, spinlock_t* lock, u32* eflags, unsigned int timeout) {
    spinlock_release(lock, eflags);
    return -1;
}

static int cond_broadcast_called = 0;
static cond_t* last_cond;
void cond_broadcast(cond_t* cond) {
    cond_broadcast_called++;
    last_cond = cond;
}

int cond_wait_intr(cond_t* cond, spinlock_t* lock, u32* eflags) {
    return -1;
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

void atomic_incr(u32* reg) {
    (*reg)++;
}

void sem_up(semaphore_t* sem) {

}

/*
 * Validate user space buffers
 */
int mm_validate_buffer(u32 buffer, u32 len, int rw) {
    return 0;
}


/*
 * Stubs for IP layer functions
 */
void ip_init() {

}

int ip_get_mtu(u32 local_addr) {
    return 1500;
}

int ip_add_route(struct rtentry* rt_entry) {
    return 0;
}

int ip_del_route(struct rtentry* rt_entry) {
    return 0;
}

void ip_create_socket(socket_t* socket) {

}

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
    for (i = 0; (i < net_msg->end - net_msg->start) && (i < 1024); i++)
        payload[i] = net_msg->start[i];
    /*
     * Destroy network message as the real IP layer would do it
     */
    net_msg_destroy(net_msg);
}


u32 ip_get_src_addr(u32 ip_dst) {
    return 0x1402000a;
}

int ip_get_rtconf(struct rtconf* rtc) {
    return 0;
}

/*
 * Network interface layer stubs
 */
static nic_t our_nic;
nic_t* net_if_get_nic(u32 ip_address) {
    return &our_nic;
}

void net_if_init() {

}

int net_if_set_addr(struct ifreq* ifr) {
    return 0;
}

int net_if_get_addr(struct ifreq* ifr) {
    return 0;
}


int net_if_set_netmask(struct ifreq* ifr) {
    return 0;
}

int net_if_get_netmask(struct ifreq* ifr) {
    return 0;
}

int net_if_get_ifconf(struct ifconf* ifc) {
    return -1;
}


int net_if_tx_msg(net_msg_t* net_msg) {
    return 0;
}

/*
 * Given a name, get network device with that name
 */
nic_t* net_if_get_nic_by_name(char* name) {
    if (0 == strncmp("eth0", name, 4))
        return &our_nic;
    return 0;
}

/*
 * Stubs for TCP layer
 */
int tcp_create_socket(socket_t* socket) {
    return 0;
}

int tcp_init() {
    return 0;
}

/*
 * Stubs for ARP layer functions
 */
void arp_init() {

}

int arp_resolve(nic_t* nic, u32 ip_address, mac_address_t* mac_address) {
    return 0;
}

/*
 * Stubs for ICMP layer
 */
static int icmp_error_sent = 0;
void icmp_send_error(net_msg_t* net_msg, int code, int type) {
    icmp_error_sent++;
}


/*
 * Signal processing - needed by functions in net.c
 */
int do_kill(int pid, int sig_no) {
    return 0;
}

/*
 * Trap
 */

void trap() {

}

/*
 * Determine process ID
 */
unsigned int pm_get_pid() {
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

/*
 * Stubs for params_get
 */
char* params_get(char* param) {
    return "10.0.2.21";
}

unsigned int params_get_int(char* param) {
    return 0;
}


/*
 * Compute UDP checksum
 */
u16 validate_udp_checksum(unsigned short udpLen, u16* ip_payload, u32 ip_src, u32 ip_dst) {
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
     * protocol and reserved: 17
     */
    sum += htons(17);
    /*
     * the length
     */
    sum += htons(udpLen);
    /*
     * and the IP payload, including the UDP header itself
     */
    while (udpLen > 1) {
        sum += * ip_payload++;
        udpLen -= 2;
    }
    if(udpLen > 0) {
        sum += ((*ip_payload) & htons(0xFF00));

    }
    /*
     * Fold 32-bit sum to 16 bits: add carrier to result
     */
    while (sum>>16) {
          sum = (sum & 0xffff) + (sum >> 16);
    }
    sum = ~sum;
    return ntohs(sum);
}

/****************************************************************************************
 * Testcases start here                                                                 *
 ***************************************************************************************/

/*
 * Testcase 1: initialize a UDP socket and check reference count and operations structure
 */
int testcase1() {
    socket_t socket;
    ASSERT(0 == udp_create_socket(&socket, AF_INET, 0));
    ASSERT(1 == socket.proto.udp.ref_count);
    ASSERT(socket.ops);
    ASSERT(socket.ops->bind);
    ASSERT(socket.ops->close);
    ASSERT(socket.ops->connect);
    ASSERT(socket.ops->listen);
    ASSERT(socket.ops->recv);
    ASSERT(socket.ops->release);
    ASSERT(socket.ops->select);
    ASSERT(socket.ops->send);
    return 0;
}

/*
 * Testcase 2: initialize a UDP socket and call send - should return error as we are not connected
 * yet
 */
int testcase2() {
    unsigned char buffer[256];
    socket_t socket;
    udp_init();
    ASSERT(0 == udp_create_socket(&socket, AF_INET, 0));
    ASSERT(-136 == socket.ops->send(&socket, buffer, 256, 0));
    return 0;
}

/*
 * Testcase 3: initialize a UDP socket and bind it to local address, including a port number
 */
int testcase3() {
    struct sockaddr_in addr;
    unsigned char buffer[256];
    socket_t socket;
    udp_init();
    ASSERT(0 == udp_create_socket(&socket, AF_INET, 0));
    /*
     * Bind socket
     */
    addr.sin_addr.s_addr = inet_addr("10.0.2.20");
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1024);
    ASSERT(0 == socket.ops->bind(&socket, (struct sockaddr*) &addr, sizeof(struct sockaddr_in)));
    /*
     * and verify status and address fields
     */
    ASSERT(inet_addr("10.0.2.20") == ((struct sockaddr_in*) &socket.laddr)->sin_addr.s_addr);
    ASSERT(AF_INET == ((struct sockaddr_in*) &socket.laddr)->sin_family);
    ASSERT(ntohs(1024) == ((struct sockaddr_in*) &socket.laddr)->sin_port);
    /*
     * send should still not be possible
     */
    ASSERT(-136 == socket.ops->send(&socket, buffer, 256, 0));
    return 0;
}

/*
 * Testcase 3: initialize a UDP socket and bind it to local address, including a port number. Then verify that a second
 * bind of another socket to this address fails
 */
int testcase4() {
    struct sockaddr_in addr;
    unsigned char buffer[256];
    socket_t socket1;
    socket_t socket2;
    udp_init();
    ASSERT(0 == udp_create_socket(&socket1, AF_INET, 0));
    ASSERT(0 == udp_create_socket(&socket2, AF_INET, 0));
    /*
     * Bind socket
     */
    addr.sin_addr.s_addr = inet_addr("10.0.2.20");
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1024);
    ASSERT(0 == socket1.ops->bind(&socket1, (struct sockaddr*) &addr, sizeof(struct sockaddr_in)));
    /*
     * and verify status and address fields
     */
    ASSERT(inet_addr("10.0.2.20") == ((struct sockaddr_in*) &socket1.laddr)->sin_addr.s_addr);
    ASSERT(AF_INET == ((struct sockaddr_in*) &socket1.laddr)->sin_family);
    ASSERT(ntohs(1024) == ((struct sockaddr_in*) &socket1.laddr)->sin_port);
    /*
     * Same bind for second socket should fail with -EADDRINUSE
     */
    __net_loglevel = 0;
    ASSERT(-135 == socket1.ops->bind(&socket2, (struct sockaddr*) &addr, sizeof(struct sockaddr_in)));
    __net_loglevel = 0;
    return 0;
}

/*
 * Testcase 5: initialize a UDP socket and bind it to local address, using port number zero. Kernel should
 * chose ephemeral port
 */
int testcase5() {
    struct sockaddr_in addr;
    unsigned char buffer[256];
    socket_t socket;
    udp_init();
    ASSERT(0 == udp_create_socket(&socket, AF_INET, 0));
    /*
     * Bind socket
     */
    addr.sin_addr.s_addr = inet_addr("10.0.2.20");
    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    ASSERT(0 == socket.ops->bind(&socket, (struct sockaddr*) &addr, sizeof(struct sockaddr_in)));
    /*
     * and verify status and address fields
     */
    ASSERT(inet_addr("10.0.2.20") == ((struct sockaddr_in*) &socket.laddr)->sin_addr.s_addr);
    ASSERT(AF_INET == ((struct sockaddr_in*) &socket.laddr)->sin_family);
    ASSERT(ntohs(UDP_EPHEMERAL_PORT) == ((struct sockaddr_in*) &socket.laddr)->sin_port);
    /*
     * send should still not be possible
     */
    ASSERT(-136 == socket.ops->send(&socket, buffer, 256, 0));
    return 0;
}

/*
 * Testcase 6: initialize a UDP socket and bind it to local address, using port number zero. Kernel should
 * chose ephemeral port. Next bind should chose a different port
 */
int testcase6() {
    struct sockaddr_in addr;
    unsigned char buffer[256];
    socket_t socket1;
    socket_t socket2;
    udp_init();
    ASSERT(0 == udp_create_socket(&socket1, AF_INET, 0));
    ASSERT(0 == udp_create_socket(&socket2, AF_INET, 0));
    /*
     * Bind socket
     */
    addr.sin_addr.s_addr = inet_addr("10.0.2.20");
    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    ASSERT(0 == socket1.ops->bind(&socket1, (struct sockaddr*) &addr, sizeof(struct sockaddr_in)));
    ASSERT(0 == socket2.ops->bind(&socket2, (struct sockaddr*) &addr, sizeof(struct sockaddr_in)));
    /*
     * and verify status and address fields
     */
    ASSERT(inet_addr("10.0.2.20") == ((struct sockaddr_in*) &socket1.laddr)->sin_addr.s_addr);
    ASSERT(AF_INET == ((struct sockaddr_in*) &socket1.laddr)->sin_family);
    ASSERT(ntohs(UDP_EPHEMERAL_PORT) == ((struct sockaddr_in*) &socket1.laddr)->sin_port);
    ASSERT(inet_addr("10.0.2.20") == ((struct sockaddr_in*) &socket2.laddr)->sin_addr.s_addr);
    ASSERT(AF_INET == ((struct sockaddr_in*) &socket2.laddr)->sin_family);
    ASSERT(ntohs(UDP_EPHEMERAL_PORT + 1) == ((struct sockaddr_in*) &socket2.laddr)->sin_port);
    return 0;
}


/*
 * Testcase 7: connect a UDP socket and send a packet.
 */
int testcase7() {
    unsigned char buffer[256];
    struct sockaddr_in in_addr;
    int i;
    int rc;
    unsigned short src_port;
    unsigned short dst_port;
    unsigned short udp_len;
    unsigned short chksum;
    socket_t* socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    net_init();
    udp_init();
    ASSERT(0 == udp_create_socket(socket, AF_INET, 0));
    /*
     * Connect socket to 10.0.2.21:30000
     */
    in_addr.sin_addr.s_addr = inet_addr("10.0.2.21");
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = ntohs(30000);
    ASSERT(0 == socket->ops->connect(socket, (struct sockaddr*) &in_addr, sizeof(struct sockaddr_in)));
    /*
     * and verify that socket is connected.
     */
    ASSERT(inet_addr("10.0.2.21") ==   ((struct sockaddr_in*) &socket->faddr)->sin_addr.s_addr);
    ASSERT(ntohs(30000) ==   ((struct sockaddr_in*) &socket->faddr)->sin_port);
    ASSERT(inet_addr("10.0.2.20") == ((struct sockaddr_in*) &socket->laddr)->sin_addr.s_addr);
    ASSERT(ntohs(UDP_EPHEMERAL_PORT) ==   ((struct sockaddr_in*) &socket->laddr)->sin_port);
    /*
     * Prepare UDP payload
     */
    for (i = 0; i < 256; i++)
        buffer[i] = i;
    /*
     * and send it
     */
    __net_loglevel = 0;
    rc = socket->ops->send(socket, buffer, 256, 0);
    __net_loglevel = 0;
    ASSERT(256 == rc);
    /*
     * Now check IP payload. First 8 bytes should be UDP header:
     * 2 bytes source port in network byte order
     * 2 bytes destination port in network byte order
     * 2 bytes length of UDP message, including header
     * 2 bytes checksum
     */
    src_port = payload[0] * 256 + payload[1];
    dst_port = payload[2] * 256 + payload[3];
    ASSERT(UDP_EPHEMERAL_PORT == src_port);
    ASSERT(30000 == dst_port);
    udp_len = payload[4] * 256 + payload[5];
    ASSERT(udp_len == 256 + 8);
    /*
     * Validate checksum
     */
    ASSERT(0 == validate_udp_checksum(udp_len, (unsigned short*) payload, inet_addr("10.0.2.20"), inet_addr("10.0.2.21")));
    free((void*) socket);
    return 0;
}

/*
 * Testcase 8: connect a UDP socket and send a packet such that the maximum IP payload is reached
 */
int testcase8() {
    unsigned char* buffer;
    struct sockaddr_in in_addr;
    int i;
    int rc;
    unsigned short src_port;
    unsigned short dst_port;
    unsigned short udp_len;
    unsigned short chksum;
    socket_t* socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    net_init();
    udp_init();
    ASSERT(0 == udp_create_socket(socket, AF_INET, 0));
    /*
     * Connect socket to 10.0.2.21:30000
     */
    in_addr.sin_addr.s_addr = inet_addr("10.0.2.21");
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = ntohs(30000);
    ASSERT(0 == socket->ops->connect(socket, (struct sockaddr*) &in_addr, sizeof(struct sockaddr_in)));
    /*
     * and verify that socket is connected.
     */
    ASSERT(inet_addr("10.0.2.21") ==   ((struct sockaddr_in*) &socket->faddr)->sin_addr.s_addr);
    ASSERT(ntohs(30000) ==   ((struct sockaddr_in*) &socket->faddr)->sin_port);
    ASSERT(inet_addr("10.0.2.20") == ((struct sockaddr_in*) &socket->laddr)->sin_addr.s_addr);
    ASSERT(ntohs(UDP_EPHEMERAL_PORT) ==   ((struct sockaddr_in*) &socket->laddr)->sin_port);
    /*
     * Prepare UDP payload. As the maximum size of an IP packet is 65535 including header, this is
     * the maximum size for the UDP payload
     */
    buffer = (unsigned char*) malloc(65535 - 20 - 8);
    ASSERT(buffer);
    /*
     * and send it
     */
    __net_loglevel = 0;
    ip_tx_msg_called = 0;
    rc = socket->ops->send(socket, buffer, 65535 - 20 - 8, 0);
    __net_loglevel = 0;
    ASSERT(ip_tx_msg_called);
    ASSERT(65535 - 8 - 20 == rc);
    free((void*) buffer);
    /*
     * Now check IP payload. First 8 bytes should be UDP header:
     * 2 bytes source port in network byte order
     * 2 bytes destination port in network byte order
     * 2 bytes length of UDP message, including header
     * 2 bytes checksum
     */
    src_port = payload[0] * 256 + payload[1];
    dst_port = payload[2] * 256 + payload[3];
    ASSERT(UDP_EPHEMERAL_PORT == src_port);
    ASSERT(30000 == dst_port);
    udp_len = payload[4] * 256 + payload[5];
    ASSERT(udp_len == 65535 - 20);
    free((void*) socket);
    return 0;
}

/*
 * Testcase 9: connect a UDP socket and send a packet such that the maximum IP payload is exceeded
 */
int testcase9() {
    unsigned char* buffer;
    struct sockaddr_in in_addr;
    int i;
    int rc;
    unsigned short src_port;
    unsigned short dst_port;
    unsigned short udp_len;
    unsigned short chksum;
    socket_t* socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    net_init();
    udp_init();
    ASSERT(0 == udp_create_socket(socket, AF_INET, 0));
    /*
     * Connect socket to 10.0.2.21:30000
     */
    in_addr.sin_addr.s_addr = inet_addr("10.0.2.21");
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = ntohs(30000);
    ASSERT(0 == socket->ops->connect(socket, (struct sockaddr*) &in_addr, sizeof(struct sockaddr_in)));
    /*
     * and verify that socket is connected.
     */
    ASSERT(inet_addr("10.0.2.21") ==   ((struct sockaddr_in*) &socket->faddr)->sin_addr.s_addr);
    ASSERT(ntohs(30000) ==   ((struct sockaddr_in*) &socket->faddr)->sin_port);
    ASSERT(inet_addr("10.0.2.20") == ((struct sockaddr_in*) &socket->laddr)->sin_addr.s_addr);
    ASSERT(ntohs(UDP_EPHEMERAL_PORT) ==   ((struct sockaddr_in*) &socket->laddr)->sin_port);
    /*
     * Prepare UDP payload. As the maximum size of an IP packet is 65535 including header, this is
     * the maximum size for the UDP payload plus 1
     */
    buffer = (unsigned char*) malloc(65535 - 20 - 7);
    ASSERT(buffer);
    /*
     * and send it
     */
    __net_loglevel = 0;
    ip_tx_msg_called = 0;
    rc = socket->ops->send(socket, buffer, 65535 - 20 - 7, 0);
    __net_loglevel = 0;
    ASSERT(0 == ip_tx_msg_called);
    ASSERT(-143 == rc);
    free((void*) buffer);
    free((void*) socket);
    return 0;
}

/*
 * Testcase 10: bind a UDP socket to a local address and receive a UDP message
 */
int testcase10() {
    socket_t* socket;
    net_msg_t* net_msg;
    u8* udp_hdr;
    u16 chksum;
    int i;
    u8* data;
    unsigned char buffer[100];
    struct sockaddr_in in_addr;
    net_init();
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    ASSERT(0 == udp_create_socket(socket, SOCK_DGRAM, 0));
    /*
     * Create network message
     */
    net_msg = net_msg_new(256);
    /*
     * fill header
     */
    udp_hdr = (u8*) net_msg_append(net_msg, 8);
    ASSERT(udp_hdr);
    /*
     * First two bytes are source port - we use 30000
     * Next two bytes are destination port - use 1024
     */
    *((u16*)(udp_hdr)) = ntohs(30000);
    *((u16*)(udp_hdr + 2)) = ntohs(1024);
    /*
     * Next two bytes are length including header. As we use 100 data bytes, this is 108
     */
    *((u16*)(udp_hdr + 4)) = ntohs(108);
    /*
     * Set checksum to 0
     */
    *((u16*)(udp_hdr + 6)) = 0;
    /*
     * Prepare data
     */
    data = net_msg_append(net_msg, 100);
    ASSERT(data);
    for (i = 0; i < 100; i++) {
        data[i] = i;
    }
    /*
     * Compute and fill in actual checksum
     */
    chksum = validate_udp_checksum(108, (u16*) udp_hdr, inet_addr("10.0.2.21"), inet_addr("10.0.2.20"));
    *((u16*)(udp_hdr + 6)) = ntohs(chksum);
    /*
     * Set fields expected by udp_rx_msg
     */
    net_msg->udp_hdr = udp_hdr;
    net_msg->ip_length = 108;
    net_msg->ip_src = inet_addr("10.0.2.21");
    net_msg->ip_dest = inet_addr("10.0.2.20");
    /*
     * Now bind socket to local address 10.0.2.20:1024
     */
    in_addr.sin_addr.s_addr = inet_addr("10.0.2.20");
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(1024);
    ASSERT(0 == socket->ops->bind(socket, (struct sockaddr*) &in_addr, sizeof(struct sockaddr_in)));
    /*
     * and receive message
     */
    __net_loglevel = 0;
    udp_rx_msg(net_msg);
    __net_loglevel = 0;
    /*
     * Message should not have been destroyed, but added to sockets receive buffer
     */
    unsigned int created;
    unsigned int destroyed;
    net_get_counters(&created, &destroyed);
    ASSERT(created == destroyed + 1);
    /*
     * We should now have data in receive buffer
     */
    ASSERT(100 == socket->ops->recv(socket, buffer, 100, 0));
    for (i = 0; i < 100; i++)
        ASSERT(buffer[i] == i);
    return 0;
}

/*
 * Testcase 11: bind a UDP socket to a local address and receive a UDP message with checksum 0
 */
int testcase11() {
    socket_t* socket;
    net_msg_t* net_msg;
    u8* udp_hdr;
    u16 chksum;
    int i;
    u8* data;
    unsigned char buffer[100];
    struct sockaddr_in in_addr;
    net_init();
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    ASSERT(0 == udp_create_socket(socket, SOCK_DGRAM, 0));
    /*
     * Create network message
     */
    net_msg = net_msg_new(256);
    /*
     * fill header
     */
    udp_hdr = (u8*) net_msg_append(net_msg, 8);
    ASSERT(udp_hdr);
    /*
     * First two bytes are source port - we use 30000
     * Next two bytes are destination port - use 1024
     */
    *((u16*)(udp_hdr)) = ntohs(30000);
    *((u16*)(udp_hdr + 2)) = ntohs(1024);
    /*
     * Next two bytes are length including header. As we use 100 data bytes, this is 108
     */
    *((u16*)(udp_hdr + 4)) = ntohs(108);
    /*
     * Set checksum to 0
     */
    *((u16*)(udp_hdr + 6)) = 0;
    /*
     * Prepare data
     */
    data = net_msg_append(net_msg, 100);
    ASSERT(data);
    for (i = 0; i < 100; i++) {
        data[i] = i;
    }
    /*
     * Set fields expected by udp_rx_msg
     */
    net_msg->udp_hdr = udp_hdr;
    net_msg->ip_length = 108;
    net_msg->ip_src = inet_addr("10.0.2.21");
    net_msg->ip_dest = inet_addr("10.0.2.20");
    /*
     * Now bind socket to local address 10.0.2.20:1024
     */
    in_addr.sin_addr.s_addr = inet_addr("10.0.2.20");
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(1024);
    ASSERT(0 == socket->ops->bind(socket, (struct sockaddr*) &in_addr, sizeof(struct sockaddr_in)));
    /*
     * and receive message
     */
    __net_loglevel = 0;
    udp_rx_msg(net_msg);
    __net_loglevel = 0;
    /*
     * Message should not have been destroyed, but added to sockets receive buffer
     */
    unsigned int created;
    unsigned int destroyed;
    net_get_counters(&created, &destroyed);
    ASSERT(created == destroyed + 1);
    /*
     * We should now have data in receive buffer
     */
    ASSERT(100 == socket->ops->recv(socket, buffer, 100, 0));
    for (i = 0; i < 100; i++)
        ASSERT(buffer[i] == i);
    return 0;
}

/*
 * Testcase 12: bind a UDP socket to a local address and receive a UDP message destined for a different socket
 */
int testcase12() {
    socket_t* socket;
    net_msg_t* net_msg;
    u8* udp_hdr;
    u16 chksum;
    int i;
    u8* data;
    unsigned char buffer[100];
    struct sockaddr_in in_addr;
    net_init();
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    ASSERT(0 == udp_create_socket(socket, SOCK_DGRAM, 0));
    /*
     * Create network message
     */
    net_msg = net_msg_new(256);
    /*
     * fill header
     */
    udp_hdr = (u8*) net_msg_append(net_msg, 8);
    ASSERT(udp_hdr);
    /*
     * First two bytes are source port - we use 30000
     * Next two bytes are destination port - use 1024
     */
    *((u16*)(udp_hdr)) = ntohs(30000);
    *((u16*)(udp_hdr + 2)) = ntohs(1024);
    /*
     * Next two bytes are length including header. As we use 100 data bytes, this is 108
     */
    *((u16*)(udp_hdr + 4)) = ntohs(108);
    /*
     * Set checksum to 0
     */
    *((u16*)(udp_hdr + 6)) = 0;
    /*
     * Prepare data
     */
    data = net_msg_append(net_msg, 100);
    ASSERT(data);
    for (i = 0; i < 100; i++) {
        data[i] = i;
    }
    /*
     * Set fields expected by udp_rx_msg
     */
    net_msg->udp_hdr = udp_hdr;
    net_msg->ip_length = 108;
    net_msg->ip_src = inet_addr("10.0.2.21");
    net_msg->ip_dest = inet_addr("10.0.2.20");
    /*
     * Now bind socket to local address 10.0.2.20:1023
     */
    in_addr.sin_addr.s_addr = inet_addr("10.0.2.20");
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(1023);
    ASSERT(0 == socket->ops->bind(socket, (struct sockaddr*) &in_addr, sizeof(struct sockaddr_in)));
    /*
     * and receive message
     */
    __net_loglevel = 0;
    icmp_error_sent = 0;
    udp_rx_msg(net_msg);
    __net_loglevel = 0;
    /*
     * Message should have been destroyed as there is no socket listening on port 1023
     */
    unsigned int created;
    unsigned int destroyed;
    net_get_counters(&created, &destroyed);
    ASSERT(created == destroyed);
    /*
     * and we should have created an ICMP error
     */
    ASSERT(1 == icmp_error_sent);
    /*
     * We should not have any data in receive buffer, recv should return EAGAIN
     */
    ASSERT(-106 == socket->ops->recv(socket, buffer, 100, 0));
    return 0;
}

/*
 * Testcase 13 bind a UDP socket to a local address and receive two messages
 */
int testcase13() {
    socket_t* socket;
    net_msg_t* net_msg;
    u8* udp_hdr;
    u16 chksum;
    int i;
    int msg;
    u8* data;
    unsigned char buffer[100];
    struct sockaddr_in in_addr;
    net_init();
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    ASSERT(0 == udp_create_socket(socket, SOCK_DGRAM, 0));
    /*
     * Now bind socket to local address 10.0.2.20:1024
     */
    in_addr.sin_addr.s_addr = inet_addr("10.0.2.20");
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(1024);
    ASSERT(0 == socket->ops->bind(socket, (struct sockaddr*) &in_addr, sizeof(struct sockaddr_in)));
    /*
     * Prepare messages
     */
    for (msg = 0; msg < 2; msg++) {
        /*
         * Create network message
         */
        net_msg = net_msg_new(256);
        /*
         * fill header
         */
        udp_hdr = (u8*) net_msg_append(net_msg, 8);
        ASSERT(udp_hdr);
        /*
         * First two bytes are source port - we use 30000
         * Next two bytes are destination port - use 1024
         */
        *((u16*)(udp_hdr)) = ntohs(30000);
        *((u16*)(udp_hdr + 2)) = ntohs(1024);
        /*
         * Next two bytes are length including header. As we use 100 data bytes, this is 108
         */
        *((u16*)(udp_hdr + 4)) = ntohs(108);
        /*
         * Set checksum to 0
         */
        *((u16*)(udp_hdr + 6)) = 0;
        /*
         * Prepare data
         */
        data = net_msg_append(net_msg, 100);
        ASSERT(data);
        for (i = 0; i < 100; i++) {
            data[i] = i + msg;
        }
        /*
         * Compute and fill in actual checksum
         */
        chksum = validate_udp_checksum(108, (u16*) udp_hdr, inet_addr("10.0.2.21"), inet_addr("10.0.2.20"));
        *((u16*)(udp_hdr + 6)) = ntohs(chksum);
        /*
         * Set fields expected by udp_rx_msg
         */
        net_msg->udp_hdr = udp_hdr;
        net_msg->ip_length = 108;
        net_msg->ip_src = inet_addr("10.0.2.21");
        net_msg->ip_dest = inet_addr("10.0.2.20");
        /*
         * and receive message
         */
        __net_loglevel = 0;
        udp_rx_msg(net_msg);
        __net_loglevel = 0;
    }
    /*
     * Messages should not have been destroyed, but added to sockets receive buffer
     */
    unsigned int created;
    unsigned int destroyed;
    net_get_counters(&created, &destroyed);
    ASSERT(created == destroyed + 2);
    /*
     * We should now have data in receive buffer
     */
    ASSERT(100 == socket->ops->recv(socket, buffer, 100, 0));
    for (i = 0; i < 100; i++)
        ASSERT(buffer[i] == i);
    /*
     * Second read should return second message
     */
    ASSERT(100 == socket->ops->recv(socket, buffer, 100, 0));
    for (i = 0; i < 100; i++)
        ASSERT(buffer[i] == i + 1);
    /*
     * final read should return -106 (EAGAIN)
     */
    ASSERT(-106 == socket->ops->recv(socket, buffer, 100, 0));
    /*
     * all messages should be gone now
     */
    net_get_counters(&created, &destroyed);
    ASSERT(created == destroyed);
    return 0;
}

/*
 * Testcase 14: bind a UDP socket to a local address and receive a UDP message. Verify that a partial read destroys
 * message as well
 */
int testcase14() {
    socket_t* socket;
    net_msg_t* net_msg;
    u8* udp_hdr;
    u16 chksum;
    int i;
    u8* data;
    unsigned char buffer[100];
    struct sockaddr_in in_addr;
    net_init();
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    ASSERT(0 == udp_create_socket(socket, SOCK_DGRAM, 0));
    /*
     * Create network message
     */
    net_msg = net_msg_new(256);
    /*
     * fill header
     */
    udp_hdr = (u8*) net_msg_append(net_msg, 8);
    ASSERT(udp_hdr);
    /*
     * First two bytes are source port - we use 30000
     * Next two bytes are destination port - use 1024
     */
    *((u16*)(udp_hdr)) = ntohs(30000);
    *((u16*)(udp_hdr + 2)) = ntohs(1024);
    /*
     * Next two bytes are length including header. As we use 100 data bytes, this is 108
     */
    *((u16*)(udp_hdr + 4)) = ntohs(108);
    /*
     * Set checksum to 0
     */
    *((u16*)(udp_hdr + 6)) = 0;
    /*
     * Prepare data
     */
    data = net_msg_append(net_msg, 100);
    ASSERT(data);
    for (i = 0; i < 100; i++) {
        data[i] = i;
    }
    /*
     * Compute and fill in actual checksum
     */
    chksum = validate_udp_checksum(108, (u16*) udp_hdr, inet_addr("10.0.2.21"), inet_addr("10.0.2.20"));
    *((u16*)(udp_hdr + 6)) = ntohs(chksum);
    /*
     * Set fields expected by udp_rx_msg
     */
    net_msg->udp_hdr = udp_hdr;
    net_msg->ip_length = 108;
    net_msg->ip_src = inet_addr("10.0.2.21");
    net_msg->ip_dest = inet_addr("10.0.2.20");
    /*
     * Now bind socket to local address 10.0.2.20:1024
     */
    in_addr.sin_addr.s_addr = inet_addr("10.0.2.20");
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(1024);
    ASSERT(0 == socket->ops->bind(socket, (struct sockaddr*) &in_addr, sizeof(struct sockaddr_in)));
    /*
     * and receive message
     */
    __net_loglevel = 0;
    udp_rx_msg(net_msg);
    __net_loglevel = 0;
    /*
     * Message should not have been destroyed, but added to sockets receive buffer
     */
    unsigned int created;
    unsigned int destroyed;
    net_get_counters(&created, &destroyed);
    ASSERT(created == destroyed + 1);
    /*
     * We should now have data in receive buffer, and a partial read should return first bytes
     */
    ASSERT(10 == socket->ops->recv(socket, buffer, 10, 0));
    for (i = 0; i < 10; i++)
        ASSERT(buffer[i] == i);
    /*
     * Message should have been destroyed
     */
    net_get_counters(&created, &destroyed);
    ASSERT(created == destroyed);
    /*
     * and next read should return -106 (EAGAIN)
     */
    ASSERT(-106 == socket->ops->recv(socket, buffer, 10, 0));
    return 0;
}

/*
 * Testcase 15: call select on a UDP socket. We should be able to write,
 * but not be able to read
 */
int testcase15() {
    socket_t* socket;
    net_init();
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    ASSERT(0 == udp_create_socket(socket, SOCK_DGRAM, 0));
    ASSERT(2 == socket->ops->select(socket, 0, 1));
    ASSERT(2 == socket->ops->select(socket, 1, 1));
    ASSERT(0 == socket->ops->select(socket, 1, 0));
    ASSERT(0 == socket->ops->select(socket, 0, 0));
    return 0;
}

/*
 * Testcase 16: bind a UDP socket to a local address and receive a UDP message. Verify that select returns correct result
 */
int testcase16() {
    socket_t* socket;
    net_msg_t* net_msg;
    u8* udp_hdr;
    u16 chksum;
    int i;
    u8* data;
    unsigned char buffer[100];
    struct sockaddr_in in_addr;
    net_init();
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    ASSERT(0 == udp_create_socket(socket, SOCK_DGRAM, 0));
    /*
     * Create network message
     */
    net_msg = net_msg_new(256);
    /*
     * fill header
     */
    udp_hdr = (u8*) net_msg_append(net_msg, 8);
    ASSERT(udp_hdr);
    /*
     * First two bytes are source port - we use 30000
     * Next two bytes are destination port - use 1024
     */
    *((u16*)(udp_hdr)) = ntohs(30000);
    *((u16*)(udp_hdr + 2)) = ntohs(1024);
    /*
     * Next two bytes are length including header. As we use 100 data bytes, this is 108
     */
    *((u16*)(udp_hdr + 4)) = ntohs(108);
    /*
     * Set checksum to 0
     */
    *((u16*)(udp_hdr + 6)) = 0;
    /*
     * Prepare data
     */
    data = net_msg_append(net_msg, 100);
    ASSERT(data);
    for (i = 0; i < 100; i++) {
        data[i] = i;
    }
    /*
     * Compute and fill in actual checksum
     */
    chksum = validate_udp_checksum(108, (u16*) udp_hdr, inet_addr("10.0.2.21"), inet_addr("10.0.2.20"));
    *((u16*)(udp_hdr + 6)) = ntohs(chksum);
    /*
     * Set fields expected by udp_rx_msg
     */
    net_msg->udp_hdr = udp_hdr;
    net_msg->ip_length = 108;
    net_msg->ip_src = inet_addr("10.0.2.21");
    net_msg->ip_dest = inet_addr("10.0.2.20");
    /*
     * Now bind socket to local address 10.0.2.20:1024
     */
    in_addr.sin_addr.s_addr = inet_addr("10.0.2.20");
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(1024);
    ASSERT(0 == socket->ops->bind(socket, (struct sockaddr*) &in_addr, sizeof(struct sockaddr_in)));
    /*
     * and receive message
     */
    __net_loglevel = 0;
    udp_rx_msg(net_msg);
    __net_loglevel = 0;
    /*
     * Message should not have been destroyed, but added to sockets receive buffer
     */
    unsigned int created;
    unsigned int destroyed;
    net_get_counters(&created, &destroyed);
    ASSERT(created == destroyed + 1);
    /*
     * Now check results of select - should be able to read and write
     */
    ASSERT(2 == socket->ops->select(socket, 0, 1));
    ASSERT(3 == socket->ops->select(socket, 1, 1));
    ASSERT(1 == socket->ops->select(socket, 1, 0));
    ASSERT(0 == socket->ops->select(socket, 0, 0));
    return 0;
}

/*
 * Testcase 17: initialize a UDP socket and bind it to local address. Verify that next bind for the same address fails
 * unless the socket is closed
 */
int testcase17() {
    struct sockaddr_in addr;
    unsigned char buffer[256];
    socket_t* socket1;
    socket_t* socket2;
    socket1 = (socket_t*) malloc(sizeof(socket_t));
    socket2 = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket1);
    ASSERT(socket2);
    udp_init();
    ASSERT(0 == udp_create_socket(socket1, AF_INET, 0));
    ASSERT(0 == udp_create_socket(socket2, AF_INET, 0));
    /*
     * Bind sockets - second call should fail with EADDRINUSE (135)
     */
    addr.sin_addr.s_addr = inet_addr("10.0.2.20");
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1024);
    ASSERT(0 == socket1->ops->bind(socket1, (struct sockaddr*) &addr, sizeof(struct sockaddr_in)));
    ASSERT( -135 == socket2->ops->bind(socket2, (struct sockaddr*) &addr, sizeof(struct sockaddr_in)));
    /*
     * Now close first socket and verify that second socket can be bound now
     */
    net_socket_close(socket1);
    ASSERT(0 == socket2->ops->bind(socket2, (struct sockaddr*) &addr, sizeof(struct sockaddr_in)));
    return 0;
}

/*
 * Testcase 18: send a packet on a socket which is not connected nor bound using sendto. In this case, the UDP layer will
 * query the IP layer for the source address to use based on a route to the destination
 */
int testcase18() {
    unsigned char buffer[256];
    struct sockaddr_in in_addr;
    int i;
    int rc;
    unsigned short src_port;
    unsigned short dst_port;
    unsigned short udp_len;
    unsigned short chksum;
    socket_t* socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    net_init();
    udp_init();
    ASSERT(0 == udp_create_socket(socket, AF_INET, 0));
    in_addr.sin_addr.s_addr = inet_addr("10.0.2.21");
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = ntohs(30000);
    /*
     * Prepare UDP payload
     */
    for (i = 0; i < 256; i++)
        buffer[i] = i;
    /*
     * and send it
     */
    __net_loglevel = 0;
    rc = socket->ops->sendto(socket, buffer, 256, 0, (struct sockaddr*) &in_addr, sizeof(struct sockaddr_in));
    __net_loglevel = 0;
    ASSERT(256 == rc);
    /*
     * Now check IP payload. First 8 bytes should be UDP header:
     * 2 bytes source port in network byte order
     * 2 bytes destination port in network byte order
     * 2 bytes length of UDP message, including header
     * 2 bytes checksum
     */
    src_port = payload[0] * 256 + payload[1];
    dst_port = payload[2] * 256 + payload[3];
    ASSERT(UDP_EPHEMERAL_PORT == src_port);
    ASSERT(30000 == dst_port);
    udp_len = payload[4] * 256 + payload[5];
    ASSERT(udp_len == 256 + 8);
    /*
     * Validate checksum
     */
    ASSERT(0 == validate_udp_checksum(udp_len, (unsigned short*) payload, inet_addr("10.0.2.20"), inet_addr("10.0.2.21")));
    free((void*) socket);
    return 0;
}

/*
 * Testcase 19: connect a UDP socket and try to invoke sendto with a destination address
 */
int testcase19() {
    unsigned char buffer[256];
    struct sockaddr_in in_addr;
    int i;
    int rc;
    unsigned short src_port;
    unsigned short dst_port;
    unsigned short udp_len;
    unsigned short chksum;
    socket_t* socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    net_init();
    udp_init();
    ASSERT(0 == udp_create_socket(socket, AF_INET, 0));
    /*
     * Connect socket to 10.0.2.21:30000
     */
    in_addr.sin_addr.s_addr = inet_addr("10.0.2.21");
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = ntohs(30000);
    ASSERT(0 == socket->ops->connect(socket, (struct sockaddr*) &in_addr, sizeof(struct sockaddr_in)));
    /*
     * and verify that socket is connected.
     */
    ASSERT(inet_addr("10.0.2.21") ==   ((struct sockaddr_in*) &socket->faddr)->sin_addr.s_addr);
    ASSERT(ntohs(30000) ==   ((struct sockaddr_in*) &socket->faddr)->sin_port);
    ASSERT(inet_addr("10.0.2.20") == ((struct sockaddr_in*) &socket->laddr)->sin_addr.s_addr);
    ASSERT(ntohs(UDP_EPHEMERAL_PORT) ==   ((struct sockaddr_in*) &socket->laddr)->sin_port);
    /*
     * sendto should return -EISCONN (145)
     */
    __net_loglevel = 0;
    rc = socket->ops->sendto(socket, buffer, 256, 0, (struct sockaddr*) &in_addr, sizeof(struct sockaddr_in));
    __net_loglevel = 0;
    ASSERT(-145 == rc);
    free((void*) socket);
    return 0;
}

/*
 * Testcase 20: send a packet on a socket which is not connected but bound using sendto.
 */
int testcase20() {
    unsigned char buffer[256];
    struct sockaddr_in in_addr;
    int i;
    int rc;
    unsigned short src_port;
    unsigned short dst_port;
    unsigned short udp_len;
    unsigned short chksum;
    socket_t* socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    net_init();
    udp_init();
    ASSERT(0 == udp_create_socket(socket, AF_INET, 0));
    /*
     * Bind socket
     */
    in_addr.sin_addr.s_addr = inet_addr("10.0.2.20");
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = ntohs(1024);
    ASSERT(0 == socket->ops->bind(socket, (struct sockaddr*) &in_addr, sizeof(struct sockaddr_in)));
    /*
     * Fill structure with destination address
     */
    in_addr.sin_addr.s_addr = inet_addr("10.0.2.21");
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = ntohs(30000);
    /*
     * Prepare UDP payload
     */
    for (i = 0; i < 256; i++)
        buffer[i] = i;
    /*
     * and send it
     */
    __net_loglevel = 0;
    rc = socket->ops->sendto(socket, buffer, 256, 0, (struct sockaddr*) &in_addr, sizeof(struct sockaddr_in));
    __net_loglevel = 0;
    ASSERT(256 == rc);
    /*
     * Now check IP payload. First 8 bytes should be UDP header:
     * 2 bytes source port in network byte order
     * 2 bytes destination port in network byte order
     * 2 bytes length of UDP message, including header
     * 2 bytes checksum
     */
    src_port = payload[0] * 256 + payload[1];
    dst_port = payload[2] * 256 + payload[3];
    ASSERT(1024 == src_port);
    ASSERT(30000 == dst_port);
    udp_len = payload[4] * 256 + payload[5];
    ASSERT(udp_len == 256 + 8);
    /*
     * Validate checksum
     */
    ASSERT(0 == validate_udp_checksum(udp_len, (unsigned short*) payload, inet_addr("10.0.2.20"), inet_addr("10.0.2.21")));
    free((void*) socket);
    return 0;
}

/*
 * Testcase 21: bind a UDP socket to a local address and receive a UDP message. Verify that recvfrom returns data and
 * address
 */
int testcase21() {
    socket_t* socket;
    net_msg_t* net_msg;
    u8* udp_hdr;
    u16 chksum;
    int i;
    u8* data;
    unsigned char buffer[100];
    struct sockaddr_in in_addr;
    struct sockaddr_in msg_addr;
    int addrlen;
    net_init();
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    ASSERT(0 == udp_create_socket(socket, SOCK_DGRAM, 0));
    /*
     * Create network message
     */
    net_msg = net_msg_new(256);
    /*
     * fill header
     */
    udp_hdr = (u8*) net_msg_append(net_msg, 8);
    ASSERT(udp_hdr);
    /*
     * First two bytes are source port - we use 30000
     * Next two bytes are destination port - use 1024
     */
    *((u16*)(udp_hdr)) = ntohs(30000);
    *((u16*)(udp_hdr + 2)) = ntohs(1024);
    /*
     * Next two bytes are length including header. As we use 100 data bytes, this is 108
     */
    *((u16*)(udp_hdr + 4)) = ntohs(108);
    /*
     * Set checksum to 0
     */
    *((u16*)(udp_hdr + 6)) = 0;
    /*
     * Prepare data
     */
    data = net_msg_append(net_msg, 100);
    ASSERT(data);
    for (i = 0; i < 100; i++) {
        data[i] = i;
    }
    /*
     * Compute and fill in actual checksum
     */
    chksum = validate_udp_checksum(108, (u16*) udp_hdr, inet_addr("10.0.2.21"), inet_addr("10.0.2.20"));
    *((u16*)(udp_hdr + 6)) = ntohs(chksum);
    /*
     * Set fields expected by udp_rx_msg
     */
    net_msg->udp_hdr = udp_hdr;
    net_msg->ip_length = 108;
    net_msg->ip_src = inet_addr("10.0.2.21");
    net_msg->ip_dest = inet_addr("10.0.2.20");
    /*
     * Now bind socket to local address 10.0.2.20:1024
     */
    in_addr.sin_addr.s_addr = inet_addr("10.0.2.20");
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(1024);
    ASSERT(0 == socket->ops->bind(socket, (struct sockaddr*) &in_addr, sizeof(struct sockaddr_in)));
    /*
     * and receive message
     */
    __net_loglevel = 0;
    udp_rx_msg(net_msg);
    __net_loglevel = 0;
    /*
     * Message should not have been destroyed, but added to sockets receive buffer
     */
    unsigned int created;
    unsigned int destroyed;
    net_get_counters(&created, &destroyed);
    ASSERT(created == destroyed + 1);
    /*
     * We should now have data in receive buffer
     */
    addrlen = sizeof(struct sockaddr_in);
    ASSERT(100 == socket->ops->recvfrom(socket, buffer, 100, 0, (struct sockaddr*) &msg_addr, &addrlen));
    for (i = 0; i < 100; i++)
        ASSERT(buffer[i] == i);
    /*
     * Check address
     */
    ASSERT(sizeof(struct sockaddr_in) == addrlen);
    ASSERT(msg_addr.sin_family == AF_INET);
    ASSERT(msg_addr.sin_port = ntohs(30000));
    ASSERT(msg_addr.sin_addr.s_addr == inet_addr("10.0.2.21"));
    /*
     * Message should have been destroyed
     */
    net_get_counters(&created, &destroyed);
    ASSERT(created == destroyed);
    /*
     * and next read should return -106 (EAGAIN)
     */
    ASSERT(-106 == socket->ops->recv(socket, buffer, 10, 0));
    return 0;
}

/*
 * Testcase 22: bind a UDP socket to a local address and receive a UDP message using an address buffer which is larger
 * than required
 */
int testcase22() {
    socket_t* socket;
    net_msg_t* net_msg;
    u8* udp_hdr;
    u16 chksum;
    int i;
    u8* data;
    unsigned char buffer[100];
    struct sockaddr_in in_addr;
    struct sockaddr_in* msg_addr;
    unsigned char addr_buffer[sizeof(struct sockaddr_in) + 16];
    int addrlen;
    net_init();
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    ASSERT(0 == udp_create_socket(socket, SOCK_DGRAM, 0));
    /*
     * Create network message
     */
    net_msg = net_msg_new(256);
    /*
     * fill header
     */
    udp_hdr = (u8*) net_msg_append(net_msg, 8);
    ASSERT(udp_hdr);
    /*
     * First two bytes are source port - we use 30001
     * Next two bytes are destination port - use 1024
     */
    *((u16*)(udp_hdr)) = ntohs(30001);
    *((u16*)(udp_hdr + 2)) = ntohs(1024);
    /*
     * Next two bytes are length including header. As we use 100 data bytes, this is 108
     */
    *((u16*)(udp_hdr + 4)) = ntohs(108);
    /*
     * Set checksum to 0
     */
    *((u16*)(udp_hdr + 6)) = 0;
    /*
     * Prepare data
     */
    data = net_msg_append(net_msg, 100);
    ASSERT(data);
    for (i = 0; i < 100; i++) {
        data[i] = i;
    }
    /*
     * Compute and fill in actual checksum
     */
    chksum = validate_udp_checksum(108, (u16*) udp_hdr, inet_addr("10.0.2.21"), inet_addr("10.0.2.20"));
    *((u16*)(udp_hdr + 6)) = ntohs(chksum);
    /*
     * Set fields expected by udp_rx_msg
     */
    net_msg->udp_hdr = udp_hdr;
    net_msg->ip_length = 108;
    net_msg->ip_src = inet_addr("10.0.2.21");
    net_msg->ip_dest = inet_addr("10.0.2.20");
    /*
     * Now bind socket to local address 10.0.2.20:1024
     */
    in_addr.sin_addr.s_addr = inet_addr("10.0.2.20");
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(1024);
    ASSERT(0 == socket->ops->bind(socket, (struct sockaddr*) &in_addr, sizeof(struct sockaddr_in)));
    /*
     * and receive message
     */
    __net_loglevel = 0;
    udp_rx_msg(net_msg);
    __net_loglevel = 0;
    /*
     * Message should not have been destroyed, but added to sockets receive buffer
     */
    unsigned int created;
    unsigned int destroyed;
    net_get_counters(&created, &destroyed);
    ASSERT(created == destroyed + 1);
    /*
     * We should now have data in receive buffer
     */
    addrlen = sizeof(struct sockaddr_in)  + 16;
    ASSERT(100 == socket->ops->recvfrom(socket, buffer, 100, 0, (struct sockaddr*) addr_buffer, &addrlen));
    for (i = 0; i < 100; i++)
        ASSERT(buffer[i] == i);
    /*
     * Check address length
     */
    ASSERT(sizeof(struct sockaddr_in) == addrlen);
    msg_addr = (struct sockaddr_in*) addr_buffer;
    ASSERT(msg_addr->sin_family == AF_INET);
    ASSERT(msg_addr->sin_port = ntohs(30001));
    ASSERT(msg_addr->sin_addr.s_addr == inet_addr("10.0.2.21"));
    /*
     * Message should have been destroyed
     */
    net_get_counters(&created, &destroyed);
    ASSERT(created == destroyed);
    /*
     * and next read should return -106 (EAGAIN)
     */
    ASSERT(-106 == socket->ops->recv(socket, buffer, 10, 0));
    return 0;
}

/*
 * Testcase 23: bind a UDP socket to a local address and receive a UDP message using an address buffer which is smaller
 * than required
 */
int testcase23() {
    socket_t* socket;
    net_msg_t* net_msg;
    u8* udp_hdr;
    u16 chksum;
    int i;
    u8* data;
    unsigned char buffer[100];
    struct sockaddr_in in_addr;
    int addrlen;
    unsigned char addr_buffer[sizeof(struct sockaddr_in) - 4];
    net_init();
    socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    ASSERT(0 == udp_create_socket(socket, SOCK_DGRAM, 0));
    /*
     * Create network message
     */
    net_msg = net_msg_new(256);
    /*
     * fill header
     */
    udp_hdr = (u8*) net_msg_append(net_msg, 8);
    ASSERT(udp_hdr);
    /*
     * First two bytes are source port - we use 30001
     * Next two bytes are destination port - use 1024
     */
    *((u16*)(udp_hdr)) = ntohs(30001);
    *((u16*)(udp_hdr + 2)) = ntohs(1024);
    /*
     * Next two bytes are length including header. As we use 100 data bytes, this is 108
     */
    *((u16*)(udp_hdr + 4)) = ntohs(108);
    /*
     * Set checksum to 0
     */
    *((u16*)(udp_hdr + 6)) = 0;
    /*
     * Prepare data
     */
    data = net_msg_append(net_msg, 100);
    ASSERT(data);
    for (i = 0; i < 100; i++) {
        data[i] = i;
    }
    /*
     * Compute and fill in actual checksum
     */
    chksum = validate_udp_checksum(108, (u16*) udp_hdr, inet_addr("10.0.2.21"), inet_addr("10.0.2.20"));
    *((u16*)(udp_hdr + 6)) = ntohs(chksum);
    /*
     * Set fields expected by udp_rx_msg
     */
    net_msg->udp_hdr = udp_hdr;
    net_msg->ip_length = 108;
    net_msg->ip_src = inet_addr("10.0.2.21");
    net_msg->ip_dest = inet_addr("10.0.2.20");
    /*
     * Now bind socket to local address 10.0.2.20:1024
     */
    in_addr.sin_addr.s_addr = inet_addr("10.0.2.20");
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(1024);
    ASSERT(0 == socket->ops->bind(socket, (struct sockaddr*) &in_addr, sizeof(struct sockaddr_in)));
    /*
     * and receive message
     */
    __net_loglevel = 0;
    udp_rx_msg(net_msg);
    __net_loglevel = 0;
    /*
     * Message should not have been destroyed, but added to sockets receive buffer
     */
    unsigned int created;
    unsigned int destroyed;
    net_get_counters(&created, &destroyed);
    ASSERT(created == destroyed + 1);
    /*
     * We should now have data in receive buffer
     */
    addrlen = sizeof(struct sockaddr_in) - 4;
    __net_loglevel = 0;
    ASSERT(100 == socket->ops->recvfrom(socket, buffer, 100, 0, (struct sockaddr*) addr_buffer, &addrlen));
    __net_loglevel = 0;
    for (i = 0; i < 100; i++)
        ASSERT(buffer[i] == i);
    /*
     * Check address length
     */
    ASSERT(sizeof(struct sockaddr_in) == addrlen);
    /*
     * Message should have been destroyed
     */
    net_get_counters(&created, &destroyed);
    ASSERT(created == destroyed);
    /*
     * and next read should return -106 (EAGAIN)
     */
    __net_loglevel = 0;
    socket->ops->recv(socket, buffer, 10, 0);
    __net_loglevel = 0;
    return 0;
}

/*
 * Testcase 24: send a packet on a socket which is not connected but bound using sendto - use wildcards for bound
 */
int testcase24() {
    unsigned char buffer[256];
    struct sockaddr_in in_addr;
    int i;
    int rc;
    unsigned short src_port;
    unsigned short dst_port;
    unsigned short udp_len;
    unsigned short chksum;
    socket_t* socket = (socket_t*) malloc(sizeof(socket_t));
    ASSERT(socket);
    net_init();
    udp_init();
    ASSERT(0 == udp_create_socket(socket, AF_INET, 0));
    /*
     * Bind socket
     */
    in_addr.sin_addr.s_addr = 0;
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = 0;
    ASSERT(0 == socket->ops->bind(socket, (struct sockaddr*) &in_addr, sizeof(struct sockaddr_in)));
    /*
     * Fill structure with destination address
     */
    in_addr.sin_addr.s_addr = inet_addr("10.0.2.21");
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = ntohs(30000);
    /*
     * Prepare UDP payload
     */
    for (i = 0; i < 256; i++)
        buffer[i] = i;
    /*
     * and send it
     */
    __net_loglevel = 0;
    rc = socket->ops->sendto(socket, buffer, 256, 0, (struct sockaddr*) &in_addr, sizeof(struct sockaddr_in));
    __net_loglevel = 0;
    ASSERT(256 == rc);
    /*
     * Now check IP payload. First 8 bytes should be UDP header:
     * 2 bytes source port in network byte order
     * 2 bytes destination port in network byte order
     * 2 bytes length of UDP message, including header
     * 2 bytes checksum
     */
    src_port = payload[0] * 256 + payload[1];
    dst_port = payload[2] * 256 + payload[3];
    ASSERT(UDP_EPHEMERAL_PORT == src_port);
    ASSERT(30000 == dst_port);
    udp_len = payload[4] * 256 + payload[5];
    ASSERT(udp_len == 256 + 8);
    /*
     * Validate checksum
     */
    ASSERT(0 == validate_udp_checksum(udp_len, (unsigned short*) payload, inet_addr("10.0.2.20"), inet_addr("10.0.2.21")));
    free((void*) socket);
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
    RUN_CASE(16);
    RUN_CASE(17);
    RUN_CASE(18);
    RUN_CASE(19);
    RUN_CASE(20);
    RUN_CASE(21);
    RUN_CASE(22);
    RUN_CASE(23);
    RUN_CASE(24);
    END;
}

