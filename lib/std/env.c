/*
 * env.c
 *
 * Functions to get and set environment variables
 */

#include "lib/errno.h"
#include "lib/string.h"
#include "lib/stdlib.h"

/*
 * This is a pointer to the last return value of getenv
 */
static char* getenv_last = 0;

extern char** environ;

/*
 * The getenv() function will search the environment of the calling process for the environment variable name if it exists
 * and return a pointer to the value of the environment variable.
 *
 * If the specified environment variable cannot be found, a null pointer will be returned.
 *
 * The application shall ensure that it does not modify the string pointed to by the getenv() function.
 *
 * The string pointed to may be overwritten by a subsequent call to getenv(), setenv(), or unsetenv()
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * none
 *
 */
char* getenv(const char* name) {
    int i;
    int j;
    int match = -1;
    int val_length;
    char* env_string;
    /*
     * Scan environ array
     */
    if (0==environ)
        return 0;
    i=0;
    while (environ[i]) {
        env_string = environ[i];
        /*
         * Search for '='
         */
        j=0;
        match = -1;
        while (j<strlen(env_string)) {
            if ('='==env_string[j]) {
                match=j;
                break;
            }
            j++;
        }
        if (-1!=match) {
            /*
             * This is a valid entry. Compare first part of string with name
             * to see if this is the entry we are looking for
             */
            if (0==strncmp(name, env_string, match)) {
                /*
                 * Free last result if needed
                 */
                if (getenv_last)
                    free(getenv_last);
                val_length = strlen(env_string)-match-1;
                getenv_last = (char*) malloc(val_length+1);
                if (0==getenv_last) {
                    errno = ENOMEM;
                    return 0;
                }
                strncpy(getenv_last, env_string+match+1, val_length);
                getenv_last[val_length]=0;
                return getenv_last;
            }
        }
        i++;
    }
    return 0;
}



