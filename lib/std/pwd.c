/*
 * pwd.c
 *
 */
#include "lib/pwd.h"
#include "lib/os/pwddb.h"
#include "lib/errno.h"

static struct passwd __current;

/*
 * Get entry from the user database for a given name
 */
struct passwd* getpwnam(const char *name) {
    struct __ctOS_passwd* res;
    int error;
    res = __ctOS_getpwnam(name, &error);
    if (0 == res) {
        if (error)
            errno = error;
        return 0;
    }
    __current.pw_dir = res->pw_dir;
    __current.pw_gid = res->pw_gid;
    __current.pw_name = res->pw_name;
    __current.pw_shell = res->pw_shell;
    __current.pw_uid = res->pw_uid;
    return &__current;
}

/*
 * Given a UID, return the entry from the password database for that UID or 0 if no matching entry could be found
 */
struct passwd* getpwuid(uid_t uid) {
    struct __ctOS_passwd* res;
    int error;
    res = __ctOS_getpwuid(uid, &error);
    if (0 == res) {
        if (error)
            errno = error;
        return 0;
    }
    __current.pw_dir = res->pw_dir;
    __current.pw_gid = res->pw_gid;
    __current.pw_name = res->pw_name;
    __current.pw_shell = res->pw_shell;
    __current.pw_uid = res->pw_uid;
    return &__current;
}
