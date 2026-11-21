#include <math.h>
#include "ti.h"

static double
utod(W w)
{
	uvlong m;
	int n;

	if(w.hi == 0)
		return (double)w.lo;
	n = __builtin_clzll(w.hi);
	m = n? w.hi<<n | w.lo>>(64-n): w.hi;
	m |= (w.lo<<n) != 0;
	return ldexp((double)m, 64-n);
}

static float
utof(W w)
{
	uvlong m;
	int n;

	if(w.hi == 0)
		return (float)w.lo;
	n = __builtin_clzll(w.hi);
	m = n? w.hi<<n | w.lo>>(64-n): w.hi;
	m |= (w.lo<<n) != 0;
	return (float)ldexp((double)(float)m, 64-n);
}

double
__floatuntidf(uvvlong a)
{
	W w;

	w.v = a;
	return utod(w);
}

float
__floatuntisf(uvvlong a)
{
	W w;

	w.v = a;
	return utof(w);
}

double
__floattidf(vvlong a)
{
	W w;

	w.v = a;
	if((vlong)w.hi >= 0)
		return utod(w);
	w.lo = ~w.lo + 1;
	w.hi = ~w.hi + (w.lo == 0);
	return -utod(w);
}

float
__floattisf(vvlong a)
{
	W w;

	w.v = a;
	if((vlong)w.hi >= 0)
		return utof(w);
	w.lo = ~w.lo + 1;
	w.hi = ~w.hi + (w.lo == 0);
	return -utof(w);
}
