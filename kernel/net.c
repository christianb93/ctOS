/*
 * net.c
 *
 * This module contains various utility functions used throughout the networking stack. Most of these functions are concerned with the
 * handling of network messages (instances of net_msg_t) which encapsulate received packets as they travel the networking stack upwards
 * and packets to be transmitted which travel the stack downwards.
 *
 * The following table lists the fields within a network message (other than those which manage the data buffer) and the layer within
 * the networking stack which is responsible to fill them, depending on the direction of the message. An n/a indicates that the field
 * is only set when the message goes in the other direction. The table also specifies which field is stored in network byte order
 *
 * Field        Responsible layer if msg goes up                      Responsible layer if msg does down         Byte order
 * --------------------------------------------------------------------------------------------------------------------------
 * nic          Device driver layer                                   IP layer (ip.c / ip_tx_msg)                n/a
 * eth_hdr      Network interface layer (net_if_multiplex_msg)        n/a                                        n/a
 * arp_hdr      Network interface layer (net_if_multiplex_msg)        n/a                                        n/a
 * ip_hdr       Network interface layer (net_if_multiplex_msg)        IP layer (ip.c / ip_tx_msg)                n/a
 * icmp_hdr     IP layer (ip_rx_msg)                                  n/a                                        n/a
 * tcp_hdr      IP layer (ip_rx_msg)                                  n/a                                        n/a
 * udp_hdr      IP layer (ip_rx_msg)                                  n/a                                        n/a
 * hw_dest      n/a                                                   IP layer (ip_tx_msg) + ARP layer           n/a
 * ethertype    n/a                                                   IP layer (ip_tx_msg) + ARP layer           network
 * ip_length    IP layer (ip_rx_msg)                                  Transport layer                            host
 * ip_proto     n/a                                                   Transport layer                            n/a
 * ip_dest      IP layer                                              Transport layer                            network
 * ip_src       IP layer                                              Transport layer                            network
 *
 *
 * Note that the fields set by one layer are not necessarily available to all other layers, but are only valid for the layer
 * directly above the originating layer. As an example, the fields
 *   nic
 *   eth_hdr
 *   arp_hdr
 *   ip_hdr
 * are set for a network message passed by the network interface layer to the IP layer, but not necessarily for a message passed
 * by the IP layer to the TCP layer, as this message might be the result of IP reassembly. This is just a result of the general fact
 * that a TCP layer should not the assumption that the messages it receives originate from an Ethernet network
 */



#include "net.h"
#include "mm.h"
#include "pm.h"
#include "lib/string.h"
#include "debug.h"
#include "lib/stdlib.h"
#include "lib/stdint.h"
#include "lib/arpa/inet.h"
#include "lib/netinet/in.h"
#include "arp.h"
#include "util.h"
#include "net_if.h"
#include "params.h"
#include "ip.h"
#include "locks.h"
#include "lib/os/errors.h"
#include "tcp.h"
#include "udp.h"
#include "lib/os/signals.h"
#include "lists.h"
#include "lib/sys/ioctl.h"
#include "lib/fcntl.h"
#include "timer.h"

/*
 * This is the common loglevel for all network modules above the drivers
 */
int __net_loglevel = 0;
/*
 * Loglevel for Ethernet driver
 */
int __eth_loglevel = 0;

static char* __module = "NET   ";

/*
 * This is a list of known DNS servers. Even though ctOS implements DNS resolution in user space,
 * the kernel keeps a registry of DNS servers which can be inquired and changed by user space
 * applications
 */
static unsigned int dns_servers[MAX_DNS_SERVERS];

/*
 * These counters contain the number of allocate and destroyed network messages and are used
 * to detect memory leaks
 */
static u32 net_msg_created = 0;
static u32 net_msg_destroyed = 0;

#define NET_DEBUG(...) do {if (__net_loglevel > 0 ) { kprintf("DEBUG at %s@%d (%s): ", __FILE__, __LINE__, __FUNCTION__); \
        kprintf(__VA_ARGS__); }} while (0)

/****************************************************************************************
 * These functions are used to work with network messages as they are passed through    *
 * the network stack                                                                    *
 ***************************************************************************************/

/*
 * Allocate a network message
 * Parameter:
 * @size - the size of the network message buffer (including headroom)
 * @headroom - the initial headroom
 * Return value:
 * 0 if no message could be created due to insufficient memory
 * a pointer to the newly created message otherwise
 */
net_msg_t* net_msg_create(u32 size, u32 headroom) {
    net_msg_t* net_msg = 0;
    if (0 == (net_msg = (net_msg_t*) kmalloc(sizeof(net_msg_t))))
        return 0;
    if (0 == (net_msg->data = (u8*) kmalloc(size))) {
        kfree((void*) net_msg);
        return 0;
    }
    net_msg->start = net_msg->data + MIN(headroom, size);
    net_msg->end = net_msg->start;
    net_msg->nic = 0;
    net_msg->length = size;
    atomic_incr(&net_msg_created);
    return net_msg;
}

/*
 * Allocate a network message
 * Parameter:
 * @size - the size of the network message buffer (not including headroom)
 * Return value:
 * 0 if no message could be created due to insufficient memory
 * a pointer to the newly created message otherwise
 * This function will create a message which has enough headroom for
 * an Ethernet header and an IP header
 */
