#include <math.h>
#include <errno.h>

#define	LN2	0.693147180559945309417232121458176568

double
acosh(double x)
{
	double t;

	if(isnan(x))
		return x;
	if(x < 1){
		errno = EDOM;
		return NaN();
	}
	if(x > 0x1p28)
		return log(x) + LN2;
	t = x - 1;
	return log1p(t + sqrt(2*t + t*t));
}

double
asinh(double x)
{
	double a, r;

	if(!isfinite(x) || x == 0)
		return x;
	a = fabs(x);
	if(a > 0x1p28)
		r = log(a) + LN2;
	else
		r = log1p(a + a*a/(1 + sqrt(1 + a*a)));
	return x < 0? -r: r;
}

double
atanh(double x)
{
	double a, r;

	if(isnan(x) || x == 0)
		return x;
	a = fabs(x);
	if(a > 1){
		errno = EDOM;
		return NaN();
	}
	if(a == 1){
		errno = ERANGE;
		return x < 0? -HUGE_VAL: HUGE_VAL;
	}
	r = 0.5*log1p(2*a/(1 - a));
	return x < 0? -r: r;
}
