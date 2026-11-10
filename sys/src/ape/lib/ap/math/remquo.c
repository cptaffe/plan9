#include <math.h>
#include <float.h>
#include <errno.h>

/*
 * x - n*y, n the nearest integer to x/y (the even, halfway),
 * and in *quo, n's sign and its low three bits.  fmod is exact,
 * and so is each subtraction, as the values are near each other.
 */
double
remquo(double x, double y, int *quo)
{
	double ax, ay;
	int q, neg, big;

	*quo = 0;
	if(isnan(x) || isnan(y))
		return x + y;
	if(isinf(x) || y == 0){
		errno = EDOM;
		return NaN();
	}
	if(isinf(y))
		return x;
	neg = (x < 0) != (y < 0);
	ax = fabs(x);
	ay = fabs(y);
	q = 0;
	big = 0;
	if(ay > DBL_MAX/8){
		/* scaled down, exactly: x is near y, or less than half of it */
		if(ax < ay/2)
			return x;
		ax = ldexp(ax, -4);
		ay = ldexp(ay, -4);
		big = 1;
	}
	ax = fmod(ax, 8*ay);
	if(ax >= 4*ay){
		ax -= 4*ay;
		q += 4;
	}
	if(ax >= 2*ay){
		ax -= 2*ay;
		q += 2;
	}
	if(ax >= ay){
		ax -= ay;
		q++;
	}
	if(2*ax > ay || (2*ax == ay && (q & 1))){
		ax -= ay;
		q++;
	}
	if(big)
		ax = ldexp(ax, 4);
	q &= 7;
	*quo = neg? -q: q;
	return signbit(x)? -ax: ax;
}

double
remainder(double x, double y)
{
	int q;

	return remquo(x, y, &q);
}
