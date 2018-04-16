/*
 * test_netdb.c
 */

#include <netdb.h>
#include <stdio.h>
#include "kunit.h"

/*
 * Testcase 1:
 * Get service entry for ftp / tcp
 */
int testcase1() {
    struct servent* service;
    service = getservbyname("ftp", "tcp");
    ASSERT(service);
    ASSERT(service->s_aliases);
    ASSERT(service->s_aliases[0] == 0);
    ASSERT(service->s_name);
    ASSERT(0 == strcmp("ftp", service->s_name));
    ASSERT(service->s_proto);
    ASSERT(0 == strcmp(service->s_proto, "tcp"));
    ASSERT(ntohs(21) == service->s_port);
    return 0;
}

/*
 * Testcase 2:
 * Get service entry for ftp / udp - should not be a match
 */
int testcase2() {
    struct servent* service;
    service = getservbyname("ftp", "udp");
    ASSERT(0 == service);
    return 0;
}

/*
 * Testcase 3:
 * Get service entry for ftp / NULL
 */
int testcase3() {
    struct servent* service;
    service = getservbyname("ftp", 0);
    ASSERT(service);
    ASSERT(service->s_aliases);
    ASSERT(service->s_aliases[0] == 0);
    ASSERT(service->s_name);
    ASSERT(0 == strcmp("ftp", service->s_name));
    ASSERT(service->s_proto);
    ASSERT(0 == strcmp(service->s_proto, "tcp"));
    ASSERT(ntohs(21) == service->s_port);
    return 0;
}

/*
 * Testcase 4:
 * Get service entry for foo / tcp - should not be a match
 */
int testcase4() {
    struct servent* service;
    service = getservbyname("foo", "tcp");
    ASSERT(0 == service);
    return 0;
}

/*
 * Testcase 5:
 * Get service entry for foo / 0 - should not be a match
 */
int testcase5() {
    struct servent* service;
    service = getservbyname("foo", 0);
    ASSERT(0 == service);
    return 0;
}

/*
 * Testcase 6:
 * Get service entry for http / tcp and verify that alias www is specified
 */
int testcase6() {
    struct servent* service;
    service = getservbyname("http", "tcp");
    ASSERT(service);
    ASSERT(service->s_aliases);
    ASSERT(service->s_aliases[0]);
    ASSERT(0 == strcmp("www", service->s_aliases[0]));
    ASSERT(service->s_aliases[1] == 0);
    ASSERT(service->s_name);
    ASSERT(0 == strcmp("http", service->s_name));
    ASSERT(service->s_proto);
    ASSERT(0 == strcmp(service->s_proto, "tcp"));
    ASSERT(ntohs(80) == service->s_port);
    return 0;
}

/*
 * Testcase 7:
 * Get service entry for www
 */
int testcase7() {
    struct servent* service;
    service = getservbyname("www", 0);
    ASSERT(service);
    ASSERT(service->s_aliases);
    ASSERT(service->s_aliases[0]);
    ASSERT(0 == strcmp("www", service->s_aliases[0]));
    ASSERT(service->s_aliases[1] == 0);
    ASSERT(service->s_name);
    ASSERT(0 == strcmp("http", service->s_name));
    ASSERT(service->s_proto);
    ASSERT(0 == strcmp(service->s_proto, "tcp"));
    ASSERT(ntohs(80) == service->s_port);
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
    END;
}

