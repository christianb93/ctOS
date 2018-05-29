/*
 * grp.c
 */

#include "lib/grp.h"
#include "lib/string.h"

/*
 * Static entry that we use. Note that according to POSIX, it is perfectly fine 
 * to use a static entry
 */
static struct group _ctOS_root_group;
static char* _ctOS_root_group_members[] = {"root", 0};

/*
 * Return a group structure for the group identified by 
 * name
 * 
 * For the time being, there is no group database like /etc/groups in ctOS, and
 * there is only one group - root
 */
struct group *getgrnam(const char *name) {
    if (0 == strcmp(name, "root")) {
        _ctOS_root_group.gr_mem = _ctOS_root_group_members;
        _ctOS_root_group.gr_gid = 0;
        _ctOS_root_group.gr_name = "root";
        return &_ctOS_root_group;
    }
    else
        return 0;
}

/*
 * Return a group structure for the group identified by 
 * the group id
  */
struct group *getgrgid(gid_t gid) {
    if (0 == gid) {
        return getgrnam("root");
    }
    return 0;
}