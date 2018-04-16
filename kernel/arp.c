/*
 * arp.c
 *
 * This is the source code for the ARP protocol layer within the TCP/IP network stack
 */

#include "arp.h"
#include "net.h"
#include "debug.h"
#include "locks.h"
#include "lib/stdint.h"
#include "lib/arpa/inet.h"
#include "mm.h"
#include "lib/os/syscalls.h"
#include "net_if.h"
#include "lib/string.h"
#include "lib/os/errors.h"
#include "timer.h"
#include "wq.h"

extern int __net_loglevel;
#define NET_DEBUG(...) do {if (__net_loglevel > 0 ) { kprintf("DEBUG at %s@%d (%s): ", __FILE__, __LINE__, __FUNCTION__); \
        kprintf(__VA_ARGS__); }} while (0)



/*
 * The ARP cache
 */
static arp_cache_entry_t arp_cache[ARP_CACHE_ENTRIES];


/*
 * A lock used to protect the ARP cache
 */
static spinlock_t arp_lock;



/****************************************************************************************
 * These functions are used to manage the ARP cache                                     *
 ***************************************************************************************/


/*
 * Locate a free entry in the ARP cache and add the IP address and MAC address as new
 * entry. If the entry already exists, it will be updated.
 * Parameter:
 * @ip_address - the IP address to be added
 * @mac_address - the MAC address to be added
 * Return value:
 * 0 upon success
 * -1 if no free entry is available
 * Locks:
 * lock on ARP cache
 */
static int add_cache_entry(u32 ip_address, mac_address_t mac_address) {
    int i;
    int hit = 0;
    int free = -1;
    u32 eflags;
    spinlock_get(&arp_lock, &eflags);
    /*
     * First scan the cache to either locate and update the entry
     * or to locate a free slot
     */
    for (i = 0; i < ARP_CACHE_ENTRIES; i++) {
        if ((-1 == free) & (ARP_STATUS_FREE == arp_cache[i].status))
            free = i;
        if (arp_cache[i].ip_addr == ip_address) {
            eth_address_copy(arp_cache[i].mac_addr, mac_address);
            hit = 1;
            arp_cache[i].status = ARP_STATUS_VALID;
            arp_cache[i].last_request = 0;
            break;
        }
    }
    if (hit) {
        spinlock_release(&arp_lock, &eflags);
        return 0;
    }
    /*
     * Entry was not found, so add a new one
     */
    if (-1 == free)
        return -1;
    eth_address_copy(arp_cache[free].mac_addr, mac_address);
    arp_cache[free].ip_addr = ip_address;
    arp_cache[free].status = ARP_STATUS_VALID;
    arp_cache[free].last_request = 0;
    spinlock_release(&arp_lock, &eflags);
    return 0;
}


/*
 * Locate an entry in the ARP cache for the specified IP address. If no entry exists, add an
 * incomplete entry. If an entry exists but is not valid, check its timestamp. If the time
 * of the last request is longer than @delay ticks in the past, update timestamp
 * Parameter:
 * @ip_address - the IP address to be searched for
 * @mac_address - if a valid entry is found, the hardware address is stored here
 * @delay - if timestamp of an incomplete entry is longer than @delay in the past, update it
 * Return value:
 * ARP_RESULT_HIT upon success
 * ARP_RESULT_NONE if no entry could be found and an incomplete entry has been added
 * ARP_RESULT_INCOMPLETE if an entry exists but is incomplete and its timestamp is younger than @delay ticks
 * ARP_RESULT_TRIGGER if an entry exists, is incomplete and its timestamp is longer than @delay in the past
 * -1 if an error occurred
 * Locks:
 * arp_lock
 */
