
#ifndef _SYS_UTSNAME_H
#define _SYS_UTSNAME_H



struct utsname {
    char  sysname[9];
    char  nodename[9];
    char  release[9];
    char  version[17];  
    char  machine[9];
};

int uname(struct utsname *);

#endif