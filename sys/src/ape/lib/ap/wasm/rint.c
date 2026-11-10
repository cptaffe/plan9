#include <math.h>
#include <limits.h>
#include <errno.h>

/*
 * wasm has the instructions, and rounds to nearest, always
 */
double
rint(double x)
{
	return __builtin_rint(x);
}

double
nearbyint(double x)
{
	return __builtin_rint(x);
}

double
trunc(double x)
{
	return __builtin_trunc(x);
}

long
lrint(double x)
{
	x = rint(x);
	if(!(x >= LONG_MIN && x < -(double)LONG_MIN)){
		errno = EDOM;
		return LONG_MIN;
	}
	return x;
}

long long
llrint(double x)
{
	x = rint(x);
	if(!(x >= LLONG_MIN && x < -(double)LLONG_MIN)){
		errno = EDOM;
		return LLONG_MIN;
	}
	return x;
}
