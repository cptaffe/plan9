#include <time.h>
#include "sys9.h"

/*
 * C11: the time, in nanoseconds, from /dev/bintime, as libc's nsec
 */
int
timespec_get(struct timespec *ts, int base)
{
	static int f = -1;
	unsigned char b[8];
	unsigned long long ns;
	int i;

	if(base != TIME_UTC)
		return 0;
	/* the file stays open, as nsec's does; it is opened again if it was lost */
	if(f < 0 || _PREAD(f, b, sizeof b, 0) != sizeof b){
		if(f >= 0)
			_CLOSE(f);
		f = _OPEN("/dev/bintime", OREAD|OCEXEC);
		if(f < 0 || _PREAD(f, b, sizeof b, 0) != sizeof b)
			return 0;
	}
	ns = 0;
	for(i = 0; i < 8; i++)
		ns = ns<<8 | b[i];
	ts->tv_sec = ns / 1000000000;
	ts->tv_nsec = ns % 1000000000;
	return base;
}
