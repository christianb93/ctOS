/*
 * dump_constants.c
 *
 * Utility function to dump a few constants
 */

#include <stdio.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <termios.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <fnmatch.h>
#include <locale.h>

int main() {
    sigset_t my_sigset;
    printf("O_CREAT = %x (decimal %d)\n", O_CREAT, O_CREAT);
    printf("_IOFBF = %x (decimal %d)\n", _IOFBF, _IOFBF);
    printf("_IOLBF = %x (decimal %d)\n", _IOLBF, _IOLBF);
    printf("_IONBF = %x (decimal %d)\n", _IONBF, _IONBF);
    printf("O_RDONLY = %x (decimal %d)\n", O_RDONLY, O_RDONLY);
    printf("O_RDWR = %x (decimal %d)\n", O_RDWR, O_RDWR);
    printf("O_TRUNC = %x (decimal %d)\n", O_TRUNC, O_TRUNC);
    printf("O_APPEND = %x (decimal %d)\n", O_APPEND, O_APPEND);
    printf("O_WRONLY = %x (decimal %d)\n", O_WRONLY, O_WRONLY);
    printf("O_NONBLOCK = %x (decimal %d)\n", O_NONBLOCK, O_NONBLOCK);
    printf("O_EXCL = %x (decimal %d)\n", O_EXCL, O_EXCL);
    printf("INT_MAX = %x (decimal %d)\n", INT_MAX, INT_MAX);
    printf("S_ISUID = %o (decimal %d)\n", S_ISUID, S_ISUID);
    printf("S_ISGID = %o (decimal %d)\n", S_ISGID, S_ISGID);
    printf("S_ISVTX = %o (decimal %d)\n", S_ISVTX, S_ISVTX);
    printf("sizeof(unsigned long int)=%ld\n", sizeof(unsigned long int));
    printf("sizeof(sigset_t)=%ld\n", sizeof(sigset_t));
    printf("sizeof(long long int)=%ld\n", sizeof(long long int));
    printf("sizeof(sigaction_t)=%ld\n", sizeof(struct sigaction));
    printf("VKILL: %d (hex %x)\n", VKILL, VKILL );
    printf("VTIME: %d (hex %x)\n", VTIME, VTIME );
    printf("VSTART: %d (hex %x)\n", VSTART, VSTART );
    printf("VSTOP: %d (hex %x)\n", VSTOP, VSTOP );
    printf("VQUIT: %d (hex %x)\n", VQUIT, VQUIT );
    printf("SOL_SOCKET: %d (hex %x)\n", SOL_SOCKET, SOL_SOCKET );
    printf("SO_RCVBUF: %d (hex %x)\n", SO_RCVBUF, SO_RCVBUF );
    printf("AF_INET: %d (hex %x)\n", AF_INET, AF_INET);
    printf("SOCK_DGRAM: %d (hex %x)\n", SOCK_DGRAM, SOCK_DGRAM);
    printf("SOL_SOCKET: %d (hex %x)\n", SOL_SOCKET, SOL_SOCKET);
    printf("SO_RCVTIMEO: %d (hex %x)\n", SO_RCVTIMEO, SO_RCVTIMEO);
    printf("SO_SNDTIMEO: %d (hex %x)\n", SO_SNDTIMEO, SO_SNDTIMEO);
    printf("sizeof(struct sockaddr_in): %ld\n", sizeof(struct sockaddr_in));
    printf("EINTR: %d (hex %x)\n", EINTR, EINTR);
    printf("MB_CUR_MAX:  %d (hex %x)\n", (int) MB_CUR_MAX, (int) MB_CUR_MAX);
    printf("FNM_NOMATCH:  %d (hex %x)\n", FNM_NOMATCH, FNM_NOMATCH);
    printf("FNM_PATHNAME:  %d (hex %x)\n", FNM_PATHNAME, FNM_PATHNAME);
    printf("FNM_NOESCAPE:  %d (hex %x)\n", FNM_NOESCAPE, FNM_NOESCAPE);
    printf("LC_ALL:  %d (hex %x)\n", LC_ALL, LC_ALL);
    printf("LC_TIME:  %d (hex %x)\n", LC_TIME, LC_TIME);
}

