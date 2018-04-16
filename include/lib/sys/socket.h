/*
 * socket.h
 *
 *  Created on: Feb 25, 2012
 *      Author: chr
 */

#ifndef _SYS_SOCKET_H_
#define _SYS_SOCKET_H_

#include "types.h"

#include "select.h"


#ifndef _SA_FAMILY_T_DEFINED
#define _SA_FAMILY_T_DEFINED
typedef unsigned short sa_family_t;
#endif

/*
 * This needs to be enlarged once we have anything else than TCP over IPv4
 */
struct sockaddr {
    sa_family_t sa_family;
    char sa_data[6];
};

#ifndef _SOCKLEN_T_DEFINED
#define _SOCKLEN_T_DEFINED
typedef unsigned int socklen_t;
#endif

#define AF_INET 2


#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define SOCK_RAW 3

/*
 * Socket options
 */
#define SOL_SOCKET 1
#define SO_RCVBUF 8
#define SO_RCVTIMEO 20
#define SO_SNDTIMEO 21

#define MSG_PEEK 0x1

int socket(int domain, int type, int proto);
int connect(int socket, const struct sockaddr* address, socklen_t address_len);
ssize_t send(int fd, void* buffer, size_t len, int flags);
ssize_t sendto(int fd, void* buffer, size_t len, int flags, struct sockaddr* addr, socklen_t addrlen);
ssize_t recv(int fd, void* buffer, size_t len, int flags);
ssize_t recvfrom(int fd, void* buffer, size_t len, int flags, struct sockaddr* addr, socklen_t* addrlen);
int listen(int fd, int backlog);
int bind(int fd, const struct sockaddr *address,  socklen_t address_len);
int accept(int fd, struct sockaddr* addr, socklen_t* len);
int setsockopt(int socket, int level, int option_name, const void *option_value, socklen_t option_len);
int getsockname(int socket, struct sockaddr* address, socklen_t* address_len);
int getpeername(int fd, struct sockaddr* address, socklen_t* addrlen);

#endif /* _SYS_SOCKET_H_ */

