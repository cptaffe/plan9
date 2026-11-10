#ifndef __RESOURCE_H__
#define __RESOURCE_H__

#if !defined(_BSD_EXTENSION) && !(_POSIX_C_SOURCE >= 200112L)
    This header file is an extension to ANSI/POSIX
#endif
#pragma lib "/$M/lib/ape/libap.a"

#include <sys/time.h>	/* struct timeval */

/*
 * POSIX's limits.  Plan 9 has none: every one is RLIM_INFINITY, and
 * setrlimit takes only that, and for RLIMIT_CORE anything (Plan 9
 * writes no core files: a broken process stays, for the debugger).
 */
typedef unsigned long long rlim_t;
#define RLIM_INFINITY	(~(rlim_t)0)
#define RLIM_SAVED_MAX	RLIM_INFINITY
#define RLIM_SAVED_CUR	RLIM_INFINITY

#define RLIMIT_CORE	0
#define RLIMIT_CPU	1
#define RLIMIT_DATA	2
#define RLIMIT_FSIZE	3
#define RLIMIT_NOFILE	4
#define RLIMIT_STACK	5
#define RLIMIT_AS	6
#define RLIM_NLIMITS	7

struct rlimit {
	rlim_t	rlim_cur;
	rlim_t	rlim_max;
};

#define RUSAGE_SELF	0
#define RUSAGE_CHILDREN	(-1)

/* getrusage fills in the times, /dev/cputime's; Plan 9 counts no others */
struct rusage {
	struct timeval ru_utime;	/* user time used */
	struct timeval ru_stime;	/* system time used */
	long	ru_maxrss;		/* max resident set size */
#define	ru_first	ru_ixrss
	long	ru_ixrss;		/* integral shared memory size */
	long	ru_idrss;		/* integral unshared data " */
	long	ru_isrss;		/* integral unshared stack " */
	long	ru_minflt;		/* page reclaims */
	long	ru_majflt;		/* page faults */
	long	ru_nswap;		/* swaps */
	long	ru_inblock;		/* block input operations */
	long	ru_oublock;		/* block output operations */
	long	ru_msgsnd;		/* messages sent */
	long	ru_msgrcv;		/* messages received */
	long	ru_nsignals;		/* signals received */
	long	ru_nvcsw;		/* voluntary context switches */
	long	ru_nivcsw;		/* involuntary " */
#define	ru_last		ru_nivcsw
};

#ifdef __cplusplus
extern "C" {
#endif

extern int getrlimit(int, struct rlimit *);
extern int setrlimit(int, const struct rlimit *);
extern int getrusage(int, struct rusage *);

#ifdef __cplusplus
}
#endif

#endif /* !__RESOURCE_H__ */
