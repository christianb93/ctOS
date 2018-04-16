/*
 * stdint.h
 */

#ifndef _STDINT_H_
#define _STDINT_H_

#ifndef _INT16T_DEFINED
#define _INT16T_DEFINED
typedef short int16_t;
#endif

#ifndef _UINT16T_DEFINED
#define _UINT16T_DEFINED
typedef unsigned short uint16_t;
#endif


typedef int int32_t;

#ifndef _UINT32T_DEFINED
#define _INT32T_DEFINED
typedef unsigned int uint32_t;
#endif

typedef unsigned long long int  uint64_t;
typedef long long int  int64_t;

typedef long long int intmax_t;
typedef unsigned long long int uintmax_t;

#define UINT64_C(c) c ## ULL

#define INT32_MAX  ((int) 0x7FFFFFFF)
#define UINT32_MAX ((unsigned int) 0xFFFFFFFF)


#endif /* _STDINT_H_ */
