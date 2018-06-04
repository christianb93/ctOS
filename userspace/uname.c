/*
 * uname.c
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/utsname.h>

int main() {
    struct utsname _utsname;
    uname(&_utsname);
    printf("%s release %s (version %s) on %s, machine type is %s\n", 
                _utsname.sysname, 
                _utsname.release, 
                _utsname.version, 
                _utsname.nodename, 
                _utsname.machine);
    return 0;
}

