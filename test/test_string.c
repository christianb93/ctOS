#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "kunit.h"



/* Testcase 1
 * Tested function: string/strlen
 */
int testcase1() {
    ASSERT(strlen("xxx")==3);
    ASSERT(strlen("")==0);
    return 0;
}

/* Testcase 2
 * Tested function: string/strncpy
 * Test case: copy string up to n bytes - n exceeds
 * length of src
 */
int testcase2() {
    char* src = "abc";
    char target[11];
    int i;
    for (i = 0; i < 11; i++)
        target[i] = 1;
    strncpy(target, src, 10);
    /* First three bytes of target are taken from src
     */
    for (i = 0; i < 3; i++)
        ASSERT(target[i] == src[i]);
    /* Then target is filled up with zeroes */
    for (i = 3; i < 10; i++)
        ASSERT(target[i]==0);
    return 0;
}

/* Testcase 3
 * Tested function: string/strncpy
 * Test case: copy string up to n bytes - n less than
 * length of src
 */
int testcase3() {
    char* src = "abc";
    char target[10];
    int i;
    for (i = 0; i < 10; i++)
        target[i] = 1;
    strncpy(target, src, 2);
    /* First two bytes of target are taken from src
     */
    for (i = 0; i < 2; i++)
        ASSERT(target[i]==src[i]);
    /* Other parts of target are not touched */
    for (i = 2; i < 10; i++)
        ASSERT(target[i]==1);
    return 0;
}

/* Testcase 4
 * Tested function: string/strncmp
 * Test case: compare two strings which are equal
 * number of bytes less than strlen
 */
int testcase4() {
    char* s1 = "abc";
    char* s2 = "abc";
    ASSERT(strncmp(s1, s2, 2)==0);
    return 0;
}

/* Testcase 5
 * Tested function: string/strncmp
 * Test case: compare two strings which are equal
 * number of bytes is equal to strlen
 */
int testcase5() {
    char* s1 = "abc";
    char* s2 = "abc";
    ASSERT(strncmp(s1, s2, 3)==0);
    return 0;
}

/* Testcase 6
 * Tested function: string/strncmp
 * Test case: compare two strings which are equal
 * number of bytes exceeds strlen
 */
int testcase6() {
    char* s1 = "abc";
    char* s2 = "abc";
    ASSERT(strncmp(s1, s2, 10)==0);
    return 0;
}

/* Testcase 7
 * Tested function: string/strncmp
 * Test case: compare two strings which differ at
 * position 5 - n=4
 */
int testcase7() {
    char* s1 = "abcde";
    char* s2 = "abcdf";
    ASSERT(strncmp(s1, s2, 4)==0);
    return 0;
}

/* Testcase 8
 * Tested function: string/strncmp
 * Test case: compare two strings which differ at
 * position 5 - n=5
 */
int testcase8() {
    char* s1 = "abcde";
    char* s2 = "abcdf";
    ASSERT(strncmp(s1, s2, 5)!=0);
    return 0;
}

/* Testcase 9
 * Tested function: string/strncmp
 * Test case: compare two strings with different length
 */
int testcase9() {
    char* s1 = "abc";
    char* s2 = "abcdf";
    ASSERT(strncmp(s1, s2, 3)==0);
    ASSERT(strncmp(s1, s2, 4)<0);
    return 0;
}

/* Testcase 10
 * Tested function: string/strncmp
 * Test case: compare two strings and evaluate sign
 * of strncmp
 */
int testcase10() {
    char* s1 = "abcde";
    char* s2 = "abcdf";
    ASSERT(strncmp(s1, s2, 5)<0);
    ASSERT(strncmp(s2, s1, 5)>0);
    return 0;
}

/* Testcase 11
 * Tested function: string/strcmp
 * Test case: compare two strings with different length
 */
int testcase11() {
    char* s1 = "abc";
    char* s2 = "abcdf";
    ASSERT(strcmp(s1, s2)<0);
    ASSERT(strcmp(s2, s1)>0);
    return 0;
}

/* Testcase 12
 * Tested function: string/strcmp
 * Test case: compare two strings which are equal
 */
int testcase12() {
    char* s1 = "abc";
    char* s2 = "abc";
    ASSERT(strcmp(s1, s2)==0);
    return 0;
}

/* Testcase 13
 * Tested function: string/memcpy
 * Test case: copy a string
 */
int testcase13() {
    char* src = "abc";
    char target[4];
    int i;
    void* result;
    for (i = 0; i < 4; i++)
        target[i] = 1;
    result = memcpy(target, src, 3);
    ASSERT(result==target);
    for (i = 0; i < 3; i++)
        ASSERT(target[i]==src[i]);
    ASSERT(target[3]==1);
    return 0;
}

