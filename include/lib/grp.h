#ifndef _GRP_H_
#define _GRP_H_

#include "sys/types.h"

/* 
 * The group structure
 * according to POSIX
 */
struct group
{
    char *gr_name;
    gid_t gr_gid;
    char **gr_mem;  /* Pointer to a null-terminated array of members */
};

struct group *getgrnam(const char *name);
struct group *getgrgid(gid_t gid);

#endif