static int get_cache_entry(u32 ip_address, mac_address_t* mac_address, u32 delay) {
    int i;
    int j;
    int rc = -1;
    int hit = 0;
    int free = -1;
    u32 my_time;
    u32 eflags;
    NET_DEBUG("Asking for IP address %x\n", ip_address);
    spinlock_get(&arp_lock, &eflags);
    /*
     * Scan the cache to either locate and update the entry
     * or to locate a free slot
     */
    for (i = 0; i < ARP_CACHE_ENTRIES; i++) {
        if ((-1 == free) & (ARP_STATUS_FREE == arp_cache[i].status))
            free = i;
        if (arp_cache[i].ip_addr == ip_address) {
            /*
             * Is the entry valid?
             */
            if (ARP_STATUS_VALID == arp_cache[i].status) {
                for (j = 0; j < ETH_ADDR_LEN; j++)
                    (*mac_address)[j] = arp_cache[i].mac_addr[j];
                rc = ARP_RESULT_HIT;
                hit = 1;
            }
            /*
             * If entry is incomplete, check timestamp
             */
            else {
                my_time = timer_get_ticks(0);
                if (my_time < arp_cache[i].last_request + delay) {
                    NET_DEBUG("Found incomplete entry, not yet due for next request\n");
                    rc = ARP_RESULT_INCOMPLETE;
                }
                else {
                    rc = ARP_RESULT_TRIGGER;
                    arp_cache[i].last_request = my_time;
                }
                hit = 1;
            }
            if (hit) {
                spinlock_release(&arp_lock, &eflags);
                return rc;
            }
        }
    }
    /*
     * No entry was found
     */
    if (-1 == free) {
        spinlock_release(&arp_lock, &eflags);
        return -1;
    }
    /*
     * Add a new, incomplete entry
     */
    arp_cache[free].ip_addr = ip_address;
    arp_cache[free].status = ARP_STATUS_INCOMPLETE;
    arp_cache[free].last_request = 0;
    arp_cache[free].last_request = timer_get_ticks(0);
    spinlock_release(&arp_lock, &eflags);
    return ARP_RESULT_NONE;
}

/****************************************************************************************
 * Process an incoming ARP packet or a request to resolve an address                    *
 ***************************************************************************************/

/*
 * Send an ARP reply
 */
static void send_reply(net_msg_t* request) {
    arp_packet_header_t* arp_header = (arp_packet_header_t*) request->arp_hdr;
    arp_packet_header_t* reply_header = 0;
    arp_eth_ip_t* arp_data = (arp_eth_ip_t*) (((u8*) arp_header) + sizeof(arp_packet_header_t));
    arp_eth_ip_t* reply_data = 0;
    net_msg_t* reply = net_msg_create(ARP_PACKET_LENGTH(arp_header) + sizeof(eth_header_t), sizeof(eth_header_t));
    int i;
    if (0 == reply) {
        ERROR("Discarding ARP reply due to memory issue\n");
        return;
    }
    reply_header = (arp_packet_header_t*) net_msg_append(reply, sizeof(arp_packet_header_t));
    /*
     * Set up header of reply
     */
    reply_header->hw_addr_len = arp_header->hw_addr_len;
    reply_header->hw_addr_type = arp_header->hw_addr_type;
    reply_header->op_code = htons(ARP_OPCODE_REPLY);
    reply_header->proto_addr_len = arp_header->proto_addr_len;
    reply_header->proto_addr_type = arp_header->proto_addr_type;
    reply->nic = request->nic;
    reply->arp_hdr = (void*) reply_header;
    memcpy((void*) reply->hw_dest, arp_data->source_hw_addr, ETH_ADDR_LEN);
    reply->ethertype = htons(ETHERTYPE_ARP);
    /*
     * Now set up data
     */
    reply_data = (arp_eth_ip_t*) net_msg_append(reply, sizeof(arp_eth_ip_t));
    if (0 == reply_data) {
        PANIC("Should never happen\n");
        return;
    }
    for (i = 0; i < ETH_ADDR_LEN; i++)
        reply_data->dest_hw_addr[i] = arp_data->source_hw_addr[i];
    reply_data->dest_proto_addr = arp_data->source_proto_addr;
    for (i = 0; i < ETH_ADDR_LEN; i++)
        reply_data->source_hw_addr[i] = request->nic->mac_address[i];
    reply_data->source_proto_addr = arp_data->dest_proto_addr;
    /*
     * and send message
     */
    net_if_tx_msg(reply);
}

/*
 * Send an ARP request
 */
