#include <math.h>
#include <float.h>

double
copysign(double x, double y)
{
	FPdbleword a, b;

	a.x = x;
	b.x = y;
	a.hi = (a.hi & 0x7fffffff) | (b.hi & 0x80000000);
	return a.x;
}
