/*
 * ip.c
 *
 * The IP module of the kernel. This module contains the entire IP processing. Incoming messages are processed via ip_rx_msg which
 * validates the message, performs reassembly if necessary and then passes the message to the corresponding transport protocol.
 *
 * Outgoing messages are handed over to ip_tx_msg by the protocol layer. For each outgoing message, the IP header is added to the
 * message and the interface to which the message needs to be sent is determined, then the message is added to a work queue for
 * later processing. If necessary, the message is fragmented.
 *
 * This module also contains the protocol specific processing to work with raw IP sockets which allow the transmission of IP
 * messages from the application layer without using an intermediate transport protocol.
 *
 * Locking:
 *
 * The most relevant locks in this module are
 * - each socket is protected by a lock
 * - the reference count of each IP socket is protected by a lock
 * - the list of known sockets is protected by a lock
 *
 * For most calls to this module from within net.c, the caller will already have acquired the lock on the socket in question. Thus
 * care needs to be taken to avoid deadlocks. The following orders of lock requests are allowed
 *
 *
 *               -------------- lock on socket list
 *               |                      A
 *               V                      |
 *    lock on ref. count                |
 *               A                      |
 *               |                      |
 *               --------------- lock on socket
 *
 * Limitations:
 *
 * - no ICMP messages are generated when a reassembly timeout occurs
 * - no IP options are processed, messages which contain IP options are discarded
 * - no IP forwarding is supported
 *
 */

#include "net.h"
#include "ip.h"
#include "icmp.h"
#include "debug.h"
#include "lib/stdint.h"
#include "lib/arpa/inet.h"
#include "lib/netinet/in.h"
#include "lib/os/errors.h"
#include "net_if.h"
#include "params.h"
#include "arp.h"
#include "lib/string.h"
#include "wq.h"
#include "tcp.h"
#include "udp.h"
#include "util.h"
#include "lists.h"
#include "mm.h"
#include "lib/os/route.h"
#include "lib/os/if.h"
#include "lib/stddef.h"

extern int __net_loglevel;
#define NET_DEBUG(...) do {if (__net_loglevel > 0 ) { kprintf("DEBUG at %s@%d (%s): ", __FILE__, __LINE__, __FUNCTION__); \
        kprintf(__VA_ARGS__); }} while (0)

/*
 * Macro to extract unsigned int IP addresses from routing table entry
 */
#define RT_DST(rt) ((((struct sockaddr_in*) &(rt)->rt_dst)->sin_addr.s_addr))
#define RT_GW(rt) ((((struct sockaddr_in*) &(rt)->rt_gateway)->sin_addr.s_addr))
#define RT_MASK(rt) ((((struct sockaddr_in*) &(rt)->rt_genmask)->sin_addr.s_addr))

/*
 * This counter is used as IP id
 */
static unsigned int id = 1;
static spinlock_t id_lock;

/*
 * Reassembly slots. A reassembly slot is a structure containing all the information required
 * to reassemble incoming fragments to the original IP package
 */
static reassembly_slot_t reassembly_slots[NR_REASSEMBLY_SLOTS];
static spinlock_t reassembly_slots_lock;

/*
 * The routing table
 */
#define ROUTING_TABLE_ENTRIES 256
static route_t routing_table[ROUTING_TABLE_ENTRIES];
static spinlock_t routing_table_lock;

/*
 * This is a list of known raw IP sockets. Sockets are added to the list upon bind or connect
 */
#define IP_SOCKET_SLOTS 1024
static ip_socket_t* raw_sockets[1024];
static spinlock_t raw_sockets_lock;

/*
 * Forward declarations
 */
static ip_socket_t* clone_socket(ip_socket_t*);
static void ip_release_socket(socket_t*);


/****************************************************************************************
 * As suggested in RFC 815, holes in a datagram which is not yet finally reassembled    *
 * are managed as a linked list, using the datagram buffer as storage. These functions  *
 * manage the list                                                                      *
 * Note that each hole is an instance of the structure hole_t and is stored at byte     *
 * hole->first in the datagram buffer of the respective slot. The fields next and prev  *
 * in the hole structure link each item to its predecessor and successor in the list    *
 * and contain the offset of previous resp. next list element in the datagram buffer    *
 * As 0 is a perfectly valid value for this, hole->prev == 0 does NOT indicate that     *
 * the element is the head of the list. Correspondingly, hole->next == 0 is possible    *
 * also if the hole is not the tail of the list. Conversely, hole->next is unused for   *
 * the list tail and hole->prev is unused for the list head                             *
 ***************************************************************************************/

/*
 * Delete hole from the list of holes in a reassembly slot
 * Parameter:
 * @slot - the reassembly slot
 * @hole - the hole to be removed
 */
static void remove_hole(reassembly_slot_t* slot, hole_t* hole) {
    hole_t* prev_hole;
    hole_t* next_hole;
    /*
     * Get pointer to previous hole in list. By convention, hole->prev is
     * meaningless if the entry is the first entry in the list
     */
    if (hole == slot->hole_list_head)
        prev_hole = 0;
    else
        prev_hole = (hole_t*) (slot->buffer + hole->prev);
    /*
     * Get pointer to next hole in list. Again, note that for the last entry
     * in the list, hole->next is not valid
     */
    if (hole == slot->hole_list_tail)
        next_hole = 0;
    else
        next_hole = (hole_t*) (slot->buffer + hole->next);
    /*
     * Adjust pointers in previous and next hole
     */
    if (next_hole) {
        /*
         * Not the last item in list - adjust prev pointer of next item
         * to point to previous hole. If the item at hand is the first item,
         * we do not have to do this as the prev member is not valid in
         * this case for the new head of the list
         */
        if (prev_hole)
            next_hole->prev = (unsigned char*) prev_hole - slot->buffer;
    }
    if (prev_hole) {
        /*
         * Not the first item in list - adjust next pointer of previous
         * item
         */
        if (next_hole)
            prev_hole->next = (unsigned char*) next_hole - slot->buffer;
    }
    /*
     * If we have just removed the first item, adapt slot_list_head
     */
    if (hole == slot->hole_list_head) {
        slot->hole_list_head = next_hole;
    }
    /*
     * Same thing for last item
     */
    if (hole == slot->hole_list_tail) {
        slot->hole_list_tail = prev_hole;
    }
}

/*
 * Add a new hole at the head of the list of holes in a reassembly slot
 * Parameter:
 * @slot - the reassembly slot to be used
 * @first - first octet of hole
 * @last - last octet of hole
 */
static void add_hole(reassembly_slot_t* slot, unsigned short first, unsigned short last) {
    hole_t* new_hole;
    new_hole = (hole_t*) (slot->buffer + first);
    new_hole->first = first;
    new_hole->last = last;
    new_hole->prev = 0;
    /*
     * If the list is not empty, we need to fill new_hole->next and make it point
     * to the current head of the list. We also need to update the previous pointer
     * of the current head
     */
    if (slot->hole_list_head) {
        new_hole->next = ( (unsigned char*) slot->hole_list_head - slot->buffer);
        slot->hole_list_head->prev = first;
    }
    /*
     * If the list is empty, our new item will become the new tail
     */
    else
        slot->hole_list_tail = new_hole;
    slot->hole_list_head = new_hole;
    NET_DEBUG("Added new hole (%d , %d), located at offset %d (%x) in buffer\n", new_hole->first, new_hole->last,
            (unsigned char*) new_hole - slot->buffer, new_hole);
}

/****************************************************************************************
 * The following functions realize IP reassembly                                        *
 ***************************************************************************************/

/*
 * Initialize a reassembly slot
 * Parameter:
 * @slot - a pointer to the slot
 * @ip_hdr - the IP header from which the slot is to be initialized
 */
