#include <stdlib.h>
#include <string.h>
#define	NONEXIT	34

typedef struct Fn Fn;
struct Fn {
	void (*f)(void);
	void (*g)(void *);
	void *a;
};

static Fn fns0[NONEXIT];
static Fn *fns = fns0;
static int nfns, mfns = NONEXIT;

static int
add(void (*f)(void), void (*g)(void *), void *a)
{
	Fn *p;

	if(nfns == mfns){
		if((p = malloc(2*mfns*sizeof *p)) == NULL)
			return -1;
		memmove(p, fns, nfns*sizeof *p);
		if(fns != fns0)
			free(fns);
		fns = p;
		mfns *= 2;
	}
	fns[nfns].f = f;
	fns[nfns].g = g;
	fns[nfns].a = a;
	nfns++;
	return 0;
}

int
atexit(void (*f)(void))
{
	return add(f, NULL, NULL) < 0;
}

int
__cxa_atexit(void (*g)(void *), void *a, void *dso)
{
	return add(NULL, g, a);
}

void
_doatexits(void)
{
	Fn e;

	while(nfns > 0){
		e = fns[--nfns];
		if(e.f)
			(*e.f)();
		else
			(*e.g)(e.a);
	}
}
