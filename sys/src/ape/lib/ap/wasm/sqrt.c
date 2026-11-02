#include <math.h>
#include <errno.h>

/* wasm has the instruction: ../plan9/sqrt.c is for the machines that have not. */
double
sqrt(double arg)
{
	if(arg < 0){
		errno = EDOM;
		return 0;
	}
	return __builtin_sqrt(arg);
}
