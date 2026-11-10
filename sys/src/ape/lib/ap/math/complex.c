#include <math.h>
#include <complex.h>
#include <float.h>

/*
 * C99's complex functions, from the real ones, as the identities
 * have them; the parts are made one at a time, so no complex
 * product or quotient (wasm/cmul.c) is needed here.
 */
#define	LN2	0.693147180559945309417232121458176568
#define	PI_2	1.57079632679489661923132169163975144

static double complex
cx(double re, double im)
{
	return CMPLX(re, im);
}

double
creal(double complex z)
{
	return __real__ z;
}

double
cimag(double complex z)
{
	return __imag__ z;
}

double complex
conj(double complex z)
{
	return cx(creal(z), -cimag(z));
}

double complex
cproj(double complex z)
{
	if(isinf(creal(z)) || isinf(cimag(z)))
		return cx(INFINITY, copysign(0, cimag(z)));
	return z;
}

double
cabs(double complex z)
{
	return hypot(creal(z), cimag(z));
}

double
carg(double complex z)
{
	return atan2(cimag(z), creal(z));
}

double complex
cexp(double complex z)
{
	double x, y, e;

	x = creal(z);
	y = cimag(z);
	if(y == 0)
		return cx(exp(x), y);
	if(isinf(x) && x < 0 && !isfinite(y))
		return cx(0, 0);
	e = exp(x);
	return cx(e*cos(y), e*sin(y));
}

double complex
clog(double complex z)
{
	return cx(log(cabs(z)), carg(z));
}

double complex
csqrt(double complex z)
{
	double x, y, t, s;

	x = creal(z);
	y = cimag(z);
	if(x == 0 && y == 0)
		return cx(0, y);
	if(isinf(y))
		return cx(INFINITY, y);
	if(isnan(x))
		return cx(x, x);
	if(isinf(x)){
		if(x > 0)
			return cx(x, isnan(y)? y: copysign(0, y));
		return cx(isnan(y)? y: 0, copysign(INFINITY, y));
	}
	s = 1;
	if(fabs(x) > DBL_MAX/4 || fabs(y) > DBL_MAX/4){
		x /= 4;	/* exact */
		y /= 4;
		s = 2;
	}
	t = sqrt((fabs(x) + hypot(x, y))/2);
	if(x >= 0)
		return cx(s*t, s*y/(2*t));
	return cx(s*fabs(y)/(2*t), s*copysign(t, y));
}

double complex
cpow(double complex z, double complex w)
{
	double complex l;
	double a, b;

	if(creal(z) == 0 && cimag(z) == 0){
		if(creal(w) > 0 && cimag(w) == 0)
			return cx(0, 0);
	}
	l = clog(z);
	a = creal(w)*creal(l) - cimag(w)*cimag(l);
	b = creal(w)*cimag(l) + cimag(w)*creal(l);
	return cexp(cx(a, b));
}

/* sin z = -i sinh iz */
double complex
csin(double complex z)
{
	double complex t;

	t = csinh(cx(-cimag(z), creal(z)));
	return cx(cimag(t), -creal(t));
}

/* cos z = cosh iz */
double complex
ccos(double complex z)
{
	return ccosh(cx(-cimag(z), creal(z)));
}

double complex
ctan(double complex z)
{
	double x, y, d;

	x = creal(z);
	y = cimag(z);
	if(y == 0)
		return cx(tan(x), y);
	if(fabs(y) > 20)	/* cosh(2y) dwarfs the rest */
		return cx(4*sin(x)*cos(x)*exp(-2*fabs(y)), copysign(1, y));
	d = cos(2*x) + cosh(2*y);
	return cx(sin(2*x)/d, sinh(2*y)/d);
}

double complex
csinh(double complex z)
{
	double x, y;

	x = creal(z);
	y = cimag(z);
	if(y == 0)
		return cx(sinh(x), y);
	return cx(sinh(x)*cos(y), cosh(x)*sin(y));
}

double complex
ccosh(double complex z)
{
	double x, y;

	x = creal(z);
	y = cimag(z);
	if(y == 0)
		return cx(cosh(x), copysign(0, x)*y);
	return cx(cosh(x)*cos(y), sinh(x)*sin(y));
}

/* tanh z = -i tan iz */
double complex
ctanh(double complex z)
{
	double complex t;

	t = ctan(cx(-cimag(z), creal(z)));
	return cx(cimag(t), -creal(t));
}

/* asinh z = log(z + sqrt(z²+1)), for Re z >= 0; odd */
double complex
casinh(double complex z)
{
	double x, y, a, b;
	double complex s, r;

	x = creal(z);
	y = cimag(z);
	if(signbit(x)){
		r = casinh(cx(-x, -y));
		return cx(-creal(r), -cimag(r));
	}
	if(hypot(x, y) < 0x1p-26)
		return z;	/* asinh z is z, to double's precision */
	if(hypot(x, y) > 1e150){
		r = clog(z);
		return cx(creal(r) + LN2, cimag(r));
	}
	a = x*x - y*y + 1;
	b = 2*x*y;
	s = csqrt(cx(a, b));
	return clog(cx(x + creal(s), y + cimag(s)));
}

/* asin z = -i asinh iz */
double complex
casin(double complex z)
{
	double complex t;

	t = casinh(cx(-cimag(z), creal(z)));
	return cx(cimag(t), -creal(t));
}

double complex
cacos(double complex z)
{
	double complex t;

	t = casin(z);
	return cx(PI_2 - creal(t), -cimag(t));
}

/* acosh z = log(z + sqrt(z+1) sqrt(z-1)) */
double complex
cacosh(double complex z)
{
	double complex p, m;
	double x, y;

	x = creal(z);
	y = cimag(z);
	p = csqrt(cx(x + 1, y));
	m = csqrt(cx(x - 1, y));
	return clog(cx(x + creal(p)*creal(m) - cimag(p)*cimag(m),
		y + creal(p)*cimag(m) + cimag(p)*creal(m)));
}

/*
 * atanh z = (log(1+z) - log(1-z))/2: its real part by log1p, which
 * keeps the digits of a small x, and its imaginary part by atan2
 */
double complex
catanh(double complex z)
{
	double x, y, d;

	x = creal(z);
	y = cimag(z);
	if(isnan(x) || isnan(y)){
		if(isinf(x) || isinf(y))
			return cx(copysign(0, x), copysign(PI_2, y));
		return cx(x + y, x + y);
	}
	if(isinf(x) || isinf(y))
		return cx(copysign(0, x), copysign(PI_2, y));
	if(fabs(x) == 1 && y == 0)
		return cx(copysign(HUGE_VAL, x), y);
	d = (1 - x)*(1 - x) + y*y;
	return cx(log1p(4*x/d)/4, atan2(2*y, (1 - x)*(1 + x) - y*y)/2);
}

/* atan z = -i atanh iz */
double complex
catan(double complex z)
{
	double complex t;

	t = catanh(cx(-cimag(z), creal(z)));
	return cx(cimag(t), -creal(t));
}
