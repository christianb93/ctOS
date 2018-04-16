/*
 * udp.c
 *
 * This is the UDP module of the ctOS kernel. It implements the operations on UDP sockets which are required by the socket
 * layer interface defined in the structure socket_ops as well as the entry point udp_rx_msg which is called by the multiplexing
 * code in the IP layer if a UDP datagram is received.
 *
 * Similar to TCP, UDP sockets are described by a UDP control block (UCB), i.e. an instance of the structure udp_socket_t which is
 * embedded into a socket_t structure. Once a UCB has been created and the socket has been bound to a local address, the UCB is added to
 * a linked list of UCBs which is searched for a match when an incoming datagram needs to be forwarded to the target socket based on
 * local and foreign IP address.
 *
 * The lifecycle of a UCB is controlled using a reference count field within upd_socket_t.
 *
 * There are basically three different types of locks involved in protecting the data structures within the UDP module
 *
 * 1) the socket level lock socket->lock
 * 2) a lock protecting the socket list described above
 * 3) a lock protecting the reference count
 *
 * Similar to the TCP module, only certain orders of acquiring these locks are allowed in order to avoid deadlocks - note that
 * in most cases, the generic socket layer will already hold the lock on the socket level upon entering one of the interface
 * functions in this module
 *
 *                   --------------------------   lock on
 *                   |                          socket list
 *                   |                               A
 *                   V                               |
 *        lock on reference count  <---------   lock on socket
 *
 */

#include "net.h"
#include "udp.h"
#include "mm.h"
#include "lib/string.h"
#include "lib/os/errors.h"
#include "lib/stddef.h"
#include "lists.h"
#include "ip.h"
#include "debug.h"
#include "util.h"
#include "debug.h"
#include "icmp.h"

extern int __net_loglevel;
#define NET_DEBUG(...) do {if (__net_loglevel > 0 ) { kprintf("DEBUG at %s@%d (%s): ", __FILE__, __LINE__, __FUNCTION__); \
        kprintf(__VA_ARGS__); }} while (0)


/*
 * This is a list of bound sockets which are eligible for receiving UDP packets
 */
static udp_socket_t* socket_list_head = 0;
static udp_socket_t* socket_list_tail = 0;
static spinlock_t socket_list_lock;

/*
 * Forward declarations
 */
static void udp_release_socket(socket_t* socket);
static int udp_bind(socket_t* socket, struct sockaddr* address, int addrlen);
static int udp_connect_socket(socket_t* socket, struct sockaddr* addr, int addrlen);
static int udp_listen(socket_t* socket);
static int udp_close_socket(socket_t* socket, u32* eflags);
static int udp_recv(socket_t* socket, void* buffer, unsigned int len, int flags);
static int udp_send(socket_t* socket, void* buffer, unsigned int len, int flags);
static int udp_recvfrom(socket_t* socket, void* buffer, unsigned int len, int flags, struct sockaddr* addr, u32* addrlen);
static int udp_sendto(socket_t* socket, void* buffer, unsigned int len, int flags, struct sockaddr* addr, u32 addrlen);
static int udp_select(socket_t* socket, int read, int write);

/*
 * Socket operations which can be performed on a UDP socket
 */
static socket_ops_t udp_socket_ops = {udp_connect_socket, udp_close_socket, udp_send, udp_recv, udp_listen, udp_bind, udp_select,
        udp_release_socket, udp_sendto, udp_recvfrom};

/****************************************************************************************
 * These functions are used to manage the reference count of a socket                   *
 ***************************************************************************************/

/*
 * Drop a reference to a socket. If the reference count of the socket drops to
 * zero, free memory held by socket and release reference count on parent
 * Parameter:
 * @socket - the socket to be released
 * Reference count:
 * Reference count of socket is decreased by one
 * Locks:
 * lock on sockets reference count
 */
static void udp_release_socket(socket_t* socket) {
    net_msg_t* net_msg;
    net_msg_t* next;
    u32 eflags;
    int ref_count;
    /*
     * Get lock
     */
    spinlock_get(&socket->proto.udp.ref_count_lock, &eflags);
    /*
     * Decrease reference count
     */
    socket->proto.udp.ref_count--;
    ref_count = socket->proto.udp.ref_count;
    /*
     * and release lock again
     */
    spinlock_release(&socket->proto.udp.ref_count_lock, &eflags);
    /*
     * If we have reached zero, free memory. Even though
     * we have released the lock again, this cannot be changed
     * by any other thread as no other thread still holds a reference
     */
    if (0 == ref_count) {
        /*
         * First free all network messages on the queue
         */
        net_msg = socket->proto.udp.rcv_buffer_head;
        while(net_msg) {
            next = net_msg->next;
            net_msg_destroy(net_msg);
            net_msg = next;
        }
        /*
         * Now free socket
         */
        kfree((void*) socket);
    }
}

