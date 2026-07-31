#ifndef SATURN_COMPAT_H
#define SATURN_COMPAT_H
#include <stddef.h>
#include <stdarg.h>

#ifndef SEEK_SET
#define SEEK_SET 0
#endif
#ifndef SEEK_CUR
#define SEEK_CUR 1
#endif
#ifndef SEEK_END
#define SEEK_END 2
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AW_FILE FILE;
FILE  *fopen(const char *path, const char *mode);
int    fclose(FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int    fseek(FILE *stream, long offset, int whence);
long   ftell(FILE *stream);
void   rewind(FILE *stream);

extern FILE *stdout;
extern FILE *stderr;

#define fflush(stream) ((void)0)
int  printf(const char *fmt, ...);
int  fprintf(FILE *stream, const char *fmt, ...);

#ifndef SATURN_DIAG
#define SATURN_DIAG 0
#endif
int  sprintf(char *str, const char *fmt, ...);
int  snprintf(char *str, size_t size, const char *fmt, ...);
int  vsprintf(char *str, const char *fmt, va_list ap);

void  *malloc(size_t size);
void   free(void *ptr);
void  *realloc(void *ptr, size_t size);

void  *mem_alloc_low(size_t size);

void   exit(int status) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif
#endif
