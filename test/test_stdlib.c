#include "lib/stdlib.h"
#include "kunit.h"
#include "stdio.h"
#include "limits.h"
#include "errno.h"

/*
 * Comparison function for byte arrays
 */
int compar1(const void* x1, const void* x2) {
    unsigned char c1 = *((unsigned char*) x1);
    unsigned char c2 = *((unsigned char*) x2);
    if (c1==c2)
        return 0;
    if (c1 > c2)
        return 1;
    return -1;
}

/*
 * Comparison function for u32 arrays
 */
int compar2(const void* x1, const void* x2) {
    unsigned int c1 = *((unsigned int*) x1);
    unsigned int c2 = *((unsigned int*) x2);
    if (c1==c2)
        return 0;
    if (c1 > c2)
        return 1;
    return -1;
}

/* Testcase 1
 * Tested function: stdlib/strtol
 * Test case: test conversion with base 10
 * stop conversion at first non-parsable char
 */
int testcase1() {
    long x;
    char* ptr;
    x = strtol("20a", &ptr, 10);
    ASSERT(20==x);
    ASSERT(*ptr=='a');
    return 0;
}

/* Testcase 2
 * Tested function: stdlib/strtol
 * Test case: test conversion with base 16
 */
int testcase2() {
    long x;
    char* ptr;
    x = strtol("a", &ptr, 16);
    ASSERT(10==x);
    ASSERT(*ptr==0);
    return 0;
}

/* Testcase 3
 * Tested function: stdlib/strtol
 * Test case: test conversion with base 10
 */
int testcase3() {
    long x;
    char* ptr;
    x = strtol("5", &ptr, 10);
    ASSERT(5==x);
    ASSERT(*ptr==0);
    return 0;
}

/* Testcase 4
 * Tested function: stdlib/strtol
 * Test case: test conversion with base 16 and prefix 0x
 */
int testcase4() {
    long x;
    char* ptr;
    x = strtol("0xa", &ptr, 16);
    ASSERT(10==x);
    ASSERT(*ptr==0);
    return 0;
}

/* Testcase 5
 * Tested function: stdlib/strtol
 * Test case: test conversion with base 16 and uppercase letters
 */
int testcase5() {
    long x;
    char* ptr;
    x = strtol("A", &ptr, 16);
    ASSERT(10==x);
    ASSERT(*ptr==0);
    return 0;
}

/*
 * Testcase 6:
 * Tested function: qsort
 * Testcase: sort an empty array
 */
int testcase6() {
    unsigned char array[1];
    qsort(array, 0, 1, compar1);
    return 0;
}

/*
 * Testcase 7:
 * Tested function: qsort
 * Testcase: sort an array with one element
 */
int testcase7() {
    unsigned char array[1];
    array[0]=1;
    qsort(array, 1, 1, compar1);
    ASSERT(1==array[0]);
    return 0;
}

/*
 * Testcase 8:
 * Tested function: qsort
 * Testcase: sort an array with two elements
 */
int testcase8() {
    unsigned char array[2];
    array[0]=5;
    array[1]=1;
    qsort(array, 2, 1, compar1);
    ASSERT(1==array[0]);
    ASSERT(5==array[1]);
    return 0;
}

/*
 * Testcase 9:
 * Tested function: qsort
 * Testcase: sort an array with three elements
 */
int testcase9() {
    unsigned char array[3];
    array[0]=5;
    array[1]=3;
    array[2]=1;
    qsort(array, 3, 1, compar1);
    ASSERT(1==array[0]);
    ASSERT(3==array[1]);
    ASSERT(5==array[2]);
    return 0;
}

/*
 * Testcase 10:
 * Tested function: qsort
 * Testcase: sort an array with four elements
 */
int testcase10() {
    unsigned char array[4];
    array[0]=5;
    array[1]=4;
    array[2]=3;
    array[3]=2;
    qsort(array, 4, 1, compar1);
    ASSERT(2==array[0]);
    ASSERT(3==array[1]);
    ASSERT(4==array[2]);
    ASSERT(5==array[3]);
    return 0;
}

/*
 * Testcase 11:
 * Tested function: qsort
 * Testcase: sort an integer array with four elements
 */