/*
 * Clone a reference to a socket, i.e. increase the reference count
 * by one
 * Parameter:
 * @socket - the socket
 * Return value:
 * a pointer to the socket
 * Reference count:
 * The reference count is increased by one
 * Locks:
 * lock on sockets reference count
 */
static socket_t* clone_socket(socket_t* socket) {
    u32 eflags;
    if (0 == socket)
        return 0;
    /*
     * Get lock on socket
     */
    spinlock_get(&socket->proto.udp.ref_count_lock, &eflags);
    /*
     * Increase reference count
     */
    socket->proto.udp.ref_count++;
    /*
     * Release lock
     */
    spinlock_release(&socket->proto.udp.ref_count_lock, &eflags);
    /*
     * and return pointer to socket
     */
    return socket;
}

/****************************************************************************************
 * All UDP sockets are kept in a doubly linked list of UDP sockets aka UDP control      *
 * blocks. The following functions manage this list                                     *
 ***************************************************************************************/

/*
 * Given local and foreign IP address and port number, locate
 * a TCP socket in the list of UDP sockets which matches best
 * Parameter:
 * @local_ip - local IP address (in network byte order)
 * @foreign_ip - foreign IP address
 * @local_port - local port number (in network byte order)
 * @foreign_port - foreign port
 * Returns:
 * Pointer to best match or 0
 * The caller should hold the lock on the socket list. The reference count
 * of the result is not increased, this needs to be done by the caller
 */
static udp_socket_t* get_matching_ucb(u32 local_ip, u32 foreign_ip, u16 local_port, u16 foreign_port) {
    int matchlevel = -1;
    int this_matchlevel;
    udp_socket_t* best_match = 0;
    socket_t* socket;
    udp_socket_t* item;
    struct sockaddr_in* laddr;
    struct sockaddr_in* faddr;
    /*
     * Scan list of existing UDP control blocks. If we find a better match than the given
     * one, update best_match. Only consider a control block a match if the port number matches the
     * local port number. Then matchlevel is the number of non-wildcard matches
     */
    LIST_FOREACH(socket_list_head, item) {
        socket = UCB2SOCK(item);
        laddr = (struct sockaddr_in*) &socket->laddr;
        faddr = (struct sockaddr_in*) &socket->faddr;
        if (laddr->sin_port == local_port) {
            this_matchlevel = 0;
            /*
             * Does local IP address match taking wildcards into account?
             */
            if (laddr->sin_addr.s_addr == local_ip) {
                /*
                 * Found match without needing a wildcard - increase matchlevel
                 */
                this_matchlevel++;
            }
            else {
                /*
                 * No direct match - is it a match taking wildcards into account? If no,
                 * this is not a match at all, go to next socket in list
                 */
                if ((INADDR_ANY != laddr->sin_addr.s_addr) && (INADDR_ANY != local_ip))
                    break;
            }
            /*
             * Repeat this for foreign IP address
             */
            if (faddr->sin_addr.s_addr == foreign_ip) {
                this_matchlevel++;
            }
            else {
                if ((INADDR_ANY != faddr->sin_addr.s_addr) && (INADDR_ANY != foreign_ip))
                    break;
            }
            /*
             * If we get to this point, both local and foreign IP address match, possibly using wildcards.
             * Check whether local port number matches
             */
            if (faddr->sin_port == foreign_port) {
                this_matchlevel++;
            }
            else {
                if ((0 != faddr->sin_port) && (0 != foreign_port))
                    break;
            }
            /*
             * If the current matchlevel is better than the previous one, this is our new
             * best match
             */
            if (this_matchlevel > matchlevel) {
                matchlevel = this_matchlevel;
                best_match = item;
            }
        }
    }
    return best_match;
}

/*
 * Drop a socket, i.e. remove it from the list of UCBs used for multiplexing.
 * The socket will still exist, but will no longer be reachable
 * Parameter:
 * @socket - the socket
 * Locks:
 * lock on socket list
 * Cross-monitor function calls:
 * ucb_release_socket
 * Reference count:
 * decrease reference count of socket by one
 */
