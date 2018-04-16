/*
 * tcp.c
 *
 * This module is the main part of the TCP processing. It contains functions to support the socket API as well as functions to receive
 * and send TCP segments
 *
 * Within the TCP layer, there are three different threads of execution.
 *
 * a) threads calling functions in the socket API
 * b) interrupts raised as segments arrive
 * c) a timer function which is called periodically within an interrupt context
 *
 *
 * Throughout this code, certain state variables are maintained by various functions. The following table gives an overview of the
 * state variables used and the functions updating them
 *
 * Variable in socket                    Description                                    Updated by (not counting initialization)
 * ---------------------------------------------------------------------------------------------------------------------------------------
 * snd_nxt                               As in RFC 793 (next byte which will            send_segment, process_ack,
 *                                       be sent)                                       trigger_send, rtx_expired
 * snd_max                               Highest value of snd_nxt - if snd_nxt          send_segment
 *                                       reaches snd_max again, the recovery
 *                                       after a retransmit is complete
 * snd_una                               As in RFC 793 (first unacknowledged byte)      tcp_rx_msg, process_ack, rtx_expired
 * rcv_nxt                               As in RFC 793 (next byte expected by the       process_text, tcp_rx_msg
 *                                       receiver)
 * snd_wnd                               Send window as advertised by peer              tcp_rx_msg
 * rcv_wnd                               Window advertised to the peer                  send_segment
 * snd_buffer_head                       Head of send buffer (ring buffer)              process_ack
 * snd_buffer_tail                       Tail of send buffer (ring buffer)              tcp_send
 * rcv_buffer_head                       Head of receive buffer (ring buffer)
 * rcv_buffer_tail                       Tail of receive buffer (ring buffer)           tcp_rcv
 * ack_count                             Number of bytes acknowledged since last        process_ack, rtx_expired
 *                                       update of congestion window
 * fin_sent                              FIN has been sent to peer                      send_segment
 * fin_seq_no                            Sequence number of FIN                         send_segment
 * smss                                  MSS when sending                               process_options
 * rmss                                  MSS advertised to the peer                     set_rmss
 * max_wnd                               Maximum window size ever advertised by the     tcp_rx_msg
 *                                       peer
 * right_win_edge                        right edge of receive window as advertised     send_segment
 *                                       to the peer
 * cwnd                                  Congestion window                              process_ack, process_dup_ack, rtx_expired
 * rto                                   Retransmission timeout                         update_srtt
 * ssthresh                              Slow start threshhold                          process_dup_ack, rtx_expired
 * rtx_count                             Number of times a specific segment is          send_segment, process_ack
 *                                       retransmitted
 * snd_wl1                               Sequence number of last window update          tcp_rx_msg
 * snd_wl2                               Acknowledgement number of last window update   tcp_rx_msg
 * closed                                Close has been called for this socket          tcp_close
 * eof                                   No further data can be received via this       drop_socket, tcp_rx_msg, tcp_close
 *                                       connection
 * epipe                                 No further data can be sent via this           drop_socket, tcp_rx_msg, tcp_close
 *                                       connection
 * timeout                               Socket has timed out                           rtx_expired
 *
 *
 * Retransmission timer:
 * ----------------------
 *
 * We use the algorithm outlined in RFC 2988, section 5 to manage the retransmission timer:
 *
 * 1) Whenever a segment containing at least one data byte is sent and the retransmission timer is not set, it is set to the current RTO
 * by send_segment. Otherwise it is left alone.
 *
 * 2) When an acknowledgement is received and accepted, the retransmission timer is canceled if there is no more outstanding
 * unacknowledged data (process_ack)
 *
 * 3) When an acceptable acknowledgement is received and there is still unacknowledged data outstanding, the retransmission timer is
 * set again with the current timeout value by process_ack (including the current backoff factor)
 *
 * 4) When the retransmission timer goes off, rtx_expired is invoked. This will double the backoff factor and force the actual
 * retransmission, including a reset of the timer to the new, backed-off value
 *
 *
 * Maintaining the RTT estimate
 * ---------------------------------
 *
 * To maintain a proper value for the smoothed RTT estimate (SRTT), individual segments are timed. At each point in time, at most
 * one segment can be timed. The number of this segment is kept in timed_segment, the number of ticks which have passed since this
 * segment has been sent is kept in current_rtt. Initially, current_rtt is -1, indicating that no segment is timed.
 *
 * Whenever a segment is send and no segment is timed yet, timed_segment will be set to the current sequence number by send_segment
 * and current_rtt is set back to 0. An exception is made for retransmissions (Karn' algorithm). With each tick, current_rtt
 * will be increased by one.
 *
 * When a valid acknowledgement is received for the timed segment, process_ack will evaluate the rtt and then set it back to minus one.
 *
 * Whenever a retransmission is made by send_segment, it will turn off the current timer by setting rtt to minus one. This avoids the
 * incorrect use of retransmitted segments for the SRTT.
 *
 *
 *
 * Maintaining the delayed ACK timer
 * -----------------------------------
 *
 * As specified in RFC 1022, our TCP implementation does not delay ACKs indefinitely. Instead, a timer is used to make sure that
 * whenever data is accepted, the corresponding ACK is sent after a specific period of time has passed.
 *
 * For this purpose, the socket structure contains an delayed ACK timer. This timer is initially not set. Every time data is accepted
 * by process_text, i.e. every time an acknowledgement is logically created by advancing RCV_NXT, this timer is set unless it is
 * set already (an exception is made for SYNs which are acknowledged immediately).
 *
 * When send_segment actually sends a segment for which the ACK flag is set, it cancels the delayed ACK timer.
 *
 * As the timer expires, trigger_send is called with the flag OF_FORCE set which will force the creation of an ACK.
 *
 * The persist timer
 * ----------------------
 *
 * The following rules apply to maintain the persist timer.
 *
 * 1) if trigger_send determines that data is available in the send buffer, no packets are in flight, but no data can be sent, the
 * persist timer is set. The value used for the persist timer is the RTO
 *
 * 2) when the timer fires, it is canceled, then trigger_send is called with the flag OF_FORCE set and the backoff of the retransmission
 * timer is increased.
 *
 * 3) If trigger_send detects that the window of the peer is closed, but data is available and the force flag is set, it will send
 * one byte of data.
 *
 * 4) whenever new data is sent and the retransmission timer is set, the persist timer is cleared by send_segment
 *
 *
 * The time wait timer
 * --------------------------
 *
 * The time wait timer is set whenever a socket moves into state TIME_WAIT. It is set to 2*TCP_MSL. When the timer fires, the socket is
 * dropped
 *
 * Reference counting
 * -----------------------
 *
 * Each socket has a reference count which is initially set to one. The functions clone_socket and tcp_release_socket should be used to
 * increase the reference count of a socket if a reference is passed back to the caller or stored somewhere respectively to mark a
 * reference to a socket as no longer used. Note that when owning a lock on a socket, you should release the lock before calling
 * tcp_release_socket as after returning from this call, the socket might have been destroyed
 *
 * Locking strategy:
 * ---------------------
 *
 * The following locks are used:
 *
 * socket->proto.tcp.ref_count_lock - this lock is used to protect the reference count of a socket
 * socket->lock - this lock protects the socket status and the list of incoming connections for a listening socket
 * socket_list_lock - protect the global list of known TCP sockets which is the basis for multiplexing and also needs to be acquired
 * each time the local or foreign address of a socket is changed
 *
 * Also note that a socket which is the result of a passive open has a pointer parent back to the listening socket from which it
 * originates and might need to lock this socket as well.
 *
 * To avoid deadlocks, the following orders of acquiring locks are explicitly allowed, all others are forbidden.
 *
 *
 *                                     ----->       ref_count_lock      <------
 *                                     |                  A                   |
 *                                     |                  |                   |
 *                               parent->lock             |            socket_list_lock
 *                                     A                  |                   A
 *                                     |                  |                   |
 *                                     --------      socket->lock       -------
 *
 *
 * Limitations:
 * ---------------
 *
 * - no reassembly queue, i.e. out-of-order datagrams are discarded
 * - no urgent data
 * - no data can be contained in SYN messages
 */



#include "tcp.h"
#include "lib/os/errors.h"
#include "lib/netinet/in.h"
#include "net.h"
#include "ip.h"
#include "debug.h"
#include "timer.h"
#include "lists.h"
#include "util.h"
#include "params.h"
#include "lib/stddef.h"
#include "lib/string.h"
#include "lib/ctype.h"
#include "mm.h"

extern int __net_loglevel;
#define NET_DEBUG(...) do {if (__net_loglevel > 0 ) { kprintf("DEBUG at %s@%d (%s): ", __FILE__, __LINE__, __FUNCTION__); \
        kprintf(__VA_ARGS__); }} while (0)

/*
 * Results of acknowledgement validations
 */
#define ACK_OK 0
#define ACK_DUP 1
#define ACK_TOOMUCH 2
#define ACK_IGN 3

/*
 * Flags which can be passed to the output processing
 * OF_FORCE   - send segment
 * OF_NODATA  - do not send any data, but only control flags
 * OF_PSH - set push flag
 * OF_FAST - perform fast retransmit
 */
#define OF_FORCE 1
#define OF_NODATA 2
#define OF_PUSH 4
#define OF_FAST 8


/*
 * This is a list of created TCP sockets. It is used by the multiplexing mechanism to locate the socket
 * to which a particular incoming segment is routed. The lock socket_list_lock also protects the reference count
 * of each socket as well as local and foreign address
 */
static tcp_socket_t* socket_list_head = 0;
static tcp_socket_t* socket_list_tail = 0;
static spinlock_t socket_list_lock;

/*
 * The socket operation structure and forward declarations. These functions are used by the generic socket layer
 * in net.c to handle TCP specific functionality
 */
static int tcp_connect(socket_t* socket, struct sockaddr* addr, int addrlen);
static int tcp_close(socket_t* socket, u32* eflags);
static int tcp_bind(socket_t* socket, struct sockaddr* address, int addrlen);
static int tcp_send(socket_t* socket, void* buffer, unsigned int len, int flags);
static int tcp_recv(socket_t* socket, void *buf, unsigned int len, int flags);
static int tcp_listen(socket_t* socket);
static int tcp_select(socket_t* socket, int read, int write);
static void tcp_release_socket(socket_t* socket);
static int tcp_recvfrom(socket_t* socket, void* buffer, unsigned int len, int flags, struct sockaddr* addr, u32* addrlen);
static int tcp_sendto(socket_t* socket, void* buffer, unsigned int len, int flags, struct sockaddr* addr, u32 addrlen);

static socket_ops_t tcp_socket_ops = {tcp_connect, tcp_close, tcp_send, tcp_recv, tcp_listen, tcp_bind, tcp_select, tcp_release_socket,
        tcp_sendto, tcp_recvfrom
};

/****************************************************************************************
 * By enabling the TCP_DUMP_* defines, we can enforce a dump of incoming and outgoing   *
 * messages at several points in the process flow                                       *
 ***************************************************************************************/

#undef TCP_DUMP_IN
#undef TCP_DUMP_OUT

