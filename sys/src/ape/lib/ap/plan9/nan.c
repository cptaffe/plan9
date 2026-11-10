#include <float.h>
#include <math.h>

#define	NANEXP	(2047<<20)
#define	NANMASK	(2047<<20)
#define	NANSIGN	(1<<31)

double
NaN(void)
{
	FPdbleword a;

	a.hi = NANEXP;
	a.lo = 1;
	return a.x;
}

int
isNaN(double d)
{
	FPdbleword a;

	a.x = d;
	if((a.hi & NANMASK) != NANEXP)
		return 0;
	return !isInf(d, 0);
}

double
Inf(int sign)
{
	FPdbleword a;

	a.hi = NANEXP;
	a.lo = 0;
	if(sign < 0)
		a.hi |= NANSIGN;
	return a.x;
}

int
isInf(double d, int sign)
{
	FPdbleword a;

	a.x = d;
	if(a.lo != 0)
		return 0;
	if(a.hi == NANEXP)
		return sign >= 0;
	if(a.hi == (NANEXP|NANSIGN))
		return sign <= 0;
	return 0;
}

/* C99: a quiet NaN (NaN's is signaling); the payload is not kept */
double
nan(const char *s)
{
	FPdbleword a;

	a.hi = NANEXP | 0x80000;
	a.lo = 0;
	return a.x;
}
