#include "lib.h"
#include <time.h>
#include <errno.h>
#include "sys9.h"

/*
 * sleep(2) is in milliseconds: the time is rounded up to them.
 * A note ends it early, and rem is what was left.
 */
int
nanosleep(const struct timespec *req, struct timespec *rem)
{
	struct timespec t0, t1;
	long long ns, ms;

	if(req->tv_nsec < 0 || req->tv_nsec >= 1000000000 || req->tv_sec < 0){
		errno = EINVAL;
		return -1;
	}
	ns = req->tv_sec*1000000000LL + req->tv_nsec;
	ms = (ns + 999999) / 1000000;
	if(ms > 0x7fffffff)
		ms = 0x7fffffff;
	timespec_get(&t0, TIME_UTC);
	if(_SLEEP(ms) < 0){
		if(rem){
			timespec_get(&t1, TIME_UTC);
			ns -= (t1.tv_sec - t0.tv_sec)*1000000000LL + t1.tv_nsec - t0.tv_nsec;
			if(ns < 0)
				ns = 0;
			rem->tv_sec = ns / 1000000000;
			rem->tv_nsec = ns % 1000000000;
		}
		errno = EINTR;
		return -1;
	}
	return 0;
}