net_msg_t* net_msg_new(u32 size) {
    net_msg_t* net_msg = 0;
    if (0 == (net_msg = (net_msg_t*) kmalloc(sizeof(net_msg_t))))
        return 0;
    if (0 == (net_msg->data = (u8*) kmalloc(size + NET_MIN_HEADROOM))) {
        kfree((void*) net_msg);
        return 0;
    }
    net_msg->start = net_msg->data + NET_MIN_HEADROOM;
    net_msg->end = net_msg->start;
    net_msg->nic = 0;
    net_msg->length = size + NET_MIN_HEADROOM;
    atomic_incr(&net_msg_created);
    return net_msg;
}

/*
 * Clone a network message, i.e. create a new network message with the same
 * metadata and data
 * Parameter:
 * @net_msg - the original message
 * Return value:
 * the new network message or 0 if there was no free memory
 */
net_msg_t* net_msg_clone(net_msg_t* net_msg) {
    net_msg_t* clone = (net_msg_t*) kmalloc(sizeof(net_msg_t));
    if (0 == clone) {
        return 0;
    }
    memcpy((void*) clone, (void*) net_msg, sizeof(net_msg_t));
    if (0 == (clone->data = (u8*) kmalloc(net_msg->length))) {
        kfree((void*) clone);
        return 0;
    }
    clone->start = clone->data + (net_msg->start - net_msg->data);
    clone->end = clone->start + net_msg_get_size(net_msg);
    clone->arp_hdr = clone->data + (net_msg->arp_hdr - (void*) net_msg->data);
    clone->eth_hdr = clone->data + (net_msg->eth_hdr - (void*) net_msg->data);
    clone->icmp_hdr = clone->data + (net_msg->icmp_hdr - (void*) net_msg->data);
    clone->ip_hdr = clone->data + (net_msg->ip_hdr - (void*) net_msg->data);
    clone->tcp_hdr = clone->data + (net_msg->tcp_hdr - (void*) net_msg->data);
    memcpy((void*) clone->data, (void*) net_msg->data, net_msg->length);
    atomic_incr(&net_msg_created);
    return clone;
}

/*
 * Destroy a network message again and free its memory
 * Parameter:
 * @net_msg - the network message
 */
void net_msg_destroy(net_msg_t* net_msg) {
    if (0 == net_msg)
        return;
    if (net_msg->data) {
        kfree((void*) net_msg->data);
        net_msg->data = 0;
    }
    kfree((void*) net_msg);
    atomic_incr(&net_msg_destroyed);
}

/*
 * Append free space at the end of a network message and
 * return a pointer to the first byte of this space
 * Parameter:
 * @net_msg - the message
 * @size - number of bytes to append
 * Return value:
 * 0 if there is not enough space left
 * pointer to first byte of free space otherwise
 */
u8* net_msg_append(net_msg_t* net_msg, u32 size) {
    u8* rc;
    /*
     * Is there enough space left?
     */
    if (size > net_msg->length)
        return 0;
    if ((net_msg->end - net_msg->data) > net_msg->length - size)
        return 0;
    rc = net_msg->end;
    net_msg->end += size;
    return rc;
}

/*
 * Create free space at the beginning of a network message
 * and return pointer to first free byte
 * Parameter:
 * @net_msg - the message
 * @size - the number of bytes to be made available
 */
u8* net_msg_prepend(net_msg_t* net_msg, u32 size) {
    /*
     * Is there enough headroom?
     */
    if (net_msg->start - net_msg->data < size)
        return 0;
    net_msg->start -= size;
    return net_msg->start;
}

/*
 * Cut off a network message at offset @offset
 * Parameter:
 * @offset - the offset, i.e. last byte which will survive (starting at end of headroom)
 */
void net_msg_cut_off(net_msg_t* net_msg, u32 offset) {
    net_msg->end = net_msg->start + offset;
}

/*
 * Set Ethernet header pointer
 * Parameter:
 * @net_msg - the message
 * @offset - offset of Ethernet header with respect to start of message
 */
void net_msg_set_eth_hdr(net_msg_t* net_msg, u32 offset) {
    net_msg->eth_hdr = net_msg->start + offset;
}

/*
 * Set ARP header pointer
 * Parameter:
 * @net_msg - the message
 * @offset - offset of ARP header with respect to the Ethernet header
 */
void net_msg_set_arp_hdr(net_msg_t* net_msg, u32 offset) {
    net_msg->arp_hdr = net_msg->eth_hdr + offset;
}

/*
 * Set IP header pointer
 * Parameter:
 * @net_msg - the message
 * @offset - offset of IP header with respect to the Ethernet header
 */
void net_msg_set_ip_hdr(net_msg_t* net_msg, u32 offset) {
    net_msg->ip_hdr = net_msg->eth_hdr + offset;
}

/*
 * Set ICMP header pointer
 * Parameter:
 * @net_msg - the message
 * @offset - offset of ICMP header with respect to the IP header
 */
void net_msg_set_icmp_hdr(net_msg_t* net_msg, u32 offset) {
    net_msg->icmp_hdr = net_msg->ip_hdr + offset;
}

