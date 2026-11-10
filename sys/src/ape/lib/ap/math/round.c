#include <math.h>
#include <limits.h>
#include <errno.h>

/*
 * to nearest, halfway away from zero
 */
double
round(double x)
{
	double t;

	if(!isfinite(x))
		return x;
	t = trunc(x);
	if(fabs(x - t) >= 0.5)	/* exact: x and t are close */
		t += copysign(1, x);
	return t;
}

long
lround(double x)
{
	x = round(x);
	if(!(x >= LONG_MIN && x < -(double)LONG_MIN)){
		errno = EDOM;
		return LONG_MIN;
	}
	return x;
}

long long
llround(double x)
{
	x = round(x);
	if(!(x >= LLONG_MIN && x < -(double)LLONG_MIN)){
		errno = EDOM;
		return LLONG_MIN;
	}
	return x;
}
