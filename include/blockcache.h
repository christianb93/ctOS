/*
 * blockcache.h
 */

#ifndef _BLOCKCACHE_H_
#define _BLOCKCACHE_H_

#include "ktypes.h"
#include "lib/unistd.h"

void bc_init();
int bc_open(dev_t dev);
int bc_close(dev_t dev);
int bc_read_bytes(u32 block, u32 bytes, void* buffer, dev_t device, u32 offset);
int bc_write_bytes(u32 block, u32 bytes, void* buffer, dev_t device, u32 offset);

#endif /* _BLOCKCACHE_H_ */
