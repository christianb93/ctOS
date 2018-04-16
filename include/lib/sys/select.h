/*
 * select.h
 */

#ifndef _SYS_SELECT_H
#define _SYS_SELECT_H

/*
 * Get timeval structure
 */
#include "time.h"

typedef long int __fd_mask;

#ifndef NFDBITS
#define NFDBITS 32
#endif

#ifndef FD_SETSIZE
#define FD_SETSIZE 1024
#endif

typedef struct {
    __fd_mask __fds_bits[FD_SETSIZE / NFDBITS];
} fd_set;

#define FD_ZERO(fdset)  do { int i; for (i = 0; i < FD_SETSIZE / NFDBITS; i++) (fdset)->__fds_bits[i] = 0; } while (0)
#define FD_CLR(fd, fdset)   ((fdset)->__fds_bits[ (fd) / NFDBITS] &= ~((1 << ((fd)  % NFDBITS))))
#define FD_SET(fd, fdset)   ((fdset)->__fds_bits[ (fd) / NFDBITS] |= (1 << ((fd)  % NFDBITS)))
#define FD_ISSET(fd, fdset) ((((fdset)->__fds_bits[ (fd) / NFDBITS] >> ((fd) % NFDBITS)) & 0x1))

int select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, struct timeval *timeout);

#endif /* _SYS_SELECT_H */
