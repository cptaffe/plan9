#include <math.h>
#include <float.h>
#include <errno.h>

/*
 * The next double from x toward y: the next integer of x's
 * bits, as an IEEE double's are ordered as integers by magnitude.
 */
double
nextafter(double x, double y)
{
	union { double d; unsigned long long u; } a;

	if(isnan(x) || isnan(y))
		return x + y;
	if(x == y)
		return y;
	if(x == 0){
		a.u = 1;
		return y > 0? a.d: -a.d;
	}
	a.d = x;
	if((x < y) == (x > 0))
		a.u++;
	else
		a.u--;
	if(isinf(a.d) || fpclassify(a.d) == FP_SUBNORMAL || a.d == 0)
		errno = ERANGE;
	return a.d;
}

/* long double is double */
double
nexttoward(double x, long double y)
{
	return nextafter(x, y);
}
