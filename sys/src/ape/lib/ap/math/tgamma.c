#include <math.h>
#include <errno.h>

extern int signgam;
extern double gamma(double);	/* log|Γ(x)|: gamma.c */

/*
 * lgamma is gamma(3), which is log|Γ(x)|, and sets signgam
 */
double
lgamma(double x)
{
	if(isnan(x))
		return x;
	if(isinf(x))
		return HUGE_VAL;
	if(x <= 0 && x == floor(x)){
		signgam = x == 0 && signbit(x)? -1: 1;
		errno = ERANGE;
		return HUGE_VAL;
	}
	if(x == 1 || x == 2){
		signgam = 1;
		return 0;
	}
	return gamma(x);
}

double
lgamma_r(double x, int *sign)
{
	int s;
	double r;

	s = signgam;
	r = lgamma(x);
	*sign = signgam;
	signgam = s;
	return r;
}

/*
 * Γ(x): a product, for an integer it is exact for; else from lgamma
 */
double
tgamma(double x)
{
	double r, l;
	int i;

	if(isnan(x) || x == HUGE_VAL)
		return x;
	if(x == 0){
		errno = ERANGE;
		return signbit(x)? -HUGE_VAL: HUGE_VAL;
	}
	if(x < 0 && x == floor(x) || x == -HUGE_VAL){
		errno = EDOM;
		return NaN();
	}
	if(x == floor(x) && x <= 23){
		r = 1;
		for(i = 2; i < x; i++)
			r *= i;
		return r;
	}
	if(x > 171.7){
		errno = ERANGE;
		return HUGE_VAL;
	}
	l = lgamma(x);
	r = exp(l);
	return signgam < 0? -r: r;
}
