#include <math.h>
#include <limits.h>
#include <errno.h>

int
ilogb(double x)
{
	int e;

	if(x == 0 || isnan(x)){
		errno = EDOM;
		return x == 0? FP_ILOGB0: FP_ILOGBNAN;
	}
	if(isinf(x)){
		errno = EDOM;
		return INT_MAX;
	}
	frexp(x, &e);
	return e - 1;
}

double
logb(double x)
{
	if(x == 0){
		errno = ERANGE;
		return -HUGE_VAL;
	}
	if(isnan(x))
		return x;
	if(isinf(x))
		return HUGE_VAL;
	return ilogb(x);
}

/* FLT_RADIX is 2 */
double
scalbn(double x, int n)
{
	return ldexp(x, n);
}

double
scalbln(double x, long n)
{
	if(n > INT_MAX)
		n = INT_MAX;
	if(n < INT_MIN)
		n = INT_MIN;
	return ldexp(x, n);
}
