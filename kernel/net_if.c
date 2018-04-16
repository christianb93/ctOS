/*
 * net_if.c
 *
 * This is the network interface layer of the ctOS TCP/IP stack. The network interface layer serves as an abstraction layer between
 * the protocol layers in the TCP/IP stack and the device drivers. Currently the interface is in parts specific to Ethernet as link
 * layer, but could theoretically be extended to cover other link layer technologies as well
 *
 * This module contains functions to:
 * - register devices with the interface layer
 * - transmit a message via a network device
 * - multiplex incoming messages to the corresponding protocol layer
 * - assign protocol addresses to network interfaces
 *
 * Note that when the configuration of an interface changes, no locking is done to keep the complexity low and avoid the danger of
 * deadlocks with other interrupt or application driven operations of the networking stack. This is a deliberate decision, motivated
 * by the fact that changes in the interface configuration are not likely to happen concurrently and will break existing connections
 * anyway
 */

#include "eth.h"
#include "net_if.h"
#include "arp.h"
#include "ip.h"
#include "lib/stdint.h"
#include "lib/arpa/inet.h"
#include "debug.h"
#include "lib/os/errors.h"
#include "net.h"
#include "locks.h"
#include "lib/os/syscalls.h"
#include "lib/string.h"
#include "params.h"
#include "wq.h"
#include "util.h"
#include "lib/sys/ioctl.h"

static char* __module = "NETIF ";


extern int __net_loglevel;
#define NET_DEBUG(...) do {if (__net_loglevel > 0 ) { kprintf("DEBUG at %s@%d (%s): ", __FILE__, __LINE__, __FUNCTION__); \
        kprintf(__VA_ARGS__); }} while (0)


/*
 * NICs known to the interface layer
 */
static nic_entry_t registered_nics[NET_IF_MAX_NICS];

/*
 * Paket statistic
 */
static u32 rx_packets = 0;
static u32 tx_packets = 0;



/****************************************************************************************
 * The following functions are used by the protocol layers to forward messages to       *
 * device drivers for transmission over the network. A queuing mechanism is used to     *
 * avoid waiting, so that it can be guaranteed that the interface functions never block *
 ****************************************************************************************/

/*
 * Get net device operations structure for a given device
 */
static net_dev_ops_t* get_ops(nic_t* nic) {
    int i;
    for (i = 0; i < NET_IF_MAX_NICS; i++) {
        if (registered_nics[i].nic == nic) {
            if (registered_nics[i].ops) {
                if (registered_nics[i].ops->nic_tx_msg)
                    return registered_nics[i].ops;
            }
        }
    }
    return 0;
}


/*
 * This handler is used to do the actual transmission of a message. When a message arrives,
 * a corresponding entry is added to the workqueue NET_IF_QUEUE_ID which invokes this function
 * when being processed
 */
static int tx_handler(void* arg, int timeout) {
    net_msg_t* net_msg = (net_msg_t*) arg;
    net_dev_ops_t* ops = get_ops(net_msg->nic);
    if (timeout) {
        NET_DEBUG("Message timed out\n");
        net_msg_destroy(net_msg);
        return 0;
    }
    if (ops) {
        NET_DEBUG("Handing message over to Ethernet driver\n");
        /*
         * Try to send message - note that the network
         * driver might return EAGAIN
         */
        return ops->nic_tx_msg(net_msg);
    }
    else {
        ERROR("Invalid message - no interface ops found\n");
        return EIO;
    }
}

/*
 * Transmit a network message
 * Parameter:
 * @nic - the network interface via which the message is to be sent
 * @net_msg - the message
 * Return value:
 * 0 upon success
 * EIO if the message could not be processed
 * This function assumes that the following fields
 * in the network message have been set up
 * nic
 * hw_dest
 * ethertype
 * and that the message is filled with the Ethernet payload already
 */
