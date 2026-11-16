#ifndef __STDLIB_H
#define __STDLIB_H
#pragma lib "/$M/lib/ape/libap.a"

#include <stddef.h>

#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0
#define MB_CUR_MAX 4
#define RAND_MAX 32767

typedef struct { int quot, rem; } div_t;
typedef struct { long quot, rem; } ldiv_t;
#if __STDC_VERSION__ >= 199901L || __cplusplus >= 201103L
typedef struct { long long quot, rem; } lldiv_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern double atof(const char *);
extern int atoi(const char *);
extern long int atol(const char *);
extern long long atoll(const char *);
extern double strtod(const char *, char **);
extern long int strtol(const char *, char **, int);
extern unsigned long int strtoul(const char *, char **, int);
extern long long int strtoll(const char *, char **, int);
extern unsigned long long int strtoull(const char *, char **, int);
extern int rand(void);
extern void srand(unsigned int seed);
extern void *calloc(size_t, size_t);
extern void free(void *);
extern void *malloc(size_t);
extern void *realloc(void *, size_t);
extern void abort(void);
extern int atexit(void (*func)(void));
extern void exit(int);
extern char *getenv(const char *);
#if _POSIX_C_SOURCE >= 200112L
extern int setenv(const char *, const char *, int);
extern int unsetenv(const char *);
extern int posix_memalign(void **, size_t, size_t);
#endif
#if _POSIX_C_SOURCE >= 200809L || defined(_SUSV2_SOURCE)
extern char *realpath(const char *, char *);
extern int mkstemp(char *);
extern char *mkdtemp(char *);
#endif
#if _POSIX_C_SOURCE >= 202405L || defined(_BSD_EXTENSION)
extern void *reallocarray(void *, size_t, size_t);
#endif
#ifdef _SUSV2_SOURCE
extern char *ptsname(int);	/* XSI: libbsd's, as <sys/pty.h> */
extern double drand48(void);
extern double erand48(unsigned short [3]);
extern long lrand48(void);
extern long nrand48(unsigned short [3]);
extern long mrand48(void);
extern long jrand48(unsigned short [3]);
extern void srand48(long);
extern unsigned short *seed48(unsigned short [3]);
extern void lcong48(unsigned short [7]);
#endif
extern int system(const char *);
extern void *bsearch(const void *, const void *, size_t, size_t, int (*)(const void *, const void *));
extern void qsort(void *, size_t, size_t, int (*)(const void *, const void *));
extern int abs(int);
extern div_t div(int, int);
extern long int labs(long int);
extern ldiv_t ldiv(long int, long int);
extern int mblen(const char *, size_t);
extern int mbtowc(wchar_t *, const char *, size_t);
extern int wctomb(char *, wchar_t);
extern size_t mbstowcs(wchar_t *, const char *, size_t);
extern size_t wcstombs(char *, const wchar_t *, size_t);

#if __STDC_VERSION__ >= 199901L || __cplusplus >= 201103L
extern long long llabs(long long);
extern lldiv_t lldiv(long long, long long);
extern float strtof(const char *, char **);
extern long double strtold(const char *, char **);
extern void _Exit(int);
#endif
#if __STDC_VERSION__ >= 201112L || __cplusplus >= 201103L
extern void *aligned_alloc(size_t, size_t);
extern int at_quick_exit(void (*)(void));
extern void quick_exit(int);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __STDLIB_H */
