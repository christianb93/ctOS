/*
 * types.h
 *
 */

#ifndef __OS_TYPES_H_
#define __OS_TYPES_H_

#ifndef __off_t_defined
typedef int off_t;
#define __off_t_defined
#endif

#ifndef __ino_t_defined
typedef unsigned int ino_t;
#define __ino_t_defined
#endif

#ifndef __dev_t_defined
typedef unsigned int dev_t;
#define __dev_t_defined
#endif

#ifndef __mode_t_defined
typedef unsigned int mode_t;
#define __mode_t_defined
#endif

#ifndef __time_t_defined
typedef long int time_t;
#define __time_t_defined
#endif

#ifndef __suseconds_t_defined
typedef long int suseconds_t;
#define __suseconds_t_defined
#endif

#ifndef __clock_t_defined
typedef int clock_t;
#define __clock_t_defined
#endif

#ifndef __pid_t_defined
typedef  int pid_t;
#define __pid_t_defined
#endif

#ifndef __ssize_t_defined
typedef int ssize_t;
#define __ssize_t_defined
#endif

#ifndef __size_t_defined
typedef unsigned int size_t;
#define __size_t_defined
#endif

#ifndef __uid_t_defined
typedef unsigned int uid_t;
#define __uid_t_defined
#endif

#ifndef __gid_t_defined
typedef unsigned int gid_t;
#define __gid_t_defined
#endif

#ifndef __nlink_t_defined
typedef unsigned int nlink_t;
#define __nlink_t_defined
#endif

#endif /* __OS_TYPES_H_ */
