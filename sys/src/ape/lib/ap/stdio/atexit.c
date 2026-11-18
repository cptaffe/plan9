#include <stdlib.h>
#include <string.h>
#define	NONEXIT	34

typedef struct Fn Fn;
typedef struct List List;
struct Fn {
	void (*f)(void);
	void (*g)(void *);
	void *a;
};
struct List {
	Fn *fn;
	int n, m;
	Fn fn0[NONEXIT];
};

static List ex, th;

static int
add(List *l, void (*f)(void), void (*g)(void *), void *a)
{
	Fn *p;

	if(l->fn == NULL){
		l->fn = l->fn0;
		l->m = NONEXIT;
	}
	if(l->n == l->m){
		if((p = malloc(2*l->m*sizeof *p)) == NULL)
			return -1;
		memmove(p, l->fn, l->n*sizeof *p);
		if(l->fn != l->fn0)
			free(l->fn);
		l->fn = p;
		l->m *= 2;
	}
	l->fn[l->n].f = f;
	l->fn[l->n].g = g;
	l->fn[l->n].a = a;
	l->n++;
	return 0;
}

int
atexit(void (*f)(void))
{
	return add(&ex, f, NULL, NULL) < 0;
}

int
__cxa_atexit(void (*g)(void *), void *a, void *dso)
{
	return add(&ex, NULL, g, a);
}

int
__cxa_thread_atexit(void (*g)(void *), void *a, void *dso)
{
	return add(&th, NULL, g, a);
}

static void
run(List *l)
{
	Fn e;

	while(l->n > 0){
		e = l->fn[--l->n];
		if(e.f)
			(*e.f)();
		else
			(*e.g)(e.a);
	}
}

void
_doatexits(void)
{
	run(&th);
	run(&ex);
}