int net_if_tx_msg(net_msg_t* net_msg) {
    net_dev_ops_t* ops;
    nic_t* nic = net_msg->nic;
    if (0 == nic)
        return EIO;
    ops = get_ops(nic);
    if (ops) {
        NET_DEBUG("Queuing message\n");
        if (-1 == wq_schedule(NET_IF_QUEUE_ID, tx_handler, (void*) net_msg, WQ_RUN_NOW)) {
            ERROR("Could not schedule message\n");
            return EIO;
        }
        atomic_incr(&tx_packets);
        return 0;
    }
    ERROR("NIC not registered with network interface layer\n");
    return EIO;
}




/****************************************************************************************
 * This is the interface of the network interface layer used by the device drivers      *
 ***************************************************************************************/


/*
 * Inform worker thread that a resource has become available again
 * This function is supposed to be invoked by the driver if is has
 * rejected a message previously with rc = EAGAIN due to a temporary
 * lack of resources like tx descriptors and these resources become
 * available again
 * Parameter:
 * @nic - the nic
 */
void net_if_tx_event(nic_t* nic) {
    wq_trigger(NET_IF_QUEUE_ID);
}

/*
 * Forward a packet to the corresponding protocol level
 * Parameter:
 * @net_msg - the network message
 */
void net_if_multiplex_msg(net_msg_t* net_msg) {
    u16 ethertype;
    if (net_msg->nic->hw_type != HW_TYPE_ETH) {
        ERROR("Ethernet is currently the only supported HW type\n");
        return;
    }
    net_msg_set_eth_hdr(net_msg, 0);
    eth_header_t* eth_header = (eth_header_t*) net_msg->eth_hdr;
    ethertype = ntohs(eth_header->ethertype);
    /*
     * Print message if loglevel > 0
     */
    if (__net_loglevel > 0) {
        kprintf("net_if:  ");
        eth_dump_header(net_msg->eth_hdr);
    }
    atomic_incr(&rx_packets);
    switch(ethertype) {
        case ETHERTYPE_ARP:
            net_msg_set_arp_hdr(net_msg, sizeof(eth_header_t));
            arp_rx_msg(net_msg);
            break;
        case ETHERTYPE_IP:
            net_msg_set_ip_hdr(net_msg, sizeof(eth_header_t));
            ip_rx_msg(net_msg);
            break;
        default:
            /*
             * Free data again
             */
            net_msg_destroy(net_msg);
    }
}

/*
 * Given a NIC, set the name field
 */
static int set_nic_name(nic_t* nic) {
    int hw_type = nic->hw_type;
    char* prefix;
    /*
     * Determine prefix
     */
    switch (hw_type) {
        case HW_TYPE_ETH:
            prefix = "eth";
            break;
        default:
            prefix = "net";
            break;
    }
    /*
     * Now determine number of existing NICs of this type
     */
    int count = 0;
    int i;
    for (i = 0; i < NET_IF_MAX_NICS; i++) {
            if (registered_nics[i].nic) {
                if (hw_type == registered_nics[i].nic->hw_type)
                    count++;
            }
    }
    /*
     * and derive full name. Note that here we assume that there are at most
     * 16 devices of the same type!
     */
    if (count > 16) {
        return -1;
    }
    strncpy(nic->name, prefix, 3);
    nic->name[3] = (count & 0xF) + '0';
    return 0;
}

/*
 * Register a NIC with the network interface layer
 * Parameter:
 * @nic - the network card to be registered
 * @ops - the corresponding public interface of the driver
 */
void net_if_add_nic(nic_t* nic, net_dev_ops_t* ops) {
    int i;
    NET_DEBUG("Adding NIC\n");
    /*
     * Determine name
     */
    if (-1 == set_nic_name(nic)) {
        ERROR("Maximum number of interfaces of type %d reached\n", nic->hw_type);
        return;
    }
    for (i = 0; i < NET_IF_MAX_NICS; i++) {
        if (0 == registered_nics[i].nic) {
            registered_nics[i].nic = nic;
            registered_nics[i].ops = ops;
            break;
        }
    }
    if (NET_IF_MAX_NICS == i )
        ERROR("Could not register NIC, maximum number reached\n");
    else
        MSG("Registered NIC %d with network interface layer\n", i);
}

