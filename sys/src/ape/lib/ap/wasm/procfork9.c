/*
 * libc's procfork, rfork's RFPROC, under APE's name (sys9.c's rfork takes it weakly): only
 * fork loads it, so only a program that forks has the fork import. Plan 9 C, as sys9.c.
 */
#define	procfork	_PROCFORK

#include "../../../../libc/wasm/procfork.c"
