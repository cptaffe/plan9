#include <fenv.h>

/*
 * wasm rounds to nearest, and keeps no exception flags
 */
int
feclearexcept(int e)
{
	return 0;
}

int
fegetexceptflag(fexcept_t *f, int e)
{
	*f = 0;
	return 0;
}

int
feraiseexcept(int e)
{
	return 0;
}

int
fesetexceptflag(const fexcept_t *f, int e)
{
	return 0;
}

int
fetestexcept(int e)
{
	return 0;
}

int
fegetround(void)
{
	return FE_TONEAREST;
}

int
fesetround(int r)
{
	return r != FE_TONEAREST;
}

int
fegetenv(fenv_t *e)
{
	*e = 0;
	return 0;
}

int
feholdexcept(fenv_t *e)
{
	*e = 0;
	return 0;
}

int
fesetenv(const fenv_t *e)
{
	return 0;
}

int
feupdateenv(const fenv_t *e)
{
	return 0;
}
