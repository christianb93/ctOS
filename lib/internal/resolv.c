/*
 * resolv.c
 *
 * The resolver library
 *
 * Here are some notes on the DNS protocol (see RFC 1035 for details).
 *
 * A DNS protocol message consists of the following parts:
 *
 * 1) a header which is the same for all messages and is described by the structure dns_header_t below
 * 2) a (possibly empty) question section which contains queries sent to a DNS server
 * 3) a (possibly empty) section containing one or more so-called resource records (RR) describing the answer of the server
 * 4) a (possibly empty) section containing RRs which point towards an authorative name server
 * 5) a (possibly empty) section containing RRs containing additional information
 *
 * The question section consists of the QNAME which is a domain name represented as a sequence of labels, a 16 bit field QTYPE
 * and a 16 bit field called QCLASS - see the declarations below for a list of the most common values. Here the type describes the
 * type of information requested, whereas class is the address class (usually IN for internet addresses)
 *
 * All resource records share the same layout which is as follows. Each RR starts with a QNAME, a QTYPE and a QCLASS similar to
 * a question record. Next there is a 32-bit TTL field which is the number of seconds the information remains valid and a 16-bit
 * field RDLENGTH which is the length in octets of the following field RDATA which contains the actual data.
 *
 * To encode a domain name, the following scheme is used. By definition, a domain name consists of a series of labels, separated
 * by dots. To store a domain name, each label is stored separately - omitting the dot - preceeded by a single octet which contains
 * the length of the label. The last label is a zero label, i.e. a label with length zero. Thus the domain www.kernel.org would be
 * stored as
 *
 * 3 w w w 6 k e r n e l 3 org 0
 *
 * The length of each label is restricted to 63 bytes. To save space, a compression might be used which essentially replaces a label
 * to a reference to another occurence of the same label in the message. A compressed label starts with a length field for which the
 * first two bits are set (which can never happen for an uncompressed label as the length is restricted to 63 bytes). The remaining 6
 * bits of the length times 256 plus the next byte form a 14-bit offset which is the offset to the start of the referenced label (i.e.
 * its length field) within the entire message, starting at the first byte of the header which has offset 0.
 *
 *
 */

#include "lib/stdio.h"
#include "lib/arpa/inet.h"
#include "lib/netinet/in.h"
#include "lib/unistd.h"
#include "lib/sys/socket.h"
#include "lib/string.h"
#include "lib/os/resolv.h"
#include "lib/stdlib.h"
#include "lib/errno.h"
#include "lib/netdb.h"
#include "lib/os/if.h"
#include "lib/sys/ioctl.h"

extern int h_errno;

/*
 * Send a DNS resolution request, i.e. a query of type A and class A
 * Parameters:
 * @fd - file descriptor used to write the request
 * @host - hostname to be resolved
 * @dest - address of DNS server to use
 * @rd - recursion desired, if this bit is set ask server to perform recursive queries
 * @id - value of ID field in host byte order
 * Return value:
 * 0 upon success
 * -1 upon failure
 */
