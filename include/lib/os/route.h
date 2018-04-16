/*
 * route.h
 *
 */

#ifndef _ROUTE_H_
#define _ROUTE_H_

#include "../netinet/in.h"
#include "if.h"

/*
 * This structure describes a routing table entry and is used by the ioctl
 * calls related to the maintenance of the routing table.
 */
struct rtentry {
    struct sockaddr rt_dst;            // Destination address
    struct sockaddr rt_gateway;        // Gateway address
    struct sockaddr rt_genmask;        // Mask
    char dev[IFNAMSIZ];                // Name of device
    unsigned short rt_flags;           // flags
};

struct rtconf  {
    int rtc_len;                             // size of buffer
    union {
        char *rtcu_buf;
        struct rtentry *rtcu_req;
    } rtc_rtcu;
};



/*
 * Route entry flags
 */
#define RT_FLAGS_GW 0x1                // indirect route, i.e. via a gateway
#define RT_FLAGS_UP 0x2                // route can be used

#endif /* _ROUTE_H_ */
