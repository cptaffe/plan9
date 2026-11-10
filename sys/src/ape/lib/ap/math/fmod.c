/* floating-point mod function; C99's infinities and NaNs first */
#include <math.h>
#include <errno.h>
double
fmod (double x, double y)
{
	int sign = 0, yexp;
	double r, yfr;

	if (isNaN(x) || isNaN(y))
		return NaN();
	if (isInf(x, 0) || y == 0) {
		errno = EDOM;
		return NaN();
	}
	if (isInf(y, 0))
		return x;
	if (y < 0)
		y = -y;
	yfr = frexp (y, &yexp);
	if (x < 0) {
		sign = 1;
		r = -x;
	} else
		r = x;
	while (r >= y) {
		int rexp;
		double rfr = frexp (r, &rexp);
		r -= ldexp (y, rexp - yexp - (rfr < yfr));
	}
	if (sign)
		r = -r;
	return r;
}
