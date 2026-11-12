#ifndef __STRINGS_H_
#define __STRINGS_H_
#pragma lib "/$M/lib/ape/libbsd.a"

/* POSIX's <strings.h>: libbsd's functions, by their POSIX types */

#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned long size_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern int	ffs(int);
extern int	strcasecmp(const char *, const char *);
extern int	strncasecmp(const char *, const char *, size_t);

#ifdef __cplusplus
}
#endif

#endif
