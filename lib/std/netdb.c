/*
 * netdb.c
 *
 */


#include "lib/netdb.h"
#include "lib/string.h"

int h_errno;

#define NTOHS(x)  (((x) >> 8) + (((x) & 0xFF) << 8))

/*
 * A list of known services
 */
static char* noalias[1] = { 0 };
static char* alias_www[2] = {"www", 0 };

static struct servent known_services[] = {
        {"ftp", noalias, NTOHS(21), "tcp" },
        {"ssh", noalias, NTOHS(22), "tcp"},
        {"ssh", noalias, NTOHS(22), "udp" },
        {"telnet", noalias, NTOHS(23), "tcp" },
        {"domain", noalias, NTOHS(53), "tcp"},
        {"domain", noalias, NTOHS(53), "udp"},
        {"http", alias_www , NTOHS(80), "tcp" },
        {"http", noalias, NTOHS(80), "udp" },
        {"https", noalias, NTOHS(443), "tcp" },
        {"https", noalias, NTOHS(443), "udp" }
};

#define NR_SERVICES (sizeof(known_services) / sizeof(struct servent))

/*
 * Given a service name and a service protocol, locate the corresponding service
 * and return a servent structure. If proto is a NULL pointer, this is handled
 * as a wildcard for the protocol.
 *
 * Services which have an alias can be identified by passing this alias as the
 * first parameter.
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * We currently use a static and hardcoded database
 */
struct servent* getservbyname(const char* name, const char* proto) {
    int i;
    int j;
    struct servent* entry = 0;
    for (i = 0; i < NR_SERVICES; i++) {
        entry = known_services + i;
        /*
         * Check whether name matches
         */
        if (0 == strcmp(entry->s_name, name)) {
            if ((0 == proto) || (0 == strcmp(proto, entry->s_proto))) {
                return entry;
            }
        }
        /*
         * If name does not match, walk list of aliases
         */
        if (entry->s_aliases) {
            for (j = 0; ; j++) {
                if (0 == entry->s_aliases[j]) {
                    break;
                }
                if (0 == strcmp(entry->s_aliases[j], name)) {
                    if ((0 == proto) || (0 == strcmp(proto, entry->s_proto))) {
                        return entry;
                    }
                }
            }
        }
    }
    return 0;
}

