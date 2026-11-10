#include <complex.h>

/*
 * The float complex functions are the double ones, rounded to float.
 */

float complex
cexpf(float complex z)
{
	return cexp(z);
}

float complex
clogf(float complex z)
{
	return clog(z);
}

float complex
csqrtf(float complex z)
{
	return csqrt(z);
}

float complex
csinf(float complex z)
{
	return csin(z);
}

float complex
ccosf(float complex z)
{
	return ccos(z);
}

float complex
ctanf(float complex z)
{
	return ctan(z);
}

float complex
casinf(float complex z)
{
	return casin(z);
}

float complex
cacosf(float complex z)
{
	return cacos(z);
}

float complex
catanf(float complex z)
{
	return catan(z);
}

float complex
csinhf(float complex z)
{
	return csinh(z);
}

float complex
ccoshf(float complex z)
{
	return ccosh(z);
}

float complex
ctanhf(float complex z)
{
	return ctanh(z);
}

float complex
casinhf(float complex z)
{
	return casinh(z);
}

float complex
cacoshf(float complex z)
{
	return cacosh(z);
}

float complex
catanhf(float complex z)
{
	return catanh(z);
}

float complex
cprojf(float complex z)
{
	return cproj(z);
}

float complex
conjf(float complex z)
{
	return conj(z);
}

float
cabsf(float complex z)
{
	return cabs(z);
}

float
cargf(float complex z)
{
	return carg(z);
}

float
cimagf(float complex z)
{
	return cimag(z);
}

float
crealf(float complex z)
{
	return creal(z);
}

float complex
cpowf(float complex z, float complex w)
{
	return cpow(z, w);
}
