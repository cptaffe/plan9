#include <math.h>

/*
 * C99: a NaN is no argument, if the other is a number
 */
double
fdim(double x, double y)
{
	if(isnan(x) || isnan(y))
		return x + y;
	return x > y? x - y: 0;
}

double
fmax(double x, double y)
{
	if(isnan(x))
		return y;
	if(isnan(y))
		return x;
	if(x == y)
		return signbit(x)? y: x;	/* +0 > -0 */
	return x > y? x: y;
}

double
fmin(double x, double y)
{
	if(isnan(x))
		return y;
	if(isnan(y))
		return x;
	if(x == y)
		return signbit(x)? x: y;
	return x < y? x: y;
}