int __ctOS_dns_send_request(int fd, unsigned char* host, struct sockaddr_in* dest, int rd, int id) {
    int qname_len;
    unsigned char buffer[MAX_DNS_MSG_SIZE];
    dns_header_t* hdr;
    unsigned char* len_ptr;
    int label_len;
    unsigned char* hostname_ptr;
    unsigned short* qtype;
    unsigned short* qclass;
    /*
     * Assemble header
     */
    hdr = (dns_header_t*) buffer;
    hdr->id = htons(id);
    hdr->rd = rd;
    hdr->tc = 0;
    hdr->aa = 0;
    hdr->opcode = 0; // standard query
    hdr->qr = 0;
    hdr->rcode = 0;
    hdr->z = 0;
    hdr->ra = 0;
    hdr->qdcount = htons(1);
    hdr->ancount = 0;
    hdr->nscount = 0;
    hdr->arcount = 0;
    /*
     * Now place request in data section. We first assemble the QNAME entries. For this purpose, the following algorithm is used.
     * 1) we maintain a ptr called len_ptr
     * 2) initially this pointer is the first byte of the QNAME field
     * 3) we then start to walk the provided host name counting bytes until we hit upon a dot, copying bytes to the QNAME
     *    field as we go
     * 4) when we hit upon the dot, we place the number of bytes in *len_ptr, increase len_ptr by the number of bytes and continue
     * 5) we stop when the string is complete and write a trailing zero
     */
    len_ptr = buffer + sizeof(dns_header_t);
    qname_len = 0;
    label_len = 0;
    hostname_ptr = (unsigned char*) host;
    while (1) {
        /*
         * Make sure that len_ptr is within buffer
         */
        if ((len_ptr - buffer) > MAX_DNS_MSG_SIZE - 1) {
            return -1;
        }
        if (('.' == *hostname_ptr) || (0 == *hostname_ptr)) {
            /*
             * If this is a trailing dot, ignore it
             */
            if (('.' == *hostname_ptr) && (0 == hostname_ptr[1])) {
                /*
                 * do nothing
                 */
            }
            else {
                /*
                 * Reached a dot. Store length of label read so far and increase
                 * len_ptr to skip the label. Also reset label_len
                 */
                *len_ptr = label_len;
                len_ptr = len_ptr + label_len + 1;
                qname_len++;
                label_len = 0;
            }
        }
        else {
            /*
             * Copy next byte to QNAME, using len_ptr as a reference
             * Check whether we are still within buffer first
             */
            label_len++;
            if ((len_ptr + label_len - buffer) > MAX_DNS_MSG_SIZE - 1) {
                return -1;
            }
            *(len_ptr + label_len) = *hostname_ptr;
            qname_len++;
        }
        if (0 == *hostname_ptr)
            break;
        hostname_ptr++;
    }
    /*
     * We need space for 5 additional bytes
     */
    if ((len_ptr - buffer + TYPE_BYTES + CLASS_BYTES + 1) > MAX_DNS_MSG_SIZE - 1) {
        return -1;
    }
    /*
     * Write zero label
     */
    *len_ptr = 0;
    len_ptr++;
    qname_len++;
    /*
     * Now qtype
     */
    qtype = (unsigned short*) len_ptr;
    *qtype = ntohs(QTYPE_A);
    /*
     * and qclass
     */
    qclass = qtype + 1;
    *qclass = ntohs(QCLASS_IN);
    /*
     * Send packet via UDP
     */
    if (sendto(fd, buffer, qname_len + sizeof(dns_header_t) + CLASS_BYTES + TYPE_BYTES,
            0, (struct sockaddr*) dest, sizeof(struct sockaddr_in)) < 0) {
        return -1;
    }
    return 0;
}

/*
 * This is a utility function which is used to parse labels in DNS messages
 * Parameter:
 * @msg - a pointer to the start of the DNS protocol message (i.e. to the start of the DNS header)
 * @offset - the offset of the label to be parsed in the message
 * @len - the length of the message
 * @domain - a buffer in which the resulting domain will be stored, needs to be at least 256 bytes long
 * Return value:
 * number of bytes parsed upon success
 * -1 when an error occurs
 */
int __ctOS_dns_parse_name(unsigned char* msg, int offset, int len, unsigned char* domain) {
    /*
     * The following variables are used during the parsing:
     * label_len - length of the label currently at hand
     * domain_offset - offset into the result string at which the next label is placed
     * label_offset - for compressed labels, offset of referenced label into message
     * ptr - currently parsed character in name
     */
    unsigned char label_len;
    int domain_offset;
    int label_offset;
    unsigned char* ptr;
    int rc = -1;
    /*
     * Check parameters
     */
    if (0 == msg)
        return -1;
    if (offset > len)
        return -1;
    /*
     * Now walk labels
     */
    ptr = msg + offset;
    domain_offset = 0;
    label_len = 0;
    while(1) {
        /*
         * Make sure that we stay within message
         */
        if ((ptr - msg) > len - 1) {
            return -1;
        }
        /*
         * Read length of label
         */
        label_len = *ptr;
        if (0 == label_len)
            break;
        /*
         * If the length exceeds 192, it is a compressed label. Continue parsing at
         * the point where the original label is located, but make sure that rc is
         * correctly set.
         * Note that there can be recursions here, so we want to make sure that we
         * only set the return code once
         */
        if (label_len >= 192) {
            label_offset = (label_len & ~192)*256 + *(ptr + 1);
            if (-1 == rc)
                rc = ptr - msg - offset + 2;
            ptr = msg + label_offset;
            if ((ptr - msg) > len - 1) {
                return -1;
            }
            label_len = *ptr;
            if ((0 == label_len) || (label_len > 63))
                return -1;
        }
        /*
         * if label length is more than 63, this is an error
         */
        else if (label_len > 63) {
            return -1;
        }
        /*
         * Copy the next label_len bytes to domain. First check that we have
         * still label_len bytes left within the message and the domain
         */
        if (domain) {
            if ((domain_offset + label_len > MAX_DOMAIN_SIZE)  || ((ptr + label_len - msg) > len - 1)) {
                return -1;
            }
            memcpy(domain + domain_offset, ptr + 1, label_len);
        }
        /*
         * and increase pointers into domain and name
         */
        domain_offset = domain_offset + label_len;
        ptr = ptr + label_len + 1;
        /*
         * finally add dot if we are not done
         */
        if ((ptr - msg) > len - 1) {
            return -1;
        }
        if (*ptr) {
            if (domain) {
                domain[domain_offset] = '.';
                if (domain_offset > MAX_DOMAIN_SIZE - 1)
                    return -1;
            }
            domain_offset++;
        }
    }
    if (-1 == rc)
        rc = ptr - msg - offset + 1;
    return rc;
}


