/*
 * host.c
 *
 * Resolve host names
 *
 * Usage:
 *
 * host [-s server] hostname
 */


#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <os/resolv.h>

/*
 * Print usage information
 */
static void print_usage() {
    printf("Usage: host [-s server] hostname\n");
    printf("\t-s specifies the IP address of the DNS server to use\n");
}

int main(int argc, char** argv) {
    char* dns_ip = 0;
    struct sockaddr_in ns;
    char* host;
    unsigned int addr = 0;
    char addr_string[INET_ADDRSTRLEN + 1];
    char c;
    int rc;
    /*
     * Parse options
     */
    while (-1 != (c = getopt(argc, argv, "s:"))) {
        switch(c) {
            case 's':
                dns_ip = optarg;
                break;
            default:
                print_usage();
                return 1;
        }
    }
    if (optind >= argc) {
        print_usage();
        return 1;
    }
    host = argv[optind];
    if (0 == dns_ip)
        dns_ip = "127.0.0.1";
    printf("Trying to resolve host %s, using server %s\n", host, dns_ip);
    /*
     * Try to resolve
     */
    ns.sin_family = AF_INET;
    ns.sin_addr.s_addr = inet_addr(dns_ip);
    if (0 == ns.sin_addr.s_addr) {
        printf("Invalid name server IP address %s\n", dns_ip);
        return 1;
    }
    ns.sin_port = ntohs(53);
    rc = __ctOS_dns_resolv((unsigned char*) host, &addr, &ns);
    if (rc) {
        printf("Resolution failed\n");
        return 1;
    }
    if (0 == inet_ntop(AF_INET, &addr, addr_string, INET_ADDRSTRLEN + 1)) {
        printf("Could not format IP address %x\n", addr);
        return 1;
    }
    printf("Host %s has IP address %s\n", host, addr_string);
    return 0;
}

