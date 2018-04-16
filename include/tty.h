/*
 * tty.h
 */

#ifndef _TTY_H_
#define _TTY_H_

#include "ktypes.h"
#include "drivers.h"
#include "lib/termios.h"
#include "locks.h"
/*
 * Maximum size of current line in canonical mode
 */
#define MAX_CANON 256
/*
 * Maximum size of input queue
 */
#define MAX_INPUT MAX_CANON


/*
 * Macro to check for end-of-line
 */
#define NL 10
#define IS_EOL(c,settings) ((c==NL) || (c==settings.c_cc[VEOF]) || (c==settings.c_cc[VEOL]))



/*
 * This structure describes a logical TTY and its state. It contains the TTY settings as well
 * as the current content of the TTY buffers
 */
typedef struct _tty_t {
    minor_dev_t minor;              // Minor device number used to identify the TTY
    u8 line_buffer[MAX_INPUT];      // line buffer
    int line_buffer_end;            // last valid character in line buffer
    u8 read_buffer[MAX_INPUT];      // characters available for user space
    int read_buffer_end;            // last valid character in read buffer
    struct termios settings;        // Settings
    spinlock_t lock;                // lock to protect TTY from concurrent access
    semaphore_t data_available;     // data is available in TTY
    semaphore_t available;          // tty is available
    pid_t pgrp;                     // foreground process group
} tty_t;

/*
 * These are the channels which might be connected to a TTY
 */
#define TTY_CHANNEL_CONS 0


int tty_setpgrp(minor_dev_t minor, pid_t pgrp);
pid_t tty_getpgrp(minor_dev_t minor);
void tty_init();
void tty_put (int channel, unsigned char* input, size_t nbytes);
ssize_t tty_read(minor_dev_t minor, ssize_t size, void* data, unsigned int flags);
ssize_t tty_write(minor_dev_t minor, ssize_t size, void* buffer);
int tty_tcgetattr(minor_dev_t minor, struct termios* termios_p);
int tty_tcsetattr(minor_dev_t minor, int action, struct termios* termios_p);

#endif /* _TTY_H_ */