static void unregister_socket(udp_socket_t* socket) {
    u32 eflags;
    udp_socket_t* ucb = 0;
    int found;
    /*
     * Get lock on socket list
     */
    spinlock_get(&socket_list_lock, &eflags);
    /*
     * First make sure that socket is in list
     */
    LIST_FOREACH(socket_list_head, ucb) {
        if (ucb == socket)
            found = 1;
    }
    if (found) {
        LIST_REMOVE(socket_list_head, socket_list_tail, socket);
        /*
         * Decrease reference count to account for the reference held by the list
         * until now.
         */
        KASSERT(socket->ref_count);
        udp_release_socket(UCB2SOCK(socket));
    }
    /*
     * and release socket on list
     */
    spinlock_release(&socket_list_lock, &eflags);
}

/*
 * Get a free UDP ephemeral port number, i.e. a port number which is not yet used
 * by any other socket. It is assumed that the caller holds the lock on the
 * socket list
 * Parameter:
 * @socket - the socket
 * Return value:
 * -1 if no free port number was found
 * a free port number otherwise
 */
static int find_free_port() {
    int i;
    udp_socket_t* ucb;
    socket_t* _socket;
    struct sockaddr_in* in_addr;
    int port_used;
    for (i = UDP_EPHEMERAL_PORT; i < 65536; i++) {
        port_used = 0;
        LIST_FOREACH(socket_list_head, ucb) {
            _socket = UCB2SOCK(ucb);
            in_addr = (struct sockaddr_in*) &_socket->laddr;
            if (in_addr->sin_port == htons(i)) {
                port_used = 1;
                break;
            }
        }
        if (0 == port_used) {
            return i;
        }
    }
    return -1;
}


/****************************************************************************************
 * Initialize the UDP layer                                                             *
 ***************************************************************************************/

void udp_init() {
    /*
     * Init spinlock
     */
    spinlock_init(&socket_list_lock);
    /*
     * and socket list
     */
    socket_list_head = 0;
    socket_list_tail = 0;
}

/****************************************************************************************
 * Socket operations                                                                    *
 ***************************************************************************************/

/*
 * Initialize a UDP socket, i.e. initialize all fields
 * Parameter:
 * @socket - the socket to be initialized
 * @domain - the domain, i.e. AF_INET
 * @proto - the protocol to use, not used for UDP sockets
 * Return value:
 * 0 - successful completion
 * Reference count:
 * the reference count of the socket is set to two
 */
int udp_create_socket(socket_t* socket, int domain, int proto) {
    /*
     * Initialize all fields with zero
     */
    memset((void*) socket, 0, sizeof(socket_t));
    /*
     * Fill operations structure
     */
    socket->ops = &udp_socket_ops;
    /*
     * Set reference count. We set the reference count to one as we "virtually" pass back
     * a reference to the caller.
     */
    socket->proto.udp.ref_count = 1;
    spinlock_init(&socket->proto.udp.ref_count_lock);
    /*
     * Init lock
     */
    spinlock_init(&socket->lock);
    return 0;
}

/*
 * Set local address for a socket. This function sets the IP source
 * address based on the route to a specified destination address and
 * chooses a free local port
 * Parameter:
 * @socket - the socket
 * @ip_dst - IP destination address used to determine the proper source address
 * Return value:
 * 0 if the operation was successful
 * -ENETUNREACH if the destination is not reachable
 * Locks:
 * lock on socket list
 */
static int set_local_address(socket_t* socket, u32 ip_dst) {
    u32 ip_src;
    u32 eflags;
    int port;
    struct sockaddr_in* laddr;
    /*
     * Ask IP module for proper source address
     */
    if (INADDR_ANY == (ip_src = ip_get_src_addr(ip_dst))) {
        return -ENETUNREACH;
    }
    /*
     * Get lock on socket list
     */
    spinlock_get(&socket_list_lock, &eflags);
    /*
     * Set local IP address
     */
    laddr = (struct sockaddr_in*) &socket->laddr;
    laddr->sin_addr.s_addr = ip_src;
    laddr->sin_family = AF_INET;
    /*
     * Determine a free local port number
     */
    if (-1 == (port = find_free_port())) {
        spinlock_release(&socket_list_lock, &eflags);
        return -EADDRINUSE;
    }
    /*
     * and assign it
     */
    laddr->sin_port = htons(port);
    /*
     * Add socket to list
     */
    LIST_ADD_END(socket_list_head, socket_list_tail, SOCK2UCB(socket));
    /*
     * Release lock
     */
    spinlock_release(&socket_list_lock, &eflags);
    return 0;
}

