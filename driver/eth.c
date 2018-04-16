/*
 * Common functions used for all Ethernet cards
 */



#include "eth.h"
#include "net.h"
#include "kprintf.h"
#include "lib/stdint.h"
#include "lib/arpa/inet.h"
#include "lib/os/errors.h"
#include "lib/string.h"


/*
 * Dump an Ethernet frame
 * Parameter
 * @buffer - the packet data
 */
void eth_dump_header(u8* buffer) {
    eth_header_t* eth_header = (eth_header_t*) buffer;
    kprintf("%h:%h:%h:%h:%h:%h --> ", eth_header->source[0], eth_header->source[1], eth_header->source[2],
            eth_header->source[3], eth_header->source[4], eth_header->source[5]);
    kprintf("%h:%h:%h:%h:%h:%h ", eth_header->destination[0], eth_header->destination[1], eth_header->destination[2],
            eth_header->destination[3], eth_header->destination[4], eth_header->destination[5]);
    kprintf("(Ethertype = %w) \n", ntohs(eth_header->ethertype));
}

/*
 * Set up an Ethernet header for a network message. Destination address,
 * source address and ethertype are taken from the network message
 * Parameter:
 * @net_msg - the network message
 * Return value:
 * 0 upon success
 * ENOMEM if there is not enough room left in the message
 */
int eth_create_header(net_msg_t* net_msg) {
    eth_header_t* eth_header;
    if (0 == (eth_header = (eth_header_t*) net_msg_prepend(net_msg, sizeof(eth_header_t)))) {
        ERROR("Not enough headroom left to add Ethernet header\n");
        return ENOMEM;
    }
    memcpy(eth_header->destination, net_msg->hw_dest, ETH_ADDR_LEN);
    memcpy(eth_header->source, net_msg->nic->mac_address, ETH_ADDR_LEN);
    eth_header->ethertype = net_msg->ethertype;
    return 0;
}

/*
 * Copy a MAC address
 */
void eth_address_copy(mac_address_t to, mac_address_t from) {
    memcpy((void*) to, (void*) from, ETH_ADDR_LEN);
}