/* Testcase 14
 * Tested function: string/strspn
 * Test case: normal processing, accept has more than one character
 */
int testcase14() {
    const char* mystr = "abc1";
    const char* accept = "abc";
    ASSERT(strspn(mystr, accept)==3);
    return 0;
}

/* Testcase 15
 * Tested function: string/strspn
 * Test case: string does not start with a character in accept
 */
int testcase15() {
    const char* mystr = "abc1";
    const char* accept = "5";
    ASSERT(strspn(mystr, accept)==0);
    return 0;
}

/* Testcase 16
 * Tested function: string/strspn
 * Test case: string consists entirely of characters in accept
 */
int testcase16() {
    const char* mystr = "abc";
    const char* accept = "abc";
    ASSERT(strspn(mystr, accept)==3);
    return 0;
}

/* Testcase 17
 * Tested function: string/strcspn
 * Test case: normal processing
 */
int testcase17() {
    const char* mystr = "abc1";
    const char* reject = "1";
    ASSERT(strcspn(mystr, reject)==3);
    return 0;
}

/* Testcase 18
 * Tested function: string/strcspn
 * Test case: string starts with a character in reject
 */
int testcase18() {
    const char* mystr = "abc1";
    const char* reject = "ad";
    ASSERT(strcspn(mystr, reject)==0);
    return 0;
}

/* Testcase 19
 * Tested function: string/strcspn
 * Test case: string consists entirely of characters not in reject
 */
int testcase19() {
    const char* mystr = "abc";
    const char* reject = "123";
    ASSERT(strcspn(mystr, reject)==3);
    return 0;
}

/* Testcase 20
 * Tested function: string/strtok
 * Test case: no token in string
 */
int testcase20() {
    char* mystr = "  // /  ";
    const char* sep = " /";
    ASSERT(0==strtok(mystr, sep));
    return 0;
}

/* Testcase 21
 * Tested function: string/strtok
 * Test case: first call, string contains token
 */
int testcase21() {
    char* mystr = "//abcd/ef x";
    char buffer[256];
    const char* sep = " /";
    char* token;
    strcpy(buffer, mystr);
    token = strtok(buffer, sep);
    ASSERT(0==strcmp(token, "abcd"));
    return 0;
}

/* Testcase 22
 * Tested function: string/strtok
 * Test case: second call, string contains token
 */
int testcase22() {
    char* mystr = "//abcd/ef x";
    char buffer[256];
    const char* sep = " /";
    char* token;
    strcpy(buffer, mystr);
    token = strtok(buffer, sep);
    ASSERT(0==strcmp(token, "abcd"));
    token = strtok(0, sep);
    ASSERT(0==strcmp(token, "ef"));
    token = strtok(0, sep);
    ASSERT(0==strcmp(token, "x"));
    return 0;
}

/* Testcase 23
 * Tested function: string/memset
 * Test case: normal processing
 */
int testcase23() {
    char buffer[256];
    buffer[2] = 'y';
    ASSERT(buffer==memset(buffer, 'x', 2));
    ASSERT('x'==buffer[0]);
    ASSERT('x'==buffer[1]);
    ASSERT('y'==buffer[2]);
    return 0;
}

/* Testcase 24
 * Tested function: string/memset
 * Test case: argument is zero
 */
int testcase24() {
    char buffer[256];
    buffer[0] = 'y';
    ASSERT(buffer==memset(buffer, 'x', 0));
    ASSERT('y'==buffer[0]);
    return 0;
}

/* Testcase 25
 * Tested function: string/strchr
 * Test case: normal processing
 */
int testcase25() {
    char *mystr = "abc";
    ASSERT(mystr+1==strchr(mystr,'b'));
    return 0;
}

/* Testcase 26
 * Tested function: string/strchr
 * Test case: character not found
 */
int testcase26() {
    char *mystr = "abc";
    ASSERT(0==strchr(mystr,'d'));
    return 0;
}

/* Testcase 27
 * Tested function: string/strchr
 * Test case: matching character is terminating zero
 */
int testcase27() {
    char *mystr = "abc";
    ASSERT(mystr+3==strchr(mystr,0));
    return 0;
}

/* Testcase 28
 * Tested function: strcpy
 * Test case: copy a string
 */
int testcase28() {
    char *mystr = "abc";
    char buffer[16];
    memset((void*) buffer, 1, 16);
    ASSERT(buffer==strcpy(buffer,mystr));
    ASSERT(0==buffer[3]);
    ASSERT(0==strcmp("abc", buffer));
    return 0;
}