/*
 * Bind socket to local address
 * Parameter:
 * @socket - the socket
 * @address - the local address
 * @addrlen - size of local address
 * Return value:
 * 0 upon success
 * -EINVAL if the address length is not as expected (sizeof(struct sockaddr_in))
 * -EINVAL if address is 0
 * -EINVAL if the socket is already bound
 * -EAFNOSUPPORT if the address family is not AF_INET
 * -EADDRNOTAVAIL if this is not a valid local address supported by one of the NICs
 * -EADDRINUSE if no free local port could be found or the specified address is in use
 * Locks:
 * lock on socket list
 */
static int udp_bind(socket_t* socket, struct sockaddr* address, int addrlen) {
    u32 eflags;
    struct sockaddr_in* laddr;
    struct sockaddr_in* socket_addr;
    udp_socket_t* other;
    int port;
    /*
     * If address length is not valid, return
     */
    if (addrlen != sizeof(struct sockaddr_in)) {
        return -EINVAL;
    }
    laddr = (struct sockaddr_in*) address;
    socket_addr = (struct sockaddr_in*) &(socket->laddr);
    if (0 == laddr)
        return -EINVAL;
    if (laddr->sin_family != AF_INET) {
        return -EAFNOSUPPORT;
    }
    /*
     * If socket is already bound, return
     */
    if (socket->bound) {
        return -EINVAL;
    }
    /*
     * Determine MTU to validate local address
     */
    if (-1 == ip_get_mtu(laddr->sin_addr.s_addr)) {
        return -EADDRNOTAVAIL;
    }
    /*
     * Get lock on socket list
     */
    spinlock_get(&socket_list_lock, &eflags);
    /*
     * If specified port number is zero, select ephemeral port
     */
    port = ntohs(laddr->sin_port);
    if (0 == port) {
        port = find_free_port();
        if (-1 == port) {
            spinlock_release(&socket_list_lock, &eflags);
            return -EADDRINUSE;
        }
    }
    /*
     * Check whether address is already in use.
     */
    else {
        NET_DEBUG("Checking whether address is in use\n");
        other = get_matching_ucb(laddr->sin_addr.s_addr, INADDR_ANY, ntohs(port), 0);
        if (other)  {
            spinlock_release(&socket_list_lock, &eflags);
            return -EADDRINUSE;
        }
    }
    socket_addr->sin_port = htons(port);
    socket_addr->sin_addr.s_addr = laddr->sin_addr.s_addr;
    socket_addr->sin_family = AF_INET;
    socket->bound = 1;
    /*
     * Add socket to list
     */
    LIST_ADD_END(socket_list_head, socket_list_tail, &socket->proto.udp);
    /*
     * Release lock on socket list
     */
    spinlock_release(&socket_list_lock, &eflags);
    return 0;
}

/*
 * Connect a UDP socket
 * Parameter:
 * @socket - the socket
 * @addr - the foreign address
 * @addrlen - length of address argument in bytes
 * Return value:
 * 0 upon success
 * -EINVAL if the address length is not sizeof(struct sockaddr_in)
 * -ENOBUFS if the socket could not be added to the list of raw IP sockets
 * -ENETUNREACH if the target network is not reachable
 * Locks:
 * lock on socket list
 */
static int udp_connect_socket(socket_t* socket, struct sockaddr* addr, int addrlen) {
    u32 eflags;
    struct sockaddr_in* faddr;
    /*
     * Verify length of address argument
     */
    if (sizeof(struct sockaddr_in) != addrlen)
        return -EINVAL;
    /*
     * Set local address if the socket is not yet bound. If no
     * local address can be determined because there is no route to the destination, return -ENETUNREACH
     */
    faddr = (struct sockaddr_in*) addr;
    if (0 == socket->bound) {
        if (set_local_address(socket, faddr->sin_addr.s_addr))
            return -ENETUNREACH;
        socket->bound = 1;
    }
    /*
     * Set foreign address - we need to get the lock on the socket list for this
     * to avoid races with the multiplexing code
     */
    spinlock_get(&socket_list_lock, &eflags);
    socket->faddr = *addr;
    socket->connected = 1;
    spinlock_release(&socket_list_lock, &eflags);
    /*
     * Trigger waiting threads
     */
   net_post_event(socket, NET_EVENT_CAN_WRITE);
   return 0;
}


