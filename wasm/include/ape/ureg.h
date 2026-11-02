#ifndef __UREG_H
#define __UREG_H
#if !defined(_PLAN9_SOURCE)
    This header file is an extension to ANSI/POSIX
#endif

/*
 * A note arrives in place, at the end of a system call (libc/wasm/sys.c):
 * the machine's state is the wasm stack, which no program can see.
 */
struct Ureg
{
	unsigned long	pc;
	unsigned long	sp;
};

#endif