/* Testcase 29
 * Tested function: strcat
 * Test case: append a string to an existing string
 */
int testcase29() {
    char *mystr = "abc";
    char buffer[16];
    /*
     * First copy mystr into buffer
     */
    memset((void*) buffer, 1, 16);
    ASSERT(buffer==strcpy(buffer,mystr));
    ASSERT(0==buffer[3]);
    ASSERT(0==strcmp("abc", buffer));
    /*
     * Now append "def"
     */
    ASSERT(buffer==strcat(buffer, "def"));
    /*
     * Verify that trailing zero has been overwritten
     */
    ASSERT('d'==buffer[3]);
    /*
     * and that a new trailing zero has been added
     */
    ASSERT(0==buffer[6]);
    ASSERT(0==strcmp(buffer, "abcdef"));
    return 0;
}

/* Testcase 30
 * Tested function: strcat
 * Test case: append a string to an empty string
 */
int testcase30() {
    char buffer[16];
    /*
     * First copy mystr into buffer
     */
    memset((void*) buffer, 1, 16);
    buffer[0]=0;
    /*
     * Now append "def"
     */
    ASSERT(buffer==strcat(buffer, "def"));
    /*
     * Verify that trailing zero has been placed
     */
    ASSERT(0==buffer[3]);
    ASSERT(0==strcmp(buffer, "def"));
    return 0;
}

/*
 * Tested function: strerror
 */
int testcase31() {
    ASSERT(strerror(-1));
    return 0;
}

/*
 * Testcase 32
 * Tested function: memcmp
 * Testcase: call memcmp with n equal to zero
 */
int testcase32() {
    char array1[16];
    char array2[16];
    ASSERT(0==memcmp(array1, array2, 0));
    return 0;
}

/*
 * Testcase 33
 * Tested function: memcmp
 * Testcase: call memcmp with both arguments being equal
 */
int testcase33() {
    char array1[16];
    char array2[16];
    int i;
    for (i=0;i<16;i++) {
        array1[i]=i;
        array2[i]=i;
    }
    ASSERT(0==memcmp(array1, array2, 16));
    return 0;
}

/*
 * Testcase 34
 * Tested function: memcmp
 * Testcase: first argument is greater than second, difference is in first position
 */
int testcase34() {
    char array1[16];
    char array2[16];
    int i;
    for (i=0;i<16;i++) {
        array1[i]=i;
        array2[i]=i;
    }
    array1[0]=55;
    ASSERT(memcmp(array1, array2, 16)>0);
    return 0;
}

/*
 * Testcase 35
 * Tested function: memcmp
 * Testcase: first argument is greater than second, difference is in last
 */
int testcase35() {
    char array1[16];
    char array2[16];
    int i;
    for (i=0;i<16;i++) {
        array1[i]=i;
        array2[i]=i;
    }
    array1[15]=55;
    ASSERT(memcmp(array1, array2, 16)>0);
    return 0;
}

/*
 * Testcase 36
 * Tested function: memcmp
 * Testcase: first argument is smaller than second, difference is in first position
 */
int testcase36() {
    char array1[16];
    char array2[16];
    int i;
    for (i=0;i<16;i++) {
        array1[i]=i;
        array2[i]=i;
    }
    array2[0]=55;
    ASSERT(memcmp(array1, array2, 16)<0);
    return 0;
}

/*
 * Testcase 37
 * Tested function: memcmp
 * Testcase: first argument is smaller than second, difference is in last
 */
int testcase37() {
    char array1[16];
    char array2[16];
    int i;
    for (i=0;i<16;i++) {
        array1[i]=i;
        array2[i]=i;
    }
    array2[15]=55;
    ASSERT(memcmp(array1, array2, 16)<0);
    return 0;
}

/* Testcase 38
 * Tested function: memmove
 * Test case: no overlap
 */
int testcase38() {
    unsigned char mymem[16];
    unsigned char* src = mymem + 8;
    unsigned char* dest = mymem;
    src[0]=1;
    src[1]=2;
    dest[0]=0;
    dest[1]=0;
    ASSERT(dest==memmove(dest, src, 2));
    ASSERT(dest[0]==1);
    ASSERT(dest[1]==2);
    return 0;
}

/* Testcase 39
 * Tested function: memmove
 * Test case: overlap, but dest > src
 */
