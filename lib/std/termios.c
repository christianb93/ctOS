/*
 * termios.c
 *
 */

#include "lib/os/oscalls.h"
#include "lib/termios.h"
#include "lib/errno.h"
#include "lib/stdio.h"

static int _valid_baud_rates[] = {B0, B50, B75, B110, B134, B150, B200, B300, B600, B1200, B1800, B2400, B4800, B9600, B19200, B38400 };

/*
 * Internal helper function to check whether
 * a baud rate is valid
 */
static int baud_rate_valid(speed_t baud_rate) {
    for (int i = 0; i < (sizeof(_valid_baud_rates) / sizeof(speed_t)); i++) {
        if (_valid_baud_rates[i] == baud_rate) {
            return 1;
        }
    }
    return 0;
}

/*
 * Get attributes of a terminal
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS: none
 */
int tcgetattr(int fd, struct termios* termios_p) {
    if (0==termios_p)
        return 0;
    int rc = __ctOS_tcgetattr(fd, termios_p);
    if (0==rc) {
        return 0;
    }
    errno = -rc;
    return -1;
}

/*
 * Set attributes of a terminal
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS: none
 */
int tcsetattr(int fd, int action, struct termios* termios_p) {
    if (0==termios_p)
        return -1;
    int rc = __ctOS_tcsetattr(fd, action, termios_p);
    if (0==rc) {
        return 0;
    }
    errno = -rc;
    return -1;
}

/*
 * Get output baud rate of a terminal
 */
speed_t cfgetospeed(const struct termios *termios_p) {
    return termios_p->orate;
}


/*
 * Get input baud rate of a terminal
 */
speed_t cfgetispeed(const struct termios *termios_p) {
    return termios_p->irate;
}

/*
 * Set input baud rate of a terminal.
 * 
 * If the baud rate is not one of the symbols B*
 * defined in termios.h, the function returns EINVAL
 */
int cfsetispeed(struct termios * term, speed_t speed) {
    if (0 == baud_rate_valid(speed)) {
        return EINVAL;
    }
    term->irate = speed;
    return 0;
}

/*
 * Set output baud rate of a terminal. 
 * 
 * If the baud rate is not one of the symbols B*
 * defined in termios.h, the function returns EINVAL
 * 
 */
int cfsetospeed(struct termios * term, speed_t speed) {
    if (0 == baud_rate_valid(speed)) {
        return EINVAL;
    }
    term->orate = speed;
    return 0;
}