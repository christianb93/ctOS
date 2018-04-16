#ifndef _STDDEF_H
#define _STDDEF_H

#define NULL ((void*) 0)


typedef __WCHAR_TYPE__ wchar_t;
typedef int ptrdiff_t;

#define offsetof(st, m) __builtin_offsetof(st, m)

#endif
