#ifndef __SELECT_H
#define __SELECT_H
#ifndef _BSD_EXTENSION
    This header file is an extension to ANSI/POSIX
#endif
#pragma lib "/$M/lib/ape/libap.a"

#ifndef _FD_SET_T
#define _FD_SET_T
/* BSD select, and adjunct types and macros */

/* assume 96 fds is sufficient for fdset size */

typedef struct fd_set {
	long fds_bits[3];
} fd_set;
#define FD_SETSIZE	96

#define FD_SET(n,p)	((p)->fds_bits[(n)>>5] |= (1 << ((n) &0x1f)))
#define FD_CLR(n,p)	((p)->fds_bits[(n)>>5] &= ~(1 << ((n) &0x1f)))
#define FD_ISSET(n,p)	((p)->fds_bits[(n)>>5] & (1 << ((n) &0x1f)))
#define FD_ZERO(p)	((p)->fds_bits[0] =0, (p)->fds_bits[1] =0, (p)->fds_bits[2] =0)
#endif

#ifndef __TIMEVAL__
#define __TIMEVAL__
#ifndef _TIME_T
#define _TIME_T
typedef long long time_t;
#endif
struct timeval {
	time_t	tv_sec;	/* p9web: POSIX's, 64 bits */
	long	tv_usec;
};
#ifdef _BSD_EXTENSION
struct timezone {
	int	tz_minuteswest;
	int	tz_dsttime;
};
#endif
#endif /* __TIMEVAL__ */

/* POSIX: as <time.h>'s, for pselect */
#ifndef _TIMESPEC
#define _TIMESPEC
struct timespec {
	time_t	tv_sec;
	long	tv_nsec;
};
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern int select(int, fd_set*, fd_set*, fd_set*, struct timeval *);

#ifdef __cplusplus
}
#endif

#endif