static void init_slot(reassembly_slot_t* slot, ip_hdr_t* ip_hdr) {
    slot->used = 1;
    slot->ip_dst = ip_hdr->ip_dest;
    slot->ip_src = ip_hdr->ip_src;
    slot->id = ip_hdr->id;
    slot->ip_proto = ip_hdr->proto;
    slot->hole_list_tail = 0;
    slot->hole_list_head = 0;
    slot->payload_length = 0;
    slot->timeout = REASSEMBLY_TIMEOUT;
    /*
     * Create an "infinite" hole, located at start of buffer. The approach
     * that the holes are stored in the reassembly buffer itself hinges on the
     * length of the hole structure being at most eight bytes - so we better check
     * for that just in case the hole structure is changed in the future
     */
    KASSERT(sizeof(hole_t) <= 8);
    add_hole(slot, 0, HOLE_INF);
}

/*
 * Once all fragments have been received, complete reassembly by building a
 * new network message
 * Parameter:
 * @slot - reassembly slot to use
 * @ip_hdr - IP header of last received fragment
 */
static net_msg_t* build_final_msg(reassembly_slot_t* slot, ip_hdr_t* ip_hdr) {
    net_msg_t* result;
    unsigned char* ip_payload;
    /*
     * If we are not able to create a new message, we return 0 which will eventually
     * lead to the slot being discarded
     */
    if (0 == (result = net_msg_new(slot->payload_length))) {
        return 0;
    }
    /*
     * Fill in header fields in IP header
     */
    ip_hdr_t* result_ip_hdr = (ip_hdr_t*) net_msg_prepend(result, sizeof(ip_hdr_t));
    KASSERT(result_ip_hdr);
    result_ip_hdr->checksum = 0;
    result_ip_hdr->flags = 0x0;
    result_ip_hdr->id = ip_hdr->id;
    result_ip_hdr->ip_dest = ip_hdr->ip_dest;
    result_ip_hdr->ip_src = ip_hdr->ip_src;
    result_ip_hdr->length = ntohs(slot->payload_length + sizeof(ip_hdr_t));
    result_ip_hdr->priority = ip_hdr->priority;
    result_ip_hdr->proto = ip_hdr->proto;
    result_ip_hdr->ttl = ip_hdr->ttl;
    result_ip_hdr->version = ip_hdr->version;
    result_ip_hdr->checksum = ntohs(net_compute_checksum((u16*) result_ip_hdr, sizeof(ip_hdr_t)));
    /*
     * Add IP payload
     */
    ip_payload = net_msg_append(result, slot->payload_length);
    KASSERT(ip_payload);
    memcpy((void*) ip_payload, (void*) slot->buffer, slot->payload_length);
    /*
     * and populate remaining fields of new network message. We only fill those fields
     * which make sense on the level of an IP message
     */
    result->ip_dest = slot->ip_dst;
    result->ip_src = slot->ip_src;
    result->ip_hdr = result_ip_hdr;
    result->ip_length = slot->payload_length;
    result->ip_proto = slot->ip_proto;
    result->nic = 0;
    return result;
}

/*
 * Perform reassembly for an incoming IP packet. If the reassembly completes, the reassembled
 * packet will be returned. In any case, this function will take ownership of the argument, i.e.
 * destroy it if needed. The caller is responsible for destroying the result
 * We follow the algorithm outlined in RFC 815.
 * Parameter:
 * @net_msg - the network message
 * Return value:
 * 0 if reassembly is not complete
 * the reassembled message otherwise
 * Locks:
 * lock on reassembly slot list
 */
net_msg_t* ip_reassembly(net_msg_t* net_msg) {
    u32 eflags;
    int i;
    int unused;
    ip_hdr_t* ip_hdr;
    reassembly_slot_t* slot = 0;
    hole_t* hole = 0;
    hole_t* next = 0;
    unsigned int flags;
    unsigned int offset;
    unsigned int fragment_first;
    unsigned int fragment_last;
    unsigned short hole_last;
    net_msg_t* result = 0;
    int mf;
    /*
     * Check whether reassembly needs to be done. To do this, we first have to extract flags and offset
     * from the IP header
     */
    KASSERT(net_msg);
    ip_hdr = (ip_hdr_t*) net_msg->ip_hdr;
    KASSERT(ip_hdr);
    flags = ntohs(ip_hdr->flags);
    /*
     * The offset in units of eight bytes is stored in lowest
     * 13 bytes of combined flags - offset fields
     */
    offset = (flags & ((1 << 13)  - 1)) * 8;
    /*
     * The first three bits are the flags
     * Bit 15: always 0
     * Bit 14: DF
     * Bit 13: MF
     */
    mf = (flags >> 13) & 0x1;
    if ((0 == mf) && (0 == offset)) {
        /*
         * Unfragmented packet - no reassembly
         */
        return net_msg;
    }
    NET_DEBUG("Need to do reassembly, offset = %d, mf = %d\n", offset, mf);
    /*
     * If we get to this point, the packet is either
     *   the first fragment - offset == 0, mf = 1
     *   the last fragment - mf = 0
     *   a fragment between first and last fragment - mf = 1, offset > 0
     * First we need to get the lock on the reassembly slots
     */
    spinlock_get(&reassembly_slots_lock, &eflags);
    /*
     * Now try to locate the entry for the datagram in the reassembly slot list. If there
     * is no entry yet, create a new one. We use the combination of IP source and destination address,
     * IP protocol and identification in the IP header to identify an existing reassembly slot
     */
    unused = -1;
    slot = 0;
    for (i = 0; i < NR_REASSEMBLY_SLOTS; i++) {
        if ((ip_hdr->ip_dest == reassembly_slots[i].ip_dst) && (ip_hdr->ip_src == reassembly_slots[i].ip_src)
                && (ip_hdr->id == reassembly_slots[i].id) && (ip_hdr->proto == reassembly_slots[i].ip_proto)
                && (1 == reassembly_slots[i].used)) {
            /*
             * Found existing slot
             */
            slot = reassembly_slots + i;
            break;
        }
        if (0 == reassembly_slots[i].used)
            unused = i;
    }
    if (0 == slot) {
        /*
         * No slot found, create new one
         */
        if (-1 == unused) {
            /*
             * No slot free - drop packet
             */
            NET_DEBUG("All slots used\n");
            spinlock_release(&reassembly_slots_lock, &eflags);
            net_msg_destroy(net_msg);
            return 0;
        }
        slot = reassembly_slots + unused;
        init_slot(slot, ip_hdr);
    }
    /*
     * At this point, we have a - possibly empty - reassembly slot. Now proceed with first part of algorithm in RFC 815:
     * Walk list of hole descriptors until we hit upon a hole which partially overlaps the received datagram. Discard this hole
     * and - if needed - replace with smaller holes
     * First identify first and last octet in fragment at hand
     */
    fragment_first = offset;
    fragment_last = offset + ntohs(ip_hdr->length) - sizeof(ip_hdr_t) - 1;
    if (fragment_last > IP_FRAGMENT_MAX_SIZE - 1) {
        NET_DEBUG("Maximum IP fragment size exceeded\n");
        spinlock_release(&reassembly_slots_lock, &eflags);
        net_msg_destroy(net_msg);
        return 0;
    }
    hole = slot->hole_list_head;
    while (hole) {
        /*
         * Determine next hole in list
         * Again do not use hole->next if the hole is already the last entry in the list
         */
        if (hole != slot->hole_list_tail)
            next = (hole_t*) (slot->buffer + hole->next);
        else
            next = 0;
        /*
         * If fragment does not overlap with this hole, we proceed to next hole
         */
        if ((fragment_first > hole->last) || (fragment_last < hole->first)) {
            hole = next;
            continue;
        }
        /*
         * If fragment does not fill up hole on the left side, update the "last" field of the hole
         * Save old value of hole->last for later use
         */
        hole_last = hole->last;
        if (fragment_first > hole->first) {
            hole->last = fragment_first - 1;
        }
        /*
         * Otherwise delete hole - we might add a new hole later again
         */
        else {
            remove_hole(slot, hole);
        }
        /*
         * If fragment does not fill up the hole on the right side and there are more
         * fragments to come, create new hole and add it at head of list
         */
        if ((fragment_last < hole_last) && (1 == mf)) {
            add_hole(slot, fragment_last + 1, hole_last);
        }
        /*
         * Update payload length and copy data to buffer
         */
        if (fragment_last + 1 > slot->payload_length)
            slot->payload_length = fragment_last + 1;
        memcpy((void*) slot->buffer + offset, (void*) net_msg->ip_hdr + sizeof(ip_hdr_t), fragment_last - fragment_first + 1);
        /*
         * and move to next item
         */
        hole = next;
    }
    /*
     * If there are no more holes, we are done. Create a new network message, populate its fields and IP header and the IP
     * payload and return it
     */
    if (0 == slot->hole_list_head) {
        slot->used = 0;
        result = build_final_msg(slot, ip_hdr);
        if (0 == result) {
            NET_DEBUG("Not enough memory for assembled message\n");
        }
    }
    /*
     * Release lock on reassembly slots
     */
    spinlock_release(&reassembly_slots_lock, &eflags);
    /*
     * Drop old message
     */
    net_msg_destroy(net_msg);
    return result;
}

