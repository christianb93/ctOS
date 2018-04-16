/*
 * ioctl.c
 *
 */

#include "lib/sys/ioctl.h"
#include "lib/os/oscalls.h"

/*
 * IOCTL implementation mapping to Linux ioctl
 */
int __ctOS_ioctl(int fd, unsigned int request, unsigned int arg) {
    return -1;
}
