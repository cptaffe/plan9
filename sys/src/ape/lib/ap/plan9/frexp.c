#include <math.h>
#include <errno.h>
#define _RESEARCH_SOURCE
#include <float.h>

#define	MASK	0x7ffL
#define	SHIFT	20
#define	BIAS	1022L
#define	SIG	52

typedef	union
{
	double	d;
	struct
	{
#ifdef IEEE_8087
		long	ls;
		long	ms;
#else
		long	ms;
		long	ls;
#endif
	};
} Cheat;

double
frexp(double d, int *ep)
{
	Cheat x, a;

	*ep = 0;
	/* order matters: only isNaN can operate on NaN */
	if(isNaN(d) || isInf(d, 0) || d == 0)
		return d;
	x.d = d;
	a.d = fabs(d);
	if((a.ms >> SHIFT) == 0){	/* normalize subnormal numbers */
		x.d = (double)(1ULL<<SIG) * d;
		*ep = -SIG;
	}
	*ep += ((x.ms >> SHIFT) & MASK) - BIAS;
	x.ms &= ~(MASK << SHIFT);
	x.ms |= BIAS << SHIFT;
	return x.d;
}

/*
 * d * 2^e, by multiplying by powers of two that are numbers:
 * each step is exact, but the last, which rounds once.  A
 * result too small to be subnormal is scaled up first, so
 * that it is rounded only there.
 */
double
ldexp(double d, int e)
{
	Cheat x;
	double r;

	if(d == 0 || isNaN(d) || isInf(d, 0))
		return d;
	if(e > 1023){
		d *= 0x1p1023;
		e -= 1023;
		if(e > 1023){
			d *= 0x1p1023;
			e -= 1023;
			if(e > 1023)
				e = 1023;
		}
	}else if(e < -1022){
		d *= 0x1p-1022 * 0x1p53;
		e += 1022 - 53;
		if(e < -1022){
			d *= 0x1p-1022 * 0x1p53;
			e += 1022 - 53;
			if(e < -1022)
				e = -1022;
		}
	}
	x.ms = (e + BIAS + 1) << SHIFT;
	x.ls = 0;
	r = d * x.d;
	if(r == 0 || isInf(r, 0))
		errno = ERANGE;
	return r;
}

double
modf(double d, double *ip)
{
	double f;
	Cheat x;
	int e;

	x.d = d;
	e = (x.ms >> SHIFT) & MASK;
	if(e == MASK){
		*ip = d;
		if(x.ls != 0 || (x.ms & 0xfffffL) != 0)	/* NaN */
			return d;
		/* ±Inf */
		x.ms &= 0x80000000L;
		return x.d;
	}
	if(d < 1) {
		if(d < 0) {
			f = modf(-d, ip);
			*ip = -*ip;
			return -f;
		}
		*ip = copysign(0, d);	/* C99: -0's integer part is -0 */
		return d;
	}
	e -= BIAS;
	if(e <= SHIFT+1) {
		x.ms &= ~(0x1fffffL >> e);
		x.ls = 0;
	} else
	if(e <= SHIFT+33)
		x.ls &= ~(0x7fffffffL >> (e-SHIFT-2));
	*ip = x.d;
	return d - x.d;
}
