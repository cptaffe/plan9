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
#define offsetof(ty,mem) ((size_t) &(((ty *)0)->mem))
#endif

typedef long ptrdiff_t;
#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned long size_t;
#endif
#ifndef _WCHAR_T
#define _WCHAR_T
#ifdef __WCHAR_TYPE__
typedef __WCHAR_TYPE__ wchar_t;	/* a compiler that says: L"..." is of its type */
#else
typedef unsigned short wchar_t;
#endif
#endif

#endif /* __STDDEF_H */
