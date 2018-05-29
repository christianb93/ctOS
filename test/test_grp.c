#include "kunit.h"
#include "lib/grp.h"
#include <stdio.h>

/*
 * Testcase 1: call getgrnam for root
 */
int testcase1() {
    struct group* mygroup;
    mygroup = getgrnam("root");
    ASSERT(mygroup);
    /*
     * Check that the name is root and the id is 0
     */
    ASSERT(0 == mygroup->gr_gid);
    ASSERT(0 == strcmp(mygroup->gr_name, "root"));
    /*
     * Check that there is exactly one user in the group
     */
    ASSERT(mygroup->gr_mem);
    ASSERT(0 == mygroup->gr_mem[1]);
    /*
     * and that this is root
     */
    ASSERT(0 == strcmp(mygroup->gr_mem[0], "root"));
    return 0;
}

/*
 * Testcase 2: call getgrnam for non-root
 */
int testcase2() {
    struct group* mygroup;
    mygroup = getgrnam("chr");
    ASSERT(0 == mygroup);
    return 0;
}

/*
 * Testcase 3: call getgrgid for root
 */
int testcase3() {
    struct group* mygroup;
    mygroup = getgrgid(0);
    ASSERT(mygroup);
    /*
     * Check that the name is root and the id is 0
     */
    ASSERT(0 == mygroup->gr_gid);
    ASSERT(0 == strcmp(mygroup->gr_name, "root"));
    /*
     * Check that there is exactly one user in the group
     */
    ASSERT(mygroup->gr_mem);
    ASSERT(0 == mygroup->gr_mem[1]);
    /*
     * and that this is root
     */
    ASSERT(0 == strcmp(mygroup->gr_mem[0], "root"));
    return 0;
}



/*
 * Testcase 4: call getgrgid for non-root
 */
int testcase4() {
    struct group* mygroup;
    mygroup = getgrgid(1);
    ASSERT(0 == mygroup);
    return 0;
}

int main() {
    INIT;
    RUN_CASE(1);
    RUN_CASE(2);
    RUN_CASE(3);
    RUN_CASE(4);
    END;
}
