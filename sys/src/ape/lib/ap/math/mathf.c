#include <math.h>
#include <errno.h>

/* d as a float: out of float's range is a range error */
static float
f(double d)
{
	float r;

	r = d;
	if(isinf(r) && !isinf(d) || r == 0 && d != 0)
		errno = ERANGE;
	return r;
}

/*
 * The float functions are the double ones, rounded to float.
 * BUG: those that round twice may be one off in the last bit.
 */

float
acosf(float x)
{
	return f(acos(x));
}

float
asinf(float x)
{
	return f(asin(x));
}

float
atanf(float x)
{
	return f(atan(x));
}

float
cosf(float x)
{
	return f(cos(x));
}

float
sinf(float x)
{
	return f(sin(x));
}

float
tanf(float x)
{
	return f(tan(x));
}

float
acoshf(float x)
{
	return f(acosh(x));
}

float
asinhf(float x)
{
	return f(asinh(x));
}

float
atanhf(float x)
{
	return f(atanh(x));
}

float
coshf(float x)
{
	return f(cosh(x));
}

float
sinhf(float x)
{
	return f(sinh(x));
}

float
tanhf(float x)
{
	return f(tanh(x));
}

float
expf(float x)
{
	return f(exp(x));
}

float
exp2f(float x)
{
	return f(exp2(x));
}

float
expm1f(float x)
{
	return f(expm1(x));
}

float
logf(float x)
{
	return f(log(x));
}

float
log10f(float x)
{
	return f(log10(x));
}

float
log1pf(float x)
{
	return f(log1p(x));
}

float
log2f(float x)
{
	return f(log2(x));
}

float
logbf(float x)
{
	return f(logb(x));
}

float
cbrtf(float x)
{
	return f(cbrt(x));
}

float
fabsf(float x)
{
	return f(fabs(x));
}

float
sqrtf(float x)
{
	return f(sqrt(x));
}

float
erff(float x)
{
	return f(erf(x));
}

float
erfcf(float x)
{
	return f(erfc(x));
}

float
lgammaf(float x)
{
	return f(lgamma(x));
}

float
tgammaf(float x)
{
	return f(tgamma(x));
}

float
ceilf(float x)
{
	return f(ceil(x));
}

float
floorf(float x)
{
	return f(floor(x));
}

float
nearbyintf(float x)
{
	return f(nearbyint(x));
}

float
rintf(float x)
{
	return f(rint(x));
}

float
roundf(float x)
{
	return f(round(x));
}

float
truncf(float x)
{
	return f(trunc(x));
}

float
atan2f(float x, float y)
{
	return f(atan2(x, y));
}

float
hypotf(float x, float y)
{
	return f(hypot(x, y));
}

float
powf(float x, float y)
{
	return f(pow(x, y));
}

float
fmodf(float x, float y)
{
	return f(fmod(x, y));
}

float
remainderf(float x, float y)
{
	return f(remainder(x, y));
}

float
copysignf(float x, float y)
{
	return f(copysign(x, y));
}

float
nextafterf(float x, float y)
{
	union { float f; unsigned int u; } a;

	if(isnan(x) || isnan(y))
		return x + y;
	if(x == y)
		return y;
	if(x == 0){
		a.u = 1;
		return y > 0? a.f: -a.f;
	}
	a.f = x;
	if((x < y) == (x > 0))
		a.u++;
	else
		a.u--;
	return a.f;
}

float
fdimf(float x, float y)
{
	return f(fdim(x, y));
}

float
fmaxf(float x, float y)
{
	return f(fmax(x, y));
}

float
fminf(float x, float y)
{
	return f(fmin(x, y));
}

float
frexpf(float x, int *e)
{
	return frexp(x, e);
}

float
ldexpf(float x, int e)
{
	return f(ldexp(x, e));
}

float
modff(float x, float *ip)
{
	double i, f;

	f = modf(x, &i);
	*ip = i;
	return f;
}

int
ilogbf(float x)
{
	return ilogb(x);
}

float
scalbnf(float x, int n)
{
	return f(scalbn(x, n));
}

float
scalblnf(float x, long n)
{
	return f(scalbln(x, n));
}

long
lrintf(float x)
{
	return lrint(x);
}

long long
llrintf(float x)
{
	return llrint(x);
}

long
lroundf(float x)
{
	return lround(x);
}

long long
llroundf(float x)
{
	return llround(x);
}

float
remquof(float x, float y, int *q)
{
	return f(remquo(x, y, q));
}

float
nanf(const char *s)
{
	return f(nan(s));
}

/* toward y as it is: not y rounded to float, which may be x */
float
nexttowardf(float x, long double y)
{
	if(isnan(x) || isnan(y))
		return x + y;
	if(x == y)
		return y;
	return nextafterf(x, x < y? INFINITY: -INFINITY);
}

/*
 * The product of floats is exact in a double, and their sum rounds
 * once there.  Rounding again, to float, is right unless the double
 * is halfway between floats and not exact: then it goes the way of
 * the error (musl's fmaf).
 */
float
fmaf(float x, float y, float z)
{
	union { double d; unsigned long long u; } a;
	double xy, r, err;
	int neg;
	float lo, hi;

	xy = (double)x * y;
	r = xy + z;
	if(!isfinite(r) || r - xy == z && r - z == xy)
		return f(r);
	/* halfway, if the floats either side of it average to it (exactly, in double) */
	lo = r;
	hi = nextafterf(lo, (double)lo < r? INFINITY: -INFINITY);
	if(((double)lo + hi)/2 != r)
		return f(r);
	a.d = r;
	neg = a.u >> 63;
	if(neg == (z > xy))
		err = xy - r + z;
	else
		err = z - r + xy;
	if(neg == (err < 0))
		a.u++;
	else
		a.u--;
	return f(a.d);
}
