/*
 * socket.c
 *
 *  Created on: Jun 11, 2012
 *      Author: chr
 */


#include "lib/os/oscalls.h"
#include "lib/errno.h"
#include "lib/sys/socket.h"

/*
 * Create a socket
 *
 * This function will create an unbound socket and return the associated file descriptor.
 *
 * Arguments:
 *
 * @domain - this is the address family (for instance AF_INET) defined in sys/socket.h
 * @type - type of socket
 * @proto - the protocol
 *
 * The type of the socket can either be
 *
 * SOCK_STREAM - a stream and connection oriented socket
 * SOCK_DGRAM - a connection-less datagram oriented socket
 * SOCK_RAW - a raw IP socket
 *
 * If proto is zero, the default protocol for address family and type is used (for instance TCP for SOCK_STREAM and AF_INET)
 *
 */
int socket(int domain, int type, int proto) {
    int res;
    res = __ctOS_socket(domain, type, proto);
    if (res >= 0)
        return res;
    errno = -res;
    return -1;
}


/*
 * Connect a socket
 *
 * For datagram sockets, set the peer address which will determine the destination address for future send operations and
 * will be used as filter for future recv operations
 *
 * For connection-oriented sockets, try to establish a connection
 *
 * If the socket has not yet been bound to a local address using bind, assign an available local address to the socket, using
 * INADDR_ANY as local IP address
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * 1) O_NONBLOCK is not supported and asynchronous completion of connections is not supported
 * 2) timeouts are not supported
 */
int connect(int socket, const struct sockaddr* address, socklen_t address_len) {
    int res;
    res = __ctOS_connect(socket, address, address_len);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return 0;
}

/*
 * Send data over a socket
 */
ssize_t send(int fd, void* buffer, size_t len, int flags) {
    int res;
    res = __ctOS_send(fd, buffer, len, flags);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

ssize_t sendto(int fd, void* buffer, size_t len, int flags, struct sockaddr* addr, socklen_t addrlen) {
    int res;
    res = __ctOS_sendto(fd, buffer, len, flags, addr, addrlen);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

/*
 * Read data over a socket
 */
ssize_t recv(int fd, void* buffer, size_t len, int flags) {
    int res;
    res = __ctOS_recv(fd, buffer, len, flags);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

ssize_t recvfrom(int fd, void* buffer, size_t len, int flags, struct sockaddr* addr, socklen_t* addrlen) {
    int res;
    res = __ctOS_recvfrom(fd, buffer, len, flags, addr, addrlen);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}



/*
 * Bind a socket to a local address
 */
int bind(int fd, const struct sockaddr *address,  socklen_t address_len) {
    int res;
    res = __ctOS_bind(fd, address, address_len);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

/*
 * Put a socket into listen state
 *
 * If the socket is not bound to a local address yet, but is a connection-oriented socket, a local port will be chosen
 */
int listen(int fd, int backlog) {
    int res;
    res = __ctOS_listen(fd, backlog);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

/*
 * Accept incoming connections on a listening socket
 */
int accept(int fd, struct sockaddr* addr, socklen_t* len) {
    int res;
    res = __ctOS_accept(fd, addr, len);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}


/*
 * Select
 */
int select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, struct timeval* timeout) {
    int res;
    res = __ctOS_select(nfds, readfds, writefds, exceptfds, timeout);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

/*
 * Set socket options
 *
 * Currently the only level supported is SOL_SOCKET, i.e. the socket level, and the only supported options are
 *
 * SO_RCVTIMEO - receive timeout, specified as a timeval structure
 * SO_SNDTIMEO - send timeout, specified as a timeval structure
 *
 *
 */
int setsockopt(int socket, int level, int option_name, const void *option_value, socklen_t option_len) {
    int res;
    res = __ctOS_setsockopt(socket, level, option_name, option_value, option_len);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return res;
}

/*
 * Implementation of POSIX gesthostbyname. This function returns a pointer to a hostent structure which contains
 * an address of type AF_INET for the host with the specified name.  If no record was found for the specified host,
 * NULL is returned.
 *
 * BASED ON:
 *
 * POSIX 2004
 *
 * LIMITATIONS:
 *
 * none
 */
struct hostent* gethostbyname(const char* name) {
    return __ctOS_gethostbyname(name);
}

/*
 * Get local address of a socket
 *
 * This function will retrieve the local address from the specified socket and copy it to the sockaddr structure
 * pointed to by the second argument. At most *addrlen bytes will be copied, and *addrlen will be updated with the
 * actual length of the address.
 *
 * If the socket is not bound to a local address, the operation will complete, but the result is not meaningful.
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * none
 */
int getsockname(int fd, struct sockaddr* address, socklen_t* addrlen) {
    int res;
    res = __ctOS_getsockaddr(fd, address, 0, addrlen);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return 0;
}

/*
 * Get foreign address of a socket
 *
 * This function will retrieve the foreign address from the specified socket and copy it to the sockaddr structure
 * pointed to by the second argument. At most *addrlen bytes will be copied, and *addrlen will be updated with the
 * actual length of the address.
 *
 * If no foreign address has been specified, either by using the connect system call or by accepting an incoming
 * connection from a peer, -ENOTCONN is returned.
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * none
 */
int getpeername(int fd, struct sockaddr* address, socklen_t* addrlen) {
    int res;
    res = __ctOS_getsockaddr(fd, 0, address, addrlen);
    if (res < 0) {
        errno = -res;
        return -1;
    }
    return 0;
}
