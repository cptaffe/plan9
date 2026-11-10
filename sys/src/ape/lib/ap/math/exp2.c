#include <math.h>
#include <errno.h>

#define	LN2	0.693147180559945309417232121458176568

/*
 * 2^x: 2^i, exactly, times e^(f ln 2), f the rest, |f| <= 1/2
 */
double
exp2(double x)
{
	double i, f;

	if(isnan(x))
		return x;
	if(x >= 1024){
		errno = ERANGE;
		return HUGE_VAL;
	}
	if(x < -1075)
		return 0;
	i = floor(x + 0.5);
	f = x - i;
	return ldexp(exp(f*LN2), (int)i);
}

/*
 * log2(x): e + log(m)/ln 2, x = m * 2^e; a power of 2 is exact
 */
double
log2(double x)
{
	double m;
	int e;

	if(isnan(x) || x == HUGE_VAL)
		return x;
	if(x == 0){
		errno = ERANGE;
		return -HUGE_VAL;
	}
	if(x < 0){
		errno = EDOM;
		return NaN();
	}
	m = frexp(x, &e);
	if(m == 0.5)
		return e - 1;
	if(m < 0.70710678118654752440){
		m *= 2;
		e--;
	}
	return e + log1p(m - 1)/LN2;
}
