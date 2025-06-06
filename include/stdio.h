/*
 * Copyright 2018-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __WISE_STDIO_H__
#define __WISE_STDIO_H__

#include <stddef.h>
#include <stdarg.h>

#ifdef __USE_NATIVE_HEADER__

#include_next <stdio.h>

#else                           /* __USE_NATIVE_HEADER__ */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_NDS32
#include <sys/reent.h>
#define FILE __FILE
#else
typedef void FILE;
#endif

extern FILE *os_stdin, *os_stdout, *os_stderr;

#define EOF	(-1)

#define _IOFBF  0               /* setvbuf should be fully buffered */
#define _IOLBF  1               /* setvbuf should set line buffered */
#define _IONBF  2               /* setvbuf should set unbuffered */

FILE *_os_fopen(const char *pathname, const char *mode);
extern FILE *(*os_fopen)(const char *pathname, const char *mode);

FILE *_os_fdopen(int, const char *);
extern FILE *(*os_fdopen) (int fd, const char *mode);

int _os_fclose(FILE * stream);
extern int (*os_fclose)(FILE * stream);

int os_ferror(FILE * stream);
size_t _os_fwrite(const void *ptr, size_t size, size_t nmemb, FILE * stream);
extern size_t (*os_fwrite)(const void *ptr, size_t size, size_t nmemb, FILE * stream);

int _os_fputc(int c, FILE * stream);
extern int (*os_fputc)(int c, FILE * stream);

int _os_fputs(const char *s, FILE * stream);
extern int (*os_fputs)(const char *s, FILE * stream);

int os_putc(int c, FILE * stream);
size_t _os_fread(void *ptr, size_t size, size_t nmemb, FILE * stream);
extern size_t (*os_fread)(void *ptr, size_t size, size_t nmemb, FILE * stream);

int _os_fgetc(FILE * stream);
extern int (*os_fgetc)(FILE * stream);


char *_os_fgets(char *s, int size, FILE * stream);
extern char *(*os_fgets)(char *s, int size, FILE * stream);

int os_getc(FILE * stream);
int _os_ungetc(int c, FILE * stream);
extern int (*os_ungetc)(int c, FILE * stream);

void os_perror(const char *s);


extern int (*os_remove)(const char *pathname);
extern int (*os_rename)(const char *oldpath, const char *newpath);

int _os_fprintf(FILE * stream, const char *format, ...);
extern int (*os_fprintf)(FILE * stream, const char *format, ...);

int _os_sprintf(char *str, const char *format, ...);
extern int (*os_sprintf)(char *str, const char *format, ...);

int _os_snprintf(char *str, size_t size, const char *format, ...);
extern int (*os_snprintf)(char *str, size_t size, const char *format, ...);

int _os_printf(const char *format, ...);
extern int (*os_printf)(const char *format, ...);

int os_vprintf(const char *format, va_list ap);
int os_vfprintf(FILE * stream, const char *format, va_list ap);

int vsprintf(char *str, const char *format, va_list ap);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);

int os_setvbuf(FILE * stream, char *buf, int mode, size_t size);
int os_putchar(int c);

int _os_puts(const char *s);
extern int (*os_puts)(const char *s);

int os_getchar(void);
int sscanf(const char *str, const char *format, ...);

int os_asprintf(char **ptr, const char *fmt, ...);
int os_vasprintf(char **ptr, const char *fmt, va_list ap);

#define stdin    os_stdin
#define stdout   os_stdout
#define stderr   os_stderr

#define fopen	 os_fopen
#define fdopen	 os_fdopen
#define fclose	 os_fclose
#define ferror	 os_ferror
#define fwrite	 os_fwrite
#define fputc	 os_fputc
#define fputs	 os_fputs
#define putc	 os_putc
#define fread	 os_fread
#define fgetc	 os_fgetc
#define fgets	 os_fgets
#define getc	 os_getc
#define ungetc	 os_ungetc
#define perror	 os_perror
#define fprintf	 os_fprintf
#ifndef sprintf
#define sprintf	 os_sprintf
#endif
#ifndef snprintf
#define snprintf os_snprintf
#endif
#ifndef printf
#define printf(format, ...)   os_printf(format, ##__VA_ARGS__)
#endif
#define vprintf	 os_vprintf
#define vfprintf os_vfprintf
#define putchar  os_putchar
#define puts     os_puts
#define getchar  os_getchar
#define setvbuf   os_setvbuf

#define remove(pathname) os_remove(pathname)
#define rename(oldpath, newpath) os_rename(oldpath, newpath)

#ifndef asprintf
#define asprintf 	os_asprintf
#endif
#ifndef vasprintf
#define vasprintf	os_vasprintf
#endif

int _getchar_timeout(unsigned timeout);
extern int (*getchar_timeout)(unsigned timeout);

int kvprintf(char const *fmt, void (*func)(int, void *), void *arg,
         int radix, va_list ap);

#ifdef __cplusplus
}
#endif

#endif                          /* __USE_NATIVE_HEADER__ */

#endif