/****************************************************************************************
 * The following function processes receipt of an IP packet and the multiplexing to     *
 * the responsible upper layers of the protocol stack                                   *
 ***************************************************************************************/

/*
 * Forward a packet to a raw IP socket, i.e. copy the message and append it to the
 * queue of pending packets for the socket
 * Parameter:
 * @socket - the socket on which we operate
 * @net_msg - the network message to add to the sockets receive buffer
 * Locks:
 * lock on socket
 */
static void store_in_socket(socket_t* socket, net_msg_t* net_msg) {
    u32 eflags;
    int bytes;
    net_msg_t* clone;
    ip_socket_t* ipcb = SOCK2IPCB(socket);
    NET_DEBUG("Placing raw IP data in socket\n");
    /*
     * Lock socket
     */
    spinlock_get(&socket->lock, &eflags);
    /*
     * Is there space left in buffer?
     */
    bytes = net_msg->ip_length + sizeof(ip_hdr_t);
    if (ipcb->pending_bytes + bytes > IP_RCV_BUFFER_SIZE) {
        /*
         * If the message does not fit into the buffer any more, drop it
         */
        spinlock_release(&socket->lock, &eflags);
        return;
    }
    /*
     * Add copy of network message to queue
     */
    if (0 == (clone = net_msg_clone(net_msg))) {
        /*
         * No memory left, drop message
         */
        spinlock_release(&socket->lock, &eflags);
        return;
    }
    LIST_ADD_END(ipcb->rcv_buffer_head, ipcb->rcv_buffer_tail, clone);
    ipcb->pending_bytes += bytes;
    /*
     * and trigger waiting threads
     */
    net_post_event(socket, NET_EVENT_CAN_READ);
    /*
     * Release lock again
     */
    spinlock_release(&socket->lock, &eflags);
}

/*
 * Receive an IP packet.
 * Parameter:
 * @net_msg - the network message to be processed
 * This function will use the information from the IP header to fill
 * the fields
 *   ip_dest
 *   ip_src
 *   ip_length (length of IP payload)
 * of the network message structure and will then forward the message to the respective
 * upper layer. If the IP version is not v4 or the checksum is not valid, the message
 * is dropped. The header pointer for the target layer is filled
 * If there are registered IP sockets with matching destination address, source address
 * and IP protocol number
 * The function assumes that the following fields in the network message are set
 *   ip_hdr
 *   nic
 * Locks:
 * lock on raw socket list
 * Cross-monitor function calls:
 * store_in_socket
 */
void ip_rx_msg(net_msg_t* net_msg) {
    u32 eflags;
    net_msg_t* out_msg = 0;
    ip_hdr_t* ip_hdr = (ip_hdr_t*) net_msg->ip_hdr;
    int version;
    int hdr_length;
    version = ip_hdr->version >> 4;
    hdr_length = ip_hdr->version & 0xF;
    u16 chksum;
    int i;
    ip_socket_t* ipcb = 0;
    NET_DEBUG("Got IP packet (version = %h, header length = %d)\n", version, hdr_length);
    /*
     * We do not yet support IPv6
     */
    if (IPV4_VERSION != version) {
        NET_DEBUG("IP version is not IPv4\n");
        net_msg_destroy(net_msg);
        return;
    }
    /*
     * nor IP options
     */
    if (IPV4_HDR_LENGTH != hdr_length) {
        NET_DEBUG("Options not yet supported\n");
        net_msg_destroy(net_msg);
        return;
    }
    /*
     * Strong host model: only accept package if it is directed towards the
     * incoming interface
     */
    if ((0 == net_msg->nic) || (0 == net_msg->nic->ip_addr_assigned) ||
            (net_msg->nic->ip_addr != ip_hdr->ip_dest)) {
        net_msg_destroy(net_msg);
        return;
    }
    /*
     * Drop packet if TTL is 0
     */
    if (0 == ip_hdr->ttl) {
        net_msg_destroy(net_msg);
        return;
    }
    /*
     * Compute checksum
     */
    chksum = net_compute_checksum((u16*) ip_hdr, hdr_length * sizeof(u32));
    if (0 != chksum) {
        NET_DEBUG("Got invalid checksum (%x)\n", chksum);
        net_msg_destroy(net_msg);
        return;
    }
    /*
     * Fill IP source and destination address and length of IP payload
     */
    net_msg->ip_dest = ip_hdr->ip_dest;
    net_msg->ip_src = ip_hdr->ip_src;
    if (ntohs(ip_hdr->length) < sizeof(u32)*hdr_length)
        return;
    net_msg->ip_length = ntohs(ip_hdr->length) - sizeof(u32)*hdr_length;
    /*
     * Do reassembly.
     */
    if (0 == (out_msg = ip_reassembly(net_msg))) {
        return;
    }
    /*
     * Are there any raw IP sockets for the protocol which we need to
     * send the packet to?
     */
    for (i = 0; i < IP_SOCKET_SLOTS; i++) {
        /*
         * Get lock on list and retrieve next socket
         * which might be a candidate, then release lock again
         */
        spinlock_get(&raw_sockets_lock, &eflags);
        ipcb = clone_socket(raw_sockets[i]);
        spinlock_release(&raw_sockets_lock, &eflags);
        /*
         * Now verify whether we need to send data to this socket
         */
        if (ipcb) {
            if (IPCB2SOCK(ipcb)->bound) {
                /*
                 * Consider socket if
                 * 1) its proto is the proto of the received packet
                 * 2) its IP source address is the IP destination address of the packet
                 * 3) the protocol matches
                 */
                if ((out_msg->ip_dest == ((struct sockaddr_in*) &IPCB2SOCK(ipcb)->laddr)->sin_addr.s_addr) &&
                        (ip_hdr->proto == ipcb->ip_proto))
                    store_in_socket(IPCB2SOCK(ipcb), net_msg);
            }
            /*
             * Drop reference created via clone again
             */
            ip_release_socket(IPCB2SOCK(ipcb));
        }
    }
    /*
     * Forward packet to higher layers
     */
    switch (ip_hdr->proto) {
        case IP_PROTO_ICMP:
            net_msg_set_icmp_hdr(out_msg, hdr_length * sizeof(u32));
            icmp_rx_msg(out_msg);
            break;
        case IP_PROTO_TCP:
            net_msg_set_tcp_hdr(out_msg, hdr_length * sizeof(u32));
            tcp_rx_msg(out_msg);
            break;
        case IP_PROTO_UDP:
            net_msg_set_udp_hdr(out_msg, hdr_length * sizeof(u32));
            udp_rx_msg(out_msg);
            break;
        default:
            net_msg_destroy(net_msg);
            break;
    }
}


