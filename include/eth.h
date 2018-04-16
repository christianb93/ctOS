/*
 * eth.h
 *
 * Common defines for all Ethernet cards
 */

#ifndef _ETH_H_
#define _ETH_H_

#include "ktypes.h"
#include "pci.h"
#include "locks.h"

/*
 * The length of a MAC address
 */
#define ETH_ADDR_LEN 6

/*
 * The minimum allowed Ethernet packet size
 */
#define ETH_MIN_SIZE 64

/*
 * A MAC address
 */
typedef u8 mac_address_t[ETH_ADDR_LEN];

/*
 * The Ethernet MTU.
 */
#define MTU_ETH 1500

/*
 * An Ethernet frame header
 */
typedef struct {
    mac_address_t destination;
    mac_address_t source;
    u16 ethertype;
} __attribute__ ((packed)) eth_header_t;

/*
 * Ethertype valid values
 */
#define ETHERTYPE_IP 0x800
#define ETHERTYPE_ARP 0x806

void eth_dump_header(u8* buffer);


#endif /* _ETH_H_ */