/*
 * Close a UDP socket
 * Parameter:
 * @socket - the socket
 * @eflags - EFLAGS argument used by caller when acquiring the lock on the socket
 * Return value:
 * 0 upon success
 */
static int udp_close_socket(socket_t* socket, u32* eflags) {
    udp_socket_t* ucb = SOCK2UCB(socket);
    net_msg_t* net_msg;
    /*
     * Remove socket from list used for multiplexing
     */
    unregister_socket(ucb);
    /*
     * Release all network messages in the receive queue
     */
    while (ucb->rcv_buffer_head) {
        net_msg = ucb->rcv_buffer_head;
        LIST_REMOVE_FRONT(ucb->rcv_buffer_head, ucb->rcv_buffer_tail);
        net_msg_destroy(net_msg);
    }
    return 0;
}


/*
 * Listen on a UDP socket. This function does nothing as the operation
 * is meaningless for a UPD socket
 * Parameter:
 * @socket - the socket
 */
static int udp_listen(socket_t* socket) {
    return 0;
}



/*
 * Given a socket, check the socket state and return either 0 or a combination of
 * the bitmasks 0x1 (can read) and 0x2 (can write), depending on the current state
 * of the socket. We assume that caller holds the lock.
 * Parameter:
 * @socket - the socket
 * @read - check whether socket is ready to read
 * @write - check whether socket is ready to write
 * Note that we do not check whether the socket is connected, as we consider a socket "ready
 * for reading / writing" if the respective system call would not block, regardless of whether
 * the transfer would succeed
 */
static int udp_select(socket_t* socket, int read, int write) {
    int rc = 0;
    udp_socket_t* ucb = &socket->proto.udp;
    /*
     * If checking for read is requested, check whether there is any data
     * in the receive queue
     */
    if (read) {
        if (ucb->pending_bytes > 0) {
            rc += NET_EVENT_CAN_READ;
        }
    }
    /*
     * As the write operation does never block, return TRUE in any case
     */
    if (write) {
        rc += NET_EVENT_CAN_WRITE;
    }
    return rc;
}

/*
 * Compute UDP checksum
 * Parameter:
 * @words - pointer to IP payload, in network byte order
 * @byte_counts - number of bytes
 * @ip_src - IP source address, in network byte order
 * @ip_dst - IP destination address, in network byte order
 * Result:
 * UDP checksum, in host byte order
 */
static u16 compute_checksum(u16* words, u16 byte_count,  u32 ip_src, u32 ip_dst) {
    u32 sum = 0;
    u16 rc;
    int i;
    u16 last_byte = 0;
    /*
     * First add all fields in the 12 byte pseudo-header:
     * 4 byte bit source IP address
     * 4 byte bit destination IP address
     * 1 byte padding
     * 1 byte IP protocol ( 17 = UDP)
     * 2 bytes UDP segment length (including header)
     * Instead of converting all fields to host byte order before adding them,
     * we add up everything in network byte order and then convert the result
     * This will give the same checksum (see RFC 1071), but will be faster
     */
    sum = IPPROTO_UDP*256 + htons(byte_count);
    sum = sum + ((ip_src >> 16) & 0xFFFF) + (ip_src & 0xFFFF);
    sum = sum + ((ip_dst >> 16) & 0xFFFF) + (ip_dst & 0xFFFF);
    /*
     * Sum up all other words
     */
    for (i = 0; i < byte_count / 2; i++) {
        sum = sum + words[i];
    }
    /*
     * If the number of bytes is odd, add left over byte << 8
     */
    if (1 == (byte_count % 2)) {
        last_byte = ((u8*) words)[byte_count - 1];
        sum = sum + last_byte;
    }
    /*
     * Repeatedly add carry to LSB until carry is zero
     */
    while (sum >> 16)
        sum = (sum >> 16) + (sum & 0xFFFF);
    rc = sum;
    rc = htons(~rc);
    return rc;
}

