/*
 * netdb.h
 *
 */

#ifndef _NETDB_H_
#define _NETDB_H_

struct hostent {
    char*  h_name;          // Official name of the host.
    char** h_aliases;       // An array of alternative host names, terminated by a null pointer.
    int h_addrtype;         // Address type.
    int h_length;           // The length, in bytes, of the address.
    char** h_addr_list;     //  A pointer to an array of pointers to network addresses (in network byte order) for the host,
                            // terminated by a null pointer.
};

struct servent {
    char   *s_name;         // Official name of the service
    char  **s_aliases;      // A pointer to an array of pointers to
                            // alternative service names, terminated by
                            // a null pointer.
    int     s_port;         // The port number at which the service
                            // resides, in network byte order.
    char   *s_proto;        // The name of the protocol to use when
                            // contacting the service.
};

#define HOST_NOT_FOUND 1
#define NO_DATA 2
#define NO_RECOVERY 3
#define TRY_AGAIN 4

extern int h_error;
struct hostent* gethostbyname(const char* name);
struct servent* getservbyname(const char *, const char *);

#endif /* _NETDB_H_ */
