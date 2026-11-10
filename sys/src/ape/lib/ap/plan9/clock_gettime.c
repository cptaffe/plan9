#include <time.h>
#include <errno.h>

/*
 * POSIX's clocks: Plan 9 has one, /dev/bintime's nanoseconds,
 * which serves for both
 */
int
clock_gettime(clockid_t id, struct timespec *ts)
{
	if(id != CLOCK_REALTIME && id != CLOCK_MONOTONIC){
		errno = EINVAL;
		return -1;
	}
	if(timespec_get(ts, TIME_UTC) == 0){
		errno = EIO;
		return -1;
	}
	return 0;
}

int
clock_getres(clockid_t id, struct timespec *ts)
{
	if(id != CLOCK_REALTIME && id != CLOCK_MONOTONIC){
		errno = EINVAL;
		return -1;
	}
	if(ts){
		ts->tv_sec = 0;
		ts->tv_nsec = 1;
	}
	return 0;
}
