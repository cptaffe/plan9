#include <math.h>

/* wasm has the instruction, which clears the sign of -0 and NaN too */
double
fabs(double arg)
{
	return __builtin_fabs(arg);
}