/*
 * Parse a section of a DNS message. This will walk the section and place all resource records identified in
 * a linked list.  It returns the number of bytes read, i.e. the length of the section in bytes
 * Note that only sections containing resource records can be parsed, i.e. no question sections
 * Parameters:
 * @msg - the DNS message, starting with the header
 * @len - the length of the DNS message
 * @offset - the offset into the message where the section begins
 * @entries - number of entries in the section
 * @result_list - header of a linked list to which the identified RRs will be added
 */
int __ctOS_dns_parse_rr_section(unsigned char* msg, int len, int offset, int entries, dns_rr_t** result_list) {
    unsigned char domain[MAX_DOMAIN_SIZE];
    int rr_offset;
    unsigned short type;
    dns_rr_t* rr;
    dns_rr_t* tail;
    int section_len;
    int rr_count;
    unsigned short class;
    unsigned short rdlength;
    unsigned char* rdata;
    int known_type;
    int rc;
    if (offset > len)
        return -1;
    if (0 == msg)
        return -1;
    rr_count = 0;
    section_len = 0;
    while (rr_count < entries) {
        rr_offset = offset + section_len;
        /*
         * Offset still needs to be within msg
         */
        if (rr_offset > len - 1) {
            return -1;
        }
        /*
         * First read domain
         */
        memset(domain, 0, MAX_DOMAIN_SIZE);
        rc = __ctOS_dns_parse_name(msg, rr_offset, len, domain);
        if (rc <= 0) {
            return -1;
        }
        section_len += rc;
        /*
         * Starting at offset rr_offset + rc, we expect 10 more bytes, containing TYPE, CLASS,
         * RDLENGTH and TTL
         */
        if (rr_offset + rc + TYPE_BYTES + CLASS_BYTES + RDLENGTH_BYTES + TTL_BYTES > len) {
            return -1;
        }
        /*
         * Get type and class
         */
        type = msg[rr_offset + rc]*256 + msg[rr_offset + rc + 1];
        class = msg[rr_offset + rc + 2]*256 + msg[rr_offset + rc + 3];
        section_len += TYPE_BYTES;
        section_len += CLASS_BYTES;
        /*
         * Only class internet is supported
         */
        if (QCLASS_IN != class) {
            return -1;
        }
        /*
         * Skip TTL and get rdlength
         */
        rdlength = msg[rr_offset + rc + 8]*256 + msg[rr_offset + rc + 9];
        section_len = section_len + rdlength + TTL_BYTES + RDLENGTH_BYTES;
        /*
         * Now process RDATA depending on type
         */
        rr = (dns_rr_t*) malloc(sizeof(dns_rr_t));
        if (0 == rr)
            return -1;
        memset((void*) rr, 0, sizeof(dns_rr_t));
        rr->type = type;
        rr->class = class;
        memcpy(rr->owner, domain, MAX_DOMAIN_SIZE);
        rdata = (unsigned char*)(msg + rr_offset + rc + 10);
        known_type = 1;
        switch (type) {
            case QTYPE_A:
                /*
                 * In this case RDATA is a 32 bit IP address
                 */
                if ((rdata - msg) + sizeof(unsigned int) > len) {
                    return -1;
                }
                rr->address = *((unsigned int*) rdata);
                break;
            case QTYPE_CNAME:
                /*
                 * The RDATA is the canonical name of the owner
                 */
                if (__ctOS_dns_parse_name(msg, rdata - msg, len, rr->cname) <= 0) {
                    return -1;
                }
                break;
            default:
                known_type = 0;
                break;
        }
        /*
         * If this is a known type, add entry to list, otherwise discard it
         */
        if (1 == known_type) {
            rr->next = 0;
            if (*result_list) {
                /*
                 * List has a head already. Navigate to tail
                 */
                tail = *result_list;
                while(tail->next)  {
                    tail = tail->next;
                }
                /*
                 * and add new record
                 */
                tail->next = rr;
            }
            /*
             * List empty - new entry is head
             */
            else {
                *result_list = rr;
            }
        }
        else
            free ((void*) rr);
        rr_count++;
    }
    return section_len;
}