/****************************************************************************************
 * These functions establish the IP routing functionality                               *
 ***************************************************************************************/

/*
 * Given a network mask, determine its length, i.e. the number of 1's in it
 * Parameter:
 * @netmask - the network mask (network byte order)
 * Return value:
 * the number of logical 1's in the network mask
 */
static int get_netmask_length(u32 netmask) {
    int i;
    int length = 0;
    for (i = 0; i < sizeof(u32); i++) {
        if ((1 << i) & netmask)
            length++;
    }
    return length;
}

/*
 * This function will try to determine a network interface (NIC) which is supposed
 * to be used to send a particular packet ("local route"). It returns a NIC and the IP address to be
 * used as "next hop"
 * Parameter
 * @ip_src - the requested IP source address of the route or INADDR_ANY
 * @ip_dst - IP destination address (in network byte order)
 * @next_hop - the IP to be used for the ARP lookup is stored here
 * Return value:
 * 0 if no route could be determined
 * the NIC if a route could be found
 * The routing algorithm is as follows:
 * - apply longest match prefix algorithm to find the best match from the routing table
 * - for local routes, i.e. routes for which the gateway flag is not set, the next hop
 *   is the destination address
 * - otherwise the next hop is the gateway address
 * If the source address is not INADDR_ANY, only routes which match the source address will
 * be selected
 * Locks:
 * routing table lock
 */
nic_t* ip_get_route(u32 ip_src, u32 ip_dst, u32* next_hop) {
    u32 eflags;
    int i;
    struct sockaddr_in* in;
    int length = -1;
    route_t* best_match = 0;
    unsigned int rt_genmask;
    unsigned int rt_dst;
    /*
     * Argument valid?
     */
    if (0 == next_hop) {
        NET_DEBUG("next_hop is NULL, giving up\n");
        return 0;
    }
    /*
     * Get lock on routing table
     */
    spinlock_get(&routing_table_lock, &eflags);
    /*
     * Apply "longest prefix match" algorithm: we walk the list and look at
     * all entries for which destination address in entry and our destination
     * address match mod the genmask. Pick the entry with the best match
     * Only consider entries with matching IP source address
     */
    for (i = 0; i < ROUTING_TABLE_ENTRIES; i++) {
        if (routing_table[i].nic) {
            if (ip_src && ((ip_src != routing_table[i].nic->ip_addr) || (0 == routing_table[i].nic->ip_addr_assigned)))
                continue;
            rt_dst = RT_DST(&routing_table[i].rt_entry);
            rt_genmask = RT_MASK(&routing_table[i].rt_entry);
            if ((rt_genmask & ip_dst) == rt_dst) {
                if (length < get_netmask_length(rt_genmask)) {
                    /*
                     * New best match
                     */
                    best_match = routing_table + i;
                    length = get_netmask_length(rt_genmask);
                }
            }
        }
    }
    /*
     * If there is no matching entry, return
     */
    if (0 == best_match) {
        spinlock_release(&routing_table_lock, &eflags);
        return 0;
    }
    /*
     * Determine next hop: if this is a local route, next hop is destination
     * address, otherwise next hop is gateway
     */
    if (best_match->rt_entry.rt_flags & RT_FLAGS_GW) {
        in = (struct sockaddr_in*) &best_match->rt_entry.rt_gateway;
        *next_hop = in->sin_addr.s_addr;
    }
    else
        *next_hop = ip_dst;
    spinlock_release(&routing_table_lock, &eflags);
    return best_match->nic;
}

/*
 * Determine the source IP address to be used for the transmission of packets
 * to the specified destination address
 * Parameter:
 * @ip_dst - the destination address
 * Return value:
 * 0 if no routing could be found
 * IP adress of network interface to which the packet would be routed otherwise
 */
u32 ip_get_src_addr(u32 ip_dst) {
    nic_t* nic;
    unsigned int next_hop;
    nic = ip_get_route(INADDR_ANY, ip_dst, &next_hop);
    if (0 == nic)
        return 0;
    return nic->ip_addr;
}

/*
 * Get the MTU of the interface associated with a given source
 * address. Note that the MTU is the actual interface payload, i.e.
 * Ethernet payload most of the time, i.e. including the IP header
 * Parameter:
 * @ip_src - source IP address
 * Return value:
 * -1 if no interface could be found with the specific source address
 * the MTU of the interface otherwise
 */
int ip_get_mtu(u32 ip_src) {
    nic_t* nic = 0;
    if (INADDR_ANY == ip_src)
        return NET_IF_DEFAULT_MTU;
    nic = net_if_get_nic(ip_src);
    if (0 == nic)
        return -1;
    return nic->mtu;
}

/*
 * Delete a routing table entry
 * Parameter:
 * @rt_entry - a routing table entry
 * Return value:
 * -ENODEV if the device is not valid
 * -EINVAL if rt_entry is 0
 * Lock:
 * lock on routing table
 */
int ip_del_route(struct rtentry* rt_entry) {
    u32 eflags;
    nic_t* nic;
    int i;
    if (0 == rt_entry)
        return -EINVAL;
    /*
     * First get NIC for name
     */
    if (0 == (nic = net_if_get_nic_by_name(rt_entry->dev))) {
        NET_DEBUG("Device does not exist\n");
        return -ENODEV;
    }
    /*
     * Get lock
     */
    spinlock_get(&routing_table_lock, &eflags);
    /*
     * Now locate matching routing table entries
     */
    for (i = 0; i < ROUTING_TABLE_ENTRIES; i++) {
        if (routing_table[i].nic) {
            if ((RT_DST(rt_entry) == RT_DST(&(routing_table[i].rt_entry)))  &&
                    (RT_GW(rt_entry) == RT_GW(&(routing_table[i].rt_entry)))  &&
                    (RT_MASK(rt_entry) == RT_MASK(&(routing_table[i].rt_entry))) &&
                    (0 == strncmp(rt_entry->dev, routing_table[i].nic->name, 4))) {
                NET_DEBUG("Deleting routing table entry %d\n", i);
                routing_table[i].nic = 0;
            }
        }
    }
    /*
     * Release lock again
     */
    spinlock_release(&routing_table_lock, &eflags);
    return 0;
}

/*
 * Add a routing table entry
 * Parameter:
 * @rt_entry - a routing table entry
 * Return value:
 * -ENODEV if the device is not valid
 * -EINVAL if rt_entry is 0
 * -ENOMEM if there is no free entry in routing table
 * Lock:
 * lock on routing table
 */
int ip_add_route(struct rtentry* rt_entry) {
    u32 eflags;
    nic_t* nic;
    int i;
    int free = -1;
    struct sockaddr_in* dst;
    struct sockaddr_in* netmask;
    struct sockaddr_in* gw;
    if (0 == rt_entry)
        return -EINVAL;
    /*
     * First get NIC for name
     */
    if (0 == (nic = net_if_get_nic_by_name(rt_entry->dev))) {
        NET_DEBUG("Device does not exist\n");
        return -ENODEV;
    }
    /*
     * Delete old duplicate routes if they exist - do this before
     * getting the lock as ip_del_route will itself try to acquire
     * the lock
     */
    ip_del_route(rt_entry);
    /*
     * Get lock
     */
    spinlock_get(&routing_table_lock, &eflags);
    /*
     * Now locate free routing table entry
     */
    for (i = 0; i < ROUTING_TABLE_ENTRIES; i++) {
        if (0 == routing_table[i].nic) {
            free = i;
            break;
        }
    }
    if (-1 == free) {
        spinlock_release(&routing_table_lock, &eflags);
        NET_DEBUG("No free routing table entry\n");
        return -ENOMEM;
    }
    /*
     * and copy data there
     */
    memcpy(&routing_table[free].rt_entry, rt_entry, sizeof(struct rtentry));
    routing_table[free].nic = nic;
    /*
     * Make sure that route destination matches provided netmask
     */
    dst = (struct sockaddr_in*) &routing_table[free].rt_entry.rt_dst;
    netmask = (struct sockaddr_in*) &routing_table[free].rt_entry.rt_genmask;
    dst->sin_addr.s_addr &= netmask->sin_addr.s_addr;
    /*
     * For a direct route, set gateway address to 0.0.0.0
     */
    if (0 == (rt_entry->rt_flags & RT_FLAGS_GW)) {
        gw = (struct sockaddr_in*) &routing_table[free].rt_entry.rt_gateway;
        gw->sin_addr.s_addr = INADDR_ANY;
    }
    /*
     * Release lock again
     */
    spinlock_release(&routing_table_lock, &eflags);
    return 0;
}

