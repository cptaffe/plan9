#ifndef __STDDEF_H
#define __STDDEF_H

#ifndef NULL
#ifdef __cplusplus
#define NULL 0
#else
#define NULL ((void*)0)
#endif
#endif
#ifndef offsetof
#define offsetof(ty,mem) __builtin_offsetof(ty, mem)
#endif

typedef long ptrdiff_t;
#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned long size_t;
#endif
#if !defined(_WCHAR_T) && !defined(__cplusplus)	/* C++'s is a keyword */
#define _WCHAR_T
#ifdef __WCHAR_TYPE__
typedef __WCHAR_TYPE__ wchar_t;	/* a compiler that says: L"..." is of its type */
#else
typedef unsigned short wchar_t;
#endif
#endif

#if __STDC_VERSION__ >= 201112L || __cplusplus >= 201103L
typedef struct { long long __ll; long double __ld; } max_align_t;
#endif

#endif /* __STDDEF_H */
