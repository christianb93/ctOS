#ifndef _GRP_H_
#define _GRP_H_

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


#endif