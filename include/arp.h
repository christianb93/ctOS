/*
 * arp.h
 *
 */

#ifndef _ARP_H_
#define _ARP_H_

#include "ktypes.h"
#include "eth.h"
#include "net.h"

/*
 * An ARP packet header
 */
typedef struct {
    u16 hw_addr_type;                       // Hardware address type, 0x1 = Ethernet
    u16 proto_addr_type;                    // Protocol address type, 0x800 = IP
    u8 hw_addr_len;                         // hardware address length
    u8 proto_addr_len;                      // protocol address length
    u16 op_code;                            // 0x1 = request, 0x2 = reply
} __attribute__ ((packed)) arp_packet_header_t;

/*
 * An ARP packet data section for Ethernet and IP
 */
typedef struct {
    mac_address_t source_hw_addr;
    u32 source_proto_addr;
    mac_address_t dest_hw_addr;
    u32 dest_proto_addr;
} __attribute__ ((packed)) arp_eth_ip_t;

/*
 * An entry in the ARP cache
 */
typedef struct {
    u32 ip_addr;                       // the IP address
    mac_address_t mac_addr;            // the hardware address
    int status;                        // the status
    u32 last_request;                  // timestamp recording when the last request has been sent for this IP address
} arp_cache_entry_t;


/*
 * Status of an ARP cache entry
 */
#define ARP_STATUS_FREE 0
#define ARP_STATUS_INCOMPLETE 1
#define ARP_STATUS_VALID 2

/*
 * Some ARP protocol constants
 */
#define ARP_HW_ADDR_TYPE_ETH 0x1
#define ARP_PROTO_ADDR_TYPE_IP 0x800
#define ARP_OPCODE_REQUEST 0x1
#define ARP_OPCODE_REPLY 0x2

/*
 * The total size of an ARP message, not including the link layer header
 */
#define ARP_PACKET_LENGTH(header) ((sizeof(arp_packet_header_t) + 2*header->hw_addr_len + 2*header->proto_addr_len))
/*
 * Number of entries in the ARP cache
 */
#define ARP_CACHE_ENTRIES 1024

/*
 * Size of ARP queue
 */
#define ARP_QUEUE_SIZE 1024


/*
 * Results of an ARP request
 */
#define ARP_RESULT_HIT 1
#define ARP_RESULT_NONE 2
#define ARP_RESULT_INCOMPLETE 3
#define ARP_RESULT_TRIGGER 4

/*
 * Delay between subsequent ARP requests in ticks. We send another request every 100 ms
 */
#define ARP_DELAY 10

void arp_rx_msg(net_msg_t* net_msg);
void arp_init();
int arp_resolve(nic_t* nic, u32 ip_address, mac_address_t* mac_address);

#endif /* _ARP_H_ */
