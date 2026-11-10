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

#endif /* __WCTYPE_H */
