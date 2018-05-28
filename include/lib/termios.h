/*
 * termios.h
 */

#ifndef _TERMIOS_H_
#define _TERMIOS_H_

#include "sys/types.h"

typedef unsigned int cc_t;
typedef unsigned int speed_t;
typedef unsigned int tcflag_t;

#define NCCS 16

struct termios {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    cc_t c_cc[NCCS];
    /*
     * The fields below are not standardized
     */
    speed_t orate;
    speed_t irate;
};

#define VINTR 0
#define VQUIT 1
#define VERASE 2
#define VKILL 3
#define VEOF 4
#define VTIME 5
#define VMIN 6
#define VSTART 8
#define VSTOP 9
#define VSUSP 10
#define VEOL 11


/*
 * Bitmasks for c_iflag
 */
#define ICRNL 0x1
#define INLCR 0x2
#define IGNCR 0x4
#define ISTRIP 0x8
#define IXON 0x10
#define BRKINT 0x20
#define PARMRK 0x40
#define IGNBRK 0x80
#define IGNPAR 0x100
#define IXOFF 0x200
#define INPCK 0x400

/*
 * Bits for c_oflag
 */
#define OPOST 0x1
#define ONLCR 0x2
#define OCRNL 0x4
#define ONOCR 0x8

/*
 * Bitmasks for c_lflag. Note that ECHOCTL is a BSD
 * extension
 */
#define ECHO 0x1
#define ICANON 0x2
#define ECHONL 0x4
#define IEXTEN 0x8
#define ISIG 0x10
#define NOFLSH 0x20
#define ECHOK 0x40
#define ECHOE 0x80
#define ECHOCTL 0x100

/*
 * Bitmasks and values for c_cflag
 */
#define CSIZE 0x3
#define CS8 0x3
#define CSTOPB 0x4
#define CREAD 0x8
#define PARENB 0x10
#define PARODD 0x20
#define HUPCL 0x40
#define CLOCAL 0x80
#define CS5 0x100
#define CS6 0x100
#define CS7 0x100

/*
 * Baud rates
 *
 */
#define B0 1
#define B50 2
#define B75 3
#define B110 4
#define B134 5
#define B150 6
#define B200 7
#define B300 8
#define B600 9
#define B1200 10
#define B1800 11
#define B2400 12
#define B4800 13
#define B9600 14
#define B19200 15
#define B38400 16

/*
 * Modes for tcsetattr
 */
#define TCSADRAIN 1
#define TCSANOW 2
#define TCSAFLUSH 4


/*
 * Modes for tcflush
 */
#define TCIFLUSH 1


int tcgetattr(int fd, struct termios* termios_p);
int tcsetattr(int fd, int action, struct termios* termios_p);
speed_t cfgetospeed(const struct termios *termios_p);
speed_t cfgetispeed(const struct termios *termios_p);
int cfsetispeed(struct termios *, speed_t);
int cfsetospeed(struct termios *, speed_t);

#endif /* _TERMIOS_H_ */
