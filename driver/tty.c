/*
 * tty.c
 *
 * TTY driver. This is the high-level TTY driver which uses the lowlevel keyboard driver in keyboard.c and the
 * VGA driver in vga.c
 */

#include "irq.h"
#include "debug.h"
#include "keyboard.h"
#include "util.h"
#include "drivers.h"
#include "dm.h"
#include "vga.h"
#include "locks.h"
#include "pm.h"
#include "tty.h"
#include "lib/os/errors.h"
#include "lib/termios.h"
#include "lib/os/signals.h"
#include "fs.h"
#include "tty_ld.h"
#include "lib/fcntl.h"

/*
 *
 * Currently, there is only one TTY structure as virtual terminals, master-slave TTYs
 * and serial line terminals are not yet implemented. In future releases, this will be an
 * array of TTY structures, one for each terminal
 */
static tty_t tty[1];
static tty_t* active_tty = tty;

/*
 * Operations structure for the tty - this is what the device driver
 * manager will get
 */
char_dev_ops_t tty_ops;


/*
 * Open a tty. This is just a stub at the moment
 * as the console does not require any opening
 */
static int tty_open(minor_dev_t minor) {
    return 0;
}

/*
 * Close a tty. This is just a stub at the moment
 * as the console does not require any closing
 */
static int tty_close(minor_dev_t minor) {
    return 0;
}

/*
 * Seek operation for a tty - stub only
 */
static int tty_seek(minor_dev_t minor, ssize_t pos) {
    return 0;
}

/*
 * Given a channel, locate the matching TTY. A channel can either be the console or
 * - in a later release - a virtual terminal or a master/slave TTY par. Currently, this
 * function simply returns the active TTY
 * Parameters:
 * @channel - the channel
 * Return value:
 * a pointer to the TTY associated with the channel
 */
static tty_t* get_tty_for_channel(int channel) {
    return active_tty;
}

/*
 * Given a minor device, return the matching tty. Currently, this
 * function simply returns the active TTY
 * Parameters:
 * @minor - the minor device
 * Return value:
 * a pointer to the TTY associated with the channel
 */
static tty_t* get_tty_for_dev(minor_dev_t minor) {
    return active_tty;
}

/*
 * Check whether a background process tries to read from its controlling terminal. This by definition happens
 * if
 * 1) the process has a controlling terminal
 * 2) the controlling terminal is the device from which we try to read
 * 3) the process group of the process is not the foreground process group of the terminal
 * Return value:
 * -EIO if the process tries to read from its controlling terminal and SIGTTIN is ignored or blocked
 * -EPAUSE if SIGTTIN has been sent to the process group
 */
static int handle_background_read(minor_dev_t minor) {
    __ksigaction_t sa;
    tty_t* tty = 0;
    u32 sigmask;
    pid_t pgrp = do_getpgrp();
    dev_t cterm = pm_get_cterm();
    tty = get_tty_for_dev(minor);
    if (0 == tty) {
        ERROR("Device is not associated with a tty\n");
        return 0;
    }
    if (cterm != DEVICE_NONE) {
        if ((cterm == DEVICE(MAJOR_TTY, minor)) && (tty->pgrp != pgrp)) {
            /*
             * Get the signal disposition for SIGTTIN
             * and the signal mask
             */
            do_sigaction(__KSIGTTIN, 0, &sa);
            do_sigprocmask(0, 0, &sigmask);
            /*
             * If either the signal is blocked or ignored, return -EIO
             */
            if ((sigmask & (1 <<__KSIGTTIN)) || (__KSIG_IGN==sa.sa_handler)) {
                return -EIO;
            }
            /*
             * otherwise, send SIGTTIN to the process group and return -EPAUSE.
             */
            else {
                do_kill(-pgrp, __KSIGTTIN);
                return -EPAUSE;
            }
        }
    }
    return 0;
}



/*
 * Read from a tty
 * Parameter:
 * @minor - minor device number of TTY to read from
 * @size - number of bytes to read
 * @data - pointer to buffer
 * @flags - flags, for instance O_NONBLOCK
 * Return value:
 * -EPAUSE if the operation was interrupted before any data could be read
 * -EIO if a background process tries to read from its controlling terminal and SIGTTIN is blocked or ignored
 * -ENODEV if the minor device is not associated with a tty
 * number of bytes read upon success
 * Locks:
 * tty->available - used to make sure that only one thread can have a pending read at a time
 * tty->data_available - semaphore to keep track on available data in buffer
 * tty->lock - spinlock to protect buffer
 */
