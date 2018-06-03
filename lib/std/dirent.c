/*
 * dirent.c
 */

#include "lib/dirent.h"
#include "lib/stdlib.h"
#include "lib/errno.h"
#include "lib/fcntl.h"

/*
 * The opendir() function will open a directory stream corresponding to the directory named by the dirname argument.
 * The directory stream is positioned at the first entry.
 *
 * As the type DIR is implemented using a file descriptor, applications will only be able to open up
 * to a total of {OPEN_MAX} files and directories.
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * none
 *
 */
DIR* opendir(const char* dirname) {
    int fd;
    /*
     * Open directory
     */
    fd = open((char*) dirname, 0);
    if (fd < 0) {
        errno = ENOENT;
        return 0;
    }
    /*
     * and delegate to fdopendir
     */
    return fdopendir(fd);
}

/*
 * The fdopendir() function is like opendir, except that it expects a file 
 * descriptor that points to an open directory. The system will take ownership
 * of the file descriptor
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * none
 *
 */
DIR* fdopendir(int dirfd) {
    __ctOS_dirstream_t* stream;
    /*
     * Allocate memory for stream structure
     */
    stream = (__ctOS_dirstream_t*) malloc(sizeof(__ctOS_dirstream_t));
    if (0 == stream) {
        errno = ENOMEM;
        return 0;
    }
    /*
     * Open stream
     */
    if(__ctOS_dirstream_open(stream, dirfd)) {
        errno = ENOMEM;
        free(stream);
        return 0;
    }
    return stream;
}

/*
 * The type DIR, which is defined in the <dirent.h> header, represents a directory stream, which is an ordered sequence of
 * all the directory entries in a particular directory.
 *
 * Directory entries represent files; files may be removed from a directory or added to a directory asynchronously to the
 * operation of readdir().
 *
 * The readdir() function will return a pointer to a structure representing the directory entry at the current position
 * in the directory stream specified by the argument dirp, and position the directory stream at the next entry.
 *
 * It will return a null pointer upon reaching the end of the directory stream.
 *
 * The structure dirent defined in the <dirent.h> header describes a directory entry.
 *
 * The pointer returned by readdir() points to data which may be overwritten by another call to readdir() on the same
 * directory stream. This data is not overwritten by another call to readdir() on a different directory stream.
 *
 * If a file is removed from or added to the directory after the most recent call to opendir() or rewinddir(),
 * whether a subsequent call to readdir() returns an entry for that file is unspecified.
 *
 * After a call to fork(), either the parent or child (but not both) may continue processing the directory stream using readdir(),
 * rewinddir(), or seekdir(). If both the parent and child processes use these functions, the result is undefined.
 *
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * 1) when reading from the directory, st_atime of the directory is not updated
 *
 */
struct dirent* readdir(DIR* dirp) {
    return (struct dirent*) __ctOS_dirstream_readdir(dirp);
}


/*
 * Close a directory stream
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS: none
 *
 *
 */
int closedir(DIR* dirp) {
    __ctOS_dirstream_close(dirp);
    return 0;
}


/*
 * Rewind a directory stream
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS: none
 *
 *
 */
void rewinddir(DIR* dirp) {
    __ctOS_dirstream_rewind(dirp);
}