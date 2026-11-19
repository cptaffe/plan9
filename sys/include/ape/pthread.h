#ifndef __PTHREAD_H
#define __PTHREAD_H
#pragma lib "/$M/lib/ape/libap.a"

/*
 * POSIX threads. On wasm a thread is a process that shares this memory (rfork RFMEM), whose one
 * fiber the kernel switches to at a call that waits; one thread runs at a time, and none is
 * preempted: a thread that spins without a call, or a sched_yield, keeps the others waiting.
 */

#include <stddef.h>
#include <time.h>
#include <sched.h>

typedef int pthread_t;
typedef int pthread_key_t;
typedef struct { int done; } pthread_once_t;
typedef struct { int locked; int type; pthread_t owner; void *wait; } pthread_mutex_t;
typedef struct { int type; } pthread_mutexattr_t;
typedef struct { int detached; size_t stacksize; } pthread_attr_t;
typedef struct { void *wait; } pthread_cond_t;
typedef struct { int clock; } pthread_condattr_t;

#define PTHREAD_ONCE_INIT	{0}
#define PTHREAD_MUTEX_INITIALIZER	{0, 0, 0, 0}
#define PTHREAD_COND_INITIALIZER	{0}
#define PTHREAD_KEYS_MAX	128
#define PTHREAD_DESTRUCTOR_ITERATIONS	4

enum {
	PTHREAD_MUTEX_NORMAL,
	PTHREAD_MUTEX_ERRORCHECK,
	PTHREAD_MUTEX_RECURSIVE,
	PTHREAD_MUTEX_DEFAULT = PTHREAD_MUTEX_NORMAL,
	PTHREAD_CREATE_JOINABLE = 0,
	PTHREAD_CREATE_DETACHED,
};

#ifdef __cplusplus
extern "C" {
#endif

extern pthread_t pthread_self(void);
extern int pthread_equal(pthread_t, pthread_t);
extern int pthread_create(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);
extern int pthread_join(pthread_t, void **);
extern int pthread_detach(pthread_t);
extern void pthread_exit(void *);
extern int pthread_attr_init(pthread_attr_t *);
extern int pthread_attr_destroy(pthread_attr_t *);
extern int pthread_attr_setdetachstate(pthread_attr_t *, int);
extern int pthread_attr_getdetachstate(const pthread_attr_t *, int *);
extern int pthread_attr_setstacksize(pthread_attr_t *, size_t);
extern int pthread_attr_getstacksize(const pthread_attr_t *, size_t *);

extern int pthread_once(pthread_once_t *, void (*)(void));

extern int pthread_key_create(pthread_key_t *, void (*)(void *));
extern int pthread_key_delete(pthread_key_t);
extern void *pthread_getspecific(pthread_key_t);
extern int pthread_setspecific(pthread_key_t, const void *);

extern int pthread_mutexattr_init(pthread_mutexattr_t *);
extern int pthread_mutexattr_destroy(pthread_mutexattr_t *);
extern int pthread_mutexattr_settype(pthread_mutexattr_t *, int);
extern int pthread_mutex_init(pthread_mutex_t *, const pthread_mutexattr_t *);
extern int pthread_mutex_destroy(pthread_mutex_t *);
extern int pthread_mutex_lock(pthread_mutex_t *);
extern int pthread_mutex_trylock(pthread_mutex_t *);
extern int pthread_mutex_unlock(pthread_mutex_t *);

extern int pthread_condattr_init(pthread_condattr_t *);
extern int pthread_condattr_destroy(pthread_condattr_t *);
extern int pthread_cond_init(pthread_cond_t *, const pthread_condattr_t *);
extern int pthread_cond_destroy(pthread_cond_t *);
extern int pthread_cond_signal(pthread_cond_t *);
extern int pthread_cond_broadcast(pthread_cond_t *);
extern int pthread_cond_wait(pthread_cond_t *, pthread_mutex_t *);
extern int pthread_cond_timedwait(pthread_cond_t *, pthread_mutex_t *, const struct timespec *);

#ifdef __cplusplus
}
#endif

#endif
