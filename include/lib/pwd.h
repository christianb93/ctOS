/*
 * pwd.h
 *
 */

#ifndef _PWD_H_
#define _PWD_H_

#include "sys/types.h"

struct passwd {
    char* pw_name;  //  User's login name.
    uid_t pw_uid;   //  Numerical user ID.
    gid_t pw_gid;   //  Numerical group ID.
    char* pw_dir;   //  Initial working directory.
    char* pw_shell; //  Program to use as shell
};

struct passwd* getpwnam(const char *name);
struct passwd* getpwuid(uid_t uid);

#endif /* _PWD_H_ */
