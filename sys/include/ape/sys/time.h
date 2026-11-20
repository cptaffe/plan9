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

#ifdef __cplusplus
extern "C" {
#endif
extern int gettimeofday(struct timeval *, struct timezone *);

#ifdef _BSD_EXTENSION
#define	timerclear(t)	((t)->tv_sec = (t)->tv_usec = 0)
#define	timerisset(t)	((t)->tv_sec || (t)->tv_usec)
#define	timercmp(a, b, op)	((a)->tv_sec == (b)->tv_sec ? \
	(a)->tv_usec op (b)->tv_usec : (a)->tv_sec op (b)->tv_sec)
#define	timeradd(a, b, r)	do { \
	(r)->tv_sec = (a)->tv_sec + (b)->tv_sec; \
	(r)->tv_usec = (a)->tv_usec + (b)->tv_usec; \
	if((r)->tv_usec >= 1000000){ (r)->tv_sec++; (r)->tv_usec -= 1000000; } \
} while(0)
#define	timersub(a, b, r)	do { \
	(r)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
	(r)->tv_usec = (a)->tv_usec - (b)->tv_usec; \
	if((r)->tv_usec < 0){ (r)->tv_sec--; (r)->tv_usec += 1000000; } \
} while(0)
#endif

#define	ITIMER_REAL	0
#define	ITIMER_VIRTUAL	1
#define	ITIMER_PROF	2

struct itimerval {
	struct timeval	it_interval;
	struct timeval	it_value;
};

extern int getitimer(int, struct itimerval *);
extern int setitimer(int, const struct itimerval *, struct itimerval *);
extern int utimes(const char *, const struct timeval [2]);

#ifdef __cplusplus
}
#endif
#endif /* __SYSTIME_H */
