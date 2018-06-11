#ifndef _STDLIB_H
#define _STDLIB_H

#include "unistd.h"
#include "stddef.h"

#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0

#define RAND_MAX (0x7FFFFFFF)

#define MB_CUR_MAX 1


/*
 * If we use GCC, declare alloca
 */
#ifdef __GNUC__
#define alloca(size) __builtin_alloca(size)
#endif

long strtol(const char* s, char** end_ptr, int base);
long long strtoll(const char* s, char** end_ptr, int base);
unsigned long long strtoull(const char* s, char** end_ptr, int base);
unsigned long strtoul(const char* s, char** end_ptr, int base);
int atoi(const char *str);
long atol(const char *str);
void *malloc(size_t size);
void *calloc(size_t nelem, size_t elsize);
void *realloc(void *ptr, size_t size);
void free(void* mem);
void exit(int status);
void qsort(void *base, size_t nel, size_t width, int (*compar)(const void *, const void *));
void abort();
char *getenv(const char *name);
int putenv(char* string);
int mbtowc(wchar_t* pwc, const char* s, size_t n);
int mbtowc(wchar_t * pwc, const char * s, size_t n);
int atexit(void (*func)(void));
int system(const char* command);

#endif
