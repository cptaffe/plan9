#include <math.h>
#include <errno.h>

/*
 * Near zero, log(1+x) and exp(x)-1 lose x's digits; these
 * correct them by the error of 1+x (Goldberg's, and Kahan's,
 * methods: the factor x/(u-1) is near 1, and exact if u is).
 */
double
log1p(double x)
{
	double u;

	if(isnan(x))
		return x;
	if(x == -1){
		errno = ERANGE;
		return -HUGE_VAL;
	}
	if(x < -1){
		errno = EDOM;
		return NaN();
	}
	if(isinf(x))
		return x;
	u = 1 + x;
	if(u == 1)
		return x;
	return log(u) * (x/(u - 1));
}

double
expm1(double x)
{
	double u, l;

	if(isnan(x))
		return x;
	if(x == 0 || fabs(x) < 0x1p-54)
		return x;
	if(x > 709.8){
		errno = ERANGE;
		return HUGE_VAL;
	}
	if(x < -40)
		return -1;
	u = exp(x);
	if(u == 1)
		return x;
	l = log(u);
	if(fabs(x) > 0.5)
		return u - 1;
	return (u - 1) * (x/l);
}
