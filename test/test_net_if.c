/*
 * test_net_if.c
 *
 */

#include "kunit.h"
#include "net_if.h"
#include "vga.h"
#include "net.h"
#include "ip.h"
#include <limits.h>


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
 * Stubs
 */
static int do_putchar = 1;
void win_putchar(win_t* win, u8 c) {
    if (do_putchar)
        printf("%c", c);
}

extern int __net_loglevel;

/*
 * Atomic operations and synchronization primitives
 */
void atomic_incr(u32* reg) {
    reg++;
}

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

void sem_up(semaphore_t* sem) {

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
 * Stub for kmalloc/kfree
 */
u32 kmalloc(size_t size) {
    return (u32) malloc(size);
}

void kfree(u32 addr) {
    free((void*) addr);
}

/*
 * Validate user space buffers
 */
int mm_validate_buffer(u32 buffer, u32 len, int rw) {
    return 0;
}


/*
 * Stubs for params_get
 */
char* params_get(char* param) {
    return "10.0.2.20";
}

unsigned int params_get_int(char* param) {
    return 0;
}

/*
 * TCP layer stubs
 */
void tcp_init() {

}

static int tcp_rx_msg_called = 0;
static net_msg_t* tcp_msg = 0;
void tcp_rx_msg(net_msg_t* net_msg) {
    tcp_rx_msg_called++;
    tcp_msg = net_msg;
}

int tcp_create_socket(socket_t* socket, int domain, int proto) {
    return 0;
}

/*
 * UDP layer stubs
 */

void udp_init() {

}

int udp_create_socket(socket_t* socket, int type, int proto) {
    return 0;
}

/*
 * ICMP layer stubs
 */
static int icmp_rx_msg_called = 0;
static net_msg_t* icmp_msg = 0;
void icmp_rx_msg(net_msg_t* net_msg) {
    icmp_rx_msg_called++;
    icmp_msg = net_msg;
}


/*
 * Stubs for ARP layer
 */

void arp_init() {

}

int arp_rx_msg(net_msg_t* msg) {
    return 0;
}

/*
 * Stubs for IP layer
 */
void ip_rx_msg(net_msg_t* msg) {
}

void ip_init() {

}

int ip_create_socket(socket_t* socket, int domain, int proto) {
    return 0;
}

static int ip_add_route_called = 0;
static struct rtentry last_rt_entry;
int ip_add_route(struct rtentry* rt_entry) {
    ip_add_route_called++;
    memcpy((void*) &last_rt_entry, rt_entry, sizeof(struct rtentry));
    return 0;
}

int ip_del_route(struct rtentry* rt_entry) {
    return 0;
}

static int ip_purge_nic_called = 0;
void ip_purge_nic(nic_t* nic) {
    ip_purge_nic_called++;
}

int ip_get_rtconf(struct rtconf* rtc) {
    return 0;
}

/*
 * Ethernet utilities
 */
void eth_dump_header(u8* buffer) {

}

/*
 * Determine process ID
 */
unsigned int pm_get_pid() {
    return 0;
}

/*
 * Work queues
 */
int wq_schedule(int wq_id, int (*handler)(void*, int), void* arg, int opt) {
    return 0;
}

void wq_trigger(int wq_id) {

}

/****************************************************************************************
 * Test cases start here                                                                *
 ***************************************************************************************/

/*
 * Testcase 1: add two interfaces and use SIOCGIFCONF to retrieve an interface list
 * We simulate the case that both interfaces have assigned IP addresses
 */
int testcase1() {
    struct ifconf ifc;
    struct ifreq if_req[16];
    nic_t nic1;
    nic_t nic2;
    nic1.ip_addr = inet_addr("10.0.2.20");
    nic1.ip_addr_assigned = 1;
    nic2.ip_addr = inet_addr("10.0.2.21");
    nic2.ip_addr_assigned = 1;
    do_putchar = 0;
    net_if_init();
    net_if_add_nic(&nic1, 0);
    net_if_add_nic(&nic2, 0);
    do_putchar = 1;
    /*
     * Now do ioctl
     */
    ifc.ifc_ifcu.ifcu_req = if_req;
    ifc.ifc_len = sizeof(struct ifreq) * 16;
    ASSERT(0 == net_if_get_ifconf(&ifc));
    /*
     * and check result
     */
    ASSERT(2 * sizeof(struct ifreq) == ifc.ifc_len);
    /*
     * IP address should both be filled for both NICs
     */
    ASSERT(inet_addr("10.0.2.20") == ((struct sockaddr_in*) &(if_req[0].ifr_ifru.ifru_addr))->sin_addr.s_addr);
    ASSERT(AF_INET == ((struct sockaddr_in*) &(if_req[0].ifr_ifru.ifru_addr))->sin_family);
    ASSERT(inet_addr("10.0.2.21") == ((struct sockaddr_in*) &(if_req[1].ifr_ifru.ifru_addr))->sin_addr.s_addr);
    ASSERT(AF_INET == ((struct sockaddr_in*) &(if_req[1].ifr_ifru.ifru_addr))->sin_family);
    return 0;
}

/*
 * Testcase 2: add two interfaces and use SIOCGIFCONF to retrieve an interface list
 * We simulate the case that only the first interface has an assigned IP address
 */
int testcase2() {
    struct ifconf ifc;
    struct ifreq if_req[16];
    nic_t nic1;
    nic_t nic2;
    nic1.ip_addr = inet_addr("10.0.2.20");
    nic1.ip_addr_assigned = 1;
    nic2.ip_addr_assigned = 0;
    do_putchar = 0;
    net_if_init();
    net_if_remove_all();
    net_if_add_nic(&nic1, 0);
    net_if_add_nic(&nic2, 0);
    do_putchar = 1;
    /*
     * Now do ioctl
     */
    ifc.ifc_ifcu.ifcu_req = if_req;
    ifc.ifc_len = sizeof(struct ifreq) * 16;
    __net_loglevel = 0;
    ASSERT(0 == net_if_get_ifconf(&ifc));
    __net_loglevel = 0;
    /*
     * and check result
     */
    ASSERT(2 * sizeof(struct ifreq) == ifc.ifc_len);
    /*
     * IP address should both be filled for both NICs, but should be zero for second NIC
     */
    ASSERT(inet_addr("10.0.2.20") == ((struct sockaddr_in*) &(if_req[0].ifr_ifru.ifru_addr))->sin_addr.s_addr);
    ASSERT(AF_INET == ((struct sockaddr_in*) &(if_req[0].ifr_ifru.ifru_addr))->sin_family);
    ASSERT(INADDR_ANY == ((struct sockaddr_in*) &(if_req[1].ifr_ifru.ifru_addr))->sin_addr.s_addr);
    ASSERT(AF_INET == ((struct sockaddr_in*) &(if_req[1].ifr_ifru.ifru_addr))->sin_family);
    return 0;
}

/*
 * Testcase 3: add two interfaces and use SIOCGIFCONF to retrieve an interface list
 * Verify that name fields are correctly filled
 */
int testcase3() {
    struct ifconf ifc;
    struct ifreq if_req[16];
    nic_t nic1;
    nic_t nic2;
    nic1.ip_addr = inet_addr("10.0.2.20");
    nic1.ip_addr_assigned = 1;
    nic1.hw_type = HW_TYPE_ETH;
    nic2.ip_addr = inet_addr("10.0.2.21");
    nic2.ip_addr_assigned = 1;
    nic2.hw_type = HW_TYPE_ETH;
    do_putchar = 0;
    net_if_init();
    net_if_remove_all();
    net_if_add_nic(&nic1, 0);
    net_if_add_nic(&nic2, 0);
    do_putchar = 1;
    /*
     * Now do ioctl
     */
    ifc.ifc_ifcu.ifcu_req = if_req;
    ifc.ifc_len = sizeof(struct ifreq) * 16;
    ASSERT(0 == net_if_get_ifconf(&ifc));
    /*
     * and check result
     */
    ASSERT(2 * sizeof(struct ifreq) == ifc.ifc_len);
    ASSERT(0 == strncmp("eth0", if_req[0].ifrn_name, 4));
    ASSERT(0 == strncmp("eth1", if_req[1].ifrn_name, 4));
    return 0;
}

/*
 * Testcase 4: add three interfaces and use SIOCGIFCONF to retrieve an interface list
 * Simulate the case that the length of the buffer is too small
 */
int testcase4() {
    struct ifconf ifc;
    struct ifreq if_req[16];
    nic_t nic1;
    nic_t nic2;
    nic_t nic3;
    nic1.ip_addr = inet_addr("10.0.2.20");
    nic1.ip_addr_assigned = 1;
    nic1.hw_type = HW_TYPE_ETH;
    nic2.ip_addr = inet_addr("10.0.2.21");
    nic2.ip_addr_assigned = 1;
    nic2.hw_type = HW_TYPE_ETH;
    nic3.ip_addr = inet_addr("10.0.2.22");
    nic3.ip_addr_assigned = 1;
    nic3.hw_type = HW_TYPE_ETH;
    do_putchar = 0;
    net_if_init();
    net_if_remove_all();
    net_if_add_nic(&nic1, 0);
    net_if_add_nic(&nic2, 0);
    net_if_add_nic(&nic3, 0);
    do_putchar = 1;
    /*
     * Now do ioctl
     */
    ifc.ifc_ifcu.ifcu_req = if_req;
    ifc.ifc_len = sizeof(struct ifreq) * 2;
    __net_loglevel = 0;
    ASSERT(0 == net_if_get_ifconf(&ifc));
    __net_loglevel = 0;
    /*
     * and check result
     */
    ASSERT(2 * sizeof(struct ifreq) == ifc.ifc_len);
    ASSERT(0 == strncmp("eth0", if_req[0].ifrn_name, 4));
    ASSERT(0 == strncmp("eth1", if_req[1].ifrn_name, 4));
    return 0;
}

/*
 * Testcase 5: add a device and assign IP address
 */
int testcase5() {
    struct ifreq ifr;
    struct ifreq ifq;
    struct sockaddr_in* in;
    nic_t nic;
    nic.ip_addr_assigned = 0;
    nic.ip_addr = 0;
    nic.hw_type = HW_TYPE_ETH;
    net_if_init();
    net_if_remove_all();
    do_putchar = 0;
    net_if_add_nic(&nic, 0);
    do_putchar = 1;
    ASSERT(0 == strncmp(nic.name, "eth0", 4));
    strncpy(ifr.ifrn_name, "eth0", 4);
    in = (struct sockaddr_in*) &ifr.ifr_ifru.ifru_addr;
    in->sin_family = AF_INET;
    in->sin_addr.s_addr = inet_addr("10.0.2.21");
    __net_loglevel = 0;
    ASSERT(0 == net_if_set_addr(&ifr));
    __net_loglevel = 0;
    ASSERT(1 == nic.ip_addr_assigned);
    ASSERT(inet_addr("10.0.2.21") == nic.ip_addr);
    /*
     * Now verify IP address using net_if_get_addr
     */
    strncpy(ifq.ifrn_name, "eth0", 4);
    ASSERT(0 == net_if_get_addr(&ifq));
    in = (struct sockaddr_in*) &ifq.ifr_ifru.ifru_addr;
    ASSERT(in->sin_family == AF_INET);
    ASSERT(in->sin_addr.s_addr == inet_addr("10.0.2.21"));
    return 0;
}

/*
 * Testcase 6: add a device and assign IP address, then verify that a route to the local
 * network is added (class A network)
 */
int testcase6() {
    struct ifreq ifr;
    struct sockaddr_in* in;
    nic_t nic;
    nic.ip_addr_assigned = 0;
    nic.ip_addr = 0;
    nic.hw_type = HW_TYPE_ETH;
    net_if_init();
    net_if_remove_all();
    do_putchar = 0;
    net_if_add_nic(&nic, 0);
    do_putchar = 1;
    ASSERT(0 == strncmp(nic.name, "eth0", 4));
    strncpy(ifr.ifrn_name, "eth0", 4);
    in = (struct sockaddr_in*) &ifr.ifr_ifru.ifru_addr;
    in->sin_family = AF_INET;
    in->sin_addr.s_addr = inet_addr("10.0.2.21");
    __net_loglevel = 0;
    ip_add_route_called = 0;
    ASSERT(0 == net_if_set_addr(&ifr));
    __net_loglevel = 0;
    ASSERT(1 == nic.ip_addr_assigned);
    ASSERT(inet_addr("10.0.2.21") == nic.ip_addr);
    /*
     * Check route - there should be a route to the local
     * network, which is a class A network
     */
    ASSERT(1 == ip_add_route_called);
    in = (struct sockaddr_in*) &last_rt_entry.rt_dst;
    ASSERT(in->sin_addr.s_addr == inet_addr("10.0.0.0"));
    in = (struct sockaddr_in*) &last_rt_entry.rt_genmask;
    ASSERT(in->sin_addr.s_addr == inet_addr("255.0.0.0"));
    return 0;
}

/*
 * Testcase 7: add a device and assign IP address, then verify that a route to the local
 * network is added (class B network)
 */
int testcase7() {
    struct ifreq ifr;
    struct sockaddr_in* in;
    nic_t nic;
    nic.ip_addr_assigned = 0;
    nic.ip_addr = 0;
    nic.hw_type = HW_TYPE_ETH;
    net_if_init();
    net_if_remove_all();
    do_putchar = 0;
    net_if_add_nic(&nic, 0);
    do_putchar = 1;
    ASSERT(0 == strncmp(nic.name, "eth0", 4));
    strncpy(ifr.ifrn_name, "eth0", 4);
    in = (struct sockaddr_in*) &ifr.ifr_ifru.ifru_addr;
    in->sin_family = AF_INET;
    in->sin_addr.s_addr = inet_addr("128.10.1.21");
    __net_loglevel = 0;
    ip_add_route_called = 0;
    __net_loglevel = 0;
    ASSERT(0 == net_if_set_addr(&ifr));
    __net_loglevel = 0;
    ASSERT(1 == nic.ip_addr_assigned);
    ASSERT(inet_addr("128.10.1.21") == nic.ip_addr);
    /*
     * Check route - there should be a route to the local
     * network, which is a class B network
     */
    ASSERT(1 == ip_add_route_called);
    in = (struct sockaddr_in*) &last_rt_entry.rt_dst;
    ASSERT(in->sin_addr.s_addr == inet_addr("128.10.0.0"));
    in = (struct sockaddr_in*) &last_rt_entry.rt_genmask;
    ASSERT(in->sin_addr.s_addr == inet_addr("255.255.0.0"));
    return 0;
}

/*
 * Testcase 8: add a device and assign IP address, then verify that a route to the local
 * network is added (class C network)
 */
int testcase8() {
    struct ifreq ifr;
    struct sockaddr_in* in;
    nic_t nic;
    nic.ip_addr_assigned = 0;
    nic.ip_addr = 0;
    nic.hw_type = HW_TYPE_ETH;
    net_if_init();
    net_if_remove_all();
    do_putchar = 0;
    net_if_add_nic(&nic, 0);
    do_putchar = 1;
    ASSERT(0 == strncmp(nic.name, "eth0", 4));
    strncpy(ifr.ifrn_name, "eth0", 4);
    in = (struct sockaddr_in*) &ifr.ifr_ifru.ifru_addr;
    in->sin_family = AF_INET;
    in->sin_addr.s_addr = inet_addr("192.168.1.21");
    __net_loglevel = 0;
    ip_add_route_called = 0;
    __net_loglevel = 0;
    ASSERT(0 == net_if_set_addr(&ifr));
    __net_loglevel = 0;
    ASSERT(1 == nic.ip_addr_assigned);
    ASSERT(inet_addr("192.168.1.21") == nic.ip_addr);
    /*
     * Check route - there should be a route to the local
     * network, which is a class C network
     */
    ASSERT(1 == ip_add_route_called);
    in = (struct sockaddr_in*) &last_rt_entry.rt_dst;
    ASSERT(in->sin_addr.s_addr == inet_addr("192.168.1.0"));
    in = (struct sockaddr_in*) &last_rt_entry.rt_genmask;
    ASSERT(in->sin_addr.s_addr == inet_addr("255.255.255.0"));
    return 0;
}

/*
 * Testcase 9: add a device and assign IP address, then verify that a route to the local
 * network is added (class A network). Then change the netmask to 255.255.255.0 and verify
 * that the route is updated
 */
int testcase9() {
    struct ifreq ifr;
    struct ifreq ifq;
    struct sockaddr_in* in;
    nic_t nic;
    nic.ip_addr_assigned = 0;
    nic.ip_addr = 0;
    nic.hw_type = HW_TYPE_ETH;
    net_if_init();
    net_if_remove_all();
    do_putchar = 0;
    net_if_add_nic(&nic, 0);
    do_putchar = 1;
    ASSERT(0 == strncmp(nic.name, "eth0", 4));
    strncpy(ifr.ifrn_name, "eth0", 4);
    in = (struct sockaddr_in*) &ifr.ifr_ifru.ifru_addr;
    in->sin_family = AF_INET;
    in->sin_addr.s_addr = inet_addr("10.0.2.21");
    __net_loglevel = 0;
    ip_add_route_called = 0;
    ASSERT(0 == net_if_set_addr(&ifr));
    __net_loglevel = 0;
    ASSERT(1 == nic.ip_addr_assigned);
    ASSERT(inet_addr("10.0.2.21") == nic.ip_addr);
    ASSERT(nic.ip_netmask == inet_addr("255.0.0.0"));
    /*
     * Check route - there should be a route to the local
     * network, which is a class A network
     */
    ASSERT(1 == ip_add_route_called);
    in = (struct sockaddr_in*) &last_rt_entry.rt_dst;
    ASSERT(in->sin_addr.s_addr == inet_addr("10.0.0.0"));
    in = (struct sockaddr_in*) &last_rt_entry.rt_genmask;
    ASSERT(in->sin_addr.s_addr == inet_addr("255.0.0.0"));
    /*
     * Now update netmask
     */
    ip_add_route_called = 0;
    ip_purge_nic_called = 0;
    strncpy(ifr.ifrn_name, "eth0", 4);
    in = (struct sockaddr_in*) &ifr.ifr_ifru.ifru_netmask;
    in->sin_family = AF_INET;
    in->sin_addr.s_addr = inet_addr("255.255.255.0");
    __net_loglevel = 0;
   ASSERT(0 == net_if_set_netmask(&ifr));
    __net_loglevel = 0;
    /*
     * Check result
     */
     strncpy(ifq.ifrn_name, "eth0", 4);
     ASSERT(0 == net_if_get_netmask(&ifq));
     in = (struct sockaddr_in*) &ifq.ifr_ifru.ifru_addr;
     ASSERT(in->sin_family == AF_INET);
     ASSERT(in->sin_addr.s_addr == inet_addr("255.255.255.0"));
    /*
     * Check route - there should be a route to the local
     * network, which is now a class C network
     */
    ASSERT(1 == ip_add_route_called);
    in = (struct sockaddr_in*) &last_rt_entry.rt_dst;
    ASSERT(in->sin_addr.s_addr == inet_addr("10.0.2.0"));
    in = (struct sockaddr_in*) &last_rt_entry.rt_genmask;
    ASSERT(in->sin_addr.s_addr == inet_addr("255.255.255.0"));
    /*
     * Old entries should have been purged
     */
    ASSERT(1 == ip_purge_nic_called);
    /*
     * and netmask should have been updated
     */
    ASSERT(nic.ip_netmask == inet_addr("255.255.255.0"));
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
    END;
}
