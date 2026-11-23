#include <u.h>
#include <libc.h>
#include <thread.h>
#include "threadimpl.h"

#ifdef __clang__
#define	F(name)	__attribute__((import_module("p9web"), import_name(#name)))
#define	EXPORT(name)	__attribute__((export_name(#name)))
#else
#define	F(name)
#define	EXPORT(name)
#endif

F(fiber_new) extern int p9web·fiber_new(void (*)(void*), void*, void*, int, void*, int, int);
F(fiber_self) extern int p9web·fiber_self(void);
F(fiber_getdata) extern void* p9web·fiber_getdata(void);
F(fiber_setdata) extern void p9web·fiber_setdata(void*);
F(fiber_block) extern void p9web·fiber_block(void);
F(fiber_wake) extern void p9web·fiber_wake(int);
F(fiber_exit) extern void p9web·fiber_exit(void);
F(fiber_exits) extern void p9web·fiber_exits(char*, int);
F(await) extern int p9web·await(int, char*, int);
extern int rforkfn(int, void(*)(void*), void*);
extern void _procstart(void(*)(void*), void*);

enum
{
	Minstack = 64*1024,
	Abuf = 64*1024,
};

EXPORT(p9web_fiber_main) void
p9web_fiber_main(void (*fn)(void*), void *arg)
{
	fn(arg);
	p9web·fiber_exit();
}

static int
newfiber(void (*fn)(void*), void *arg, uchar *stk, uint n, uchar *abuf, int flag)
{
	int r;

	if((r = p9web·fiber_new(fn, arg, stk, n, abuf, Abuf, flag)) < 0)
		sysfatal("fiber_new: error %d", -r);
	return r;
}

void
_systhreadinit(void)
{
}

Proc*
_threadgetproc(void)
{
	return p9web·fiber_getdata();
}

void
_threadsetproc(Proc *p)
{
	p9web·fiber_setdata(p);
}

void
_threadinitstack(Thread *t, void (*f)(void*), void *arg)
{
	if(t->stksize < Minstack){
		free(t->stk);
		t->stksize = Minstack;
		t->stk = _threadmalloc(t->stksize, 0);
	}
	t->abuf = _threadmalloc(Abuf, 0);
	t->fn = f;
	t->arg = arg;
}

static void
threadstart(void *v)
{
	Thread *t;

	t = v;
	_threadsetproc(t->proc);
	(*t->fn)(t->arg);
	threadexits(nil);
}

void
_threadjump(Thread *t)
{
	if(t->fiber == 0)
		t->fiber = newfiber(threadstart, t, t->stk, t->stksize, t->abuf, 0);
	else
		p9web·fiber_wake(t->fiber);
	p9web·fiber_block();
}

void
_threadpause(Proc *p, Thread *t)
{
	p9web·fiber_wake(p->fiber);
	if(t->moribund)
		p9web·fiber_exit();
	p9web·fiber_block();
}

int
_threadinsched(Proc *p)
{
	return p9web·fiber_self() == p->fiber;
}

void
_threadschedself(Proc *p)
{
	p->fiber = p9web·fiber_self();
}

static void
schedstart(void *v)
{
	_procstart(_schedinit, v);
}

int
_schedfork(Proc *p)
{
	newfiber(schedstart, p, _threadmalloc(Minstack, 0), Minstack, _threadmalloc(Abuf, 0), RFPROC|RFMEM|RFNOWAIT|p->rforkflag);
	return 0;
}

static void
efork(void *v)
{
	Execargs *e;
	char buf[ERRMAX];

	e = v;
	close(e->fd[0]);
	exec(e->prog, e->args);
	rerrstr(buf, sizeof buf);
	if(buf[0] == 0)
		strecpy(buf, buf+sizeof buf, "exec failed");
	write(e->fd[1], buf, strlen(buf));
	close(e->fd[1]);
	_exits(buf);
}

int
_schedexec(Execargs *e)
{
	return rforkfn(RFFDG|RFNOTEG|RFREND, efork, e);
}

Waitmsg*
_threadwaitpid(int pid)
{
	int n, l;
	char buf[512], *fld[5];
	Waitmsg *w;

	if((n = p9web·await(pid, buf, sizeof buf-1)) < 0)
		return nil;
	buf[n] = '\0';
	if(tokenize(buf, fld, nelem(fld)) != nelem(fld)){
		werrstr("couldn't parse wait message");
		return nil;
	}
	l = strlen(fld[4])+1;
	if((w = malloc(sizeof(Waitmsg)+l)) == nil)
		return nil;
	w->pid = atoi(fld[0]);
	w->time[0] = atoi(fld[1]);
	w->time[1] = atoi(fld[2]);
	w->time[2] = atoi(fld[3]);
	w->msg = (char*)&w[1];
	memmove(w->msg, fld[4], l);
	return w;
}

static int firstsched = -1;

void
_threadfirstproc(void)
{
	firstsched = p9web·fiber_self();
}

void
_threadschedend(char *msg)
{
	if(p9web·fiber_self() == firstsched)
		p9web·fiber_exits(msg, strlen(msg));
	p9web·fiber_exit();
}

typedef struct Semwait Semwait;
struct Semwait
{
	long	*addr;
	int	fiber;
	int	queued;
	Semwait	*next;
};
static int semlk;
static Semwait *semwait;

static void
semlock(void)
{
	while(_tas(&semlk))
		;
}

static void
semunlock(void)
{
	cas32((u32int*)&semlk, 1, 0);
}

static void
semsleep(long *addr)
{
	Semwait w, **l;

	w.addr = addr;
	w.fiber = p9web·fiber_self();
	w.queued = 1;
	w.next = nil;
	for(l = &semwait; *l != nil; l = &(*l)->next)
		;
	*l = &w;
	do{
		semunlock();
		p9web·fiber_block();
		semlock();
	}while(w.queued);
}

static int
semwake(long *addr)
{
	Semwait **l, *w;

	for(l = &semwait; (w = *l) != nil; l = &w->next)
		if(w->addr == addr){
			*l = w->next;
			w->queued = 0;
			p9web·fiber_wake(w->fiber);
			return 1;
		}
	return 0;
}

int
semacquire(long *addr, int block)
{
	int r;

	semlock();
	for(;;){
		if(*addr < 0){
			r = -1;
			break;
		}
		if(*addr > 0){
			--*addr;
			r = 1;
			break;
		}
		if(!block){
			r = 0;
			break;
		}
		semsleep(addr);
	}
	semunlock();
	if(r < 0)
		werrstr("bad arg in system call");
	return r;
}

int
tsemacquire(long *addr, ulong ms)
{
	vlong end;

	end = nsec() + ms*1000000LL;
	for(;;){
		semlock();
		if(*addr > 0){
			--*addr;
			semunlock();
			return 1;
		}
		semunlock();
		if(nsec() >= end)
			return 0;
		sleep(1);
	}
}

long
semrelease(long *addr, long n)
{
	long i, v;

	semlock();
	*addr += n;
	for(i = 0; i < n && semwake(addr); i++)
		;
	v = *addr;
	semunlock();
	return v;
}
