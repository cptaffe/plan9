#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "thr.h"

#define F(name)	__attribute__((import_module("p9web"), import_name(#name)))

enum {
	RFPROC = 1<<4,
	RFMEM = 1<<5,
	RFNOWAIT = 1<<6,
};

F(fiber_new) extern int p9web_fiber_new(void (*)(void *), void *, void *, int, void *, int, int);
F(fiber_setdata) extern void p9web_fiber_setdata(void *);
F(fiber_exit) extern void p9web_fiber_exit(void);


__attribute__((export_name("p9web_fiber_main"))) void
p9web_fiber_main(void (*fn)(void *), void *arg)
{
	fn(arg);
	p9web_fiber_exit();
}

static void
start(void *v)
{
	Thr *t;

	t = v;
	p9web_fiber_setdata(t);
	pthread_exit(t->f(t->arg));
}

static void
reap(void)
{
	Thr *t;

	while((t = _thrdead) != NULL){
		_thrdead = (Thr*)t->ret;
		free(t->stk);
		free(t->abuf);
		free(t);
	}
}

int
pthread_create(pthread_t *id, const pthread_attr_t *a, void *(*f)(void *), void *arg)
{
	Thr *t, **nt;
	size_t n;
	int i;

	reap();
	if((t = calloc(1, sizeof *t)) == NULL)
		return EAGAIN;
	n = a != NULL && a->stacksize? a->stacksize: Thrstack;
	t->stk = malloc(n);
	t->abuf = malloc(Thrabuf);
	if(t->stk == NULL || t->abuf == NULL)
		goto Nomem;
	for(i = 0; i < _nthr && _thr[i] != NULL; i++)
		;
	if(i == _nthr){
		if((nt = realloc(_thr, (_nthr+16)*sizeof *nt)) == NULL)
			goto Nomem;
		memset(nt+_nthr, 0, 16*sizeof *nt);
		_thr = nt;
		_nthr += 16;
	}
	t->id = i+2;
	t->f = f;
	t->arg = arg;
	t->detached = a != NULL && a->detached;
	_thr[i] = t;
	if((t->fiber = p9web_fiber_new(start, t, t->stk, n, t->abuf, Thrabuf, RFPROC|RFMEM|RFNOWAIT)) < 0){
		_thr[i] = NULL;
		goto Nomem;
	}
	_nalive++;
	*id = t->id;
	return 0;
Nomem:
	free(t->stk);
	free(t->abuf);
	free(t);
	return EAGAIN;
}
