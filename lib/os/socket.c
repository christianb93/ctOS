/*
 * socket.c
 *
 */


#include "lib/os/syscalls.h"
#include "lib/sys/socket.h"

/*
 * Create a new socket and return the associated file descriptor
 */
int __ctOS_socket(int domain, int type, int proto) {
    return __ctOS_syscall(__SYSNO_SOCKET, 3, domain, type, proto);
}

/*
 * Connect a socket
 */
int __ctOS_connect(int fd, const struct sockaddr* address, socklen_t addrlen) {
    return __ctOS_syscall(__SYSNO_CONNECT, 3, fd, address, addrlen);
}

/*
 * Send data over a socket
 */
ssize_t __ctOS_send(int fd, void* buffer, size_t len, int flags) {
    return __ctOS_syscall(__SYSNO_SEND, 4, fd, buffer, len, flags);
}

ssize_t __ctOS_sendto(int fd, void* buffer, size_t len, int flags, struct sockaddr* addr, socklen_t addrlen) {
    return __ctOS_syscall(__SYSNO_SENDTO, 6, fd, buffer, len, flags, addr, addrlen);
}

/*
 * Read data from a socket
 */
ssize_t __ctOS_recv(int fd, void* buffer, size_t len, int flags) {
    return __ctOS_syscall(__SYSNO_RECV, 4, fd, buffer, len, flags);
}

ssize_t __ctOS_recvfrom(int fd, void* buffer, size_t len, int flags, struct sockaddr* addr, socklen_t* addrlen) {
    return __ctOS_syscall(__SYSNO_RECVFROM, 6, fd, buffer, len, flags, addr, addrlen);
}

/*
 * Put socket into listen state
 */
int __ctOS_listen(int fd, int backlog) {
    return __ctOS_syscall(__SYSNO_LISTEN, 2, fd, backlog);
}

/*
 * Bind socket to a local address
 */
int __ctOS_bind(int fd, const struct sockaddr *address,  socklen_t address_len) {
    return __ctOS_syscall(__SYSNO_BIND, 3, fd, address, address_len);
}

/*
 * Accept incoming connections
 */
int __ctOS_accept(int fd, struct sockaddr* addr, socklen_t* len) {
    return __ctOS_syscall(__SYSNO_ACCEPT, 3, fd, addr, len);
}

/*
 * Select
 */
int __ctOS_select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, struct timeval* timeout) {
    return __ctOS_syscall(__SYSNO_SELECT, 5, nfds, readfds, writefds, exceptfds, timeout);
}


/*
 * Set socket options
 */
int __ctOS_setsockopt(int socket, int level, int option_name, const void *option_value, socklen_t option_len) {
    return __ctOS_syscall(__SYSNO_SETSOOPT, 5, socket, level, option_name, option_value, option_len);
}

/*
 * Get socket addresses
 */
int __ctOS_getsockaddr(int socket, struct sockaddr* laddr, struct sockaddr* faddr, socklen_t* addrlen) {
    return __ctOS_syscall(__SYSNO_GETSOCKADDR, 4, socket, laddr, faddr, addrlen);
}
