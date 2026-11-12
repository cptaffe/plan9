#ifndef __SYSTIME_H
#define __SYSTIME_H
#pragma lib "/$M/lib/ape/libap.a"

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

extern int gettimeofday(struct timeval *, struct timezone *);

#endif /* __SYSTIME_H */
