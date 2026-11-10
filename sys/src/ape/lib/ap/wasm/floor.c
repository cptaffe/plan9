#include <math.h>

/* wasm has the instructions: ../math/floor.c is for the machines that have not */
double
floor(double d)
{
	return __builtin_floor(d);
}

double
ceil(double d)
{
	return __builtin_ceil(d);
}
