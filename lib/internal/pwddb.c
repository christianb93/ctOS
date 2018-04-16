/*
 * pwd.c
 *
 * Functions to manage the password database
 */


#include "lib/os/pwddb.h"
#include "lib/stdio.h"
#include "lib/string.h"
#include "lib/stdlib.h"
#include "lib/os/errors.h"

/*
 * Maxixum length of a line in the password file
 */
#define PWD_LINE_MAX 1024
/*
 * Number of fields in a line
 */
#define PWD_LINE_FIELDS 7

/*
 * State of password database file
 */
static FILE* db = 0;
static int error = 0;
static char line[PWD_LINE_MAX];
static struct __ctOS_passwd current_entry;

/*
 * Errors
 */
#define PWD_ERROR_LINE_TOO_LONG 1
#define PWD_ERROR_NR_FIELDS 2
#define PWD_ERROR_FILE_NOT_FOUND 3
#define PWD_ERROR_NOT_NUMERIC 4



/*
 * Read next entry from the password database and store it in our static buffers. Return zero upon success,
 * -1 if the end of the file is reached or an error code
 */
static int get_next_entry(FILE* __db) {
    char* fields[PWD_LINE_FIELDS];
    int i;
    char* ptr;
    char* tmp;
    /*
     * Read next line
     */
    if (0 == fgets(line, PWD_LINE_MAX, __db)) {
        return -1;
    }
    /*
     * We expect that we have read an entire line, i.e. the last character is a \n
     */
    if (0 == strlen(line)) {
        return PWD_ERROR_LINE_TOO_LONG;
    }
    if ('\n' != line[strlen(line) - 1])
        return PWD_ERROR_LINE_TOO_LONG;
    line[strlen(line) - 1] = 0;
    /*
     * Parse line. We replace each : by a 0 and store the pointers to the individual strings
     * obtained in this way in our array
     */
    ptr = line;
    for (i = 0; i < PWD_LINE_FIELDS; i++) {
        fields[i] = ptr;
        while ((*ptr) &&(':' != *ptr))
            ptr++;
        /*
         * Premature end of line - return error
         */
        if ((0 == *ptr) && (i < PWD_LINE_FIELDS - 1)) {
            return PWD_ERROR_NR_FIELDS;
        }
        *ptr = 0;
        ptr++;
    }
    /*
     * Now populate password entry
     */
    current_entry.pw_name = fields[0];
    current_entry.pw_passwd = fields[1];
    current_entry.pw_shell = fields[6];
    current_entry.pw_gecos = fields[4];
    current_entry.pw_dir = fields[5];
    /*
     * Convert numeric fields
     */
    current_entry.pw_gid = strtoll(fields[3], &tmp, 10);
    if (*tmp) {
        return PWD_ERROR_NOT_NUMERIC;
    }
    current_entry.pw_uid = strtoll(fields[2], &tmp, 10);
    if (*tmp) {
        return PWD_ERROR_NOT_NUMERIC;
    }
    return 0;
}

/*
 * Get the next entry from the password database. If the database is not yet open, it will
 * be opened.
 *
 * If an error occurs or the end of the database is reached, NULL is returned
 */
struct __ctOS_passwd *__ctOS_getpwent() {
    /*
     * If there is an error, return - there is no way to recover except rewinding
     */
    if (error)
        return 0;
    /*
     * If the password file is not yet open, open it
     */
    if (0 == db) {
        db = fopen("/etc/passwd", "r");
        if (0 == db) {
            error = PWD_ERROR_FILE_NOT_FOUND;
            return 0;
        }
    }
    if ((error = get_next_entry(db))) {
        if (-1 == error) {
            error = 0;
        }
        return 0;
    }
    return &current_entry;
}

/*
 * Close password database. This will also clear any error flags
 */
void __ctOS_endpwent() {
    if (db) {
        fclose(db);
        db = 0;
        error = 0;
    }
}


/*
 * Get password entry for a specified UID
 */
struct __ctOS_passwd* __ctOS_getpwuid(uid_t uid, int* __error) {
    FILE* __db;
    int rc;
    /*
     * If there is an error return
     */
    if (error) {
        *__error = EIO;
        return 0;
    }
    /*
     * Open password database
     */
    __db = fopen("/etc/passwd", "r");
    if (0 == __db) {
        *__error = EIO;
        return 0;
    }

    /*
     * Now walk file until we find an entry with matching uid
     */
    while(1) {
        if ((rc = get_next_entry(__db))) {
            if (-1 != rc)
                *__error = EIO;
            return 0;
        }
        if (current_entry.pw_uid == uid) {
            fclose(__db);
            return &current_entry;
        }
    }

    /*
     * Close database again
     */
    fclose(__db);
    return 0;
}

/*
 * Get password entry for a specified name
 */
struct __ctOS_passwd* __ctOS_getpwnam(const char* name, int* __error) {
    FILE* __db;
    int rc;
    if (error) {
        *__error = EIO;
        return 0;
    }
    /*
     * Open password database
     */
    __db = fopen("/etc/passwd", "r");
    if (0 == __db) {
        *__error = EIO;
        return 0;
    }

    /*
     * Now walk file until we find an entry with matching name
     */
    while(1) {
        if ((rc = get_next_entry(__db))) {
            if (-1 != rc)
                *__error = EIO;
            return 0;
        }
        if (0 == strcmp(current_entry.pw_name, name)) {
            fclose(__db);
            return &current_entry;
        }
    }

    /*
     * Close database again
     */
    fclose(__db);
    return 0;
}
