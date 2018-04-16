/*
 * net_if.h
 *
 */

#ifndef _NET_IF_H_
#define _NET_IF_H_

#include "ktypes.h"
#include "net.h"
#include "lib/os/if.h"


/*
 * Configuration of a network driver
 */
typedef struct {
    u8 speed;            // Network speed (0 = 10, 1 = 100, 2 = 1000)
    u8 autoneg;          // Auto-negotiation enabled
    u8 duplex;           // Duplex mode (0 = half, 1 = full)
    u8 port;             // 0 = on-chip transceiver, 1 = MII
    u8 link;             // 1 = link established
} net_dev_conf_t;

/*
 * The public interface of a network driver
 */
typedef struct {
    int (*nic_tx_msg)(net_msg_t* msg);                    // Transmit a message - should never block or sleep
    int (*nic_get_config)(nic_t* nic, net_dev_conf_t*);   // get current configuration
    void (*nic_debug)(nic_t* nic);                        // Print debugging output
} net_dev_ops_t;


/*
 * This structure is used to maintain a table of registered NICs
 */
typedef struct {
    nic_t* nic;
    net_dev_ops_t* ops;
} nic_entry_t;

/*
 * The maximum number of NICs which can be registered. Note that our naming scheme depends
 * on this!
 */
#define NET_IF_MAX_NICS 16

/*
 * The size of the transmission queue - needs to be a power of 2
 */
#define TX_QUEUE_SIZE 1024

/*
 * Network speed and duplex mode
 */
#define IF_SPEED_10 0
#define IF_SPEED_100 1
#define IF_SPEED_1000 2
#define IF_DUPLEX_FULL 1
#define IF_DUPLEX_HALF 0

/*
 * Medium
 */
#define IF_PORT_UNKNOWN 0
#define IF_PORT_TP 1
#define IF_PORT_MII 2

/*
 * Default MTU
 */
#define NET_IF_DEFAULT_MTU 576


void net_if_multiplex_msg(net_msg_t* net_msg);
void net_if_add_nic(nic_t* nic, net_dev_ops_t* ops);
int net_if_tx_msg(net_msg_t* net_msg);
void net_if_tx_event(nic_t* nic);
void net_if_init();
nic_t* net_if_get_nic(u32 ip_address);
nic_t* net_if_get_nic_by_name(char* name);
void net_if_print();
u32 net_if_packets();
int net_if_get_ifconf(struct ifconf* ifc);
void net_if_remove_all();
int net_if_set_addr(struct ifreq* ifr);
int net_if_get_addr(struct ifreq* ifr);
int net_if_set_netmask(struct ifreq* ifr);
int net_if_get_netmask(struct ifreq* ifr);

#endif /* _NET_IF_H_ */
