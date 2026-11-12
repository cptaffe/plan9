#ifndef __PTHREAD_H
#define __PTHREAD_H
#pragma lib "/$M/lib/ape/libap.a"

/*
 * POSIX threads, as much as a process of one thread of control has: thread-specific data,
 * once, and mutexes. pthread_create says EAGAIN: the process has its one thread only. A
 * program that uses threads for their data (a library's state per thread) runs as it does
 * with one thread; one that needs a second thread to run at once does not run.
 */

#include <stddef.h>

typedef int pthread_t;
typedef int pthread_key_t;
typedef struct { int done; } pthread_once_t;
typedef struct { int locked; int type; } pthread_mutex_t;
typedef struct { int type; } pthread_mutexattr_t;
typedef struct { int detached; } pthread_attr_t;

#define PTHREAD_ONCE_INIT	{0}
#define PTHREAD_MUTEX_INITIALIZER	{0, 0}
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

#ifdef __cplusplus
}
#endif

#endif
