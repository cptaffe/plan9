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

#define	ITIMER_REAL	0
#define	ITIMER_VIRTUAL	1
#define	ITIMER_PROF	2

struct itimerval {
	struct timeval	it_interval;
	struct timeval	it_value;
};

extern int getitimer(int, struct itimerval *);
extern int setitimer(int, const struct itimerval *, struct itimerval *);

#endif /* __SYSTIME_H */