/* Send data via a UDP socket. If the addr argument is NULL and the socket is connected,
 * the foreign address of the socket is used as destination address, otherwise the specified
 * address @addr is used. If the socket is already connected and addr is not NULL, an error
 * is raised
 * Parameter:
 * @socket - the socket
 * @buffer - the buffer containing the message payload
 * @len - length of IP payload
 * @flags - flags, currently ignored
 * @addr - destination address, may be NULL
 * @addrlen - size of destination address
 * Return values:
 * -EMSGSIZE if the maximum size of a message is exceeded
 * -ENOMEM if no memory could be allocated for a new network message
 * -ENETUNREACH if the network is not reachable (no route found)
 * -ENOTCONN if socket is not connected and addr is NULL
 * -EISCONN if a destination address is specified but the socket is connected
 * -EINVAL if the addrlen field is not valid
 * Locks: lock on socket list
* */
static int udp_sendto(socket_t* socket, void* buffer, unsigned int len, int flags, struct sockaddr* addr, u32 addrlen) {
    u32 eflags;
    net_msg_t* net_msg;
    unsigned char* data;
    struct sockaddr_in* laddr;
    struct sockaddr_in* faddr;
    udp_hdr_t* udp_hdr;
    u16 chksum;
    u16 src_port;
    /*
     * Return if addrlen is not as expected
     */
    if ((sizeof(struct sockaddr_in) != addrlen) && (addr))
        return -EINVAL;
    /*
     * If addr is zero, socket needs to be connected
     */
    if ((0 == addr) && (0 == socket->connected))
        return -ENOTCONN;
    /*
     * If socket is connected, no address should be specified
     */
    if ((1 == socket->connected) && (addr))
        return -EISCONN;
    /*
     * Make sure that maximum length of IP payload is not exceeded
     */
    if (len > IP_FRAGMENT_MAX_SIZE - sizeof(udp_hdr_t)) {
        return -EMSGSIZE;
    }
    /*
     * Create a new network message
     */
    if (0 == (net_msg = net_msg_new(len + sizeof(udp_hdr_t)))) {
        return -ENOMEM;
    }
    /*
     * Set fields required by ip_tx_msg.
     * Target address:  we use the target address specified in the address argument if
     * that argument is not null and the address in the socket otherwise
     * Local address: if the socket is bound to a non-zero address, we use this address, otherwise
     * we ask the IP layer to determine a route for us (we cannot simply put 0 here to make this work
     * automatically as we need the IP source address to compute the checksum
     *
     */
    laddr = (struct sockaddr_in*) &socket->laddr;
    if (addr)
        faddr = (struct sockaddr_in*) addr;
    else
        faddr = (struct sockaddr_in*) &socket->faddr;
    net_msg->ip_dest = faddr->sin_addr.s_addr;
    if (socket->bound) {
        src_port = laddr->sin_port;
        net_msg->ip_src = laddr->sin_addr.s_addr;
    }
    else {
        net_msg->ip_src = INADDR_ANY;
        src_port = 0;
    }
    /*
     * If IP address is wildcard, determine outgoing interface
     */
    if (INADDR_ANY == net_msg->ip_src) {
        net_msg->ip_src = ip_get_src_addr(net_msg->ip_dest);
        if (INADDR_ANY == net_msg->ip_src) {
            net_msg_destroy(net_msg);
            return -ENETUNREACH;
        }
    }
    /*
     * If source port is 0, use free port
     */
    if (0 == src_port) {
        spinlock_get(&socket_list_lock, &eflags);
        src_port = find_free_port();
        spinlock_release(&socket_list_lock, &eflags);
        if (-1 == src_port) {
            return -EADDRINUSE;
        }
        else {
            src_port = ntohs(src_port);
        }
    }
    net_msg->ip_df = 0;
    net_msg->ip_proto = IPPROTO_UDP;
    /*
     * Create UDP header
     */
    if (0 == (udp_hdr = (udp_hdr_t*) net_msg_append(net_msg, sizeof(udp_hdr_t)))) {
        return -ENOMEM;
    }
    udp_hdr->dst_port = faddr->sin_port;
    udp_hdr->src_port = src_port;
    udp_hdr->chksum = 0;
    udp_hdr->length = ntohs(len + sizeof(udp_hdr_t));
    NET_DEBUG("UDP length: %d\n", ntohs(udp_hdr->length));
    /*
     * Copy data to net message
     */
    data = net_msg_append(net_msg, len);
    KASSERT(data);
    memcpy((void*) data, (void*) buffer, len);
    /*
     * Now compute checksum
     */
    chksum = compute_checksum((u16*) udp_hdr, len + sizeof(udp_hdr_t), net_msg->ip_src, net_msg->ip_dest);
    /*
     * If chksum is 0, map to 0xFFFF (note that chksum can never be 0xFFFF)
     */
    if (0 == chksum)
        udp_hdr->chksum = 0xFFFF;
    else
        udp_hdr->chksum = htons(chksum);
    /*
     * Finally hand message over to IP layer
     */
    ip_tx_msg(net_msg);
    return len;
}

