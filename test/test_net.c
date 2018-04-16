/*
 * test_net.c
 *
 */

#include "kunit.h"
#include "net.h"
#include "vga.h"
#include "lib/os/route.h"
#include "lib/limits.h"

extern int __net_loglevel;

static socket_ops_t ip_ops;

/*
 * Stubs
 */
static int do_putchar = 1;
void win_putchar(win_t* win, u8 c) {
    if (do_putchar)
        printf("%c", c);
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


void trap() {
    printf("------- PANIC!!! --------------\n");
    _exit(1);
}

void arp_init() {

}

void ip_init() {

}

void udp_init() {

}

int params_get_int(char* param) {
    return 0;
}

void net_if_init() {

}

int net_if_get_ifconf(struct ifconf* ifc) {
    return -1;
}

int net_if_get_addr(struct ifreq* ifr) {
    return 0;
}

int net_if_get_netmask(struct ifreq* ifr) {
    return 0;
}


int do_kill(int pid, int sig_no) {
    return 0;
}

int pm_get_pid() {
    return 1;
}

void tcp_init() {

}

void spinlock_get(spinlock_t* lock, u32* flags) {
}
void spinlock_release(spinlock_t* lock, u32* flags) {
}
void spinlock_init(spinlock_t* lock) {
}

void cond_init(cond_t* cond) {

}

int cond_wait_intr(cond_t* cond, spinlock_t* lock, u32* eflags) {
    spinlock_release(lock, eflags);
    return -1;
}


int cond_wait_intr_timed(cond_t* cond, spinlock_t* lock, u32* eflags, unsigned int timeout) {
    spinlock_release(lock, eflags);
    return -1;
}

void cond_broadcast(cond_t* cond) {

}

void sem_up(semaphore_t* sem) {

}

void atomic_incr(u32* reg) {
    (*reg)++;
}

u32 kmalloc(u32 size) {
    return (u32) malloc(size);
}

void kfree(void* ptr) {
    free(ptr);
}

int mm_validate_buffer(u32 buffer, u32 len, int rw) {
    return 0;
}

/*
 * Stubs for IP socket operations
 */
static int ip_socket_send_rc = 0;
static void (*ip_send_stub)() = 0;
static int ip_socket_send(socket_t* socket, void* buffer, unsigned int len, int flags) {
    if (ip_send_stub)
        ip_send_stub();
    return ip_socket_send_rc;
}

static int ip_socket_recv_rc = 0;
static int ip_socket_recv(socket_t* socket, void* buffer, unsigned int len, int flags) {
    return ip_socket_recv_rc;
}



static int ip_socket_connect(socket_t* socket, struct sockaddr* addr, int addrlen) {
    struct sockaddr_in* laddr;
    /*
     * Set local address if the socket is not yet bound
     */
    if (0 == socket->bound) {
        laddr = (struct sockaddr_in*) &socket->laddr;
        laddr->sin_addr.s_addr = inet_addr("10.0.2.20");
        laddr->sin_family = AF_INET;
        laddr->sin_port = 0;
        socket->bound = 1;
    }
    /*
     * Set foreign address
     */
    socket->faddr = *addr;
    socket->connected = 1;
    return 0;
}

void ip_create_socket(socket_t* socket, int domain, int proto) {
    ip_ops.bind = 0;
    ip_ops.send = ip_socket_send;
    ip_ops.recv = ip_socket_recv;
    ip_ops.connect = ip_socket_connect;
    socket->ops = &ip_ops;
}

int ip_get_rtconf(struct rtconf* rtc) {
    return 0;
}

void tcp_create_socket(socket_t* socket, int domain, int proto) {
}

int net_if_set_addr(struct ifreq* ifr) {
    return 0;
}

int ip_add_route(struct rtentry* rt_entry) {
    return 0;
}

int net_if_set_netmask(struct ifreq* ifr) {
    return 0;
}

int ip_del_route(struct rtentry* rt_entry) {
    return 0;
}

int udp_create_socket(socket_t* socket, int type, int proto) {
    return 0;
}

/*
 * Testcase 1:
 * Convert an IP address into a 32-bit number in network byte order
 *
 */
int testcase1() {
    u32 ip;
    ip = net_str2ip("10.0.2.20");
    ASSERT(ip == 0x1402000a);
    return 0;
}

/*
 * Testcase 1:
 * Convert an IP address into a 32-bit number in network byte order
 * - use hex notation
 *
 */
int testcase2() {
    u32 ip;
    ip = net_str2ip("0xa.0.2.20");
    ASSERT(ip == 0x1402000a);
    return 0;
}

/*
 * Testcase 3
 * Create a network message
 */
int testcase3() {
    net_msg_t* net_msg;
    net_msg = net_msg_create(256, 32);
    ASSERT(net_msg);
    return 0;
}

/*
 * Testcase 4
 * Create a network message with standard headroom and destroy it
 */
int testcase4() {
    net_msg_t* net_msg;
    net_msg = net_msg_new(256);
    ASSERT(net_msg);
    net_msg_destroy(net_msg);
    return 0;
}

/*
 * Testcase 5
 * Create a network message with standard headroom and allocate all
 * the available space
 */
int testcase5() {
    net_msg_t* net_msg;
    net_msg = net_msg_new(256);
    ASSERT(net_msg);
    ASSERT(net_msg_append(net_msg, 256));
    ASSERT(0 == net_msg_append(net_msg, 1));
    return 0;
}

/*
 * Testcase 6
 * Create a network message with standard headroom and try to allocate more than
 * the available space
 */
int testcase6() {
    net_msg_t* net_msg;
    net_msg = net_msg_new(256);
    ASSERT(net_msg);
    ASSERT(0 == net_msg_append(net_msg, 257));
    return 0;
}

/*
 * Testcase 7
 * Create a network message with standard headroom and try to prepend
 * some bytes for a TCP header, an IP header and an Ethernet header
 */
int testcase7() {
    net_msg_t* net_msg;
    net_msg = net_msg_new(256);
    ASSERT(net_msg);
    ASSERT(net_msg_prepend(net_msg, 14 + 20 + 20));
    return 0;
}

/*
 * Create a TCP socket and verify initial state
 */
int testcase8() {
    socket_t* socket;
    socket = net_socket_create(AF_INET, SOCK_STREAM, 0);
    ASSERT(socket);
    ASSERT(0 == socket->connected);
    ASSERT(0 == socket->bound);
    ASSERT(0 == socket->error);
    ASSERT(0 == socket->so_queue_head);
    ASSERT(0 == socket->so_queue_tail);
    ASSERT(0 == socket->select_queue_head);
    ASSERT(0 == socket->select_queue_tail);
    ASSERT(0 == socket->parent);
    return 0;
}

/*
 * Create a raw IP socket and verify initial state
 */
int testcase9() {
    socket_t* socket;
    __net_loglevel = 0;
    socket = net_socket_create(AF_INET, SOCK_RAW, 0);
    __net_loglevel = 0;
    ASSERT(socket);
    ASSERT(socket->ops);
    ASSERT(0 == socket->connected);
    ASSERT(0 == socket->bound);
    ASSERT(0 == socket->error);
    ASSERT(0 == socket->so_queue_head);
    ASSERT(0 == socket->so_queue_tail);
    ASSERT(0 == socket->select_queue_head);
    ASSERT(0 == socket->select_queue_tail);
    ASSERT(0 == socket->parent);
    return 0;
}

/*
 * Create a raw IP socket and call recv. Simulate the case that the we have to wait are are interrupted by a signal
 */
int testcase10() {
    struct sockaddr_in in;
    socket_t* socket;
    char buffer[100];
    socket = net_socket_create(AF_INET, SOCK_RAW, 0);
    ASSERT(socket);
    in.sin_family = AF_INET;
    in.sin_addr.s_addr = inet_addr("10.0.2.21");
    __net_loglevel = 0;
    ASSERT(0 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    __net_loglevel = 0;
    /*
     * and call net_socket_recv - set up stub for protocol specific recv to return EAGAIN
     */
    ip_socket_recv_rc = -106;
    __net_loglevel = 0;
    ASSERT(-122 == net_socket_recv(socket, buffer, 100, 0, 0, 0, 0));
    __net_loglevel = 0;
    return 0;
}


/*
 * Create a raw IP socket and send 100 bytes. Simulate the case that the we have to wait (which actually never happens
 * with IP sockets...) are are interrupted by a signal - no data sent yet
 */
int testcase11() {
    ip_send_stub = 0;
    struct sockaddr_in in;
    socket_t* socket;
    char buffer[100];
    socket = net_socket_create(AF_INET, SOCK_RAW, 0);
    ASSERT(socket);
    in.sin_family = AF_INET;
    in.sin_addr.s_addr = inet_addr("10.0.2.21");
    __net_loglevel = 0;
    ASSERT(0 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    __net_loglevel = 0;
    /*
     * Set up IP send stub to return -EAGAIN
     */
    ip_socket_send_rc = -106;
    /*
     * and call net_socket_send
     */
    __net_loglevel = 0;
    ASSERT(-122 == net_socket_send(socket, buffer, 100, 0, 0, 0, 0));
    __net_loglevel = 0;
    return 0;
}

/*
 * Here we simulate the case that a first call of the protocol specific send call yields a result smaller than the number
 * of bytes to be sent and the second call yields -EAGAIN. If the wait is then interrupted by a signal, we should return
 * the number of bytes sent, not -EPAUSE
 */
static int ip_send_count;
void ip_send_stub_tc12() {
    ip_send_count++;
    if (2 == ip_send_count)
        ip_socket_send_rc = -106;
}
int testcase12() {
    ip_send_count = 0;
    struct sockaddr_in in;
    socket_t* socket;
    char buffer[200];
    socket = net_socket_create(AF_INET, SOCK_RAW, 0);
    ASSERT(socket);
    in.sin_family = AF_INET;
    in.sin_addr.s_addr = inet_addr("10.0.2.21");
    __net_loglevel = 0;
    ASSERT(0 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    __net_loglevel = 0;
    /*
     * Set up IP send stub to return 100 initially and set up pointer
     */
    ip_socket_send_rc = 100;
    ip_send_stub = ip_send_stub_tc12;
    /*
     * and call net_socket_send
     */
    __net_loglevel = 0;
    ASSERT(100 == net_socket_send(socket, buffer, 200, 0, 0, 0, 0));
    __net_loglevel = 0;
    return 0;
}

/*
 * Create a raw IP socket and inquire address data
 */
int testcase13() {
    ip_send_stub = 0;
    struct sockaddr_in in;
    struct sockaddr_in faddr;
    struct sockaddr_in laddr;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    socket_t* socket;
    char buffer[100];
    socket = net_socket_create(AF_INET, SOCK_RAW, 0);
    ASSERT(socket);
    in.sin_family = AF_INET;
    in.sin_addr.s_addr = inet_addr("10.0.2.21");
    ASSERT(0 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    __net_loglevel = 0;
    net_socket_getaddr(socket, (struct sockaddr*) &laddr, (struct sockaddr*) &faddr, &addrlen);
    ASSERT(addrlen == sizeof(struct sockaddr));
    __net_loglevel = 0;
    ASSERT(laddr.sin_addr.s_addr == inet_addr("10.0.2.20"));
    ASSERT(faddr.sin_addr.s_addr == inet_addr("10.0.2.21"));
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
    END;
}
