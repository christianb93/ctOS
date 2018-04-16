/*
 * test_pwd.c
 *
 *  Created on: Oct 1, 2012
 *      Author: chr
 */


#include "kunit.h"
#include "lib/os/pwddb.h"
#include <pwd.h>


/*
 * Testcase 1: getpwent to read first entry
 */
int testcase1() {
    struct __ctOS_passwd* res;
    struct passwd* baseline;
    /*
     * Get first entry from password file - this will also open the file
     */
    baseline = getpwent();
    res = __ctOS_getpwent();
    /*
     * Now compare field by field
     */
    ASSERT(baseline);
    ASSERT(res);
    ASSERT(res->pw_gid == baseline->pw_gid);
    ASSERT(res->pw_uid == baseline->pw_uid);
    ASSERT(0 == strcmp(res->pw_dir, baseline->pw_dir));
    ASSERT(0 == strcmp(res->pw_name, baseline->pw_name));
    ASSERT(0 == strcmp(res->pw_shell, baseline->pw_shell));
    /*
     * Now close both databases again
     */
    endpwent();
    __ctOS_endpwent();
    return 0;
}

/*
 * Testcase 2: endpwent
 */
int testcase2() {
    struct __ctOS_passwd* res;
    uid_t uid;
    /*
     * Get first entry from password file - this will also open the file
     */
    res = __ctOS_getpwent();
    uid = res->pw_uid;
    /*
     * Now close database again
     */
    __ctOS_endpwent();
    /*
     * Reading the next entry should now return the first entry again
     */
    res = __ctOS_getpwent();
    ASSERT(uid == res->pw_uid);
    __ctOS_endpwent();
    return 0;
}

/*
 * Testcase 3: use getpwent to walk entire file
 */
int testcase3() {
    struct __ctOS_passwd* res;
    struct passwd* baseline;
    /*
     * Make sure that both databases are reset
     */
    endpwent();
    __ctOS_endpwent();
    /*
     * First we walk the file using our function
     */
    while ((res = __ctOS_getpwent())) {
        baseline = getpwent();
        ASSERT(baseline);
        /*
         * Now compare field by field
         */
        ASSERT(res->pw_gid == baseline->pw_gid);
        ASSERT(res->pw_uid == baseline->pw_uid);
        ASSERT(0 == strcmp(res->pw_dir, baseline->pw_dir));
        ASSERT(0 == strcmp(res->pw_name, baseline->pw_name));
        ASSERT(0 == strcmp(res->pw_shell, baseline->pw_shell));
    }
    /*
     * Close both databases again
     */
    endpwent();
    __ctOS_endpwent();
    /*
     * and repeat this using the baseline implementation as driver
     */
    while ((baseline = getpwent())) {
        res = __ctOS_getpwent();
        ASSERT(res);
        /*
         * Now compare field by field
         */
        ASSERT(res->pw_gid == baseline->pw_gid);
        ASSERT(res->pw_uid == baseline->pw_uid);
        ASSERT(0 == strcmp(res->pw_dir, baseline->pw_dir));
        ASSERT(0 == strcmp(res->pw_name, baseline->pw_name));
        ASSERT(0 == strcmp(res->pw_shell, baseline->pw_shell));
    }
    return 0;
}

/*
 * Testcase 4: test getpwuid - first entry
 */
int testcase4() {
    struct __ctOS_passwd* res;
    struct passwd* baseline;
    int error = 0;
    res = __ctOS_getpwuid(0, &error);
    baseline = getpwuid(0);
    /*
     * Now compare field by field
     */
    ASSERT(baseline);
    ASSERT(res);
    ASSERT(0 == error);
    ASSERT(0 == res->pw_uid);
    ASSERT(res->pw_gid == baseline->pw_gid);
    ASSERT(res->pw_uid == baseline->pw_uid);
    ASSERT(0 == strcmp(res->pw_dir, baseline->pw_dir));
    ASSERT(0 == strcmp(res->pw_name, baseline->pw_name));
    ASSERT(0 == strcmp(res->pw_shell, baseline->pw_shell));
    return 0;
}

/*
 * Testcase 5: test getpwuid - use an entry different from the first one
 */
int testcase5() {
    struct __ctOS_passwd* res;
    struct passwd* baseline;
    uid_t uid;
    int error = 0;
    /*
     * Get UID of second entry
     */
    __ctOS_endpwent();
    res = __ctOS_getpwent();
    res = __ctOS_getpwent();
    ASSERT(res);
    uid = res->pw_uid;
    /*
     * Now search for this record
     */
    res = __ctOS_getpwuid(uid, &error);
    ASSERT(0 == error);
    baseline = getpwuid(uid);
    /*
     * Now compare field by field
     */
    ASSERT(baseline);
    ASSERT(res);
    ASSERT(uid == res->pw_uid);
    ASSERT(res->pw_gid == baseline->pw_gid);
    ASSERT(res->pw_uid == baseline->pw_uid);
    ASSERT(0 == strcmp(res->pw_dir, baseline->pw_dir));
    ASSERT(0 == strcmp(res->pw_name, baseline->pw_name));
    ASSERT(0 == strcmp(res->pw_shell, baseline->pw_shell));
    return 0;
}

/*
 * Testcase 6: test getpwnam - first entry
 */
int testcase6() {
    struct __ctOS_passwd* res;
    struct passwd* baseline;
    int error = 0;
    res = __ctOS_getpwnam("root", &error);
    baseline = getpwnam("root");
    /*
     * Now compare field by field
     */
    ASSERT(baseline);
    ASSERT(res);
    ASSERT(0 == error);
    ASSERT(0 == res->pw_uid);
    ASSERT(res->pw_gid == baseline->pw_gid);
    ASSERT(res->pw_uid == baseline->pw_uid);
    ASSERT(0 == strcmp(res->pw_dir, baseline->pw_dir));
    ASSERT(0 == strcmp(res->pw_name, baseline->pw_name));
    ASSERT(0 == strcmp(res->pw_shell, baseline->pw_shell));
    return 0;
}

/*
 * Testcase 7: test getpwnam - use an entry different from the first one
 */
int testcase7() {
    struct __ctOS_passwd* res;
    struct passwd* baseline;
    char name[256];
    int error = 0;
    /*
     * Get name of second entry
     */
    __ctOS_endpwent();
    res = __ctOS_getpwent();
    res = __ctOS_getpwent();
    ASSERT(res);
    strcpy(name, res->pw_name);
    /*
     * Now search for this record
     */
    res = __ctOS_getpwnam(name, &error);
    ASSERT(0 == error);
    baseline = getpwnam(name);
    /*
     * Now compare field by field
     */
    ASSERT(baseline);
    ASSERT(res);
    ASSERT(baseline->pw_uid == res->pw_uid);
    ASSERT(res->pw_gid == baseline->pw_gid);
    ASSERT(res->pw_uid == baseline->pw_uid);
    ASSERT(0 == strcmp(res->pw_dir, baseline->pw_dir));
    ASSERT(0 == strcmp(res->pw_name, baseline->pw_name));
    ASSERT(0 == strcmp(res->pw_shell, baseline->pw_shell));
    return 0;
}

int main() {
    INIT;
    RUN_CASE(1);
    RUN_CASE(2);
    RUN_CASE(3);
    RUN_CASE(4);
    RUN_CASE(5);
    RUN_CASE(6);
    RUN_CASE(7);
    END;
}