/****************************************************************************************
 * Configuration of network devices                                                     *
 ***************************************************************************************/

/*
 * Given an IP address, determine the netmask based on the class A/B/C logic
 */
static unsigned int get_default_netmask(unsigned int ip_addr) {
    NET_DEBUG("Determine netmask for address %x\n", ip_addr);
    /*
     * If the first bit (i.e. lowest bit in network byte order) of
     * the IP address is 0, this is a class A network
     */
    if (0 == (ip_addr & 0x80)) {
        return NETMASK_CLASS_A;
    }
    /*
     * If the first two bits are 10, this is a class B network
     */
    if (0x80 == (ip_addr & 0xc0)) {
        return NETMASK_CLASS_B;
    }
    /*
     * If the first three bits are 110, this is a class C network
     */
    if (0xc0 == (ip_addr & 0xe0)) {
        return NETMASK_CLASS_C;
    }
    /*
     * Otherwise, we have classed D or E
     */
    return 0xffffffff;
}

/*
 * Set address and netmask for an interface
 */
static int setup_if(nic_t* nic, u32 ip_addr, u32 netmask) {
    struct rtentry rt_entry;
    struct sockaddr_in* in;
    nic->ip_addr = ip_addr;
    nic->ip_netmask = netmask;
    if (ip_addr) {
        nic->ip_addr_assigned = 1;
        /*
         * And add a new routing table entry for a direct route to the connected network
         */
        strncpy(rt_entry.dev, nic->name, 4);
        rt_entry.rt_flags = RT_FLAGS_UP;
        in = (struct sockaddr_in*) &rt_entry.rt_dst;
        in->sin_family = AF_INET;
        in->sin_addr.s_addr = netmask & ip_addr;
        in = (struct sockaddr_in*) &rt_entry.rt_genmask;
        in->sin_family = AF_INET;
        in->sin_addr.s_addr = netmask;
        return ip_add_route(&rt_entry);
    }
    else
        nic->ip_addr_assigned = 0;
    return 0;
}

/*
 * Assign a new interface address
 * Parameter:
 * @ifr - the interface request from the corresponding IOCTL
 * Return value:
 * 0 upon success
 * -ENODEV if device is not known
 * -EAFNOSUPPORT if address family is not supported
 */
int net_if_set_addr(struct ifreq* ifr) {
    unsigned int netmask;
    unsigned int ip_addr;
    nic_t* nic;
    /*
     * Locate NIC
     */
    if (0 == (nic = net_if_get_nic_by_name(ifr->ifrn_name))) {
        NET_DEBUG("Device not found\n");
        return -ENODEV;
    }
    /*
     * Check address family
     */
    if (AF_INET != ((struct sockaddr_in*) &ifr->ifr_ifru.ifru_addr)->sin_family)
        return -EAFNOSUPPORT;
    /*
     * If there was already an address assigned for this NIC, purge routing table
     */
    if (nic->ip_addr_assigned)
        ip_purge_nic(nic);
    /*
     * Get default netmask
     */
    ip_addr = ((struct sockaddr_in*) &ifr->ifr_ifru.ifru_addr)->sin_addr.s_addr;
    netmask = get_default_netmask(ip_addr);
    NET_DEBUG("Default netmask: %x\n", netmask);
    /*
     * Set address
     */
    return setup_if(nic, ip_addr, netmask);
}

/*
 * Get the interface address
 * Parameter:
 * @ifr - the interface request from the corresponding IOCTL
 * Return value:
 * 0 upon success
 * -ENODEV if device is not known
 */
