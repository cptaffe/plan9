#include <math.h>
#include <errno.h>
/*
	atan2 discovers what quadrant the angle
	is in and calls atan.
*/
#define pio2 1.5707963267948966192313217
#define pi   3.1415926535897932384626434

double
atan2(double arg1, double arg2)
{

	/* C99: the signs of zeros and infinities say which way */
	if(isNaN(arg1) || isNaN(arg2))
		return arg1 + arg2;
	if(arg1 == 0){
		if(signbit(arg2))
			return copysign(pi, arg1);
		return arg1;
	}
	if(isInf(arg1, 0) && isInf(arg2, 0)){
		if(arg2 > 0)
			return copysign(pio2/2, arg1);
		return copysign(3*pio2/2, arg1);
	}
	if(isInf(arg2, 0)){
		if(arg2 > 0)
			return copysign(0, arg1);
		return copysign(pi, arg1);
	}
	if(arg1+arg2 == arg1) {
		if(arg1 >= 0)
			return pio2;
		return -pio2;
	}
	if(arg2 < 0){	/* the half-turn the quotient lost, of y's sign */
		if(arg1 > 0)
			return atan(arg1/arg2) + pi;
		return atan(arg1/arg2) - pi;
	}
	return atan(arg1/arg2);
}
