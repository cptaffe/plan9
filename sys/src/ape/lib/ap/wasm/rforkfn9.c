/*
 * libc's rforkfn under APE's name, _RFORKFN (rforkfn.c is APE's own, on it): only a program
 * that starts a child so has forkentry and its imports. Plan 9 C, as sys9.c.
 */
#define	rforkfn		_RFORKFN
#define	_exits		_EXITS

#include "../../../../libc/wasm/rforkfn.c"