int testcase39() {
    unsigned char mymem[16];
    unsigned char* src = mymem + 1;
    unsigned char* dest = mymem;
    dest[0]=0;
    dest [1]=0;
    src[0]=1;
    src[1]=2;
    ASSERT(dest==memmove(dest, src, 2));
    ASSERT(dest[0]==1);
    ASSERT(dest[1]==2);
    return 0;
}

/* Testcase 40
 * Tested function: memmove
 * Test case: overlap, dest < src
 */
int testcase40() {
    unsigned char mymem[16];
    unsigned char* src = mymem;
    unsigned char* dest = mymem+1;
    dest[0]=0;
    dest [1]=0;
    src[0]=1;
    src[1]=2;
    ASSERT(dest==memmove(dest, src, 2));
    ASSERT(dest[0]==1);
    ASSERT(dest[1]==2);
    return 0;
}

/*
 * Testcase 41:
 * Tested function: strstr
 * Testcase: locate a substring at position 0
 */
int testcase41() {
    char* s1 = "abxy";
    char* s2 = "ab";
    ASSERT(s1==strstr(s1, s2));
    return 0;
}

/*
 * Testcase 42:
 * Tested function: strstr
 * Testcase: locate a substring at position 1
 */
int testcase42() {
    char* s1 = "abxy";
    char* s2 = "bx";
    ASSERT(s1+1==strstr(s1, s2));
    return 0;
}

/*
 * Testcase 43:
 * Tested function: strstr
 * Testcase: locate a substring at the end of the string
 */
int testcase43() {
    char* s1 = "abxy";
    char* s2 = "bxy";
    ASSERT(s1+1==strstr(s1, s2));
    return 0;
}

/*
 * Testcase 44:
 * Tested function: strstr
 * Testcase: both strings are equal
 */
int testcase44() {
    char* s1 = "abxy";
    char* s2 = "abxy";
    ASSERT(s1==strstr(s1, s2));
    return 0;
}

/*
 * Testcase 45:
 * Tested function: strstr
 * Testcase: the first string is a substring of the second string
 */
int testcase45() {
    char* s1 = "abxy";
    char* s2 = "abxyz";
    ASSERT(0==strstr(s1, s2));
    return 0;
}

/*
 * Testcase 46:
 * Tested function: strstr
 * Testcase: the first string is disjoint from the second string but shorter
 */
int testcase46() {
    char* s1 = "abxy";
    char* s2 = "zzzzz";
    ASSERT(0==strstr(s1, s2));
    return 0;
}

/*
 * Testcase 47:
 * Tested function: strstr
 * Testcase: no match
 */
int testcase47() {
    char* s1 = "abxy";
    char* s2 = "xx";
    ASSERT(0==strstr(s1, s2));
    return 0;
}


/*
 * Testcase 48:
 * Tested function: strstr
 * Testcase: second string is empty
 */
int testcase48() {
    char* s1 = "abxy";
    char* s2 = "";
    ASSERT(s1==strstr(s1, s2));
    return 0;
}

/*
 * Testcase 49
 * Tested function: strpbrk
 * Testcase: first match at start of string, second character in s2 matches
 */
int testcase49() {
    char* sep =".;";
    char* s1 = ";ab";
    ASSERT(s1==strpbrk(s1, sep));
    return 0;
}

/*
 * Testcase 50
 * Tested function: strpbrk
 * Testcase: first match at start of string, first character in s2 matches
 */
int testcase50() {
    char* sep =".;";
    char* s1 = ".ab";
    ASSERT(s1==strpbrk(s1, sep));
    return 0;
}

/*
 * Testcase 51
 * Tested function: strpbrk
 * Testcase: first match at end of string, first character in s2 matches
 */
int testcase51() {
    char* sep =".;";
    char* s1 = "ab.";
    ASSERT(s1+2==strpbrk(s1, sep));
    return 0;
}

/*
 * Testcase 52
 * Tested function: strpbrk
 * Testcase: no match
 */
int testcase52() {
    char* sep =".;";
    char* s1 = "ab";
    ASSERT(0==strpbrk(s1, sep));
    return 0;
}

/*
 * Testcase 53
 * Tested function: strpbrk
 * Testcase: s2 empty
 */
int testcase53() {
    char* sep ="";
    char* s1 = "ab";
    ASSERT(0==strpbrk(s1, sep));
    return 0;
}

/*
 * Testcase 54
 * Tested function: strpbrk
 * Testcase: s1 empty
 */
int testcase54() {
    char* sep =".;";
    char* s1 = "";
    ASSERT(0==strpbrk(s1, sep));
    return 0;
}

/*
 * Testcase 55
 * Tested function: strdup
 * Testcase: duplicate a string
 */
