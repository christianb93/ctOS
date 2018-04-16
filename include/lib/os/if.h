/*
 * if.h
 *
 * This header contains definitions related to network interfaces
 */

#ifndef _IF_H_
#define _IF_H_

#include "../netinet/in.h"

#define IFNAMSIZ 4

/*
 * Maximum number of DNS servers registered with the kernel
 */
#define MAX_DNS_SERVERS 3


struct ifreq {
    char  ifrn_name[IFNAMSIZ];               // interface name (like eth0)
    union {
        struct  sockaddr ifru_addr;          // interface address
        struct  sockaddr ifru_netmask;       // interface netmask
        int ifru_ivalue;                     // index of interface, starting with zero
    } ifr_ifru;
};

struct ifconf  {
    int ifc_len;                             // size of buffer
    union {
        char *ifcu_buf;
        struct ifreq *ifcu_req;
    } ifc_ifcu;
    unsigned int ifc_dns_servers[MAX_DNS_SERVERS];
};

#endif /* _IF_H_ */
