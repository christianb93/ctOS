/*
 * net.c
 *
 */

#include "lib/stdint.h"
#include "lib/string.h"
#include "lib/stdlib.h"
#include "debug.h"
#include "lib/netinet/in.h"
#include "lib/arpa/inet.h"
#include "lib/stdio.h"


/*
 * Convert a 16-bit unsigned int from network byte order to host byte order
 */
uint16_t ntohs(uint16_t netshort) {
    uint16_t result;
    result = netshort >> 8;
    result = result + ((netshort & 0xFF) << 8);
    return result;
}

/*
 * Convert a 16-bit unsigned int from host byte order to network byte order
 */
uint16_t htons(uint16_t hostshort) {
    return ntohs(hostshort);
}

/*
 * Convert a 32-bit unsigned int from host byte order to network byte order
 */
uint32_t htonl(uint32_t hostlong) {
    uint32_t result;
    result = htons(hostlong >> 16);
    result = result + (htons(hostlong & 0xFFFF) << 16);
    return result;
}

/*
 * Convert a 32-bit unsigned int from network byte order to host byte order
 */
uint32_t ntohl(uint32_t netlong) {
    return htonl(netlong);
}

/*
 * Convert an IP address into a dword in network byte order
 *
 * When the provided address has less than 4 octets, the last octet will be interpreted as 16, 24 or 32 bit number
 * respectively and placed in the rightmost bytes of the address
 *
 */
unsigned int inet_addr(const char* ip_address) {
    int i = 0;
    int j = 0;
    int k = 0;
    char octet_str[12];
    unsigned int result = 0;
    int octet_pos = 0;
    unsigned long octet;
    char* endptr;
    for (octet_pos = 0; octet_pos < 4; octet_pos++) {
        /*
         * Proceed to next dot
         */
        while ((ip_address[i] != '.') && (ip_address[i])) {
            i++;
        }
        /*
         * Make sure that we have not consumed more than 10 digits
         */
        if (i - j > 10)
            return -1;
        /*
         * Convert octet
         */
        strncpy(octet_str, ip_address + j, i - j);
        octet_str[i-j] = 0;
        octet = strtoull(octet_str, &endptr, 0);
        if (endptr) {
            if (*endptr)
                return -1;
        }
        /*
         * and add to result. Note that the "octet" can actually be up
         * to 32 bits long if shorthand notations are used, so we need to convert
         * it to network byte order if needed. Thus determine number of actual
         * octets first
         */
        k = 0;
        while ((octet >> (8*k)) && (k < 4)) {
            k++;
        }
        /*
         * And add octet to result
         */
        while (k) {
            k--;
            result = result + (((octet >> (k*8)) & 0xff) << (octet_pos * 8));
            if (k)
                octet_pos++;
        }
        /*
         * If we have reached the end of the string
         * return result assembled so far
         */
        if (0 == ip_address[i])
            return result;
        /*
         * proceed to next character
         */
        i++;
        j = i;
    }
    return result;
}


