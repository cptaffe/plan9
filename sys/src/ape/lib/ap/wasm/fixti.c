#include <math.h>
#include "ti.h"

static W
dtou(double d)
{
	W w;
	uvlong m;
	int e;

	w.lo = w.hi = 0;
	if(!(d >= 1))
		return w;
	if(d < 18446744073709551616.0){
		w.lo = (uvlong)d;
		return w;
	}
	m = (uvlong)ldexp(frexp(d, &e), 64);
	if(e > 128){
		w.lo = w.hi = ~0ULL;
		return w;
	}
	e -= 64;
	if(e == 64){
		w.hi = m;
		return w;
	}
	w.hi = m >> (64-e);
	w.lo = m << e;
	return w;
}

uvvlong
__fixunsdfti(double d)
{
	return dtou(d).v;
}

uvvlong
__fixunssfti(float f)
{
	return dtou(f).v;
}

vvlong
__fixdfti(double d)
{
	W w;

	if(d >= 0)
		return dtou(d).v;
	w = dtou(-d);
	w.lo = ~w.lo + 1;
	w.hi = ~w.hi + (w.lo == 0);
	return w.v;
}

vvlong
__fixsfti(float f)
{
	return __fixdfti(f);
}
