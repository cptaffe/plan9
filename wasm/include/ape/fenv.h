#ifndef __FENV_H
#define __FENV_H
#pragma lib "/$M/lib/ape/libap.a"
/*
 * C99 fenv.h for wasm: it rounds to nearest, always, and keeps
 * no exception flags, so there are none to name.
 */

typedef int fenv_t;
typedef int fexcept_t;

#define FE_ALL_EXCEPT	0
#define FE_TONEAREST	0
#define FE_DFL_ENV	((const fenv_t *)0)

#ifdef __cplusplus
extern "C" {
#endif

extern int feclearexcept(int);
extern int fegetexceptflag(fexcept_t *, int);
extern int feraiseexcept(int);
extern int fesetexceptflag(const fexcept_t *, int);
extern int fetestexcept(int);
extern int fegetround(void);
extern int fesetround(int);
extern int fegetenv(fenv_t *);
extern int feholdexcept(fenv_t *);
extern int fesetenv(const fenv_t *);
extern int feupdateenv(const fenv_t *);

#ifdef __cplusplus
}
#endif

#endif /* __FENV_H */
