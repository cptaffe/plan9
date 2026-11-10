#include <math.h>

/*
 * A first guess from the exponent, then Newton's method,
 * whose last step is checked against its neighbours.
 */
double
cbrt(double x)
{
	double y, m, r, best, e, eb;
	int i, n;

	if(x == 0 || !isfinite(x))
		return x;
	m = frexp(fabs(x), &n);
	while(n % 3 != 0){
		m /= 2;	/* exact */
		n++;
	}
	y = 0.5 + 0.5*m;	/* m in [1/8, 1) */
	for(i = 0; i < 8; i++)
		y = y - (y*y*y - m)/(3*y*y);
	/* the y of the three whose cube is nearest m */
	best = y;
	eb = fabs(fma(y*y, y, -m));
	for(i = -1; i <= 1; i += 2){
		r = nextafter(y, i*2.0);
		e = fabs(fma(r*r, r, -m));
		if(e < eb)
			best = r, eb = e;
	}
	r = ldexp(best, n/3);
	return x < 0? -r: r;
}
