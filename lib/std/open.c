/*
 * open.c
 *
 */

#include "lib/os/oscalls.h"
#include "lib/errno.h"
#include "lib/fcntl.h"
#include "lib/stdarg.h"


/*
 * The open() function will establish the connection between a file and a file descriptor. It will create an open file description
 * that refers to a file and a file descriptor that refers to that open file description. The file descriptor is
 * used by other I/O functions to refer to that file. The path argument points to a pathname naming the file.
 *
 * The open() function will return a file descriptor for the named file that is the lowest file descriptor not currently open
 * for that process. The open file description is new, and therefore the file descriptor will not share it with any other process
 * in the system. The FD_CLOEXEC file descriptor flag associated with the new file descriptor will be cleared.
 *
 * The file offset used to mark the current position within the file will be set to the beginning of the file.
 *
 * The file status flags and file access modes of the open file description will be set according to the value of oflag.
 *
 * Values for oflag are constructed by a bitwise-inclusive OR of flags from the following list, defined in <fcntl.h>.
 *
 * Applications shall specify exactly one of the first three values (file access modes) below in the value of oflag:
 *
 * O_RDONLY - Open for reading only
 * O_WRONLY - Open for writing only
 * O_RDWR - Open for reading and writing. The result is undefined if this flag is applied to a FIFO
 *
 * Any combination of the following may be used:
 *
 * O_APPEND
 * If set, the file offset shall be set to the end of the file prior to each write.
 *
 * O_CREAT
 * If the file exists, this flag has no effect. Otherwise, the file will be created; the user ID of the file will
 * be set to the effective user ID of the process; the group ID of the file will be set to the to the effective group ID of the process
 * and the access permission bits (see <sys/stat.h>) of the file mode will be set to the value of the third argument taken as type
 * mode_t modified as follows: a bitwise AND is performed on the file-mode bits and the corresponding bits in the complement of
 * the process' file mode creation mask. Thus, all bits in the file mode whose corresponding bit in the file mode creation mask is set
 * are cleared. The third argument does not affect whether the file is open for reading, writing, or for both.
 *
 * O_TRUNC
 * If the file exists and is a regular file, and the file is successfully opened O_RDWR or O_WRONLY, its length will be truncated to 0,
 * and the mode and owner will be unchanged. It will have no effect on FIFO special files or terminal device files.
 *
 * If O_CREAT is set and the file did not previously exist, upon successful completion, open() will mark for update the
 * st_atime, st_ctime, and st_mtime fields of the file.
 *
 * O_EXCL
 * If O_EXCL and O_CREAT are both set and the file does already exist, fail. The check is done atomically
 *
 * O_NONBLOCK
 * Request non-blocking read/write access to the file. This flag is only supported for specific file types like pipes
 *
 * BASED ON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * 1) when a file is created using O_CREAT, its group ID is always set to the effective group ID of the process
 * 2) when a file is created, the access time and modification time of the parent directory are not updated
 * 3) the flags O_EXCL, O_NOCTTY and O_NONBLOCK are not supported
 *
 */
int open(const char* path, int oflag, ...) {
    int mode = 0;
    va_list args;
    va_start(args, oflag);
    /*
     * The third argument is the mode and only supplied
     * if O_CREAT is set
     */
    if (O_CREAT | oflag) {
        mode = va_arg(args, int);
    }
    int res = __ctOS_open((char*) path, oflag, mode);
    if (res<0) {
         errno = -res;
         return -1;
     }
     return res;
}

/*
 * Create a directory with the specified path name and the specified file access mode which will be modified by the process umask.
 * Only the file permissions bits of mode are evaluated, i.e. mode & 0777.
 *
 * The newly created directory will be empty and only contain the entries for . and ..
 *
 * BASED UPON: POSIX 2004
 *
 * LIMITATIONS:
 *
 * - no file permission checks are currently implemented, user ID and group ID are not set
 *
 *
 */
int mkdir(const char* path, mode_t mode) {
    int res = __ctOS_mkdir((char*) path, mode);
    if (res < 0) {
         errno = -res;
         return -1;
     }
     return res;
}
