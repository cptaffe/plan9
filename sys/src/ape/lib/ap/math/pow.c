#include <math.h>
#include <errno.h>
#include <limits.h>

double
pow(double x, double y) /* return x ^ y (exponentiation) */
{
	double xy, y1, ye;
	long i;
	int ex, ey, flip;

	int yint, yodd;

	/* C99 Annex F's cases, which have no number to compute */
	if(y == 0 || x == 1)
		return 1;
	if(isNaN(x) || isNaN(y))
		return NaN();
	yint = isInf(y, 0) || y == floor(y);
	yodd = yint && !isInf(y, 0) && fabs(y) < 0x1p53 && fmod(fabs(y), 2) == 1;
	if(x == 0){
		if(y < 0){
			errno = ERANGE;
			return yodd? copysign(HUGE_VAL, x): HUGE_VAL;
		}
		return yodd? x: 0;
	}
	if(isInf(y, 0)){
		if(x == -1)
			return 1;
		if((fabs(x) < 1) == (y > 0))
			return 0;
		return HUGE_VAL;
	}
	if(isInf(x, 0)){
		if(x > 0)
			return y < 0? 0: HUGE_VAL;
		if(y < 0)
			return yodd? -0.0: 0;
		return yodd? -HUGE_VAL: HUGE_VAL;
	}
	if(x < 0){
		if(!yint){
			errno = EDOM;
			return NaN();
		}
		xy = pow(-x, y);
		return yodd? -xy: xy;
	}

	flip = 0;
	if(y < 0.){
		y = -y;
		flip = 1;
	}
	y1 = modf(y, &ye);
	if(y1 != 0.0){
		if(x <= 0.)
			goto zreturn;
		if(y1 > 0.5) {
			y1 -= 1.;
			ye += 1.;
		}
		xy = exp(y1 * log(x));
	}else
		xy = 1.0;
	if(ye > LONG_MAX){
		if(x <= 0.){
 zreturn:
			if(x || flip)
				errno = EDOM;
			return 0.;
		}
		if(flip){
			if(y == .5)
				return 1./sqrt(x);
			y = -y;
		}else if(y == .5)
			return sqrt(x);
		return exp(y * log(x));
	}
	x = frexp(x, &ex);
	ey = 0;
	i = ye;
	if(i)
		for(;;){
			if(i & 1){
				xy *= x;
				ey += ex;
			}
			i >>= 1;
			if(i == 0)
				break;
			x *= x;
			ex <<= 1;
			if(x < .5){
				x += x;
				ex -= 1;
			}
		}
	if(flip){
		xy = 1. / xy;
		ey = -ey;
	}
	return ldexp(xy, ey);	/* ERANGE, if it is out of range */
}