/*
 * Parse a DNS message. This will walk the sections and place all resource records identified in
 * a linked list.  On success, 0 is returned
 * Parameters:
 * @msg - the DNS message, starting with the header
 * @len - the length of the DNS message
 * @result_list - header of a linked list to which the identified RRs will be added
 */
int __ctOS_dns_parse_reply(unsigned char* msg, int len, dns_rr_t** result_list) {
    int qcount;
    int rr_count;
    int rc;
    dns_header_t* hdr;
    /*
     * Check parameter
     */
    if (0 == msg)
        return -1;
    if (0 == result_list)
        return -1;
    /*
     * Read header and extract number of records in question section and all other
     * sections
     */
    if (len < sizeof(dns_header_t))
        return -1;
    hdr = (dns_header_t*) msg;
    if (0 == hdr->qr)
        return -1;
    qcount = ntohs(hdr->qdcount);
    rr_count = ntohs(hdr->ancount) + ntohs(hdr->arcount) + ntohs(hdr->nscount);
    if ((qcount < 0) || (rr_count < 0))
        return -1;
    /*
     * First we skip over the question section if any
     */
    if (qcount > 0) {
        rc = __ctOS_dns_parse_name(msg, sizeof(dns_header_t), len, 0);
        if (rc <= 0)
            return -1;
        /*
         * As the QNAME is followed by QTYPE and QCLASS, first section starts at offset rc + 4
         * after header
         */
    }
    /*
     * Now parse sections
     */
    if (__ctOS_dns_parse_rr_section(msg, len, sizeof(dns_header_t) + rc + TYPE_BYTES + CLASS_BYTES, rr_count, result_list) < 0)
        return -1;
    return 0;
}

/*
 * Given a result list and a host name, walk the result list and search for matching CNAME records.
 * If a CNAME record is found matching the host name, the name is translated
 * @host - host to be resolved (i.e. alias)
 * @cname - if a CNAME record is found, the CNAME is stored here
 * @result_list - list of RRs to consider
 * Return value:
 * 0 - no mapping done
 * 1 - mapping done, the caller should call again to resolve possible chains
 */
static int map_to_cname(unsigned char* host, unsigned char* cname, dns_rr_t* result_list) {
    dns_rr_t* item;
    item = result_list;
    while(item) {
        if ((QTYPE_CNAME == item->type) && (QCLASS_IN == item->class) &&
                (0 == strcasecmp((char*) item->owner, (char*) host))) {
            strcpy((char*) cname, (char*) item->cname);
            break;
        }
        item = item->next;
    }
    return 0;
}

/*
 * Resolve a hostname, i.e. determine its IP address
 * Parameter:
 * @host - the host name to be resolved
 * @addr - result will be stored there as 32 bit integer in network byte order
 * @ns - address of name server to try
 * Return value:
 * 0 upon success
 * -1 on error
 */
