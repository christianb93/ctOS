/*
 * tty_ld.h
 *
 */

#ifndef _TTY_LD_H_
#define _TTY_LD_H_

#include "lib/os/types.h"

int tty_ld_put(tty_t* tty, unsigned char* input, size_t nbytes);
void tty_ld_init(tty_t* tty);
void tty_ld_flush(tty_t* tty);
ssize_t tty_ld_read(tty_t* tty, unsigned char* buffer, size_t nbytes);
ssize_t tty_ld_write(tty_t* tty, unsigned char* buffer, size_t nbytes);

#endif /* _TTY_LD_H_ */
