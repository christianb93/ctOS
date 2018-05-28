/*
 * mntent.c
 *
 */

#include "lib/mntent.h"
#include "lib/stdlib.h"
#include "lib/string.h"

static struct mntent _mntent;

/*
 * Prepares the read from the mount table. The provided file must be in the format of the /etc/mtab file
 *
 * If the function was successful, a file handler is returned, otherwise NULL is returned and errno is set
 * accordingly.
 *
 * BASED ON: Linux getmntent
 *
 */
FILE* setmntent(const char *file, const char *mode) {
    FILE* mtab;
    mtab = fopen(file, mode);
    /*
     * Set up mntent
     */
    _mntent.mnt_fsname = 0;
    _mntent.mnt_fsname = 0;
    _mntent.mnt_dir = 0;
    _mntent.mnt_freq = 0;
    _mntent.mnt_opts = 0;
    _mntent.mnt_passno = 0;
    return mtab;
}

#define ALLOC_AND_COPY(x)  { if (x) free(x); x = malloc(strlen(token)+1);  strncpy(x, token, strlen(token)); }
#define FREE(x) { if (x) free(x); x = 0;}

/*
 * This function accepts a file handle previously returned by getmntent. It then
 * reads the next entry from that file and provides an instance of the struct mntent
 * filled with the respective entry or NULL if no further entry could be found
 * 
 * The file is parsed according to the following rules:
 * Blank lines and lines starting with a comment are ignored
 * Fields are separated by spaces and / or tabs
 * No (!) conversion of backslash and spaces is performed 
 * 
 * Note that a pointer to the same static buffer will be returned, hence this
 * function is not thread safe
 */
 
struct mntent * getmntent (FILE *stream) {
    char line[512];
    int pos = 0;
    char* token;
    if (0 == stream)
        return 0;
    /*
     * Read the next line from the file. Somehow we assume that a line
     * has at most 512 characters...
     */
    if (fgets(line, 512, stream)) {
        /*
         * Now parse the entries
         *
         */
        pos = 0;
        token = strtok(line, " \t");
        while (token) {
            switch (pos) {
                    case 0:
                        ALLOC_AND_COPY(_mntent.mnt_fsname);
                        break;
                    case 1: 
                        ALLOC_AND_COPY(_mntent.mnt_dir);
                        break;
                    case 2: 
                        ALLOC_AND_COPY(_mntent.mnt_type);
                        break;
                    case 3: 
                        ALLOC_AND_COPY(_mntent.mnt_opts);
                        break;                  
                    case 4:
                        _mntent.mnt_freq = strtol(token, NULL, 10);
                        break;
                    case 5:
                        _mntent.mnt_passno = strtol(token, NULL, 10);
                        break;                        
                    default:
                        break;
            }
            pos++;
            token = strtok(NULL, " \t");
        }
        return &_mntent;
    }
    return 0;
}

/*
 * Close the mtab file and free all memory
 */
int endmntent(FILE *filep) {
    if (filep)
        fclose(filep); 
    FREE(_mntent.mnt_fsname);
    FREE(_mntent.mnt_dir);
    FREE(_mntent.mnt_type);
    FREE(_mntent.mnt_opts);
    return 1;
}