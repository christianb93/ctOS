/*
 * test_fnmatch.c
 *
 */


#include <fnmatch.h>
#include <stdio.h>
#include <stdlib.h>
#include "kunit.h"

/* Testcase 1
 * Tested function: fnmatch
 * Testcase: try to match two identical strings with ordinary characters only
 */
int testcase1() {
    ASSERT(0 == fnmatch("abc", "abc", 0));
    return 0;
}

/* Testcase 2
 * Tested function: fnmatch
 * Testcase: try to match two strings with ordinary characters - pattern too short
 */
int testcase2() {
    ASSERT(FNM_NOMATCH == fnmatch("ab", "abc", 0));
    return 0;
}

/* Testcase 3
 * Tested function: fnmatch
 * Testcase: try to match two strings with ordinary characters - pattern too long
 */
int testcase3() {
    ASSERT(FNM_NOMATCH == fnmatch("abc", "ab", 0));
    return 0;
}

/* Testcase 4
 * Tested function: fnmatch
 * Testcase: try to match two strings with ordinary characters - strings not equal
 */
int testcase4() {
    ASSERT(FNM_NOMATCH == fnmatch("abd", "abc", 0));
    return 0;
}

/* Testcase 5
 * Tested function: fnmatch
 * Testcase: try to match two strings - use ?
 */
int testcase5() {
    ASSERT(0 == fnmatch("ab?", "abc", 0));
    return 0;
}

/* Testcase 6
 * Tested function: fnmatch
 * Testcase: try to match two strings - use ?, pattern too long
 */
int testcase6() {
    ASSERT(FNM_NOMATCH == fnmatch("ab?", "ab", 0));
    return 0;
}

/* Testcase 7
 * Tested function: fnmatch
 * Testcase: try to match two strings - use ?, make sure that it matches only one character
 */
int testcase7() {
    ASSERT(FNM_NOMATCH == fnmatch("a?c", "abbc", 0));
    return 0;
}

/* Testcase 8
 * Tested function: fnmatch
 * Testcase: try to match two strings - use ?, special case of /, no FNM_PATHNAME
 */
int testcase8() {
    ASSERT(0 == fnmatch("ab?", "ab/", 0));
    return 0;
}

/* Testcase 9
 * Tested function: fnmatch
 * Testcase: try to match two strings - use ?, special case of /, FNM_PATHNAME set
 */
int testcase9() {
    ASSERT(FNM_NOMATCH == fnmatch("ab?", "ab/", FNM_PATHNAME));
    return 0;
}

/*
 * Testcase 10
 * Tested function: fnmatch
 * Testcase: test that a * matches any sequence of characters - * only pattern
 */
int testcase10 () {
    ASSERT(0 == fnmatch("*", "abc", 0));
    return 0;
}

/*
 * Testcase 11
 * Tested function: fnmatch
 * Testcase: test that a * matches any sequence of characters - two *
 */
int testcase11 () {
    ASSERT(0 == fnmatch("**", "abc", 0));
    return 0;
}

/*
 * Testcase 12
 * Tested function: fnmatch
 * Testcase: test that a * matches any sequence of characters - * followed by a ?
 */
int testcase12 () {
    ASSERT(0 == fnmatch("*?", "abc", 0));
    return 0;
}

/*
 * Testcase 13
 * Tested function: fnmatch
 * Testcase: test that a * matches any sequence of characters - * followed by another character which matches
 */
int testcase13 () {
    ASSERT(0 == fnmatch("*c", "abc", 0));
    return 0;
}

/*
 * Testcase 14
 * Tested function: fnmatch
 * Testcase: test that a * matches any sequence of characters - * followed by another character which does not match
 */
int testcase14 () {
    ASSERT(FNM_NOMATCH == fnmatch("*c", "abd", 0));
    return 0;
}

/*
 * Testcase 15
 * Tested function: fnmatch
 * Testcase: test that a * matches across a / if FNM_PATHNAME is not set
 */
int testcase15 () {
    ASSERT(0 == fnmatch("*", "a/c", 0));
    return 0;
}

/*
 * Testcase 16
 * Tested function: fnmatch
 * Testcase: test that a * does not match across a / if FNM_PATHNAME is set
 */
int testcase16 () {
    ASSERT(FNM_NOMATCH == fnmatch("*", "a/c", FNM_PATHNAME));
    return 0;
}

/*
 * Testcase 17
 * Tested function: fnmatch
 * Testcase: match an escaped *
 */
int testcase17 () {
    ASSERT(0 == fnmatch("a\\*c", "a*c", 0));
    return 0;
}

/*
 * Testcase 18
 * Tested function: fnmatch
 * Testcase: test that an escaped * does not act as a wildcard
 */
int testcase18 () {
    ASSERT(FNM_NOMATCH == fnmatch("a\\*c", "abc", 0));
    return 0;
}

/*
 * Testcase 19
 * Tested function: fnmatch
 * Testcase: test when FNM_NOESCAPE is set, a backslash has no special meaning
 */
int testcase19 () {
    ASSERT(0 == fnmatch("a\\c", "a\\c", FNM_NOESCAPE));
    ASSERT(FNM_NOMATCH == fnmatch("a\\c", "abc", FNM_NOESCAPE));
    return 0;
}

/*
 * Testcase 20
 * Tested function: fnmatch
 * Testcase: test escaping of an ordinary character
 */
int testcase20 () {
    ASSERT(0 == fnmatch("a\\c", "ac", 0));
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
    RUN_CASE(17);
    RUN_CASE(18);
    RUN_CASE(19);
    RUN_CASE(20);
    END;
}

