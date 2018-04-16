/*
 * inet.h
 *
 */

#ifndef _INET_H_
#define _INET_H_

#ifndef _SOCKLEN_T_DEFINED
#define _SOCKLEN_T_DEFINED
typedef unsigned int socklen_t;
#endif


#ifndef _UINT16T_DEFINED
#define _UINT16T_DEFINED
typedef unsigned short uint16_t;
#endif


#ifndef _UINT32T_DEFINED
#define _INT32T_DEFINED
typedef unsigned int uint32_t;
#endif


#ifndef _IN_ADDR_DEFINED
#define _IN_ADDR_DEFINED
typedef unsigned int in_addr_t;
#endif

#ifndef _STRUCT_IN_ADDR_DEFINED
#define _STRUCT_IN_ADDR_DEFINED
struct in_addr {
    in_addr_t s_addr;
};
#endif

uint32_t htonl(uint32_t);
uint16_t htons(uint16_t);
uint32_t ntohl(uint32_t);
uint16_t ntohs(uint16_t);
uint32_t htonl(uint32_t);

in_addr_t inet_addr(const char *cp);
const char *inet_ntop(int af, const void* src, char* dst, socklen_t size);
char *inet_ntoa(struct in_addr in);

#endif /* _INET_H_ */
