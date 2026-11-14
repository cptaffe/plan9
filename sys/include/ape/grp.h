#ifndef __GRP
#define __GRP
#ifndef _POSIX_SOURCE
   This header file is not defined in pure ANSI
#endif
#pragma lib "/$M/lib/ape/libap.a"
#include <sys/types.h>

struct	group {
	char	*gr_name;
	char	*gr_passwd;	/* XSI */
	gid_t	gr_gid;
	char	**gr_mem;
};

#ifdef __cplusplus
extern "C" {
#endif

extern struct group *getgrgid(gid_t);
#ifdef _BSD_EXTENSION
extern char *group_from_gid(gid_t, int);
extern int gid_from_group(const char *, gid_t *);
#endif
extern struct group *getgrnam(const char *);
#ifdef _BSD_EXTENSION
extern struct group *getgrent(void);
extern void setgrent(void);
extern void endgrent(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
