#include <time.h>
#include "sys9.h"

/*
 * C11: the time, in nanoseconds, from /dev/bintime, as libc's nsec
 */
int
timespec_get(struct timespec *ts, int base)
{
	unsigned char b[8];
	unsigned long long ns;
	int i, f, n;

	if(base != TIME_UTC)
		return 0;
	if((f = _OPEN("/dev/bintime", OREAD|OCEXEC)) < 0)
		return 0;
	n = _PREAD(f, b, sizeof b, 0);
	_CLOSE(f);
	if(n != sizeof b)
		return 0;
	ns = 0;
	for(i = 0; i < 8; i++)
		ns = ns<<8 | b[i];
	ts->tv_sec = ns / 1000000000;
	ts->tv_nsec = ns % 1000000000;
	return base;
}