/*
 * Remove all routing table entries for a specific NIC
 * Parameter:
 * @nic - the nic
 * Locks:
 * lock on routing table
 */
void ip_purge_nic(nic_t* nic) {
    u32 eflags;
    int i;
    /*
     * Get lock
     */
    spinlock_get(&routing_table_lock, &eflags);
    /*
     * Now walk all entries for the NIC and mark
     * them as invalid
     */
    for (i = 0; i < ROUTING_TABLE_ENTRIES; i++) {
        if (nic == routing_table[i].nic) {
            routing_table[i].nic = 0;
        }
    }
    /*
     * Release lock
     */
    spinlock_release(&routing_table_lock, &eflags);
}

/*
 * Return content of kernel routing table in a rtconf structure as defined in route.h
 * Parameter:
 * @rtc - pointer to rtconf structure in which the field rtc_rtcu.rtcu_req points to an array of
 *        rtentry structures and rtc_len contains the length of the array in bytes
 * Return value:
 * 0 upon success (in this case rtc->rtc_len will be set to the number of actually retrieved bytes)
 */
int ip_get_rtconf(struct rtconf* rtc) {
    int count = 0;
    struct rtentry* rt_entry;
    int i;
    nic_t* nic;
    for (i = 0; i < ROUTING_TABLE_ENTRIES; i++) {
            nic = routing_table[i].nic;
            if (nic) {
                if (count*sizeof(struct rtentry) < rtc->rtc_len) {
                    /*
                     * Copy data from routing table entry to structure
                     */
                    NET_DEBUG("Copying data for routing table entry %d\n", count);
                    rt_entry = rtc->rtc_rtcu.rtcu_req + count;
                    memcpy((void*) rt_entry, (void*) &routing_table[i].rt_entry, sizeof(struct rtentry));
                }
                else {
                    NET_DEBUG("Length of result field exceeded, count = %d\n", count);
                    break;
                }
                count++;
            }
    }
    rtc->rtc_len = count * sizeof(struct rtentry);
    return 0;
}


/****************************************************************************************
 * Handle transmission of messages. This is done in two steps. First a message is       *
 * compiled and an entry referencing it is added to a work queue, then the worker       *
 * thread will run a handler which does the actual transmission                         *
 ***************************************************************************************/

/*
 * This is the part of the IP message transmission which is added as a handler to a work queue
 * and is called by the work queue manager in wq.c
 * Parameter:
 * @arg - a pointer to the network message
 * @timeout - this is set if the entry in the work queue has timed out
 * Return value:
 * EAGAIN if the message should be requeued
 * 0 if the message has been processed
 */
static int tx_handler(void* arg, int timeout) {
    int rc;
    net_msg_t*net_msg = (net_msg_t*) arg;
    mac_address_t dest_hw_address;
    NET_DEBUG("Processing message in work queue\n");
    if (0 == arg)
        return 0;
    if (1 == timeout) {
        NET_DEBUG("Timeout in tx_handler, probably due to ARP request timing out\n");
        net_msg_destroy((net_msg_t*) arg);
        return 0;
    }
    /*
     * Determine target hw address. If there is an entry in the cache, arp_resolve will return
     * 0, otherwise EAGAIN - in this case we also return EAGAIN to instruct the work queue manager
     * to re-add the entry to the queue for later processing
     */
    if (EAGAIN == (rc = arp_resolve(net_msg->nic, net_msg->ip_dest, &dest_hw_address))) {
        NET_DEBUG("Requesting requeue\n");
        return EAGAIN;
    }
    else if (rc) {
        NET_DEBUG("Error %d in arp_resolve, dropping message\n", rc);
        return 0;
    }
    memcpy((void*) net_msg->hw_dest, dest_hw_address, ETH_ADDR_LEN);
    /*
     * and hand over packet to network interface layer
     */
    if (0 == net_if_tx_msg(net_msg))
        return 0;
    else
        ERROR("Error while handing over message to network interface layer, message dropped\n");
    return 0;
}


/*
 * Determine an ID for use as ID in the IP header.
 * Return value:
 * the next usable ID
 * Locks:
 * lock on ID
 */
static unsigned int get_id() {
    unsigned int res;
    u32 eflags;
    spinlock_get(&id_lock, &eflags);
    /*
     * Protect against wrapping IDs - should never be zero
     */
    if (0 == id)
        id++;
    res = id;
    id++;
    spinlock_release(&id_lock, &eflags);
    return res;
}

/*
 * Transmit an IP message.
 * Parameter:
 * @net_msg - the message
 * @ip_dst - IPv4 address of the destination
 * Return values:
 * 0 upon successful completion
 * -EMSGSIZE if DF is set but fragmentation would be required
 * -ENETUNREACH if no route to destination address could be found
 * -ENOMEM if no memory was available for temporary data
 * This function assumes that the net_msg has already been filled with the IP payload, but will add the IP
 * header. It assumes that the following field in the network message structure are filled
 *   ip_proto - filled with the IP transport protocol to be used
 *   ip_df - DF flag
 *   ip_src
 *   ip_dest
 * It sets the following fields for use by the network interface layer
 *   nic - is set based on destination IP address
 *   ethertype - set to 0x800 (in network byte order)
 * If the field net_msg->ip_src is filled with anything different from INADDR_ANY (0),
 * this is used as the IP source address in the header if a matching route exists, otherwise the NICs assigned IP
 * address is used. For the destination address, the destination address in the network message is used. The IP
 * length is derived from the size of the network message
 * Once the message has been assembled, a corresponding entry will be
 * added to the work queue IP_TX_QUEUE_ID to trigger transmission of the message
 */
