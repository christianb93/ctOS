/*
 * termios.c
 *
 */

#include "lib/os/oscalls.h"
#include "lib/termios.h"
#include "lib/errno.h"

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
 * Get baud rate of a terminal
 */
speed_t cfgetospeed(const struct termios *termios_p) {
    return termios_p->orate;
}