static void send_request(nic_t* nic, u32 ip_address) {
    arp_packet_header_t* request_header = 0;
    arp_eth_ip_t* request_data = 0;
    net_msg_t* request = net_msg_create(sizeof(arp_packet_header_t) + sizeof(arp_eth_ip_t) + sizeof(eth_header_t), sizeof(eth_header_t));
    int i;
    if (0 == request) {
        ERROR("Discarding ARP request due to memory issue\n");
        return;
    }
    request_header = (arp_packet_header_t*) net_msg_append(request, sizeof(arp_packet_header_t));
    /*
     * Set up header of request
     */
    request_header->hw_addr_len = ETH_ADDR_LEN;
    request_header->hw_addr_type = htons(ARP_HW_ADDR_TYPE_ETH);
    request_header->op_code = htons(ARP_OPCODE_REQUEST);
    request_header->proto_addr_len = sizeof(u32);
    request_header->proto_addr_type = htons(ARP_PROTO_ADDR_TYPE_IP);
    request->nic = nic;
    request->arp_hdr = (void*) request_header;
    for (i = 0; i < ETH_ADDR_LEN; i++)
        request->hw_dest[i] = 0xff;
    request->ethertype = htons(ETHERTYPE_ARP);
    /*
     * Now set up data
     */
    request_data = (arp_eth_ip_t*) net_msg_append(request, sizeof(arp_eth_ip_t));
    if (0 == request_data) {
        PANIC("Should never happen\n");
        return;
    }
    for (i = 0; i < ETH_ADDR_LEN; i++)
        request_data->dest_hw_addr[i] = 0;
    /*
     * IP address is internally stored in network byte order already (for instance
     * 10.0.2.21 is 0x1502000a), so we do not need to convert them
     */
    request_data->dest_proto_addr = ip_address;
    for (i = 0; i < ETH_ADDR_LEN; i++)
        request_data->source_hw_addr[i] = nic->mac_address[i];
    request_data->source_proto_addr = nic->ip_addr;
    /*
     * and send message
     */
    NET_DEBUG("Sending ARP request for IP address %x\n", ip_address);
    net_if_tx_msg(request);
}

/*
 * Process an incoming ARP packet
 * Parameter:
 * @net_msg - the message containing the packet
 */
void arp_rx_msg(net_msg_t* net_msg) {
    arp_eth_ip_t* arp_data = 0;
    arp_packet_header_t* arp_header = (arp_packet_header_t*) net_msg->arp_hdr;
    if ((ntohs(arp_header->hw_addr_type) != ARP_HW_ADDR_TYPE_ETH) || (ntohs(arp_header->proto_addr_type) != ARP_PROTO_ADDR_TYPE_IP)
            || (arp_header->hw_addr_len != ETH_ADDR_LEN) || (arp_header->proto_addr_len != 4)) {
        ERROR("Unsupported ARP protocol types\n");
        return;
    }
    arp_data = (arp_eth_ip_t*) (((u8*) arp_header) + sizeof(arp_packet_header_t));
    /*
     * Add entry or update existing entry
     */
    NET_DEBUG("Adding cache entry for IP address %x\n", arp_data->source_proto_addr);
    if (-1 == add_cache_entry(arp_data->source_proto_addr, arp_data->source_hw_addr))
        ERROR("ARP Cache full\n");
    /*
     * Inform the IP transmission work queue about the new entry so that any requests waiting
     * for ARP address resolution can be processed
     */
    wq_trigger(IP_TX_QUEUE_ID);
    /*
     * If the entry refers to our own IP address and is a request, prepare a reply
     * and hand it over to the network interface layer
     */
    if ((net_msg->nic->ip_addr == arp_data->dest_proto_addr) && (net_msg->nic->ip_addr_assigned) &&
            (ARP_OPCODE_REQUEST == ntohs(arp_header->op_code))) {
        send_reply(net_msg);
    }
    /*
     * and free network message
     */
    net_msg_destroy(net_msg);
}

/*
 * Try to resolve an IP address in a LAN
 * Parameter:
 * @nic - the NIC connected to the LAN (used for sending ARP requests)
 * @ip_address - address to be resolved
 * @mac_address - result is stored here
 * Return value:
 * 0 upon success
 * EAGAIN if a request had to be submitted
 * -1 if an unrecoverable error occurred
 */
int arp_resolve(nic_t* nic, u32 ip_address, mac_address_t* mac_address) {
    /*
     * Try to resolve address
     */
    int rc = get_cache_entry(ip_address, mac_address, ARP_DELAY);
    NET_DEBUG("get_cache_entry for IP address %x returned with rc = %d\n", ip_address, rc);
    /*
     * If we have found an entry, return it
     */
    if (ARP_RESULT_HIT == rc)
        return 0;
    /*
     * If an error occurred, give up
     */
    if (-1 == rc)
        return -1;
    /*
     * If we have found an incomplete entry for which the last request has been sent
     * at most ARP_DELAY ticks in the past, or if we have just added a new, incomplete entry,
     * send request
     */
    if ((ARP_RESULT_TRIGGER == rc) || (ARP_RESULT_NONE == rc)) {
        send_request(nic, ip_address);
    }
    return EAGAIN;
}

/****************************************************************************************
 * Initialization                                                                       *
 ***************************************************************************************/


/*
 * Initialize the ARP protocol layer
 */
void arp_init() {
    /*
     * Initialize spinlocks
     */
    spinlock_init(&arp_lock);
}