int ip_tx_msg(net_msg_t* net_msg) {
    u16 chksum;
    int mtu;
    int do_fragment;
    int id;
    int fragment = 0;
    int mf;
    net_msg_t* orig_msg;
    unsigned int bytes_sent;
    unsigned int data_bytes;
    unsigned int offset;
    unsigned int fragment_length;
    unsigned int next_hop;
    ip_hdr_t* ip_hdr;
    /*
     * Determine device to which the message will be routed
     */
    if (0 == (net_msg->nic = ip_get_route(net_msg->ip_src, net_msg->ip_dest, &next_hop))) {
        net_msg_destroy(net_msg);
        return -ENETUNREACH;
    }
    /*
     * Determine MTU
     */
    mtu = net_msg->nic->mtu;
    /*
     * Do we have to fragment the message?
     */
    if (net_msg_get_size(net_msg) + sizeof(ip_hdr_t) > mtu)
        do_fragment = 1;
    else
        do_fragment = 0;
    /*
     * If DF (Don't fragment) flag is requested, we drop message if we need to fragment
     */
    if (net_msg->ip_df && do_fragment) {
        NET_DEBUG("Dropping message to avoid fragmentation, size = %d, ip_df = %d, mtu = %d\n", net_msg_get_size(net_msg), net_msg->ip_df, mtu);
        net_msg_destroy(net_msg);
        return -EMSGSIZE;
    }
    bytes_sent = 0;
    /*
     * Set Ethertype
     */
    net_msg->ethertype = htons(ETHERTYPE_IP);
    /*
     * Determine number of data bytes
     */
    data_bytes = net_msg_get_size(net_msg);
    NET_DEBUG("Message has %d data bytes (IP payload)\n", data_bytes);
    /*
     * and add IP header
     */
    ip_hdr = (ip_hdr_t*) net_msg_prepend(net_msg, sizeof(ip_hdr_t));
    net_msg->ip_hdr = (void*) ip_hdr;
    KASSERT(ip_hdr);
    /*
     * Fill IP header values
     */
    ip_hdr->checksum = 0;
    ip_hdr->flags = ntohs((net_msg->ip_df) << 14);
    ip_hdr->id = 0;
    ip_hdr->ip_dest = net_msg->ip_dest;
    if (net_msg->ip_src)
        ip_hdr->ip_src = net_msg->ip_src;
    else
        ip_hdr->ip_src = net_msg->nic->ip_addr;
    ip_hdr->length = htons(net_msg_get_size(net_msg));
    ip_hdr->priority = 0;
    ip_hdr->proto = net_msg->ip_proto;
    ip_hdr->ttl = IP_DEFAULT_TTL;
    ip_hdr->version = (IPV4_VERSION << 4) + IPV4_HDR_LENGTH;
    /*
     * We now send the individual fragments within the message. Note that as soon as we place the message
     * in the work queue, we hand over ownership of the message to the lower layers. Thus if we need to fragment,
     * we need to keep the original message until the end
     */
    if (do_fragment) {
        orig_msg = net_msg_clone(net_msg);
    }
    else
        orig_msg = 0;
    while (bytes_sent < data_bytes) {
        /*
         * If we do fragmentation, and this is not the first message, we have already
         * handed over the network message to the lower layer. Thus we need to rebuild
         * it from the original message
         */
        if (do_fragment && fragment) {
            if (0 == (net_msg = net_msg_clone(orig_msg))) {
                NET_DEBUG("Not sufficient memory to create new fragment\n");
                net_msg_destroy(orig_msg);
                return -ENOMEM;
            }
        }
        /*
         * Make ip_hdr point to our working copy again
         */
        ip_hdr = (ip_hdr_t*) net_msg->ip_hdr;
        /*
         * Adjust network length if necessary to respect MTU.
         */
        offset = bytes_sent;
        if (do_fragment) {
            /*
             * We have data_bytes - bytes_sent bytes left. If this is the first fragment, cut
             * off and do nothing. Otherwise, copy data from offset in orig_msg to start of
             * data area of our new message
             */
            fragment_length = MIN(mtu, data_bytes - bytes_sent + sizeof(ip_hdr_t)) - sizeof(ip_hdr_t);
            /*
             * If this is not the last fragment, we need to make sure that the fragment length is a multiple of eight
             */
            if (bytes_sent + fragment_length < data_bytes) {
                fragment_length = (fragment_length >> 3) << 3;
            }
            net_msg_cut_off(net_msg, fragment_length + sizeof(ip_hdr_t));
            ip_hdr->length = htons(fragment_length + sizeof(ip_hdr_t));
            if (fragment) {
                memcpy((void*) net_msg->start + sizeof(ip_hdr_t), orig_msg->start + sizeof(ip_hdr_t) + offset, fragment_length);
            }
        }
        else
            fragment_length = data_bytes;
        /*
         * Update bytes sent
         */
        bytes_sent += fragment_length;
        /*
         * Determine ID if necessary and set flags and offset if fragmentation is done
         */
        if (do_fragment) {
            if (0 == fragment)
                id = get_id();
            ip_hdr->id = htons(id);
            mf = (bytes_sent < data_bytes) ? 1 : 0;
            ip_hdr->flags = htons((mf << 13) + offset / 8);
        }
        /*
         * Compute and add checksum
         */
        chksum = net_compute_checksum((u16*) ip_hdr, sizeof(u32) * 0x5);
        ip_hdr->checksum = htons(chksum);
        /*
         * overwrite ip_dst in net_msg with next hop - this will be used for the
         * ARP lookup
         */
        net_msg->ip_dest = next_hop;
        /*
         * and add entry to work queue - this will implicitly also free the
         * network message
         */
        fragment++;
        if (wq_schedule(IP_TX_QUEUE_ID, tx_handler, (void*) net_msg, WQ_RUN_NOW)) {
            NET_DEBUG("Could not add entry to work queue, dropping message\n");
            net_msg_destroy(net_msg);
        }
        /*
         * If this was the last fragment, we can drop the original message again
         */
        if (do_fragment && (bytes_sent == data_bytes))
            net_msg_destroy(orig_msg);

    }
    return 0;
}

/****************************************************************************************
 * Protocol specific operations for raw IP sockets                                      *
 ***************************************************************************************/

/*
 * Clone the reference to a raw IP socket, i.e. increase its reference count
 * by one
 * Parameter:
 * @ipcb - the IP specific part of the socket ("IP control block")
 * Return value:
 * pointer to socket
 * Locks:
 * lock on reference count
 * Reference count:
 * reference count is increased by one
 */
static ip_socket_t* clone_socket(ip_socket_t* ipcb) {
    u32 eflags;
    if (0 == ipcb)
        return 0;
    spinlock_get(&ipcb->ref_count_lock, &eflags);
    ipcb->ref_count++;
    spinlock_release(&ipcb->ref_count_lock, &eflags);
    return ipcb;
}

/*
 * Release reference to a socket and free socket memory
 * if this is the last reference
 * Parameter:
 * @socket - the socket
 * Locks:
 * lock on reference count
 * Reference count
 * the reference count is decreased by one
 */
static void ip_release_socket(socket_t* socket) {
    u32 eflags;
    int count;
    ip_socket_t* ipcb;
    if (0 == socket)
        return;
    ipcb = SOCK2IPCB(socket);
    spinlock_get(&ipcb->ref_count_lock, &eflags);
    count = (ipcb->ref_count--);
    spinlock_release(&ipcb->ref_count_lock, &eflags);
    /*
     * If reference count reaches zero, release memory
     */
    if (0 == count)
        kfree((void*) socket);
}

/*
 * Add a raw IP socket to the list of sockets
 * Parameter
 * @ipcb - the IP control block of the socket
 * Return value:
 * 0 if the operation was successful
 * -ENOBUFS if there is no free raw socket slot
 * Locks:
 * lock on socket list
 * Cross-monitor function calls:
 * clone_socket
 * Reference count:
 * the reference count of the IP control block is increased by one
 */
static int add_socket(ip_socket_t* ipcb) {
    u32 eflags;
    int i;
    int found_slot = 0;
    spinlock_get(&raw_sockets_lock, &eflags);
    /*
     * Find a free slot and use it to store reference to the
     * socket
     */
    for (i = 0; i < IP_SOCKET_SLOTS; i++) {
        if (0 == raw_sockets[i]) {
            raw_sockets[i] = clone_socket(ipcb);
            found_slot = 1;
            break;
        }
    }
    spinlock_release(&raw_sockets_lock, &eflags);
    if (0 == found_slot)
        return -ENOBUFS;
    return 0;
}

/*
 * Remove a socket from the list of known raw sockets
 * Parameter:
 * @ipcb - IP control block to be removed
 * Locks:
 * lock on socket list
 * Cross-monitor function calls:
 * ip_release_socket
 * Reference count:
 * the reference count of the socket drops by one for each
 * instance in the list (usually only one)
 */
static void drop_socket(ip_socket_t* ipcb) {
    u32 eflags;
    int i;
    if (0 == ipcb)
        return;
    /*
     * Lock list
     */
    spinlock_get(&raw_sockets_lock, &eflags);
    /*
     * Walk list and drop all references to this socket
     */
    for (i = 0; i < IP_SOCKET_SLOTS; i++) {
        if (ipcb == raw_sockets[i]) {
            ip_release_socket(IPCB2SOCK(raw_sockets[i]));
            raw_sockets[i] = 0;
        }
    }
    /*
     * Release lock again
     */
    spinlock_release(&raw_sockets_lock, &eflags);
}