#ifdef TCP_DUMP_IN
static void dump_ringbuffer(unsigned char* buffer, int buffer_size, int start, int bytes) {
    int i;
    int line;
    for (line = 0; line < bytes / 16; line++) {
        PRINT("%x   ", (line*16 + start) % buffer_size);
        for (i = 0; i < 16; i++) {
            PRINT("%h ", buffer[(line*16 + i + start) % buffer_size]);
        }
        PRINT("   ");
        for (i = 0; i < 16; i++) {
            if (isprint(buffer[(line*16 + i + start) % buffer_size])) {
                PRINT("%c", buffer[(line*16 + i + start) % buffer_size]);
            }
            else {
                PRINT(".");
            }
        }
        PRINT("\n");
    }
}
#endif

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
static void tcp_release_socket(socket_t* socket) {
    u32 eflags;
    int ref_count;
    /*
     * Get lock
     */
    spinlock_get(&socket->proto.tcp.ref_count_lock, &eflags);
    /*
     * Decrease reference count
     */
    socket->proto.tcp.ref_count--;
    ref_count = socket->proto.tcp.ref_count;
    /*
     * and release lock again
     */
    spinlock_release(&socket->proto.tcp.ref_count_lock, &eflags);
    /*
     * If we have reached zero, free memory. Even though
     * we have released the lock again, this cannot be changed
     * by any other thread as no other thread still holds a reference
     * Also do not forget to release reference count on parent
     */
    if (0 == ref_count) {
        if (socket->parent)
            tcp_release_socket(socket->parent);
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
    spinlock_get(&socket->proto.tcp.ref_count_lock, &eflags);
    /*
     * Increase reference count
     */
    socket->proto.tcp.ref_count++;
    /*
     * Release lock
     */
    spinlock_release(&socket->proto.tcp.ref_count_lock, &eflags);
    /*
     * and return pointer to socket
     */
    return socket;
}

/****************************************************************************************
 * All TCP sockets are kept in a doubly linked list of TCP sockets aka TCP control      *
 * blocks. The following functions manage this list                                     *
 ***************************************************************************************/

/*
 * Given a local and foreign IP address and port number, locate
 * a TCP socket in the list of TCP sockets which matches best
 * Parameter:
 * @local_ip - local IP address (in network byte order)
 * @foreign_ip - foreign IP address (in network byte order)
 * @local_port - local port number (in network byte order)
 * @foreign_port - foreign port number (in network byte order)
 * Returns:
 * Pointer to best match or 0
 * The caller should hold the lock on the socket list. The reference count
 * of the result is not increased, this needs to be done by the caller
 */
static tcp_socket_t* get_matching_tcb(u32 local_ip, u32 foreign_ip, u16 local_port, u16 foreign_port) {
    int matchlevel = -1;
    int this_matchlevel;
    tcp_socket_t* best_match = 0;
    socket_t* socket;
    tcp_socket_t* item;
    struct sockaddr_in* laddr;
    struct sockaddr_in* faddr;
    /*
     * Scan list of existing TCP control blocks. If we find a better match than the given
     * one, update best_match. Only consider a TCB a match if the port number matches the
     * local port number. Then matchlevel is the number of non-wildcard matches
     */
    LIST_FOREACH(socket_list_head, item) {
        socket = TCB2SOCK(item);
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
 * Locate a socket for a given connection, return it and
 * increase reference count
 * Parameter:
 * @laddr - the local address of the connection
 * @faddr - the foreign address of the connection
 * Return value
 * a pointer to the socket if a socket matches the connection quadruple
 * 0 if no matching socket is found
 * Locks:
 * lock on socket list
 * Cross-monitor function calls:
 * clone_socket
 * Reference count:
 * increase reference count on socket by one
 */
static socket_t* locate_socket(struct sockaddr_in * laddr, struct sockaddr_in* faddr) {
    u32 eflags;
    tcp_socket_t* tcb;
    socket_t* res = 0;
    /*
     * Lock list
     */
    spinlock_get(&socket_list_lock, &eflags);
    /*
     * Get best match. Note that by convention, the lock on the socket list also protects the addresses
     * of each socket in the list, so we should have a stable basis here
     */
    if ((tcb = get_matching_tcb(laddr->sin_addr.s_addr, faddr->sin_addr.s_addr, laddr->sin_port, faddr->sin_port))) {
        res = clone_socket(TCB2SOCK(tcb));
    }
    /*
     * Release lock and return
     */
    spinlock_release(&socket_list_lock, &eflags);
    return res;
}

/*
 * Drop a socket, i.e. remove it from the list of TCBs used for multiplexing.
 * The socket will still exist, but will no longer be reachable
 * Parameter:
 * @socket - the socket
 * Locks:
 * lock on socket list
 * Cross-monitor function calls:
 * tcp_release_socket
 * Reference count:
 * decrease reference count of socket by one
 */
static void unregister_socket(socket_t* socket) {
    u32 eflags;
    tcp_socket_t* tcb = 0;
    int found;
    /*
     * Get lock on socket list
     */
    spinlock_get(&socket_list_lock, &eflags);
    /*
     * First make sure that socket is in list
     */
    LIST_FOREACH(socket_list_head, tcb) {
        if (TCB2SOCK(tcb) == socket)
            found = 1;
    }
    if (found) {
        LIST_REMOVE(socket_list_head, socket_list_tail, &socket->proto.tcp);
        /*
         * Decrease reference count to account for the reference held by the list
         * until now.
         */
        KASSERT(socket->proto.tcp.ref_count);
        tcp_release_socket(socket);
    }
    /*
     * and release socket on list
     */
    spinlock_release(&socket_list_lock, &eflags);
}


/*
 * Add a socket to the list of known sockets
 * Parameter:
 * @socket - the socket
 * Return value:
 * 0 if the socket was successfully added
 * -ENOMEM if the upper limit of sockets is reached
 * Locks:
 * lock on socket list
 * Cross-monitor function calls:
 * clone_socket
 * Reference counts:
 * reference count of socket is increased by one as it is added to
 * the list
 */
static int register_socket(socket_t* socket) {
    u32 eflags;
    int count;
    tcp_socket_t* item;
    /*
     * Get lock on socket list
     */
    spinlock_get(&socket_list_lock, &eflags);
    /*
     * First check whether maximum allowed number of sockets has
     * been reached
     */
    count = 0;
    LIST_FOREACH(socket_list_head, item) {
        count++;
    }
    if (count >= MAX_TCP_SOCKETS) {
        spinlock_release(&socket_list_lock, &eflags);
        return -ENOMEM;
    }
    /*
     * Clone reference to socket and add socket to list. Note that we increase the reference
     * count only be one even though the list uses two references - we hold both or none of
     * these references, so this simplification is ok
     */
    clone_socket(socket);
    LIST_ADD_END(socket_list_head, socket_list_tail, &socket->proto.tcp);
    /*
     * Release lock again
     */
    spinlock_release(&socket_list_lock, &eflags);
    return 0;
}

/*
 * Add socket to the list of known sockets, but check that the
 * address quadruple of the socket is unique and fully qualified
 * Parameter:
 * @socket - the socket
 * Return value:
 * 0 if the socket was added
 * -EADDRINUSE if the socket was not added because its address quadruple matches
 * the fully qualified address quadruple of another socket in the list
 * -EINVAL if the socket address contains a wildcard
 * -ENOMEM if the upper limit of sockets is reached
 * Locks:
 * lock on socket list
 * Cross-monitor function calls:
 * clone_socket
 * Reference count:
 * reference count if socket is increased by one if it is added
 */
static int add_socket_check(socket_t* socket) {
    u32 eflags;
    int match;
    struct sockaddr_in* laddr;
    struct sockaddr_in* faddr;
    tcp_socket_t* tcb;
    int count;
    /*
     * Make sure that socket address does not contain a wildcard
     */
    laddr = (struct sockaddr_in*) &socket->laddr;
    faddr = (struct sockaddr_in*) &socket->faddr;
    if ((INADDR_ANY == laddr->sin_addr.s_addr) || (INADDR_ANY == faddr->sin_addr.s_addr)  ||
            (0 == laddr->sin_port) || (0 == faddr->sin_port)) {
        return -EINVAL;
    }
    /*
     * Get lock on socket list
     */
    spinlock_get(&socket_list_lock, &eflags);
    /*
     * First check whether maximum allowed number of sockets has
     * been reached
     */
    count = 0;
    LIST_FOREACH(socket_list_head, tcb) {
        count++;
    }
    if (count >= MAX_TCP_SOCKETS) {
        spinlock_release(&socket_list_lock, &eflags);
        return -ENOMEM;
    }
    /*
     * Scan list to see whether there is already an entry with an exactly
     * matching address.
     */
    match = 0;
    LIST_FOREACH(socket_list_head, tcb) {
        if ((laddr->sin_addr.s_addr == ((struct sockaddr_in*) &(TCB2SOCK(tcb)->laddr))->sin_addr.s_addr))
            if ((faddr->sin_addr.s_addr == ((struct sockaddr_in*) &(TCB2SOCK(tcb)->faddr))->sin_addr.s_addr))
                if ((laddr->sin_port == ((struct sockaddr_in*) &(TCB2SOCK(tcb)->laddr))->sin_port))
                    if ((faddr->sin_port == ((struct sockaddr_in*) &(TCB2SOCK(tcb)->faddr))->sin_port)) {
                        match = 1;
                        break;
                    }
    }
    if (1 == match) {
        spinlock_release(&socket_list_lock, &eflags);
        return -EADDRINUSE;
    }
    /*
     * Add socket to list and increase reference count by one
     */
    clone_socket(socket);
    LIST_ADD_END(socket_list_head, socket_list_tail, &socket->proto.tcp);
    /*
     * Release lock and return
     */
    spinlock_release(&socket_list_lock, &eflags);
    return 0;
}

/*
 * Get a free TCP ephemeral port number, i.e. a port number which is not yet used
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
    tcp_socket_t* tcb;
    socket_t* _socket;
    struct sockaddr_in* in_addr;
    int port_used;
    for (i = TCP_EPHEMERAL_PORT; i < 65536; i++) {
        port_used = 0;
        LIST_FOREACH(socket_list_head, tcb) {
            _socket = TCB2SOCK(tcb);
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
 * A socket in state LISTEN maintains a queue of connection requests. These functions   *
 * manage this queue                                                                    *
 ***************************************************************************************/

/*
 * This function is used to create a new socket if a SYN is received by a socket in state LISTEN
 * The new socket will be bound to the provided address quadruple. The caller is supposed to
 * hold the lock on the parent socket
 * Parameter:
 * @listen_socket - the parent socket
 * @laddr - the local address to which we bind the new socket
 * @faddr - the foreign address to which we bind the new socket
 * Reference count:
 * the reference count of the parent socket is increased by one
 * the reference count of the new socket is set to one
 */
static socket_t* copy_socket(socket_t* listen_socket, struct sockaddr_in* laddr, struct sockaddr_in* faddr) {
    socket_t* new_socket;
    struct sockaddr_in* saddr;
    new_socket = (socket_t*) kmalloc(sizeof(socket_t));
    if (0 == new_socket)
        return 0;
    /*
     * Clone and re-initialize some values
     */
    memcpy((void*) new_socket, (void*) listen_socket, sizeof(socket_t));
    spinlock_init(&new_socket->lock);
    new_socket->proto.tcp.ref_count = 1;
    cond_init(&new_socket->rcv_buffer_change);
    cond_init(&new_socket->snd_buffer_change);
    new_socket->prev = 0;
    new_socket->next = 0;
    new_socket->bound = 1;
    new_socket->connected = 0;
    new_socket->proto.tcp.timeout = 0;
    new_socket->so_queue_head = 0;
    new_socket->so_queue_tail = 0;
    new_socket->select_queue_head = 0;
    new_socket->select_queue_tail = 0;
    new_socket->parent = clone_socket(listen_socket);
    /*
     * Update address of socket to make sure that we now have a fully
     * qualified address
     */
    saddr = (struct sockaddr_in*) &new_socket->laddr;
    saddr->sin_addr.s_addr = laddr->sin_addr.s_addr;
    saddr->sin_port = laddr->sin_port;
    saddr->sin_family = AF_INET;
    saddr = (struct sockaddr_in*) &new_socket->faddr;
    saddr->sin_addr.s_addr = faddr->sin_addr.s_addr;
    saddr->sin_port = faddr->sin_port;
    saddr->sin_family = AF_INET;
    return new_socket;
}


/*
 * Remove a socket from the queue of not yet accepted connections
 * Locks:
 * lock on socket
 * Cross-monitor function calls:
 * tcp_release_socket
 * Reference count:
 * the reference count of the socket which is removed from the
 * queue is decreased by one
 */
static void remove_queued_connection(socket_t* parent, socket_t* socket) {
    u32 eflags;
    int found = 0;
    socket_t* item;
    /*
     * Lock parent
     */
    spinlock_get(&parent->lock, &eflags);
    /*
     * Make sure that socket is on list first
     */
    LIST_FOREACH(parent->so_queue_head, item) {
        if (item == socket)
            found = 1;
    }
    /*
     * If yes, remove it and decrease reference count
     */
    if (found) {
        LIST_REMOVE(parent->so_queue_head, parent->so_queue_tail, socket);
        tcp_release_socket(socket);
    }
    /*
     * Finally release lock on parent again
     */
    spinlock_release(&parent->lock, &eflags);
}


/****************************************************************************************
 * The following functions initialize and destroy sockets                               *
 ***************************************************************************************/

/*
 * Set initial sequence number and update SND_MAX, SND_UNA and SND_NXT
 * SND_NXT is set to ISS+1
 * SND_MAX is set to SND_NXT
 * SND_UNA is set to ISS
 * Parameter:
 * @socket - the socket
 */
static void set_isn(socket_t* socket) {
    u32 seconds;
    u32 useconds;
    u32 iss;
    if (do_gettimeofday(&seconds, &useconds)) {
        ERROR("Could not get time of day, using default ISN\n");
        iss = 1;
    }
    else
        iss = useconds;
    socket->proto.tcp.snd_max = iss;
    socket->proto.tcp.snd_una = iss;
    socket->proto.tcp.isn = iss;
    socket->proto.tcp.snd_nxt = iss;
}

/*
 * Initialize a TCP socket, i.e. initialize all fields and add the socket to the
 * list of TCP sockets
 * Parameter:
 * @socket - the socket to be initialized
 * @domain - the domain, i.e. AF_INET
 * @proto - the protocol to use, not used for TCP sockets
 * Return value:
 * 0 - successful completion
 * -ENOMEM - internal limit for number of sockets reached
 * Reference count:
 * the reference count of the socket is set to two
 */
int tcp_create_socket(socket_t* socket, int domain, int proto) {
    /*
     * Initialize all fields with zero
     */
    memset((void*) socket, 0, sizeof(socket_t));
    /*
     * Fill operations structure
     */
    socket->ops = &tcp_socket_ops;
    /*
     * Set status
     */
    socket->proto.tcp.status = TCP_STATUS_CLOSED;
    /*
     * and reference count. We set the reference count to one as we "virtually" pass back
     * a reference to the caller.
     */
    socket->proto.tcp.ref_count = 1;
    spinlock_init(&socket->proto.tcp.ref_count_lock);
    /*
     * Initialize windows
     */
    socket->proto.tcp.rcv_wnd = RCV_BUFFER_SIZE;
    socket->proto.tcp.cwnd = 1;
    socket->proto.tcp.ssthresh = SSTHRESH_INIT;
    /*
     * Initialize options
     */
    socket->proto.tcp.tcp_options = 0;
    if (0 == params_get_int("tcp_disable_cc"))
        socket->proto.tcp.tcp_options += TCP_OPTIONS_CC;
    /*
     * Set up MSS to default value
     */
    socket->proto.tcp.smss = TCP_DEFAULT_MSS;
    socket->proto.tcp.rmss = TCP_DEFAULT_MSS;
    /*
     * and cwnd to the same default
     */
    socket->proto.tcp.cwnd = TCP_DEFAULT_MSS;
    /*
     * Set RTO to default value
     */
    socket->proto.tcp.rto = RTO_INIT;
    /*
     * and initialize variables used for RTT measurements
     */
    socket->proto.tcp.current_rtt = RTT_NONE;
    socket->proto.tcp.first_rtt = 1;
    /*
     * Init lock
     */
    spinlock_init(&socket->lock);
    /*
     * and add socket to list. Note that once the socket has been added,
     * it becomes reachable for incoming sockets
     */
    return register_socket(socket);
}

/*
 * Update the receive MSS stored in the socket. This is the MSS which we announce
 * to the peer and is determined based on the local IP address of the socket
 * Parameter:
 * @socket - the socket
 */
static void set_rmss(socket_t* socket) {
    struct sockaddr_in* laddr = (struct sockaddr_in*) &socket->laddr;
    u32 mss;
    if (INADDR_ANY == laddr->sin_addr.s_addr) {
        mss = TCP_DEFAULT_MSS;
    }
    else {
        mss = ip_get_mtu(laddr->sin_addr.s_addr);
        if (-1 == mss) {
            mss = TCP_DEFAULT_MSS;
        }
        else {
            mss = mss - sizeof(tcp_hdr_t) - sizeof(ip_hdr_t);
        }
    }
    socket->proto.tcp.rmss = mss;
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
     * Determine MSS which we announce with our SYN
     */
    set_rmss(socket);
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
     * Release lock
     */
    spinlock_release(&socket_list_lock, &eflags);
    return 0;
}


/*
 * Drop a socket. This function will unregister the socket and set its status to CLOSED.
 * The queue of incoming connections for this socket is emptied. The caller is assumed
 * to hold the lock on the socket. Note that this lock is temporarily dropped to avoid
 * deadlocks
 * Parameter:
 * @socket - the socket to be dropped
 * @eflags - pointer to EFLAGS used for acquiring the lock on the socket
 * Locks:
 * lock on each socket which is in the connection queue
 * Cross-monitor function calls:
 * unregister_socket
 * Reference count
 * decrease reference count by one
 */
static void drop_socket(socket_t* socket, u32* eflags) {
    socket_t* queued_sockets[MAX_LISTEN_BACKLOG];
    socket_t* item;
    int i;
    int count;
    u32 _eflags;
    if (0 == socket)
        return;
    /*
     * Set socket status to CLOSED
     */
    socket->proto.tcp.status = TCP_STATUS_CLOSED;
    /*
     * and mark it as not usable any more
     */
    socket->proto.tcp.eof = 1;
    socket->proto.tcp.epipe = 1;
    net_post_event(socket, NET_EVENT_CAN_READ + NET_EVENT_CAN_WRITE);
    /*
     * Unregister socket - this will decrease the reference count
     */
    NET_DEBUG("Reference count is %d\n", socket->proto.tcp.ref_count);
    unregister_socket(socket);
    /*
     * Walk list of queued connections and remove them from our list. Note that to avoid
     * deadlocks, we are not supposed to get a lock on any of the sockets in the queue
     * while we have locked the parent. Thus we get a copy of the queue first, then we
     * temporarily drop the lock on the parent and walk that list
     */
    count = 0;
    LIST_FOREACH(socket->so_queue_head, item) {
        /*
         * Here we do not call clone as we take over the reference
         * previously owned by the list
         */
        queued_sockets[count] = item;
        count++;
        if (count > MAX_LISTEN_BACKLOG - 1)
            PANIC("Did not expect that many sockets in that queue, something went wrong\n");
    }
    socket->so_queue_head = 0;
    /*
     * Now release lock on parent
     */
    spinlock_release(&socket->lock, eflags);
    /*
     * and walk previously queued sockets
     */
    for (i = 0; i < count; i++) {
        /*
         * Close these sockets as well
         */
        item = queued_sockets[i];
        spinlock_get(&item->lock, &_eflags);
        tcp_close(item, eflags);
        spinlock_release(&item->lock, &_eflags);
        /*
         * and release the reference previously held by the queue
         */
        tcp_release_socket(item);
    }
    /*
     * Finally get lock on parent again
     */
    spinlock_get(&socket->lock, eflags);
    NET_DEBUG("Reference count is %d\n", socket->proto.tcp.ref_count);
}

/****************************************************************************************
 * Functions to assemble a TCP segment and send it                                      *
 ***************************************************************************************/

/*
 * Compute TCP checksum
 * Parameter:
 * @words - pointer to IP payload, in network byte order
 * @byte_counts - number of bytes
 * @ip_src - IP source address, in network byte order
 * @ip_dst - IP destination address, in network byte order
 * Result:
 * TCP checksum, in host byte order
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
     * 1 byte IP protocol (6 = TCP)
     * 2 bytes TCP segment length
     * Instead of converting all fields to host byte order before adding them,
     * we add up everything in network byte order and then convert the result
     * This will give the same checksum (see RFC 1071), but will be faster
     */
    sum = 0x6*256 + +htons(byte_count);
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


/*
 * This function creates a TCP network message in which all fields have defaults
 * No checksum is created yet
 * Parameter:
 * @socket - the socket, can be NULL
 * @tcp_payload_len - the number of bytes after the TCP header
 * @tcp_options_len - number of option bytes
 * @request - used to set source and target port if socket is 0
 * The fields in the TCP header will be set up as follows:
 * Destination port: foreign port as stored in socket or source port from request
 * Source port: local port as stored in socket or destination port from request
 * Sequence number: snd_nxt in socket if either the request is NULL or rst = 0 or
 *    a) 0 if the request is not an ACK
 *    b) the acknowledgment number of the request if the request is an ACK
 * Acknowledgment number: rcv_nxt in socket if request is NULL or rst = 0
 *    a) sequence number plus length from request if the request is not an ACK
 *    b) 0 if the request is an ACK
 * Control flags: all flags zero
 * Window size:  receive window size as stored in socket or initial size if no socket is specified
 * TCP checksum: zero
 */
static net_msg_t* create_segment(socket_t* socket, int tcp_payload_len, int tcp_options_len, net_msg_t* request, int rst)  {
    net_msg_t* net_msg = 0;
    tcp_hdr_t* hdr = 0;
    tcp_hdr_t* request_hdr = 0;
    /*
     * Create network message
     */
    if (0 == (net_msg = net_msg_new(sizeof(tcp_hdr_t) + tcp_options_len + tcp_payload_len))) {
        ERROR("Not sufficient memory for net msg\n");
        return 0;
    }
    /*
     * We take some values from the request if available. Otherwise we use the socket. Make sure
     * that at least one of the cases is possible
     */
    if ((0 == request) && (0 == socket)) {
        PANIC("Socket and request both NULL\n");
    }
    hdr = (tcp_hdr_t*) net_msg_append(net_msg, sizeof(tcp_hdr_t));
    KASSERT(hdr);
    if (request)
        request_hdr = (tcp_hdr_t*) request->tcp_hdr;
    /*
     * Initialize header fields
     */
    hdr->ack = 0;
    if (request && rst) {
        if (request_hdr->ack)
            hdr->ack_no = 0;
        else
            hdr->ack_no = htonl(ntohl(request_hdr->seq_no) + request->ip_length - request_hdr->hlength*sizeof(u32));
    }
    else if (socket)
        hdr->ack_no = htonl(socket->proto.tcp.rcv_nxt);
    else {
        PANIC("Need at least request or socket\n");
        return 0;
    }
    hdr->checksum = 0;
    hdr->cwr = 0;
    if (socket) {
        hdr->dst_port = ((struct sockaddr_in*) &socket->faddr)->sin_port;
        hdr->src_port = ((struct sockaddr_in*) &socket->laddr)->sin_port;
    }
    else {
        hdr->dst_port = ((tcp_hdr_t*) request->tcp_hdr)->src_port;
        hdr->src_port = ((tcp_hdr_t*) request->tcp_hdr)->dst_port;
    }
    hdr->ece = 0;
    hdr->fin = 0;
    hdr->hlength = (sizeof(tcp_hdr_t) + tcp_options_len) / sizeof(u32);
    hdr->psh = 0;
    hdr->rst = 0;
    hdr->rsv1 = 0;
    if (request && rst) {
        if (request_hdr->ack)
            hdr->seq_no = request_hdr->ack_no;
        else
            hdr->seq_no = 0;
    }
    else {
        hdr->seq_no = htonl(socket->proto.tcp.snd_nxt);
    }
    hdr->syn = 0;
    hdr->urg = 0;
    hdr->urgent_ptr = 0;
    /*
     * Advertise  size of receive window
     */
    if (socket)
        hdr->window = htons(socket->proto.tcp.rcv_wnd);
    else
        hdr->window = htons(RCV_BUFFER_SIZE);
    /*
     * and set TCP header
     */
    net_msg->tcp_hdr = (void*) hdr;
    return net_msg;
}

/*
 * Compute window which we are going to advertise to the peer with a new
 * segment. No updates are made to the socket structure yet
 * Parameter:
 * @socket - the socket
 * @flags - a pointer to the outflags argument for trigger_send
 * If the new window computed by this function moves the right edge of the
 * peers window by at least two segments to the left, OF_FORCE will be added
 * to *flags
 */
static u32 compute_win(socket_t* socket, int* flags) {
    tcp_socket_t* tcb = &socket->proto.tcp;
    u32 rcv_user;
    u32 space;
    u32 new_right_edge;
    /*
     * Compute available space in receive buffer - this is
     * RCV.BUFFER - RCV.USER in the terminology used in RFC 1122
     */
    rcv_user = tcb->rcv_buffer_tail - tcb->rcv_buffer_head;
    space = RCV_BUFFER_SIZE - rcv_user;
    NET_DEBUG("Space = %d, RCV.NXT = %d, advertised right edge = %d, RCV.WND = %d\n", space, tcb->rcv_nxt, tcb->right_win_edge,
            tcb->rcv_wnd);
    /*
     * Do not advertise windows smaller than the MSS to avoid the SWS
     */
    if (space < tcb->smss)
        space = 0;
    /*
     * Calculate the new right edge of the window which the sender would use if we were
     * to send an ACK using space as advertised window. Note that the difference between
     * this number and the right edge of the window advertised with the previous ACK
     * is the number of bytes which the application has consumed and removed from the receive
     * buffer since the last ACK
     */
    new_right_edge = tcb->rcv_nxt + space;
    /*
     * Avoid a shrinking window, i.e. do not allow the right edge of the window to
     * move to the left
     */
    if (TCP_LT(new_right_edge, tcb->right_win_edge)) {
        space = tcb->right_win_edge - tcb->rcv_nxt;
        new_right_edge = tcb->right_win_edge;
    }
    /*
     * RFC 1122 recommends to combine SWS avoidance on the receiver side with delayed ACK to acknowledge
     * every other segment. It does, however, not specify any details at this point. We follow the approach
     * taken by BSD-style Unix systems and force an ACK if a larger window is available and the ACK moves
     * the right edge of the senders window at least by either 2*MSS or 1/4*RCV_BUFFER_SIZE to the right. This
     * implies that, if the application does not read any data, no ACKs are sent for every other segment, but
     * ACKs are sent for every second segment which has completed its travel from the sender through the network
     * and the receive buffer into the responsibility of the application.
     */
    if (TCP_GEQ(new_right_edge, tcb->right_win_edge + 2*tcb->smss)) {
        *flags |= OF_FORCE;
    }
    else if (TCP_GEQ(new_right_edge, tcb->right_win_edge +  (RCV_BUFFER_SIZE >> 2))) {
        *flags |= OF_FORCE;
    }
    return space;
}

/*
 * Send a segment using data from a ring buffer, starting at the current head
 * If required the retransmission timer is set and the segment is timed
 * SND_NXT, SND_MAX, RCV_WND and RIGHT_WIN_EDGE are updated. For retransmissions,
 * the retransmission counter RTX_COUNT is increased. If the segment contains a FIN,
 * the status of the socket is updated
 * Parameter:
 * @socket - socket to use, can be NULL
 * @ack - should we set the ACK flag
 * @syn - should we set the SYN flag
 * @rst - should we set the RST flag
 * @push - should we set the PSH flag
 * @fin - set the FIN flag
 * @request - used to extract source and target address if socket is 0
 * @data - pointer to ring buffer
 * @head - head of ring buffer
 * @buffer_size - size of ring buffer
 * @bytes - bytes to be sent
 * @new_win - window to be advertised
 * @options - options to include in the message, might be NULL
 *
 */
static int send_segment(socket_t* socket, int ack, int syn, int rst, int push, int fin, net_msg_t* request, u8* data, u32 head,
        u32 buffer_size, u32 bytes, u32 new_win, tcp_options_t* options) {
    u32 ip_src;
    u32 ip_dst;
    tcp_socket_t* tcb;
    net_msg_t* net_msg = 0;
    tcp_hdr_t* hdr = 0;
    u8* tcp_data = 0;
    u8* tcp_options;
    u16 chksum;
    u16* mss;
    int tcp_options_len = 0;
    int i;
    /*
     * MSS is currently the only supported option and is only sent with a SYN
     * We therefore do not need to reduce the number of bytes to transmit when
     * adding options
     */
    if (options && syn)
        tcp_options_len = TCP_OPT_LEN_MSS;
    /*
     * Create network message
     */
    if (0 == (net_msg = create_segment(socket, bytes, tcp_options_len, request, rst))) {
        ERROR("Not sufficient memory for net msg\n");
        return -ENOMEM;
    }
    /*
     * Add options if needed. Currently only the MSS is supported, thus
     * we need TCP_OPT_LEN_MSS additional bytes
     */
    if (options && syn) {
        if (0 == (tcp_options = net_msg_append(net_msg, TCP_OPT_LEN_MSS))) {
            PANIC("Not enough room left in network message, something went wrong\n");
        }
        tcp_options[0] = TCP_OPT_KIND_MSS;
        tcp_options[1] = TCP_OPT_LEN_MSS;
        mss = (u16*)(tcp_options + 2);
        *mss = htons(options->mss);
    }
    /*
     * Append room for data
     */
    if (0 == (tcp_data = net_msg_append(net_msg, bytes))) {
        PANIC("Not enough room left in network message, something went wrong\n");
    }
    /*
     * Copy data from head of ring buffer
     */
    for (i = 0; i < bytes; i++) {
        tcp_data[i] = data[(head + i) % buffer_size];
    }
    /*
     * Set non-standard header fields, in particular we overwrite
     * the window size in the header as set by create_segment
     */
    hdr = (tcp_hdr_t*) net_msg->tcp_hdr;
    hdr->syn = syn;
    hdr->ack = ack;
    hdr->rst = rst;
    hdr->psh = push;
    hdr->fin = fin;
    hdr->window = htons(new_win);
    /*
     * Determine IP source and IP destination address
     */
    if (socket) {
        ip_dst = ((struct sockaddr_in*) &socket->faddr)->sin_addr.s_addr;
        ip_src = ((struct sockaddr_in*) &socket->laddr)->sin_addr.s_addr;
    }
    else if (request) {
        ip_dst = request->ip_src;
        ip_src = request->ip_dest;
    }
    /*
     * Compute checksum.
     */
    chksum = compute_checksum((u16*) hdr, sizeof(tcp_hdr_t) + tcp_options_len + bytes, ip_src, ip_dst);
    hdr->checksum = htons(chksum);
    /*
     * and send message
     */
    net_msg->ip_dest = ip_dst;
    net_msg->ip_src = ip_src;
    net_msg->ip_proto = IPPROTO_TCP;
    net_msg->ip_length = sizeof(tcp_hdr_t) + bytes;
    net_msg->ip_df = 1;
    NET_DEBUG("[SENDING] RST = %d, ACK = %d, SYN = %d, PSH = %d, FIN = %d, SEQ = %d, ACK_NO = %d, LEN = %d, WIN = %d, RECOVERY = %d\n",
            rst, ack, syn,
            push, fin, ntohl(hdr->seq_no), ntohl(hdr->ack_no), bytes, ntohs(hdr->window), (socket) ? socket->proto.tcp.snd_max : 0);
    ip_tx_msg(net_msg);
    /*
     * If we do not operate on a socket, we are done - the remainder of the function will
     * update the socket status
     */
    if (0 == socket)
        return 0;
    tcb = (tcp_socket_t*) &socket->proto.tcp;
    /*
     * Set retransmission timer if the segment contains at least one data byte
     * and the retransmission timer is not yet running
     */
    if (bytes || (1 == syn) || (1 == fin)) {
        if (0 == tcb->rtx_timer.time) {
            if (syn) {
                /*
                 * If the message is a SYN, use SYN_TIMEOUT instead of RTO
                 */
                tcb->rtx_timer.time = SYN_TIMEOUT << tcb->rtx_timer.backoff;
                if (tcb->rtx_timer.time > SYN_TIMEOUT_MAX)
                    tcb->rtx_timer.time = SYN_TIMEOUT_MAX;
            }
            else {
                tcb->rtx_timer.time = tcb->rto << tcb->rtx_timer.backoff;
                if (tcb->rtx_timer.time > RTO_MAX)
                    tcb->rtx_timer.time = RTO_MAX;
            }
            NET_DEBUG("Set retransmission timer to %d\n", tcb->rtx_timer.time);
            /*
             * Clear persist timer
             */
            tcb->persist_timer.time = 0;
        }
        /*
         * If no segment is timed yet and this is not a retransmission, time this segment
         */
        if ((RTT_NONE == tcb->current_rtt) && TCP_GEQ(tcb->snd_nxt, tcb->snd_max)) {
            NET_DEBUG("Timing segment\n");
            tcb->timed_segment = tcb->snd_nxt;
            tcb->current_rtt = 0;
        }
        /*
         * If this is a retransmission, disable timer and
         * update retransmission counter
         */
        else if (TCP_LT(tcb->snd_nxt, tcb->snd_max)) {
            tcb->current_rtt = RTT_NONE;
            tcb->rtx_count++;
        }
    }
    /*
     * If this is an ACK, cancel delayed ACK timer
     */
    if (ack)
        tcb->delack_timer.time = 0;
    /*
     * If the segment contains a FIN, change status
     * and set reminder that FIN has been sent
     */
    if (fin) {
        tcb->fin_sent = 1;
        tcb->fin_seq_no = ntohl(hdr->seq_no);
        switch (tcb->status) {
            case TCP_STATUS_ESTABLISHED:
            case TCP_STATUS_SYN_RCVD:
                tcb->status = TCP_STATUS_FIN_WAIT_1;
                break;
            case TCP_STATUS_CLOSE_WAIT:
                tcb->status = TCP_STATUS_LAST_ACK;
                break;
            default:
                break;
        }
    }
    /*
     * Increase snd_nxt, adapt right_win_edge and rcv_wnd
     */
    if ((1 == syn) || (1 == fin))
        tcb->snd_nxt++;
    tcb->snd_nxt += bytes;
    if (TCP_GT(tcb->snd_nxt, tcb->snd_max))
        tcb->snd_max = tcb->snd_nxt;
    tcb->right_win_edge = new_win + tcb->rcv_nxt;
    tcb->rcv_wnd = new_win;
    return 0;
}

/*
 * Check whether data on the send queue can be transmitted and if yes transmit as many
 * bytes as possible. If required, a FIN is included in the message. This function is the
 * heart of the TCP output processing and is invoked from corresponding system calls as well
 * as from the input processing if an incoming ACK signals that we are allowed to send new data
 * Parameter:
 * @socket - the socket
 * @flags - a combination of the OF_* flags, see comment at the top of this file
 */
static void trigger_send(socket_t* socket, int flags) {
    tcp_socket_t* tcb = &socket->proto.tcp;
    u32 data_bytes;
    u32 max_data_bytes;
    u32 fin;
    u32 have_fin = 0;
    u32 usable_window;
    int send = 0;
    int cont = 1;
    u32 new_win;
    u32 win;
    u32 old_snd_nxt;
    while(cont) {
        /*
         * Save old value of snd_nxt
         */
        old_snd_nxt = tcb->snd_nxt;
        /*
         * If we are doing a fast retransmit, set snd_nxt to
         * snd_una to force a retransmission
         */
        if (flags & OF_FAST) {
            tcb->snd_nxt = tcb->snd_una;
        }
        /*
         * Determine number of bytes available, i.e. the number of bytes in the send
         * buffer which have not yet been send
         */
        max_data_bytes = tcb->snd_buffer_tail - tcb->snd_buffer_head -
                (tcb->snd_nxt - tcb->snd_una);
        /*
         * If we have sent a SYN which has not yet been acknowledged, this calculation needs to be
         * corrected by the outstanding SYN
         */
        if ((TCP_STATUS_SYN_SENT == tcb->status) || (TCP_STATUS_SYN_RCVD == tcb->status))
            max_data_bytes += 1;
        data_bytes = max_data_bytes;
        NET_DEBUG("Bytes in send buffer which have not yet been sent: %d\n", data_bytes);
        /*
         * Determine whether we need to send a FIN. We send a FIN if
         * socket->closed is set, but no FIN has been sent yet, or if socket->closed is set and
         * we do a retransmission of the FIN
         */
        fin = 0;
        if (tcb->closed) {
            if (0 == tcb->fin_sent)
                fin = 1;
            if ((tcb->fin_sent) && (TCP_LEQ(tcb->snd_nxt, tcb->fin_seq_no)))
                fin = 1;
        }
        have_fin = fin;
        NET_DEBUG("Need to send FIN: %d\n", have_fin);
        /*
         * Send at most the maximum segment size, if there is more try to
         * send what is left in the next iteration
         */
        if (data_bytes > tcb->smss) {
            data_bytes = tcb->smss;
            cont = 1;
        }
        else
            cont = 0;
        /*
         * Determine size of usable window. Take congestion control window cwnd into account
         * if corresponding option is enabled
         */
        NET_DEBUG("SND_WND = %d, CWND = %d\n", tcb->snd_wnd, tcb->cwnd);
        win = tcb->snd_wnd;
        if ((tcb->tcp_options & TCP_OPTIONS_CC) && (win > tcb->cwnd))
            win = tcb->cwnd;
        /*
         * If we are doing a fast retransmission, set window size to one segment
         * at most as we only want to retransmit the missing segment
         */
        if ((win > tcb->smss) && (flags & OF_FAST))
            win = tcb->smss;
        /*
         * Determine usable window
         */
        if (TCP_GT(tcb->snd_una + win, tcb->snd_nxt))
            usable_window = tcb->snd_una + win - tcb->snd_nxt;
        else
            usable_window = 0;
        NET_DEBUG("Send buffer tail = %d, Send buffer head = %d, SND_NXT = %d, SND_UNA = %d, flags = %d, win = %d, data_bytes = %d\n",
                socket->proto.tcp.snd_buffer_tail, socket->proto.tcp.snd_buffer_head, socket->proto.tcp.snd_nxt,
                socket->proto.tcp.snd_una, flags, win, data_bytes);
        /*
         * and compute new window to be advertised to peer
         */
        new_win = compute_win(socket, &flags);
        NET_DEBUG("New window: %d\n", new_win);
        /*
         * We are allowed to send data if any of the following is true
         * 1) there is no unacknowledged data and we can send all data
         * 2) there is no unacknowledged data and we can send at least half of the maximum advertised window size
         * 3) if we can fill up a segment
         * 4) we have been asked to send an immediate acknowledgement, i.e. OF_FORCE is set
         * 5) we want to send a FIN, but there is no data to be sent
         */
        send = 0;
        if ((tcb->snd_una == tcb->snd_max) && (data_bytes + fin <= usable_window))
            send = 1;
        if ((tcb->snd_una == tcb->snd_max) &&
                (MIN(data_bytes + fin, usable_window) >= (tcb->max_wnd / 2)))
            send = 1;
        if (MIN(data_bytes + fin, usable_window) >= tcb->smss)
            send = 1;
        if (flags & OF_FORCE)
            send = 1;
        if (fin && (0 == data_bytes))
            send = 1;
        NET_DEBUG("Result of SWS algorithm: send = %d\n", send);
        /*
         * If no data can be sent, no packets are in flight and data is available,
         * set persist timer if not already set.
         * Note that to determine whether there are outstanding (i.e. sent, but
         * unacknowledged) segments, we use the comparison between snd_una and snd_max
         * similar to the check in send_segment. This guarantees that whenever trigger_send
         * is called and data is available, either the persist timer or the retransmission
         * timer is set
         */
        if ((0 == send) && (tcb->snd_una == tcb->snd_max) && (max_data_bytes > 0)) {
            if (0 == tcb->persist_timer.time) {
                tcb->persist_timer.time = tcb->rto;
            }
        }
        /*
         * Send at most usable_window bytes (including FIN) and try to send the remaining bytes in
         * the next iteration.
         */
        if (data_bytes + fin > usable_window) {
            NET_DEBUG("Send FIN later as usable window (%d) is to small\n", usable_window);
            data_bytes = usable_window;
            fin = 0;
            cont = 1;
        }
        /*
         * Only send a FIN if there is no more data in the send buffer after we have
         * sent this segment, otherwise try later
         */
        if (data_bytes < max_data_bytes) {
            NET_DEBUG("Send FIN later\n");
            fin = 0;
            cont = 1;
        }
        /*
         * If there is no data to be sent, return, unless we are forced to send
         */
        if (((0 == send) || ((0 == data_bytes + fin))) && (0 == (flags & OF_FORCE))) {
            NET_DEBUG("No data to be sent\n");
            return;
        }
        /*
         * If we cannot send data because the usable window is null, or if we have been asked to send a segment
         * without any data,leave loop to send at most one segment
         */
        if (flags & OF_NODATA) {
            data_bytes = 0;
            cont = 0;
        }
        if (0 == usable_window)
            cont = 0;
        NET_DEBUG("data_bytes = %d, usable_window = %d, snd_wnd = %d\n", data_bytes, usable_window, socket->proto.tcp.snd_wnd);
        /*
         * If there is data, but the window of the peer is closed, send at least one data byte
         * if we are forced to send data by the force flag
         */
        if (max_data_bytes && (OF_FORCE & flags) && (0 == tcb->snd_wnd)) {
            data_bytes = 1;
        }
        /*
         * If we have a pending FIN, no data and the window of the peer is closed and this is a window probe,
         * use FIN for window probe
         */
        if ((0 == data_bytes) && (OF_FORCE & flags) && (0 == tcb->snd_wnd) && have_fin)
            fin = 1;
        /*
         * Force PSH flag if we send data and this will empty our send buffer
         */
        if ((data_bytes == max_data_bytes) && (data_bytes > 0))
            flags |= OF_PUSH;
        /*
         * Send data. As the send buffer contains unsent data and sent but unacknowledged data, we use the
         * "effective head" snd_buffer_head + SND_NXT - SND_UNA which is the head of the unsent data in the buffer
         */
        send_segment(socket, 1, 0, 0, ((OF_PUSH & flags) ? 1 : 0), fin,  0,  socket->proto.tcp.snd_buffer,
                socket->proto.tcp.snd_buffer_head + socket->proto.tcp.snd_nxt - socket->proto.tcp.snd_una,
                SND_BUFFER_SIZE, data_bytes, new_win, 0);
        /*
         * If we have been forced to send one byte of data (window probe), pull snd_nxt back.
         * Otherwise, if the window of the peer opens up again and we resume processing in slow start,
         * we would not send a segment as the byte which counts as unacknowledged data would shrink our usable window
         * to one byte less than the congestion window and we would refrain from sending data until the persist timer fires
         */
        if ((0 == tcb->snd_wnd) && (flags & OF_FORCE) && (tcb->snd_nxt == tcb->snd_una + 1))
                tcb->snd_nxt = tcb->snd_una;
        /*
         * Reset force flag so that we do not send all data in additional
         * iterations of the while loop
         */
        flags &= ~OF_FORCE;
        /*
         * If we are in fast retransmit, restore old value of snd_nxt and make sure that we leave the loop.
         * Note that if our retransmitted segment included new data (which is possible if a partial segment got lost),
         * we need to make sure that we do not decrease snd_nxt by accident
         */
        if (flags & OF_FAST) {
            cont = 0;
            if (TCP_LT(tcb->snd_nxt,old_snd_nxt))
                tcb->snd_nxt = old_snd_nxt;
        }
    }
}



/****************************************************************************************
 * The next functions are related to TCP input processing                               *
 ***************************************************************************************/


/*
 * Compute new RTT estimate and reset segment timer in socket to prepare timing of next
 * segment
 * Parameters:
 * @socket - the socket
 * @rtt_sample - current RTT sample in TCP ticks
 *
 * To recompute the SRTT whenever a valid sample for the RTT has been taken, we use the following equations (see RFC 6298)
 *
 *   DELTA = new RTT sample - SRTT
 *   SRTT = SRTT + 1/8 * DELTA
 *   RTTVAR = RTTVAR + 1/4 * (ABS(DELTA) - RTTVAR)
 *
 * To avoid the use of floating point arithmetic, the values for SRTT and RTTVAR are stored not in units of ticks, but in units
 * of ticks / 8. For example, tcb->srtt = 8*SRTT. Thus the equations above read
 *
 * delta = 8*rtt_sample - srtt
 * srtt <- srtt + delta >> 3
 * rttvar <- rttvar - ABS(delta/4) - rttvar / 4
 * rto <-  (srtt + max(8, 4*rttvar)) >> 3 but at least RTO_INIT

 * Finally, the RTO is updated as follows
 *
 *   RTO = SRTT + max(G, 4*RTTVAR)
 *
 * Here G is the clock granularity. As all computations are done in units of 8*ticks, this is 8. If the RTO computed this way is
 * less than 1 second, it is rounded up to one second
 *
 * A special rule is applied for the first RTT sample. In this case,
 *
 *   SRTT = RTT
 *   RTTVAR = RTT / 2
 *
 * Again after correcting by the factor 8:
 *
 * srtt = rtt_sample*8
 * rttvar = srtt / 2
 */
static void update_srtt(socket_t* socket, u32 rtt_sample) {
    tcp_socket_t* tcb = &socket->proto.tcp;
    int delta;
    NET_DEBUG("Updating SRTT, sample is %d, current SRTT = %d\n", rtt_sample, tcb->srtt);
    if (1 == tcb->first_rtt) {
        /*
         * First RTT sample
         */
        tcb->first_rtt = 0;
        tcb->srtt = rtt_sample << SRTT_SHIFT;
        tcb->rttvar = tcb->srtt >> 1;
    }
    else {
        delta = (rtt_sample << SRTT_SHIFT) - tcb->srtt;
        tcb->srtt = tcb->srtt + delta / 8;
        if (delta < 0)
            delta = -delta;
        tcb->rttvar = tcb->rttvar - tcb->rttvar / 4 + delta / 4;
    }
    /*
     * Update RTO
     */
    tcb->rto = (tcb->srtt + MAX((1 << SRTT_SHIFT), 4 * tcb->rttvar)) >> SRTT_SHIFT;
    if (tcb->rto < RTO_INIT) {
        tcb->rto = RTO_INIT;
    }
    if (tcb->rto > RTO_MAX)
        tcb->rto = RTO_MAX;
    /*
     * Reset current RTT to prepare for next segment
     */
    tcb->current_rtt = RTT_NONE;
}

/*
 * Move a socket to state "established"
 * Parameter:
 * @socket - the socket
 * @net_msg - the incoming message which causes the status transition
 * This function will:
 * - update SND_UNA, SND_WL1 and SND_WL2
 * - update SND_WND
 * - set the status to ESTABLISHED
 * - set the congestion window to one segment ("slow start")
 * - reset the retransmission counter / timer and use the incoming message as an
 *   RTT sample
 * - broadcast a signal on the condition variable "snd_buffer_change"
 */
static void establish_connection(socket_t* socket, net_msg_t* net_msg) {
    tcp_hdr_t* tcp_hdr = (tcp_hdr_t*) net_msg->tcp_hdr;
    socket->proto.tcp.snd_una = ntohl(tcp_hdr->ack_no);
    socket->proto.tcp.status = TCP_STATUS_ESTABLISHED;
    socket->proto.tcp.cwnd = CWND_IW * socket->proto.tcp.smss;
    socket->connected = 1;
    /*
     * Set SND_WL1 and SND_WL2 and SND_WND
     */
    socket->proto.tcp.snd_wl1 = ntohl(tcp_hdr->seq_no);
    socket->proto.tcp.snd_wl2 = ntohl(tcp_hdr->ack_no);
    socket->proto.tcp.snd_wnd = ntohs(tcp_hdr->window);
    if (socket->proto.tcp.snd_wnd > socket->proto.tcp.max_wnd)
        socket->proto.tcp.max_wnd = socket->proto.tcp.snd_wnd;
    /*
     * Reset retransmission counter
     */
    socket->proto.tcp.rtx_count = 0;
    /*
     * Take RTT sample
     */
    update_srtt(socket, socket->proto.tcp.current_rtt);
    /*
     * and cancel retransmission timer
     */
    socket->proto.tcp.rtx_timer.time = 0;
    /*
     * Inform waiting thread that we are now connected
     */
    net_post_event(socket, NET_EVENT_CAN_WRITE);
}


/*
 * Verify whether an incoming segment is acceptable and determine the number
 * and start of data bytes which will be accepted. The socket state is not modified
 * Parameter:
 * @socket - the socket
 * @segment - the incoming segment
 * @first_byte - will be set to the offset of the first data byte within the payload which can be accepted
 * @last_byte - will be set to th offset of the last data byte within the payload which can be accepted
 * @fin - will be set to 1 if there is a FIN within the window
 * Return value:
 * 1 if segment is acceptable
 * 0 if the segment is not acceptable
 */
static int acceptable(socket_t* socket, net_msg_t* segment, u32* first_byte, u32* last_byte, int* fin) {
    tcp_hdr_t* tcp_hdr = (tcp_hdr_t*) segment->tcp_hdr;
    tcp_socket_t* tcb = &socket->proto.tcp;
    u32 seq = ntohl(tcp_hdr->seq_no);
    u32 len;
    u32 acceptable;
    u32 ctrl;
    /*
     * Determine number of data bytes
     */
    len = segment->ip_length - tcp_hdr->hlength * sizeof(u32);
    /*
     * and control bytes
     */
    ctrl = 0;
    if (tcp_hdr->syn)
        ctrl++;
    if (tcp_hdr->fin)
        ctrl++;
    NET_DEBUG("SEG.LEN = %d, RCV_NXT = %d, RCV_WND = %d, SEQ.SEQ = %d\n", len, tcb->rcv_nxt, tcb->rcv_wnd, seq);
    *fin = 0;
    *last_byte = 0;
    *first_byte = 0;
    /*
     * If receive window is closed, segment is acceptable if its segment number equals RCV_NXT
     */
    if ((0 == tcb->rcv_wnd) && (tcb->rcv_nxt == seq)) {
        NET_DEBUG("Receive window closed\n");
        return 1;
    }
    /*
     * If segment length is zero (including SYN/FIN), accept segment if RCV_NXT <= SEQ < RCV_NXT + RCV_WND
     */
    if ((0 == len) && (0 == ctrl) && (TCP_LEQ(tcb->rcv_nxt,seq)) && (TCP_LT(seq,tcb->rcv_nxt + tcb->rcv_wnd))) {
        NET_DEBUG("Zero data length segment\n");
        return 1;
    }
    /*
     * Accept segment if it starts or ends within our receive window
     */
    if (TCP_LEQ(tcb->rcv_nxt, seq) && TCP_LT(seq, tcb->rcv_nxt + tcb->rcv_wnd)) {
        /*
         * Segment starts within our window. Determine first and last relative sequence number within the segment
         * which are in the window and number of bytes which we can accept
         */
        acceptable = MIN(len + ctrl, tcb->rcv_wnd - (seq - tcb->rcv_nxt));
        *last_byte = acceptable - 1;
        *first_byte = 0;
        /*
         * If there is a FIN, check whether it is within the acceptable area -
         * this will happen if and only if the entire segment fits into the window,
         * as the FIN is considered to be the last octet in the segment
         */
        if (tcp_hdr->fin) {
            if (acceptable == len + ctrl) {
                *fin = 1;
                if ((*last_byte) > 0)
                    (*last_byte)--;
            }
        }
        return 1;
    }
    if (TCP_LEQ(tcb->rcv_nxt,seq + len + ctrl- 1) && TCP_LT(seq + len + ctrl - 1,tcb->rcv_nxt + tcb->rcv_wnd)) {
        /*
         * Segment ends within the window, but starts at the left of the window. Accept
         * only bytes within the window
         */
        *first_byte = tcb->rcv_nxt - seq;
        *last_byte = len + ctrl - 1;
        /*
         * If there is a FIN, it is considered the last byte and therefore acceptable
         */
        if (tcp_hdr->fin) {
            *fin = 1;
            if ((*last_byte) > 0)
                (*last_byte)--;
        }
        return 1;
    }
    return 0;
}


/*
 * Process an ACK, i.e. remove acknowledged octets from the send queue and
 * update SND_UNA. In addition, if the ACK is valid
 * 1) the retransmission counter is reset
 * 2) the congestion window is updated
 * 3) the retransmission timer is reset or canceled
 * 4) an update of the RTT is triggered
 * Parameter:
 * @socket - the socket
 * @segment - the incoming segment
 * Return value:
 * ACK_OK - acknowledgement was valid
 * ACK_DUP - duplicate acknowledgement
 * ACK_TOOMUCH - acknowledged something which we have not sent yet
 * ACK_IGN - ignore ACK
 */
static int process_ack(socket_t* socket, net_msg_t* segment) {
    tcp_hdr_t* tcp_hdr = (tcp_hdr_t*) segment->tcp_hdr;
    u32 ack_no = ntohl(tcp_hdr->ack_no);
    tcp_socket_t* tcb = &socket->proto.tcp;
    u32 len = segment->ip_length - sizeof(u32)*tcp_hdr->hlength;
    NET_DEBUG("Validating incoming ACK, ACK_NO = %d, SND_UNA = %d, SND_NXT = %d, SND_RECOVERY = %d, SND_WND = %d\n",
            ntohl(tcp_hdr->ack_no), tcb->snd_una, tcb->snd_nxt, tcb->snd_max, tcb->snd_wnd);
    /*
     * If this is a valid acknowledgement and we are in established state, compute how many bytes are
     * acknowledged by this segment. Increase SND_UNA accordingly, remove acknowledged bytes from the
     * head of the send buffer and inform threads waiting for the send buffer to become empty
     */
    if (TCP_LT(tcb->snd_una, ack_no) && TCP_LEQ(ack_no, tcb->snd_max)) {
        if (TCP_STATUS_ESTABLISHED == tcb->status) {
            tcb->snd_buffer_head += (ack_no - tcb->snd_una);
            net_post_event(socket, NET_EVENT_CAN_WRITE);
            /*
             * Update counter for number of bytes acknowledged since last update of
             * congestion window
             */
            tcb->ack_count += (ack_no - tcb->snd_una);
            /*
             * Update congestion window. If we are below the slow start threshold SSTHRESH,
             * we are still in slow start - increase congestion window by min(N, SMSS) where
             * N is the number of bytes acknowledged. Otherwise increase congestion window
             * by number of bytes acknowledged since last update - we are in congestion avoidance
             * (see RFC 2581, RFC 5681)
             */
            if (TCP_LT(tcb->cwnd, tcb->ssthresh)) {
                tcb->cwnd += MIN(tcb->smss, ack_no - tcb->snd_una);
                tcb->ack_count = 0;
            }
            else {
                if (TCP_GEQ(tcb->ack_count, tcb->cwnd)) {
                    tcb->cwnd += tcb->smss;
                    tcb->ack_count = 0;
                }
            }
        }
        /*
         * Adapt snd_una
         */
        tcb->snd_una = ack_no;
        /*
         * Set counter for duplicate ACKs back. If we were in fast recovery,
         * also deflate congestion window again
         */
        if (TCP_GEQ(tcb->dupacks, DUPACK_TRIGGER)) {
            tcb->cwnd = tcb->ssthresh;
        }
        tcb->dupacks = 0;
        /*
         * Reset retransmission counter
         */
        tcb->rtx_count = 0;
        /*
         * If we receive an ACK for segment n + X after retransmitting segment n,
         * it can happen that snd_nxt < snd_una. Adapt snd_nxt in this case
         */
        if (TCP_GT(tcb->snd_una, tcb->snd_nxt))
            tcb->snd_nxt = tcb->snd_una;
        /*
         * If there is still unacknowledged data outstanding, reset retransmission timer,
         * otherwise cancel it
         */
        if (tcb->snd_una == tcb->snd_max) {
            tcb->rtx_timer.time = 0;
        }
        else
            tcb->rtx_timer.time = tcb->rto;
        /*
         * In any case reset backoff factor
         */
        tcb->rtx_timer.backoff = 0;
        /*
         * Evaluate RTT if the timed segment has been acknowledged
         */
        if ((RTT_NONE != tcb->current_rtt) && TCP_GEQ(ack_no, tcb->timed_segment)) {
            update_srtt(socket, socket->proto.tcp.current_rtt);
            tcb->current_rtt = RTT_NONE;
        }
    }
    else if (TCP_GEQ(tcb->snd_una, ack_no)) {
        NET_DEBUG("Potential duplicate ACK, ACK = %d, SND_UNA = %d, LEN = %d, WIN = %d, SND_WND = %d\n", ack_no, tcb->snd_una, len,
                ntohs(tcp_hdr->window), tcb->snd_wnd);
        /*
         * We acknowledge something which has been acknowledged before
         * According to RFC 5618, we count this as a duplicate acknowledgement if the following holds:
         * - we have data outstanding
         * - the ACK carries no data
         * - it is not a SYN or a FIN
         * - the acknowledgement number is SND_UNA
         * - window does not change
         */
        if (TCP_LT(tcb->snd_una, tcb->snd_max) && (0 == len) && (0 == tcp_hdr->syn)
                && (0 == tcp_hdr->fin) && (tcb->snd_una == ack_no) && (tcb->snd_wnd == ntohs(tcp_hdr->window)))
            return ACK_DUP;
        return ACK_IGN;
    }
    else {
        /*
         * Acknowledgement for something which we have not sent yet
         */
        return ACK_TOOMUCH;
    }
    return ACK_OK;
}

/*
 * Process the text part of a segment, i.e. add data at tail of receive buffer
 * and adjust RCV_NXT if the data is located at the left side of the window.
 * The delayed ACK timer is set if not yet done
 * Parameter:
 * @socket - the socket on which we operate
 * @segment - the segment to be processed
 * @first_byte - the first byte of the payload which is to be processed
 * @last_byte - the last byte of the payload which is to be processed
 * @fin - segment contains a FIN
 * Return value:
 * 0 if data could be added to the receive queue
 * 1 if data was not aligned with left window edge or could not be copied to receive buffer
 */
static int process_text(socket_t* socket, net_msg_t* segment, u32 first_byte, u32 last_byte, int fin) {
    tcp_socket_t* tcb = &socket->proto.tcp;
    tcp_hdr_t* tcp_hdr = (tcp_hdr_t*) segment->tcp_hdr;
    u32 ctrl_bytes = 0;
    u32 bytes = 0;
    u8* data = segment->tcp_hdr + tcp_hdr->hlength * sizeof(u32);
    u32 i;
    NET_DEBUG("Last byte = %d, first byte = %d\n", last_byte, first_byte);
    /*
     * Determine number of data bytes and control bytes (FIN) received
     */
    if (last_byte)
        bytes = last_byte - first_byte + 1;
    else
        bytes = 0;
    if (fin)
        ctrl_bytes++;
    /*
     * If our window is zero, but the segment contains data, return 1 to force delivery of a pure ACK
     */
    if ((0 == socket->proto.tcp.rcv_wnd) && (segment->ip_length > sizeof(u32)*tcp_hdr->hlength))
        return 1;
    /*
     * Return if there is no data to be processed
     */
    if (0 == bytes + ctrl_bytes)
        return 0;
    /*
     * If we have more bytes than we can put into our receive buffer, sender has not
     * respected our window - return error to force delivery of a pure ACK
     */
    if (tcb->rcv_buffer_tail - tcb->rcv_buffer_head + bytes > RCV_BUFFER_SIZE) {
        NET_DEBUG("Number of bytes (%d) exceeds available buffer size (HEAD = %d, TAIL = %d)\n",
                bytes, tcb->rcv_buffer_head, tcb->rcv_buffer_tail);
        return 1;
    }
    /*
     * If segment is located at the left of the receive window, add it to receive
     * buffer and advance RCV_NXT. If socket->eof is set, discard data
     */
    NET_DEBUG("SEQ = %d, RCV_NXT = %d\n", ntohl(tcp_hdr->seq_no), tcb->rcv_nxt);
    if (TCP_LEQ(ntohl(tcp_hdr->seq_no), tcb->rcv_nxt)) {
        NET_DEBUG("Segment is at the left edge of receive window, bytes = %d, tail = %d\n", bytes, tcb->rcv_buffer_tail);
        /*
         * Now copy data into our receive buffer, starting at first_byte and
         * ending at last_byte. If EOF flag is set, skip this step
         */
        if (0 == tcb->eof) {
#ifdef TCP_DUMP_IN
            int old_tail = tcb->rcv_buffer_tail;
            PRINT("%d@%s (%s): Copying %d bytes to receive buffer, buffer tail is %d, SEQ = %d, RCV_NXT = %d, first_byte = %d\n",
                    __LINE__, __FILE__, __FUNCTION__, bytes,
                    old_tail, ntohl(tcp_hdr->seq_no), tcb->rcv_nxt, first_byte);
#endif
            for (i = 0; i < bytes; i++) {
                tcb->rcv_buffer[tcb->rcv_buffer_tail % RCV_BUFFER_SIZE] = data[first_byte + i];
                tcb->rcv_buffer_tail++;
            }
#ifdef TCP_DUMP_IN
            dump_ringbuffer(tcb->rcv_buffer, RCV_BUFFER_SIZE, old_tail, bytes);
#endif
            /*
             * Inform any threads waiting on the buffer that we have added data
             */
            net_post_event(socket, NET_EVENT_CAN_READ);
        }
        /*
         * Update RCV_NXT
         */
        NET_DEBUG("Increasing RCV_NXT by %d\n", bytes + ctrl_bytes);
        tcb->rcv_nxt += bytes + ctrl_bytes;
        /*
         * Set delayed ACK timer if not set already
         */
        if (0 == tcb->delack_timer.time)
            tcb->delack_timer.time = DELACK_TO;
        return 0;
    }
    return 1;
}




/*
 * Process options of an incoming TCP segment. Currently the only processed option is
 * the MSS option - if that option is detected, the SMSS of the socket is updated
 * Parameter:
 * @socket - the socket
 * @segment - the segment
 */
static void process_options(socket_t* socket, net_msg_t* segment) {
    tcp_hdr_t* tcp_hdr = (tcp_hdr_t*) segment->tcp_hdr;
    u32 opt_bytes;
    u8* options;
    int kind = -1;
    int len;
    /*
     * Return if there are no options to be processed
     * or if option bytes appear unlikely
     */
    opt_bytes = sizeof(u32)*tcp_hdr->hlength - sizeof(tcp_hdr_t);
    options = ((u8*) segment->tcp_hdr) + sizeof(tcp_hdr_t);
    if (0 == opt_bytes)
        return;
    if (opt_bytes + sizeof(tcp_hdr_t) > segment->ip_length) {
        NET_DEBUG("Option length not valid, returning\n");
        return;
    }
    /*
     * Walk options. Recall that for all options, the first byte is the kind
     * and the second byte is the length
     */
    while ((kind) && (options - ((u8*)tcp_hdr) < opt_bytes + sizeof(tcp_hdr_t))) {
        kind = options[0];
        if ((TCP_OPT_KIND_NOP == kind) || (TCP_OPT_KIND_EOD == kind))
            len = 1;
        else
            len = options[1];
        switch (kind) {
            case TCP_OPT_KIND_MSS:
                /*
                 * Only process MSS if this is a SYN and socket is not yet connected
                 */
                if (tcp_hdr->syn && (TCP_STATUS_ESTABLISHED != socket->proto.tcp.status)) {
                    socket->proto.tcp.smss = ntohs(*((u16*)(options+2)));
                    if (socket->proto.tcp.smss > socket->proto.tcp.rmss)
                        socket->proto.tcp.smss = socket->proto.tcp.rmss;
                }
                break;
            default:
                break;
        }
        options += len;
    }

}


/*
 * Process a duplicate acknowledgement in ESTABLISHED state and perform
 * fast recovery / fast retransmit if possible
 * Parameter:
 * @socket - the socket
 * @flags - a pointer to the flags which are later passed to trigger_send
 */
static void process_dup_ack(socket_t* socket, int* flags) {
    tcp_socket_t* tcb = &socket->proto.tcp;
    tcb->dupacks++;
    if ((DUPACK_TRIGGER == tcb->dupacks) && (TCP_OPTIONS_CC & tcb->tcp_options)) {
        /*
         * Invoke fast retransmit, i.e. adapt slow start threshold, set OF_FAST to force
         * retransmission of one segment and adapt congestion window
         */
        tcb->ssthresh = MAX(2*tcb->smss, (tcb->snd_max - tcb->snd_una) / 2);
        tcb->cwnd = tcb->ssthresh + DUPACK_TRIGGER * tcb->smss;
        *flags |= (OF_FAST + OF_FORCE);
        /*
         * Cancel retransmission timer - will be set again by
         * send_segment as we retransmit the lost segment
         */
        tcb->rtx_timer.time = 0;
    }
    if (DUPACK_TRIGGER < tcb->dupacks) {
        /*
         * As the duplicate ACK indicates that one more out-of-order segment has
         * been received by our peer, increase congestion window
         */
        tcb->cwnd += tcb->smss;
    }
}

/*
 * Promote a socket in state SYN_RECEIVED to ESTABLISHED
 * Parameter:
 * @socket - a socket in state SYN_RECEIVED
 * @net_msg - a valid SYN_ACK
 * Locks:
 * lock on parent socket
 */
static void promote_socket(socket_t* socket, net_msg_t* net_msg) {
    u32 eflags;
    if (socket->parent) {
        spinlock_get(&socket->parent->lock, &eflags);
        /*
         * Establish connection - this will also set socket->connected
         * to one
         */
        establish_connection(socket, net_msg);
        /*
         * Inform thread waiting in accept. We do this while still
         * holding the lock to avoid races with net_socket_accept - we
         * want to make sure that this function is not currently scanning
         * the queue
         */
        net_post_event(socket->parent, NET_EVENT_CAN_READ);
        spinlock_release(&socket->parent->lock, &eflags);
    }
    else {
        /*
         * We did not get to this state via a LISTEN, but via a simultaneous open - simply establish
         * connection
         */
        establish_connection(socket, net_msg);
    }

}

/*
 * Update window information and SND_WL1, SND_WL2 from a TCP header
 * Parameter:
 * @socket - the socket to be updated
 * @tcp_hdr - the header containing the window information
 */
static void update_snd_window(socket_t* socket, tcp_hdr_t* tcp_hdr) {
    socket->proto.tcp.snd_wnd = ntohs(tcp_hdr->window);
     if (socket->proto.tcp.snd_wnd > socket->proto.tcp.max_wnd)
         socket->proto.tcp.max_wnd = socket->proto.tcp.snd_wnd;
     socket->proto.tcp.snd_wl1 = ntohl(tcp_hdr->seq_no);
     socket->proto.tcp.snd_wl2 = ntohl(tcp_hdr->ack_no);
}

/*
 * Receive a TCP segment. This function is the core of the TCP input processing. It is rather long, but
 * modelled loosely along the lines of pages 64-75 RFC 793 ("SEGMENT ARRIVES") to improve readability
 * This function takes ownership of the received message and destroys it eventually
 * Parameter:
 * @net_msg - the message
 * Locks:
 * lock on socket
 */
void tcp_rx_msg(net_msg_t* net_msg) {
    u32 eflags;
    int rc;
    struct sockaddr_in laddr;
    struct sockaddr_in faddr;
    tcp_hdr_t* tcp_hdr = (tcp_hdr_t*) net_msg->tcp_hdr;
    socket_t* socket;
    socket_t* new_socket;
    tcp_socket_t* tcb;
    int ack_valid;
    int outflags = 0;
    u32 seq_no;
    u32 ack_no;
    u32 first_byte;
    u32 last_byte;
    int fin;
    int conn_count;
    int ack_ok;
    tcp_options_t options;
    /*
     * Get sequence number and ACK number
     */
    seq_no = ntohl(tcp_hdr->seq_no);
    ack_no = ntohl(tcp_hdr->ack_no);
    NET_DEBUG("[RECEIVING] ACK = %d, SYN = %d, SEQ = %d, ACK_NO = %d, LEN = %d, WIN = %d\n",
            tcp_hdr->ack, tcp_hdr->syn, seq_no,
            ack_no, net_msg->ip_length - sizeof(u32)*tcp_hdr->hlength, ntohs(tcp_hdr->window));
    /*
     * Validate checksum - if the checksum does not match, the packet is discarded
     */
    if (compute_checksum((u16*) tcp_hdr, net_msg->ip_length, net_msg->ip_src, net_msg->ip_dest)) {
        return;
    }
    /*
     * First we need to extract the address quadruple (foreign IP address, foreign port,
     * local IP address, local port) from the message and the TCP header
     */
    laddr.sin_addr.s_addr = net_msg->ip_dest;
    laddr.sin_port = tcp_hdr->dst_port;
    faddr.sin_addr.s_addr = net_msg->ip_src;
    faddr.sin_port = tcp_hdr->src_port;
    socket = locate_socket(&laddr, &faddr);
    if (socket)
        tcb = &socket->proto.tcp;
    if (0 == socket) {
        /*
         * If this is not itself a RST, send a RST in reply
         */
        if (0 == tcp_hdr->rst) {
            send_segment(socket, (tcp_hdr->ack) ? 0 : 1, 0, 1, 0, 0, net_msg, 0, 0, 0, 0, RCV_BUFFER_SIZE, 0);
        }
    }
    else {
        /*
         * Get lock on socket
         */
        spinlock_get(&socket->lock, &eflags);
        NET_DEBUG("Got lock on socket\n");
        /*
         * Process options
         */
        process_options(socket, net_msg);
        /*
         * Further processing depends on current state of socket
         */
        switch (socket->proto.tcp.status) {
            case TCP_STATUS_CLOSED:
                /*
                 * All data in the segment is discarded. An incoming segment containing a RST is discarded.
                 * An incoming segment not containing a RST causes a RST to be sent in response
                 */
                if (0 == tcp_hdr->rst) {
                    send_segment(socket, (tcp_hdr->ack) ? 0 : 1, 0, 1, 0, 0, net_msg, 0, 0, 0, 0, 0, 0);
                }
                break;
            case TCP_STATUS_LISTEN:
                /*
                 * First check for a RST. An incoming segment containing a RST is ignored
                 */
                if (1 == tcp_hdr->rst) {
                    break;
                }
                /*
                 * Second check for an ACK. Form an acceptable RST and return.
                 */
                if (1 == tcp_hdr->ack) {
                    send_segment(socket, 0, 0, 1, 0, 0, net_msg, 0, 0, 0, 0, 0, 0);
                    break;
                }
                /*
                 * Third check for a SYN
                 */
                if ((0 == tcp_hdr->syn) || (1 == tcp_hdr->fin))
                    break;
                /*
                 * This looks like an acceptable SYN. Check whether we have reached the
                 * maximum backlog of queued connections
                 */
                NET_DEBUG("Acceptable SYN - creating new socket\n");
                conn_count = 0;
                LIST_FOREACH(socket->so_queue_head, new_socket) {
                    conn_count++;
                }
                if (conn_count >= socket->max_connection_backlog) {
                    /*
                     * We just ignore the segment in this case, so that the peer
                     * will at some point retransmit the SYN. As it is quite likely that
                     * the application calls accept soon, chances are that when the SYN
                     * is retransmitted, we can process it
                     */
                    break;
                }
                 /*
                 * We now create a new socket which will
                 * be bound to the fully qualified address used by the incoming SYN
                 */
                new_socket = copy_socket(socket, &laddr, &faddr);
                if (0 == new_socket) {
                    NET_DEBUG("Could not create new socket - out of memory\n");
                    break;
                }
                /*
                 * An ISS is is selected, SND_NXT is set to ISS and SND_UNA is set to ISS
                 * (note that when we send the SYN-ACK further below, SND_NXT will be increased
                 * to ISS+1)
                 */
                set_isn(new_socket);
                /*
                 * Set receive MSS for the new socket - this is necessary as the listen socket
                 * might have been bound to INADDR_ANY
                 */
                set_rmss(new_socket);
                /*
                 * Update send window
                 */
                update_snd_window(new_socket, tcp_hdr);
                /*
                 * Process options again for this socket. We need to do this as the options for a SYN
                 * typically contains the MSS which we need to process in the context of the new socket which
                 * is now bound to a specific local address and might therefore have a different MTU
                 */
                process_options(new_socket, net_msg);
                /*
                 *
                 * Set RCV_NXT to SEQ.SEQ_NO + 1. Then set socket status to SYN_RECEIVED.
                 */
                new_socket->proto.tcp.rcv_nxt = seq_no + 1;
                options.mss = new_socket->proto.tcp.rmss;
                new_socket->proto.tcp.status = TCP_STATUS_SYN_RCVD;
                /*
                 * Add new socket to list of TCP sockets - at this point, the socket will be ready
                 * to receive requests. If we cannot add the new socket, drop SYN
                 */
                if ((rc = add_socket_check(new_socket)) < 0) {
                    if (-EINVAL == rc) {
                        NET_DEBUG("Invalid source address in incoming SYN - dropping segment\n");
                    }
                    else
                        NET_DEBUG("Could not add newly created socket - address already in use. Dropping SYN\n");
                    kfree((void*) new_socket);
                    break;
                }
                /*
                 * Add new socket to queue of incoming connection requests. Note that we already own
                 * the lock on the parent socket
                 */
                LIST_ADD_END(socket->so_queue_head, socket->so_queue_tail, clone_socket(new_socket));
                /*
                 * Send SYN_ACK
                 */
                send_segment(new_socket, 1, 1, 0, 0, 0, net_msg, 0, 0, 0, 0, new_socket->proto.tcp.rcv_wnd, &options);
                break;
            case TCP_STATUS_SYN_SENT:
                /*
                 * First check ACK bit and ACK no
                 */
                ack_ok = 0;
                if (tcp_hdr->ack) {
                    /*
                     * If the ACK bit is set:
                     * If SEQ.ACK <= ISS or SEQ.ACK > SND_NXT, send a RST unless the RST bit is set in the incoming
                     * segment
                     */
                    if ((TCP_LEQ(ack_no, tcb->isn)) || (TCP_GT(ack_no, tcb->snd_nxt))) {
                        NET_DEBUG("ACK not acceptable, expected %d, got %d\n", socket->proto.tcp.snd_una, htonl(tcp_hdr->ack_no));
                        if (!tcp_hdr->rst)
                            send_segment(socket, 0, 0, 1, 0, 0, net_msg, 0, 0, 0, 0, socket->proto.tcp.rcv_wnd, 0);
                        return;
                    }
                    ack_ok = 1;
                }
                /*
                 * Second check the reset bit
                 */
                if (tcp_hdr->rst) {
                    /*
                     * If the RST bit is set:
                     * If the ACK was acceptable, then drop the segment, enter CLOSED state, delete TCB and return
                     */
                    if (ack_ok) {
                        socket->proto.tcp.status = TCP_STATUS_CLOSED;
                        unregister_socket(socket);
                    }
                    return;
                }
                /* Check the SYN bit
                 * If we get to this point, there was no ACK or the ACK was ok and no RST has been received
                 */
                if (tcp_hdr->syn) {
                    /*
                     * If the SYN bit is on, RCV_NXT is set to SEQ.SEQ_NO + 1
                     */
                    tcb->rcv_nxt = seq_no + 1;
                    /*
                     * If this acknowledges our SYN, call establish_connection which will
                     * 1) advance SND_UNA
                     * 2) change the connection state to ESTABLISHED
                     * 3) update send window, init retransmission timer and congestion window
                     */
                    if ((tcp_hdr->ack) && (1 == ack_ok)) {
                        establish_connection(socket, net_msg);
                        /*
                         * Then form an ACK segment and sent it
                         */
                        send_segment(socket, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, tcb->rcv_wnd, 0);
                    }
                    /*
                     * Otherwise enter SYN_RECEIVED, form an ACK segment
                     */
                    else {
                        tcb->status = TCP_STATUS_SYN_RCVD;
                        options.mss = socket->proto.tcp.rmss;
                        /*
                         * Reset SND_NXT to ISN
                         */
                        tcb->snd_nxt = tcb->isn;
                        /*
                         * Update send window
                         */
                        update_snd_window(socket, tcp_hdr);
                        /*
                         * and send SYN_ACK
                         */
                        send_segment(socket, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, tcb->rcv_wnd, &options);
                    }
                }
                break;
            case TCP_STATUS_SYN_RCVD:
            case TCP_STATUS_ESTABLISHED:
            case TCP_STATUS_FIN_WAIT_1:
            case TCP_STATUS_FIN_WAIT_2:
            case TCP_STATUS_CLOSE_WAIT:
            case TCP_STATUS_CLOSING:
            case TCP_STATUS_LAST_ACK:
            case TCP_STATUS_TIME_WAIT:
                /*
                 * First check the segment number
                 * If an incoming segment is not acceptable, an ACK is sent in reply (unless
                 * the RST bit is set)
                 */
                if (0 == acceptable(socket, net_msg, &first_byte, &last_byte, &fin)) {
                    if (0 == tcp_hdr->rst)  {
                        send_segment(socket, 1, 0, 0, 0, 0, net_msg, 0, 0, 0, 0, socket->proto.tcp.rcv_wnd, 0);
                    }
                    /*
                     * If we are in TIME_WAIT and the segment was a FIN (possibly retransmitted), set
                     * TIME_WAIT timer again
                     */
                    if (tcp_hdr->fin && (TCP_STATUS_TIME_WAIT == socket->proto.tcp.status)) {
                        socket->proto.tcp.time_wait_timer.time = 2*TCP_MSL;
                    }
                    /*
                     * Drop the segment and return
                     */
                    break;
                }
                /*
                 * second check the RST bit
                 */
                if (tcp_hdr->rst) {
                    switch (socket->proto.tcp.status) {
                        case TCP_STATUS_SYN_RCVD:
                            /*
                             * If this connection is due to a passive open, drop TCB - the parent
                             * connection will still be listening
                             */
                            if (socket->parent) {
                                /*
                                 * Remove socket from list of queued connnections of parent
                                 */
                                remove_queued_connection(socket->parent, socket);
                                socket->proto.tcp.status = TCP_STATUS_CLOSED;
                            }
                            else {
                                /*
                                 * If the connection was initiated with an active open, signal
                                 * connection refused to the user
                                 */
                                socket->error = -ECONNREFUSED;
                            }
                        default:
                            /*
                             * Connection has been reset by peer. Mark socket as no longer usable for
                             * read / write operations, set it to CLOSED and remove it from list of
                             * known TCP sockets
                             */
                            tcb->eof = 1;
                            tcb->epipe = 1;
                            net_post_event(socket, NET_EVENT_CAN_READ | NET_EVENT_CAN_WRITE);
                            socket->proto.tcp.status = TCP_STATUS_CLOSED;
                            unregister_socket(socket);
                            if (-ECONNREFUSED != socket->error)
                                socket->error = -ECONNRESET;
                            break;
                    }
                }
                /*
                 * Check the SYN bit. If we get to this point, the segment was acceptable and hence the SYN is
                 * in the window. Send a reset, enter closed state and delete TCB
                 */
                if (tcp_hdr->syn) {
                    /*
                     * Send reset
                     */
                    send_segment(socket, 0, 0, 1, 0, 0, net_msg, 0, 0, 0, 0, socket->proto.tcp.rcv_wnd, 0);
                    /*
                     * and drop socket
                     */
                    tcb->eof = 1;
                    tcb->epipe = 1;
                    net_post_event(socket, NET_EVENT_CAN_READ | NET_EVENT_CAN_WRITE);
                    socket->proto.tcp.status = TCP_STATUS_CLOSED;
                    unregister_socket(socket);
                    socket->error = -ECONNRESET;
                    break;
                }
                /*
                 * Check the ACK field. If the ACK bit is off, drop the segment and return
                 */
                if (0 == tcp_hdr->ack)
                    break;
                /*
                 * If we get to this point, ACK is set. Process acknowledgement, i.e. remove segments on the
                 * retransmission queue (i.e. octets in the send buffer) which have been acknowledged and increase
                 * SND_UNA
                 */
                ack_valid = process_ack(socket, net_msg);
                /*
                 * If the ACK is not valid and we are in state SYN_RCVD, send reset
                 */
                if ((TCP_STATUS_SYN_RCVD == tcb->status) && (ACK_OK != ack_valid)) {
                    send_segment(socket, 0, 0, 1, 0, 0, net_msg, 0, 0, 0, 0, 0, 0);
                    break;
                }
                if ((ACK_TOOMUCH == ack_valid) && (TCP_STATUS_ESTABLISHED == tcb->status)) {
                    /*
                     * Acknowledgement invalid. Send an ACK and return
                     * this will also advance RCV_NXT over the FIN
                     */
                    trigger_send(socket, OF_NODATA + OF_FORCE);
                    break;
                }
                else if ((ACK_DUP == ack_valid) && (tcb->status == TCP_STATUS_ESTABLISHED)) {
                    /*
                     * Duplicate ACK. Perform fast retransmit / fast recovery as per RFC 2581.
                     */
                    process_dup_ack(socket, &outflags);
                }
                else  {
                    /*
                     * ACK valid. If we are in SYN_RCVD and this acknowledges our reply, establish connection
                     * and inform any threads waiting in an accept on the parent, then continue processing
                     */
                    if ((TCP_STATUS_SYN_RCVD == socket->proto.tcp.status) && (ACK_OK == ack_valid)) {
                        promote_socket(socket, net_msg);
                    }
                    /*
                     * If we are in LAST_ACK and this was the ACK for our FIN, close socket and remove
                     * socket from the list of known sockets
                     */
                    if ((TCP_STATUS_LAST_ACK == tcb->status) &&
                            (TCP_GT(socket->proto.tcp.snd_una, socket->proto.tcp.fin_seq_no))) {
                        socket->proto.tcp.status = TCP_STATUS_CLOSED;
                        unregister_socket(socket);
                        break;
                    }
                    /*
                     * If we are in FIN_WAIT_1 and this acknowledges our FIN, go to FIN_WAIT_2.
                     */
                    if (TCP_STATUS_FIN_WAIT_1 == tcb->status) {
                        if (TCP_GT(socket->proto.tcp.snd_una, socket->proto.tcp.fin_seq_no)) {
                            socket->proto.tcp.status = TCP_STATUS_FIN_WAIT_2;
                        }
                    }
                    /*
                     * If we are in CLOSING and this acknowledges our FIN, go to TIME WAIT
                     */
                    if (TCP_STATUS_CLOSING == socket->proto.tcp.status) {
                        if (TCP_GT(socket->proto.tcp.snd_una, socket->proto.tcp.fin_seq_no)) {
                            socket->proto.tcp.status = TCP_STATUS_TIME_WAIT;
                            socket->proto.tcp.rtx_timer.time = 0;
                            socket->proto.tcp.persist_timer.time = 0;
                            socket->proto.tcp.delack_timer.time = 0;
                            socket->proto.tcp.time_wait_timer.time = 2*TCP_MSL;
                        }
                    }
                    /*
                     * Update send window if this is not an "old" segment
                     * (see RFC 793, page 72)
                     */
                    if ((TCP_LT(tcb->snd_wl1, seq_no))
                            || ((tcb->snd_wl1 == seq_no) && (TCP_LEQ(tcb->snd_wl2, ack_no))))
                    {
                        update_snd_window(socket, tcp_hdr);
                    }
                }
                /*
                 * Process text. If data could not be added as it was not located on the
                 * left edge of the receive window, force sending of a pure ACK.
                 */
                switch (socket->proto.tcp.status) {
                    case TCP_STATUS_ESTABLISHED:
                    case TCP_STATUS_FIN_WAIT_1:
                    case TCP_STATUS_FIN_WAIT_2:
                        /*
                         * Once in the ESTABLISHED state, it is possible to deliver segment text to the
                         * user receive buffer, this is done by process_text. If this function returns 1,
                         * this is an indication that a segment is missing, update the flags in this case
                         * to force sending of a pure ACK
                         */
                        if (process_text(socket, net_msg, first_byte, last_byte, fin)) {
                            outflags |= (OF_FORCE + OF_NODATA);
                        }
                        break;
                    default:
                        break;
                }
                /*
                 * Check the FIN bit
                 */
                if (tcp_hdr->fin) {
                    /*
                     * Note that we do not get to this point if we are in state CLOSED, LISTEN or SYN_SENT.
                     * Send an acknowledgement immediately. Note that RCV_NXT was already advanced by process_text
                     */
                    outflags |= OF_FORCE;
                    /*
                     * In state FIN_WAIT_1, this is a simultaneous close - move to CLOSING
                     * and send ACK. Also set tcb->eof as we do not expect any additional
                     * data from the peer
                     */
                    if (TCP_STATUS_FIN_WAIT_1 == tcb->status) {
                        tcb->status = TCP_STATUS_CLOSING;
                        tcb->eof = 1;
                        outflags |= OF_FORCE;
                    }
                    /*
                     * In state SYN-RECEIVED or ESTABLISHED, move to CLOSE_WAIT and set socket->eof
                     */
                    if ((TCP_STATUS_ESTABLISHED == tcb->status) || (TCP_STATUS_SYN_RCVD == tcb->status)) {
                        tcb->status = TCP_STATUS_CLOSE_WAIT;
                        tcb->eof = 1;
                    }
                    /*
                     * In state FIN_WAIT_2, move to TIME_WAIT and continue processing in that state
                     */
                    if (TCP_STATUS_FIN_WAIT_2 == tcb->status)  {
                        tcb->status = TCP_STATUS_TIME_WAIT;
                        tcb->eof = 1;
                    }
                    /*
                     * In state TIME_WAIT, reset TIME_WAIT timer and turn off all timers
                     */
                    if (TCP_STATUS_TIME_WAIT == tcb->status) {
                        tcb->rtx_timer.time = 0;
                        tcb->delack_timer.time = 0;
                        tcb->persist_timer.time = 0;
                        tcb->time_wait_timer.time = 2*TCP_MSL;
                    }
                }
                /*
                 * Send data if possible
                 */
                NET_DEBUG("Calling trigger_send\n");
                trigger_send(socket, outflags);
                break;
            default:
                break;
        }
        /*
         * End of switch(status). Release lock
         */
        spinlock_release(&socket->lock, &eflags);
    }
    /*
     * Free network message
     */
    NET_DEBUG("Destroying network message\n");
    net_msg_destroy(net_msg);
    /*
     * and release socket again
     */
    NET_DEBUG("Releasing reference on socket\n");
    if (socket)
        tcp_release_socket(socket);
}

/****************************************************************************************
 * The following functions form the public interface of the TCP layer towards the       *
 * upper parts of the networking layer and essentially correspond to the respective     *
 * system calls                                                                         *
 ***************************************************************************************/

/*
 * Send data, i.e. hand data over to the send queue and try to send as much data as possible
 * (this might include data already in the send queue)
 * Parameter:
 * @socket - the socket
 * @buffer - pointer to data
 * @len - number of bytes in buffer
 * @flags - flags, currently not used
 * Return value:
 * number of bytes copied to the send buffer on success
 * -EPIPE if the connection is closed for sending
 * -EAGAIN if there is no space left in the sockets send buffer
 */
static int tcp_send(socket_t* socket, void* buffer, unsigned int len, int flags) {
    u32 bytes;
    int i;
    /*
     * If connection can no longer accept data for sending, signal EPIPE
     */
    if (socket->proto.tcp.epipe){
        return -EPIPE;
    }
    /*
     * Is there any space left in the buffer? If no, return -EAGAIN to inform caller
     * that it needs to wait
     */
    if (socket->proto.tcp.snd_buffer_tail - socket->proto.tcp.snd_buffer_head == SND_BUFFER_SIZE) {
        return -EAGAIN;
    }
    /*
     * Determine number of bytes available in send buffer
     */
    if (SND_BUFFER_SIZE == socket->proto.tcp.snd_buffer_tail - socket->proto.tcp.snd_buffer_head)
        bytes = 0;
    else
        bytes = SND_BUFFER_SIZE - ((socket->proto.tcp.snd_buffer_tail - socket->proto.tcp.snd_buffer_head) % SND_BUFFER_SIZE);
    if (bytes > len)
        bytes = len;
    /*
     * Copy as many bytes as we can into the buffer, starting at current tail
     */
    for (i = 0; i < bytes; i++) {
        socket->proto.tcp.snd_buffer[socket->proto.tcp.snd_buffer_tail % SND_BUFFER_SIZE] = ((u8*)buffer)[i];
        socket->proto.tcp.snd_buffer_tail++;
    }
    /*
     * Call trigger_send to send data in the buffer if possible
     */
    trigger_send(socket, 0);
    return bytes;
}

/*
 * For TCP sockets, sendto is just like send with the additional arguments being ignored - we do not even return EISCONN
 * if they are not NULL. This is in line with POSIX, but differs from what other implementations (Linux) do.
 */
static int tcp_sendto(socket_t* socket, void* buffer, unsigned int len, int flags, struct sockaddr* addr, u32 addrlen) {
    return tcp_send(socket, buffer, len, flags);
}

/*
 * Connect a TCP socket
 * Parameter:
 * @socket - the socket
 * @addr - the address to which we connect
 * @addrlen - the length of the address parameter
 * Return value:
 * 0 - operation successful
 * -EINVAL - address length invalid
 * -EAGAIN - connection initiated, but threads needs to wait
 * -ENOMEM - no memory for network message
 * -ENETUNREACH - destination network unreachable
 * -EADDRINUSE - no free local port number could be found
 */
static int tcp_connect(socket_t* socket, struct sockaddr* addr, int addrlen) {
    struct sockaddr_in* laddr;
    u32 ip_dst;
    int rc;
    tcp_options_t options;
    /*
     * Verify length of address argument
     */
    if (sizeof(struct sockaddr_in) != addrlen)
        return -EINVAL;
    /*
     * If the socket is not closed, return -EISCONN, as a connection
     * has already been initiated
     */
    if (TCP_STATUS_CLOSED != socket->proto.tcp.status)
        return -EISCONN;
    /*
     * Set initial sequence number
     */
    set_isn(socket);
    /*
     * Set local address if the socket is not yet bound or if the local address is INADDR_ANY
     * Note that we need to set a valid local address before sending the first SYN segment as the source IP address
     * is part of the connection quadruple and - via the TCP pseudo header - indirectly contained in the TCP checksum
     */
    ip_dst = ((struct sockaddr_in*) addr)->sin_addr.s_addr;
    laddr = (struct sockaddr_in*) &socket->laddr;
    if ((0 == socket->bound) || (INADDR_ANY == laddr->sin_addr.s_addr)) {
        if (set_local_address(socket, ip_dst))
            return -ENETUNREACH;
        socket->bound = 1;
    }
    /*
     * Set foreign address
     */
    socket->faddr = *addr;
    /*
     * Send TCP SYN, including MSS option
     */
    options.mss = socket->proto.tcp.rmss;
    rc = send_segment(socket, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, socket->proto.tcp.rcv_wnd, &options);
    if (rc)
        return rc;
    /*
     * Update status
     */
    socket->proto.tcp.status = TCP_STATUS_SYN_SENT;
    /*
     * and instruct caller to wait. Now the calling thread should go to sleep and only wake up if an event
     * occurs on the condition variable socket->state_change. This event is raised by the receiver thread if a
     * SYN-ACK arrives
     */
    return -EAGAIN;
}


/*
 * Receive data, i.e. try to copy data from the sockets receive buffer into a user
 * provided buffer. Then try to send data if possible, including a window update
 * Parameter:
 * @socket - the socket
 * @buf - a pointer to the user provided buffer
 * @len - number of bytes to read
 * @flags - currently not used
 * Return values:
 * the number of bytes retrieved upon success
 * -ENOTCONN if the socket is not connected
 * -EAGAIN if there is no data in the receive buffer, but the sockets EOF flag is
 *  not set
 * 0 if there is no data in the receive buffer and the sockets EOF flag is set
 */
static int tcp_recv(socket_t* socket, void *buf, unsigned int len, int flags) {
    tcp_socket_t* tcb = &socket->proto.tcp;
    int i;
    u32 bytes;
    /*
     * Make sure that we are connected
     */
    if (TCP_STATUS_ESTABLISHED > tcb->status) {
        return -ENOTCONN;
    }
    /*
     * If we have timed out, return -ETIMEDOUT
     */
    if (tcb->timeout) {
        return -ETIMEDOUT;
    }
    /*
     * If there is no data in the sockets receive queue, return -EAGAIN or
     * EOF
     */
    if (tcb->rcv_buffer_head == tcb->rcv_buffer_tail) {
        NET_DEBUG("No data in receive buffer\n");
        if (tcb->eof)
            return 0;
        return -EAGAIN;
    }
    /*
     * Determine number of bytes to get from buffer
     */
    bytes = MIN(len, tcb->rcv_buffer_tail - tcb->rcv_buffer_head);
    /*
     * and copy data
     */
    for (i = 0; i < bytes; i++) {
        ((u8*)buf)[i] = tcb->rcv_buffer[(tcb->rcv_buffer_head + i) % RCV_BUFFER_SIZE];
    }
#ifdef TCP_DUMP_IN
    PRINT("%d@%s (%s): Copied %d bytes of data to user supplied buffer, flags = %d\n", __LINE__, __FILE__, __FUNCTION__, bytes, flags);
    dump_ringbuffer(buf, len + 1, 0, bytes);
#endif
    /*
     * Adjust head of queue unless MSG_PEEK is specified
     */
    if (0 == (flags & MSG_PEEK)) {
        tcb->rcv_buffer_head += bytes;
        /*
         * As this might have increased the window, call trigger_send to make sure
         * that a window update is sent if required
         */
        trigger_send(socket, 0);
    }
    return bytes;
}


/*
 * Receive data, i.e. try to copy data from the sockets receive buffer into a user
 * provided buffer. Then try to send data if possible, including a window update
 * Parameter:
 * @socket - the socket
 * @buf - a pointer to the user provided buffer
 * @len - number of bytes to read
 * @flags - currently not used
 * @addr - here the address of the peer will be stored
 * @addrlen - length of address field, will be updated with actual size of address
 * Return values:
 * the number of bytes retrieved upon success
 * -ENOTCONN if the socket is not connected
 * -EAGAIN if there is no data in the receive buffer, but the sockets EOF flag is
 *  not set
 * -EINVAL if address is not NULL but addrlen is NULL
 * 0 if there is no data in the receive buffer and the sockets EOF flag is set
 */
static int tcp_recvfrom(socket_t* socket, void* buffer, unsigned int len, int flags, struct sockaddr* addr, u32* addrlen) {
    int rc;
    /*
     * First do actual receive operation
     */
    rc = tcp_recv(socket, buffer, len, flags);
    /*
     * If we have an error return now
     */
    if (rc < 0)
        return rc;
    /*
     * Otherwise take care of address if required
     */
    if (addr) {
        if (0 == addrlen)
            return -EINVAL;
        /*
         * Copy address from socket
         */
        memcpy((void*) addr, (void*) &socket->faddr, MIN(sizeof(struct sockaddr_in), *addrlen));
        *addrlen = sizeof(struct sockaddr_in);
    }
    return rc;
}

/*
 * Listen on a socket, i.e. put the socket in LISTEN state
 * Parameter:
 * @socket - the socket
 * Return value:
 * 0 upon success
 * -EADDRINUSE if the socket is not yet bound and no free local port could be found
 * Locks:
 * lock on socket list
 */
static int tcp_listen(socket_t* socket) {
    u32 eflags;
    int port;
    /*
     * Get lock on socket list
     */
    spinlock_get(&socket_list_lock, &eflags);
    /*
     * If socket is not yet bound, determine a free local port number
     */
    if (0 == socket->bound) {
        port = find_free_port();
        if (-1 == port) {
            spinlock_release(&socket_list_lock, &eflags);
            return -EADDRINUSE;
        }
        socket->bound = 1;
        ((struct sockaddr_in*) &socket->laddr)->sin_port = ntohs(port);
    }
    /*
     * Release lock on socket list
     */
    spinlock_release(&socket_list_lock, &eflags);
    /*
     * Put socket into listen state
     */
    socket->proto.tcp.status = TCP_STATUS_LISTEN;
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
static int tcp_bind(socket_t* socket, struct sockaddr* address, int addrlen) {
    u32 eflags;
    struct sockaddr_in* laddr;
    struct sockaddr_in* socket_addr;
    tcp_socket_t* other;
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
     * Check whether address is already in use. Note that as we use wildcards for foreign address
     * and foreign port number, this check will not permit us to bind a combination of local IP
     * address and port number already in use by any other TCP socket
     */
    else {
        other = get_matching_tcb(laddr->sin_addr.s_addr, INADDR_ANY, ntohs(port), 0);
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
     * Release lock on socket list
     */
    spinlock_release(&socket_list_lock, &eflags);
    /*
     * Determine receive MSS
     */
    set_rmss(socket);
    return 0;
}

/*
 * Close a socket. We assume that the caller holds the lock on the socket
 * Parameter:
 * @socket - the socket
 * @eflags - location of stored EFLAGS used to acquire the lock
 */
int tcp_close(socket_t* socket, u32* eflags) {
    switch(socket->proto.tcp.status) {
        case TCP_STATUS_ESTABLISHED:
        case TCP_STATUS_CLOSE_WAIT:
        case TCP_STATUS_SYN_RCVD:
            /*
             * Mark socket as no longer being able to read and write data. This avoids
             * that new data is added to the receive queue by incoming messages and that
             * new data is added to the send queue by the send system call
             */
            socket->proto.tcp.eof = 1;
            socket->proto.tcp.epipe = 1;
            /*
             * Set flag in socket structure indicating that socket has been closed and
             * call trigger_send. The flag will be checked by trigger_send to determine
             * whether a FIN bit will be added. As soon as the FIN has been sent,
             * send_segment will update the status of the socket
             */
            socket->proto.tcp.closed = 1;
            trigger_send(socket, 0);
            break;
        default:
            /*
             * Simply delete TCB
             */
            drop_socket(socket, eflags);
            break;
    }
    return 0;
}

/*
 * Initialize the TCP module
 */
void tcp_init() {
    /*
     * Initialize socket list
     */
    socket_list_head = 0;
    socket_list_tail = 0;
    spinlock_init(&socket_list_lock);
}


/*
 * Given a socket, check the socket state and return either 0 or a combination of
 * the bitmasks 0x1 (NET_EVENT_CAN_READ) and 0x2 (NET_EVENT_CAN_WRITE), depending on the current state
 * of the socket. We assume that caller holds the lock.
 * Parameter:
 * @socket -t the socket
 * @read - check whether socket is ready to read
 * @write - check whether socket is ready to write
 * Return value:
 * a combination of NET_EVENT_CAN_READ and NET_EVENT_CAN_WRITE
 * Note that we do not check whether the socket is connected, as we consider a socket "ready
 * for reading / writing" if the respective system call would not block, regardless of whether
 * the transfer would succeed
 */
static int tcp_select(socket_t* socket, int read, int write) {
    int rc = 0;
    tcp_socket_t* tcb = &socket->proto.tcp;
    /*
     * If requested, check whether there is data in the receive queue
     */
    if (read) {
        if (tcb->rcv_buffer_head != tcb->rcv_buffer_tail) {
            rc += NET_EVENT_CAN_READ;
        }
    }
    /*
     * Same for writing
     */
    if (write) {
        if (socket->proto.tcp.snd_buffer_tail - socket->proto.tcp.snd_buffer_head != SND_BUFFER_SIZE) {
            rc += NET_EVENT_CAN_WRITE;
        }
    }
    return rc;
}

/****************************************************************************************
 * Handle the various timers a socket can be connected to                               *
 ***************************************************************************************/


/*
 * This function is called whenever the retransmission timer of a socket
 * expires. If the maximum number of retries is exceeded, the socket is dropped,
 * otherwise a retransmission is initiated
 * Parameter:
 * @socket - the socket
 * @eflags - the caller needs to hold the lock on the socket, this is the eflags field used
 */
static void rtx_expired(socket_t* socket, u32* eflags) {
    tcp_socket_t* tcb = &socket->proto.tcp;
    tcp_options_t options;
    int status = tcb->status;
    NET_DEBUG("Retransmission timer has expired\n");
    switch (status) {
        case TCP_STATUS_ESTABLISHED:
        case TCP_STATUS_CLOSE_WAIT:
        case TCP_STATUS_FIN_WAIT_1:
        case TCP_STATUS_FIN_WAIT_2:
        case TCP_STATUS_LAST_ACK:
        case TCP_STATUS_CLOSING:
        case TCP_STATUS_TIME_WAIT:
            /*
             * If we exceed the treshold for retransmissions, give up
             * unless we are doing window probes
             */
            if ((socket->proto.tcp.rtx_count >= TCP_MAX_RTX) && (socket->proto.tcp.snd_wnd)) {
                /*
                 * Reset connection
                 */
                send_segment(socket, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0);
                /*
                 * and mark timeout
                 */
                socket->proto.tcp.timeout = 1;
                socket->error = -ETIMEDOUT;
                /*
                 * Now drop socket
                 */
                drop_socket(socket, eflags);
            }
            else {
                /*
                 * Prepare for retransmission. First increase backoff
                 */
                if (tcb->rtx_timer.backoff < TCP_MAX_BACKOFF)
                    tcb->rtx_timer.backoff++;
                /*
                 * If congestion control is enabled, set congestion window
                 * back to initial value and adjust slow start threshold
                 */
                if (tcb->tcp_options & TCP_OPTIONS_CC) {
                    tcb->ack_count = 0;
                    tcb->cwnd = CWND_IW * tcb->smss;
                    tcb->dupacks = 0;
                    tcb->ssthresh = MAX((tcb->snd_max - tcb->snd_una) / 2, 2*tcb->smss);
                }
                /*
                 * Set snd_nxt back to snd_una
                 */
                tcb->snd_nxt = tcb->snd_una;
                /*
                 * and call trigger_send. This will perform the actual retransmission and,
                 * at the same time, reset the retransmission timer with the new value
                 * of the backoff factor
                 */
                trigger_send(socket, OF_FORCE);
            }
            break;
        case TCP_STATUS_SYN_SENT:
        case TCP_STATUS_SYN_RCVD:
            /*
             * SYN or SYN_ACK has timed out. Set SND_NXT back to ISN and send one more SYN/SYN_ACK
             * if we are under the SYN retry threshold, otherwise send RST and give
             * up
             */
            if (socket->proto.tcp.rtx_count < SYN_MAX_RTX) {
                socket->proto.tcp.snd_nxt = socket->proto.tcp.isn;
                socket->proto.tcp.snd_una = socket->proto.tcp.isn;
                options.mss = socket->proto.tcp.rmss;
                if (TCP_STATUS_SYN_SENT == socket->proto.tcp.status)
                    send_segment(socket, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, socket->proto.tcp.rcv_wnd, &options);
                else
                    send_segment(socket, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, socket->proto.tcp.rcv_wnd, &options);
                /*
                 * Apply backoff and set timer again
                 */
                tcb->rtx_timer.backoff++;
                tcb->rtx_timer.time = SYN_TIMEOUT << tcb->rtx_timer.backoff;
            }
            else {
                /*
                 * Reset connection
                 */
                if (TCP_STATUS_SYN_SENT == socket->proto.tcp.status)
                    send_segment(socket, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0);
                else
                    send_segment(socket, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0);
                /*
                 * and mark timeout
                 */
                socket->proto.tcp.timeout = 1;
                socket->error = -ETIMEDOUT;
                drop_socket(socket, eflags);
            }
            break;
        default:
            break;
    }
}

/*
 * Process a TCP timer tick for a particular socket
 * Parameter:
 * @socket - the socket
 * Locks:
 * lock on socket
 */
static void process_timers(socket_t* socket) {
    u32 eflags;
    tcp_socket_t* tcb = &socket->proto.tcp;
    /*
     * Lock socket
     */
    spinlock_get(&socket->lock, &eflags);
    /*
     * We need to be prepared for the case that the socket is closed
     * as we work with a temporary copy of the socket list
     */
    if (TCP_STATUS_CLOSED == tcb->status) {
        spinlock_release(&socket->lock, &eflags);
        return;
    }
    /*
     * Update RTT
     */
    if (RTT_NONE != socket->proto.tcp.current_rtt)
        socket->proto.tcp.current_rtt++;
    /*
     * Process retransmission timer if set
     */
    if (tcb->rtx_timer.time) {
        tcb->rtx_timer.time--;
        if (0 == tcb->rtx_timer.time) {
            NET_DEBUG("Retransmission timer is zero for socket %x\n", socket);
            rtx_expired(socket, &eflags);
        }
    }
    /*
     * Process delayed ACK timer
     */
    if (tcb->delack_timer.time) {
        tcb->delack_timer.time--;
        if (0 == tcb->delack_timer.time) {
            trigger_send(socket, OF_FORCE);
        }
    }
    /*
     * Process persist timer
     */
    if (tcb->persist_timer.time) {
        tcb->persist_timer.time--;
        if (0 == tcb->persist_timer.time) {
            NET_DEBUG("Persist timer fired\n");
            tcb->rtx_timer.backoff++;
            trigger_send(socket, OF_FORCE);
            NET_DEBUG("Persist timer done\n");
        }
    }
    /*
     * Process time wait timer
     */
    if (tcb->time_wait_timer.time) {
        tcb->time_wait_timer.time--;
        if (0 == tcb->time_wait_timer.time) {
            NET_DEBUG("Time wait timer expired\n");
            drop_socket(socket, &eflags);
        }
    }
    /*
     * Release lock again
     */
    spinlock_release(&socket->lock, &eflags);
}

/*
 * TCP timer ticks. This function needs to be called by the timer module
 * every 250 ms
 * Locks:
 * lock on socket list
 */
void tcp_do_tick() {
    u32 eflags;
    int i;
    int count = 0;
    tcp_socket_t* tcb;
    socket_t* sockets[MAX_TCP_SOCKETS];
    /*
     * In order to avoid deadlocks, we first create a copy of the current socket list and then
     * work with that copy. This might imply that individual sockets are missing this tick
     * or that we process a tick for a socket which has just been closed - so be prepared for
     * that
     */
    spinlock_get(&socket_list_lock, &eflags);
    LIST_FOREACH(socket_list_head, tcb) {
        sockets[count] = clone_socket(TCB2SOCK(tcb));
        count++;
        if (count > MAX_TCP_SOCKETS - 1) {
            ERROR("Too many TCP sockets, ignoring remaining sockets for this tick\n");
            break;
        }
    }
    spinlock_release(&socket_list_lock, &eflags);
    /*
     * Now process actual list. Whenever we are done with one socket, drop that
     * reference again
     */
    for (i = 0; i < count; i++) {
        tcb = &sockets[i]->proto.tcp;
        process_timers(TCB2SOCK(tcb));
        tcp_release_socket(TCB2SOCK(tcb));
    }
}



/****************************************************************************************
 * Everything below this line is for debugging only                                     *
 ***************************************************************************************/

/*
 * Print existing sockets
 */
int tcp_print_sockets() {
    tcp_socket_t* tcb;
    struct sockaddr_in* laddr;
    struct sockaddr_in* faddr;
    int count = 0;
    PRINT("\n");
    PRINT("Local       Foreign      Local    Foreign    State\n");
    PRINT("IP addr.    IP addr.     port     port\n");
    PRINT("----------------------------------------------------------\n");
    LIST_FOREACH(socket_list_head, tcb) {
        laddr = (struct sockaddr_in*) &TCB2SOCK(tcb)->laddr;
        faddr = (struct sockaddr_in*) &TCB2SOCK(tcb)->faddr;
        count++;
        PRINT("%x   %x    %.5d    %.5d      ", laddr->sin_addr.s_addr, faddr->sin_addr.s_addr,
                ntohs(laddr->sin_port), ntohs(faddr->sin_port));
        switch(tcb->status) {
            case TCP_STATUS_CLOSED:
                PRINT("CLOSED\n");
                break;
            case TCP_STATUS_CLOSE_WAIT:
                PRINT("CLOSE_WAIT\n");
                break;
            case TCP_STATUS_ESTABLISHED:
                PRINT("ESTABLISHED\n");
                break;
            case TCP_STATUS_FIN_WAIT_1:
                PRINT("FIN_WAIT_1\n");
                break;
            case TCP_STATUS_FIN_WAIT_2:
                PRINT("FIN_WAIT_2\n");
                break;
            case TCP_STATUS_TIME_WAIT:
                PRINT("TIME_WAIT\n");
                break;
            case TCP_STATUS_LAST_ACK:
                PRINT("LAST_ACK\n");
                break;
            case TCP_STATUS_LISTEN:
                PRINT("LISTEN\n");
                break;
            case TCP_STATUS_SYN_RCVD:
                PRINT("SYN_RECEIVED\n");
                break;
            case TCP_STATUS_SYN_SENT:
                PRINT("SYN_SENT\n");
                break;
            default:
                PRINT("UNKNOWN (%d)\n", tcb->status);
                break;

        }
    }
    return count;
}
