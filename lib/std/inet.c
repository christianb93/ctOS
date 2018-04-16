/*
 * inet.c
 *
 */

#include "lib/arpa/inet.h"
#include "lib/netinet/in.h"
#include "lib/errno.h"
#include "lib/stdio.h"

/*
 * inet_ntop
 *
 * Convert a numeric network address into printable format.
 * Parameters:
 * @af - address family (currently only AF_INET is supported)
 * @src - a pointer to a 32 bit unsigned integer holding an IPv4 address in network byte order
 * @dst - buffer for result
 * @size - size of buffer
 * Return value:
 * pointer to buffer on success
 * 0 otherwise
 */

const char* inet_ntop(int af, const void* src, char* dst, socklen_t size) {
    unsigned int ip_address;
    if (0 == src)
        return 0;
    ip_address = *((unsigned int*) src);
    /*
     * Check address family
     */
    if (AF_INET != af) {
        errno = -EAFNOSUPPORT;
        return 0;
    }
    /*
     * Check argument length
     */
    if ((0 == dst) || (size < INET_ADDRSTRLEN)) {
        errno = -ENOSPC;
        return 0;
    }
    snprintf(dst, size, "%d.%d.%d.%d", ip_address & 0xFF, (ip_address >> 8) & 0xFF, (ip_address >> 16) & 0xFF, (ip_address >> 24) & 0xFF);
    return dst;
}

/*
 * Convert an IPv4 address in network byte order into a string representation
 */
static char inet_ntoa_result[INET_ADDRSTRLEN + 1];
char *inet_ntoa(struct in_addr in) {
    unsigned int ip_addr = in.s_addr;
    snprintf(inet_ntoa_result, INET_ADDRSTRLEN + 1, "%d.%d.%d.%d", ip_addr & 0xFF, (ip_addr >> 8) & 0xFF,
            (ip_addr >> 16) & 0xFF, ip_addr >> 24 );
    return inet_ntoa_result;
}
