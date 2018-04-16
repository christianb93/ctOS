/*
 * test_env.c
 *
 */

#include "kunit.h"

extern char* getenv(const char*);
extern char** environ;

/*
 * Testcase 1
 * Tested function: getenv
 * Testcase: read an environment value
 */
int testcase1() {
    char** old_env;
    char* env[2];
    char* env_val;
    env[0]="HOME=/";
    env[1]=0;
    old_env = environ;
    environ = env;
    env_val = getenv("HOME");
    ASSERT(env_val);
    ASSERT(0==strcmp(env_val, "/"));
    environ = old_env;
    return 0;
}

/*
 * Testcase 2
 * Tested function: getenv
 * Testcase: read a non-defined environment value
 */
int testcase2() {
    char** old_env;
    char* env[2];
    char* env_val;
    env[0]="HOME=/";
    env[1]=0;
    old_env = environ;
    environ = env;
    env_val = getenv("XX");
    ASSERT(0==env_val);
    environ = old_env;
    return 0;
}

int main() {
    INIT;
    RUN_CASE(1);
    RUN_CASE(2);
    END;
}
