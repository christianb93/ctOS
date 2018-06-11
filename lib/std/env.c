/*
 * Routines to manage environment variables
 *
 * This module maintains its own environment pointer char** __environ which is stored and
 * operated on by all calls
 *
 * The tricky part is managing the memory used for the environment strings. The POSIX putenv call
 * requires that the passed string becomes part of the environment, i.e. environ[i] points to memory
 * owned by the application. We must never free this memory and if we clone an environment because
 * we need more space, we need to copy the pointer. In other cases, when setenv is used, the library
 * owns the memory. To be able to distinguish between these two cases, we maintain two data structures.
 *
 * - an environment structure
 * - an array of integers telling us whether we own the respective entry
 * 
 * Then the following rules apply for memory management:
 * 
 * When the library is initialized in _start (crt0.c), we reallocate the memory for the array, but leave
 * the environment strings where they are (on the stack of the main function, where the exec system call
 * has placed them). 
 * 
 * The only way how ownership of an environment string can be given to the library is to use setenv, otherwise
 * all strings are owned by the application
 * 
 * When putenv is called for an existing key, the old string is only freed if it is owned by the library, and the new
 * string will be owned by the application
 * 
 * When the environment has to be reallocated, no strings will be freed or reallocated 
 */
 
#include "lib/stdlib.h"
#include "lib/errno.h"
#include "lib/string.h"

/*
 * This is the external environment pointer
 */
extern char** environ;

/*
 * This is the internally maintained pointer
 */
static char** __ctOS_environ = 0;
/*
 * and this is the integer array that tells us
 * whether we own an entry
 */
static char* __ctOS_entry_owner = 0;

/*
 * Utility function to count the number of (non-NULL)
 * entries in an environment
 */
int static count_entries(char** env) {
    int entries = 0;
    while (env[entries]) {
        entries++;
    }
    return entries;
}

/*
 * Free the existing copy of the environment. This will also free the
 * environment itself and the owner list
 */
static void __ctOS_free_environ(char** __environ, char* owner_list) {
    if (0 == __environ)
        return;
    int i = 0;
    while (__environ[i]) {
        /*
         * Only free the entry if the owner list
         * has a one at this point
         */
        if (1 == owner_list[i])
            free(__environ[i]);
        i++;
    }
    free(__environ);
    free(owner_list);
}

/*
 * Copy an existing environment to a given target. This assumes
 * that there is sufficient memory in the target. No trailing NULL
 * will be written
 * The first two arguments are source and target for the environment,
 * the third and fourth are source and target for the owner list
 * If src_owner is NULL, ownership for all entries is assumed
 * to be with the caller
 * 
 * This is not a deep copy, i.e. the environment strings themselves
 * will stay where they are
 */
static void __ctOS_copy_environ(char** src, char** target, char* src_owner, char* target_owner) {
    int entries = 0;
    int i;
    /*
     * Check how many entries we have in the source
     */
    entries = count_entries(src);
    for (i = 0; i < entries; i++) {
        target[i] =  src[i];
        if (src_owner)
            target_owner[i] = src_owner[i];
        else
            target_owner[i] = 0;
    }
}

/*
 * Clone an existing environment. This function will initialize its own environment
 * from the given one and return the address of the new environment. It will not try
 * to free the environment passed as argument. It will, however, free the internal
 * environment if this is set.
 */

char** __ctOS_clone_environ(char** __environ) {
    int entries;
    if (0 == __environ) {
        errno = EINVAL;
        return 0;
    }
    /*
     * If there is already an environment, free it
     */
    if (__ctOS_environ) {
        __ctOS_free_environ(__ctOS_environ, __ctOS_entry_owner);
        __ctOS_environ = 0;
        __ctOS_entry_owner = 0;
    }
    /*
     * Now see how many environment entries we have in the source
     */
    entries = count_entries(__environ);
    /*
     * We need to allocate one more entry as we add a trailing NULL
     */
    __ctOS_environ = malloc(sizeof(char*) * (1 + entries));
    if (0 == __ctOS_environ) {
        errno = ENOMEM;
        return 0;
    }
    /*
     * We might be called with an empy environment. In order
     * to avoid that malloc returns NULL, we always allocate
     * at least one entry
     */
    __ctOS_entry_owner = malloc(sizeof(char) * (entries ? entries : 1));
    if (0 == __ctOS_entry_owner) {
        free(__ctOS_environ);
        __ctOS_environ = 0;
        errno = ENOMEM;
        return 0;
    }
    /*
     * Now create copies of the entries
     */
    __ctOS_copy_environ(__environ, __ctOS_environ, NULL, __ctOS_entry_owner);
    /*
     * Finally set last entry to zero
     */
    __ctOS_environ[entries] = 0;
    return __ctOS_environ;
}


/*
 * Search the internally maintained environment for a given key and return
 * the value. Note that the application must never change this value directly
 *
 */
char* __ctOS_getenv(const char* key) {
    int i = 0;
    char* index;
    if (0 == __ctOS_environ) {
        return 0;
    } 
    while (__ctOS_environ[i]) {
        /*
         * Find index of '='
         */
        index = strchr(__ctOS_environ[i], '=');
        /*
         * If we have found a = and it is not the first character, check
         * whether this is our entry
         */
        if ((index) && (index != __ctOS_environ[i])) {
            if (0 == strncmp(__ctOS_environ[i], key, index - __ctOS_environ[i])) {
                return index + 1;
            }
        }
        i++;
    }
    return 0;
}

