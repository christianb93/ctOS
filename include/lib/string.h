#ifndef _STRING_H
#define _STRING_H

#include "unistd.h"

int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, int max);
int strlen(const char* s);
char* strncpy(char* s1, const char* s2, int max);
char *strcat(char* s1, const char* s2);
char *strerror(int errnum);
char* strcpy(char* s1, const char* s2);
void* memcpy(void* to, const void* from, size_t n);
char *strtok(char *s1, const char *s2);
size_t strspn(const char *s, const char *accept);
size_t strcspn(const char *s, const char *reject);
void *memset(void *s, int c, size_t n);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
char *strstr(const char *s1, const char *s2);
int memcmp(const void *s1, const void *s2, size_t n);
char* strdup(const char* src);
void* memmove(void *s1, const void *s2, size_t n);
char *strpbrk(const char *s1, const char *s2);
char *strncat(char* s1, const char* s2, size_t n);
int strcoll(const char *s1, const char *s2);



#endif