/*
 * Set TCP header pointer
 * Parameter:
 * @net_msg - the message
 * @offset - offset of TCP header with respect to the IP header
 */
void net_msg_set_tcp_hdr(net_msg_t* net_msg, u32 offset) {
    net_msg->tcp_hdr = net_msg->ip_hdr + offset;
}

/*
 * Set UDP header pointer
 * Parameter:
 * @net_msg - the message
 * @offset - offset of UDP header with respect to the IP header
 */
void net_msg_set_udp_hdr(net_msg_t* net_msg, u32 offset) {
    net_msg->udp_hdr = net_msg->ip_hdr + offset;
}


/*
 * Return the number of bytes actually in use in the
 * network message
 * Parameter:
 * @net_msg - the network message
 */
u32 net_msg_get_size(net_msg_t* net_msg) {
    return net_msg->end - net_msg->start;
}

/*
 * Return a pointer to the first used byte
 * of a network message
 * Parameter:
 * @net_msg - the network message
 */
u8* net_msg_get_start(net_msg_t* net_msg) {
    return net_msg->start;
}

/****************************************************************************************
 * Some utility functions to work with IP addresses and IP packets. Note that           *
 * internally, IP addresses are still stored in network byte order                      *
 ***************************************************************************************/

/*
 * Print an IP address (in network byte order) using kprintf
 * Parameter:
 * @ip_address - the address to be printed
 */
void net_print_ip(u32 ip_address) {
       kprintf("%d.%d.%d.%d", ip_address & 0xFF, (ip_address >> 8) & 0xFF, (ip_address >> 16) & 0xFF, (ip_address >> 24) & 0xFF);
}

/*
 * Given an IP address in the usual notation, return the corresponding IP address
 * in network byte order (this is just a wrapper around inet_addr, but is there for
 * historical reasons)
 * Parameter:
 * @ip_address - the IP address
 */
u32 net_str2ip(char* ip_address) {
    return inet_addr((const char*) ip_address);
}

/****************************************************************************************
 * Utility functions to compute checksums                                               *
 ***************************************************************************************/

/*
 * Compute the IP checksum of a word array. The elements within the
 * array are assumed to be stored in network byte order. This could probably
 * be optimized a lot...
 * Parameter:
 * @words - pointer to start of word array
 * @byte_count - number of bytes (!) in the array
 */
