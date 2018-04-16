/*
 * fnmatch.h
 *
 */

#ifndef _FNMATCH_H_
#define _FNMATCH_H_

#define FNM_NOMATCH 1
#define FNM_PATHNAME 1
#define FNM_NOESCAPE 2

int fnmatch(const char *, const char *, int);

#endif /* _FNMATCH_H_ */