int net_if_get_addr(struct ifreq* ifr) {
    struct sockaddr_in* in;
    nic_t* nic;
    /*
     * Locate NIC
     */
    if (0 == (nic = net_if_get_nic_by_name(ifr->ifrn_name))) {
        NET_DEBUG("Device not found\n");
        return -ENODEV;
    }
    /*
     * Get address
     */
    in = (struct sockaddr_in*) &ifr->ifr_ifru.ifru_addr;
    if (0 == nic->ip_addr_assigned)
        in->sin_addr.s_addr = INADDR_ANY;
    else
        in->sin_addr.s_addr = nic->ip_addr;
    in->sin_family = AF_INET;
    return 0;
}


/*
 * Assign a new interface netmask
 * Parameter:
 * @ifr - the interface request from the corresponding IOCTL
 * Return value:
 * 0 upon success
 * -ENODEV if device is not known
 * -EAFNOSUPPORT if address family is not supported
 */
int net_if_set_netmask(struct ifreq* ifr) {
    unsigned int netmask;
    unsigned int ip_addr;
    nic_t* nic;
    /*
     * Locate NIC
     */
    if (0 == (nic = net_if_get_nic_by_name(ifr->ifrn_name))) {
        NET_DEBUG("Device not found\n");
        return -ENODEV;
    }
    /*
     * Check address family
     */
    if (AF_INET != ((struct sockaddr_in*) &ifr->ifr_ifru.ifru_addr)->sin_family)
        return -EAFNOSUPPORT;
    /*
     * If there was already an address assigned for this NIC, purge routing table
     */
    if (nic->ip_addr_assigned)
        ip_purge_nic(nic);
    /*
     * Setup address
     */
    netmask = ((struct sockaddr_in*) &ifr->ifr_ifru.ifru_netmask)->sin_addr.s_addr;
    if (nic->ip_addr_assigned)
        ip_addr = nic->ip_addr;
    else
        ip_addr = INADDR_ANY;
    return setup_if(nic, ip_addr, netmask);
}

/*
 * Get the interface netmask
 * Parameter:
 * @ifr - the interface request from the corresponding IOCTL
 * Return value:
 * 0 upon success
 * -ENODEV if device is not known
 */
int net_if_get_netmask(struct ifreq* ifr) {
    nic_t* nic;
    /*
     * Locate NIC
     */
    if (0 == (nic = net_if_get_nic_by_name(ifr->ifrn_name))) {
        NET_DEBUG("Device not found\n");
        return -ENODEV;
    }
    /*
     * Get result
     */

    if (nic->ip_addr_assigned)
        ((struct sockaddr_in*) &ifr->ifr_ifru.ifru_netmask)->sin_addr.s_addr = nic->ip_netmask;
    else
        ((struct sockaddr_in*) &ifr->ifr_ifru.ifru_netmask)->sin_addr.s_addr = INADDR_ANY;
    ((struct sockaddr_in*) &ifr->ifr_ifru.ifru_netmask)->sin_family = AF_INET;
    return 0;
}

/****************************************************************************************
 * Initialization                                                                       *
 ***************************************************************************************/

/*
 * Initialize the network interface layer
 */
void net_if_init() {
    /*
     * Init statistics
     */
    tx_packets = 0;
    rx_packets = 0;
    /*
     * We do NOT reinit the NIC table as we are called after all devices have
     * been registered!
     */
}

/*
 * Remove all registered NICs again
 */
void net_if_remove_all() {
    int i;
    for (i = 0; i < NET_IF_MAX_NICS; i++)
        registered_nics[i].nic = 0;
}

/****************************************************************************************
 * Query interface table                                                                *
 ***************************************************************************************/

/*
 * Given an IP address, locate the first network device with that address
 */
nic_t* net_if_get_nic(u32 ip_address) {
    int i;
    for (i = 0; i < NET_IF_MAX_NICS; i++) {
        if (registered_nics[i].nic) {
            if (registered_nics[i].nic->ip_addr_assigned) {
                if (registered_nics[i].nic->ip_addr == ip_address)
                    return registered_nics[i].nic;
            }
        }
    }
    return 0;
}

