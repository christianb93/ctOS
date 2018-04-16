/*
 * termios.c
 *
 */


#include "lib/termios.h"
#include "lib/os/syscalls.h"
#include "lib/os/oscalls.h"

/*
 * tcgetattr
 */
int __ctOS_tcgetattr(int fd, struct termios* termios_p) {
    return __ctOS_syscall(__SYSNO_TCGETATTR, 2, fd, termios_p);
}


/*
 * tcsetattr
 */
int __ctOS_tcsetattr(int fd, int action, struct termios* termios_p) {
    return __ctOS_syscall(__SYSNO_TCSETATTR, 3, fd, action, termios_p);
}
