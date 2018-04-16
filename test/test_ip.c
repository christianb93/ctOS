/*
 * test_ip.c
 *
 */



#include "ktypes.h"
#include "vga.h"
#include "arp.h"
#include "ip.h"
#include "lib/os/if.h"
#include "kunit.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>

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
 * Stubs for ARP layer functions
 */
void arp_init() {

}

int arp_resolve(nic_t* nic, u32 ip_address, mac_address_t* mac_address) {
    return 0;
}

/*
 * Stubs for synchronization primitives
 */
void cond_init(cond_t* cond) {

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

int cond_wait_intr_timed(cond_t* cond, spinlock_t* lock, u32* eflags, unsigned int timeout) {
    spinlock_release(lock, eflags);
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
 * Validate user space buffers
 */
int mm_validate_buffer(u32 buffer, u32 len, int rw) {
    return 0;
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
 * Network interface layer stubs
 */
static nic_t* our_nic = 0;
static nic_t* second_nic = 0;
nic_t* net_if_get_nic(u32 ip_address) {
    if (0 == our_nic)
        return 0;
    if ((our_nic->ip_addr_assigned == 1) && (our_nic->ip_addr == ip_address))
        return our_nic;
    if (second_nic) {
        if ((second_nic->ip_addr_assigned == 1) && (second_nic->ip_addr == ip_address))
            return second_nic;
    }
    return 0;
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
        return our_nic;
    if (0 == strncmp("eth1", name, 4))
        return second_nic;
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
 * ICMP layer stubs
 */
static int icmp_rx_msg_called = 0;
static net_msg_t* icmp_msg = 0;
void icmp_rx_msg(net_msg_t* net_msg) {
    icmp_rx_msg_called++;
    icmp_msg = net_msg;
}

/*
 * UDP layer stubs
 */

void udp_init() {

}

int udp_create_socket(socket_t* socket, int type, int proto) {
    return 0;
}


void udp_rx_msg(net_msg_t* net_msg) {

}

/*
 * Work scheduler
 */
static int wq_schedule_called = 0;
static net_msg_t* tx_net_msg[16];
int wq_schedule(int wq_id, int (*handler)(void*, int), void* arg, int opt) {
    tx_net_msg[wq_schedule_called % 16] = (net_msg_t*) arg;
    wq_schedule_called++;
    return 0;
}

/*
 * Compute IP checksum
 */
u16 validate_ip_checksum(unsigned short ipLen, u16* ip_hdr) {
    unsigned long sum = 0;
    while (ipLen > 1) {
        sum += * ip_hdr++;
        ipLen -= 2;
    }
    if(ipLen > 0) {
        sum += ((*ip_hdr) & htons(0xFF00));

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


/****************************************************************************************
 * The actual test cases start here                                                     *
 ***************************************************************************************/

/*
 * Utility to add a route to a local network for a given device
 */
static int add_route(u32 ip_dst, u32 netmask, char* dev) {
    struct rtentry rt_entry;
    struct sockaddr_in* in;
    strncpy(rt_entry.dev, dev, 4);
    rt_entry.rt_flags = RT_FLAGS_UP;
    in = (struct sockaddr_in*) &rt_entry.rt_dst;
    in->sin_family = AF_INET;
    in->sin_addr.s_addr = (ip_dst & netmask);
    in = (struct sockaddr_in*) &rt_entry.rt_genmask;
    in->sin_family = AF_INET;
    in->sin_addr.s_addr = netmask;
    in = (struct sockaddr_in*) &rt_entry.rt_gateway;
    in->sin_family = AF_INET;
    in->sin_addr.s_addr = INADDR_ANY;
    return ip_add_route(&rt_entry);
}

/*
 * Testcase 1: ip_tx_msg
 * Call ip_tx_msg to transmit a single, unfragmented IP message and verify the resulting call to
 * wq_schedule
 */
int testcase1() {
    net_msg_t* net_msg;
    ip_hdr_t* ip_hdr;
    nic_t nic;
    int i;
    unsigned char* data;
    int rc;
    net_init();
    /*
     * Create net_msg and simulate 100 bytes of data
     */
    net_msg = net_msg_new(256);
    data = net_msg_append(net_msg, 100);
    for (i = 0; i < 100; i++)
        data[i] = i;
    /*
     * and set ip_proto field as well as IP destination address
     * and IP source address
     */
    net_msg->ip_proto = IP_PROTO_UDP;
    net_msg->ip_dest = 0x1502000a;
    net_msg->ip_src = 0x1402000a;
    net_msg->ip_df = 1;
    /*
     * We need to simulate a NIC which is returned by our stub for net_if_get_nic -
     * address needs to match source address in net_msg
     */
    our_nic = &nic;
    nic.ip_addr = 0x1402000a;
    nic.ip_addr_assigned = 1;
    nic.mtu=1024;
    /*
     * add route
     */
    __net_loglevel = 0;
    ASSERT(0 == add_route(net_msg->ip_dest, inet_addr("255.255.0.0"), "eth0"));
    __net_loglevel = 0;
    /*
     * Now call ip_tx_msg
     */
    wq_schedule_called = 0;
    __net_loglevel = 0;
    rc = ip_tx_msg(net_msg);
    __net_loglevel = 0;
    /*
     * and verify result
     */
    ASSERT(0 == rc);
    ASSERT(1 == wq_schedule_called);
    ASSERT(tx_net_msg[0] == net_msg);
    /*
     * Now check fields in net_msg
     */
    ASSERT(net_msg->nic == our_nic);
    ASSERT(0x800 == ntohs(net_msg->ethertype));
    /*
     * and verify contents of IP header
     * - ID is 0 as we do not use fragmentation
     * - Flags are 0x4000 (DF = 1, MF = 0)
     * - IP_SRC and IP_DEST are as specified in the network message
     * - IP length is length of network message plus size of header
     * - IP protocol is as specified
     * - IP version is 0x45 (IP version 4, header length 5 dwords)
     * - TTL is 64
     */
    ip_hdr = (ip_hdr_t*) net_msg->start;
    ASSERT(ip_hdr->id == 0);
    ASSERT(ip_hdr->flags == htons(0x4000));
    ASSERT(ip_hdr->ip_dest == 0x1502000a);
    ASSERT(ip_hdr->ip_src == 0x1402000a);
    ASSERT(ip_hdr->length == htons(100 + 20));
    ASSERT(ip_hdr->proto == IP_PROTO_UDP);
    ASSERT(ip_hdr->version == 0x45);
    ASSERT(ip_hdr->ttl == IP_DEFAULT_TTL);
    /*
     * IP checksum should be zero
     */
    ASSERT(0 == validate_ip_checksum(20, (unsigned short*) ip_hdr));
    /*
     * Finally verify data
     */
    for (i = 0; i < 100; i++)
        ASSERT(((unsigned char*) net_msg->start + sizeof(ip_hdr_t))[i] == i);
    /*
     * and make sure that number of created network messages equals calls to wq_schedule
     */
    unsigned int created;
    unsigned int destroyed;
    net_get_counters(&created, &destroyed);
    ASSERT(created == destroyed + wq_schedule_called);
    return 0;
}

/*
 * Testcase 2: ip_tx_msg
 * Call ip_tx_msg to transmit a single, unfragmented IP message and verify the resulting call to
 * wq_schedule - special case that IP source address is 0
 */
int testcase2() {
    net_msg_t* net_msg;
    ip_hdr_t* ip_hdr;
    nic_t nic;
    net_init();
    /*
     * Create net_msg and simulate 100 bytes of data
     */
    net_msg = net_msg_new(256);
    net_msg_append(net_msg, 100);
    /*
     * and set ip_proto field as well as IP destination address
     * and IP source address
     */
    net_msg->ip_proto = IP_PROTO_UDP;
    net_msg->ip_dest = 0x1502000a;
    net_msg->ip_src = 0;
    net_msg->ip_df = 1;
    /*
     * We need to simulate a NIC which is returned by our stub for net_if_get_nic
     */
    our_nic = &nic;
    nic.ip_addr = 0x1602000a;
    nic.mtu = 1204;
    ASSERT(0 == add_route(net_msg->ip_dest, inet_addr("255.255.0.0"), "eth0"));
    /*
     * Now call ip_tx_msg
     */
    wq_schedule_called = 0;
    ip_tx_msg(net_msg);
    /*
     * and verify result
     */
    ASSERT(1 == wq_schedule_called);
    ASSERT(tx_net_msg[0] == net_msg);
    /*
     * Now check fields in net_msg
     */
    ASSERT(net_msg->nic == our_nic);
    ASSERT(0x800 == ntohs(net_msg->ethertype));
    /*
     * and verify contents of IP header
     * - ID is 0 as we do not use fragmentation
     * - Flags are 0x4000 (DF = 1, MF = 0)
     * - IP_DEST is as specified in the network message
     * - IP_SRC is taken from the NIC
     * - IP length is length of network message plus size of header
     * - IP protocol is as specified
     * - IP version is 0x45 (IP version 4, header length 5 dwords)
     * - TTL is 64
     */
    ip_hdr = (ip_hdr_t*) net_msg->start;
    ASSERT(ip_hdr->id == 0);
    ASSERT(ip_hdr->flags == htons(0x4000));
    ASSERT(ip_hdr->ip_dest == 0x1502000a);
    ASSERT(ip_hdr->ip_src == 0x1602000a);
    ASSERT(ip_hdr->length == htons(100 + 20));
    ASSERT(ip_hdr->proto == IP_PROTO_UDP);
    ASSERT(ip_hdr->version == 0x45);
    ASSERT(ip_hdr->ttl == IP_DEFAULT_TTL);
    /*
     * IP checksum should be zero
     */
    ASSERT(0 == validate_ip_checksum(20, (unsigned short*) ip_hdr));
    /*
     * and make sure that number of created network messages equals calls to wq_schedule
     */
    unsigned int created;
    unsigned int destroyed;
    net_get_counters(&created, &destroyed);
    ASSERT(created == destroyed + wq_schedule_called);
    return 0;
}

/*
 * Testcase 3: ip_tx_msg
 * Call ip_tx_msg with a message which requires fragmentation. Verify first fragment
 */
int testcase3() {
    net_msg_t* net_msg;
    ip_hdr_t* ip_hdr;
    nic_t nic;
    unsigned char* data;
    int i;
    ip_init();
    net_init();
    /*
     * Create net_msg and simulate 2000 bytes of data (IP payload)
     */
    net_msg = net_msg_new(8192);
    data = net_msg_append(net_msg, 2000);
    ASSERT(data);
    ASSERT(net_msg_get_size(net_msg) == 2000);
    for (i = 0; i < 2000; i++)
        data[i] = i;
    /*
     * and set ip_proto field as well as IP destination address
     * and IP source address
     */
    net_msg->ip_proto = IP_PROTO_UDP;
    net_msg->ip_dest = 0x1502000a;
    net_msg->ip_src = 0x1402000a;
    net_msg->ip_df = 0;
    /*
     * We need to simulate a NIC which is returned by our stub for net_if_get_nic
     */
    our_nic = &nic;
    nic.ip_addr = 0x1402000a;
    nic.ip_addr_assigned = 1;
    nic.mtu = 1500;
    ASSERT(0 == add_route(net_msg->ip_dest, inet_addr("255.255.0.0"), "eth0"));
    /*
     * Now call ip_tx_msg
     */
    wq_schedule_called = 0;
    __net_loglevel = 0;
    ip_tx_msg(net_msg);
    __net_loglevel = 0;
    /*
     * and verify result. As we exceed the MTU, we should have created two messages,
     * the first one being our original message
     */
    ASSERT(2 == wq_schedule_called);
    ASSERT(tx_net_msg[0] == net_msg);
    /*
     * Now check fields in net_msg
     */
    ASSERT(net_msg->nic == our_nic);
    ASSERT(0x800 == ntohs(net_msg->ethertype));
    /*
     * and IP header
     */
    ip_hdr = (ip_hdr_t*) net_msg->start;
    ASSERT(ip_hdr->version == 0x45);
    /*
     * The first fragment should be 1500 bytes including 20 bytes for the header, i.e. 1480 data bytes
     */
    ASSERT(ip_hdr->length == htons(1500));
    /*
     * IP checksum should be zero
     */
    ASSERT(0 == validate_ip_checksum(20, (unsigned short*) ip_hdr));
    /*
     * ID should be different from zero
     */
    ASSERT(ip_hdr->id);
    /*
     * and flags should be 0x2000 (DF = 0, MF = 1)
     */
    ASSERT(ip_hdr->flags == htons(0x2000));
    /*
     * Verify remaining fields
     */
    ASSERT(ip_hdr->ip_dest == 0x1502000a);
    ASSERT(ip_hdr->ip_src == 0x1402000a);
    ASSERT(ip_hdr->proto == IP_PROTO_UDP);
    ASSERT(ip_hdr->version == 0x45);
    ASSERT(ip_hdr->ttl == IP_DEFAULT_TTL);
    /*
     * and data
     */
    for (i = 0; i < 1480; i++) {
        if (((unsigned char*) net_msg->start + sizeof(ip_hdr_t))[i] != (i % 256))
            printf("Found difference at index %d \n", i);
        ASSERT(((unsigned char*) net_msg->start + sizeof(ip_hdr_t))[i] == i % 256);
    }
    /*
     * and make sure that number of created network messages equals calls to wq_schedule
     */
    unsigned int created;
    unsigned int destroyed;
    net_get_counters(&created, &destroyed);
    ASSERT(created == destroyed + wq_schedule_called);
    return 0;
}

/*
 * Testcase 4: ip_tx_msg
 * Call ip_tx_msg with a message which requires fragmentation. Verify second (and last) fragment
 */
int testcase4() {
    net_msg_t* net_msg;
    ip_hdr_t* ip_hdr;
    unsigned int id;
    nic_t nic;
    int i;
    unsigned char* data;
    ip_init();
    net_init();
    /*
     * Create net_msg and simulate 2000 bytes of data (IP payload)
     */
    net_msg = net_msg_new(8192);
    data = net_msg_append(net_msg, 2000);
    ASSERT(data);
    for (i = 0; i < 2000; i++)
        data[i] = i;
    ASSERT(net_msg_get_size(net_msg) == 2000);
    /*
     * and set ip_proto field as well as IP destination address
     * and IP source address
     */
    net_msg->ip_proto = IP_PROTO_UDP;
    net_msg->ip_dest = 0x1502000a;
    net_msg->ip_src = 0x1402000a;
    net_msg->ip_df = 0;
    /*
     * We need to simulate a NIC which is returned by our stub for net_if_get_nic
     */
    our_nic = &nic;
    nic.ip_addr = 0x1402000a;
    nic.ip_addr_assigned = 1;
    nic.mtu = 1500;
    ASSERT(0 == add_route(net_msg->ip_dest, inet_addr("255.255.0.0"), "eth0"));
    /*
     * Now call ip_tx_msg
     */
    wq_schedule_called = 0;
    __net_loglevel = 0;
    ip_tx_msg(net_msg);
    __net_loglevel = 0;
    /*
     * and verify result. As we exceed the MTU, we should have created two messages,
     * the first one being our original message
     */
    ASSERT(2 == wq_schedule_called);
    ASSERT(tx_net_msg[0] == net_msg);
    ASSERT(tx_net_msg[1]);
    ASSERT(tx_net_msg[1] != net_msg);
    /*
     * Get ID from first message
     */
    ip_hdr = (ip_hdr_t*) net_msg->start;
    id = ntohs(ip_hdr->id);
    net_msg = tx_net_msg[1];
    /*
     * Now check fields in net_msg
     */
    ASSERT(net_msg->nic == our_nic);
    ASSERT(0x800 == ntohs(net_msg->ethertype));
    /*
     * and IP header
     */
    ip_hdr = (ip_hdr_t*) net_msg->start;
    ASSERT(ip_hdr->version == 0x45);
    /*
     * Our total payload is 2000 bytes. With the first fragment, we have transmitted 1480 data bytes, i.e. there are
     * 2000 - 1480 = 520 data bytes left. So our second fragment should be 520 + 20 = 540 bytes
     */
    ASSERT(ip_hdr->length == htons(540));
    /*
     * IP checksum should be zero
     */
    ASSERT(0 == validate_ip_checksum(20, (unsigned short*) ip_hdr));
    /*
     * ID should be different from zero and equal to ID of first fragment
     */
    ASSERT(ip_hdr->id);
    ASSERT(ntohs(ip_hdr->id) == id);
    /*
     * and flags should be 0x0000 (DF = 0, MF = 0), offset should be 1480  / 8 = 0xb9
     */
    ASSERT(ip_hdr->flags == htons(0xb9));
    /*
     * Verify remaining fields
     */
    ASSERT(ip_hdr->ip_dest == 0x1502000a);
    ASSERT(ip_hdr->ip_src == 0x1402000a);
    ASSERT(ip_hdr->proto == IP_PROTO_UDP);
    ASSERT(ip_hdr->version == 0x45);
    ASSERT(ip_hdr->ttl == IP_DEFAULT_TTL);
    /*
     * and data
     */
    for (i = 0; i < 520; i++) {
        if (((unsigned char*) net_msg->start + sizeof(ip_hdr_t))[i] != ((i + 1480) % 256))
            printf("Found difference at index %d \n", i);
        ASSERT(((unsigned char*) net_msg->start + sizeof(ip_hdr_t))[i] == (i + 1480) % 256);
    }
    /*
     * and make sure that number of created network messages equals calls to wq_schedule
     */
    unsigned int created;
    unsigned int destroyed;
    net_get_counters(&created, &destroyed);
    ASSERT(created == destroyed + wq_schedule_called);
    return 0;
}

/*
 * Testcase 5: ip_tx_msg
 * Call ip_tx_msg with a message which requires fragmentation. Verify first fragment - more than two fragments
 */
int testcase5() {
    net_msg_t* net_msg;
    ip_hdr_t* ip_hdr;
    nic_t nic;
    unsigned char* data;
    int i;
    ip_init();
    net_init();
    /*
     * Create net_msg and simulate 5000 bytes of data (IP payload)
     */
    net_msg = net_msg_new(8192);
    data = net_msg_append(net_msg, 5000);
    ASSERT(data);
    ASSERT(net_msg_get_size(net_msg) == 5000);
    for (i = 0; i < 5000; i++)
        data[i] = i;
    /*
     * and set ip_proto field as well as IP destination address
     * and IP source address
     */
    net_msg->ip_proto = IP_PROTO_UDP;
    net_msg->ip_dest = 0x1502000a;
    net_msg->ip_src = 0x1402000a;
    net_msg->ip_df = 0;
    /*
     * We need to simulate a NIC which is returned by our stub for net_if_get_nic
     */
    our_nic = &nic;
    nic.ip_addr = 0x1402000a;
    nic.ip_addr_assigned = 1;
    nic.mtu = 1500;
    ASSERT(0 == add_route(net_msg->ip_dest, inet_addr("255.255.0.0"), "eth0"));
    /*
     * Now call ip_tx_msg
     */
    wq_schedule_called = 0;
    __net_loglevel = 0;
    ip_tx_msg(net_msg);
    __net_loglevel = 0;
    /*
     * and verify result. As we exceed the MTU, we should have created four messages,
     * the first one being our original message
     */
    ASSERT(4 == wq_schedule_called);
    ASSERT(tx_net_msg[0] == net_msg);
    /*
     * Now check fields in net_msg
     */
    ASSERT(net_msg->nic == our_nic);
    ASSERT(0x800 == ntohs(net_msg->ethertype));
    /*
     * and IP header
     */
    ip_hdr = (ip_hdr_t*) net_msg->start;
    ASSERT(ip_hdr->version == 0x45);
    /*
     * The first fragment should be 1500 bytes including 20 bytes for the header, i.e. 1480 data bytes
     */
    ASSERT(ip_hdr->length == htons(1500));
    /*
     * IP checksum should be zero
     */
    ASSERT(0 == validate_ip_checksum(20, (unsigned short*) ip_hdr));
    /*
     * ID should be different from zero
     */
    ASSERT(ip_hdr->id);
    /*
     * and flags should be 0x2000 (DF = 0, MF = 1)
     */
    ASSERT(ip_hdr->flags == htons(0x2000));
    /*
     * Verify remaining fields
     */
    ASSERT(ip_hdr->ip_dest == 0x1502000a);
    ASSERT(ip_hdr->ip_src == 0x1402000a);
    ASSERT(ip_hdr->proto == IP_PROTO_UDP);
    ASSERT(ip_hdr->version == 0x45);
    ASSERT(ip_hdr->ttl == IP_DEFAULT_TTL);
    /*
     * and data
     */
    for (i = 0; i < 1480; i++) {
        if (((unsigned char*) net_msg->start + sizeof(ip_hdr_t))[i] != (i % 256))
            printf("Found difference at index %d \n", i);
        ASSERT(((unsigned char*) net_msg->start + sizeof(ip_hdr_t))[i] == i % 256);
    }
    /*
     * and make sure that number of created network messages equals calls to wq_schedule
     */
    unsigned int created;
    unsigned int destroyed;
    net_get_counters(&created, &destroyed);
    ASSERT(created == destroyed + wq_schedule_called);
    return 0;
}

/*
 * Testcase 6: ip_tx_msg
 * Call ip_tx_msg with a message which requires fragmentation. Verify last fragment - more than two fragments
 */
int testcase6() {
    net_msg_t* net_msg;
    ip_hdr_t* ip_hdr;
    unsigned int id;
    nic_t nic;
    int i;
    unsigned char* data;
    ip_init();
    /*
     * Create net_msg and simulate 5000 bytes of data (IP payload)
     */
    net_msg = net_msg_new(8192);
    data = net_msg_append(net_msg, 5000);
    ASSERT(data);
    for (i = 0; i < 5000; i++)
        data[i] = i;
    ASSERT(net_msg_get_size(net_msg) == 5000);
    /*
     * and set ip_proto field as well as IP destination address
     * and IP source address
     */
    net_msg->ip_proto = IP_PROTO_UDP;
    net_msg->ip_dest = 0x1502000a;
    net_msg->ip_src = 0x1402000a;
    net_msg->ip_df = 0;
    /*
     * We need to simulate a NIC which is returned by our stub for net_if_get_nic
     */
    our_nic = &nic;
    nic.ip_addr = 0x1402000a;
    nic.ip_addr_assigned = 1;
    nic.mtu = 1500;
    ASSERT(0 == add_route(net_msg->ip_dest, inet_addr("255.255.0.0"), "eth0"));
    /*
     * Now call ip_tx_msg
     */
    wq_schedule_called = 0;
    __net_loglevel = 0;
    ip_tx_msg(net_msg);
    __net_loglevel = 0;
    /*
     * and verify result. As we exceed the MTU, we should have created two messages,
     * the first one being our original message
     */
    ASSERT(4 == wq_schedule_called);
    ASSERT(tx_net_msg[0] == net_msg);
    ASSERT(tx_net_msg[3]);
    ASSERT(tx_net_msg[3] != net_msg);
    /*
     * Get ID from first message
     */
    ip_hdr = (ip_hdr_t*) net_msg->start;
    id = ntohs(ip_hdr->id);
    net_msg = tx_net_msg[3];
    /*
     * Now check fields in net_msg
     */
    ASSERT(net_msg->nic == our_nic);
    ASSERT(0x800 == ntohs(net_msg->ethertype));
    /*
     * and IP header
     */
    ip_hdr = (ip_hdr_t*) net_msg->start;
    ASSERT(ip_hdr->version == 0x45);
    /*
     * Our total payload is 5000 bytes. With the first fragment, we have transmitted 1480 data bytes, i.e. there are
     * 5000 - 1480 = 3520 data bytes left. So our second fragment should be 1500 bytes again and contain another 1480
     * data bytes, leaving us with 2040 bytes. The third fragment should again contain 1500 bytes, and finally fragment
     * #4 should have 560 data bytes
     */
    ASSERT(ip_hdr->length == htons(560 + 20));
    /*
     * IP checksum should be zero
     */
    ASSERT(0 == validate_ip_checksum(20, (unsigned short*) ip_hdr));
    /*
     * ID should be different from zero and equal to ID of first fragment
     */
    ASSERT(ip_hdr->id);
    ASSERT(ntohs(ip_hdr->id) == id);
    /*
     * and flags should be 0x0000 (DF = 0, MF = 0), offset should be 3*1480  / 8 = 0x22b
     */
    ASSERT(ip_hdr->flags == htons(0x22b));
    /*
     * Verify remaining fields
     */
    ASSERT(ip_hdr->ip_dest == 0x1502000a);
    ASSERT(ip_hdr->ip_src == 0x1402000a);
    ASSERT(ip_hdr->proto == IP_PROTO_UDP);
    ASSERT(ip_hdr->version == 0x45);
    ASSERT(ip_hdr->ttl == IP_DEFAULT_TTL);
    /*
     * and data
     */
    for (i = 0; i < 560; i++) {
        if (((unsigned char*) net_msg->start + sizeof(ip_hdr_t))[i] != ((i + 3*1480) % 256))
            printf("Found difference at index %d \n", i);
        ASSERT(((unsigned char*) net_msg->start + sizeof(ip_hdr_t))[i] == (i + 3*1480) % 256);
    }
    return 0;
}

/*
 * Testcase 7: ip_tx_msg
 * Call ip_tx_msg with a message which requires fragmentation. Verify second fragment - more than two fragments
 */
int testcase7() {
    net_msg_t* net_msg;
    ip_hdr_t* ip_hdr;
    unsigned int id;
    nic_t nic;
    int i;
    unsigned char* data;
    ip_init();
    /*
     * Create net_msg and simulate 5000 bytes of data (IP payload)
     */
    net_msg = net_msg_new(8192);
    data = net_msg_append(net_msg, 5000);
    ASSERT(data);
    for (i = 0; i < 5000; i++)
        data[i] = i;
    ASSERT(net_msg_get_size(net_msg) == 5000);
    /*
     * and set ip_proto field as well as IP destination address
     * and IP source address
     */
    net_msg->ip_proto = IP_PROTO_UDP;
    net_msg->ip_dest = 0x1502000a;
    net_msg->ip_src = 0x1402000a;
    net_msg->ip_df = 0;
    /*
     * We need to simulate a NIC which is returned by our stub for net_if_get_nic
     */
    our_nic = &nic;
    nic.ip_addr = 0x1402000a;
    nic.ip_addr_assigned = 1;
    nic.mtu = 1500;
    ASSERT(0 == add_route(net_msg->ip_dest, inet_addr("255.255.0.0"), "eth0"));
    /*
     * Now call ip_tx_msg
     */
    wq_schedule_called = 0;
    __net_loglevel = 0;
    ASSERT(0 == ip_tx_msg(net_msg));
    __net_loglevel = 0;
    /*
     * and verify result. As we exceed the MTU, we should have created two messages,
     * the first one being our original message
     */
    ASSERT(4 == wq_schedule_called);
    ASSERT(tx_net_msg[0] == net_msg);
    ASSERT(tx_net_msg[3]);
    ASSERT(tx_net_msg[3] != net_msg);
    /*
     * Get ID from first message
     */
    ip_hdr = (ip_hdr_t*) net_msg->start;
    id = ntohs(ip_hdr->id);
    net_msg = tx_net_msg[1];
    /*
     * Now check fields in net_msg which now points to second message
     */
    ASSERT(net_msg->nic == our_nic);
    ASSERT(0x800 == ntohs(net_msg->ethertype));
    /*
     * and IP header
     */
    ip_hdr = (ip_hdr_t*) net_msg->start;
    ASSERT(ip_hdr->version == 0x45);
    /*
     * Our total payload is 5000 bytes. With the first fragment, we have transmitted 1480 data bytes, i.e. there are
     * 5000 - 1480 = 3520 data bytes left. So our second fragment should be 1500 bytes again and contain another 1480
     * data bytes, leaving us with 2040 bytes. The third fragment should again contain 1500 bytes, and finally fragment
     * #4 should have 560 data bytes
     */
    ASSERT(ip_hdr->length == htons(1480 + 20));
    /*
     * IP checksum should be zero
     */
    ASSERT(0 == validate_ip_checksum(20, (unsigned short*) ip_hdr));
    /*
     * ID should be different from zero and equal to ID of first fragment
     */
    ASSERT(ip_hdr->id);
    ASSERT(ntohs(ip_hdr->id) == id);
    /*
     * and flags should be 0x0000 (DF = 0, MF = 1), offset should be 1480  / 8 = 0xb9
     */
    ASSERT(ip_hdr->flags == htons(0xb9 + (1 << 13)));
    /*
     * Verify remaining fields
     */
    ASSERT(ip_hdr->ip_dest == 0x1502000a);
    ASSERT(ip_hdr->ip_src == 0x1402000a);
    ASSERT(ip_hdr->proto == IP_PROTO_UDP);
    ASSERT(ip_hdr->version == 0x45);
    ASSERT(ip_hdr->ttl == IP_DEFAULT_TTL);
    /*
     * and data
     */
    for (i = 0; i < 1480; i++) {
        if (((unsigned char*) net_msg->start + sizeof(ip_hdr_t))[i] != ((i + 1480) % 256))
            printf("Found difference at index %d \n", i);
        ASSERT(((unsigned char*) net_msg->start + sizeof(ip_hdr_t))[i] == (i + 1480) % 256);
    }
    return 0;
}

/*
 * Testcase 8: ip_tx_msg
 * Call ip_tx_msg with a message which requires fragmentation with DF flag set
 */
int testcase8() {
    net_msg_t* net_msg;
    ip_hdr_t* ip_hdr;
    unsigned int id;
    nic_t nic;
    int i;
    unsigned char* data;
    unsigned int created;
    unsigned int destroyed;

    
    ip_init();
    net_init();
    /*
     * Create net_msg and simulate 2000 bytes of data (IP payload)
     */
    net_msg = net_msg_new(8192);
    data = net_msg_append(net_msg, 2000);
    ASSERT(data);
    for (i = 0; i < 2000; i++)
        data[i] = i;
    ASSERT(net_msg_get_size(net_msg) == 2000);
    /*
     * and set ip_proto field as well as IP destination address
     * and IP source address
     */
    net_msg->ip_proto = IP_PROTO_UDP;
    net_msg->ip_dest = 0x1502000a;
    net_msg->ip_src = 0x1402000a;
    net_msg->ip_df = 1;
    /*
     * We need to simulate a NIC which is returned by our stub for net_if_get_nic
     */
    our_nic = &nic;
    nic.ip_addr = 0x1402000a;
    nic.ip_addr_assigned = 1;
    nic.mtu = 1500;
    ASSERT(0 == add_route(net_msg->ip_dest, inet_addr("255.255.0.0"), "eth0"));
    /*
     * Now call ip_tx_msg
     */
    wq_schedule_called = 0;
    __net_loglevel = 0;
    ASSERT(-143 == ip_tx_msg(net_msg));
    __net_loglevel = 0;
    /*
     * and verify result.
     */
    ASSERT(0 == wq_schedule_called);
    /*
     * and make sure that number of created network messages equals calls to wq_schedule
     */
    net_get_counters(&created, &destroyed);
    ASSERT(created == destroyed + wq_schedule_called);
    return 0;
}

/*
 * Testcase 9: ip_tx_msg
 * Call ip_tx_msg to transmit a single, unfragmented IP message and verify the resulting call to
 * wq_schedule - case DF not set
 */
int testcase9() {
    net_msg_t* net_msg;
    ip_hdr_t* ip_hdr;
    nic_t nic;
    int i;
    unsigned char* data;
    unsigned int created;
    unsigned int destroyed;
    
    
    ip_init();
    net_init();
    /*
     * Create net_msg and simulate 100 bytes of data
     */
    net_msg = net_msg_new(256);
    data = net_msg_append(net_msg, 100);
    for (i = 0; i < 100; i++)
        data[i] = i;
    /*
     * and set ip_proto field as well as IP destination address
     * and IP source address
     */
    net_msg->ip_proto = IP_PROTO_UDP;
    net_msg->ip_dest = 0x1502000a;
    net_msg->ip_src = 0x1402000a;
    net_msg->ip_df = 0;
    /*
     * We need to simulate a NIC which is returned by our stub for net_if_get_nic
     */
    our_nic = &nic;
    nic.ip_addr = 0x1402000a;
    nic.ip_addr_assigned = 1;
    nic.mtu = 128; 
    ASSERT(0 == add_route(net_msg->ip_dest, inet_addr("255.255.0.0"), "eth0"));
    /*
     * Now call ip_tx_msg
     */
    wq_schedule_called = 0;
    ip_tx_msg(net_msg);
    /*
     * and verify result
     */
    ASSERT(1 == wq_schedule_called);
    ASSERT(tx_net_msg[0] == net_msg);
    /*
     * Now check fields in net_msg
     */
    ASSERT(net_msg->nic == our_nic);
    ASSERT(0x800 == ntohs(net_msg->ethertype));
    /*
     * and verify contents of IP header
     * - ID is 0 as we do not use fragmentation
     * - Flags are 0x0 (DF = 0, MF = 0)
     * - IP_SRC and IP_DEST are as specified in the network message
     * - IP length is length of network message plus size of header
     * - IP protocol is as specified
     * - IP version is 0x45 (IP version 4, header length 5 dwords)
     * - TTL is 64
     */

    ip_hdr = (ip_hdr_t*) net_msg->start;

    ASSERT(ip_hdr->id == 0);
    ASSERT(ip_hdr->flags == htons(0x0));
    ASSERT(ip_hdr->ip_dest == 0x1502000a);
    ASSERT(ip_hdr->ip_src == 0x1402000a);
    ASSERT(ip_hdr->length == htons(100 + 20));
    ASSERT(ip_hdr->proto == IP_PROTO_UDP);
    ASSERT(ip_hdr->version == 0x45);
    ASSERT(ip_hdr->ttl == IP_DEFAULT_TTL);

    /*
     * IP checksum should be zero
     */
    ASSERT(0 == validate_ip_checksum(20, (unsigned short*) ip_hdr));

    /*
     * Finally verify data
     */
    for (i = 0; i < 100; i++)
        ASSERT(((unsigned char*) net_msg->start + sizeof(ip_hdr_t))[i] == i);
    /*
     * and make sure that number of created network messages equals calls to wq_schedule
     */
 
    net_get_counters(&created, &destroyed);
    ASSERT(created == destroyed + wq_schedule_called);

    return 0;
}

/*
 * Testcase 10: ip_tx_msg
 * Call ip_tx_msg with a message which requires fragmentation. Verify first fragment - special case that
 * MTU is not a multiple of eight
 */
int testcase10() {
    net_msg_t* net_msg;
    ip_hdr_t* ip_hdr;
    nic_t nic;
    unsigned char* data;
    int i;
    net_init();
    ip_init();
    /*
     * Create net_msg and simulate 2000 bytes of data (IP payload)
     */
    net_msg = net_msg_new(8192);
    data = net_msg_append(net_msg, 2000);
    ASSERT(data);
    ASSERT(net_msg_get_size(net_msg) == 2000);
    for (i = 0; i < 2000; i++)
        data[i] = i;
    /*
     * and set ip_proto field as well as IP destination address
     * and IP source address
     */
    net_msg->ip_proto = IP_PROTO_UDP;
    net_msg->ip_dest = 0x1502000a;
    net_msg->ip_src = 0x1402000a;
    net_msg->ip_df = 0;
    /*
     * We need to simulate a NIC which is returned by our stub for net_if_get_nic
     */
    our_nic = &nic;
    nic.ip_addr = 0x1402000a;
    nic.ip_addr_assigned = 1;
    nic.mtu = 1495;
    ASSERT(0 == add_route(net_msg->ip_dest, inet_addr("255.255.0.0"), "eth0"));
    /*
     * Now call ip_tx_msg
     */
    wq_schedule_called = 0;
    __net_loglevel = 0;
    ip_tx_msg(net_msg);
    __net_loglevel = 0;
    /*
     * and verify result. As we exceed the MTU, we should have created two messages,
     * the first one being our original message
     */
    ASSERT(2 == wq_schedule_called);
    ASSERT(tx_net_msg[0] == net_msg);
    /*
     * Now check fields in net_msg
     */
    ASSERT(net_msg->nic == our_nic);
    ASSERT(0x800 == ntohs(net_msg->ethertype));
    /*
     * and IP header
     */
    ip_hdr = (ip_hdr_t*) net_msg->start;
    ASSERT(ip_hdr->version == 0x45);
    /*
     * The first fragment should be 1495 bytes including 20 bytes for the header, i.e. 1475 data bytes. However,
     * as the fragment size needs to be a multiple of eight, we expect in fact that we have 1472 data bytes only
     */
    ASSERT(ip_hdr->length == htons(1472 + 20));
    /*
     * IP checksum should be zero
     */
    ASSERT(0 == validate_ip_checksum(20, (unsigned short*) ip_hdr));
    /*
     * ID should be different from zero
     */
    ASSERT(ip_hdr->id);
    /*
     * and flags should be 0x2000 (DF = 0, MF = 1)
     */
    ASSERT(ip_hdr->flags == htons(0x2000));
    /*
     * Verify remaining fields
     */
    ASSERT(ip_hdr->ip_dest == 0x1502000a);
    ASSERT(ip_hdr->ip_src == 0x1402000a);
    ASSERT(ip_hdr->proto == IP_PROTO_UDP);
    ASSERT(ip_hdr->version == 0x45);
    ASSERT(ip_hdr->ttl == IP_DEFAULT_TTL);
    /*
     * and data
     */
    for (i = 0; i < 1472; i++) {
        if (((unsigned char*) net_msg->start + sizeof(ip_hdr_t))[i] != (i % 256))
            printf("Found difference at index %d \n", i);
        ASSERT(((unsigned char*) net_msg->start + sizeof(ip_hdr_t))[i] == i % 256);
    }
    /*
     * and make sure that number of created network messages equals calls to wq_schedule
     */
    unsigned int created;
    unsigned int destroyed;
    net_get_counters(&created, &destroyed);
    ASSERT(created == destroyed + wq_schedule_called);
    return 0;
}

/*
 * Testcase 11: ip_tx_msg
 * Call ip_tx_msg with a message which requires fragmentation. Verify second (and last) fragment - special case of
 * MTU not being a multiple of eight
 */
int testcase11() {
    net_msg_t* net_msg;
    ip_hdr_t* ip_hdr;
    unsigned int id;
    nic_t nic;
    int i;
    unsigned char* data;
    ip_init();
    /*
     * Create net_msg and simulate 2000 bytes of data (IP payload)
     */
    net_msg = net_msg_new(8192);
    data = net_msg_append(net_msg, 2000);
    ASSERT(data);
    for (i = 0; i < 2000; i++)
        data[i] = i;
    ASSERT(net_msg_get_size(net_msg) == 2000);
    /*
     * and set ip_proto field as well as IP destination address
     * and IP source address
     */
    net_msg->ip_proto = IP_PROTO_UDP;
    net_msg->ip_dest = 0x1502000a;
    net_msg->ip_src = 0x1402000a;
    net_msg->ip_df = 0;
    /*
     * We need to simulate a NIC which is returned by our stub for net_if_get_nic
     */
    our_nic = &nic;
    nic.ip_addr = 0x1402000a;
    nic.ip_addr_assigned = 1;
    nic.mtu = 1495;
    ASSERT(0 == add_route(net_msg->ip_dest, inet_addr("255.255.0.0"), "eth0"));
    /*
     * Now call ip_tx_msg
     */
    wq_schedule_called = 0;
    __net_loglevel = 0;
    ip_tx_msg(net_msg);
    __net_loglevel = 0;
    /*
     * and verify result. As we exceed the MTU, we should have created two messages,
     * the first one being our original message
     */
    ASSERT(2 == wq_schedule_called);
    ASSERT(tx_net_msg[0] == net_msg);
    ASSERT(tx_net_msg[1]);
    ASSERT(tx_net_msg[1] != net_msg);
    /*
     * Get ID from first message
     */
    ip_hdr = (ip_hdr_t*) net_msg->start;
    id = ntohs(ip_hdr->id);
    net_msg = tx_net_msg[1];
    /*
     * Now check fields in net_msg
     */
    ASSERT(net_msg->nic == our_nic);
    ASSERT(0x800 == ntohs(net_msg->ethertype));
    /*
     * and IP header
     */
    ip_hdr = (ip_hdr_t*) net_msg->start;
    ASSERT(ip_hdr->version == 0x45);
    /*
     * Our total payload is 2000 bytes. With the first fragment, we have transmitted 1472 data bytes, i.e. there are
     * 2000 - 1472 = 528 data bytes left. So our second fragment should be 528 + 20 = 548 bytes
     */
    ASSERT(ip_hdr->length == htons(548));
    /*
     * IP checksum should be zero
     */
    ASSERT(0 == validate_ip_checksum(20, (unsigned short*) ip_hdr));
    /*
     * ID should be different from zero and equal to ID of first fragment
     */
    ASSERT(ip_hdr->id);
    ASSERT(ntohs(ip_hdr->id) == id);
    /*
     * and flags should be 0x0000 (DF = 0, MF = 0), offset should be 1472  / 8 = 0xb8
     */
    ASSERT(ip_hdr->flags == htons(0xb8));
    /*
     * Verify remaining fields
     */
    ASSERT(ip_hdr->ip_dest == 0x1502000a);
    ASSERT(ip_hdr->ip_src == 0x1402000a);
    ASSERT(ip_hdr->proto == IP_PROTO_UDP);
    ASSERT(ip_hdr->version == 0x45);
    ASSERT(ip_hdr->ttl == IP_DEFAULT_TTL);
    /*
     * and data
     */
    for (i = 0; i < 528; i++) {
        if (((unsigned char*) net_msg->start + sizeof(ip_hdr_t))[i] != ((i + 1472) % 256))
            printf("Found difference at index %d \n", i);
        ASSERT(((unsigned char*) net_msg->start + sizeof(ip_hdr_t))[i] == (i + 1472) % 256);
    }
    return 0;
}

/*
 * Testcase 12: receive an ICMP message
 */
int testcase12() {
    int i;
    unsigned char* data;
    ip_hdr_t* ip_hdr;
    nic_t nic;
    /*
     * Create network message
     */
    net_init();
    net_msg_t* net_msg = net_msg_new(256);
    ASSERT(net_msg);
    net_msg->nic = &nic;
    nic.ip_addr_assigned = 1;
    nic.ip_addr = 0x1402000a;
    data = net_msg_append(net_msg, 100);
    ASSERT(data);
    ip_hdr = (ip_hdr_t*) net_msg_prepend(net_msg, sizeof(ip_hdr_t));
    net_msg->ip_hdr = (void*) ip_hdr;
    ip_hdr->checksum = 0;
    ip_hdr->flags = ntohs(0x4000);
    ip_hdr->id = 0;
    ip_hdr->ip_dest = 0x1402000a;
    ip_hdr->ip_src = 0x1502000a;
    ip_hdr->length = ntohs(100 + sizeof(ip_hdr_t));
    ip_hdr->proto = IP_PROTO_ICMP;
    ip_hdr->ttl = 64;
    ip_hdr->version = 0x45;
    ip_hdr->checksum = htons(validate_ip_checksum(sizeof(ip_hdr_t), (u16*) ip_hdr));
    for (i = 0; i < 100; i++)
        data[i] = i;
    /*
     * and call ip_rx_msg
     */
    icmp_rx_msg_called = 0;
    __net_loglevel = 0;
    ip_rx_msg(net_msg);
    __net_loglevel = 0;
    /*
     * This should have passed the message to icmp_rx_msg
     */
    ASSERT(1 == icmp_rx_msg_called);
    ASSERT(net_msg == icmp_msg);
    /*
     * and the fields ip_src, ip_dst and ip_length should have been set
     */
    ASSERT(0x1502000a == net_msg->ip_src);
    ASSERT(0x1402000a == net_msg->ip_dest);
    ASSERT(100 == net_msg->ip_length);
    /*
     * The ICMP header pointer should be set as well
     */
    ASSERT(net_msg->icmp_hdr == net_msg->ip_hdr + sizeof(ip_hdr_t));
    /*
     * and make sure that number of created network messages equals number of destroyed messages
     * plus number of calls to icmp_rx_msg
     */
    unsigned int created;
    unsigned int destroyed;
    net_get_counters(&created, &destroyed);
    ASSERT(created == destroyed + icmp_rx_msg_called);
    return 0;
}

/*
 * Testcase 13: receive a TCP message
 */
int testcase13() {
    int i;
    unsigned char* data;
    ip_hdr_t* ip_hdr;
    nic_t nic;
    /*
     * Create network message
     */
    net_init();
    net_msg_t* net_msg = net_msg_new(256);
    ASSERT(net_msg);
    net_msg->nic = &nic;
    nic.ip_addr_assigned = 1;
    nic.ip_addr_assigned = 1;
    nic.ip_addr = 0x1402000a;
    data = net_msg_append(net_msg, 100);
    ASSERT(data);
    ip_hdr = (ip_hdr_t*) net_msg_prepend(net_msg, sizeof(ip_hdr_t));
    net_msg->ip_hdr = (void*) ip_hdr;
    ip_hdr->checksum = 0;
    ip_hdr->flags = ntohs(0x4000);
    ip_hdr->id = 0;
    ip_hdr->ip_dest = 0x1402000a;
    ip_hdr->ip_src = 0x1502000a;
    ip_hdr->length = ntohs(100 + sizeof(ip_hdr_t));
    ip_hdr->proto = IP_PROTO_TCP;
    ip_hdr->ttl = 64;
    ip_hdr->version = 0x45;
    ip_hdr->checksum = htons(validate_ip_checksum(sizeof(ip_hdr_t), (u16*) ip_hdr));
    for (i = 0; i < 100; i++)
        data[i] = i;
    /*
     * and call ip_rx_msg
     */
    tcp_rx_msg_called = 0;
    __net_loglevel = 0;
    ip_rx_msg(net_msg);
    __net_loglevel = 0;
    /*
     * This should have passed the message to icmp_rx_msg
     */
    ASSERT(1 == tcp_rx_msg_called);
    ASSERT(net_msg == tcp_msg);
    /*
     * and the fields ip_src, ip_dst and ip_length should have been set
     */
    ASSERT(0x1502000a == net_msg->ip_src);
    ASSERT(0x1402000a == net_msg->ip_dest);
    ASSERT(100 == net_msg->ip_length);
    /*
     * The TCP header pointer should be set as well
     */
    ASSERT(net_msg->tcp_hdr == net_msg->ip_hdr + sizeof(ip_hdr_t));
    /*
     * and make sure that number of created network messages equals number of destroyed messages
     * plus number of calls to tcp_rx_msg
     */
    unsigned int created;
    unsigned int destroyed;
    net_get_counters(&created, &destroyed);
    ASSERT(created == destroyed + tcp_rx_msg_called);
    return 0;
}

/*
 * Testcase 14: receive a TCP message with invalid checksum
 */
int testcase14() {
    int i;
    unsigned char* data;
    ip_hdr_t* ip_hdr;
    nic_t nic;
    /*
     * Create network message
     */
    net_init();
    net_msg_t* net_msg = net_msg_new(256);
    ASSERT(net_msg);
    data = net_msg_append(net_msg, 100);
    net_msg->nic = &nic;
    nic.ip_addr_assigned = 1;
    nic.ip_addr = 0x1402000a;
    ASSERT(data);
    ip_hdr = (ip_hdr_t*) net_msg_prepend(net_msg, sizeof(ip_hdr_t));
    net_msg->ip_hdr = (void*) ip_hdr;
    ip_hdr->checksum = 0;
    ip_hdr->flags = ntohs(0x4000);
    ip_hdr->id = 0;
    ip_hdr->ip_dest = 0x1402000a;
    ip_hdr->ip_src = 0x1502000a;
    ip_hdr->length = ntohs(100 + sizeof(ip_hdr_t));
    ip_hdr->proto = IP_PROTO_TCP;
    ip_hdr->ttl = 64;
    ip_hdr->version = 0x45;
    ip_hdr->checksum = htons(999 + validate_ip_checksum(sizeof(ip_hdr_t), (u16*) ip_hdr));
    for (i = 0; i < 100; i++)
        data[i] = i;
    /*
     * and call ip_rx_msg
     */
    tcp_rx_msg_called = 0;
    __net_loglevel = 0;
    do_putchar = 0;
    ip_rx_msg(net_msg);
    __net_loglevel = 0;
    do_putchar = 1;
    /*
     * This should NOT have passed the message to tcp_rx_msg
     */
    ASSERT(0 == tcp_rx_msg_called);
    /*
     * and make sure that number of created network messages equals number of destroyed messages
     * plus number of calls to icmp_rx_msg
     */
    unsigned int created;
    unsigned int destroyed;
    net_get_counters(&created, &destroyed);
    ASSERT(created == destroyed + tcp_rx_msg_called);
    return 0;
}

/*
 * Testcase 15: receive an ICMP message consisting of two fragments  - no overlap, first fragment received first
 */
int testcase15() {
    int i;
    unsigned char* data;
    ip_hdr_t* ip_hdr;
    nic_t nic;
    /*
     * Create network message for first fragment - 1480 IP data bytes
     */
    net_init();
    net_msg_t* net_msg = net_msg_new(1480);
    ASSERT(net_msg);
    data = net_msg_append(net_msg, 1480);
    ASSERT(data);
    ip_hdr = (ip_hdr_t*) net_msg_prepend(net_msg, sizeof(ip_hdr_t));
    net_msg->ip_hdr = (void*) ip_hdr;
    net_msg->eth_hdr = (void*) net_msg_prepend(net_msg, 14);
    net_msg->nic = &nic;
    nic.ip_addr_assigned = 1;
    nic.ip_addr = 0x1402000a;
    ip_hdr->checksum = 0;
    ip_hdr->flags = ntohs(0x2000);
    ip_hdr->id = 101;
    ip_hdr->ip_dest = 0x1402000a;
    ip_hdr->ip_src = 0x1502000a;
    ip_hdr->length = ntohs(1480 + sizeof(ip_hdr_t));
    ip_hdr->proto = IP_PROTO_ICMP;
    ip_hdr->ttl = 64;
    ip_hdr->version = 0x45;
    ip_hdr->checksum = htons(validate_ip_checksum(sizeof(ip_hdr_t), (u16*) ip_hdr));
    for (i = 0; i < 1480; i++)
        data[i] = i;
    /*
     * and call ip_rx_msg
     */
    icmp_rx_msg_called = 0;
    __net_loglevel = 0;
    ip_rx_msg(net_msg);
    __net_loglevel = 0;
    /*
     * This should NOT have passed the message to icmp_rx_msg
     */
    ASSERT(0 == icmp_rx_msg_called);
    /*
     * Now assemble and send second packet - data byte 1481 - 1490
     */
    net_msg = net_msg_new(10);
    ASSERT(net_msg);
    data = net_msg_append(net_msg, 10);
    net_msg->nic = &nic;
    ASSERT(data);
    ip_hdr = (ip_hdr_t*) net_msg_prepend(net_msg, sizeof(ip_hdr_t));
    net_msg->ip_hdr = (void*) ip_hdr;
    net_msg->nic = &nic;
    net_msg->eth_hdr = (void*) net_msg_prepend(net_msg, 14);
    ip_hdr->checksum = 0;
    ip_hdr->flags = ntohs(0xb9);
    ip_hdr->id = 101;
    ip_hdr->ip_dest = 0x1402000a;
    ip_hdr->ip_src = 0x1502000a;
    ip_hdr->length = ntohs(10 + sizeof(ip_hdr_t));
    ip_hdr->proto = IP_PROTO_ICMP;
    ip_hdr->ttl = 64;
    ip_hdr->version = 0x45;
    ip_hdr->checksum = htons(validate_ip_checksum(sizeof(ip_hdr_t), (u16*) ip_hdr));
    for (i = 0; i < 10; i++)
        data[i] = i + 1480;
    /*
     * and call icmp_rx_msg
     */
    __net_loglevel = 0;
    ip_rx_msg(net_msg);
    __net_loglevel = 0;
    ASSERT(1 == icmp_rx_msg_called);
    /*
     * Now verify fields of message. Let us start with the IP header
     */
    ip_hdr = (ip_hdr_t*) icmp_msg->ip_hdr;
    ASSERT(ip_hdr);
    ASSERT(ip_hdr->ip_src == 0x1502000a);
    ASSERT(ip_hdr->ip_dest == 0x1402000a);
    ASSERT(ip_hdr->flags == 0);
    ASSERT(ip_hdr->id == 101);
    ASSERT(ntohs(ip_hdr->length) == sizeof(ip_hdr_t) + 1490);
    ASSERT(ip_hdr->proto == IP_PROTO_ICMP);
    ASSERT(ip_hdr->ttl == 64);
    ASSERT(ip_hdr->version == 0x45);
    /*
     * Now check fields in network message. We expect that all fields which are set by the
     * IP layer are also set for the reassembled message
     */
    ASSERT(icmp_msg->ip_hdr);
    ASSERT(icmp_msg->icmp_hdr);
    ASSERT(icmp_msg->ip_length == 1490);
    ASSERT(ip_hdr->ip_src == icmp_msg->ip_src);
    ASSERT(ip_hdr->ip_dest == icmp_msg->ip_dest);
    ASSERT(icmp_msg->ip_proto == IP_PROTO_ICMP);
    /*
     * Finally verify data
     */
    data = (unsigned char*) ip_hdr + sizeof(ip_hdr_t);
    for (i = 0; i < 1490; i++)
        ASSERT(data[i] == (i % 256));
    /*
     * and make sure that number of created network messages equals number of destroyed messages
     * plus number of calls to icmp_rx_msg
     */
    unsigned int created;
    unsigned int destroyed;
    net_get_counters(&created, &destroyed);
    ASSERT(created == destroyed + icmp_rx_msg_called);
    return 0;
}

/*
 * Testcase 16: receive an ICMP message consisting of two fragments  - no overlap, first fragment received last
 */
int testcase16() {
    int i;
    unsigned char* data;
    ip_hdr_t* ip_hdr;
    nic_t nic;
    /*
     * First assemble and send second packet - data byte 1481 - 1490
     */
    net_init();
    net_msg_t* net_msg = net_msg_new(10);
    ASSERT(net_msg);
    data = net_msg_append(net_msg, 10);
    ASSERT(data);
    ip_hdr = (ip_hdr_t*) net_msg_prepend(net_msg, sizeof(ip_hdr_t));
    net_msg->ip_hdr = (void*) ip_hdr;
    net_msg->nic = &nic;
    nic.ip_addr_assigned = 1;
    nic.ip_addr = 0x1402000a;
    net_msg->eth_hdr = (void*) net_msg_prepend(net_msg, 14);
    ip_hdr->checksum = 0;
    ip_hdr->flags = ntohs(0xb9);
    ip_hdr->id = 101;
    ip_hdr->ip_dest = 0x1402000a;
    ip_hdr->ip_src = 0x1502000a;
    ip_hdr->length = ntohs(10 + sizeof(ip_hdr_t));
    ip_hdr->proto = IP_PROTO_ICMP;
    ip_hdr->ttl = 64;
    ip_hdr->version = 0x45;
    ip_hdr->checksum = htons(validate_ip_checksum(sizeof(ip_hdr_t), (u16*) ip_hdr));
    for (i = 0; i < 10; i++)
        data[i] = i + 1480;
    /*
     * and call ip_rx_msg
     */
    icmp_rx_msg_called = 0;
    __net_loglevel = 0;
    ip_rx_msg(net_msg);
    __net_loglevel = 0;
    /*
     * This should NOT have passed the message to icmp_rx_msg
     */
    ASSERT(0 == icmp_rx_msg_called);
    /*
     * Create network message for first fragment - 1480 IP data bytes
     */
    net_msg = net_msg_new(1480);
    ASSERT(net_msg);
    data = net_msg_append(net_msg, 1480);
    net_msg->nic = &nic;
    ASSERT(data);
    ip_hdr = (ip_hdr_t*) net_msg_prepend(net_msg, sizeof(ip_hdr_t));
    net_msg->ip_hdr = (void*) ip_hdr;
    net_msg->eth_hdr = (void*) net_msg_prepend(net_msg, 14);
    net_msg->nic = &nic;
    ip_hdr->checksum = 0;
    ip_hdr->flags = ntohs(0x2000);
    ip_hdr->id = 101;
    ip_hdr->ip_dest = 0x1402000a;
    ip_hdr->ip_src = 0x1502000a;
    ip_hdr->length = ntohs(1480 + sizeof(ip_hdr_t));
    ip_hdr->proto = IP_PROTO_ICMP;
    ip_hdr->ttl = 64;
    ip_hdr->version = 0x45;
    ip_hdr->checksum = htons(validate_ip_checksum(sizeof(ip_hdr_t), (u16*) ip_hdr));
    for (i = 0; i < 1480; i++)
        data[i] = i;
    /*
     * and call icmp_rx_msg
     */
    __net_loglevel = 0;
    ip_rx_msg(net_msg);
    __net_loglevel = 0;
    ASSERT(1 == icmp_rx_msg_called);
    /*
     * Now verify fields of message. Let us start with the IP header
     */
    ip_hdr = (ip_hdr_t*) icmp_msg->ip_hdr;
    ASSERT(ip_hdr);
    ASSERT(ip_hdr->ip_src == 0x1502000a);
    ASSERT(ip_hdr->ip_dest == 0x1402000a);
    ASSERT(ip_hdr->flags == 0);
    ASSERT(ip_hdr->id == 101);
    ASSERT(ntohs(ip_hdr->length) == sizeof(ip_hdr_t) + 1490);
    ASSERT(ip_hdr->proto == IP_PROTO_ICMP);
    ASSERT(ip_hdr->ttl == 64);
    ASSERT(ip_hdr->version == 0x45);
    /*
     * Now check fields in network message. We expect that all fields which are set by the
     * IP layer are also set for the reassembled message
     */
    ASSERT(icmp_msg->ip_hdr);
    ASSERT(icmp_msg->icmp_hdr);
    ASSERT(icmp_msg->ip_length == 1490);
    ASSERT(ip_hdr->ip_src == icmp_msg->ip_src);
    ASSERT(ip_hdr->ip_dest == icmp_msg->ip_dest);
    ASSERT(icmp_msg->ip_proto == IP_PROTO_ICMP);
    /*
     * Finally verify data
     */
    data = (unsigned char*) ip_hdr + sizeof(ip_hdr_t);
    for (i = 0; i < 1490; i++)
        ASSERT(data[i] == (i % 256));
    /*
     * and make sure that number of created network messages equals number of destroyed messages
     * plus number of calls to icmp_rx_msg
     */
    unsigned int created;
    unsigned int destroyed;
    net_get_counters(&created, &destroyed);
    ASSERT(created == destroyed + icmp_rx_msg_called);
    return 0;
}

/*
 * Testcase 17: receive an ICMP message consisting of two fragments  - overlap, first fragment received first
 */
int testcase17() {
    int i;
    unsigned char* data;
    ip_hdr_t* ip_hdr;
    nic_t nic;
    /*
     * Create network message for first fragment - 1480 IP data bytes
     */
    net_msg_t* net_msg = net_msg_new(1480);
    ASSERT(net_msg);
    data = net_msg_append(net_msg, 1480);
    ASSERT(data);
    ip_hdr = (ip_hdr_t*) net_msg_prepend(net_msg, sizeof(ip_hdr_t));
    net_msg->ip_hdr = (void*) ip_hdr;
    net_msg->eth_hdr = (void*) net_msg_prepend(net_msg, 14);
    net_msg->nic = &nic;
    nic.ip_addr_assigned = 1;
    nic.ip_addr = 0x1402000a;
    ip_hdr->checksum = 0;
    ip_hdr->flags = ntohs(0x2000);
    ip_hdr->id = 101;
    ip_hdr->ip_dest = 0x1402000a;
    ip_hdr->ip_src = 0x1502000a;
    ip_hdr->length = ntohs(1480 + sizeof(ip_hdr_t));
    ip_hdr->proto = IP_PROTO_ICMP;
    ip_hdr->ttl = 64;
    ip_hdr->version = 0x45;
    ip_hdr->checksum = htons(validate_ip_checksum(sizeof(ip_hdr_t), (u16*) ip_hdr));
    for (i = 0; i < 1480; i++)
        data[i] = i;
    /*
     * and call ip_rx_msg
     */
    icmp_rx_msg_called = 0;
    __net_loglevel = 0;
    ip_rx_msg(net_msg);
    __net_loglevel = 0;
    /*
     * This should NOT have passed the message to icmp_rx_msg
     */
    ASSERT(0 == icmp_rx_msg_called);
    /*
     * Now assemble and send second packet - this packet contains data bytes 1472 - 1489, i.e. we have 8 bytes
     * overlap
     */
    net_msg = net_msg_new(20);
    ASSERT(net_msg);
    net_msg->nic = &nic;
    data = net_msg_append(net_msg, 18);
    ASSERT(data);
    ip_hdr = (ip_hdr_t*) net_msg_prepend(net_msg, sizeof(ip_hdr_t));
    net_msg->ip_hdr = (void*) ip_hdr;
    net_msg->nic = &nic;
    net_msg->eth_hdr = (void*) net_msg_prepend(net_msg, 14);
    ip_hdr->checksum = 0;
    ip_hdr->flags = ntohs(1472 / 8);
    ip_hdr->id = 101;
    ip_hdr->ip_dest = 0x1402000a;
    ip_hdr->ip_src = 0x1502000a;
    ip_hdr->length = ntohs(18 + sizeof(ip_hdr_t));
    ip_hdr->proto = IP_PROTO_ICMP;
    ip_hdr->ttl = 64;
    ip_hdr->version = 0x45;
    ip_hdr->checksum = htons(validate_ip_checksum(sizeof(ip_hdr_t), (u16*) ip_hdr));
    for (i = 0; i < 18; i++)
        data[i] = i + 1472;
    /*
     * and call icmp_rx_msg
     */
    __net_loglevel = 0;
    ip_rx_msg(net_msg);
    __net_loglevel = 0;
    ASSERT(1 == icmp_rx_msg_called);
    /*
     * Now verify fields of message. Let us start with the IP header
     */
    ip_hdr = (ip_hdr_t*) icmp_msg->ip_hdr;
    ASSERT(ip_hdr);
    ASSERT(ip_hdr->ip_src == 0x1502000a);
    ASSERT(ip_hdr->ip_dest == 0x1402000a);
    ASSERT(ip_hdr->flags == 0);
    ASSERT(ip_hdr->id == 101);
    ASSERT(ntohs(ip_hdr->length) == sizeof(ip_hdr_t) + 1490);
    ASSERT(ip_hdr->proto == IP_PROTO_ICMP);
    ASSERT(ip_hdr->ttl == 64);
    ASSERT(ip_hdr->version == 0x45);
    /*
     * Now check fields in network message. We expect that all fields which are set by the
     * IP layer are also set for the reassembled message if they make sense, i.e. are valid on the
     * IP level
     */
    ASSERT(icmp_msg->ip_hdr);
    ASSERT(icmp_msg->icmp_hdr);
    ASSERT(icmp_msg->ip_length == 1490);
    ASSERT(ip_hdr->ip_src == icmp_msg->ip_src);
    ASSERT(ip_hdr->ip_dest == icmp_msg->ip_dest);
    ASSERT(icmp_msg->ip_proto == IP_PROTO_ICMP);
    /*
     * Finally verify data
     */
    data = (unsigned char*) ip_hdr + sizeof(ip_hdr_t);
    for (i = 0; i < 1490; i++)
        ASSERT(data[i] == (i % 256));
    return 0;
}

/*
 * Testcase 18: receive an ICMP message consisting of three fragments  - no overlap, first fragment received first
 */
int testcase18() {
    int i;
    unsigned char* data;
    ip_hdr_t* ip_hdr;
    nic_t nic;
    /*
     * Create network message for first fragment - 1480 IP data bytes
     */
    net_msg_t* net_msg = net_msg_new(1480);
    ASSERT(net_msg);
    data = net_msg_append(net_msg, 1480);
    ASSERT(data);
    ip_hdr = (ip_hdr_t*) net_msg_prepend(net_msg, sizeof(ip_hdr_t));
    net_msg->ip_hdr = (void*) ip_hdr;
    net_msg->eth_hdr = (void*) net_msg_prepend(net_msg, 14);
    net_msg->nic = &nic;
    nic.ip_addr_assigned = 1;
    nic.ip_addr = 0x1402000a;
    ip_hdr->checksum = 0;
    ip_hdr->flags = ntohs(0x2000);
    ip_hdr->id = 101;
    ip_hdr->ip_dest = 0x1402000a;
    ip_hdr->ip_src = 0x1502000a;
    ip_hdr->length = ntohs(1480 + sizeof(ip_hdr_t));
    ip_hdr->proto = IP_PROTO_ICMP;
    ip_hdr->ttl = 64;
    ip_hdr->version = 0x45;
    ip_hdr->checksum = htons(validate_ip_checksum(sizeof(ip_hdr_t), (u16*) ip_hdr));
    for (i = 0; i < 1480; i++)
        data[i] = i;
    /*
     * and call ip_rx_msg
     */
    icmp_rx_msg_called = 0;
    __net_loglevel = 0;
    ip_rx_msg(net_msg);
    __net_loglevel = 0;
    /*
     * This should NOT have passed the message to icmp_rx_msg
     */
    ASSERT(0 == icmp_rx_msg_called);
    /*
     * Now assemble and send second packet, containing another 1480 data bytes
     */
    net_msg = net_msg_new(1480);
    ASSERT(net_msg);
    net_msg->nic = &nic;
    data = net_msg_append(net_msg, 1480);
    ASSERT(data);
    ip_hdr = (ip_hdr_t*) net_msg_prepend(net_msg, sizeof(ip_hdr_t));
    net_msg->ip_hdr = (void*) ip_hdr;
    net_msg->nic = &nic;
    net_msg->eth_hdr = (void*) net_msg_prepend(net_msg, 14);
    ip_hdr->checksum = 0;
    ip_hdr->flags = ntohs(1480 / 8 + 0x2000);
    ip_hdr->id = 101;
    ip_hdr->ip_dest = 0x1402000a;
    ip_hdr->ip_src = 0x1502000a;
    ip_hdr->length = ntohs(1480 + sizeof(ip_hdr_t));
    ip_hdr->proto = IP_PROTO_ICMP;
    ip_hdr->ttl = 64;
    ip_hdr->version = 0x45;
    ip_hdr->checksum = htons(validate_ip_checksum(sizeof(ip_hdr_t), (u16*) ip_hdr));
    for (i = 0; i < 1480; i++)
        data[i] = i + 1480;
    /*
     * and call icmp_rx_msg
     */
    __net_loglevel = 0;
    ip_rx_msg(net_msg);
    __net_loglevel = 0;
    ASSERT(0 == icmp_rx_msg_called);
    /*
     * Now assemble and send third packet, containing another 10 data bytes
     */
    net_msg = net_msg_new(10);
    ASSERT(net_msg);
    net_msg->nic = &nic;
    data = net_msg_append(net_msg, 10);
    ASSERT(data);
    ip_hdr = (ip_hdr_t*) net_msg_prepend(net_msg, sizeof(ip_hdr_t));
    net_msg->ip_hdr = (void*) ip_hdr;
    net_msg->nic = &nic;
    net_msg->eth_hdr = (void*) net_msg_prepend(net_msg, 14);
    ASSERT(net_msg->eth_hdr);
    ip_hdr->checksum = 0;
    ip_hdr->flags = ntohs(2*1480 / 8);
    ip_hdr->id = 101;
    ip_hdr->ip_dest = 0x1402000a;
    ip_hdr->ip_src = 0x1502000a;
    ip_hdr->length = ntohs(10 + sizeof(ip_hdr_t));
    ip_hdr->proto = IP_PROTO_ICMP;
    ip_hdr->ttl = 64;
    ip_hdr->version = 0x45;
    ip_hdr->checksum = htons(validate_ip_checksum(sizeof(ip_hdr_t), (u16*) ip_hdr));
    for (i = 0; i < 10; i++)
        data[i] = i + 2*1480;
    /*
     * and call icmp_rx_msg
     */
    __net_loglevel = 0;
    ip_rx_msg(net_msg);
    __net_loglevel = 0;
    ASSERT(1 == icmp_rx_msg_called);
    /*
     * Now verify fields of message. Let us start with the IP header
     */
    ip_hdr = (ip_hdr_t*) icmp_msg->ip_hdr;
    ASSERT(ip_hdr);
    ASSERT(ip_hdr->ip_src == 0x1502000a);
    ASSERT(ip_hdr->ip_dest == 0x1402000a);
    ASSERT(ip_hdr->flags == 0);
    ASSERT(ip_hdr->id == 101);
    ASSERT(ntohs(ip_hdr->length) == sizeof(ip_hdr_t) + 2*1480 + 10);
    ASSERT(ip_hdr->proto == IP_PROTO_ICMP);
    ASSERT(ip_hdr->ttl == 64);
    ASSERT(ip_hdr->version == 0x45);
    /*
     * Now check fields in network message. We expect that all fields which are set by the
     * IP layer are also set for the reassembled message
     */
    ASSERT(icmp_msg->ip_hdr);
    ASSERT(icmp_msg->icmp_hdr);
    ASSERT(icmp_msg->ip_length == 2*1480 + 10);
    ASSERT(ip_hdr->ip_src == icmp_msg->ip_src);
    ASSERT(ip_hdr->ip_dest == icmp_msg->ip_dest);
    ASSERT(icmp_msg->ip_proto == IP_PROTO_ICMP);
    /*
     * Finally verify data
     */
    data = (unsigned char*) ip_hdr + sizeof(ip_hdr_t);
    for (i = 0; i < 2*1480 + 10; i++)
        ASSERT(data[i] == (i % 256));
    return 0;
}

/*
 * Testcase 19: receive an ICMP message consisting of three fragments  - no overlap, fragments coming in in order 2, 1, 3
 */
int testcase19() {
    int i;
    unsigned char* data;
    ip_hdr_t* ip_hdr;
    nic_t nic;
    /*
     * Assemble and send second packet, containing 1480 data bytes
     */
    net_init();
    net_msg_t* net_msg = net_msg_new(1480);
    ASSERT(net_msg);
    data = net_msg_append(net_msg, 1480);
    ASSERT(data);
    ip_hdr = (ip_hdr_t*) net_msg_prepend(net_msg, sizeof(ip_hdr_t));
    net_msg->ip_hdr = (void*) ip_hdr;
    net_msg->nic = &nic;
    nic.ip_addr_assigned = 1;
    nic.ip_addr = 0x1402000a;
    net_msg->eth_hdr = (void*) net_msg_prepend(net_msg, 14);
    ip_hdr->checksum = 0;
    ip_hdr->flags = ntohs(1480 / 8 + 0x2000);
    ip_hdr->id = 101;
    ip_hdr->ip_dest = 0x1402000a;
    ip_hdr->ip_src = 0x1502000a;
    ip_hdr->length = ntohs(1480 + sizeof(ip_hdr_t));
    ip_hdr->proto = IP_PROTO_ICMP;
    ip_hdr->ttl = 64;
    ip_hdr->version = 0x45;
    ip_hdr->checksum = htons(validate_ip_checksum(sizeof(ip_hdr_t), (u16*) ip_hdr));
    for (i = 0; i < 1480; i++)
        data[i] = i + 1480;
    /*
     * and call icmp_rx_msg
     */
    __net_loglevel = 0;
    icmp_rx_msg_called = 0;
    ip_rx_msg(net_msg);
    __net_loglevel = 0;
    ASSERT(0 == icmp_rx_msg_called);
    /*
     * Now create network message for first fragment - 1480 IP data bytes
     */
    net_msg = net_msg_new(1480);
    ASSERT(net_msg);
    net_msg->nic = &nic;
    data = net_msg_append(net_msg, 1480);
    ASSERT(data);
    ip_hdr = (ip_hdr_t*) net_msg_prepend(net_msg, sizeof(ip_hdr_t));
    net_msg->ip_hdr = (void*) ip_hdr;
    net_msg->eth_hdr = (void*) net_msg_prepend(net_msg, 14);
    net_msg->nic = &nic;
    ip_hdr->checksum = 0;
    ip_hdr->flags = ntohs(0x2000);
    ip_hdr->id = 101;
    ip_hdr->ip_dest = 0x1402000a;
    ip_hdr->ip_src = 0x1502000a;
    ip_hdr->length = ntohs(1480 + sizeof(ip_hdr_t));
    ip_hdr->proto = IP_PROTO_ICMP;
    ip_hdr->ttl = 64;
    ip_hdr->version = 0x45;
    ip_hdr->checksum = htons(validate_ip_checksum(sizeof(ip_hdr_t), (u16*) ip_hdr));
    for (i = 0; i < 1480; i++)
        data[i] = i;
    /*
     * and call ip_rx_msg
     */
    icmp_rx_msg_called = 0;
    __net_loglevel = 0;
    ip_rx_msg(net_msg);
    __net_loglevel = 0;
    /*
     * This should NOT have passed the message to icmp_rx_msg
     */
    ASSERT(0 == icmp_rx_msg_called);
    /*
     * Now assemble and send third packet, containing another 10 data bytes
     */
    net_msg = net_msg_new(10);
    ASSERT(net_msg);
    net_msg->nic = &nic;
    data = net_msg_append(net_msg, 10);
    ASSERT(data);
    ip_hdr = (ip_hdr_t*) net_msg_prepend(net_msg, sizeof(ip_hdr_t));
    net_msg->ip_hdr = (void*) ip_hdr;
    net_msg->nic = &nic;
    nic.ip_addr_assigned = 1;
    nic.ip_addr = 0x1402000a;
    net_msg->eth_hdr = (void*) net_msg_prepend(net_msg, 14);
    ASSERT(net_msg->eth_hdr);
    ip_hdr->checksum = 0;
    ip_hdr->flags = ntohs(2*1480 / 8);
    ip_hdr->id = 101;
    ip_hdr->ip_dest = 0x1402000a;
    ip_hdr->ip_src = 0x1502000a;
    ip_hdr->length = ntohs(10 + sizeof(ip_hdr_t));
    ip_hdr->proto = IP_PROTO_ICMP;
    ip_hdr->ttl = 64;
    ip_hdr->version = 0x45;
    ip_hdr->checksum = htons(validate_ip_checksum(sizeof(ip_hdr_t), (u16*) ip_hdr));
    for (i = 0; i < 10; i++)
        data[i] = i + 2*1480;
    /*
     * and call icmp_rx_msg
     */
    __net_loglevel = 0;
    ip_rx_msg(net_msg);
    __net_loglevel = 0;
    ASSERT(1 == icmp_rx_msg_called);
    /*
     * Now verify fields of message. Let us start with the IP header
     */
    ip_hdr = (ip_hdr_t*) icmp_msg->ip_hdr;
    ASSERT(ip_hdr);
    ASSERT(ip_hdr->ip_src == 0x1502000a);
    ASSERT(ip_hdr->ip_dest == 0x1402000a);
    ASSERT(ip_hdr->flags == 0);
    ASSERT(ip_hdr->id == 101);
    ASSERT(ntohs(ip_hdr->length) == sizeof(ip_hdr_t) + 2*1480 + 10);
    ASSERT(ip_hdr->proto == IP_PROTO_ICMP);
    ASSERT(ip_hdr->ttl == 64);
    ASSERT(ip_hdr->version == 0x45);
    /*
     * Now check fields in network message. We expect that all fields which are set by the
     * IP layer are also set for the reassembled message
     */
    ASSERT(icmp_msg->ip_hdr);
    ASSERT(icmp_msg->icmp_hdr);
    ASSERT(icmp_msg->ip_length == 2*1480 + 10);
    ASSERT(ip_hdr->ip_src == icmp_msg->ip_src);
    ASSERT(ip_hdr->ip_dest == icmp_msg->ip_dest);
    ASSERT(icmp_msg->ip_proto == IP_PROTO_ICMP);
    /*
     * Finally verify data
     */
    data = (unsigned char*) ip_hdr + sizeof(ip_hdr_t);
    for (i = 0; i < 2*1480 + 10; i++)
        ASSERT(data[i] == (i % 256));
    /*
     * and make sure that number of created network messages equals number of destroyed messages
     * plus number of calls to icmp_rx_msg
     */
    unsigned int created;
    unsigned int destroyed;
    net_get_counters(&created, &destroyed);
    ASSERT(created == destroyed + icmp_rx_msg_called);
    return 0;
}

/*
 * Testcase 20: receive an ICMP message consisting of two fragments  - timeout occurs after first fragment
 */
int testcase20() {
    int i;
    unsigned char* data;
    ip_hdr_t* ip_hdr;
    nic_t nic;
    /*
     * Create network message for first fragment - 1480 IP data bytes
     */
    net_init();
    net_msg_t* net_msg = net_msg_new(1480);
    ASSERT(net_msg);
    data = net_msg_append(net_msg, 1480);
    ASSERT(data);
    ip_hdr = (ip_hdr_t*) net_msg_prepend(net_msg, sizeof(ip_hdr_t));
    net_msg->ip_hdr = (void*) ip_hdr;
    net_msg->eth_hdr = (void*) net_msg_prepend(net_msg, 14);
    net_msg->nic = &nic;
    nic.ip_addr_assigned = 1;
    nic.ip_addr = 0x1402000a;
    ip_hdr->checksum = 0;
    ip_hdr->flags = ntohs(0x2000);
    ip_hdr->id = 101;
    ip_hdr->ip_dest = 0x1402000a;
    ip_hdr->ip_src = 0x1502000a;
    ip_hdr->length = ntohs(1480 + sizeof(ip_hdr_t));
    ip_hdr->proto = IP_PROTO_ICMP;
    ip_hdr->ttl = 64;
    ip_hdr->version = 0x45;
    ip_hdr->checksum = htons(validate_ip_checksum(sizeof(ip_hdr_t), (u16*) ip_hdr));
    for (i = 0; i < 1480; i++)
        data[i] = i;
    /*
     * and call ip_rx_msg
     */
    icmp_rx_msg_called = 0;
    __net_loglevel = 0;
    ip_rx_msg(net_msg);
    __net_loglevel = 0;
    /*
     * This should NOT have passed the message to icmp_rx_msg
     */
    ASSERT(0 == icmp_rx_msg_called);
    /*
     * Now simulate 15 ticks
     */
    for (i = 0; i < 15; i++)
        ip_do_tick();
    /*
     * Now assemble and send second packet - data byte 1481 - 1490
     */
    net_msg = net_msg_new(10);
    ASSERT(net_msg);
    net_msg->nic = &nic;
    data = net_msg_append(net_msg, 10);
    ASSERT(data);
    ip_hdr = (ip_hdr_t*) net_msg_prepend(net_msg, sizeof(ip_hdr_t));
    net_msg->ip_hdr = (void*) ip_hdr;
    net_msg->nic = &nic;
    net_msg->eth_hdr = (void*) net_msg_prepend(net_msg, 14);
    ip_hdr->checksum = 0;
    ip_hdr->flags = ntohs(0xb9);
    ip_hdr->id = 101;
    ip_hdr->ip_dest = 0x1402000a;
    ip_hdr->ip_src = 0x1502000a;
    ip_hdr->length = ntohs(10 + sizeof(ip_hdr_t));
    ip_hdr->proto = IP_PROTO_ICMP;
    ip_hdr->ttl = 64;
    ip_hdr->version = 0x45;
    ip_hdr->checksum = htons(validate_ip_checksum(sizeof(ip_hdr_t), (u16*) ip_hdr));
    for (i = 0; i < 10; i++)
        data[i] = i + 1480;
    /*
     * and call icmp_rx_msg. Due to the timeout, no message should be reassembled
     */
    __net_loglevel = 0;
    ip_rx_msg(net_msg);
    __net_loglevel = 0;
    ASSERT(0 == icmp_rx_msg_called);
    /*
     * and make sure that number of created network messages equals number of destroyed messages
     * plus number of calls to icmp_rx_msg
     */
    unsigned int created;
    unsigned int destroyed;
    net_get_counters(&created, &destroyed);
    ASSERT(created == destroyed + icmp_rx_msg_called);
    return 0;
}

/*
 * Testcase 21: receive an ICMP message consisting of three fragments  - timeout after second fragment
 */
int testcase21() {
    int i;
    unsigned char* data;
    ip_hdr_t* ip_hdr;
    nic_t nic;
    ip_init();
    net_init();
    /*
     * Create network message for first fragment - 1480 IP data bytes
     */
    net_msg_t* net_msg = net_msg_new(1480);
    ASSERT(net_msg);
    data = net_msg_append(net_msg, 1480);
    ASSERT(data);
    ip_hdr = (ip_hdr_t*) net_msg_prepend(net_msg, sizeof(ip_hdr_t));
    net_msg->ip_hdr = (void*) ip_hdr;
    net_msg->eth_hdr = (void*) net_msg_prepend(net_msg, 14);
    net_msg->nic = &nic;
    nic.ip_addr_assigned = 1;
    nic.ip_addr = 0x1402000a;
    ip_hdr->checksum = 0;
    ip_hdr->flags = ntohs(0x2000);
    ip_hdr->id = 101;
    ip_hdr->ip_dest = 0x1402000a;
    ip_hdr->ip_src = 0x1502000a;
    ip_hdr->length = ntohs(1480 + sizeof(ip_hdr_t));
    ip_hdr->proto = IP_PROTO_ICMP;
    ip_hdr->ttl = 64;
    ip_hdr->version = 0x45;
    ip_hdr->checksum = htons(validate_ip_checksum(sizeof(ip_hdr_t), (u16*) ip_hdr));
    for (i = 0; i < 1480; i++)
        data[i] = i;
    /*
     * and call ip_rx_msg
     */
    icmp_rx_msg_called = 0;
    __net_loglevel = 0;
    ip_rx_msg(net_msg);
    __net_loglevel = 0;
    /*
     * This should NOT have passed the message to icmp_rx_msg
     */
    ASSERT(0 == icmp_rx_msg_called);
    /*
     * Simulate 14 ticks
     */
    for (i = 0; i < 14; i++)
        ip_do_tick();
    /*
     * Now assemble and send second packet, containing another 1480 data bytes
     */
    net_msg = net_msg_new(1480);
    ASSERT(net_msg);
    net_msg->nic = &nic;
    data = net_msg_append(net_msg, 1480);
    ASSERT(data);
    ip_hdr = (ip_hdr_t*) net_msg_prepend(net_msg, sizeof(ip_hdr_t));
    net_msg->ip_hdr = (void*) ip_hdr;
    net_msg->eth_hdr = (void*) net_msg_prepend(net_msg, 14);
    ip_hdr->checksum = 0;
    ip_hdr->flags = ntohs(1480 / 8 + 0x2000);
    ip_hdr->id = 101;
    ip_hdr->ip_dest = 0x1402000a;
    ip_hdr->ip_src = 0x1502000a;
    ip_hdr->length = ntohs(1480 + sizeof(ip_hdr_t));
    ip_hdr->proto = IP_PROTO_ICMP;
    ip_hdr->ttl = 64;
    ip_hdr->version = 0x45;
    ip_hdr->checksum = htons(validate_ip_checksum(sizeof(ip_hdr_t), (u16*) ip_hdr));
    for (i = 0; i < 1480; i++)
        data[i] = i + 1480;
    /*
     * and call icmp_rx_msg
     */
    __net_loglevel = 0;
    ip_rx_msg(net_msg);
    __net_loglevel = 0;
    ASSERT(0 == icmp_rx_msg_called);
    /*
     * Do one more tick - reach timeout now
     */
    ip_do_tick();
    /*
     * Now assemble and send third packet, containing another 10 data bytes
     */
    net_msg = net_msg_new(10);
    ASSERT(net_msg);
    data = net_msg_append(net_msg, 10);
    ASSERT(data);
    ip_hdr = (ip_hdr_t*) net_msg_prepend(net_msg, sizeof(ip_hdr_t));
    net_msg->ip_hdr = (void*) ip_hdr;
    net_msg->nic = &nic;
    net_msg->eth_hdr = (void*) net_msg_prepend(net_msg, 14);
    ASSERT(net_msg->eth_hdr);
    ip_hdr->checksum = 0;
    ip_hdr->flags = ntohs(2*1480 / 8);
    ip_hdr->id = 101;
    ip_hdr->ip_dest = 0x1402000a;
    ip_hdr->ip_src = 0x1502000a;
    ip_hdr->length = ntohs(10 + sizeof(ip_hdr_t));
    ip_hdr->proto = IP_PROTO_ICMP;
    ip_hdr->ttl = 64;
    ip_hdr->version = 0x45;
    ip_hdr->checksum = htons(validate_ip_checksum(sizeof(ip_hdr_t), (u16*) ip_hdr));
    for (i = 0; i < 10; i++)
        data[i] = i + 2*1480;
    /*
     * and call icmp_rx_msg
     */
    __net_loglevel = 0;
    ip_rx_msg(net_msg);
    __net_loglevel = 0;
    ASSERT(0 == icmp_rx_msg_called);
    /*
     * and make sure that number of created network messages equals number of destroyed messages
     * plus number of calls to icmp_rx_msg
     */
    unsigned int created;
    unsigned int destroyed;
    net_get_counters(&created, &destroyed);
    ASSERT(created == destroyed + icmp_rx_msg_called);
    return 0;
}

/*
 * Testcase 22: receive an ICMP message consisting of two fragments  - less than 15 seconds pass between first and second
 * fragment
 */
int testcase22() {
    int i;
    unsigned char* data;
    ip_hdr_t* ip_hdr;
    nic_t nic;
    ip_init();
    /*
     * Create network message for first fragment - 1480 IP data bytes
     */
    net_msg_t* net_msg = net_msg_new(1480);
    ASSERT(net_msg);
    data = net_msg_append(net_msg, 1480);
    ASSERT(data);
    ip_hdr = (ip_hdr_t*) net_msg_prepend(net_msg, sizeof(ip_hdr_t));
    net_msg->ip_hdr = (void*) ip_hdr;
    net_msg->eth_hdr = (void*) net_msg_prepend(net_msg, 14);
    net_msg->nic = &nic;
    nic.ip_addr_assigned = 1;
    nic.ip_addr = 0x1402000a;
    ip_hdr->checksum = 0;
    ip_hdr->flags = ntohs(0x2000);
    ip_hdr->id = 101;
    ip_hdr->ip_dest = 0x1402000a;
    ip_hdr->ip_src = 0x1502000a;
    ip_hdr->length = ntohs(1480 + sizeof(ip_hdr_t));
    ip_hdr->proto = IP_PROTO_ICMP;
    ip_hdr->ttl = 64;
    ip_hdr->version = 0x45;
    ip_hdr->checksum = htons(validate_ip_checksum(sizeof(ip_hdr_t), (u16*) ip_hdr));
    for (i = 0; i < 1480; i++)
        data[i] = i;
    /*
     * and call ip_rx_msg
     */
    icmp_rx_msg_called = 0;
    __net_loglevel = 0;
    ip_rx_msg(net_msg);
    __net_loglevel = 0;
    /*
     * This should NOT have passed the message to icmp_rx_msg
     */
    ASSERT(0 == icmp_rx_msg_called);
    /*
     * Simulate less than 15 seconds
     */
    __net_loglevel = 0;
    for (i = 0; i < REASSEMBLY_TIMEOUT - 1; i++)
        ip_do_tick();
    __net_loglevel = 0;
    /*
     * Now assemble and send second packet - data byte 1481 - 1490
     */
    net_msg = net_msg_new(10);
    ASSERT(net_msg);
    net_msg->nic = &nic;
    data = net_msg_append(net_msg, 10);
    ASSERT(data);
    ip_hdr = (ip_hdr_t*) net_msg_prepend(net_msg, sizeof(ip_hdr_t));
    net_msg->ip_hdr = (void*) ip_hdr;
    net_msg->nic = &nic;
    net_msg->eth_hdr = (void*) net_msg_prepend(net_msg, 14);
    ip_hdr->checksum = 0;
    ip_hdr->flags = ntohs(0xb9);
    ip_hdr->id = 101;
    ip_hdr->ip_dest = 0x1402000a;
    ip_hdr->ip_src = 0x1502000a;
    ip_hdr->length = ntohs(10 + sizeof(ip_hdr_t));
    ip_hdr->proto = IP_PROTO_ICMP;
    ip_hdr->ttl = 64;
    ip_hdr->version = 0x45;
    ip_hdr->checksum = htons(validate_ip_checksum(sizeof(ip_hdr_t), (u16*) ip_hdr));
    for (i = 0; i < 10; i++)
        data[i] = i + 1480;
    /*
     * and call icmp_rx_msg
     */
    __net_loglevel = 0;
    ip_rx_msg(net_msg);
    __net_loglevel = 0;
    ASSERT(1 == icmp_rx_msg_called);
    /*
     * Now verify fields of message. Let us start with the IP header
     */
    ip_hdr = (ip_hdr_t*) icmp_msg->ip_hdr;
    ASSERT(ip_hdr);
    ASSERT(ip_hdr->ip_src == 0x1502000a);
    ASSERT(ip_hdr->ip_dest == 0x1402000a);
    ASSERT(ip_hdr->flags == 0);
    ASSERT(ip_hdr->id == 101);
    ASSERT(ntohs(ip_hdr->length) == sizeof(ip_hdr_t) + 1490);
    ASSERT(ip_hdr->proto == IP_PROTO_ICMP);
    ASSERT(ip_hdr->ttl == 64);
    ASSERT(ip_hdr->version == 0x45);
    /*
     * Now check fields in network message. We expect that all fields which are set by the
     * IP layer are also set for the reassembled message
     */
    ASSERT(icmp_msg->ip_hdr);
    ASSERT(icmp_msg->icmp_hdr);
    ASSERT(icmp_msg->ip_length == 1490);
    ASSERT(ip_hdr->ip_src == icmp_msg->ip_src);
    ASSERT(ip_hdr->ip_dest == icmp_msg->ip_dest);
    ASSERT(icmp_msg->ip_proto == IP_PROTO_ICMP);
    /*
     * Finally verify data
     */
    data = (unsigned char*) ip_hdr + sizeof(ip_hdr_t);
    for (i = 0; i < 1490; i++)
        ASSERT(data[i] == (i % 256));
    return 0;
}

/*
 * Testcase 23: receive an ICMP message consisting of multiple fragments, reaching but not breaking the limit for IP message
 * length of 64 kB. As the maximum size of an IP message is 65535 bytes (limited by the length field) and the header consumes
 * 20 bytes, we can have at most 65515 data bytes. This corresponds to 45 messages at 1480 bytes each plus one message with
 * 395 data bytes
 */
int testcase23() {
    int i;
    unsigned char* data;
    ip_hdr_t* ip_hdr;
    nic_t nic;
    int msg;
    net_init();
    /*
     * Number of messages which we send
     */
    int msg_count = 45;
    /*
     * Data bytes of last msg
     */
    int last_msg = 395;
    /*
     * Create network message for first fragment - 1480 IP data bytes
     */
    net_msg_t* net_msg = net_msg_new(1480);
    ASSERT(net_msg);
    data = net_msg_append(net_msg, 1480);
    ASSERT(data);
    ip_hdr = (ip_hdr_t*) net_msg_prepend(net_msg, sizeof(ip_hdr_t));
    net_msg->ip_hdr = (void*) ip_hdr;
    net_msg->eth_hdr = (void*) net_msg_prepend(net_msg, 14);
    net_msg->nic = &nic;
    nic.ip_addr_assigned = 1;
    nic.ip_addr = 0x1402000a;
    ip_hdr->checksum = 0;
    ip_hdr->flags = ntohs(0x2000);
    ip_hdr->id = 101;
    ip_hdr->ip_dest = 0x1402000a;
    ip_hdr->ip_src = 0x1502000a;
    ip_hdr->length = ntohs(1480 + sizeof(ip_hdr_t));
    ip_hdr->proto = IP_PROTO_ICMP;
    ip_hdr->ttl = 64;
    ip_hdr->version = 0x45;
    ip_hdr->checksum = htons(validate_ip_checksum(sizeof(ip_hdr_t), (u16*) ip_hdr));
    for (i = 0; i < 1480; i++)
        data[i] = i;
    /*
     * and call ip_rx_msg
     */
    icmp_rx_msg_called = 0;
    __net_loglevel = 0;
    ip_rx_msg(net_msg);
    __net_loglevel = 0;
    /*
     * This should NOT have passed the message to icmp_rx_msg
     */
    ASSERT(0 == icmp_rx_msg_called);
    /*
     * Now assemble and send msg_count - 2 additional packets, each having
     * 1480 bytes. Msg #msg will start at offset msg*1480
     */
    for (msg = 1; msg < msg_count - 1; msg++) {
        net_msg = net_msg_new(1480);
        ASSERT(net_msg);
        net_msg->nic = &nic;
        data = net_msg_append(net_msg, 1480);
        ASSERT(data);
        ip_hdr = (ip_hdr_t*) net_msg_prepend(net_msg, sizeof(ip_hdr_t));
        net_msg->ip_hdr = (void*) ip_hdr;
        net_msg->nic = &nic;
        net_msg->eth_hdr = (void*) net_msg_prepend(net_msg, 14);
        ip_hdr->checksum = 0;
        ip_hdr->flags = ntohs(msg*1480 / 8 + 0x2000);
        ip_hdr->id = 101;
        ip_hdr->ip_dest = 0x1402000a;
        ip_hdr->ip_src = 0x1502000a;
        ip_hdr->length = ntohs(1480 + sizeof(ip_hdr_t));
        ip_hdr->proto = IP_PROTO_ICMP;
        ip_hdr->ttl = 64;
        ip_hdr->version = 0x45;
        ip_hdr->checksum = htons(validate_ip_checksum(sizeof(ip_hdr_t), (u16*) ip_hdr));
        for (i = 0; i < 1480; i++)
            data[i] = i + msg*1480;
        /*
         * and call icmp_rx_msg
         */
        __net_loglevel = 0;
        ip_rx_msg(net_msg);
        __net_loglevel = 0;
        ASSERT(0 == icmp_rx_msg_called);
    }
    /*
     * Now assemble and send last packet, containing another last_msg data bytes
     */
    net_msg = net_msg_new(last_msg);
    ASSERT(net_msg);
    net_msg->nic = &nic;
    data = net_msg_append(net_msg, last_msg);
    ASSERT(data);
    ip_hdr = (ip_hdr_t*) net_msg_prepend(net_msg, sizeof(ip_hdr_t));
    net_msg->ip_hdr = (void*) ip_hdr;
    net_msg->nic = &nic;
    net_msg->eth_hdr = (void*) net_msg_prepend(net_msg, 14);
    ASSERT(net_msg->eth_hdr);
    ip_hdr->checksum = 0;
    ip_hdr->flags = ntohs(msg*1480 / 8);
    ip_hdr->id = 101;
    ip_hdr->ip_dest = 0x1402000a;
    ip_hdr->ip_src = 0x1502000a;
    ip_hdr->length = ntohs(last_msg + sizeof(ip_hdr_t));
    ip_hdr->proto = IP_PROTO_ICMP;
    ip_hdr->ttl = 64;
    ip_hdr->version = 0x45;
    ip_hdr->checksum = htons(validate_ip_checksum(sizeof(ip_hdr_t), (u16*) ip_hdr));
    for (i = 0; i < last_msg; i++)
        data[i] = i + msg*1480;
    /*
     * and call icmp_rx_msg
     */
    __net_loglevel = 0;
    ip_rx_msg(net_msg);
    __net_loglevel = 0;
    ASSERT(1 == icmp_rx_msg_called);
    /*
     * Now verify fields of message. Let us start with the IP header
     */
    ip_hdr = (ip_hdr_t*) icmp_msg->ip_hdr;
    ASSERT(ip_hdr);
    ASSERT(ip_hdr->ip_src == 0x1502000a);
    ASSERT(ip_hdr->ip_dest == 0x1402000a);
    ASSERT(ip_hdr->flags == 0);
    ASSERT(ip_hdr->id == 101);
    ASSERT(ntohs(ip_hdr->length) == sizeof(ip_hdr_t) + msg*1480 + last_msg);
    ASSERT(ip_hdr->proto == IP_PROTO_ICMP);
    ASSERT(ip_hdr->ttl == 64);
    ASSERT(ip_hdr->version == 0x45);
    /*
     * Now check fields in network message. We expect that all fields which are set by the
     * IP layer are also set for the reassembled message
     */
    ASSERT(icmp_msg->ip_hdr);
    ASSERT(icmp_msg->icmp_hdr);
    ASSERT(icmp_msg->ip_length == msg*1480 + last_msg);
    ASSERT(ip_hdr->ip_src == icmp_msg->ip_src);
    ASSERT(ip_hdr->ip_dest == icmp_msg->ip_dest);
    ASSERT(icmp_msg->ip_proto == IP_PROTO_ICMP);
    /*
     * Finally verify data
     */
    data = (unsigned char*) ip_hdr + sizeof(ip_hdr_t);
    for (i = 0; i < msg*1480 + last_msg; i++)
        ASSERT(data[i] == (i % 256));
    /*
     * and make sure that number of created network messages equals number of destroyed messages
     * plus number of calls to icmp_rx_msg
     */
    unsigned int created;
    unsigned int destroyed;
    net_get_counters(&created, &destroyed);
    ASSERT(created == destroyed + icmp_rx_msg_called);
    return 0;
}

/*
 * Testcase 24: receive an ICMP message consisting of multiple fragments,  breaking the limit for IP message
 * length of 64 kB. As the maximum size of an IP message is 65535 bytes (limited by the length field) and the header consumes
 * 20 bytes, we can have at most 65515 data bytes. This corresponds to 45 messages at 1480 bytes each plus one message with
 * 395 + 1 data bytes
 */
int testcase24() {
    int i;
    unsigned char* data;
    ip_hdr_t* ip_hdr;
    nic_t nic;
    int msg;
    /*
     * Number of messages which we send
     */
    int msg_count = 45;
    /*
     * Data bytes of last msg
     */
    int last_msg = 395 + 1;
    /*
     * Create network message for first fragment - 1480 IP data bytes
     */
    net_msg_t* net_msg = net_msg_new(1480);
    ASSERT(net_msg);
    data = net_msg_append(net_msg, 1480);
    ASSERT(data);
    ip_hdr = (ip_hdr_t*) net_msg_prepend(net_msg, sizeof(ip_hdr_t));
    net_msg->ip_hdr = (void*) ip_hdr;
    net_msg->eth_hdr = (void*) net_msg_prepend(net_msg, 14);
    net_msg->nic = &nic;
    nic.ip_addr_assigned = 1;
    nic.ip_addr = 0x1402000a;
    ip_hdr->checksum = 0;
    ip_hdr->flags = ntohs(0x2000);
    ip_hdr->id = 101;
    ip_hdr->ip_dest = 0x1402000a;
    ip_hdr->ip_src = 0x1502000a;
    ip_hdr->length = ntohs(1480 + sizeof(ip_hdr_t));
    ip_hdr->proto = IP_PROTO_ICMP;
    ip_hdr->ttl = 64;
    ip_hdr->version = 0x45;
    ip_hdr->checksum = htons(validate_ip_checksum(sizeof(ip_hdr_t), (u16*) ip_hdr));
    for (i = 0; i < 1480; i++)
        data[i] = i;
    /*
     * and call ip_rx_msg
     */
    icmp_rx_msg_called = 0;
    __net_loglevel = 0;
    ip_rx_msg(net_msg);
    __net_loglevel = 0;
    /*
     * This should NOT have passed the message to icmp_rx_msg
     */
    ASSERT(0 == icmp_rx_msg_called);
    /*
     * Now assemble and send msg_count - 2 additional packets, each having
     * 1480 bytes. Msg #msg will start at offset msg*1480
     */
    for (msg = 1; msg < msg_count - 1; msg++) {
        net_msg = net_msg_new(1480);
        ASSERT(net_msg);
        net_msg->nic = &nic;
        data = net_msg_append(net_msg, 1480);
        ASSERT(data);
        ip_hdr = (ip_hdr_t*) net_msg_prepend(net_msg, sizeof(ip_hdr_t));
        net_msg->ip_hdr = (void*) ip_hdr;
        net_msg->nic = &nic;
        net_msg->eth_hdr = (void*) net_msg_prepend(net_msg, 14);
        ip_hdr->checksum = 0;
        ip_hdr->flags = ntohs(msg*1480 / 8 + 0x2000);
        ip_hdr->id = 101;
        ip_hdr->ip_dest = 0x1402000a;
        ip_hdr->ip_src = 0x1502000a;
        ip_hdr->length = ntohs(1480 + sizeof(ip_hdr_t));
        ip_hdr->proto = IP_PROTO_ICMP;
        ip_hdr->ttl = 64;
        ip_hdr->version = 0x45;
        ip_hdr->checksum = htons(validate_ip_checksum(sizeof(ip_hdr_t), (u16*) ip_hdr));
        for (i = 0; i < 1480; i++)
            data[i] = i + msg*1480;
        /*
         * and call icmp_rx_msg
         */
        __net_loglevel = 0;
        ip_rx_msg(net_msg);
        __net_loglevel = 0;
        ASSERT(0 == icmp_rx_msg_called);
    }
    /*
     * Now assemble and send last packet, containing another last_msg data bytes
     */
    net_msg = net_msg_new(last_msg);
    ASSERT(net_msg);
    net_msg->nic = &nic;
    data = net_msg_append(net_msg, last_msg);
    ASSERT(data);
    ip_hdr = (ip_hdr_t*) net_msg_prepend(net_msg, sizeof(ip_hdr_t));
    net_msg->ip_hdr = (void*) ip_hdr;
    net_msg->nic = &nic;
    net_msg->eth_hdr = (void*) net_msg_prepend(net_msg, 14);
    ASSERT(net_msg->eth_hdr);
    ip_hdr->checksum = 0;
    ip_hdr->flags = ntohs(msg*1480 / 8);
    ip_hdr->id = 101;
    ip_hdr->ip_dest = 0x1402000a;
    ip_hdr->ip_src = 0x1502000a;
    ip_hdr->length = ntohs(last_msg + sizeof(ip_hdr_t));
    ip_hdr->proto = IP_PROTO_ICMP;
    ip_hdr->ttl = 64;
    ip_hdr->version = 0x45;
    ip_hdr->checksum = htons(validate_ip_checksum(sizeof(ip_hdr_t), (u16*) ip_hdr));
    for (i = 0; i < last_msg; i++)
        data[i] = i + msg*1480;
    /*
     * and call icmp_rx_msg
     */
    __net_loglevel = 0;
    do_putchar = 0;
    ip_rx_msg(net_msg);
    __net_loglevel = 0;
    do_putchar = 1;
    ASSERT(0 == icmp_rx_msg_called);
    return 0;
}

/*
 * Testcase 25: receive an ICMP message consisting of two fragments  - no overlap, first fragment received first. Second
 * fragment exceeds maximum total size
 */
int testcase25() {
    int i;
    unsigned char* data;
    ip_hdr_t* ip_hdr;
    nic_t nic;
    /*
     * Create network message for first fragment - 44*1480 IP data bytes
     */
    net_msg_t* net_msg = net_msg_new(44*1480);
    ASSERT(net_msg);
    data = net_msg_append(net_msg, 44*1480);
    ASSERT(data);
    ip_hdr = (ip_hdr_t*) net_msg_prepend(net_msg, sizeof(ip_hdr_t));
    net_msg->ip_hdr = (void*) ip_hdr;
    net_msg->eth_hdr = (void*) net_msg_prepend(net_msg, 14);
    net_msg->nic = &nic;
    nic.ip_addr_assigned = 1;
    nic.ip_addr = 0x1402000a;
    ip_hdr->checksum = 0;
    ip_hdr->flags = ntohs(0x2000);
    ip_hdr->id = 101;
    ip_hdr->ip_dest = 0x1402000a;
    ip_hdr->ip_src = 0x1502000a;
    ip_hdr->length = ntohs(44*1480 + sizeof(ip_hdr_t));
    ip_hdr->proto = IP_PROTO_ICMP;
    ip_hdr->ttl = 64;
    ip_hdr->version = 0x45;
    ip_hdr->checksum = htons(validate_ip_checksum(sizeof(ip_hdr_t), (u16*) ip_hdr));
    for (i = 0; i < 44*1480; i++)
        data[i] = i;
    /*
     * and call ip_rx_msg
     */
    icmp_rx_msg_called = 0;
    __net_loglevel = 0;
    ip_rx_msg(net_msg);
    __net_loglevel = 0;
    /*
     * This should NOT have passed the message to icmp_rx_msg, but placed 65120 bytes in the reassembly buffer
     */
    ASSERT(0 == icmp_rx_msg_called);
    /*
     * Now assemble and send second packet containing 1000 additional bytes. This should exceed the size of the
     * buffer
     */
    net_msg = net_msg_new(1000);
    ASSERT(net_msg);
    net_msg->nic = &nic;
    data = net_msg_append(net_msg, 1000);
    ASSERT(data);
    ip_hdr = (ip_hdr_t*) net_msg_prepend(net_msg, sizeof(ip_hdr_t));
    net_msg->ip_hdr = (void*) ip_hdr;
    net_msg->nic = &nic;
    net_msg->eth_hdr = (void*) net_msg_prepend(net_msg, 14);
    ip_hdr->checksum = 0;
    ip_hdr->flags = ntohs(44*1480 / 8);
    ip_hdr->id = 101;
    ip_hdr->ip_dest = 0x1402000a;
    ip_hdr->ip_src = 0x1502000a;
    ip_hdr->length = ntohs(1000 + sizeof(ip_hdr_t));
    ip_hdr->proto = IP_PROTO_ICMP;
    ip_hdr->ttl = 64;
    ip_hdr->version = 0x45;
    ip_hdr->checksum = htons(validate_ip_checksum(sizeof(ip_hdr_t), (u16*) ip_hdr));
    for (i = 0; i < 1000; i++)
        data[i] = i + 44*1480;
    /*
     * and call icmp_rx_msg
     */
    __net_loglevel = 0;
    do_putchar = 0;
    ip_rx_msg(net_msg);
    __net_loglevel = 0;
    do_putchar = 1;
    ASSERT(0 == icmp_rx_msg_called);
    return 0;
}

/*
 * Testcase 26: Strong host model - receive an ICMP message which is not directed towards the incoming interface
 * and verify that message is dropped
 */
int testcase26() {
    int i;
    unsigned char* data;
    ip_hdr_t* ip_hdr;
    nic_t nic;
    /*
     * Create network message
     */
    net_msg_t* net_msg = net_msg_new(256);
    ASSERT(net_msg);
    net_msg->nic = &nic;
    nic.ip_addr_assigned = 1;
    nic.ip_addr = 0x1602000a;
    data = net_msg_append(net_msg, 100);
    ASSERT(data);
    ip_hdr = (ip_hdr_t*) net_msg_prepend(net_msg, sizeof(ip_hdr_t));
    net_msg->ip_hdr = (void*) ip_hdr;
    ip_hdr->checksum = 0;
    ip_hdr->flags = ntohs(0x4000);
    ip_hdr->id = 0;
    ip_hdr->ip_dest = 0x1402000a;
    ip_hdr->ip_src = 0x1502000a;
    ip_hdr->length = ntohs(100 + sizeof(ip_hdr_t));
    ip_hdr->proto = IP_PROTO_ICMP;
    ip_hdr->ttl = 64;
    ip_hdr->version = 0x45;
    ip_hdr->checksum = htons(validate_ip_checksum(sizeof(ip_hdr_t), (u16*) ip_hdr));
    for (i = 0; i < 100; i++)
        data[i] = i;
    /*
     * and call ip_rx_msg
     */
    icmp_rx_msg_called = 0;
    __net_loglevel = 0;
    ip_rx_msg(net_msg);
    __net_loglevel = 0;
    /*
     * This should NOT have passed the message to icmp_rx_msg
     */
    ASSERT(0 == icmp_rx_msg_called);
    return 0;
}

/*
 * Testcase 27: receive an ICMP message with TTL 0
 */
int testcase27() {
    int i;
    unsigned char* data;
    ip_hdr_t* ip_hdr;
    nic_t nic;
    /*
     * Create network message
     */
    net_init();
    net_msg_t* net_msg = net_msg_new(256);
    ASSERT(net_msg);
    net_msg->nic = &nic;
    nic.ip_addr_assigned = 1;
    nic.ip_addr = 0x1402000a;
    data = net_msg_append(net_msg, 100);
    ASSERT(data);
    ip_hdr = (ip_hdr_t*) net_msg_prepend(net_msg, sizeof(ip_hdr_t));
    net_msg->ip_hdr = (void*) ip_hdr;
    ip_hdr->checksum = 0;
    ip_hdr->flags = ntohs(0x4000);
    ip_hdr->id = 0;
    ip_hdr->ip_dest = 0x1402000a;
    ip_hdr->ip_src = 0x1502000a;
    ip_hdr->length = ntohs(100 + sizeof(ip_hdr_t));
    ip_hdr->proto = IP_PROTO_ICMP;
    ip_hdr->ttl = 0;
    ip_hdr->version = 0x45;
    ip_hdr->checksum = htons(validate_ip_checksum(sizeof(ip_hdr_t), (u16*) ip_hdr));
    for (i = 0; i < 100; i++)
        data[i] = i;
    /*
     * and call ip_rx_msg
     */
    icmp_rx_msg_called = 0;
    __net_loglevel = 0;
    ip_rx_msg(net_msg);
    __net_loglevel = 0;
    /*
     * This should have NOT passed the message to icmp_rx_msg
     */
    ASSERT(0 == icmp_rx_msg_called);
    /*
     * and make sure that number of created network messages equals number of destroyed messages
     * plus number of calls to icmp_rx_msg
     */
    unsigned int created;
    unsigned int destroyed;
    net_get_counters(&created, &destroyed);
    ASSERT(created == destroyed + icmp_rx_msg_called);
    return 0;
}

/*
 * Testcase 28: try to add a route to an unknown interface
 */
int testcase28() {
    struct rtentry rt_entry;
    struct sockaddr_in* in;
    ip_init();
    /*
     * Set up rt_entry
     */
    strncpy(rt_entry.dev, "eth1", 4);
    in = (struct sockaddr_in*) &rt_entry.rt_dst;
    in->sin_addr.s_addr = inet_addr("0.0.0.0");
    in->sin_family = AF_INET;
    rt_entry.rt_flags = RT_FLAGS_UP;
    in = (struct sockaddr_in*) &rt_entry.rt_gateway;
    in->sin_addr.s_addr = inet_addr("0.0.0.0");
    in->sin_family = AF_INET;
    in = (struct sockaddr_in*) &rt_entry.rt_genmask;
    in->sin_addr.s_addr = inet_addr("0.0.0.0");
    in->sin_family = AF_INET;
    ASSERT(-110 == ip_add_route(&rt_entry));
    return 0;
}

/*
 * Testcase 29: add a route to a local network and verify that the route is selected
 * Routing table:
 * DEST         MASK              GW          Flags     Device
 * 10.0.2.0     255.255.255.0     0.0.0.0     U         eth0 (10.0.2.21)
 */
int testcase29() {
    struct rtentry rt_entry;
    struct sockaddr_in* in;
    unsigned int next_hop;
    nic_t nic;
    ip_init();
    our_nic = &nic;
    strncpy(our_nic->name, "eth0", 4);
    our_nic->hw_type = HW_TYPE_ETH;
    our_nic->ip_addr_assigned = 1;
    our_nic->ip_addr = inet_addr("10.0.2.21");
    /*
     * Set up rt_entry
     */
    strncpy(rt_entry.dev, "eth0", 4);
    in = (struct sockaddr_in*) &rt_entry.rt_dst;
    in->sin_addr.s_addr = inet_addr("10.0.2.0");
    in->sin_family = AF_INET;
    rt_entry.rt_flags = RT_FLAGS_UP;
    in = (struct sockaddr_in*) &rt_entry.rt_gateway;
    in->sin_addr.s_addr = inet_addr("0.0.0.0");
    in->sin_family = AF_INET;
    in = (struct sockaddr_in*) &rt_entry.rt_genmask;
    in->sin_addr.s_addr = inet_addr("255.255.255.0");
    in->sin_family = AF_INET;
    /*
     * and add entry
     */
    ASSERT(0 == ip_add_route(&rt_entry));
    /*
     * Now make sure that a route to 10.0.2.15 is found
     * and that - as this is a local route - the next hop is the destination address
     */
    ASSERT(inet_addr("10.0.2.21") == ip_get_src_addr(inet_addr("10.0.2.15")));
    ASSERT(our_nic == ip_get_route(0, inet_addr("10.0.2.15"), &next_hop));
    ASSERT(inet_addr("10.0.2.15") == next_hop);
    return 0;
}

/*
 * Testcase 30: add a route to a local network and a default route verify that the route is selected
 * for local address and takes precedence over the less specific default route
 * Routing table:
 * DEST         MASK              GW          Flags     Device
 * 0.0.0.0      0.0.0.0           10.0.2.1    UG        eth0 (10.0.2.21)
 * 10.0.2.0     255.255.255.0     0.0.0.0     U         eth0 (10.0.2.21)
 */
int testcase30() {
    struct rtentry rt_entry;
    struct sockaddr_in* in;
    unsigned int next_hop;
    nic_t nic;
    ip_init();
    our_nic = &nic;
    strncpy(our_nic->name, "eth0", 4);
    our_nic->hw_type = HW_TYPE_ETH;
    our_nic->ip_addr_assigned = 1;
    our_nic->ip_addr = inet_addr("10.0.2.21");
    /*
     * Set up rt_entry
     */
    strncpy(rt_entry.dev, "eth0", 4);
    in = (struct sockaddr_in*) &rt_entry.rt_dst;
    in->sin_addr.s_addr = inet_addr("10.0.2.0");
    in->sin_family = AF_INET;
    rt_entry.rt_flags = RT_FLAGS_UP;
    in = (struct sockaddr_in*) &rt_entry.rt_gateway;
    in->sin_addr.s_addr = inet_addr("0.0.0.0");
    in->sin_family = AF_INET;
    in = (struct sockaddr_in*) &rt_entry.rt_genmask;
    in->sin_addr.s_addr = inet_addr("255.255.255.0");
    in->sin_family = AF_INET;
    /*
     * and add entry
     */
    ASSERT(0 == ip_add_route(&rt_entry));
    /*
     * Similarly add default route to gateway 10.0.2.1
     */
    in = (struct sockaddr_in*) &rt_entry.rt_dst;
    in->sin_addr.s_addr = inet_addr("0.0.0.0");
    rt_entry.rt_flags = RT_FLAGS_UP | RT_FLAGS_GW;
    in = (struct sockaddr_in*) &rt_entry.rt_gateway;
    in->sin_addr.s_addr = inet_addr("10.0.2.1");
    in = (struct sockaddr_in*) &rt_entry.rt_genmask;
    in->sin_addr.s_addr = inet_addr("0.0.0.0");
    ASSERT(0 == ip_add_route(&rt_entry));
    /*
     * Now make sure that a route to 10.0.2.15 is found
     * and that - as this is a local route - the next hop is the destination address
     */
    ASSERT(inet_addr("10.0.2.21") == ip_get_src_addr(inet_addr("10.0.2.15")));
    ASSERT(our_nic == ip_get_route(0, inet_addr("10.0.2.15"), &next_hop));
    ASSERT(inet_addr("10.0.2.15") == next_hop);
    return 0;
}

/*
 * Testcase 31: add a route to a local network and a default route verify that the route is selected
 * for local address and takes precedence over the less specific default route
 * Request routing with a deviating source address and verify that the route is rejected
 * Routing table:
 * DEST         MASK              GW          Flags     Device
 * 0.0.0.0      0.0.0.0           10.0.2.1    UG        eth0 (10.0.2.21)
 * 10.0.2.0     255.255.255.0     0.0.0.0     U         eth0 (10.0.2.21)
 */
int testcase31() {
    struct rtentry rt_entry;
    struct sockaddr_in* in;
    unsigned int next_hop;
    nic_t nic;
    ip_init();
    our_nic = &nic;
    strncpy(our_nic->name, "eth0", 4);
    our_nic->hw_type = HW_TYPE_ETH;
    our_nic->ip_addr_assigned = 1;
    our_nic->ip_addr = inet_addr("10.0.2.21");
    /*
     * Set up rt_entry
     */
    strncpy(rt_entry.dev, "eth0", 4);
    in = (struct sockaddr_in*) &rt_entry.rt_dst;
    in->sin_addr.s_addr = inet_addr("10.0.2.0");
    in->sin_family = AF_INET;
    rt_entry.rt_flags = RT_FLAGS_UP;
    in = (struct sockaddr_in*) &rt_entry.rt_gateway;
    in->sin_addr.s_addr = inet_addr("0.0.0.0");
    in->sin_family = AF_INET;
    in = (struct sockaddr_in*) &rt_entry.rt_genmask;
    in->sin_addr.s_addr = inet_addr("255.255.255.0");
    in->sin_family = AF_INET;
    /*
     * and add entry
     */
    ASSERT(0 == ip_add_route(&rt_entry));
    /*
     * Similarly add default route to gateway 10.0.2.1
     */
    in = (struct sockaddr_in*) &rt_entry.rt_dst;
    in->sin_addr.s_addr = inet_addr("0.0.0.0");
    rt_entry.rt_flags = RT_FLAGS_UP | RT_FLAGS_GW;
    in = (struct sockaddr_in*) &rt_entry.rt_gateway;
    in->sin_addr.s_addr = inet_addr("10.0.2.1");
    in = (struct sockaddr_in*) &rt_entry.rt_genmask;
    in->sin_addr.s_addr = inet_addr("0.0.0.0");
    ASSERT(0 == ip_add_route(&rt_entry));
    /*
     * Now make sure that a route to 10.0.2.15 is found
     * and that - as this is a local route - the next hop is the destination address
     */
    ASSERT(inet_addr("10.0.2.21") == ip_get_src_addr(inet_addr("10.0.2.15")));
    ASSERT(0 == ip_get_route(inet_addr("10.0.2.22"), inet_addr("10.0.2.15"), &next_hop));
    return 0;
}

/*
 * Testcase 32: add a route to a local network and a default route verify that the route is selected
 * for local address and takes precedence over the less specific default route
 * Request routing with a specified, but matching source address
 * Routing table:
 * DEST         MASK              GW          Flags     Device
 * 0.0.0.0      0.0.0.0           10.0.2.1    UG        eth0 (10.0.2.21)
 * 10.0.2.0     255.255.255.0     0.0.0.0     U         eth0 (10.0.2.21)
 */
int testcase32() {
    struct rtentry rt_entry;
    struct sockaddr_in* in;
    unsigned int next_hop;
    nic_t nic;
    ip_init();
    our_nic = &nic;
    strncpy(our_nic->name, "eth0", 4);
    our_nic->hw_type = HW_TYPE_ETH;
    our_nic->ip_addr_assigned = 1;
    our_nic->ip_addr = inet_addr("10.0.2.21");
    /*
     * Set up rt_entry
     */
    strncpy(rt_entry.dev, "eth0", 4);
    in = (struct sockaddr_in*) &rt_entry.rt_dst;
    in->sin_addr.s_addr = inet_addr("10.0.2.0");
    in->sin_family = AF_INET;
    rt_entry.rt_flags = RT_FLAGS_UP;
    in = (struct sockaddr_in*) &rt_entry.rt_gateway;
    in->sin_addr.s_addr = inet_addr("0.0.0.0");
    in->sin_family = AF_INET;
    in = (struct sockaddr_in*) &rt_entry.rt_genmask;
    in->sin_addr.s_addr = inet_addr("255.255.255.0");
    in->sin_family = AF_INET;
    /*
     * and add entry
     */
    ASSERT(0 == ip_add_route(&rt_entry));
    /*
     * Similarly add default route to gateway 10.0.2.1
     */
    in = (struct sockaddr_in*) &rt_entry.rt_dst;
    in->sin_addr.s_addr = inet_addr("0.0.0.0");
    rt_entry.rt_flags = RT_FLAGS_UP | RT_FLAGS_GW;
    in = (struct sockaddr_in*) &rt_entry.rt_gateway;
    in->sin_addr.s_addr = inet_addr("10.0.2.1");
    in = (struct sockaddr_in*) &rt_entry.rt_genmask;
    in->sin_addr.s_addr = inet_addr("0.0.0.0");
    ASSERT(0 == ip_add_route(&rt_entry));
    /*
     * Now make sure that a route to 10.0.2.15 is found
     * and that - as this is a local route - the next hop is the destination address
     */
    ASSERT(inet_addr("10.0.2.21") == ip_get_src_addr(inet_addr("10.0.2.15")));
    ASSERT(our_nic == ip_get_route(inet_addr("10.0.2.21"), inet_addr("10.0.2.15"), &next_hop));
    return 0;
}

/*
 * Testcase 33: simulate two outgoing network interfaces, both having a route
 * to a given destination address. Verify that if an IP source address is specified,
 * the matching route is chosen
 * Routing table:
 * DEST         MASK              GW          Flags     Device
 * 10.0.2.0     255.255.255.0     0.0.0.0     U         eth0 (10.0.2.21)
 * 10.0.2.0     255.255.255.0     0.0.0.0     U         eth1 (10.0.2.22)
 */
int testcase33() {
    struct rtentry rt_entry;
    struct sockaddr_in* in;
    unsigned int next_hop;
    nic_t nic;
    nic_t nic2;
    ip_init();
    our_nic = &nic;
    second_nic = &nic2;
    strncpy(our_nic->name, "eth0", 4);
    our_nic->hw_type = HW_TYPE_ETH;
    our_nic->ip_addr_assigned = 1;
    our_nic->ip_addr = inet_addr("10.0.2.21");
    strncpy(second_nic->name, "eth1", 4);
    second_nic->hw_type = HW_TYPE_ETH;
    second_nic->ip_addr_assigned = 1;
    second_nic->ip_addr = inet_addr("10.0.2.22");
    /*
     * Set up rt_entry
     */
    strncpy(rt_entry.dev, "eth0", 4);
    in = (struct sockaddr_in*) &rt_entry.rt_dst;
    in->sin_addr.s_addr = inet_addr("10.0.2.0");
    in->sin_family = AF_INET;
    rt_entry.rt_flags = RT_FLAGS_UP;
    in = (struct sockaddr_in*) &rt_entry.rt_gateway;
    in->sin_addr.s_addr = inet_addr("0.0.0.0");
    in->sin_family = AF_INET;
    in = (struct sockaddr_in*) &rt_entry.rt_genmask;
    in->sin_addr.s_addr = inet_addr("255.255.255.0");
    in->sin_family = AF_INET;
    /*
     * and add entry
     */
    ASSERT(0 == ip_add_route(&rt_entry));
    /*
     * Same for second route
     */
    strncpy(rt_entry.dev, "eth1", 4);
    in = (struct sockaddr_in*) &rt_entry.rt_dst;
    in->sin_addr.s_addr = inet_addr("10.0.2.0");
    in->sin_family = AF_INET;
    rt_entry.rt_flags = RT_FLAGS_UP;
    in = (struct sockaddr_in*) &rt_entry.rt_gateway;
    in->sin_addr.s_addr = inet_addr("0.0.0.0");
    in->sin_family = AF_INET;
    in = (struct sockaddr_in*) &rt_entry.rt_genmask;
    in->sin_addr.s_addr = inet_addr("255.255.255.0");
    in->sin_family = AF_INET;
    ASSERT(0 == ip_add_route(&rt_entry));
    /*
     * Now make sure that a route to 10.0.2.15 is found
     * and that - as this is a local route - the next hop is the destination address
     */
    ASSERT(second_nic == ip_get_route(inet_addr("10.0.2.22"), inet_addr("10.0.2.15"), &next_hop));
    ASSERT(next_hop == inet_addr("10.0.2.15"));
    return 0;
}

/*
 * Testcase 34: add a route to a local network and a default route. Verify that the default route is
 * selected for addresses not on the local network
 * Routing table:
 * DEST         MASK              GW          Flags     Device
 * 0.0.0.0      0.0.0.0           10.0.2.1    UG        eth0 (10.0.2.21)
 * 10.0.2.0     255.255.255.0     0.0.0.0     U         eth0 (10.0.2.21)
 */
int testcase34() {
    struct rtentry rt_entry;
    struct sockaddr_in* in;
    unsigned int next_hop;
    nic_t nic;
    ip_init();
    our_nic = &nic;
    strncpy(our_nic->name, "eth0", 4);
    our_nic->hw_type = HW_TYPE_ETH;
    our_nic->ip_addr_assigned = 1;
    our_nic->ip_addr = inet_addr("10.0.2.21");
    /*
     * Set up rt_entry
     */
    strncpy(rt_entry.dev, "eth0", 4);
    in = (struct sockaddr_in*) &rt_entry.rt_dst;
    in->sin_addr.s_addr = inet_addr("10.0.2.0");
    in->sin_family = AF_INET;
    rt_entry.rt_flags = RT_FLAGS_UP;
    in = (struct sockaddr_in*) &rt_entry.rt_gateway;
    in->sin_addr.s_addr = inet_addr("0.0.0.0");
    in->sin_family = AF_INET;
    in = (struct sockaddr_in*) &rt_entry.rt_genmask;
    in->sin_addr.s_addr = inet_addr("255.255.255.0");
    in->sin_family = AF_INET;
    /*
     * and add entry
     */
    ASSERT(0 == ip_add_route(&rt_entry));
    /*
     * Similarly add default route to gateway 10.0.2.1
     */
    in = (struct sockaddr_in*) &rt_entry.rt_dst;
    in->sin_addr.s_addr = inet_addr("0.0.0.0");
    rt_entry.rt_flags = RT_FLAGS_UP | RT_FLAGS_GW;
    in = (struct sockaddr_in*) &rt_entry.rt_gateway;
    in->sin_addr.s_addr = inet_addr("10.0.2.1");
    in = (struct sockaddr_in*) &rt_entry.rt_genmask;
    in->sin_addr.s_addr = inet_addr("0.0.0.0");
    ASSERT(0 == ip_add_route(&rt_entry));
    /*
     * Now make sure that a route to 128.0.0.1 is found
     * and that - as this is a indirect route - the next hop is the gateway address
     */
    ASSERT(our_nic == ip_get_route(0, inet_addr("128.0.0.1"), &next_hop));
    ASSERT(inet_addr("10.0.2.1") == next_hop);
    ASSERT(inet_addr("10.0.2.21") == ip_get_src_addr(inet_addr("128.0.0.1")));
    return 0;
}

/*
 * Testcase 35: simulate two outgoing network interfaces on different networks
 * Verify that a packet is correctly routed to the respective interface
 * Routing table:
 * DEST         MASK              GW          Flags     Device
 * 10.0.2.0     255.255.255.0     0.0.0.0     U         eth0 (10.0.2.21)
 * 11.0.2.0     255.255.255.0     0.0.0.0     U         eth1 (11.0.2.21)
 */
int testcase35() {
    struct rtentry rt_entry;
    struct sockaddr_in* in;
    unsigned int next_hop;
    nic_t nic;
    nic_t nic2;
    ip_init();
    our_nic = &nic;
    second_nic = &nic2;
    strncpy(our_nic->name, "eth0", 4);
    our_nic->hw_type = HW_TYPE_ETH;
    our_nic->ip_addr_assigned = 1;
    our_nic->ip_addr = inet_addr("10.0.2.21");
    strncpy(second_nic->name, "eth1", 4);
    second_nic->hw_type = HW_TYPE_ETH;
    second_nic->ip_addr_assigned = 1;
    second_nic->ip_addr = inet_addr("11.0.2.21");
    /*
     * Set up rt_entry
     */
    strncpy(rt_entry.dev, "eth0", 4);
    in = (struct sockaddr_in*) &rt_entry.rt_dst;
    in->sin_addr.s_addr = inet_addr("10.0.2.0");
    in->sin_family = AF_INET;
    rt_entry.rt_flags = RT_FLAGS_UP;
    in = (struct sockaddr_in*) &rt_entry.rt_gateway;
    in->sin_addr.s_addr = inet_addr("0.0.0.0");
    in->sin_family = AF_INET;
    in = (struct sockaddr_in*) &rt_entry.rt_genmask;
    in->sin_addr.s_addr = inet_addr("255.255.255.0");
    in->sin_family = AF_INET;
    /*
     * and add entry
     */
    ASSERT(0 == ip_add_route(&rt_entry));
    /*
     * Same for second route
     */
    strncpy(rt_entry.dev, "eth1", 4);
    in = (struct sockaddr_in*) &rt_entry.rt_dst;
    in->sin_addr.s_addr = inet_addr("11.0.2.0");
    in->sin_family = AF_INET;
    rt_entry.rt_flags = RT_FLAGS_UP;
    in = (struct sockaddr_in*) &rt_entry.rt_gateway;
    in->sin_addr.s_addr = inet_addr("0.0.0.0");
    in->sin_family = AF_INET;
    in = (struct sockaddr_in*) &rt_entry.rt_genmask;
    in->sin_addr.s_addr = inet_addr("255.255.255.0");
    in->sin_family = AF_INET;
    ASSERT(0 == ip_add_route(&rt_entry));
    /*
     * Now make sure that a route to 10.0.2.15 is found
     */
    ASSERT(our_nic == ip_get_route(0, inet_addr("10.0.2.15"), &next_hop));
    ASSERT(next_hop == inet_addr("10.0.2.15"));
    /*
     * 11.0.2.15 should be reachable via the second interface
     */
    ASSERT(second_nic == ip_get_route(0, inet_addr("11.0.2.15"), &next_hop));
    ASSERT(next_hop == inet_addr("11.0.2.15"));
    return 0;
}

/*
 * Testcase 36: add a route to a local network and a default route verify that both routes are selected. Then
 * call ip_purge_nic and verify that both routes have been deleted
 * Routing table:
 * DEST         MASK              GW          Flags     Device
 * 0.0.0.0      0.0.0.0           10.0.2.1    UG        eth0 (10.0.2.21)
 * 10.0.2.0     255.255.255.0     0.0.0.0     U         eth0 (10.0.2.21)
 */
int testcase36() {
    struct rtentry rt_entry;
    struct sockaddr_in* in;
    unsigned int next_hop;
    nic_t nic;
    ip_init();
    our_nic = &nic;
    strncpy(our_nic->name, "eth0", 4);
    our_nic->hw_type = HW_TYPE_ETH;
    our_nic->ip_addr_assigned = 1;
    our_nic->ip_addr = inet_addr("10.0.2.21");
    /*
     * Set up rt_entry
     */
    strncpy(rt_entry.dev, "eth0", 4);
    in = (struct sockaddr_in*) &rt_entry.rt_dst;
    in->sin_addr.s_addr = inet_addr("10.0.2.0");
    in->sin_family = AF_INET;
    rt_entry.rt_flags = RT_FLAGS_UP;
    in = (struct sockaddr_in*) &rt_entry.rt_gateway;
    in->sin_addr.s_addr = inet_addr("0.0.0.0");
    in->sin_family = AF_INET;
    in = (struct sockaddr_in*) &rt_entry.rt_genmask;
    in->sin_addr.s_addr = inet_addr("255.255.255.0");
    in->sin_family = AF_INET;
    /*
     * and add entry
     */
    ASSERT(0 == ip_add_route(&rt_entry));
    /*
     * Similarly add default route to gateway 10.0.2.1
     */
    in = (struct sockaddr_in*) &rt_entry.rt_dst;
    in->sin_addr.s_addr = inet_addr("0.0.0.0");
    rt_entry.rt_flags = RT_FLAGS_UP | RT_FLAGS_GW;
    in = (struct sockaddr_in*) &rt_entry.rt_gateway;
    in->sin_addr.s_addr = inet_addr("10.0.2.1");
    in = (struct sockaddr_in*) &rt_entry.rt_genmask;
    in->sin_addr.s_addr = inet_addr("0.0.0.0");
    ASSERT(0 == ip_add_route(&rt_entry));
    /*
     * Now make sure that a route to 10.0.2.15 is found
     * and that - as this is a local route - the next hop is the destination address
     */
    ASSERT(inet_addr("10.0.2.21") == ip_get_src_addr(inet_addr("10.0.2.15")));
    ASSERT(our_nic == ip_get_route(0, inet_addr("10.0.2.15"), &next_hop));
    ASSERT(inet_addr("10.0.2.15") == next_hop);
    /*
     * Routes to other networks should go via the gateway
     */
    ASSERT(our_nic == ip_get_route(0, inet_addr("11.0.2.15"), &next_hop));
    ASSERT(inet_addr("10.0.2.1") == next_hop);
    /*
     * Purge all routing table entries for this NIC
     */
    ip_purge_nic(our_nic);
    /*
     * Now both routes should no longer be valid
     */
    ASSERT(0 == ip_get_route(0, inet_addr("10.0.2.15"), &next_hop));
    ASSERT(0 == ip_get_route(0, inet_addr("11.0.2.15"), &next_hop));
    return 0;
}

/*
 * Testcase 37: simulate two outgoing network interfaces on different networks
 * Verify that a packet is correctly routed to the respective interface. Then purge
 * one network interface and verify that the second is still used
 * Routing table:
 * DEST         MASK              GW          Flags     Device
 * 10.0.2.0     255.255.255.0     0.0.0.0     U         eth0 (10.0.2.21)
 * 11.0.2.0     255.255.255.0     0.0.0.0     U         eth1 (11.0.2.21)
 */
int testcase37() {
    struct rtentry rt_entry;
    struct sockaddr_in* in;
    unsigned int next_hop;
    nic_t nic;
    nic_t nic2;
    ip_init();
    our_nic = &nic;
    second_nic = &nic2;
    strncpy(our_nic->name, "eth0", 4);
    our_nic->hw_type = HW_TYPE_ETH;
    our_nic->ip_addr_assigned = 1;
    our_nic->ip_addr = inet_addr("10.0.2.21");
    strncpy(second_nic->name, "eth1", 4);
    second_nic->hw_type = HW_TYPE_ETH;
    second_nic->ip_addr_assigned = 1;
    second_nic->ip_addr = inet_addr("11.0.2.21");
    /*
     * Set up rt_entry
     */
    strncpy(rt_entry.dev, "eth0", 4);
    in = (struct sockaddr_in*) &rt_entry.rt_dst;
    in->sin_addr.s_addr = inet_addr("10.0.2.0");
    in->sin_family = AF_INET;
    rt_entry.rt_flags = RT_FLAGS_UP;
    in = (struct sockaddr_in*) &rt_entry.rt_gateway;
    in->sin_addr.s_addr = inet_addr("0.0.0.0");
    in->sin_family = AF_INET;
    in = (struct sockaddr_in*) &rt_entry.rt_genmask;
    in->sin_addr.s_addr = inet_addr("255.255.255.0");
    in->sin_family = AF_INET;
    /*
     * and add entry
     */
    ASSERT(0 == ip_add_route(&rt_entry));
    /*
     * Same for second route
     */
    strncpy(rt_entry.dev, "eth1", 4);
    in = (struct sockaddr_in*) &rt_entry.rt_dst;
    in->sin_addr.s_addr = inet_addr("11.0.2.0");
    in->sin_family = AF_INET;
    rt_entry.rt_flags = RT_FLAGS_UP;
    in = (struct sockaddr_in*) &rt_entry.rt_gateway;
    in->sin_addr.s_addr = inet_addr("0.0.0.0");
    in->sin_family = AF_INET;
    in = (struct sockaddr_in*) &rt_entry.rt_genmask;
    in->sin_addr.s_addr = inet_addr("255.255.255.0");
    in->sin_family = AF_INET;
    ASSERT(0 == ip_add_route(&rt_entry));
    /*
     * Now make sure that a route to 10.0.2.15 is found
     */
    ASSERT(our_nic == ip_get_route(0, inet_addr("10.0.2.15"), &next_hop));
    ASSERT(next_hop == inet_addr("10.0.2.15"));
    /*
     * 11.0.2.15 should be reachable via the second interface
     */
    ASSERT(second_nic == ip_get_route(0, inet_addr("11.0.2.15"), &next_hop));
    ASSERT(next_hop == inet_addr("11.0.2.15"));
    /*
     * Now purge first interface
     */
    ip_purge_nic(our_nic);
    /*
     * Second network should still be reachable, but routes to first network should have
     * been deleted
     */
    ASSERT(second_nic == ip_get_route(0, inet_addr("11.0.2.15"), &next_hop));
    ASSERT(next_hop == inet_addr("11.0.2.15"));
    ASSERT(0 == ip_get_route(0, inet_addr("10.0.2.15"), &next_hop));
    return 0;
}

/*
 * Testcase 38: add a route to a local network and a default route. Verify that ip_get_rtconf returns both entries
 * Routing table:
 * DEST         MASK              GW          Flags     Device
 * 0.0.0.0      0.0.0.0           10.0.2.1    UG        eth0 (10.0.2.21)
 * 10.0.2.0     255.255.255.0     0.0.0.0     U         eth0 (10.0.2.21)
 */
int testcase38() {
    struct rtentry rt_entry;
    struct rtconf rt_conf;
    struct rtentry routing_table[16];
    struct sockaddr_in* in;
    unsigned int next_hop;
    int i;
    int first_entry_found;
    int second_entry_found;
    nic_t nic;
    ip_init();
    our_nic = &nic;
    strncpy(our_nic->name, "eth0", 4);
    our_nic->hw_type = HW_TYPE_ETH;
    our_nic->ip_addr_assigned = 1;
    our_nic->ip_addr = inet_addr("10.0.2.21");
    /*
     * Set up rt_entry
     */
    strncpy(rt_entry.dev, "eth0", 4);
    in = (struct sockaddr_in*) &rt_entry.rt_dst;
    in->sin_addr.s_addr = inet_addr("10.0.2.0");
    in->sin_family = AF_INET;
    rt_entry.rt_flags = RT_FLAGS_UP;
    in = (struct sockaddr_in*) &rt_entry.rt_gateway;
    in->sin_addr.s_addr = inet_addr("0.0.0.0");
    in->sin_family = AF_INET;
    in = (struct sockaddr_in*) &rt_entry.rt_genmask;
    in->sin_addr.s_addr = inet_addr("255.255.255.0");
    in->sin_family = AF_INET;
    /*
     * and add entry
     */
    ASSERT(0 == ip_add_route(&rt_entry));
    /*
     * Similarly add default route to gateway 10.0.2.1
     */
    in = (struct sockaddr_in*) &rt_entry.rt_dst;
    in->sin_addr.s_addr = inet_addr("0.0.0.0");
    rt_entry.rt_flags = RT_FLAGS_UP | RT_FLAGS_GW;
    in = (struct sockaddr_in*) &rt_entry.rt_gateway;
    in->sin_addr.s_addr = inet_addr("10.0.2.1");
    in = (struct sockaddr_in*) &rt_entry.rt_genmask;
    in->sin_addr.s_addr = inet_addr("0.0.0.0");
    ASSERT(0 == ip_add_route(&rt_entry));
    /*
     * Now call ip_get_rtconf
     */
    rt_conf.rtc_len = 16 * sizeof(struct rtentry);
    rt_conf.rtc_rtcu.rtcu_req = routing_table;
    __net_loglevel = 0;
    ASSERT(0 == ip_get_rtconf(&rt_conf));
    __net_loglevel = 0;
    ASSERT(2 * sizeof(struct rtentry) == rt_conf.rtc_len);
    /*
     * and search for both entries
     */
    first_entry_found = 0;
    second_entry_found = 0;
    for (i = 0; i< 2; i++) {
        ASSERT(0 == strncmp("eth0", routing_table[i].dev, 4));
        ASSERT(routing_table[i].rt_flags & RT_FLAGS_UP);
        if (routing_table[i].rt_flags & RT_FLAGS_GW) {
            /*
             * Looks like entry for gateway
             */
            ASSERT(0 == ((struct sockaddr_in*) &routing_table[i].rt_dst)->sin_addr.s_addr);
            ASSERT(0 == ((struct sockaddr_in*) &routing_table[i].rt_genmask)->sin_addr.s_addr);
            ASSERT(inet_addr("10.0.2.1") == ((struct sockaddr_in*) &routing_table[i].rt_gateway)->sin_addr.s_addr);
            ASSERT(AF_INET == ((struct sockaddr_in*) &routing_table[i].rt_dst)->sin_family);
            first_entry_found = 1;
        }
        else {
            ASSERT(inet_addr("10.0.2.0") == ((struct sockaddr_in*) &routing_table[i].rt_dst)->sin_addr.s_addr);
            ASSERT(inet_addr("255.255.255.0") == ((struct sockaddr_in*) &routing_table[i].rt_genmask)->sin_addr.s_addr);
            ASSERT(inet_addr("0.0.0.0") == ((struct sockaddr_in*) &routing_table[i].rt_gateway)->sin_addr.s_addr);
            ASSERT(AF_INET == ((struct sockaddr_in*) &routing_table[i].rt_dst)->sin_family);
            second_entry_found = 1;
        }
    }
    return 0;
}

/*
 * Testcase 39: simulate two outgoing network interfaces on different networks
 * Verify that a packet is correctly routed to the respective interface. Then delete
 * one route and verify that the second is still used
 * Routing table:
 * DEST         MASK              GW          Flags     Device
 * 10.0.2.0     255.255.255.0     0.0.0.0     U         eth0 (10.0.2.21)
 * 11.0.2.0     255.255.255.0     0.0.0.0     U         eth1 (11.0.2.21)
 */
int testcase39() {
    struct rtentry rt_entry;
    struct sockaddr_in* in;
    unsigned int next_hop;
    nic_t nic;
    nic_t nic2;
    ip_init();
    our_nic = &nic;
    second_nic = &nic2;
    strncpy(our_nic->name, "eth0", 4);
    our_nic->hw_type = HW_TYPE_ETH;
    our_nic->ip_addr_assigned = 1;
    our_nic->ip_addr = inet_addr("10.0.2.21");
    strncpy(second_nic->name, "eth1", 4);
    second_nic->hw_type = HW_TYPE_ETH;
    second_nic->ip_addr_assigned = 1;
    second_nic->ip_addr = inet_addr("11.0.2.21");
    /*
     * Set up rt_entry
     */
    strncpy(rt_entry.dev, "eth0", 4);
    in = (struct sockaddr_in*) &rt_entry.rt_dst;
    in->sin_addr.s_addr = inet_addr("10.0.2.0");
    in->sin_family = AF_INET;
    rt_entry.rt_flags = RT_FLAGS_UP;
    in = (struct sockaddr_in*) &rt_entry.rt_gateway;
    in->sin_addr.s_addr = inet_addr("0.0.0.0");
    in->sin_family = AF_INET;
    in = (struct sockaddr_in*) &rt_entry.rt_genmask;
    in->sin_addr.s_addr = inet_addr("255.255.255.0");
    in->sin_family = AF_INET;
    /*
     * and add entry
     */
    ASSERT(0 == ip_add_route(&rt_entry));
    /*
     * Same for second route
     */
    strncpy(rt_entry.dev, "eth1", 4);
    in = (struct sockaddr_in*) &rt_entry.rt_dst;
    in->sin_addr.s_addr = inet_addr("11.0.2.0");
    in->sin_family = AF_INET;
    rt_entry.rt_flags = RT_FLAGS_UP;
    in = (struct sockaddr_in*) &rt_entry.rt_gateway;
    in->sin_addr.s_addr = inet_addr("0.0.0.0");
    in->sin_family = AF_INET;
    in = (struct sockaddr_in*) &rt_entry.rt_genmask;
    in->sin_addr.s_addr = inet_addr("255.255.255.0");
    in->sin_family = AF_INET;
    ASSERT(0 == ip_add_route(&rt_entry));
    /*
     * Now make sure that a route to 10.0.2.15 is found
     */
    ASSERT(our_nic == ip_get_route(0, inet_addr("10.0.2.15"), &next_hop));
    ASSERT(next_hop == inet_addr("10.0.2.15"));
    /*
     * 11.0.2.15 should be reachable via the second interface
     */
    ASSERT(second_nic == ip_get_route(0, inet_addr("11.0.2.15"), &next_hop));
    ASSERT(next_hop == inet_addr("11.0.2.15"));
    /*
     * Now delete last route again
     */
    do_putchar = 1;
    __net_loglevel = 0;
    ASSERT(0 == ip_del_route(&rt_entry));
    __net_loglevel = 0;
    /*
     * First network should still be reachable, but route to second network should have
     * been deleted
     */
    ASSERT(0 == ip_get_route(0, inet_addr("11.0.2.15"), &next_hop));
    ASSERT(next_hop == inet_addr("11.0.2.15"));
    ASSERT(our_nic == ip_get_route(0, inet_addr("10.0.2.15"), &next_hop));
    return 0;
}

/*
 * Create a raw IP / ICMP socket and use it to send data
 */
int testcase40() {
    ip_hdr_t* ip_hdr;
    net_msg_t* net_msg;
    socket_t* socket = net_socket_create(AF_INET, SOCK_RAW, IP_PROTO_ICMP);
    /*
     * Proto of socket should be IP_PROTO_ICMP now
     */
    ASSERT(IP_PROTO_ICMP == socket->proto.ip.ip_proto);
    /*
     * and reference count should be one
     */
    ASSERT(1 == socket->proto.ip.ref_count);
    struct sockaddr_in in;
    nic_t nic;
    unsigned char buffer[256];
    unsigned char* data;
    int i;
    for (i = 0; i < 256; i++)
        buffer[i] = 256 - i;
    ASSERT(socket);
    ip_init();
    /*
     * Connect socket to remote host 10.0.2.21
     */
    in.sin_family = AF_INET;
    in.sin_addr.s_addr = inet_addr("10.0.2.21");
    ASSERT(0 == socket->ops->connect(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * As this should have added the socket to list of sockets used for multiplexing,
     * reference count should increase
     */
    ASSERT(2 == socket->proto.ip.ref_count);
    /*
     * Set up route to destination
     */
    our_nic = &nic;
    nic.ip_addr = 0x1402000a;
    nic.ip_addr_assigned = 1;
    ASSERT(0 == add_route(inet_addr("10.0.2.21"), inet_addr("255.255.0.0"), "eth0"));
    /*
     * and send 256 bytes
     */
    wq_schedule_called = 0;
    __net_loglevel = 0;
    ASSERT(256 == socket->ops->send(socket, buffer, 256, 0));
    __net_loglevel = 0;
    ASSERT(1 == wq_schedule_called);
    /*
     * now check content of network message
     */
    net_msg = tx_net_msg[0];
    ASSERT(net_msg);
    /*
     * Check NIC, ip_dest, ethertype
     */
    ASSERT(net_msg->nic == our_nic);
    ASSERT(net_msg->ip_dest == inet_addr("10.0.2.21"));
    ASSERT(net_msg->ethertype == htons(0x800));
    /*
     * Now check IP header
     */
    ASSERT(net_msg->ip_hdr);
    ip_hdr = (ip_hdr_t*) net_msg->ip_hdr;
    ASSERT(inet_addr("10.0.2.21") == ip_hdr->ip_dest);
    ASSERT(inet_addr("10.0.2.20") == ip_hdr->ip_src);
    ASSERT(ip_hdr->proto == IP_PROTO_ICMP);
    ASSERT(ip_hdr->length == ntohs(sizeof(ip_hdr_t) + 256));
    /*
     * and data
     */
    data = (unsigned char*) ip_hdr + sizeof(ip_hdr_t);
    for (i = 0; i < 256; i++)
        ASSERT(data[i] == buffer[i]);
    return 0;
}

/*
 * Create a raw IP / ICMP socket and use it to receive data - receive one message, then call read
 */
int testcase41() {
    ip_hdr_t* ip_hdr;
    unsigned char* data;
    net_msg_t* net_msg;
    socket_t* socket = net_socket_create(AF_INET, SOCK_RAW, IP_PROTO_ICMP);
    /*
     * Proto of socket should be IP_PROTO_ICMP now
     */
    ASSERT(IP_PROTO_ICMP == socket->proto.ip.ip_proto);
    struct sockaddr_in in;
    nic_t nic;
    unsigned char buffer[256];
    int i;
    ASSERT(socket);
    ip_init();
    /*
     * Bind socket to address 10.0.2.20
     */
    in.sin_family = AF_INET;
    in.sin_addr.s_addr = inet_addr("10.0.2.20");
    ASSERT(0 == socket->ops->bind(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * Now simulate incoming message
     */
    net_msg = net_msg_new(256);
    ASSERT(net_msg);
    net_msg->nic = &nic;
    nic.ip_addr_assigned = 1;
    nic.ip_addr = 0x1402000a;
    data = net_msg_append(net_msg, 100);
    ASSERT(data);
    ip_hdr = (ip_hdr_t*) net_msg_prepend(net_msg, sizeof(ip_hdr_t));
    net_msg->ip_hdr = (void*) ip_hdr;
    ip_hdr->checksum = 0;
    ip_hdr->flags = ntohs(0x4000);
    ip_hdr->id = 0;
    ip_hdr->ip_dest = 0x1402000a;
    ip_hdr->ip_src = 0x1502000a;
    ip_hdr->length = ntohs(100 + sizeof(ip_hdr_t));
    ip_hdr->proto = IP_PROTO_ICMP;
    ip_hdr->ttl = 64;
    ip_hdr->version = 0x45;
    ip_hdr->checksum = htons(validate_ip_checksum(sizeof(ip_hdr_t), (u16*) ip_hdr));
    for (i = 0; i < 100; i++)
        data[i] = i;
    /*
     * and call ip_rx_msg
     */
    cond_broadcast_called = 0;
    icmp_rx_msg_called = 0;
    __net_loglevel = 0;
    ip_rx_msg(net_msg);
    __net_loglevel = 0;
    ASSERT(icmp_rx_msg_called);
    /*
     * We should be able to read the IP header and payload from our socket now
     */
    __net_loglevel = 1;
    ASSERT(120 == socket->ops->recv(socket, buffer, 120, 0));
    __net_loglevel = 0;
    /*
     * and a broadcast should have been issued
     */
    ASSERT(cond_broadcast_called);
    /*
     * Validate data: we should have the IP header followed by 100 bytes of data
     */
    for (i = 0; i < 100; i++) {
        ASSERT(buffer[i + sizeof(ip_hdr_t)] == (i % 256));
    }
    ip_hdr = (ip_hdr_t*) buffer;
    ASSERT(ip_hdr->ip_dest == inet_addr("10.0.2.20"));
    ASSERT(ip_hdr->ip_src == inet_addr("10.0.2.21"));
    return 0;
}

/*
 * Create a raw IP / ICMP socket and use it to receive data - receive two messages, then call read
 */
int testcase42() {
    ip_hdr_t* ip_hdr;
    unsigned char* data;
    net_msg_t* net_msg;
    socket_t* socket = net_socket_create(AF_INET, SOCK_RAW, IP_PROTO_ICMP);
    /*
     * Proto of socket should be IP_PROTO_ICMP now
     */
    ASSERT(IP_PROTO_ICMP == socket->proto.ip.ip_proto);
    struct sockaddr_in in;
    nic_t nic;
    unsigned char buffer[256];
    int i;
    ASSERT(socket);
    ip_init();
    /*
     * Bind socket to address 10.0.2.20
     */
    in.sin_family = AF_INET;
    in.sin_addr.s_addr = inet_addr("10.0.2.20");
    ASSERT(0 == socket->ops->bind(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * Now simulate first incoming message
     */
    net_msg = net_msg_new(256);
    ASSERT(net_msg);
    net_msg->nic = &nic;
    nic.ip_addr_assigned = 1;
    nic.ip_addr = 0x1402000a;
    data = net_msg_append(net_msg, 100);
    ASSERT(data);
    ip_hdr = (ip_hdr_t*) net_msg_prepend(net_msg, sizeof(ip_hdr_t));
    net_msg->ip_hdr = (void*) ip_hdr;
    ip_hdr->checksum = 0;
    ip_hdr->flags = ntohs(0x4000);
    ip_hdr->id = 1;
    ip_hdr->ip_dest = 0x1402000a;
    ip_hdr->ip_src = 0x1502000a;
    ip_hdr->length = ntohs(100 + sizeof(ip_hdr_t));
    ip_hdr->proto = IP_PROTO_ICMP;
    ip_hdr->ttl = 64;
    ip_hdr->version = 0x45;
    ip_hdr->checksum = htons(validate_ip_checksum(sizeof(ip_hdr_t), (u16*) ip_hdr));
    for (i = 0; i < 100; i++)
        data[i] = i;
    /*
     * and call ip_rx_msg
     */
    cond_broadcast_called = 0;
    icmp_rx_msg_called = 0;
    __net_loglevel = 0;
    ip_rx_msg(net_msg);
    __net_loglevel = 0;
    ASSERT(icmp_rx_msg_called);
    /*
     * Now simulate second message
     */
    net_msg = net_msg_new(256);
    ASSERT(net_msg);
    net_msg->nic = &nic;
    nic.ip_addr_assigned = 1;
    nic.ip_addr = 0x1402000a;
    data = net_msg_append(net_msg, 100);
    ASSERT(data);
    ip_hdr = (ip_hdr_t*) net_msg_prepend(net_msg, sizeof(ip_hdr_t));
    net_msg->ip_hdr = (void*) ip_hdr;
    ip_hdr->checksum = 0;
    ip_hdr->flags = ntohs(0x4000);
    ip_hdr->id = 2;
    ip_hdr->ip_dest = 0x1402000a;
    ip_hdr->ip_src = 0x1502000a;
    ip_hdr->length = ntohs(100 + sizeof(ip_hdr_t));
    ip_hdr->proto = IP_PROTO_ICMP;
    ip_hdr->ttl = 64;
    ip_hdr->version = 0x45;
    ip_hdr->checksum = htons(validate_ip_checksum(sizeof(ip_hdr_t), (u16*) ip_hdr));
    for (i = 0; i < 100; i++)
        data[i] = i;
    /*
     * and call ip_rx_msg
     */
     cond_broadcast_called = 0;
     icmp_rx_msg_called = 0;
     __net_loglevel = 0;
     ip_rx_msg(net_msg);
     __net_loglevel = 0;
     ASSERT(icmp_rx_msg_called);
    /*
     * We should be able to read the IP header and payload of the first message from our socket now
     */
    __net_loglevel = 0;
    ASSERT(120 == socket->ops->recv(socket, buffer, 120, 0));
    __net_loglevel = 0;
    /*
     * Validate data: we should have the IP header followed by 100 bytes of data
     */
    for (i = 0; i < 100; i++) {
        ASSERT(buffer[i + sizeof(ip_hdr_t)] == (i % 256));
    }
    ip_hdr = (ip_hdr_t*) buffer;
    ASSERT(ip_hdr->ip_dest == inet_addr("10.0.2.20"));
    ASSERT(ip_hdr->ip_src == inet_addr("10.0.2.21"));
    ASSERT(ip_hdr->id == 1);
    /*
     * Now read second message - should be ID 2
     */
    __net_loglevel = 0;
    ASSERT(120 == socket->ops->recv(socket, buffer, 120, 0));
    __net_loglevel = 0;
    for (i = 0; i < 100; i++) {
        ASSERT(buffer[i + sizeof(ip_hdr_t)] == (i % 256));
    }
    ip_hdr = (ip_hdr_t*) buffer;
    ASSERT(ip_hdr->ip_dest == inet_addr("10.0.2.20"));
    ASSERT(ip_hdr->ip_src == inet_addr("10.0.2.21"));
    ASSERT(ip_hdr->id == 2);
    /*
     * Third read should return -EAGAIN
     */
    ASSERT(-106 == socket->ops->recv(socket, buffer, 120, 0));
    return 0;
}

/*
 * Create a raw IP / ICMP socket and use it to receive data - receive one message, then call recv and read
 * only a part of the message. Verify that remainder is discarded
 */
int testcase43() {
    ip_hdr_t* ip_hdr;
    unsigned char* data;
    net_msg_t* net_msg;
    socket_t* socket = net_socket_create(AF_INET, SOCK_RAW, IP_PROTO_ICMP);
    /*
     * Proto of socket should be IP_PROTO_ICMP now
     */
    ASSERT(IP_PROTO_ICMP == socket->proto.ip.ip_proto);
    struct sockaddr_in in;
    nic_t nic;
    unsigned char buffer[256];
    int i;
    ASSERT(socket);
    ip_init();
    /*
     * Bind socket to address 10.0.2.20
     */
    in.sin_family = AF_INET;
    in.sin_addr.s_addr = inet_addr("10.0.2.20");
    ASSERT(0 == socket->ops->bind(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * Now simulate incoming message
     */
    net_msg = net_msg_new(256);
    ASSERT(net_msg);
    net_msg->nic = &nic;
    nic.ip_addr_assigned = 1;
    nic.ip_addr = 0x1402000a;
    data = net_msg_append(net_msg, 100);
    ASSERT(data);
    ip_hdr = (ip_hdr_t*) net_msg_prepend(net_msg, sizeof(ip_hdr_t));
    net_msg->ip_hdr = (void*) ip_hdr;
    ip_hdr->checksum = 0;
    ip_hdr->flags = ntohs(0x4000);
    ip_hdr->id = 0;
    ip_hdr->ip_dest = 0x1402000a;
    ip_hdr->ip_src = 0x1502000a;
    ip_hdr->length = ntohs(100 + sizeof(ip_hdr_t));
    ip_hdr->proto = IP_PROTO_ICMP;
    ip_hdr->ttl = 64;
    ip_hdr->version = 0x45;
    ip_hdr->checksum = htons(validate_ip_checksum(sizeof(ip_hdr_t), (u16*) ip_hdr));
    for (i = 0; i < 100; i++)
        data[i] = i;
    /*
     * and call ip_rx_msg
     */
    cond_broadcast_called = 0;
    icmp_rx_msg_called = 0;
    __net_loglevel = 0;
    ip_rx_msg(net_msg);
    __net_loglevel = 0;
    ASSERT(icmp_rx_msg_called);
    /*
     * Do partial read
     *
     */
    __net_loglevel = 1;
    ASSERT(30 == socket->ops->recv(socket, buffer, 30, 0));
    __net_loglevel = 0;
    /*
     * and a broadcast should have been issued
     */
    ASSERT(cond_broadcast_called);
    /*
     * Validate data: we should have the IP header followed by 10 bytes of data
     */
    for (i = 0; i < 10; i++) {
        ASSERT(buffer[i + sizeof(ip_hdr_t)] == (i % 256));
    }
    ip_hdr = (ip_hdr_t*) buffer;
    ASSERT(ip_hdr->ip_dest == inet_addr("10.0.2.20"));
    ASSERT(ip_hdr->ip_src == inet_addr("10.0.2.21"));
    /*
     * Next read should return -EAGAIN
     */
    ASSERT(-106 == socket->ops->recv(socket, buffer, 10, 0));
    return 0;
}

/*
 * Create a raw IP / ICMP socket and use it to receive data - receive two messages, then call read
 * and try to read more than one message. Verify that still only one message is returned
 */
int testcase44() {
    ip_hdr_t* ip_hdr;
    unsigned char* data;
    net_msg_t* net_msg;
    socket_t* socket = net_socket_create(AF_INET, SOCK_RAW, IP_PROTO_ICMP);
    /*
     * Proto of socket should be IP_PROTO_ICMP now
     */
    ASSERT(IP_PROTO_ICMP == socket->proto.ip.ip_proto);
    struct sockaddr_in in;
    nic_t nic;
    unsigned char buffer[256];
    int i;
    ASSERT(socket);
    net_init();
    ip_init();
    /*
     * Bind socket to address 10.0.2.20
     */
    in.sin_family = AF_INET;
    in.sin_addr.s_addr = inet_addr("10.0.2.20");
    ASSERT(0 == socket->ops->bind(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * Now simulate first incoming message
     */
    net_msg = net_msg_new(256);
    ASSERT(net_msg);
    net_msg->nic = &nic;
    nic.ip_addr_assigned = 1;
    nic.ip_addr = 0x1402000a;
    data = net_msg_append(net_msg, 100);
    ASSERT(data);
    ip_hdr = (ip_hdr_t*) net_msg_prepend(net_msg, sizeof(ip_hdr_t));
    net_msg->ip_hdr = (void*) ip_hdr;
    ip_hdr->checksum = 0;
    ip_hdr->flags = ntohs(0x4000);
    ip_hdr->id = 1;
    ip_hdr->ip_dest = 0x1402000a;
    ip_hdr->ip_src = 0x1502000a;
    ip_hdr->length = ntohs(100 + sizeof(ip_hdr_t));
    ip_hdr->proto = IP_PROTO_ICMP;
    ip_hdr->ttl = 64;
    ip_hdr->version = 0x45;
    ip_hdr->checksum = htons(validate_ip_checksum(sizeof(ip_hdr_t), (u16*) ip_hdr));
    for (i = 0; i < 100; i++)
        data[i] = i;
    /*
     * and call ip_rx_msg
     */
    cond_broadcast_called = 0;
    icmp_rx_msg_called = 0;
    __net_loglevel = 0;
    ip_rx_msg(net_msg);
    __net_loglevel = 0;
    ASSERT(1 == icmp_rx_msg_called);
    /*
     * Now simulate second message
     */
    net_msg = net_msg_new(256);
    ASSERT(net_msg);
    net_msg->nic = &nic;
    nic.ip_addr_assigned = 1;
    nic.ip_addr = 0x1402000a;
    data = net_msg_append(net_msg, 100);
    ASSERT(data);
    ip_hdr = (ip_hdr_t*) net_msg_prepend(net_msg, sizeof(ip_hdr_t));
    net_msg->ip_hdr = (void*) ip_hdr;
    ip_hdr->checksum = 0;
    ip_hdr->flags = ntohs(0x4000);
    ip_hdr->id = 2;
    ip_hdr->ip_dest = 0x1402000a;
    ip_hdr->ip_src = 0x1502000a;
    ip_hdr->length = ntohs(100 + sizeof(ip_hdr_t));
    ip_hdr->proto = IP_PROTO_ICMP;
    ip_hdr->ttl = 64;
    ip_hdr->version = 0x45;
    ip_hdr->checksum = htons(validate_ip_checksum(sizeof(ip_hdr_t), (u16*) ip_hdr));
    for (i = 0; i < 100; i++)
        data[i] = i;
    /*
     * and call ip_rx_msg
     */
     cond_broadcast_called = 0;
     __net_loglevel = 0;
     ip_rx_msg(net_msg);
     __net_loglevel = 0;
     ASSERT(2 == icmp_rx_msg_called);
    /*
     * We should be able to read the IP header and payload of the first message from our socket now
     * Even if we try to read more than 120 bytes, we should only get 120 bytes back
     */
    __net_loglevel = 0;
    ASSERT(120 == socket->ops->recv(socket, buffer, 200, 0));
    __net_loglevel = 0;
    /*
     * Validate data: we should have the IP header followed by 100 bytes of data
     */
    for (i = 0; i < 100; i++) {
        ASSERT(buffer[i + sizeof(ip_hdr_t)] == (i % 256));
    }
    ip_hdr = (ip_hdr_t*) buffer;
    ASSERT(ip_hdr->ip_dest == inet_addr("10.0.2.20"));
    ASSERT(ip_hdr->ip_src == inet_addr("10.0.2.21"));
    ASSERT(ip_hdr->id == 1);
    /*
     * Now read second message - should be ID 2, and should still be present
     */
    __net_loglevel = 0;
    ASSERT(120 == socket->ops->recv(socket, buffer, 120, 0));
    __net_loglevel = 0;
    for (i = 0; i < 100; i++) {
        ASSERT(buffer[i + sizeof(ip_hdr_t)] == (i % 256));
    }
    ip_hdr = (ip_hdr_t*) buffer;
    ASSERT(ip_hdr->ip_dest == inet_addr("10.0.2.20"));
    ASSERT(ip_hdr->ip_src == inet_addr("10.0.2.21"));
    ASSERT(ip_hdr->id == 2);
    /*
     * Third read should return -EAGAIN
     */
    ASSERT(-106 == socket->ops->recv(socket, buffer, 120, 0));
    /*
     * At this point, all network messages should have been destroyed, up
     * to the calls of icmp_rx_msg which does not destroy the messages it receives
     * (unlike the actual implementation)
     */
    unsigned int created;
    unsigned int destroyed;
    net_get_counters(&created, &destroyed);
    ASSERT(created == destroyed + icmp_rx_msg_called);
    return 0;
}

/*
 * Create a raw IP / ICMP socket and verify that select call returns readiness to read only if there is data
 * in the buffer
 */
int testcase45() {
    ip_hdr_t* ip_hdr;
    unsigned char* data;
    net_msg_t* net_msg;
    socket_t* socket = net_socket_create(AF_INET, SOCK_RAW, IP_PROTO_ICMP);
    /*
     * Proto of socket should be IP_PROTO_ICMP now
     */
    ASSERT(IP_PROTO_ICMP == socket->proto.ip.ip_proto);
    struct sockaddr_in in;
    nic_t nic;
    unsigned char buffer[256];
    int i;
    ASSERT(socket);
    ip_init();
    /*
     * Bind socket to address 10.0.2.20
     */
    in.sin_family = AF_INET;
    in.sin_addr.s_addr = inet_addr("10.0.2.20");
    ASSERT(0 == socket->ops->bind(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * Now simulate first incoming message
     */
    net_msg = net_msg_new(256);
    ASSERT(net_msg);
    net_msg->nic = &nic;
    nic.ip_addr_assigned = 1;
    nic.ip_addr = 0x1402000a;
    data = net_msg_append(net_msg, 100);
    ASSERT(data);
    ip_hdr = (ip_hdr_t*) net_msg_prepend(net_msg, sizeof(ip_hdr_t));
    net_msg->ip_hdr = (void*) ip_hdr;
    ip_hdr->checksum = 0;
    ip_hdr->flags = ntohs(0x4000);
    ip_hdr->id = 1;
    ip_hdr->ip_dest = 0x1402000a;
    ip_hdr->ip_src = 0x1502000a;
    ip_hdr->length = ntohs(100 + sizeof(ip_hdr_t));
    ip_hdr->proto = IP_PROTO_ICMP;
    ip_hdr->ttl = 64;
    ip_hdr->version = 0x45;
    ip_hdr->checksum = htons(validate_ip_checksum(sizeof(ip_hdr_t), (u16*) ip_hdr));
    for (i = 0; i < 100; i++)
        data[i] = i;
    /*
     * Select should return 0
     */
    ASSERT(0 == socket->ops->select(socket, 1, 0));
    /*
     * Now call ip_rx_msg
     */
    cond_broadcast_called = 0;
    icmp_rx_msg_called = 0;
    __net_loglevel = 0;
    ip_rx_msg(net_msg);
    __net_loglevel = 0;
    ASSERT(icmp_rx_msg_called);
    /*
     * Select should return 1 now as there is unread data in the buffer
     */
    ASSERT(1 == socket->ops->select(socket, 1, 0));
    /*
     * Read data
     */
    ASSERT(100 + sizeof(ip_hdr_t) == socket->ops->recv(socket, buffer, 100 + sizeof(ip_hdr_t), 0));
    /*
     * Select should return 0 again
     */
    ASSERT(0 == socket->ops->select(socket, 1, 0));
    return 0;
}

/*
 * Create a raw IP / ICMP socket and verify that select call returns readiness to write
 */
int testcase46() {
    u32 eflags;
    ip_hdr_t* ip_hdr;
    unsigned char* data;
    net_msg_t* net_msg;
    socket_t* socket = net_socket_create(AF_INET, SOCK_RAW, IP_PROTO_ICMP);
    /*
     * Proto of socket should be IP_PROTO_ICMP now
     */
    ASSERT(IP_PROTO_ICMP == socket->proto.ip.ip_proto);
    /*
     * and reference count should be one - socket is not yet connected
     */
    ASSERT(1 == socket->proto.ip.ref_count);
    struct sockaddr_in in;
    nic_t nic;
    unsigned char buffer[256];
    int i;
    ASSERT(socket);
    ip_init();
    /*
     * Bind socket to address 10.0.2.20
     */
    in.sin_family = AF_INET;
    in.sin_addr.s_addr = inet_addr("10.0.2.20");
    ASSERT(0 == socket->ops->bind(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * As socket is now added to list of sockets eligible for receiving packets, reference count
     * should be two
     */
    ASSERT(2 == socket->proto.ip.ref_count);
    /*
     * Select for write should return 0x2
     */
    ASSERT(0x2 == socket->ops->select(socket, 0, 1));
    /*
     * Now close socket - socket reference count should drop to one immediately
     */
    socket->ops->close(socket, &eflags);
    ASSERT(1 == socket->proto.ip.ref_count);
    /*
     * and finally release socket
     */
    socket->ops->release(socket);
    return 0;
}


/*
 * Create a raw IP / ICMP socket and use it to send data - use sendto
 */
int testcase47() {
    ip_hdr_t* ip_hdr;
    net_msg_t* net_msg;
    socket_t* socket = net_socket_create(AF_INET, SOCK_RAW, IP_PROTO_ICMP);
    /*
     * Proto of socket should be IP_PROTO_ICMP now
     */
    ASSERT(IP_PROTO_ICMP == socket->proto.ip.ip_proto);
    /*
     * and reference count should be one
     */
    ASSERT(1 == socket->proto.ip.ref_count);
    struct sockaddr_in in;
    nic_t nic;
    unsigned char buffer[256];
    unsigned char* data;
    int i;
    for (i = 0; i < 256; i++)
        buffer[i] = 256 - i;
    ASSERT(socket);
    ip_init();
    in.sin_family = AF_INET;
    in.sin_addr.s_addr = inet_addr("10.0.2.21");
    /*
     * Set up route to destination
     */
    our_nic = &nic;
    nic.ip_addr = 0x1402000a;
    nic.ip_addr_assigned = 1;
    ASSERT(0 == add_route(inet_addr("10.0.2.21"), inet_addr("255.255.0.0"), "eth0"));
    /*
     * and send 256 bytes
     */
    wq_schedule_called = 0;
    __net_loglevel = 0;
    ASSERT(256 == socket->ops->sendto(socket, buffer, 256, 0, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    __net_loglevel = 0;
    ASSERT(1 == wq_schedule_called);
    /*
     * now check content of network message
     */
    net_msg = tx_net_msg[0];
    ASSERT(net_msg);
    /*
     * Check NIC, ip_dest, ethertype
     */
    ASSERT(net_msg->nic == our_nic);
    ASSERT(net_msg->ip_dest == inet_addr("10.0.2.21"));
    ASSERT(net_msg->ethertype == htons(0x800));
    /*
     * Now check IP header
     */
    ASSERT(net_msg->ip_hdr);
    ip_hdr = (ip_hdr_t*) net_msg->ip_hdr;
    ASSERT(inet_addr("10.0.2.21") == ip_hdr->ip_dest);
    ASSERT(inet_addr("10.0.2.20") == ip_hdr->ip_src);
    ASSERT(ip_hdr->proto == IP_PROTO_ICMP);
    ASSERT(ip_hdr->length == ntohs(sizeof(ip_hdr_t) + 256));
    /*
     * and data
     */
    data = (unsigned char*) ip_hdr + sizeof(ip_hdr_t);
    for (i = 0; i < 256; i++)
        ASSERT(data[i] == buffer[i]);
    return 0;
}

/*
 * Create a raw IP / ICMP socket and use it to receive data - receive one message, then call recvfrom
 */
int testcase48() {
    int addrlen;
    ip_hdr_t* ip_hdr;
    unsigned char* data;
    net_msg_t* net_msg;
    socket_t* socket = net_socket_create(AF_INET, SOCK_RAW, IP_PROTO_ICMP);
    /*
     * Proto of socket should be IP_PROTO_ICMP now
     */
    ASSERT(IP_PROTO_ICMP == socket->proto.ip.ip_proto);
    struct sockaddr_in in;
    nic_t nic;
    unsigned char buffer[256];
    int i;
    ASSERT(socket);
    ip_init();
    /*
     * Bind socket to address 10.0.2.20
     */
    in.sin_family = AF_INET;
    in.sin_addr.s_addr = inet_addr("10.0.2.20");
    ASSERT(0 == socket->ops->bind(socket, (struct sockaddr*) &in, sizeof(struct sockaddr_in)));
    /*
     * Now simulate incoming message
     */
    net_msg = net_msg_new(256);
    ASSERT(net_msg);
    net_msg->nic = &nic;
    nic.ip_addr_assigned = 1;
    nic.ip_addr = 0x1402000a;
    data = net_msg_append(net_msg, 100);
    ASSERT(data);
    ip_hdr = (ip_hdr_t*) net_msg_prepend(net_msg, sizeof(ip_hdr_t));
    net_msg->ip_hdr = (void*) ip_hdr;
    ip_hdr->checksum = 0;
    ip_hdr->flags = ntohs(0x4000);
    ip_hdr->id = 0;
    ip_hdr->ip_dest = 0x1402000a;
    ip_hdr->ip_src = 0x1502000a;
    ip_hdr->length = ntohs(100 + sizeof(ip_hdr_t));
    ip_hdr->proto = IP_PROTO_ICMP;
    ip_hdr->ttl = 64;
    ip_hdr->version = 0x45;
    ip_hdr->checksum = htons(validate_ip_checksum(sizeof(ip_hdr_t), (u16*) ip_hdr));
    for (i = 0; i < 100; i++)
        data[i] = i;
    /*
     * and call ip_rx_msg
     */
    cond_broadcast_called = 0;
    icmp_rx_msg_called = 0;
    __net_loglevel = 0;
    ip_rx_msg(net_msg);
    __net_loglevel = 0;
    ASSERT(icmp_rx_msg_called);
    /*
     * We should be able to read the IP header and payload from our socket now
     */
    __net_loglevel = 1;
    addrlen = sizeof(struct sockaddr_in);
    ASSERT(120 == socket->ops->recvfrom(socket, buffer, 120, 0, (struct sockaddr*) &in, &addrlen));
    __net_loglevel = 0;
    /*
     * and a broadcast should have been issued
     */
    ASSERT(cond_broadcast_called);
    /*
     * Validate data: we should have the IP header followed by 100 bytes of data
     */
    for (i = 0; i < 100; i++) {
        ASSERT(buffer[i + sizeof(ip_hdr_t)] == (i % 256));
    }
    ip_hdr = (ip_hdr_t*) buffer;
    ASSERT(ip_hdr->ip_dest == inet_addr("10.0.2.20"));
    ASSERT(ip_hdr->ip_src == inet_addr("10.0.2.21"));
    /*
     * Check address
     */
    ASSERT(sizeof(struct sockaddr_in) == addrlen);
    ASSERT(in.sin_family == AF_INET);
    ASSERT(in.sin_addr.s_addr == inet_addr("10.0.2.21"));
    return 0;
}

/*
 * Main
 */
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
    END;
}
