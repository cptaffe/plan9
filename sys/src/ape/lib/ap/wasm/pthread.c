#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include "../stdio/iolib.h"
#include "thr.h"

#define F(name)	__attribute__((import_module("p9web"), import_name(#name)))

F(fiber_self) extern int p9web_fiber_self(void);
F(fiber_getdata) extern void *p9web_fiber_getdata(void);
F(fiber_block) extern void p9web_fiber_block(void);
F(fiber_wake) extern void p9web_fiber_wake(int);
F(fiber_exit) extern void p9web_fiber_exit(void);

static struct {
	int used;
	void (*dtor)(void *);
} keys[PTHREAD_KEYS_MAX];

Thr _thrmain = {1};
Thr **_thr;
int _nthr, _nalive = 1;

Thr *_thrdead;

extern void _doatthrexits(void);

void
_thrfree(Thr *t)
{
	int i;

	for(i = 0; i < _nthr; i++)
		if(_thr[i] == t)
			_thr[i] = NULL;
	t->ret = _thrdead;
	_thrdead = t;
}

Thr*
_thrself(void)
{
	Thr *t;

	t = p9web_fiber_getdata();
	return t != NULL? t: &_thrmain;
}

Thr*
_thrget(pthread_t id)
{
	if(id == 1)
		return &_thrmain;
	if(id < 2 || id-2 >= _nthr)
		return NULL;
	return _thr[id-2];
}

static void
unwait(Waiter **q, Waiter *w)
{
	Waiter **l;

	for(l = q; *l != NULL; l = &(*l)->next)
		if(*l == w){
			*l = w->next;
			return;
		}
}

void
_thrleave(Thr *t)
{
	Waiter **q, *w;

	if((q = t->q) == NULL)
		return;
	w = t->w;
	t->q = NULL;
	t->w = NULL;
	if(w->woken)
		_thrwake(q);
	else
		unwait(q, w);
}

static Thr*
enqueue(Waiter **q, Waiter *w)
{
	Thr *t;
	Waiter **l;

	t = _thrself();
	_thrleave(t);
	w->fiber = p9web_fiber_self();
	w->woken = 0;
	w->next = NULL;
	for(l = q; *l != NULL; l = &(*l)->next)
		;
	*l = w;
	t->q = q;
	t->w = w;
	return t;
}

void
_thrwait(Waiter **q, Waiter *w)
{
	Thr *t;

	t = enqueue(q, w);
	while(!w->woken)
		p9web_fiber_block();
	t->q = NULL;
	t->w = NULL;
}

int
_thrwake(Waiter **q)
{
	Waiter *w;

	if((w = *q) == NULL)
		return 0;
	*q = w->next;
	w->woken = 1;
	if(!w->timed)
		p9web_fiber_wake(w->fiber);
	return 1;
}

static int
past(const struct timespec *t)
{
	struct timespec now;

	clock_gettime(CLOCK_REALTIME, &now);
	return now.tv_sec > t->tv_sec || now.tv_sec == t->tv_sec && now.tv_nsec >= t->tv_nsec;
}

static int
timedwait(Waiter **q, Waiter *w, const struct timespec *t)
{
	struct timespec ms = {0, 1000000};
	Thr *s;

	w->timed = 1;
	s = enqueue(q, w);
	while(!w->woken){
		if(past(t)){
			unwait(q, w);
			s->q = NULL;
			s->w = NULL;
			return ETIMEDOUT;
		}
		nanosleep(&ms, NULL);
	}
	s->q = NULL;
	s->w = NULL;
	return 0;
}

pthread_t
pthread_self(void)
{
	return _thrself()->id;
}

int
pthread_equal(pthread_t a, pthread_t b)
{
	return a == b;
}

int
_thrsignal(pthread_t id, int sig)
{
	Thr *t;

	if((t = _thrget(id)) == NULL || t->done)
		return ESRCH;
	if(sig == 0)
		return 0;
	if(t == &_thrmain)
		return kill(getpid(), sig) < 0? errno: 0;
	return kill(t->pid, sig) < 0? errno: 0;
}

int
pthread_join(pthread_t id, void **r)
{
	Thr *t;
	Waiter w;

	if((t = _thrget(id)) == NULL)
		return ESRCH;
	if(t == _thrself())
		return EDEADLK;
	if(t->detached)
		return EINVAL;
	w.timed = 0;
	while(!t->done)
		_thrwait(&t->joinq, &w);
	if(r != NULL)
		*r = t->ret;
	_thrfree(t);
	return 0;
}