/*
 * Create a new or updated environment entry. As this might reallocate the 
 * memory for the environment, it will return the newly allocated memory or
 * 0 on error
 * 
 */
char** __ctOS_putenv(char* string) {
    int key_len = 0;
    char* index;
    int i;
    int entries;
    /*
     * First we validate the string passed
     * as argument and determine the length
     * of the key part
     * 
     */
    if (0 == string)
        return 0;
    index = strchr(string, '=');
    if ((0 == index) || (index == string))
        return 0;
    key_len = index - string;
    /*
     * Now we walk the existing environment and see whether we already have 
     * an entry for this key
     */
    i = 0;
    while (__ctOS_environ[i]) {
        index = strchr(__ctOS_environ[i], '=');
        /*
         * Do the keys have the same length?
         */
        if ((index) && ((index - __ctOS_environ[i]) == key_len)) {
            if (0 == strncmp(__ctOS_environ[i], string, key_len)) {
                /*
                 * We have found the entry. Now we redefine the environment pointer
                 * and free the old string if we own it
                 */
                if (1 == __ctOS_entry_owner[i]) {
                    free(__ctOS_environ[i]);
                }
                __ctOS_environ[i] = string;
                __ctOS_entry_owner[i] = 0;
                return __ctOS_environ; 
            }
        }
        i++;
    }
    /*
     * If we get to this point, the entry does not exist yet. So we need
     * to reallocate the entire environment and add the additional string
     */
    entries = count_entries(__ctOS_environ);
    char** new_env = malloc(sizeof(char*)*(2 + entries));
    if (0 == new_env) {
        return 0;
    }
    char* new_owner_list = malloc(sizeof(char)*(2 + entries));
    if (0 == new_owner_list) {
        free(new_env);
        return 0;
    }
    /*
     * Now copy each environment string from the old to the new
     * environment. This step does not reallocate or free any
     * memory
     */
    for (i = 0; i < entries; i++) {
        new_env[i] = __ctOS_environ[i];
        new_owner_list[i] = __ctOS_entry_owner[i];
    }
    /*
     * and free the old one
     */
    free(__ctOS_environ);
    free(__ctOS_entry_owner);
    __ctOS_environ = new_env;
    __ctOS_entry_owner = new_owner_list;
    /*
     * Finally place the new string as entry entries
     * and add a NULL
     */
    __ctOS_environ[entries] = string;
    __ctOS_entry_owner[entries] = 0;
    __ctOS_environ[entries + 1] = 0;
    return __ctOS_environ;
}

/*
 * This function will validate the ownership list of the existing environment against a
 * second environment. If any of the entries that we think we own is detected in the
 * second environment, the ownership flag is reset
 */
static void __ctOS_validate_environ(char** ref_env) {
    if (0 == __ctOS_environ)
        return;
    int i = 0;
    int j = 0;
    /*
     * Walk our environment
     */
    while (__ctOS_environ[i]) {
        /*
         * Check whether the same string is referenced in ref_env
         */
        j = 0;
        while (ref_env[j]) {
            if (ref_env[j] == __ctOS_environ[i]) {
                __ctOS_entry_owner[i] = 0;
                break;
            }
            j++;
        }
        i++;
    }
}

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
    /*
     * Some application might reallocate environ themselves and
     * come with their own implementation of putenv (like the GNU lib)
     * To deal with this, we first check whether environ is still what
     * we think it should be and re-init if needed
     * 
     * However there is one situation against which we need to protect ourselves. 
     * Suppose that the application has used setenv to add an environment string and
     * then reallocates environ, but simply copies the pointer to the environment strings
     * into the newly allocated array. When we now simply free our environment, we will free
     * the memory allocated by setenv, even though the application still holds a reference. 
     * We therefore validate the environment first, i.e. we check whether the new environ
     * contains any pointer to memory that we own and hand over ownership to the application
     * (accepting a potentially resulting memory leak)
     */
    if (environ != __ctOS_environ) {
        __ctOS_validate_environ(environ);
        environ = __ctOS_clone_environ(environ);
    }
    return __ctOS_getenv(name);
}


/*
 * The putenv() function will add the given string to the environment. The argument should point to a string of the form name=value. 
 * 
 * Note that putenv can either add a new environment entry or change an existing one. In either case, the string pointed to by string will 
 * become part of the environment, so altering the string will change the environment. The memory management for the string remains
 * the responsibility of the callee, so the callee must make sure that the string is not freed and is not an automatic variable on the stack
 * going out of scope
 * 
 * This function may reallocate the memory pointed to be environ and change environ correspondingly
 * 
 */
int putenv(char* string) {
    /*
     * Resynchronize our environment with the global one 
     * if needed - see the comments for getenv
     */
   if (environ != __ctOS_environ) {
        __ctOS_validate_environ(environ);
        environ = __ctOS_clone_environ(environ);
    }     
    char** new_env = __ctOS_putenv(string);
    if (0 == new_env) {
        errno = ENOMEM;
        return -1;
    }
    environ = new_env;
    return 0;
}

