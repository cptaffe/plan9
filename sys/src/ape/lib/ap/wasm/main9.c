/*
 * The machines' main9.s, for wasm: _start sets up the environment as _main does
 * (../plan9/_envsetup.c), and takes the arguments from the runtime.
 * As in libc/wasm/main9.c, the runtime knows a Plan 9 program by its mark, p9web_plan9.
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define	WASI(name)	__attribute__((import_module("wasi_snapshot_preview1"), import_name(name)))

WASI("args_sizes_get") extern int argsizes(int*, int*);
WASI("args_get") extern int argsget(char**, char*);

extern void _envsetup(void);

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
	static char *buf;
	int n;

	_envsetup();
	argsizes(&argc, &n);
	argv = malloc((argc+1)*sizeof(char*));
	buf = malloc(n);
	argsget(argv, buf);
	argv[argc] = 0;
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
