#include <math.h>
#include <complex.h>

/*
 * The compiler's calls for the product and quotient of complex
 * numbers (clang's, and gcc's): C99 Annex G's, which find the
 * infinities that the plain formulas make NaNs of.
 */
static double
box(double x)	/* x's sign, of 1 or 0: infinity is 1 */
{
	return copysign(isinf(x)? 1: 0, x);
}

double complex
__muldc3(double a, double b, double c, double d)
{
	double ac, bd, ad, bc, x, y;
	int again;

	ac = a*c;
	bd = b*d;
	ad = a*d;
	bc = b*c;
	x = ac - bd;
	y = ad + bc;
	if(isnan(x) && isnan(y)){
		again = 0;
		if(isinf(a) || isinf(b)){
			a = box(a);
			b = box(b);
			if(isnan(c))
				c = copysign(0, c);
			if(isnan(d))
				d = copysign(0, d);
			again = 1;
		}
		if(isinf(c) || isinf(d)){
			c = box(c);
			d = box(d);
			if(isnan(a))
				a = copysign(0, a);
			if(isnan(b))
				b = copysign(0, b);
			again = 1;
		}
		if(!again && (isinf(ac) || isinf(bd) || isinf(ad) || isinf(bc))){
			if(isnan(a))
				a = copysign(0, a);
			if(isnan(b))
				b = copysign(0, b);
			if(isnan(c))
				c = copysign(0, c);
			if(isnan(d))
				d = copysign(0, d);
			again = 1;
		}
		if(again){
			x = INFINITY*(a*c - b*d);
			y = INFINITY*(a*d + b*c);
		}
	}
	return CMPLX(x, y);
}

double complex
__divdc3(double a, double b, double c, double d)
{
	double l, den, x, y;
	int n;

	n = 0;
	l = logb(fmax(fabs(c), fabs(d)));
	if(isfinite(l)){
		n = l;
		c = scalbn(c, -n);
		d = scalbn(d, -n);
	}
	den = c*c + d*d;
	x = scalbn((a*c + b*d)/den, -n);
	y = scalbn((b*c - a*d)/den, -n);
	if(isnan(x) && isnan(y)){
		if(den == 0 && (!isnan(a) || !isnan(b))){
			x = copysign(INFINITY, c)*a;
			y = copysign(INFINITY, c)*b;
		}else if((isinf(a) || isinf(b)) && isfinite(c) && isfinite(d)){
			a = box(a);
			b = box(b);
			x = INFINITY*(a*c + b*d);
			y = INFINITY*(b*c - a*d);
		}else if(isinf(l) && l > 0 && isfinite(a) && isfinite(b)){
			c = box(c);
			d = box(d);
			x = 0*(a*c + b*d);
			y = 0*(b*c - a*d);
		}
	}
	return CMPLX(x, y);
}

float complex
__mulsc3(float a, float b, float c, float d)
{
	return __muldc3(a, b, c, d);
}

float complex
__divsc3(float a, float b, float c, float d)
{
	return __divdc3(a, b, c, d);
}
