#include "lib/sys/utsname.h"
#include "lib/string.h"

/*
 * Implementation of the uname function
 *
 * Currently all these values are hardcoded
 */

int uname(struct utsname* _utsname) {
    if (0 == _utsname)
        return -1;
    strcpy(_utsname->machine,  "x86_PC");
    strcpy(_utsname->nodename, "myhost");
    strcpy(_utsname->release,  "v0.3.0");
    strncpy(_utsname->version, __DATE__, 16);
    _utsname->version[16] = 0;
    strcpy(_utsname->sysname, "ctOS");
    return 0;
}