ssize_t tty_read(minor_dev_t minor, ssize_t size, void* data, u32 flags) {
    u32 eflags;
    int read;
    int rc;
    tty_t* tty;
    /*
     * Handle background reads
     */
    rc = handle_background_read(minor);
    if (rc)
        return rc;
    /*
     * Get tty
     */
    tty = get_tty_for_dev(minor);
    if (0==tty)
        return -ENODEV;
    /*
     * Block any other thread from reading
     */
    if (flags & O_NONBLOCK) {
        if (-1==sem_down_nowait(&tty->available)) {
            return -EAGAIN;
        }
    }
    else {
        if (-1==sem_down_intr(&tty->available)) {
            return -EPAUSE;
        }
    }
    /*
     * Sleep if no data is available or return with -EAGAIN
     * if O_NONBLOCK is specified
     */
    if (flags & O_NONBLOCK) {
        if (-1==sem_down_nowait(&tty->data_available)) {
            mutex_up(&tty->available);
            return -EAGAIN;
        }
    }
    else {
        if (-1 == sem_down_intr(&tty->data_available)) {
            mutex_up(&tty->available);
            return -EPAUSE;
        }
    }
    /*
     * Handle background reads again - the conditions might have changed
     * while we were sleeping
     */
    rc = handle_background_read(minor);
    if (rc)
        return rc;
    /*
     * At this point, data is present in the buffer
     * Invoke the line disciplines read function to get the data
     */
    spinlock_get(&tty->lock, &eflags);
    read = tty_ld_read(tty, data, size);
    if (tty->read_buffer_end>=0)
        mutex_up(&tty->data_available);
    spinlock_release(&tty->lock, &eflags);
    /*
     * Allow other threads to enter the
     * critical region again
     */
    mutex_up(&tty->available);
    return read;
}


/*
 * Write to a tty
 * Parameter:
 * @minor - minor device
 * @size - number of bytes to write
 * @buffer - data to be written
 * Return value:
 * number of bytes acctually written
 */
ssize_t tty_write(minor_dev_t minor, ssize_t size, void* buffer) {
    tty_t* tty = get_tty_for_dev(minor);
    if (0==tty)
        return -ENODEV;
    return tty_ld_write(tty, buffer, size);
}



/*
 * Initialize tty driver
 */
void tty_init() {
    tty_ld_init(active_tty);
    tty_ops.close = tty_close;
    tty_ops.open = tty_open;
    tty_ops.read = tty_read;
    tty_ops.write = tty_write;
    tty_ops.seek = tty_seek;
    dm_register_char_dev(MAJOR_TTY, &tty_ops);
    if ((irq_add_handler_isa(kbd_isr, 2, 0x1, 0)) < 0) {
        PANIC("Could not register interrupt handler for keyboard interrupt\n");
    }
}


/*
 * Set the foreground process group
 * Parameter:
 * @minor - the minor device
 * @pgrp - the process group
 * Return value:
 * 0 if the operation was successful
 */
int tty_setpgrp(minor_dev_t minor, pid_t pgrp) {
    u32 eflags;
    tty_t* tty = get_tty_for_dev(minor);
    if (0 == tty)
        return -ENODEV;
    spinlock_get(&tty->lock, &eflags);
    tty->pgrp = pgrp;
    spinlock_release(&tty->lock, &eflags);
    return 0;
}

/*
 * Get the foreground process group
 * Parameter:
 * @minor - the minor device
 */
pid_t tty_getpgrp(minor_dev_t minor) {
    pid_t pgrp;
    u32 eflags;
    tty_t* tty = get_tty_for_dev(minor);
    if (0==tty)
        return -ENODEV;
    spinlock_get(&tty->lock, &eflags);
    pgrp = tty->pgrp;
    spinlock_release(&tty->lock, &eflags);
    return pgrp;
}


/*
 * Get the terminal attributes
 * Parameter:
 * @minor - the minor device
 * @termios_p - this is where we store the result
 * Return value:
 * 0 upon success
 * -ENOTTY if the device is not a tty
 */
int tty_tcgetattr(minor_dev_t minor, struct termios* termios_p) {
    u32 eflags;
    tty_t* tty = get_tty_for_dev(minor);
    if (0==tty)
        return -ENOTTY;
    spinlock_get(&tty->lock, &eflags);
    if (termios_p)
        *termios_p = tty->settings;
    spinlock_release(&tty->lock, &eflags);
    return 0;
}

/*
 * Set the terminal attributes
 * Parameter:
 * @minor - the minor device
 * @action - TCSANOW, TCSADRAIN or TCSAFLUSH
 * @termios_p - value to be set
 * Return value:
 * 0 upon success
 * -ENOTTY if the device is not a tty
 */
int tty_tcsetattr(minor_dev_t minor, int action, struct termios* termios_p) {
    u32 eflags;
    tty_t* tty = get_tty_for_dev(minor);
    if (0==tty)
        return -ENOTTY;
    spinlock_get(&tty->lock, &eflags);
    if (termios_p) {
        if (TCSAFLUSH==action)
            tty_ld_flush(tty);
        tty->settings = *termios_p;
    }
    spinlock_release(&tty->lock, &eflags);
    return 0;
}


/*
 * Receive a sequence of characters from the low-level device driver and
 * place it in the queue of incoming chars. Note that one key event on the keyboard
 * can generate more than one character (like the arrow keys which generate an
 * escape sequence), thus we receive a string as input, not a character. If the buffer
 * is already filled up, the entire string is discarded
 * Parameters:
 * @channel - the input channel
 * @input - the characters
 * @nbytes - number of characters
 */
void tty_put (int channel, unsigned char* input, size_t nbytes) {
    u32 eflags;
    tty_t* tty = get_tty_for_channel(channel);
    spinlock_get(&tty->lock, &eflags);
    if (tty_ld_put(tty, input, nbytes)) {
        mutex_up(&tty->data_available);
    }
    spinlock_release(&tty->lock, &eflags);
}
