#ifndef __WCTYPE_H
#define __WCTYPE_H
#pragma lib "/$M/lib/ape/libap.a"
/*
 * C95 wctype.h: the classes of runes are libutf's (isalpharune, ...).
 */
#include <stddef.h>

#ifndef _WINT_T
#define _WINT_T
typedef unsigned int wint_t;
#endif
typedef int wctrans_t;
typedef int wctype_t;
#ifndef WEOF
#define WEOF	0xffffffffU
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern int iswalnum(wint_t);
extern int iswalpha(wint_t);
extern int iswblank(wint_t);
extern int iswcntrl(wint_t);
extern int iswdigit(wint_t);
extern int iswgraph(wint_t);
extern int iswlower(wint_t);
extern int iswprint(wint_t);
extern int iswpunct(wint_t);
extern int iswspace(wint_t);
extern int iswupper(wint_t);
extern int iswxdigit(wint_t);
extern int iswctype(wint_t, wctype_t);
extern wctype_t wctype(const char *);
extern wint_t towlower(wint_t);
extern wint_t towupper(wint_t);
extern wint_t towctrans(wint_t, wctrans_t);
extern wctrans_t wctrans(const char *);

#ifdef __cplusplus
}
#endif

#if _POSIX_C_SOURCE >= 200809L
#ifndef _LOCALE_T
#define _LOCALE_T
typedef struct __locale *locale_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern int iswalnum_l(wint_t, locale_t);
extern int iswalpha_l(wint_t, locale_t);
extern int iswblank_l(wint_t, locale_t);
extern int iswcntrl_l(wint_t, locale_t);
extern int iswdigit_l(wint_t, locale_t);
extern int iswgraph_l(wint_t, locale_t);
extern int iswlower_l(wint_t, locale_t);
extern int iswprint_l(wint_t, locale_t);
extern int iswpunct_l(wint_t, locale_t);
extern int iswspace_l(wint_t, locale_t);
extern int iswupper_l(wint_t, locale_t);
extern int iswxdigit_l(wint_t, locale_t);
extern int iswctype_l(wint_t, wctype_t, locale_t);
extern wctype_t wctype_l(const char *, locale_t);
extern wint_t towlower_l(wint_t, locale_t);
extern wint_t towupper_l(wint_t, locale_t);
extern wint_t towctrans_l(wint_t, wctrans_t, locale_t);
extern wctrans_t wctrans_l(const char *, locale_t);

#ifdef __cplusplus
}
#endif
#endif

#endif /* __WCTYPE_H */
