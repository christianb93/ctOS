/*
 * test_params.c
 */

#include "kunit.h"
#include "vga.h"
#include "params.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>



/*
 * Stub for win_putchar
 */
void win_putchar(win_t* win, u8 c) {
    printf("%c", c);
}

/*
 * Testcase 1
 * Test parsing of string value
 */
int testcase1() {
    char cmd_line[255];
    int i;
    for (i=0;i<255;i++)
        cmd_line[i]=0;
    strcpy(cmd_line, "heap_validate=0");
    params_parse(cmd_line);
    ASSERT(strcmp(params_get("heap_validate"), "0")==0);
    return 0;
}

/*
 * Testcase 2
 * Tested function: params_get
 * Testcase: name not existing
 */
int testcase2() {
    ASSERT(params_get("blabla")==0);
    return 0;
}

/*
 * Testcase 3
 * Tested function: params_parse
 * Testcase: test parsing of integer values
 */
int testcase3() {
    char cmd_line[255];
    int i;
    for (i=0;i<255;i++)
        cmd_line[i]=0;
    strcpy(cmd_line, "heap_validate=5");
    params_parse(cmd_line);
    ASSERT(strcmp(params_get("heap_validate"), "5")==0);
    ASSERT(params_get_int("heap_validate")==5);
    return 0;
}


/*
 * Testcase 4
 * Tested function: params_parse
 * Testcase: test parsing of command line with more than one argument
 */
int testcase4() {
    char cmd_line[255];
    int i;
    for (i=0;i<255;i++)
        cmd_line[i]=0;
    strcpy(cmd_line, "heap_validate=5 use_debug_port=0");
    params_parse(cmd_line);
    ASSERT(strcmp(params_get("heap_validate"), "5")==0);
    ASSERT(params_get_int("heap_validate")==5);
    ASSERT(strcmp(params_get("use_debug_port"), "0")==0);
    ASSERT(params_get_int("use_debug_port")==0);
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
