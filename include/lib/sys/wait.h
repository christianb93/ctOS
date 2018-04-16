/*
 * wait.h
 */

#ifndef __SYS_WAIT_H_
#define __SYS_WAIT_H_

#include "resource.h"

#ifndef __pid_t_defined
typedef  int pid_t;
#define __pid_t_defined
#endif

/*
 * Make sure that this is aligned with the definitions in pm.h
 */
#define WIFEXITED(x)     (((x) & 0xff)==0)
#define WEXITSTATUS(x)   ((((x) >> 8) & 0xff))

#define WIFSTOPPED(x)    (( (x) & 0xff) == 0177)
#define WSTOPSIG(x)      ((((x) >> 8) & 0xff))

#define WIFSIGNALED(x)   (((x) & 0xff) && (((x) & 0xff) != 0177))
#define WTERMSIG(x)      (((x) & 0xff))

#define WCOREDUMP(x)     (( (x) & 0xff) == 0200)

/*
 * Options for waitpid
 */
#define WNOHANG 1
#define WUNTRACED 2

pid_t  wait(int *);
pid_t  waitpid(pid_t, int *, int);
pid_t wait3(int *status, int options, struct rusage *rusage);


#endif /* __SYS_WAIT_H_ */