int testcase11() {
    unsigned int array[4];
    array[0]=5;
    array[1]=4;
    array[2]=3;
    array[3]=2;
    qsort(array, 4, 4, compar2);
    ASSERT(2==array[0]);
    ASSERT(3==array[1]);
    ASSERT(4==array[2]);
    ASSERT(5==array[3]);
    return 0;
}

/*
 * Testcase 12:
 * Tested function: qsort
 * Testcase: sort a character array ("quicksort")
 */
int testcase12() {
    char array[10];
    strcpy(array, "quicksort");
    qsort(array, 9, 1, compar1);
    ASSERT(0==strcmp("cikoqrstu", array));
    return 0;
}

/*
 * Testcase 13
 * Tested function: strtoull
 * Testcase: Convert an emtpy string
 */
int testcase13() {
    char* end_ptr = 0;
    char* my_string = "";
    unsigned long long result = strtoull(my_string, &end_ptr, 10);
    ASSERT(0==result);
    ASSERT(end_ptr==my_string);
    return 0;
}

/*
 * Testcase 14
 * Tested function: strtoull
 * Testcase: Convert a string which consists of whitespace characters only
 */
int testcase14() {
    char* end_ptr = 0;
    char* my_string = "  \n\t";
    unsigned long long result = strtoull(my_string, &end_ptr, 10);
    ASSERT(0==result);
    ASSERT(end_ptr==my_string);
    return 0;
}

/*
 * Testcase 15
 * Tested function: strtoull
 * Testcase: Convert a decimal integer without leading white space
 */
int testcase15() {
    char* my_string = "1";
    char* end_ptr;
    unsigned long long result = strtoull("1", &end_ptr, 10);
    ASSERT(1==result);
    ASSERT(end_ptr==my_string+1);
    return 0;
}

/*
 * Testcase 16
 * Tested function: strtoull
 * Testcase: Convert a decimal integer with leading white space
 */
int testcase16() {
    char* my_string = "  110";
    char* end_ptr;
    unsigned long long result = strtoull(my_string, &end_ptr, 10);
    ASSERT(110==result);
    ASSERT(end_ptr==my_string+5);
    return 0;
}

/*
 * Testcase 17
 * Tested function: strtoull
 * Testcase: Convert a decimal integer with leading sign
 */
int testcase17() {
    char* end_ptr;
    char* my_string = "+1";
    unsigned long long result = strtoull(my_string, &end_ptr, 10);
    ASSERT(1==result);
    ASSERT(end_ptr==my_string+2);
    return 0;
}

/*
 * Testcase 18
 * Tested function: strtoull
 * Testcase: Convert an octal number with a specified base and a leading 0
 */
int testcase18() {
    char* end_ptr;
    char* my_string = "010";
    unsigned long long result = strtoull(my_string, &end_ptr, 8);
    ASSERT(8==result);
    ASSERT(end_ptr==my_string+strlen(my_string));
    /*
     * Now check case that we have the string '0'
     */
    my_string = "0";
    result = strtoull(my_string, &end_ptr, 8);
    ASSERT(0 == result);
    ASSERT(end_ptr == my_string + strlen(my_string));
    return 0;
}

/*
 * Testcase 19
 * Tested function: strtoull
 * Testcase: Convert an octal number with a specified base and without a leading 0
 */
int testcase19() {
    char* end_ptr;
    char* my_string = "10";
    unsigned long long result = strtoull(my_string, &end_ptr, 8);
    ASSERT(8==result);
    ASSERT(end_ptr==my_string+strlen(my_string));
    return 0;
}

/*
 * Testcase 20
 * Tested function: strtoull
 * Testcase: Convert an octal number without a specified base
 */
int testcase20() {
    char* end_ptr;
    char* my_string = "011";
    unsigned long long result = strtoull(my_string, &end_ptr, 0);
    ASSERT(9==result);
    ASSERT(end_ptr==my_string+strlen(my_string));
    /*
     * Now test case of string "0"
     */
    my_string = "0";
    result = strtoull(my_string, &end_ptr, 0);
    ASSERT(0 == result);
    ASSERT(end_ptr == my_string+strlen(my_string));
    /*
     * and finally try "10"
     */
    my_string = "10";
    result = strtoull(my_string, &end_ptr, 0);
    ASSERT(10 == result);
    ASSERT(end_ptr == my_string+strlen(my_string));
    return 0;
}