int testcase55() {
    char* src = "abc";
    char* copy = strdup(src);
    ASSERT(copy);
    ASSERT(copy != src);
    ASSERT(0==strcmp(copy, src));
    free((void*) copy);
    return 0;
}

/* Testcase 56
 * Tested function: strcasecmp
 * Test case: compare two strings with different length
 */
int testcase56() {
    char* s1 = "abc";
    char* s2 = "abcdf";
    ASSERT(strcasecmp(s1, s2)<0);
    ASSERT(strcasecmp(s2, s1)>0);
    return 0;
}

/* Testcase 57
 * Tested function: strcasecmp
 * Test case: compare two strings which are equal up to capitalization
 */
int testcase57() {
    char* s1 = "abc";
    char* s2 = "ABC";
    ASSERT(strcasecmp(s1, s2)==0);
    return 0;
}

/*
 * Testcase 58
 * Tested function: strrchr
 * Test case: call strrchr to locate the last character of a string
 */
int testcase58() {
    char* mystring = "abc";
    ASSERT(mystring+2==strrchr(mystring, 'c'));
    return 0;
}

/*
 * Testcase 59
 * Tested function: strrchr
 * Test case: call strrchr to locate the first character of a string
 */
int testcase59() {
    char* mystring = "abc";
    ASSERT(mystring==strrchr(mystring, 'a'));
    return 0;
}

/*
 * Testcase 60
 * Tested function: strrchr
 * Test case: call strrchr to locate a character within a string which occurs twice
 */
int testcase60() {
    char* mystring = "aac";
    ASSERT(mystring+1==strrchr(mystring, 'a'));
    return 0;
}

/*
 * Testcase 61
 * Tested function: strrchr
 * Test case: call strrchr to locate the trailing zero in a string
 */
int testcase61() {
    char* mystring = "aac";
    ASSERT(mystring+3==strrchr(mystring, 0));
    return 0;
}

/*
 * Testcase 62
 * Tested function: strrchr
 * Test case: call strrchr to locate a character which is not contained in the string
 */
int testcase62() {
    char* mystring = "aac";
    ASSERT(0==strrchr(mystring, 'x'));
    return 0;
}

/*
 * Testcase 63
 * Tested function: strncat
 * Testcase: append a string where n is less than the size of the string
 */
int testcase63() {
    char buffer[512];
    int i;
    for (i=0;i<512;i++)
        buffer[i]=1;
    strcpy(buffer, "abc");
    buffer[3]=0;
    ASSERT(0==strcmp(buffer, "abc"));
    strncat(buffer, "xy", 1);
    ASSERT(0==strcmp(buffer,"abcx"));
    ASSERT(0==buffer[4]);
    return 0;
}

/*
 * Testcase 64
 * Tested function: strncat
 * Testcase: append a string where n is equal to the size of the string
 */
int testcase64() {
    char buffer[512];
    int i;
    for (i=0;i<512;i++)
        buffer[i]=1;
    strcpy(buffer, "abc");
    buffer[3]=0;
    ASSERT(0==strcmp(buffer, "abc"));
    strncat(buffer, "xy", 2);
    ASSERT(0==strcmp(buffer,"abcxy"));
    ASSERT(0==buffer[5]);
    return 0;
}


/*
 * Testcase 65
 * Tested function: strncat
 * Testcase: append a string where n greater than the size of the string
 */
int testcase65() {
    char buffer[512];
    int i;
    for (i=0;i<512;i++)
        buffer[i]=1;
    strcpy(buffer, "abc");
    buffer[3]=0;
    ASSERT(0==strcmp(buffer, "abc"));
    strncat(buffer, "xy", 3);
    ASSERT(0==strcmp(buffer,"abcxy"));
    ASSERT(0==buffer[5]);
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
    RUN_CASE(41);
    RUN_CASE(42);
    RUN_CASE(43);
    RUN_CASE(44);
    RUN_CASE(45);
    RUN_CASE(46);
    RUN_CASE(47);
    RUN_CASE(48);
    RUN_CASE(49);
    RUN_CASE(50);
    RUN_CASE(51);
    RUN_CASE(52);
    RUN_CASE(53);
    RUN_CASE(54);
    RUN_CASE(55);
    RUN_CASE(56);
    RUN_CASE(57);
    RUN_CASE(58);
    RUN_CASE(59);
    RUN_CASE(60);
    RUN_CASE(61);
    RUN_CASE(62);
    RUN_CASE(63);
    RUN_CASE(64);
    RUN_CASE(65);
    END;
}