/*
 * Connect a raw IP socket
 * Parameter:
 * @socket - the socket
 * @addr - the foreign address
 * @addrlen - length of address argument in bytes
 * Return value:
 * 0 upon success
 * -EINVAL if the address length is not sizeof(struct sockaddr_in)
 * -ENOBUFS if the socket could not be added to the list of raw IP sockets
 */
static int ip_connect_socket(socket_t* socket, struct sockaddr* addr, int addrlen) {
    struct sockaddr_in* laddr;
    /*
     * Verify length of address argument
     */
    if (sizeof(struct sockaddr_in) != addrlen)
        return -EINVAL;
    /*
     * Set local address if the socket is not yet bound
     */
    if (0 == socket->bound) {
        laddr = (struct sockaddr_in*) &socket->laddr;
        laddr->sin_addr.s_addr = ip_get_src_addr(((struct sockaddr_in*) addr)->sin_addr.s_addr);
        laddr->sin_family = AF_INET;
        laddr->sin_port = 0;
        /*
          * Add socket to list
          */
        if (add_socket(SOCK2IPCB(socket)) < 0)
            return -ENOBUFS;
        socket->bound = 1;
    }
    /*
     * Set foreign address
     */
    socket->faddr = *addr;
    socket->connected = 1;
    /*
     * Trigger waiting threads
     */
   net_post_event(socket, NET_EVENT_CAN_WRITE);
   return 0;
}

/*
 * Close a raw IP socket
 * Parameter:
 * @socket - the socket
 * @eflags - EFLAGS argument used by caller when acquiring the lock on the socket
 * Return value:
 * 0 upon success
 */
static int ip_close_socket(socket_t* socket, u32* eflags) {
    ip_socket_t* ipcb = SOCK2IPCB(socket);
    net_msg_t* net_msg;
    /*
     * Remove socket from list used for multiplexing
     */
    drop_socket(ipcb);
    /*
     * Release all network messages in the receive queue
     */
    while (ipcb->rcv_buffer_head) {
        net_msg = ipcb->rcv_buffer_head;
        LIST_REMOVE_FRONT(ipcb->rcv_buffer_head, ipcb->rcv_buffer_tail);
        net_msg_destroy(net_msg);
    }
    return 0;
}

/*
 * Send data via a raw IP socket
 * Parameter:
 * @socket - the socket
 * @buffer - the buffer containing the message payload
 * @len - length of IP payload
 * @flags - flags, currently ignored
 * @addr - destination address
 * @addrlen - size of address field
 * Return values:
 * -EMSGSIZE if the maximum size of a message is exceeded
 * -ENOMEM if no memory could be allocated for a new network message
 * -ENETUNREACH if the network is not reachable (no route found)
 * -ENOTCONN if socket is not connected and addr is NULL
 * -EINVAL if addr is not NULL and addrlen is not sizeof(struct sockaddr_in)
 * -EISCONN if the socket is connected but a destination address is specified
 * number of bytes transmitted otherwise
 */
static int ip_socket_sendto(socket_t* socket, void* buffer, unsigned int len, int flags, struct sockaddr* addr, u32 addrlen) {
    net_msg_t* net_msg;
    unsigned char* data;
    int rc;
    /*
     * Return if we are not connected and do not have a destination address
     */
    if ((0 == socket->connected) && (0 == addr))
        return -ENOTCONN;
    /*
     * If addr is specified, addrlen needs to be valid
     */
    if ((addr) && (sizeof(struct sockaddr_in) != addrlen))
        return -EINVAL;
    /*
     * If socket is connected, addr needs to be NULL
     */
    if ((addr) && (socket->connected))
        return -EISCONN;
    /*
     * Make sure that maximum length of IP payload is not exceeded
     */
    if (len > IP_FRAGMENT_MAX_SIZE) {
        return -EMSGSIZE;
    }
    /*
     * Create a new network message
     */
    if (0 == (net_msg = net_msg_new(len))) {
        return -ENOMEM;
    }
    /*
     * Set fields required by ip_tx_msg. Note that we use the IP protocol which was
     * used to open the socket
     * If the socket is not bound, we set the source address to INADDR_ANY so that ip_tx_msg
     * will determine the source address based on the routing table
     * If the socket is connected, use socket destination address, else use specified address
     */
    if (socket->bound)
        net_msg->ip_src = ((struct sockaddr_in*) &socket->laddr)->sin_addr.s_addr;
    else
        net_msg->ip_src = INADDR_ANY;
    if (socket->connected)
        net_msg->ip_dest = ((struct sockaddr_in*) &socket->faddr)->sin_addr.s_addr;
    else
        net_msg->ip_dest = ((struct sockaddr_in*) addr)->sin_addr.s_addr;
    net_msg->ip_df = 0;
    net_msg->ip_proto = socket->proto.ip.ip_proto;
    /*
     * Copy data to net message
     */
    data = net_msg_append(net_msg, len);
    KASSERT(data);
    memcpy((void*) data, (void*) buffer, len);
    if (0 == (rc = ip_tx_msg(net_msg)))
        return len;
    return rc;
}


/*
 * Send data via a raw IP socket
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
static int ip_socket_send(socket_t* socket, void* buffer, unsigned int len, int flags) {
    return ip_socket_sendto(socket, buffer, len, flags, 0, 0);
}

/*
 * Read from a raw IP socket
 * Parameter:
 * @socket - socket to read from
 * @buffer - buffer
 * @len - number of bytes we read at most
 * @flags - flags to apply
 * @addr - will be filled with address of peer
 * @addrlen - size of provided address buffer, updated with actual length of address
 * Return value:
 * number of bytes read upon success
 * -EINVAL if one of the arguments is not valid or the socket is not bound
 * -EAGAIN if the buffer is empty
 */
static int ip_recvfrom(socket_t* socket, void* buffer, unsigned int len, int flags, struct sockaddr* addr, u32* addrlen) {
    unsigned int bytes;
    unsigned char* data;
    struct sockaddr_in faddr;
    int i;
    if (0 == socket)
        return -EINVAL;
    if (0 == socket->bound)
        return -EINVAL;
    net_msg_t* item;
    ip_socket_t* ipcb = &socket->proto.ip;
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
    if (0 == ipcb->rcv_buffer_head) {
        NET_DEBUG("No data in receive buffer\n");
        return -EAGAIN;
    }
    /*
     * If there are messages in the queue, copy first message to buffer
     */
    item = ipcb->rcv_buffer_head;
    bytes = MIN(item->ip_length + sizeof(ip_hdr_t), len);
    /*
     * and copy data
     */
    data = (unsigned char*) item->ip_hdr;
    for (i = 0; i < bytes; i++) {
        ((u8*) buffer)[i] = data[i];
        ipcb->pending_bytes--;
    }
    /*
     * If addr is specified, copy peer address there
     */
    if (addr) {
        faddr.sin_family = AF_INET;
        faddr.sin_addr.s_addr = item->ip_src;
        faddr.sin_port = 0;
        memcpy((void*) addr, (void*) &faddr, MIN(*addrlen, sizeof(struct sockaddr_in)));
        *addrlen = sizeof(struct sockaddr_in);
    }
    /*
     * Now remove entry from queue and drop reference - this is necessary as
     * we have only added a copy to the receive buffer
     */
    LIST_REMOVE(ipcb->rcv_buffer_head, ipcb->rcv_buffer_tail, item);
    net_msg_destroy(item);
    return bytes;
}


/*
 * Read from a raw IP socket
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
static int ip_recv(socket_t* socket, void* buffer, unsigned int len, int flags) {
    return ip_recvfrom(socket, buffer, len, flags, 0, 0);
}

/*
 * Listen on a raw IP socket. This function does nothing as the operation
 * is meaningless for a raw IP socket
 * Parameter:
 * @socket - the socket
 */
