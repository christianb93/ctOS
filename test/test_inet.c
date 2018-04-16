/*
 * test_inet.c
 *
 */

#include "kunit.h"
#include "lib/arpa/inet.h"
#include "lib/netinet/in.h"
#include <stdio.h>
#include "vga.h"

/*
 * Stub for win_putchar
 */
void win_putchar(win_t* win, u8 c) {
    printf("%c", c);
}

/*
 * Testcase 1:
 * Convert an IP address into a 32-bit number in network byte order
 *
 */
int testcase1() {
    unsigned int ip;
    ip = inet_addr("10.0.2.20");
    ASSERT(ip == 0x1402000a);
    return 0;
}

/*
 * Testcase 2:
 * Convert an IP address into a 32-bit number in network byte order
 * Special case: three component address
 *
 */
int testcase2() {
    unsigned int ip;
    ip = inet_addr("10.0.1025");
    ASSERT(ip == 0x0104000a);
    return 0;
}

/*
 * Testcase 3:
 * Convert an IP address into a 32-bit number in network byte order
 * Special case: two component address
 *
 */
int testcase3() {
    unsigned int ip;
    ip = inet_addr("10.65700");
    ASSERT(ip == 0xa400010a);
    return 0;
}

/*
 * Testcase 4:
 * Convert an IP address into a 32-bit number in network byte order
 * Special case: one component address
 *
 */
int testcase4() {
    unsigned int ip;
    ip = inet_addr("4133256730");
    ASSERT(ip == 0x1a7e5cf6);
    return 0;
}

/*
 * Testcase 5:
 * Convert an IP address into a 32-bit number in network byte order - use hex notation
 *
 */
int testcase5() {
    unsigned int ip;
    ip = inet_addr("0xa.0.2.20");
    ASSERT(ip == 0x1402000a);
    return 0;
}

/*
 * Testcase 6:
 * Try to convert an invalid IP address
 *
 */
int testcase6() {
    unsigned int ip;
    ip = inet_addr("ABC.0.2.20");
    ASSERT(ip == -1);
    return 0;
}

/*
 * Testcase 7:
 * Convert a numeric IP address into a string
 */
int testcase7() {
    unsigned int ip;
    char ip_str[INET_ADDRSTRLEN];
    ip = inet_addr("10.0.2.21");
    ASSERT(inet_ntop(AF_INET, (const void*) &ip, ip_str, 16));
    ASSERT(0 == strcmp("10.0.2.21", ip_str));
    return 0;
}

/*
 * Testcase 8:
 * Use inet_ntoa
 */
int testcase8() {
    struct in_addr addr;
    addr.s_addr = inet_addr("127.0.0.1");
    ASSERT(0 == strcmp("127.0.0.1", inet_ntoa(addr)));
    return 0;
}

int main() {
    INIT;
    RUN_CASE(1);
    RUN_CASE(2);
    RUN_CASE(3);
    RUN_CASE(4);
    RUN_CASE(5);
    RUN_CASE(6);
    RUN_CASE(7);
    RUN_CASE(8);
    END;
}
