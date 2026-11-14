#include <stdlib.h>
#include <errno.h>
#include <limits.h>

long long
strtonum(const char *s, long long min, long long max, const char **err)
{
	long long v;
	char *e;
	int en;

	v = 0;
	en = errno;
	errno = 0;
	if(min > max)
		goto invalid;
	v = strtoll(s, &e, 10);
	if(s == e || *e != 0)
		goto invalid;
	if((v == LLONG_MIN && errno == ERANGE) || v < min){
		if(err)
			*err = "too small";
		errno = ERANGE;
		return 0;
	}
	if((v == LLONG_MAX && errno == ERANGE) || v > max){
		if(err)
			*err = "too large";
		errno = ERANGE;
		return 0;
	}
	if(err)
		*err = NULL;
	errno = en;
	return v;
invalid:
	if(err)
		*err = "invalid";
	errno = EINVAL;
	return 0;
}