/*
 * Given a name, get network device with that name
 * Parameter
 * @name - the name like eth0, up to 4 characters, not necessarily 0 terminated
 */
nic_t* net_if_get_nic_by_name(char* name) {
    int i;
    for (i = 0; i < NET_IF_MAX_NICS; i++) {
        if (registered_nics[i].nic) {
            if (0 == strncmp(registered_nics[i].nic->name, name, 4))
                return registered_nics[i].nic;
        }
    }
    return 0;
}



/*
 * Return interface configuration information (corresponds to ioctl SIOCGIFCONF)
 * for all interfaces registered with the interface layer
 */
int net_if_get_ifconf(struct ifconf* ifc) {
    int count = 0;
    struct ifreq* ifr;
    struct sockaddr_in* in_addr;
    int i;
    nic_t* nic;
    for (i = 0; i < NET_IF_MAX_NICS; i++) {
            nic = registered_nics[i].nic;
            if (nic) {
                NET_DEBUG("Found NIC #%d, count = %d\n", i, count);
                if (count*sizeof(struct ifreq) < ifc->ifc_len) {
                    /*
                     * Copy data from NIC to ifreq
                     */
                    ifr = ifc->ifc_ifcu.ifcu_req + count;
                    ifr->ifr_ifru.ifru_ivalue = i;
                    in_addr = (struct sockaddr_in*)  &ifr->ifr_ifru.ifru_addr;
                    in_addr->sin_family = AF_INET;
                    if (nic->ip_addr_assigned) {
                        in_addr->sin_addr.s_addr = nic->ip_addr;
                    }
                    else {
                        in_addr->sin_addr.s_addr = INADDR_ANY;
                    }
                    strncpy(ifr->ifrn_name, nic->name, 4);
                }
                else {
                    NET_DEBUG("Length of result field exceeded, count = %d\n", count);
                    break;
                }
                count++;
            }
    }
    ifc->ifc_len = count * sizeof(struct ifreq);
    return 0;
}


/****************************************************************************************
 * Everything below this line is for debugging only                                     *
 ***************************************************************************************/




/*
 * Print configuration information for a given card
 */
static void print_nic_config(net_dev_conf_t* config) {
    PRINT("Speed:               ");
    switch(config->speed) {
        case IF_SPEED_10:
            PRINT("10 MB/s\n");
            break;
        case IF_SPEED_100:
            PRINT("100 MB/s\n");
            break;
        case IF_SPEED_1000:
            PRINT("1000 MB/s\n");
            break;
        default:
            PRINT("Unknown\n");
            break;
    }
    PRINT("Auto neg. enabled:   %d\n", config->autoneg);
    PRINT("Full duplex:         %d\n", config->duplex);
    PRINT("Link established:    %d\n", config->link);
    PRINT("Port:                ");
    switch (config->port) {
        case IF_PORT_UNKNOWN:
            PRINT("AUTO\n");
            break;
        case IF_PORT_MII:
            PRINT("MII\n");
            break;
        case IF_PORT_TP:
            PRINT("TP\n");
            break;
    }
}

/*
 * Print connected NICs
 */
void net_if_print() {
    int i;
    nic_t* nic;
    net_dev_conf_t config;
    for (i = 0; i < NET_IF_MAX_NICS; i++) {
        nic = registered_nics[i].nic;
        if (nic) {
            if (registered_nics[i].ops) {
                if (registered_nics[i].ops->nic_get_config) {
                    registered_nics[i].ops->nic_get_config(nic, &config);
                    PRINT("ETH%d\n", i);
                    PRINT("---------------------------------\n");
                    print_nic_config(&config);

                }
                if (registered_nics[i].ops->nic_debug)
                    registered_nics[i].ops->nic_debug(nic);
            }
        }
    }
}

/*
 * Return number of processed packets
 */
u32 net_if_packets() {
    return tx_packets + rx_packets;
}