/*
 * Testcase 21
 * Tested function: strtoull
 * Testcase: Convert a hexadeciaml number with a specified base and a leading 0x
 */
int testcase21() {
    char* end_ptr;
    char* my_string = "0x10";
    unsigned long long result = strtoull(my_string, &end_ptr, 16);
    ASSERT(16==result);
    ASSERT(end_ptr==my_string+strlen(my_string));
    return 0;
}

/*
 * Testcase 22
 * Tested function: strtoull
 * Testcase: Convert a hexadecimal number with a specified base and without a leading 0x
 */
int testcase22() {
    char* end_ptr;
    char* my_string = "ff";
    unsigned long long result = strtoull(my_string, &end_ptr, 16);
    ASSERT(255==result);
    ASSERT(end_ptr==my_string+strlen(my_string));
    return 0;
}

/*
 * Testcase 23
 * Tested function: strtoull
 * Testcase: Convert a hexadecimal number without a specified base
 */
int testcase23() {
    char* end_ptr;
    char* my_string = "0x11";
    unsigned long long result = strtoull(my_string, &end_ptr, 0);
    ASSERT(17==result);
    ASSERT(end_ptr==my_string+strlen(my_string));
    return 0;
}

/*
 * Testcase 24
 * Tested function: strtoull
 * Testcase: Convert a hexadecimal number but specify base 8
 */
int testcase24() {
    char* end_ptr;
    char* my_string = "0x11";
    unsigned long long result = strtoull(my_string, &end_ptr, 8);
    ASSERT(0==result);
    ASSERT(end_ptr==my_string+1);
    return 0;
}

/*
 * Testcase 25
 * Tested function: strtoull
 * Testcase: Test "final string"
 */
int testcase25() {
    char* end_ptr;
    char* my_string = "0x11zzz";
    unsigned long long result = strtoull(my_string, &end_ptr, 0);
    ASSERT(17==result);
    ASSERT(end_ptr==my_string+4);
    return 0;
}

/*
 * Testcase 26
 * Tested function: strtol
 * Testcase: Test maximum positive value
 */
int testcase26() {
    char* end_ptr;
    char* my_string = "0x7fffffff";
    errno = 0;
    long result = strtol(my_string, &end_ptr, 0);
    ASSERT(LONG_MAX==result);
    ASSERT(0==errno);
    return 0;
}

/*
 * Testcase 27
 * Tested function: strtol
 * Testcase: Test maximum positive value plus 1
 */
int testcase27() {
    char* end_ptr;
    char* my_string = "0x80000000";
    errno = 0;
    long result = strtol(my_string, &end_ptr, 0);
    ASSERT(LONG_MAX==result);
    ASSERT(errno);
    return 0;
}

/*
 * Testcase 28
 * Tested function: strtol
 * Testcase: Test maximum positive value minus 1
 */
int testcase28() {
    char* end_ptr;
    char* my_string = "0x7ffffffe";
    errno = 0;
    long result = strtol(my_string, &end_ptr, 0);
    ASSERT(LONG_MAX==result+1);
    ASSERT(0==errno);
    return 0;
}

/*
 * Testcase 29
 * Tested function: strtol
 * Testcase: Test minimum negative value
 */
int testcase29() {
    char* end_ptr;
    char* my_string = "-0x80000000";
    errno = 0;
    long result = strtol(my_string, &end_ptr, 0);
    ASSERT(LONG_MIN==result);
    ASSERT(0==errno);
    return 0;
}

/*
 * Testcase 30
 * Tested function: strtol
 * Testcase: Test minimum negative value - 1
 */
int testcase30() {
    char* end_ptr;
    char* my_string = "-0x80000001";
    errno = 0;
    long result = strtol(my_string, &end_ptr, 0);
    ASSERT(LONG_MIN==result);
    ASSERT(errno);
    return 0;
}

/*
 * Testcase 31
 * Tested function: strtol
 * Testcase: Test minimum negative value plus 1
 */
int testcase31() {
    char* end_ptr;
    char* my_string = "-0x7fffffff";
    errno = 0;
    long result = strtol(my_string, &end_ptr, 0);
    ASSERT(LONG_MIN+1==result);
    ASSERT(0==errno);
    return 0;
}

