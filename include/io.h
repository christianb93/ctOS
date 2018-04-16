/*
 * Read and write from and to port I/O and
 * memory mapped I/O
 */

#ifndef _io_h
#define _io_h

#include "ktypes.h"

u8 inb(u16 port);
u16 inw(u16 port);
void outb(u8 value, u16 port);
void outw(u16 value, u16 port);
u32 inl(u16 port);
void outl(u32 value, u16 port);

#endif
