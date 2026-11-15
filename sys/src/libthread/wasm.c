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

#ifdef __clang__
#define	F(name)	__attribute__((import_module("p9web"), import_name(#name)))
#define	EXPORT(name)	__attribute__((export_name(#name)))
#else
#define	F(name)
#define	EXPORT(name)
#endif

F(fiber_new) extern int p9web·fiber_new(void (*)(void*), void*, void*, int, void*, int, int);	/* the last: a new proc's rfork flags, or 0 */
F(fiber_self) extern int p9web·fiber_self(void);
F(fiber_getdata) extern void* p9web·fiber_getdata(void);
F(fiber_setdata) extern void p9web·fiber_setdata(void*);
F(fiber_block) extern void p9web·fiber_block(void);
F(fiber_wake) extern void p9web·fiber_wake(int);
F(fiber_exit) extern void p9web·fiber_exit(void);
F(fiber_exits) extern void p9web·fiber_exits(char*, int);
F(await) extern int p9web·await(int, char*, int);
extern int rforkfn(int, void(*)(void*), void*);

enum
{
	Minstack = 64*1024,	/* the C stack: only what a function takes the address of */
	Abuf = 64*1024,	/* what asyncify saves of a stack */
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
		p9web·fiber_wake(t->fiber);
	p9web·fiber_block();
}

/* _threadpause goes from thread t to p's scheduler; a moribund thread does not come back. */
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
	Proc *p;

	p = v;
	_schedinit(p);
}

/* _schedfork starts proc p: a kernel process in this memory, made by the rfork that makes it
 * (procrfork's flags too: what RFFDG copies is this proc's now, not when p first runs), and a
 * fiber for its scheduler. */
int
_schedfork(Proc *p)
{
	newfiber(schedstart, p, _threadmalloc(Minstack, 0), Minstack, _threadmalloc(Abuf, 0), RFPROC|RFMEM|RFNOWAIT|p->rforkflag);
	return 0;
}

/* efork is _schedexec's child, as Plan 9's: the exec, or why it failed on e->fd[1]. */
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

/* _schedexec starts e->prog in a process of its own, with this proc's descriptors but the
 * close-on-exec ones, of which e->fd[1] tells procexec that it worked. Plan 9's child shares
 * this memory (RFMEM); here it is a copy that starts at efork (rforkfn), and exec replaces it. */
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

/* _threadschedend ends proc p, whose threads are gone. The first proc ends as Plan 9's does,
 * with its message, and the others go on: the memory is theirs till the last of them ends. (Its
 * exits would end them all: threadexitsall's, here.) */
void
_threadschedend(char *msg)
{
	if(p9web·fiber_self() == firstsched)
		p9web·fiber_exits(msg, strlen(msg));
	p9web·fiber_exit();
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
			p9web·fiber_wake(w->fiber);
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
		w.fiber = p9web·fiber_self();
		w.next = nil;
		for(l = &semwait; *l != nil; l = &(*l)->next)	/* the first to wait is the first woken */
			;
		*l = &w;
		p9web·fiber_block();
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