int __ctOS_dns_resolv(unsigned char* host, unsigned int* addr, struct sockaddr_in* ns) {
    struct timeval timeout;
    unsigned char msg[MAX_DNS_MSG_SIZE];
    dns_rr_t* result_list;
    dns_rr_t* next;
    dns_rr_t* item;
    unsigned char cname[MAX_DOMAIN_SIZE + 1];
    unsigned int result = 0;
    struct sockaddr_in src;
    int rc;
    int fd;
    int depth = 0;
    int attempts = 0;
    /*
     * Validate parameters
     */
    if ((0 == host) || (0 == addr) || (0 == ns))
        return -1;
    if (strlen((char*) host) > MAX_DOMAIN_SIZE)
        return -1;
    /*
     * Open UDP socket
     */
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        return -1;
    }
    /*
     * Set receive timeout
     */
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (void*) &timeout, sizeof(struct timeval))) {
        close(fd);
        return -1;
    }
    /*
     * Bind so that we can receive the reply
     */
    src.sin_addr.s_addr = 0;
    src.sin_family = AF_INET;
    src.sin_port = 0;
    if (bind(fd, (struct sockaddr*) &src, sizeof(struct sockaddr_in)) < 0) {
        close(fd);
        return -1;
    }
    /*
     * Send request
     */
    if (__ctOS_dns_send_request(fd, host, ns, 1, getpid()) < 0) {
        close(fd);
        return -1;
    }
    /*
     * Wait for reply
     */
    while(attempts < 5) {
        rc = recv(fd, msg, 512, 0);
        if (rc < 0) {
            /*
             * If we have a timeout, continue, otherwise return
             */
            if ((errno != EWOULDBLOCK) && (errno != EAGAIN)) {
                close(fd);
                return -1;
            }
        }
        attempts++;
        /*
         * ID of reply is stored in first two octets - check whether
         * this matches our ID
         */
        if (rc) {
            if (getpid() == ntohs(*((unsigned short*) msg)))
                break;
        }
        if (DNS_RESOLV_ATTEMPTS <= attempts)
            return -1;
    }
    /*
     * Close socket
     */
    close(fd);
    /*
     * and parse result
     */
    result_list = 0;
    rc = __ctOS_dns_parse_reply(msg, MAX_DNS_MSG_SIZE, &result_list);
    /*
     * Resolve CNAME if needed. Note that according to RFC 1034, section 3.6, a name server is expected
     * to return the IP address for the canonical name along with the canonical name in one reply if a
     * request of type A is processed - we rely on that and do not try a second query for the CNAME
     */
    strcpy((char*) cname, (char*) host);
    while (map_to_cname(host, cname, result_list)) {
        /*
         * Make sure that we do not loop forever
         */
        depth++;
        if (depth > DNS_RESOLV_ATTEMPTS) {
            rc = -1;
            break;
        }
    }
    /*
     * Now walk result list and free all records. When we hit upon a record
     * of type IN, return that address
     */
    item = result_list;
    while(item) {
        next = item->next;
        if ((0 == rc) && (QTYPE_A == item->type) && (QCLASS_IN == item->class)) {
            if ((0 == strcasecmp((char*) item->owner, (char*) cname)) && (0 == result)) {
                result = item->address;
            }
        }
        free((void*) item);
        item = next;
    }
    if ((rc) || (0 == result))
        return -1;
    *addr = result;
    return 0;

}

/*
 * Static memory for return value of gethostbyname
 */
static struct hostent lookup_result;
static unsigned int ip_addr;
static char* addr_list[2];
static char host_name[MAX_DOMAIN_SIZE];

/*
 * Implementation of POSIX gesthostbyname. This function returns a pointer to a hostent structure which contains
 * an address of type AF_INET for the host with the specified name.  If no record was found for the specified host,
 * NULL is returned.
 */
struct hostent* __ctOS_gethostbyname(const char* name) {
    int i;
    int fd;
    struct sockaddr_in ns;
    struct ifconf if_conf;
    struct ifreq if_req[32];
    if (strlen(name) > MAX_DOMAIN_SIZE) {
        h_errno = NO_RECOVERY;
        return 0;
    }
    /*
     * Determine name server to use
     */
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        h_errno = NO_RECOVERY;
        return 0;
    }
    if_conf.ifc_len = 32 * sizeof(unsigned int);
    if_conf.ifc_ifcu.ifcu_req = if_req;
    if (ioctl(fd, SIOCGIFCONF, (void*) &if_conf) < 0) {
        close(fd);
        h_errno = NO_RECOVERY;
        return 0;
    }
    /*
     * Close socket again
     */
    close(fd);
    ns.sin_family = AF_INET;
    ns.sin_addr.s_addr = 0;
    ns.sin_port = ntohs(53);
    for (i = 0; i < MAX_DNS_SERVERS; i++) {
        if (if_conf.ifc_dns_servers[i]) {
            ns.sin_addr.s_addr = if_conf.ifc_dns_servers[i];
            break;
        }
    }
    if (0 == ns.sin_addr.s_addr) {
        h_errno = NO_RECOVERY;
        return 0;
    }
    if (__ctOS_dns_resolv((unsigned char*) name, &ip_addr, &ns)) {
        h_errno = HOST_NOT_FOUND;
        return 0;
    }
    lookup_result.h_addr_list = addr_list;
    addr_list[0] = (char*) &ip_addr;
    addr_list[1] = 0;
    lookup_result.h_addrtype = AF_INET;
    lookup_result.h_aliases = 0;
    lookup_result.h_length = sizeof(unsigned int);
    lookup_result.h_name = host_name;
    strncpy(host_name, name, MAX_DOMAIN_SIZE);
    return &lookup_result;
}
