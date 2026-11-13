/*
 * The machines' main9.s, for wasm: _start sets up the environment as _main does
 * (../plan9/_envsetup.c), and takes the arguments from the stack, where exec puts them: its
 * last word points at argc, which argv and a nil follow (k/src/proc/mach.ts).
 * As in libc/wasm/main9.c, the kernel knows a Plan 9 program by its mark, p9web_plan9.
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

extern void _envsetup(void);
extern char __stack_high[];

/* the program's name, as BSD's err.h (libbsd) has it */
char *__progname;

/*
 * The compiler names a program's main by its arguments: main(void) has an alias __main_void,
 * and main(argc, argv) is __main_argc_argv.  main(argc, argv, envp), which is neither ANSI's nor
 * POSIX's, is main, and cannot be told from those here: its program has no main to run.
 */
extern int __main_void(void) __attribute__((weak));
extern int __main_argc_argv(int, char**) __attribute__((weak));

void
_start(void)
{
	static int argc, r;
	static char **argv;
	int *a;

	_envsetup();
	a = *(int**)(__stack_high - sizeof(int*));
	argc = a[0];
	argv = (char**)(a+1);
	if(argc > 0)
		__progname = argv[0];
	if(__main_argc_argv)
		r = __main_argc_argv(argc, argv);
	else if(__main_void)
		r = __main_void();
	else
		_exit(1);
	exit(r);
}

/* This is a Plan 9 program, not a WASI one: its stats are 9P2000's. */
__attribute__((export_name("p9web_plan9"))) void
isplan9(void)
{
}

/*
 * asyncify saves the stack here at a fork: every frame and its locals. A
 * program on APE (CPython's eval loop) may be deep: 1 MB, untouched until used.
 */
enum { Asynclen = 1<<20 };
static char asyncb[Asynclen] __attribute__((aligned(16)));

__attribute__((export_name("p9web_async_buf"))) char*
asyncbuf(void)
{
	return asyncb;
}

__attribute__((export_name("p9web_async_len"))) int
asynclen(void)
{
	return Asynclen;
}