u16 net_compute_checksum(u16* words, int byte_count) {
    u32 sum = 0;
    u16 rc;
    int i;
    u16 last_byte = 0;
    /*
     * First sum up all words. We do all the sums in network byte order
     * and only convert the result
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
    rc = ntohs(~rc);
    return rc;
}

/****************************************************************************************
 * Initialization of the entire networking stack                                        *
 ***************************************************************************************/

/*
 * Initialize networking stack. As this will spawn threads, it needs to be done once
 * interrupts have been enabled
 */
void net_init() {
    /*
     * Reset counters
     */
    net_msg_created = 0;
    net_msg_destroyed = 0;
    /*
     * and DNS servers
     */
    memset((void*) dns_servers, 0, sizeof(u32)*MAX_DNS_SERVERS);
    /*
     * Set loglevel to 1 if requested
     */
    if (1 == params_get_int("net_loglevel")) {
        MSG("Turning on logging for network stack\n");
        __net_loglevel = 1;
    }
    else
        __net_loglevel = 0;
    if (1 == params_get_int("eth_loglevel")) {
        MSG("Turning on logging for Ethernet devices\n");
        __eth_loglevel = 1;
    }
    else
        __eth_loglevel = 0;
    /*
     * Initialize network interface layer
     */
    net_if_init();
    /*
     * Initialize ARP layer
     */
    arp_init();
    /*
     * IP layer
     */
    ip_init();
    /*
     * and UDP / TCP layer
     */
    udp_init();
    tcp_init();
}

/****************************************************************************************
 * These functions are a generic socket layer which invokes protocol specific functions *
 * if needed                                                                            *
 ***************************************************************************************/

/*
 * Close a socket
 * Parameter:
 * @socket - the socket to be closed
 * Locks:
 * lock on socket
 */
void net_socket_close(socket_t* socket) {
    u32 eflags;
    if (socket) {
        /*
         * Get lock
         */
        spinlock_get(&socket->lock, &eflags);
        /*
         * Call protocol specific close function.
         */
        if (socket->ops) {
            if (socket->ops->close) {
                socket->ops->close(socket, &eflags);
            }
        }
        /*
         * Release lock again
         */
        spinlock_release(&socket->lock, &eflags);
        /*
         * finally drop our reference to socket
         */
        socket->ops->release(socket);
    }
}

/*
 * Create a socket
 * Parameter:
 * @domain - address family
 * @type - type (stream, datagram, raw)
 * @proto - protocol
 */
socket_t* net_socket_create(int domain, int type, int proto) {
    socket_t* res = 0;
    int rc;
    /*
     * Validate parameters
     */
    switch (domain) {
        case AF_INET:
            switch (type) {
                case SOCK_RAW:
                    break;
                case SOCK_STREAM:
                    break;
                case SOCK_DGRAM:
                    break;
                default:
                    NET_DEBUG("Invalid socket type %d\n", type);
                    return 0;

            }
            break;
        default:
            NET_DEBUG("Invalid domain %d\n", domain);
            return 0;
    }
    /*
     * Now allocate memory for socket and initialize fields
     */
    if (0 == (res = (socket_t*) kmalloc(sizeof(socket_t))))
        return 0;
    /*
     * Zero all fields to be on the save side
     */
    memset((void*) res, 0, sizeof(socket_t));
    /*
     * Call protocol specific initialization routine which is responsible
     * for filling the ops structure and for initializing protocol specific fields
     * in the union socket->proto - this will also zero all fields, so we do our initialization
     * afterwards
     */
    res->ops = 0;
    if ((AF_INET == domain) && (SOCK_RAW == type)) {
        NET_DEBUG("Creating IP socket\n");
        rc = ip_create_socket(res, domain, proto);
    }
    if ((AF_INET == domain) && (SOCK_STREAM == type)) {
        NET_DEBUG("Creating TCP socket\n");
        rc = tcp_create_socket(res, domain, proto);
    }
    if ((AF_INET == domain) && (SOCK_DGRAM == type)) {
        NET_DEBUG("Creating UDP socket\n");
        rc = udp_create_socket(res, domain, proto);
    }
    if (rc < 0) {
        return 0;
    }
    /*
     * Do remaining initialization
     */
    spinlock_init(&res->lock);
    cond_init(&res->snd_buffer_change);
    cond_init(&res->rcv_buffer_change);
    res->so_queue_head = 0;
    res->so_queue_tail = 0;
    res->select_queue_head = 0;
    res->select_queue_tail = 0;
    return res;
}

/*
 * Connect a socket
 * Parameter:
 * @socket - the socket to be connected
 * @addr - the foreign address to which we connect
 * @addrlen - length of foreign address
 * Return value:
 * 0 if operation was successful
 * -EINTR if the operation was interrupted
 * -EINVAL if the socket operations structure is not valid
 * -ETIMEDOUT if the operation timed out
 * Locks:
 * lock on socket
 */
int net_socket_connect(socket_t* socket, struct sockaddr* addr, int addrlen) {
    u32 eflags;
    int rc;
    if (0 == socket->ops) {
        NET_DEBUG("No socket operations\n");
        return -EINVAL;
    }
    if (0 == socket->ops->connect) {
        NET_DEBUG("No connect operation\n");
        return -EINVAL;
    }
    NET_DEBUG("Connecting socket\n");
    /*
     * Lock socket
     */
    spinlock_get(&socket->lock, &eflags);
    /*
     * Is socket already connected?
     */
    if (socket->connected) {
        NET_DEBUG("Socket already connected\n");
        spinlock_release(&socket->lock, &eflags);
        return -EISCONN;
    }
    /*
     * Call socket specific connect. If connecting is an asynchronous process, the protocol
     * specific function needs to check states to verify that there is no connection attempt
     * in progress yet
     */
    NET_DEBUG("Calling connect (%x)\n", socket->ops->connect);
    rc = socket->ops->connect(socket, addr, addrlen);
    /*
     * If 0 == rc, the operation was successful and the protocol specific
     * connect function has update socket->connected, socket->bound, socket->laddr
     * and socket->faddr for us.
     * If rc is not 0 and not -EAGAIN, return error
     */
    if ((0 == rc) || (-EAGAIN != rc)) {
        spinlock_release(&socket->lock, &eflags);
        return rc;
    }
    /*
     * If we get to this point, the connection is established asynchronously. Wait until
     * socket->connected is set, socket->error is set or until we are interrupted by a signal
     * Note that the protocol specific connection routine is not supposed to block
     */
    NET_DEBUG("Waiting until connection request completes\n");
    while (0 == socket->connected) {
        rc = cond_wait_intr(&socket->snd_buffer_change, &socket->lock, &eflags);
        if (-1 == rc) {
            /*
             * A signal arrived. Note that this system call is not restartable, as we have already
             * changed the state of the socket. Thus we return -EINTR instead of -EPAUSE
             */
            return -EINTR;
        }
        if (socket->error) {
            spinlock_release(&socket->lock, &eflags);
            return socket->error;
        }
    }
    spinlock_release(&socket->lock, &eflags);
    return 0;
}

/*
 * Send data to a socket
 * Parameter:
 * @fd - the file descriptor representing the socket
 * @buffer - pointer to data
 * @len - length of data
 * @flags - flags
 * @addr - destination address
 * @addrlen - length of destination address
 * @sendto - use semantics of sendto instead of send
 * Return values:
 * Number of bytes successfully sent
 * -EINVAL if the socket is not valid
 * -EPAUSE if the operation has been interrupted by a signal
 * Locks:
 * lock on socket
 * Unless an error occurs or the operation is interrupted, this function
 * will wait in a loop and call the protocol specific send function until all
 * provided data has been transmitted
 *
 */
ssize_t net_socket_send(socket_t* socket, void* buffer, size_t len, int flags, struct sockaddr* addr, u32 addrlen, int sendto) {
    u32 eflags;
    int rc;
    int sent;
    if (0 == socket->ops) {
        NET_DEBUG("No socket operations\n");
        return -EINVAL;
    }
    if ((0 == socket->ops->send)  && (0 == sendto)) {
        NET_DEBUG("No send operation\n");
        return -EINVAL;
    }
    if ((0 == socket->ops->sendto)  && (1 == sendto)) {
        NET_DEBUG("No sendto operation\n");
        return -EINVAL;
    }
    /*
     * Make sure not to send more than INT_MAX
     */
    if (len > INT32_MAX)
        len = INT32_MAX;
    /*
     * Lock socket
     */
    spinlock_get(&socket->lock, &eflags);
    /*
     * Call protocol specific send. The send is supposed to return
     * -EAGAIN if no buffer space is available. In this case we go to sleep until we are woken up
     * by an event on the condition variable socket->snd_buffer_change
     */
    sent = 0;
    while (1) {
        if (sendto)
            rc = socket->ops->sendto(socket, buffer + sent, len - sent, flags, addr, addrlen);
        else
            rc = socket->ops->send(socket, buffer + sent, len - sent, flags);
        NET_DEBUG("Return code from protocol specific send: %d\n", rc);
        if (rc >= 0)
            sent += rc;
        /*
         * Return if all data has been sent or we received an error code
         * not equal to EAGAIN
         */
        if (((rc < 0) && (rc != -EAGAIN)) || (sent == len))
            break;
        if (0 == socket->so_sndtimeout)
            rc = cond_wait_intr(&socket->snd_buffer_change, &socket->lock, &eflags);
        else
            rc = cond_wait_intr_timed(&socket->snd_buffer_change, &socket->lock, &eflags, socket->so_sndtimeout);
        if (-1 == rc) {
            /*
             * We have been interrupted by a signal. If we have already sent some data,
             * return -EPAUSE, otherwise return the number of bytes sent so far. Note that
             * if cond_wait_intr returns -1, we have already given up the lock
             */
            NET_DEBUG("Interrupted by signal\n");
            if (0 == sent)
                return -EPAUSE;
            else
                return sent;
        }
        if (-2 == rc) {
            /*
             * Timeout. Return number of bytes sent or EAGAIN if no data has been sent yet
             */
            if (0 == sent)
                return -EAGAIN;
            else
                return sent;
        }
    }
    if (-EPIPE == rc) {
        do_kill(pm_get_pid(), __KSIGPIPE);
    }
    if (0 == rc)
        rc = sent;
    spinlock_release(&socket->lock, &eflags);
    return rc;
}

/*
 * Read data from a socket
 * Parameter:
 * @fd - the file descriptor representing the socket
 * @buffer - pointer to data
 * @len - length of data
 * @flags - flags
 * @addr - source address of received data is stored here
 * @addrlen - length of addr field
 * @recvfrom - use recvfrom semantics
 * Return values:
 * Number of bytes successfully read
 * -ENOTCONN if the socket is not connected
 * -EINVAL if the socket is not valid
 * -ETIMEDOUT if the socket timed out
 * -EPAUSE if the read request was interrupted by a signal
 * Locks:
 * lock on socket
 * Note that we do not guarantee that @len bytes are read, in fact if there is data
 * available via the protocol specific recv function, we return this data. MSG_WAITALL
 * is not yet implemented
 *
 */
ssize_t net_socket_recv(socket_t* socket, void* buffer, size_t len, int flags, struct sockaddr* addr, u32* addrlen, int recvfrom) {
    u32 eflags;
    int rc;
    if (0 == socket->ops) {
        NET_DEBUG("No socket operations\n");
        return -EINVAL;
    }
    if ((0 == socket->ops->recv) && (0 == recvfrom)) {
        NET_DEBUG("No recv operation\n");
        return -EINVAL;
    }
    if ((0 == socket->ops->recvfrom) && (1 == recvfrom)) {
        NET_DEBUG("No recvfrom operation\n");
        return -EINVAL;
    }
    /*
     * Limit size to signed value
     */
    if (len > INT32_MAX)
        len = INT32_MAX;
    /*
     * Lock socket
     */
    spinlock_get(&socket->lock, &eflags);
    /*
     * check whether socket is bound
     */
    if (0 == socket->bound) {
        spinlock_release(&socket->lock, &eflags);
        return -EINVAL;
    }
    /*
     * and loop until there is data available
     */
    while(1) {
        /*
         * Call protocol specific receive which is supposed to return
         * -EAGAIN if no data is available. In this case we go to sleep until we are woken up
         * by an event on the condition variable socket->rcv_buffer_change
         */
        if (recvfrom)
            rc = socket->ops->recvfrom(socket, buffer, len, flags, addr, addrlen);
        else
            rc = socket->ops->recv(socket, buffer, len, flags);
        NET_DEBUG("Return code from protocol specific recv: %d\n", rc);
        if (-EAGAIN == rc) {
            /*
             * No data - need to wait unless O_NONBLOCK is set
             */
            if (flags && O_NONBLOCK) {
                rc = 0;
                break;
            }
            if (0 == socket->so_rcvtimeout)
                rc = cond_wait_intr(&socket->rcv_buffer_change, &socket->lock, &eflags);
            else
                rc = cond_wait_intr_timed(&socket->rcv_buffer_change, &socket->lock, &eflags, socket->so_rcvtimeout);
            if (-1 == rc) {
                /*
                 * We have been interrupted by a signal - return
                 * -EPAUSE
                 */
                return -EPAUSE;
            }
            if (-2 == rc) {
                /*
                 * Timeout
                 */
                return -EAGAIN;
            }
        }
        else {
            /*
             * Have either error or read some bytes
             */
            break;
        }
    }
    spinlock_release(&socket->lock, &eflags);
    return rc;
}

/*
 * Bind a socket to a local address
 * Parameter:
 * @socket - the socket
 * @address - the local address
 * @addrlen - length of address argument in bytes
 * Return value:
 * -ENOTSOCK - invalid socket
 * -EINVAL - socket is in invalid state
 * Lock:
 * lock on socket
 */
int net_socket_bind(socket_t* socket, struct sockaddr* address, int addrlen) {
    u32 eflags;
    int rc;
    /*
     * Get lock
     */
    if (0 == socket)
        return -ENOTSOCK;
    spinlock_get(&socket->lock, &eflags);
    /*
     * If socket is already bound, return
     */
    if (socket->bound) {
        spinlock_release(&socket->lock, &eflags);
        return -EINVAL;
    }
    /*
     * Call specific bind function - note that this function is not expected
     * to block
     */
    rc = socket->ops->bind(socket, address, addrlen);
    spinlock_release(&socket->lock, &eflags);
    return rc;
}

/*
 * Prepare a socket for receiving incoming connection (listen state).
 * Parameter:
 * @socket - the socket
 * @backlog - maximum value for queue of incoming connections
 * Return value:
 * -EINVAL if the socket is already connected
 * -ENOTSOCK if the first argument is NULL
 * Locks:
 * lock on socket
 * When the socket has not yet been bound to a local address and the protocol used
 * is connection oriented, a local port number will be determined (ephemeral port)
 * and the local IP address will be set to INADDR_ANY
 *
 */
int net_socket_listen(socket_t* socket, int backlog) {
    u32 eflags;
    int rc = 0;
    if (0 == socket)
        return -ENOTSOCK;
    /*
     * Get lock
     */
    spinlock_get(&socket->lock, &eflags);
    /*
     * If socket is already connected, return error code
     */
    if (socket->connected) {
        spinlock_release(&socket->lock, &eflags);
        return -EINVAL;
    }
    /*
     * Set upper bound for connection queue
     */
    if (backlog > MAX_LISTEN_BACKLOG)
        socket->max_connection_backlog = MAX_LISTEN_BACKLOG;
    else
        socket->max_connection_backlog = backlog;
    /*
     * Invoke socket specific listen
     */
    rc = socket->ops->listen(socket);
    spinlock_release(&socket->lock, &eflags);
    return rc;
}

/*
 * Accept incoming connections from a listening socket
 * Parameter:
 * @socket - the socket
 * @addr - the address to which the socket is connected will be stored here
 * @addrlen - the length of the address
 * @result - the new socket
 * Locks:
 * lock on socket
 */
int net_socket_accept(socket_t* socket, struct sockaddr* addr, socklen_t* addrlen, socket_t** result) {
    u32 eflags;
    socket_t* new_socket;
    int rc;
    /*
     * Get lock on socket
     */
    spinlock_get(&socket->lock, &eflags);
    while (1) {
        /*
         * Scan queue to see if there is a connected socket on the queue
         */
        LIST_FOREACH(socket->so_queue_head, new_socket) {
            /*
             * Is socket connected? If yes, return it. Note that the transport layer is expected
             * to only change the flag connected when holding the lock on the parent
             * As we pass on the reference to the caller and at the same time remove the socket
             * from the list, there is no need to increase the reference count
             */
            if (new_socket->connected) {
                LIST_REMOVE(socket->so_queue_head, socket->so_queue_tail, new_socket);
                if (addr && addrlen) {
                    /*
                     * Fill in address
                     */
                    memcpy((void*) addr, (void*) &new_socket->faddr, MIN(sizeof(struct sockaddr), *addrlen));
                    *addrlen = sizeof(struct sockaddr);
                }
                spinlock_release(&socket->lock, &eflags);
                *result = new_socket;
                return 0;
            }
        }
        /*
         * If we get to this point, there is no established socket in the list
         * Wait until a socket becomes available
         */
        rc = cond_wait_intr(&socket->rcv_buffer_change, &socket->lock, &eflags);
        if (-1 == rc) {
            /*
             * We have been interrupted by a signal - note that we do not own the
             * lock in this case
             */
            return -EPAUSE;
        }
    }
    return 0;
}

/*
 * Post an event on a socket. This function is supposed to be used by the protocol specific
 * functions if a event like the availability of data occurs. It will also wake up any threads
 * which are currently blocked in a select on this socket
 * Parameter:
 * @socket - the socket
 * @event - the event
 * Note that the event can be one of the following or any bitwise combination
 * NET_EVENT_CAN_READ - data available for reading
 * NET_EVENT_CAN_WRITE - data available
 * No locking is done, this needs to be taken care of by the caller
 */
void net_post_event(socket_t* socket, int event) {
    select_req_t* req;
    /*
     * Broadcast on condition variable depending on event type
     */
    if (event & NET_EVENT_CAN_READ) {
        cond_broadcast(&socket->rcv_buffer_change);
    }
    if (event & NET_EVENT_CAN_WRITE) {
        cond_broadcast(&socket->snd_buffer_change);
    }
    /*
     * See whether we have any pending select requests
     * for this event. For each event found, do a sem up operation on the corresponding semaphore
     * and record the reason why we woke up
     */
    LIST_FOREACH(socket->select_queue_head, req) {
        if (req->event & event) {
            req->actual_event |= event;
            sem_up(req->sem);
        }
    }
}

/*
 * Socket specific select
 * Parameter:
 * @socket - the socket on which we are waiting
 * @read - wait for read event
 * @write - wait for write event
 * @semaphore - semaphore to use
 * Return value:
 * NET_IF_CAN_READ if select should return immediately as we can read
 * NET_IF_CAN_WRITE if select should return immediately as we can write
 * NET_IF_CAN_READ + NET_IF_CAN_WRITE if select should return as we can read and write
 * 0 if select needs to wait
 * -1 if an error occurred
 * Locks:
 * lock on socket
 */
int net_socket_select(socket_t* socket, int read, int write, semaphore_t* sem) {
    u32 eflags;
    select_req_t* req;
    int rc;
    /*
     * Lock socket
     */
    if (0 == socket)
        return -1;
    spinlock_get(&socket->lock, &eflags);
    /*
     * Using the protocol specific functions, check whether we can actually get / write data now
     */
    rc = socket->ops->select(socket, read, write);
    if (rc)  {
        spinlock_release(&socket->lock, &eflags);
        return rc;
    }
    /*
     * Add select entry to select table of socket
     */
    req = (select_req_t*) kmalloc(sizeof(select_req_t));
    if (req) {
        req->event = 0;
        req->actual_event = 0;
        if (read)
            req->event += NET_EVENT_CAN_READ;
        if (write)
            req->event += NET_EVENT_CAN_WRITE;
        req->sem = sem;
        LIST_ADD_END(socket->select_queue_head, socket->select_queue_head, req);
    }
    /*
     * Release lock again
     */
    spinlock_release(&socket->lock, &eflags);
    return 0;
}

/*
 * Cancel any pending select requests for the given semaphore and
 * return the event - if any - which caused the event to fire
 * Parameter:
 * @socket - the socket
 * @sem - the semaphore (used to identify select requests which need to be canceled)
 * Return value:
 * event
 * Locks:
 * lock on socket
 */
int net_socket_cancel_select(socket_t* socket, semaphore_t* sem) {
    u32 eflags;
    int rc = 0;
    select_req_t* req;
    /*
     * Lock socket
     */
    if (0 == socket)
        return -1;
    spinlock_get(&socket->lock, &eflags);
    /*
     * Walk table to locate select requests for this semaphore
     */
    LIST_FOREACH(socket->select_queue_head, req) {
        if (req->sem == sem) {
            rc |= req->actual_event;
            LIST_REMOVE(socket->select_queue_head, socket->select_queue_tail, req);
            kfree((void*) req);
        }
    }
    /*
     * Release lock again
     */
    spinlock_release(&socket->lock, &eflags);
    return rc;
}

/*
 * Add a DNS server to the list of registered DNS servers
 * Parameter:
 * @ip_addr - pointer to IP address in network byte order
 * Return value:
 * 0 upon success
 * -ENOMEM if all slots are used
 * -EINVAL if argument is not valid
 */
static int net_add_dns(u32* ip_addr) {
    int i;
    if (0 == ip_addr)
        return -EINVAL;
    for (i = 0; i < MAX_DNS_SERVERS; i++) {
        if (0 == dns_servers[i]) {
            dns_servers[i] = *ip_addr;
            return 0;
        }
    }
    return -ENOMEM;
}

/*
 * Remove a DNS server from the list of registered DNS servers
 * Parameter:
 * @ip_addr - pointer to IP address in network byte order
 * Return value:
 * 0 upon success
 * -EINVAL if argument is not valid
 */
static int net_del_dns(u32* ip_addr) {
    int i;
    int rc = -EINVAL;
    if (0 == ip_addr)
        return -EINVAL;
    for (i = 0; i < MAX_DNS_SERVERS; i++) {
        if (*ip_addr == dns_servers[i]) {
            dns_servers[i] = 0;
            rc = 0;
        }
    }
    return rc;
}


/*
 * Socket ioctl
 * Parameter:
 * @socket - the socket
 * @cmd - the IOCTL command
 * @arg - the argument
 * Return values:
 * 0 upon success
 * -ENOTSOCK if the socket argument is NULL
 * -EINVAL if request or arg is not valid
 */
int net_ioctl(socket_t* socket, unsigned int cmd, void* arg) {
    int rc;
    if (0 == socket)
        return -ENOTSOCK;
    switch (cmd) {
        case SIOCADDNS:
            if (-1 == mm_validate_buffer((u32) arg, sizeof(unsigned int), 0))
                return -EINVAL;
            return net_add_dns((unsigned int*) arg);
        case SIOCDELNS:
            if (-1 == mm_validate_buffer((u32) arg, sizeof(unsigned int), 0))
                return -EINVAL;
            return net_del_dns((unsigned int*) arg);
        case SIOCGIFCONF:
            if (-1 == mm_validate_buffer((u32) arg, sizeof(struct ifconf), 1))
                return -EINVAL;
            rc = net_if_get_ifconf((struct ifconf*) arg);
            if (0 == rc) {
                /*
                 * Add DNS information
                 */
                memcpy(((struct ifconf*) arg)->ifc_dns_servers, dns_servers, sizeof(u32)*MAX_DNS_SERVERS);
            }
            return rc;
        case SIOCGRTCONF:
            if (-1 == mm_validate_buffer((u32) arg, sizeof(struct rtconf), 1))
                return -EINVAL;
            return ip_get_rtconf((struct rtconf*) arg);
        case SIOCSIFADDR:
            if (-1 == mm_validate_buffer((u32) arg, sizeof(struct ifreq), 0))
                return -EINVAL;
            return net_if_set_addr((struct ifreq*) arg);
        case SIOCGIFADDR:
            if (-1 == mm_validate_buffer((u32) arg, sizeof(struct ifreq), 1))
                return -EINVAL;
            return net_if_get_addr((struct ifreq*) arg);
        case SIOCSIFNETMASK:
            if (-1 == mm_validate_buffer((u32) arg, sizeof(struct ifreq), 0))
                return -EINVAL;
            return net_if_set_netmask((struct ifreq*) arg);
        case SIOCGIFNETMASK:
            if (-1 == mm_validate_buffer((u32) arg, sizeof(struct ifreq), 1))
                return -EINVAL;
            return net_if_get_netmask((struct ifreq*) arg);
        case SIOCADDRT:
            if (-1 == mm_validate_buffer((u32) arg, sizeof(struct rtentry), 0))
                return -EINVAL;
            return ip_add_route((struct rtentry*) arg);
        case SIOCDELRT:
            if (-1 == mm_validate_buffer((u32) arg, sizeof(struct rtentry), 0))
                return -EINVAL;
            return ip_del_route((struct rtentry*) arg);
        default:
            return -EINVAL;
    }
}

/*
 * Set socket options
 * Parameter:
 * @socket - the socket
 * @level - level of option
 * @option - option name
 * @option_value - pointer to option value
 * @option_len - length of option in bytes
 * Return value:
 * 0 upon success
 * -EINVAL if level or option are invalid
 * -EDOM if a timeval is expected but the option_len is not equal to sizeof(struct timeval)
 * Note that currently SOL_SOCKET is the only supported level. The implemented options are
 * SO_SNDTIMEO
 * SO_RCVTIMEO
 */
int net_socket_setoption(socket_t* socket, int level, int option, void* option_value, unsigned int option_len) {
    u32 eflags;
    /*
     * Accept only socket level
     */
    if (level != SOL_SOCKET)
        return -EINVAL;
    if (0 == option_value)
        return -EINVAL;
    /*
     * Get lock on socket
     */
    spinlock_get(&socket->lock, &eflags);
    /*
     * and process option
     */
    switch (option) {
        case SO_RCVTIMEO:
            if (sizeof(struct timeval) != option_len) {
                spinlock_release(&socket->lock, &eflags);
                return -EDOM;
            }
            socket->so_rcvtimeout = timer_convert_timeval((struct timeval*) option_value);
            break;
        case SO_SNDTIMEO:
            if (sizeof(struct timeval) != option_len) {
                spinlock_release(&socket->lock, &eflags);
                return -EDOM;
            }
            socket->so_sndtimeout = timer_convert_timeval((struct timeval*) option_value);
            break;
        default:
            spinlock_release(&socket->lock, &eflags);
            return -EINVAL;
    }
    /*
     * Release socket and return success
     */
    spinlock_release(&socket->lock, &eflags);
    return 0;
}

/*
 * Return local and foreign address of a socket
 * Parameter:
 * @socket - the socket
 * @laddr - the local address will be stored here
 * @faddr - the foreign address will be stored here
 * @addrlen - length of address arguments
 * Locks:
 * lock on socket
 */
int net_socket_getaddr(socket_t* socket, struct sockaddr* laddr, struct sockaddr* faddr, unsigned int* addrlen) {
    u32 eflags;
    if (0 == addrlen)
        return -EINVAL;
    /*
     * Get lock
     */
    spinlock_get(&socket->lock, &eflags);
    /*
     * If the socket is not connected and a foreign address is requested, abort
     */
    if (faddr && (0 == socket->connected)) {
        return -ENOTCONN;
    }
    /*
     * Copy address and update address length field
     */
    if (laddr)
        memcpy((void*) laddr, (void*) &socket->laddr, MIN(*addrlen, sizeof(struct sockaddr)));
    if (faddr)
        memcpy((void*) faddr, (void*) &socket->faddr, MIN(*addrlen, sizeof(struct sockaddr)));
    *addrlen = sizeof(struct sockaddr);
    /*
     * Release lock
     */
    spinlock_release(&socket->lock, &eflags);
    return 0;
}

/****************************************************************************************
 * Everything below this line is for debugging purposes only                            *
 ***************************************************************************************/

void net_get_counters(int* created, int* destroyed) {
    *created = net_msg_created;
    *destroyed = net_msg_destroyed;
}
