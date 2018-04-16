/*
 * stdio.h
 *
 */

#ifndef _STDIO_H_
#define _STDIO_H_

#include "stddef.h"
#include "os/streams.h"
#include "sys/types.h"
#include "stdarg.h"

/*
 * Default size of a stdio buffer
 */
#define BUFSIZ 256

/*
 * End-of-file condition
 */
#define EOF -1

/*
 * Buffering mode
 */
#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2

/*
 * A FILE
 */
typedef __ctOS_stream_t FILE;

/*
 * Pre-defined streams
 */
extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;

/*
 * Operations on files
 */
void clearerr(FILE *stream);
FILE* fopen(const char* filename, const char* mode);
void clearerr(FILE* stream);
int feof(FILE* stream);
int ferror(FILE* stream);
int fgetc(FILE* stream);
int fgetc(FILE* stream);
int fputc(int c, FILE* stream);
long ftell(FILE* stream);
int fseek(FILE* stream, long offset, int whence);
int fseeko(FILE* stream, off_t offset, int whence);
char* fgets(char* s, int n, FILE* stream);
int remove(const char *path);
int rename(const char *old, const char *new);
int fputs(const char* s, FILE* stream);
size_t fread(void* ptr, size_t size, size_t nitems, FILE* stream);
FILE *fdopen(int fd, const char *mode);
FILE *freopen(const char* filename, const char* mode,  FILE* stream);
int fclose(FILE* stream);
size_t fwrite(void* ptr, size_t size, size_t nitems, FILE* stream);
int getc(FILE* stream);
int putc(int c, FILE* stream);
int putchar(int c);
int puts(char* s);
int getchar();
void rewind(FILE* stream);
int setvbuf(FILE* stream, char* buf, int type, size_t size);
void setbuf(FILE* stream, char* buf);
int ungetc(int c, FILE *stream);
int fflush(FILE* stream);
int printf(const char *format, ...);
int fprintf(FILE* stream, const char* format, ...);
int snprintf(char* s, size_t n, const char* format, ...);
int vsnprintf(char* s, size_t n, const char* format, va_list ap);
int sprintf(char* s, const char* format, ...);
int vfprintf(FILE* stream, const char* format, va_list ap);
int vprintf(const char* format, va_list ap);
int vsprintf(char* s, const char* format, va_list ap);
int sscanf(const char* s, const char* template, ...);
int vsscanf(const char* s, const char* format, va_list arg);
int vfscanf(FILE* stream, const char* format, va_list arg);
int fscanf(FILE* stream, const char* format, ...);
int scanf(const char* format, ...);
int vscanf(const char* format, va_list args);
int fileno(FILE *stream);
void perror(const char *s);

#endif /* _STDIO_H_ */