int
pthread_detach(pthread_t id)
{
	Thr *t;

	if((t = _thrget(id)) == NULL)
		return ESRCH;
	if(t->detached)
		return EINVAL;
	t->detached = 1;
	if(t->done)
		_thrfree(t);
	return 0;
}

static void
dtors(Thr *t)
{
	int i, n, again;
	void *v;

	for(n = 0; n < PTHREAD_DESTRUCTOR_ITERATIONS; n++){
		again = 0;
		for(i = 0; i < PTHREAD_KEYS_MAX; i++)
			if(keys[i].used && keys[i].dtor != NULL && (v = t->keys[i]) != NULL){
				t->keys[i] = NULL;
				keys[i].dtor(v);
				again = 1;
			}
		if(!again)
			break;
	}
}

void
pthread_exit(void *r)
{
	Thr *t;
	struct timespec ms = {0, 1000000};

	t = _thrself();
	_thrleave(t);
	_doatthrexits();
	dtors(t);
	if(t == &_thrmain){
		while(_nalive > 1)
			nanosleep(&ms, NULL);
		exit(0);
	}
	t->ret = r;
	t->done = 1;
	_nalive--;
	while(_thrwake(&t->joinq))
		;
	if(t->detached)
		_thrfree(t);
	p9web_fiber_exit();
	for(;;)
		;
}

int pthread_attr_init(pthread_attr_t *a) { a->detached = 0; a->stacksize = 0; return 0; }
int pthread_attr_destroy(pthread_attr_t *a) { return 0; }

int
pthread_attr_setdetachstate(pthread_attr_t *a, int s)
{
	if(s != PTHREAD_CREATE_JOINABLE && s != PTHREAD_CREATE_DETACHED)
		return EINVAL;
	a->detached = s;
	return 0;
}

int
pthread_attr_getdetachstate(const pthread_attr_t *a, int *s)
{
	*s = a->detached;
	return 0;
}

int
pthread_attr_setstacksize(pthread_attr_t *a, size_t n)
{
	if(n < 16*1024)
		return EINVAL;
	a->stacksize = n;
	return 0;
}

int
pthread_attr_getstacksize(const pthread_attr_t *a, size_t *n)
{
	*n = a->stacksize? a->stacksize: Thrstack;
	return 0;
}

int
pthread_once(pthread_once_t *o, void (*f)(void))
{
	if(o->done == 0){
		o->done = 1;
		f();
		o->done = 2;
	}
	while(o->done == 1)
		sched_yield();
	return 0;
}

int
pthread_key_create(pthread_key_t *k, void (*dtor)(void *))
{
	int i, j;

	for(i = 0; i < PTHREAD_KEYS_MAX; i++)
		if(!keys[i].used){
			keys[i].used = 1;
			keys[i].dtor = dtor;
			_thrmain.keys[i] = NULL;
			for(j = 0; j < _nthr; j++)
				if(_thr[j] != NULL)
					_thr[j]->keys[i] = NULL;
			*k = i;
			return 0;
		}
	return EAGAIN;
}

int
pthread_key_delete(pthread_key_t k)
{
	if(k < 0 || k >= PTHREAD_KEYS_MAX || !keys[k].used)
		return EINVAL;
	keys[k].used = 0;
	return 0;
}

void*
pthread_getspecific(pthread_key_t k)
{
	if(k < 0 || k >= PTHREAD_KEYS_MAX || !keys[k].used)
		return NULL;
	return _thrself()->keys[k];
}

int
pthread_setspecific(pthread_key_t k, const void *v)
{
	if(k < 0 || k >= PTHREAD_KEYS_MAX || !keys[k].used)
		return EINVAL;
	_thrself()->keys[k] = (void*)v;
	return 0;
}

int pthread_mutexattr_init(pthread_mutexattr_t *a) { a->type = PTHREAD_MUTEX_DEFAULT; return 0; }
int pthread_mutexattr_destroy(pthread_mutexattr_t *a) { return 0; }

int
pthread_mutexattr_settype(pthread_mutexattr_t *a, int t)
{
	if(t != PTHREAD_MUTEX_NORMAL && t != PTHREAD_MUTEX_ERRORCHECK && t != PTHREAD_MUTEX_RECURSIVE)
		return EINVAL;
	a->type = t;
	return 0;
}

int
pthread_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *a)
{
	m->locked = 0;
	m->owner = 0;
	m->wait = NULL;
	m->type = a != NULL? a->type: PTHREAD_MUTEX_DEFAULT;
	return 0;
}

