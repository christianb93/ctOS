/*
 * limits.h
 *
 */

#ifndef __LIMITS_H_
#define __LIMITS_H_

#define INT_MAX ((int) 0x7FFFFFFF)
#define UINT_MAX ((unsigned int) 0xFFFFFFFF)
#define LONG_MAX ((long) 0x7FFFFFFF)
#define ULONG_MAX ((unsigned int) 0xFFFFFFFF)
#define LONG_MIN ((long) -0x80000000)
#define LLONG_MAX ((long long) 0x7FFFFFFFFFFFFFFFLL)
#define LLONG_MIN ((long long) -0x8000000000000000LL)
#define ULLONG_MAX ((unsigned long long) 0xFFFFFFFFFFFFFFFFLL)


#define PIPE_BUF 1024
/*
 * This should match FS_MAX_FD in fs.h
 */
#define OPEN_MAX 127

/*
 * Maximum length a path can have
 */
#define PATH_MAX 1024

/*
 * Maximum number of links to an inode
 */
#define LINK_MAX 16

/*
 * Maximum number of exit handlers
 */
#define ATEXIT_MAX 64

#endif /* __LIMITS_H_ */