/*
 * Testcase 32
 * Tested function: strtoll
 * Testcase: Test maximum positive value
 */
int testcase32() {
    char* end_ptr;
    char* my_string = "0x7fffffffffffffff";
    errno = 0;
    long long result = strtoll(my_string, &end_ptr, 0);
    ASSERT(LLONG_MAX==result);
    ASSERT(0==errno);
    return 0;
}

/*
 * Testcase 33
 * Tested function: strtoll
 * Testcase: Test maximum positive value plus 1
 */
int testcase33() {
    char* end_ptr;
    char* my_string = "0x8000000000000000";
    errno = 0;
    long long result = strtoll(my_string, &end_ptr, 0);
    ASSERT(LLONG_MAX==result);
    ASSERT(errno);
    return 0;
}

/*
 * Testcase 34
 * Tested function: strtoll
 * Testcase: Test maximum positive value minus 1
 */
int testcase34() {
    char* end_ptr;
    char* my_string = "0x7ffffffffffffffe";
    errno = 0;
    long long result = strtoll(my_string, &end_ptr, 0);
    ASSERT(LLONG_MAX==result+1);
    ASSERT(0==errno);
    return 0;
}

/*
 * Testcase 35
 * Tested function: strtoll
 * Testcase: Test minimum negative value
 */
int testcase35() {
    char* end_ptr;
    char* my_string = "-0x8000000000000000";
    errno = 0;
    long long result = strtoll(my_string, &end_ptr, 0);
    ASSERT(LLONG_MIN==result);
    ASSERT(0==errno);
    return 0;
}

/*
 * Testcase 36
 * Tested function: strtoll
 * Testcase: Test minimum negative value - 1
 */
int testcase36() {
    char* end_ptr;
    char* my_string = "-0x8000000000000001";
    errno = 0;
    long long result = strtoll(my_string, &end_ptr, 0);
    ASSERT(LLONG_MIN==result);
    ASSERT(errno);
    return 0;
}

/*
 * Testcase 37
 * Tested function: strtoll
 * Testcase: Test minimum negative value plus 1
 */
int testcase37() {
    char* end_ptr;
    char* my_string = "-0x7fffffffffffffff";
    errno = 0;
    long long result = strtoll(my_string, &end_ptr, 0);
    ASSERT(LLONG_MIN+1==result);
    ASSERT(0==errno);
    return 0;
}

/*
 * Testcase 38
 * Tested function: strtoull
 * Testcase: Test maximum value
 */
int testcase38() {
    char* end_ptr;
    char* my_string = "0xffffffffffffffff";
    errno = 0;
    unsigned long long result = strtoull(my_string, &end_ptr, 0);
    ASSERT(ULLONG_MAX==result);
    ASSERT(0==errno);
    return 0;
}

/*
 * Testcase 39
 * Tested function: strtoull
 * Testcase: Test maximum positive value plus 1
 */
int testcase39() {
    char* end_ptr;
    char* my_string = "0x10000000000000000";
    errno = 0;
    unsigned long long result = strtoull(my_string, &end_ptr, 0);
    ASSERT(ULLONG_MAX==result);
    ASSERT(errno);
    return 0;
}

/*
 * Testcase 40
 * Tested function: strtoll
 * Testcase: Test maximum positive value minus 1
 */
int testcase40() {
    char* end_ptr;
    char* my_string = "0xfffffffffffffffe";
    errno = 0;
    unsigned long long result = strtoull(my_string, &end_ptr, 0);
    ASSERT(ULLONG_MAX==result+1);
    ASSERT(0==errno);
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
    RUN_CASE(21);
    RUN_CASE(22);
    RUN_CASE(23);
    RUN_CASE(24);
    RUN_CASE(25);
    RUN_CASE(26);
    RUN_CASE(27);
    RUN_CASE(28);
    RUN_CASE(29);
    RUN_CASE(30);
    RUN_CASE(31);
    RUN_CASE(32);
    RUN_CASE(33);
    RUN_CASE(34);
    RUN_CASE(35);
    RUN_CASE(36);
    RUN_CASE(37);
    RUN_CASE(38);
    RUN_CASE(39);
    RUN_CASE(40);
    END;
}