static int ip_listen(socket_t* socket) {
    return 0;
}


/*
 * Bind a raw IP socket to a local address - at this point it will start
 * to receive packets directed towards this address
 * Parameter:
 * @socket - the socket
 * @in - the address to use
 * @addrlen - length of address
 * Return value:
 * -EINVAL if the address length is not equal to sizeof(struct sockaddr_in)
 * -EINVAL if the socket is already bound
 * -ENOBUFS if there is no free entry in the list of raw IP sockets
 */
static int ip_bind(socket_t* socket, struct sockaddr* in, int addrlen) {
    struct sockaddr_in* laddr;
    int rc;
    /*
     * Verify length of address argument
     */
    if (sizeof(struct sockaddr_in) != addrlen)
        return -EINVAL;
    /*
     * Set local address if the socket is not yet bound
     */
    if (0 == socket->bound) {
        laddr = (struct sockaddr_in*) &socket->laddr;
        laddr->sin_addr.s_addr = ((struct sockaddr_in*) in)->sin_addr.s_addr;
        laddr->sin_family = AF_INET;
        laddr->sin_port = 0;
        if (0 == (rc = add_socket(SOCK2IPCB(socket)))) {
            socket->bound = 1;
        }
        else
            return -ENOBUFS;

    }
    else {
        return -EINVAL;
    }
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
static int ip_select(socket_t* socket, int read, int write) {
    int rc = 0;
    ip_socket_t* ipcb = &socket->proto.ip;
    /*
     * If checking for read is requested, check whether there is any data
     * in the receive queue
     */
    if (read) {
        if (ipcb->pending_bytes > 0) {
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
 * This is the socket operations structure which is the interface towards net.c for operations
 * on raw IP sockets
 */
static socket_ops_t ip_socket_ops = {ip_connect_socket, ip_close_socket, ip_socket_send, ip_recv, ip_listen, ip_bind, ip_select,
        ip_release_socket, ip_socket_sendto, ip_recvfrom};

/*
 * Create a raw IP socket, i.e. populate a pre-allocated socket
 * structure
 * Parameter:
 * @socket - a pointer to the pre-allocated socket structure
 * @domain - domain (AF_INET)
 * @proto - IP protocol
 * Return value:
 * 0 upon success
 * -EINVAL if domain is invalid
 */
int ip_create_socket(socket_t* socket, int domain, int proto) {
    /*
     * Only AF_INET supported
     */
    if (AF_INET != domain)
        return -EINVAL;
    /*
     * Fill operations structure
     */
    socket->ops = &ip_socket_ops;
    /*
     * and remember proto
     */
    socket->proto.ip.ip_proto = proto;
    /*
     * init receive buffer
     */
    socket->proto.ip.rcv_buffer_head = 0;
    socket->proto.ip.rcv_buffer_tail = 0;
    socket->proto.ip.pending_bytes = 0;
    /*
     * Finally initialize reference count and corresponding lock
     */
    socket->proto.ip.ref_count = 1;
    spinlock_init(&socket->proto.ip.ref_count_lock);
    return 0;
}

/****************************************************************************************
 * Initialization and timer code                                                        *
 ***************************************************************************************/

/*
 * Initialize IP layer
 */
void ip_init() {
    int i;
    /*
     * Initialize reassembly slots
     */
    for (i = 0; i < NR_REASSEMBLY_SLOTS; i++)
        reassembly_slots[i].used = 0;
    spinlock_init(&reassembly_slots_lock);
    /*
     * and routing table
     */
    for (i = 0; i < ROUTING_TABLE_ENTRIES; i++) {
        routing_table[i].nic = 0;
        routing_table[i].rt_entry.rt_flags = 0;
    }
    spinlock_init(&routing_table_lock);
    /*
     * as well as raw socket list
     */
    for (i = 0; i < IP_SOCKET_SLOTS; i++) {
        raw_sockets[i] = 0;
    }
    spinlock_init(&raw_sockets_lock);
}

/*
 * Timer. This function is called once a second by the timer
 * module timer.c
 */
void ip_do_tick() {
    u32 eflags;
    int i;
    /*
     * We walk the list of reassembly slots and discard all slots
     * which have reached the timeout limit
     */
    spinlock_get(&reassembly_slots_lock, &eflags);
    for (i = 0; i < NR_REASSEMBLY_SLOTS; i++) {
        if (reassembly_slots[i].used) {
            if (reassembly_slots[i].timeout > 0)
                reassembly_slots[i].timeout--;
            if (0 == reassembly_slots[i].timeout) {
                NET_DEBUG("Reassembly timeout for slot %d\n", i);
                reassembly_slots[i].used = 0;
            }
        }
    }
    spinlock_release(&reassembly_slots_lock, &eflags);
}

/****************************************************************************************
 * Everything below this line is for testing purposes only                              *
 ***************************************************************************************/

/*
 * Send a PING to 10.0.2.21 for testing purposes
 */
void ip_test() {
    net_msg_t* net_msg;
    icmp_hdr_t* request_hdr;
    void* request_data;
    int i;
    u16* id;
    u16* seq_no;
    u16 chksum;
    net_msg = net_msg_new(1024);
    KASSERT(net_msg);
    net_msg->icmp_hdr = net_msg_append(net_msg, 256);
    net_msg->ip_length = 256;
    request_hdr = (icmp_hdr_t*) net_msg->icmp_hdr;
    request_data = ((void*) request_hdr) + sizeof(icmp_hdr_t);
    /*
     * Fill ICMP header
     */
    request_hdr->code = 0;
    request_hdr->type = ICMP_ECHO_REQUEST;
    request_hdr->checksum = 0;
    /*
     * Fill ICMP data area. The first two bytes are an identifier, the next two bytes the sequence number
     */
    id = (u16*)(request_data + 2);
    seq_no = id + 1;
    *id = 0xabcd;
    *seq_no = htons(1);
    /*
     * Fill up remaining bytes
     */
    for (i = 0; i < 256 - 4; i++) {
        ((u8*)(request_data + 4))[i] = i;
    }
    /*
     * Compute checksum
     */
    chksum = net_compute_checksum((u16*) request_hdr, net_msg->ip_length);
    request_hdr->checksum = htons(chksum);
    /*
     * and send message
     */
    net_msg->ip_proto = IP_PROTO_ICMP;
    net_msg->ip_dest = net_str2ip("10.0.2.21");
    net_msg->ip_df = 0;
    ip_tx_msg(net_msg);
}

/*
 * Print routing table
 */
void ip_print_routing_table() {
    struct sockaddr_in* dst;
    struct sockaddr_in* gw;
    struct sockaddr_in* mask;
    char dev[5];
    unsigned short flags;
    int i;
    PRINT("\n");
    PRINT("Dest         Mask          Gateway        Device   Flags\n");
    PRINT("--------------------------------------------------------\n");
    for (i = 0; i < ROUTING_TABLE_ENTRIES; i++) {
        if (routing_table[i].nic) {
            dst = (struct sockaddr_in*) &routing_table[i].rt_entry.rt_dst;
            gw = (struct sockaddr_in*) &routing_table[i].rt_entry.rt_gateway;
            mask = (struct sockaddr_in*) &routing_table[i].rt_entry.rt_genmask;
            strncpy(dev, routing_table[i].nic->name, 4);
            dev[4] = 0;
            flags = routing_table[i].rt_entry.rt_flags;
            PRINT("%x    %x     %x      ", dst->sin_addr.s_addr, mask->sin_addr.s_addr, gw->sin_addr.s_addr);
            PRINT("%s     ", dev);
            if (flags & RT_FLAGS_UP)
                PRINT("U");
            if (flags & RT_FLAGS_GW)
                PRINT("G");
            PRINT("\n");
        }
    }
}
