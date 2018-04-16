/*
 * test_getopt.c
 *
 */

#include "kunit.h"
#include <unistd.h>
#include <stdio.h>

/*
 * Testcase 1: verify that optind is initially 1
 */
int testcase1() {
    ASSERT(1==optind);
    return 0;
}

/*
 * Testcase 2: locate an option
 */
int testcase2() {
    char* argv[2];
    argv[0]="test";
    argv[1]="-a";
    ASSERT('a'==getopt(2, argv, "a"));
    return 0;
}

/*
 * Testcase 3: reset and locate two options specified in the same
 * argument.
 */
int testcase3() {
    char* argv[2];
    argv[0]="test";
    argv[1]="-ab";
    optind = 0;
    ASSERT('a'==getopt(2, argv, "ab"));
    ASSERT('b'==getopt(2, argv, "ab"));
    ASSERT(-1==getopt(2, argv, "ab"));
    return 0;
}

/*
 * Testcase 4: reset and locate two options specified in different arguments
 */
int testcase4() {
    char* argv[3];
    argv[0]="test";
    argv[1]="-a";
    argv[2]="-b";
    optind = 0;
    ASSERT('a'==getopt(3, argv, "ab"));
    ASSERT(2==optind);
    ASSERT('b'==getopt(3, argv, "ab"));
    ASSERT(3==optind);
    ASSERT(-1==getopt(3, argv, "ab"));
    return 0;
}

/*
 * Testcase 5: reset and test behaviour for an empty argument
 */
int testcase5() {
    char* argv[3];
    argv[0]="test";
    argv[1]="-";
    argv[2]="-b";
    optind = 0;
    ASSERT('b'==getopt(3, argv, "ab"));
    ASSERT(3==optind);
    return 0;
}

/*
 * Testcase 6: reset and test behaviour for no arguments at all
 *  (argc==1)
 */
int testcase6() {
    char* argv[3];
    argv[0]="test";
    argv[1]="";
    argv[2]="";
    optind = 0;
    ASSERT(-1==getopt(1, argv, "ab"));
    return 0;
}

/*
 * Testcase 7: reset and test behaviour for no arguments at all
 * (argc=2, but argument is the empty string)
 */
int testcase7() {
    char* argv[3];
    argv[0]="test";
    argv[1]="";
    argv[2]="";
    optind = 0;
    ASSERT(-1==getopt(2, argv, "ab"));
    return 0;
}

/*
 * Testcase 8: reset and test an option with argument - argument in separate argv entry
 */
int testcase8() {
    char* argv[3];
    argv[0]="test";
    argv[1]="-a";
    argv[2]="x";
    optind = 0;
    ASSERT('a'==getopt(3, argv, "a:"));
    ASSERT(optarg);
    ASSERT(0==strcmp(optarg, "x"));
    return 0;
}


/*
 * Testcase 9: reset and test an option with argument - argument is missing
 */
int testcase9() {
    char* argv[2];
    argv[0]="test";
    argv[1]="-a";
    optind = 0;
    ASSERT('?'==getopt(2, argv, "a:"));
    return 0;
}

/*
 * Testcase 10: reset and test an option with argument - argument is in same argv
 */
int testcase10() {
    char* argv[2];
    argv[0]="test";
    argv[1]="-ax";
    optind = 0;
    ASSERT('a'==getopt(2, argv, "a:"));
    ASSERT(optarg);
    ASSERT(0==strcmp(optarg, "x"));
    ASSERT(2==optind);
    return 0;
}

/*
 * Testcase 11: reset and test an option with argument - argument is in same argv and has more than one characters
 */
int testcase11() {
    char* argv[2];
    argv[0]="test";
    argv[1]="-axy";
    optind = 0;
    ASSERT('a'==getopt(2, argv, "a:"));
    ASSERT(optarg);
    ASSERT(0==strcmp(optarg, "xy"));
    ASSERT(2==optind);
    return 0;
}

/*
 * Testcase 12: special argument --
 */
int testcase12() {
    char* argv[2];
    argv[0]="test";
    argv[1]="--";
    optind = 0;
    ASSERT(-1==getopt(2, argv, "a:"));
    ASSERT(2==optind);
    return 0;
}

/*
 * Testcase 13: an option character is detected which is not in the list of valid options
 */
int testcase13() {
    char* argv[2];
    argv[0]="test";
    argv[1]="-x";
    optind = 0;
    ASSERT('?'==getopt(2, argv, "a:"));
    return 0;
}

/*
 * Testcase 14: reset and test an option with argument - argument is missing, first entry in option list is :
 */
int testcase14() {
    char* argv[2];
    argv[0]="test";
    argv[1]="-a";
    optind = 0;
    ASSERT(':'==getopt(2, argv, ":a:"));
    ASSERT(optopt=='a');
    return 0;
}

/*
 * Testcase 15: reset and test an argument not starting with -
 * This should not change optind
 */
int testcase15() {
    char* argv[2];
    argv[0]="test";
    argv[1]="a";
    optind = 0;
    ASSERT(-1==getopt(2, argv, ":a:"));
    ASSERT(optind==1);
    return 0;
}

/*
 * Testcase 16: test processing in case there is an additional non-option argument
 */
int testcase16() {
    char* argv[4];
    argv[1] = "-s";
    argv[2] = "127.0.0.1";
    argv[3] = "www.google.de";
    optind = 0;
    ASSERT('s' == getopt(4, argv, "s:"));
    ASSERT(-1 == getopt(4, argv, "s:"));
    ASSERT(3 == optind);
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
    RUN_CASE(8);
    RUN_CASE(9);
    RUN_CASE(10);
    RUN_CASE(11);
    RUN_CASE(12);
    RUN_CASE(13);
    RUN_CASE(14);
    RUN_CASE(15);
    RUN_CASE(16);
    END;
}
