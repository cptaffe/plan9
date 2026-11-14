#include "lib.h"
#include <stddef.h>
#include <sys/time.h>
#include <errno.h>
#include "sys9.h"

static long
ms(const struct timeval *t)
{
	long m;

	m = t->tv_sec*1000 + (t->tv_usec+999)/1000;
	if(m == 0 && (t->tv_sec || t->tv_usec))
		m = 1;
	return m;
}

static int
bad(const struct timeval *t)
{
	return t->tv_sec < 0 || t->tv_usec < 0 || t->tv_usec >= 1000000;
}

static void
tv(struct timeval *t, long m)
{
	t->tv_sec = m/1000;
	t->tv_usec = (m%1000)*1000;
}

int
setitimer(int which, const struct itimerval *v, struct itimerval *o)
{
	long left;

	if(which != ITIMER_REAL || bad(&v->it_value) || bad(&v->it_interval)){
		errno = EINVAL;
		return -1;
	}
	left = _ALARM(ms(&v->it_value));
	if(o != NULL){
		tv(&o->it_value, left);
		tv(&o->it_interval, _itimerms);
	}
	_itimerms = ms(&v->it_value) ? ms(&v->it_interval) : 0;
	return 0;
}

int
getitimer(int which, struct itimerval *o)
{
	long left;

	if(which != ITIMER_REAL){
		errno = EINVAL;
		return -1;
	}
	left = _ALARM(0);
	if(left > 0)
		_ALARM(left);
	tv(&o->it_value, left);
	tv(&o->it_interval, _itimerms);
	return 0;
}