/*
 * Send data via a UDP socket
 * Parameter:
 * @socket - the socket
 * @buffer - the buffer containing the message payload
 * @len - length of IP payload
 * @flags - flags, currently ignored
 * Return values:
 * -EMSGSIZE if the maximum size of a message is exceeded
 * -ENOMEM if no memory could be allocated for a new network message
 * -ENETUNREACH if the network is not reachable (no route found)
 * -ENOTCONN if socket is not connected
 * number of bytes transmitted otherwise
 */
static int udp_send(socket_t* socket, void* buffer, unsigned int len, int flags) {
    return udp_sendto(socket, buffer, len, flags, 0, 0);
}



/*
 * Read from a UDP socket and place the peers address in a
 * provided buffer of type struct sockaddr. The field addrlen is updated
 * with the size of the address. If initially *addrlen is less than the size
 * of the address, the address is being truncated
 * Parameter:
 * @socket - socket to read from
 * @buffer - buffer
 * @len - number of bytes we read at most
 * @flags - flags to apply
 * @addr - pointer to socket address
 * @len - pointer to address length
 * Return value:
 * number of bytes read upon success
 * -EINVAL if one of the arguments is not valid or the socket is not bound
 * -EAGAIN if the buffer is empty
 */
static int udp_recvfrom(socket_t* socket, void* buffer, unsigned int len, int flags, struct sockaddr* addr, u32* addrlen) {
    unsigned int bytes;
    struct sockaddr_in faddr;
    unsigned char* data;
    int i;
    net_msg_t* item;
    udp_socket_t* ucb = &socket->proto.udp;
    if (0 == socket)
        return -EINVAL;
    /*
     * Socket needs to be bound, otherwise we would wait indefinitely
     */
    if (0 == socket->bound)
        return -EINVAL;
    /*
     * If address is specified verify address length
     */
    if (addr) {
        if (0 == addrlen)
            return -EINVAL;
    }
    /*
     *
     * Generic layer has locked socket for us. So we can safely check how many bytes we have in
     * the buffer and update the buffer
     * If there is no data in the sockets receive queue, return -EAGAIN
     */
    if (0 == ucb->rcv_buffer_head) {
        NET_DEBUG("No data in receive buffer\n");
        return -EAGAIN;
    }
    /*
     * If there are messages in the queue, copy first message to buffer
     */
    item = ucb->rcv_buffer_head;
    bytes = MIN(item->ip_length - sizeof(udp_hdr_t), len);
    /*
     * and copy data
     */
    data = (unsigned char*) (item->udp_hdr + sizeof(udp_hdr_t));
    for (i = 0; i < bytes; i++) {
        ((u8*) buffer)[i] = data[i];
        ucb->pending_bytes--;
    }
    /*
     * If addr is specified, copy peer address there
     */
    if (addr) {
        faddr.sin_family = AF_INET;
        faddr.sin_addr.s_addr = item->ip_src;
        faddr.sin_port = ((udp_hdr_t*) item->udp_hdr)->src_port;
        memcpy((void*) addr, (void*) &faddr, MIN(*addrlen, sizeof(struct sockaddr_in)));
        *addrlen = sizeof(struct sockaddr_in);
    }
    /*
     * Now remove entry from queue
     */
    LIST_REMOVE(ucb->rcv_buffer_head, ucb->rcv_buffer_tail, item);
    /*
     * and free item
     */
    net_msg_destroy(item);
    return bytes;
}


/*
 * Read from a UDP socket
 * Parameter:
 * @socket - socket to read from
 * @buffer - buffer
 * @len - number of bytes we read at most
 * @flags - flags to apply
 * Return value:
 * number of bytes read upon success
 * -EINVAL if one of the arguments is not valid or the socket is not bound
 * -EAGAIN if the buffer is empty
 */
static int udp_recv(socket_t* socket, void* buffer, unsigned int len, int flags) {
    return udp_recvfrom(socket, buffer, len, flags, 0, 0);
}

/****************************************************************************************
 * UDP input processing                                                                 *
 ***************************************************************************************/

/*
 * Store a network message in a sockets receive queue
 * Parameter:
 * @ucb - pointer to socket
 * @net_msg - network message
 * Locks:
 * lock on socket
 */
