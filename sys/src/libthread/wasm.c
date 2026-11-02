/*
 * libthread on wasm. A stack cannot be switched by hand there, so a thread is a fiber: a
 * coroutine of the runtime (k/src/proc/fibers.ts), with a stack and an asyncify buffer of its
 * own. A proc is a kernel process that shares this memory, and its scheduler is a fiber too; the
 * fibers of a proc use its kernel process. One fiber runs at a time, and a switch is at a call
 * that waits or at the imports below. sched.c's setjmp and longjmp are _threadjump and _threadpause.
 */
#include <u.h>
#include <libc.h>
#include <thread.h>
#include "threadimpl.h"

#define	F(name)	__attribute__((import_module("p9web"), import_name(name)))

F("fiber_new") extern int fibernew(void (*)(void*), void*, void*, int, void*, int, int);
F("fiber_self") extern int fiberself(void);
F("fiber_getdata") extern void* fibergetdata(void);
F("fiber_setdata") extern void fibersetdata(void*);
F("fiber_block") extern void fiberblock(void);
F("fiber_wake") extern void fiberwake(int);
F("fiber_exit") extern void fiberexit(void);
F("spawn9") extern int spawn9(char*, int, char*, int, int);

enum
{
	Minstack = 64*1024,	/* the C stack: only what a function takes the address of */
	Abuf = 64*1024,	/* what asyncify saves of a stack */
};

__attribute__((export_name("p9web_fiber_main"))) void
fibermain(void (*fn)(void*), void *arg)
{
	fn(arg);
	fiberexit();
}

static int
newfiber(void (*fn)(void*), void *arg, uchar *stk, uint n, uchar *abuf, int own)
{
	int r;

	if((r = fibernew(fn, arg, stk, n, abuf, Abuf, own)) < 0)
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
	return fibergetdata();
}

void
_threadsetproc(Proc *p)
{
	fibersetdata(p);
}

/* The thread's own fiber: its stack is at least Minstack, whatever its creator asked. */
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

/* _threadjump goes from p's scheduler to thread t, and comes back when t pauses. */
void
_threadjump(Thread *t)
{
	if(t->fiber == 0)
		t->fiber = newfiber(threadstart, t, t->stk, t->stksize, t->abuf, 0);
	else
		fiberwake(t->fiber);
	fiberblock();
}

/* _threadpause goes from thread t to p's scheduler; a moribund thread does not come back. */
void
_threadpause(Proc *p, Thread *t)
{
	fiberwake(p->fiber);
	if(t->moribund)
		fiberexit();
	fiberblock();
}

int
_threadinsched(Proc *p)
{
	return fiberself() == p->fiber;
}

void
_threadschedself(Proc *p)
{
	p->fiber = fiberself();
}

static void
schedstart(void *v)
{
	Proc *p;

	p = v;
	if(p->rforkflag)
		rfork(p->rforkflag);
	_schedinit(p);
}

/* _schedfork starts proc p: a kernel process in this memory, and a fiber for its scheduler. */
int
_schedfork(Proc *p)
{
	newfiber(schedstart, p, _threadmalloc(Minstack, 0), Minstack, _threadmalloc(Abuf, 0), 1);
	return 0;
}

/* _schedexec starts e->prog in a process of its own, with this proc's descriptors but the
 * close-on-exec ones, of which e->fd[1] tells procexec that it worked. */
int
_schedexec(Execargs *e)
{
	char *b, *q, buf[ERRMAX];
	int i, n, pid;

	n = 0;
	for(i = 0; e->args[i] != nil; i++)
		n += strlen(e->args[i])+1;
	b = _threadmalloc(n+1, 0);
	q = b;
	for(i = 0; e->args[i] != nil; i++)
		q = strecpy(q, b+n+1, e->args[i])+1;
	pid = spawn9(e->prog, strlen(e->prog), b, n, RFFDG|RFNOTEG|RFREND);
	free(b);
	if(pid < 0){
		rerrstr(buf, sizeof buf);
		if(buf[0] == 0)
			strecpy(buf, buf+sizeof buf, "exec failed");
		write(e->fd[1], buf, strlen(buf));
	}
	return pid;
}

static int parked;	/* the first fiber's: its proc ended with others running */
static char parkmsg[ERRMAX];
static int firstsched = -1;

void
_threadfirstproc(void)
{
	firstsched = fiberself();
}

/* _threadschedend ends proc p, whose threads are gone. The first process holds the memory, so
 * its proc waits for the others; the last of them to end ends it. */
void
_threadschedend(char *msg)
{
	int none;

	lock(&_threadpq.lock);
	none = _threadpq.head == nil;
	unlock(&_threadpq.lock);
	if(fiberself() == firstsched){
		if(none)
			_exits(msg);
		utfecpy(parkmsg, parkmsg+sizeof parkmsg, msg);
		parked = 1;
		fiberblock();
		_exits(parkmsg);
	}
	if(none && parked)
		fiberwake(firstsched);
	fiberexit();
}

/*
 * semacquire(2). The kernel cannot see this memory, and those who share a semaphore are the
 * fibers of this instance: one runs at a time, so the count needs no atomic instruction, and a
 * waiter blocks here, in a list by address, until a semrelease wakes it. Unlike the kernel's, a
 * wait is not interrupted by a note: it goes to the fiber when it is woken.
 */
typedef struct Semwait Semwait;
struct Semwait
{
	long	*addr;
	int	fiber;
	Semwait	*next;
};
static Semwait *semwait;

static void
semwake(long *addr)
{
	Semwait **l, *w;

	for(l = &semwait; (w = *l) != nil; l = &w->next)
		if(w->addr == addr){
			*l = w->next;
			fiberwake(w->fiber);
			return;
		}
}

int
semacquire(long *addr, int block)
{
	Semwait w, **l;

	if(*addr < 0){
		werrstr("bad arg in system call");
		return -1;
	}
	while(*addr <= 0){
		if(!block)
			return 0;
		w.addr = addr;
		w.fiber = fiberself();
		w.next = nil;
		for(l = &semwait; *l != nil; l = &(*l)->next)	/* the first to wait is the first woken */
			;
		*l = &w;
		fiberblock();
	}
	--*addr;
	return 1;
}

/* A timed wait polls: sleep lets the fibers run that will release it. */
int
tsemacquire(long *addr, ulong ms)
{
	vlong end;

	end = nsec() + ms*1000000LL;
	while(*addr <= 0){
		if(nsec() >= end)
			return 0;
		sleep(1);
	}
	--*addr;
	return 1;
}

long
semrelease(long *addr, long n)
{
	long i;

	*addr += n;
	for(i = 0; i < n; i++)
		semwake(addr);
	return *addr;
}
