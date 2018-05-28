/*
 * test_tools.c
 *
 */


#include "kunit.h"
#include <mntent.h>


/*
 * Testcase 1: call setmntent 
 */
int testcase1() {
    ASSERT(setmntent(MOUNTED, "r"));
    return 0;
}

/*
 * Testcase 2: call setmntent followed by endmntent 
 */
int testcase2() {
    FILE* fp;
    fp = setmntent(MOUNTED, "r");
    ASSERT(fp);
    ASSERT(1 == endmntent(fp));
    return 0;
}

/*
 * Testcase 3: read one line from mtab
 */
int testcase3() {
    FILE* fp;
    struct mntent* result = 0;
    fp = setmntent(MOUNTED, "r");
    ASSERT(fp);
    result = getmntent(fp);
    ASSERT(result);
    endmntent(fp);
    return 0;
}

/*
 * Testcase 4: read line by line
 */
int testcase4() {
    FILE* fp;
    struct mntent* result = 0;
    fp = setmntent(MOUNTED, "r");
    ASSERT(fp);
    while(getmntent(fp)) {
        
    }
    endmntent(fp);
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
