/*
 * test_env.c
 *
 */

#include "kunit.h"

extern char* getenv(const char*);
extern char** environ;

extern char** __ctOS_clone_environ(char**);
extern char* __ctOS_getenv(const char* key);
extern char** __ctOS_putenv(char* string);

/*
 * Testcase 1
 * Tested function: getenv
 * Testcase: read an environment value
 */
int testcase1() {
    char* env[2];
    char* env_val;
    env[0]="HOME=/";
    env[1]=0;
    environ = __ctOS_clone_environ(env);
    env_val = getenv("HOME");
    ASSERT(env_val);
    ASSERT(0==strcmp(env_val, "/"));
    return 0;
}

/*
 * Testcase 2
 * Tested function: getenv
 * Testcase: read a non-defined environment value
 */
int testcase2() {
    char* env[2];
    char* env_val;
    env[0]="HOME=/";
    env[1]=0;
    __ctOS_clone_environ(env);
    env_val = getenv("XX");
    ASSERT(0==env_val);
    return 0;
}

/*
 * Testcase 3
 * Clone an environment
 */
int testcase3() {
    char* env[2];
    env[0]="HOME=/";
    env[1]=0;
    char** new_env = __ctOS_clone_environ(env);
    ASSERT(new_env);
    ASSERT(0 == new_env[1]);
    ASSERT(0 == strcmp(new_env[0], "HOME=/"));
    /*
     * This should not have reallocated 
     * any of the strings
     */
    ASSERT(new_env[0] == env[0]);
    /*
     * Do this once more
     */
    new_env = __ctOS_clone_environ(env);
    ASSERT(new_env);
    ASSERT(0 == new_env[1]);
    ASSERT(0 == strcmp(new_env[0], "HOME=/"));
    return 0;
}

/*
 * Testcase 4
 * getenv
 */
int testcase4() {
    char* env[4];
    env[0]="x=a";
    env[1]="yy=bb";
    env[2]="=bb";
    env[3]=0;
    /*
     * Init module by cloning this environment
     */
    ASSERT(__ctOS_clone_environ(env));
    /*
     * Get a few keys
     */
    ASSERT(__ctOS_getenv("x"));
    ASSERT(0 == strcmp("a", __ctOS_getenv("x")));
    ASSERT(__ctOS_getenv("yy"));
    ASSERT(0 == strcmp("bb", __ctOS_getenv("yy")));
    ASSERT(0 == __ctOS_getenv("notthere"));
    return 0;
}

/*
 * Testcase 5
 * putenv - existing key
 */
int testcase5() {
    char* env[4];
    env[0]="x=a";
    env[1]="yy=bb";
    env[2]="=bb";
    env[3]=0;
    char* env_string = "x=b";
    char** ctOS_env = 0;
    /*
     * Init module by cloning this environment
     */
    ASSERT(ctOS_env = __ctOS_clone_environ(env));
    /*
     * Get key x
     */
    ASSERT(__ctOS_getenv("x"));
    ASSERT(0 == strcmp("a", __ctOS_getenv("x")));
    /*
     * Now change x to b
     */
    ASSERT(ctOS_env == __ctOS_putenv(env_string));
    /*
     * and check
     */
    ASSERT(__ctOS_getenv("x"));
    ASSERT(0 == strcmp("b", __ctOS_getenv("x")));
    /*
     * We expect that the environment entry now points to our
     * string
     */
    ASSERT(env_string == ctOS_env[0]);
    /*
     * The other entries should be unchanged
     */
    ASSERT(__ctOS_getenv("yy"));
    ASSERT(0 == strcmp("bb", __ctOS_getenv("yy"))); 
    return 0;
}

/*
 * Testcase 6
 * putenv - new key
 */
int testcase6() {
    char* env[3];
    env[0]="x=a";
    env[1]="yy=bb";
    env[2]=0;
    char* env_string = "z=1";
    char** ctOS_env = 0;
    char** new_ctOS_env = 0;
    /*
     * Init module by cloning this environment
     */
    ASSERT(ctOS_env = __ctOS_clone_environ(env));
    /*
     * Get key x
     */
    ASSERT(__ctOS_getenv("x"));
    ASSERT(0 == strcmp("a", __ctOS_getenv("x")));
    /*
     * Now add new key
     */
   ASSERT(new_ctOS_env = __ctOS_putenv(env_string));
    /*
     * and check
     */
    ASSERT(__ctOS_getenv("z"));
    ASSERT(0 == strcmp("1", __ctOS_getenv("z")));
    /* 
     * We also expect that the new string is contained at position 2
     */
    ASSERT(new_ctOS_env[2]);
    ASSERT(new_ctOS_env[2] == env_string);
    /*
     * and that the new environment is again null terminated
     */
    ASSERT(new_ctOS_env[3] == 0); 
    /*
     * The previous entries should have been copied, not cloned
     */ 
    ASSERT(new_ctOS_env[0] == env[0]);
    ASSERT(new_ctOS_env[1] == env[1]);
    return 0;
}

/*
 * Testcase 7
 * Simulate a possible scenario where ownership goes forth and back
 */
int testcase7() {
    char* env[3];
    env[0]="x=a";
    env[1]="y=b";
    env[2]=0;
    char* result;
    char** lastenv;
    /*
     * Clone the environment initially and set environ accordingly
     * as our C library startup code does it
     */
    environ = __ctOS_clone_environ(env);
    /*
     * Now simulate an application that has its own implementation 
     * of putenv and will clone environ
     */
    char** new_env = (char**) malloc(sizeof(char*)*4);
    for (int i = 0; i < 2; i++) {
        new_env[i] = environ[i];
    }
    new_env[2] = "z=c";
    new_env[3] = 0;
    environ = new_env;
    lastenv = new_env;
    /*
     * and now simulate a call to getenv
     */
    ASSERT(result = getenv("z"));
    ASSERT(0 == strcmp("c", result));
    /*
     * old values should still be there
     */
    ASSERT(result = getenv("x"));
    ASSERT(0 == strcmp("a", result));
    ASSERT(result = getenv("y"));
    ASSERT(0 == strcmp("b", result));
    /*
     * But getenv should have adjusted environ
     */
    ASSERT(environ != lastenv);
    /*
     * We should also be able to free lastenv
     */
    free(lastenv);
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
