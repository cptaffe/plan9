#ifndef __FNMATCH_H
#define __FNMATCH_H
#pragma lib "/$M/lib/ape/libap.a"

#define	FNM_NOMATCH	1
#define	FNM_NOSYS	2

#define	FNM_NOESCAPE	0x01
#define	FNM_PATHNAME	0x02
#define	FNM_PERIOD	0x04
#if _POSIX_C_SOURCE >= 202405L || defined(_BSD_EXTENSION)
#define	FNM_CASEFOLD	0x10
#define	FNM_IGNORECASE	FNM_CASEFOLD
#endif
#ifdef _BSD_EXTENSION
#define	FNM_LEADING_DIR	0x08
#define	FNM_FILE_NAME	FNM_PATHNAME
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern int fnmatch(const char *, const char *, int);

#ifdef __cplusplus
}
#endif

#endif
