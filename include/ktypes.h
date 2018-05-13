/*
 * ktypes.h
 *
 *  Created on: Dec 19, 2011
 *      Author: chr
 */

#ifndef _KTYPES_H_
#define _KTYPES_H_

#include "lib/os/types.h"

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long int u64;

#ifdef __GNUC__
    #if __SIZEOF_LONG_LONG__ != 8
        #error I am assuming that long long is 64 bits wide
    #endif
#endif

typedef unsigned int reg_t;

#endif /* _KTYPES_H_ */