int
pthread_mutex_destroy(pthread_mutex_t *m)
{
	return m->locked? EBUSY: 0;
}

int
pthread_mutex_lock(pthread_mutex_t *m)
{
	pthread_t self;
	Waiter w;

	self = pthread_self();
	w.timed = 0;
	for(;;){
		if(m->locked == 0){
			m->locked = 1;
			m->owner = self;
			return 0;
		}
		if(m->owner == self){
			if(m->type != PTHREAD_MUTEX_RECURSIVE)
				return EDEADLK;
			m->locked++;
			return 0;
		}
		_thrwait((Waiter**)&m->wait, &w);
	}
}

int
pthread_mutex_trylock(pthread_mutex_t *m)
{
	pthread_t self;

	self = pthread_self();
	if(m->locked && (m->owner != self || m->type != PTHREAD_MUTEX_RECURSIVE))
		return EBUSY;
	m->locked++;
	m->owner = self;
	return 0;
}

int
pthread_mutex_unlock(pthread_mutex_t *m)
{
	if(m->locked == 0 || m->owner != pthread_self())
		return EPERM;
	if(--m->locked == 0){
		m->owner = 0;
		_thrwake((Waiter**)&m->wait);
	}
	return 0;
}

int pthread_condattr_init(pthread_condattr_t *a) { a->clock = CLOCK_REALTIME; return 0; }
int pthread_condattr_destroy(pthread_condattr_t *a) { return 0; }

int
pthread_cond_init(pthread_cond_t *c, const pthread_condattr_t *a)
{
	c->wait = NULL;
	return 0;
}

int
pthread_cond_destroy(pthread_cond_t *c)
{
	return c->wait != NULL? EBUSY: 0;
}

int
pthread_cond_signal(pthread_cond_t *c)
{
	_thrwake((Waiter**)&c->wait);
	return 0;
}

int
pthread_cond_broadcast(pthread_cond_t *c)
{
	while(_thrwake((Waiter**)&c->wait))
		;
	return 0;
}

static int
release(pthread_mutex_t *m)
{
	int n;

	n = m->locked;
	m->locked = 0;
	m->owner = 0;
	_thrwake((Waiter**)&m->wait);
	return n;
}

static void
relock(pthread_mutex_t *m, int n)
{
	pthread_mutex_lock(m);
	m->locked = n;
}

int
pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m)
{
	Waiter w;
	int n;

	if(m->locked == 0 || m->owner != pthread_self())
		return EPERM;
	if(_nalive <= 1){
		n = release(m);
		pause();
		relock(m, n);
		return 0;
	}
	n = release(m);
	w.timed = 0;
	_thrwait((Waiter**)&c->wait, &w);
	relock(m, n);
	return 0;
}

int
pthread_cond_timedwait(pthread_cond_t *c, pthread_mutex_t *m, const struct timespec *t)
{
	Waiter w;
	int n, r;

	if(t->tv_nsec < 0 || t->tv_nsec >= 1000000000)
		return EINVAL;
	if(m->locked == 0 || m->owner != pthread_self())
		return EPERM;
	if(past(t))
		return ETIMEDOUT;
	n = release(m);
	r = timedwait((Waiter**)&c->wait, &w, t);
	relock(m, n);
	return r;
}

typedef struct Flock Flock;
struct Flock {
	pthread_t owner;
	int n;
	Waiter *q;
};

static Flock flocks[FOPEN_MAX];

static Flock*
flockof(FILE *f)
{
	if(f < _IO_stream || f >= _IO_stream+FOPEN_MAX)
		return NULL;
	return &flocks[f-_IO_stream];
}

int
_thrlockfile(FILE *f, int try)
{
	Flock *l;
	Waiter w;
	pthread_t me;

	if((l = flockof(f)) == NULL || f->flags&STRING)
		return 0;
	me = _thrself()->id;
	if(l->n > 0 && l->owner == me){
		l->n++;
		return 0;
	}
	if(l->n > 0 && try)
		return -1;
	w.timed = 0;
	while(l->n > 0)
		_thrwait(&l->q, &w);
	l->owner = me;
	l->n = 1;
	f->lk = 1;
	return 0;
}

void
_thrunlockfile(FILE *f)
{
	Flock *l;

	if((l = flockof(f)) == NULL || l->n == 0 || l->owner != _thrself()->id)
		return;
	if(--l->n == 0){
		f->lk = 0;
		_thrwake(&l->q);
	}
}
