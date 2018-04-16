/*
 * pwddb.h
 *
 *  Created on: Oct 1, 2012
 *      Author: chr
 */

#ifndef _PWDDB_H_
#define _PWDDB_H_

#include "types.h"


/*
 * This structure defines an entry in the password database. Note that pw_passwd and pw_gecos are
 * not required by POSIX. The order of fields is as in /etc/passwd
 */
struct __ctOS_passwd {
    char* pw_name;         // User's login name
    char* pw_passwd;       // Password
    uid_t pw_uid;          // Numerical user ID
    gid_t pw_gid;          // Numerical group ID
    char* pw_gecos;        // User information, typically the name or a comment
    char* pw_dir;          // Initial working directory
    char* pw_shell;        //  Program to use as shell
};


struct __ctOS_passwd* __ctOS_getpwent();
void __ctOS_endpwent();
struct __ctOS_passwd* __ctOS_getpwuid(uid_t uid, int* error);
struct __ctOS_passwd* __ctOS_getpwnam(const char* name, int* error);

#endif /* _PWDDB_H_ */
