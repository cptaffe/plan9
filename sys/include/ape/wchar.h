#ifndef __WCHAR_H
#define __WCHAR_H
#pragma lib "/$M/lib/ape/libap.a"
/*
 * C95 and C99 wchar.h.  A wchar_t is a rune: its multibyte form is
 * UTF-8, in every locale.  A wide stream is a byte stream of UTF-8.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>

#ifndef _WINT_T
#define _WINT_T
typedef unsigned int wint_t;
#endif
#ifndef _MBSTATE_T
#define _MBSTATE_T
typedef struct { int __n; char __b[4]; unsigned int __c; } mbstate_t;	/* bytes of a rune not yet whole; a surrogate */
#endif
#define WEOF	0xffffffffU

struct tm;

#ifdef __cplusplus
extern "C" {
#endif

extern int fwprintf(FILE *, const wchar_t *, ...);
extern int fwscanf(FILE *, const wchar_t *, ...);
extern int swprintf(wchar_t *, size_t, const wchar_t *, ...);
extern int swscanf(const wchar_t *, const wchar_t *, ...);
extern int vfwprintf(FILE *, const wchar_t *, va_list);
extern int vfwscanf(FILE *, const wchar_t *, va_list);
extern int vswprintf(wchar_t *, size_t, const wchar_t *, va_list);
extern int vswscanf(const wchar_t *, const wchar_t *, va_list);
extern int vwprintf(const wchar_t *, va_list);
extern int vwscanf(const wchar_t *, va_list);
extern int wprintf(const wchar_t *, ...);
extern int wscanf(const wchar_t *, ...);
extern wint_t fgetwc(FILE *);
extern wchar_t *fgetws(wchar_t *, int, FILE *);
extern wint_t fputwc(wchar_t, FILE *);
extern int fputws(const wchar_t *, FILE *);
extern int fwide(FILE *, int);
extern wint_t getwc(FILE *);
extern wint_t getwchar(void);
extern wint_t putwc(wchar_t, FILE *);
extern wint_t putwchar(wchar_t);
extern wint_t ungetwc(wint_t, FILE *);

extern double wcstod(const wchar_t *, wchar_t **);
extern float wcstof(const wchar_t *, wchar_t **);
extern long double wcstold(const wchar_t *, wchar_t **);
extern long wcstol(const wchar_t *, wchar_t **, int);
extern long long wcstoll(const wchar_t *, wchar_t **, int);
extern unsigned long wcstoul(const wchar_t *, wchar_t **, int);
extern unsigned long long wcstoull(const wchar_t *, wchar_t **, int);

extern wchar_t *wcscpy(wchar_t *, const wchar_t *);
extern wchar_t *wcsncpy(wchar_t *, const wchar_t *, size_t);
extern wchar_t *wmemcpy(wchar_t *, const wchar_t *, size_t);
extern wchar_t *wmemmove(wchar_t *, const wchar_t *, size_t);
extern wchar_t *wcscat(wchar_t *, const wchar_t *);
extern wchar_t *wcsncat(wchar_t *, const wchar_t *, size_t);
extern int wcscmp(const wchar_t *, const wchar_t *);
extern int wcscoll(const wchar_t *, const wchar_t *);
extern int wcsncmp(const wchar_t *, const wchar_t *, size_t);
extern size_t wcsxfrm(wchar_t *, const wchar_t *, size_t);
extern int wmemcmp(const wchar_t *, const wchar_t *, size_t);
extern wchar_t *wcschr(const wchar_t *, wchar_t);
extern size_t wcscspn(const wchar_t *, const wchar_t *);
extern wchar_t *wcspbrk(const wchar_t *, const wchar_t *);
extern wchar_t *wcsrchr(const wchar_t *, wchar_t);
extern size_t wcsspn(const wchar_t *, const wchar_t *);
extern wchar_t *wcsstr(const wchar_t *, const wchar_t *);
extern wchar_t *wcstok(wchar_t *, const wchar_t *, wchar_t **);
extern wchar_t *wmemchr(const wchar_t *, wchar_t, size_t);
extern size_t wcslen(const wchar_t *);
extern wchar_t *wmemset(wchar_t *, wchar_t, size_t);
extern size_t wcsftime(wchar_t *, size_t, const wchar_t *, const struct tm *);

extern wint_t btowc(int);
extern int wctob(wint_t);
extern int mbsinit(const mbstate_t *);
extern size_t mbrlen(const char *, size_t, mbstate_t *);
extern size_t mbrtowc(wchar_t *, const char *, size_t, mbstate_t *);
extern size_t wcrtomb(char *, wchar_t, mbstate_t *);
extern size_t mbsrtowcs(wchar_t *, const char **, size_t, mbstate_t *);
extern size_t wcsrtombs(char *, const wchar_t **, size_t, mbstate_t *);

#ifdef __cplusplus
}
#endif

#endif /* __WCHAR_H */
