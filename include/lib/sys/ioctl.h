/*
 * ioctl.h
 *
 */

#ifndef _IOCTL_H_
#define _IOCTL_H_


/*
 * TTY ioctls
 */
#define TIOCGPGRP 0x1   // get process group
#define TIOCSPGRP 0x2   // set process group
#define TIOCGETD  0x3   // get line discipline

/*
 * Socket ioctls
 */
#define SIOCSIFNETMASK 0x21 // Set network mask
#define SIOCGIFNETMASK 0x22 // get network mask
#define SIOCSIFADDR 0x23    // set interface address
#define SIOCGIFADDR 0x24    // get interface address
#define SIOCGIFCONF 0x25    // return list of interfaces
#define SIOCGRTCONF 0x26    // return content of kernel routing table
#define SIOCADDRT 0x27      // Add route
#define SIOCDELRT 0x28      // Delete route
#define SIOCADDNS 0x29      // add DNS server
#define SIOCDELNS 0x2a      // delete DNS server

/*
 * TTY line discipline
 */
#define NTTYDISC 2

int ioctl(int fd, unsigned int request, ...);

#endif /* _IOCTL_H_ */