static void store_msg(udp_socket_t* ucb, net_msg_t* net_msg) {
    u32 eflags;
    socket_t* socket = UCB2SOCK(ucb);
    if (0 == socket)
        return;
    /*
     * Get lock on socket
     */
    spinlock_get(&socket->lock, &eflags);
    /*
     * Make sure that buffer does not exceed a certain maximum size - if this happens
     * discard message
     */
    if ((ucb->pending_bytes + net_msg->ip_length - sizeof(udp_hdr_t)) > UDP_RECVBUFFER_SIZE) {
        spinlock_release(&socket->lock, &eflags);
        return;
    }
    /*
     * Increase pending bytes
     */
    ucb->pending_bytes += net_msg->ip_length - sizeof(udp_hdr_t);
    /*
     * and add network message to queue. This works as we only add the message
     * to one queue. If we ever implement multicast, we need to place a copy and free
     * the copy again in udp_recv
     */
    LIST_ADD_END(ucb->rcv_buffer_head, ucb->rcv_buffer_tail, net_msg);
    /*
     * Inform waiting threads
     */
    net_post_event(UCB2SOCK(ucb), NET_EVENT_CAN_READ);
    /*
     * Release spinlock
     */
    spinlock_release(&socket->lock, &eflags);
}

/*
 * Process an incoming UDP message
 * Parameter:
 * @net_msg - the message
 * Locks:
 * lock on socket list
 * This function assumes that the following fields in the network message have been set by
 * the IP layer:
 * net_msg->udp_hdr
 * net_msg->ip_src
 * net_msg->ip_dest
 * net_msg->ip_length
 */
void udp_rx_msg(net_msg_t* net_msg) {
    u32 eflags;
    u16 chksum;
    udp_hdr_t* udp_hdr;
    u16 udp_length;
    u32 ip_src;
    u32 ip_dest;
    u16 dest_port;
    u16 src_port;
    udp_socket_t* ucb;
    if (0 == net_msg)
        return;
    /*
     * Get pointer to UDP header
     */
    udp_hdr = (udp_hdr_t*) net_msg->udp_hdr;
    if (0 == udp_hdr) {
        net_msg_destroy(net_msg);
        return;
    }
    /*
     * Determine UDP length, IP source address and IP target address as well
     * as source and target ports
     */
    ip_src = net_msg->ip_src;
    ip_dest = net_msg->ip_dest;
    src_port = udp_hdr->src_port;
    dest_port = udp_hdr->dst_port;
    udp_length = ntohs(udp_hdr->length);
    /*
     * Validate length against IP header information
     */
    if (udp_length != net_msg->ip_length) {
        NET_DEBUG("UPD length (%d) does not match IP payload length (%d)\n", udp_length, net_msg->ip_length);
        net_msg_destroy(net_msg);
        return;
    }
    /*
     * Validate checksum. If checksum is 0, this is an indication that the sender has
     * not computed the checksum, skip check in this case. Note that the special case that the
     * checksum is 0xFFFF is not considered here - this only happens when on the senders side,
     * the result of the checksum computation was zero, but is not a special case on the receiver
     * side as 0xFFFF and 0x0000 are equivalent in one's complement arithmetic
     */
    chksum = htons(udp_hdr->chksum);
    if (chksum) {
        if (compute_checksum((u16*) udp_hdr, udp_length, ip_src, ip_dest )) {
            NET_DEBUG("Invalid checksum\n");
            net_msg_destroy(net_msg);
            return;
        }
    }
    /*
     * Now locate UDP socket for which this packet is destined
     */
    spinlock_get(&socket_list_lock, &eflags);
    ucb = get_matching_ucb(ip_dest, ip_src, dest_port, src_port);
    if (ucb)
        clone_socket(UCB2SOCK(ucb));
    spinlock_release(&socket_list_lock, &eflags);
    if (0 == ucb) {
        NET_DEBUG("No matching port\n");
        /*
         * Send ICMP message "port unreachable"
         */
        icmp_send_error(net_msg, ICMP_CODE_PORT_UNREACH, ICMP_DEST_UNREACH);
        net_msg_destroy(net_msg);
        return;
    }
    /*
     * and copy data to socket, thus passing the reference to the
     * network message to the socket
     */
    store_msg(ucb, net_msg);
    /*
     * Release reference to socket. We do not free the message as we have
     * handed it over to the socket
     */
    udp_release_socket(UCB2SOCK(ucb));
}

