/*
 * The system calls on wasm: each an import of the p9web module, which the runtime makes the
 * kernel's call (k/src/proc/wasi.ts). An import gives -errno on failure, and errstr then has
 * the kernel's text, as on any Plan 9.
 */
#include <u.h>
#include <libc.h>
#include <tos.h>

#define	P9(name)	__attribute__((import_module("p9web"), import_name(name)))

P9("open") extern int p9open(char*, int, int);
P9("create") extern int p9create(char*, int, int, ulong);
P9("close") extern int p9close(int);
P9("pread") extern long p9pread(int, void*, long, vlong);
P9("pwrite") extern long p9pwrite(int, void*, long, vlong);
P9("seek") extern int p9seek(int, vlong, int, vlong*);
P9("stat") extern int p9stat(char*, int, uchar*, int);
P9("fstat") extern int p9fstat(int, uchar*, int);
P9("wstat") extern int p9wstat(char*, int, uchar*, int);
P9("fwstat") extern int p9fwstat(int, uchar*, int);
P9("remove") extern int p9remove(char*, int);
P9("cd") extern int p9cd(char*, int);
P9("fd2path") extern int p9fd2path(int, char*, int);
P9("dup2") extern int p9dup2(int, int);
P9("dup") extern int p9dup1(int);
P9("pipe") extern int p9pipe(int*);
P9("rfork") extern int p9rfork(int);
P9("fork") extern int p9fork(int);
P9("exec") extern int p9exec(char*, int, char*, int, char*, int);
P9("await") extern int p9await(int, char*, int);
P9("errswap") extern int p9errswap(char*, uint);
P9("sleep") extern int p9sleep(long);
P9("bind") extern int p9bind(char*, int, char*, int, int);
P9("mount") extern int p9mount(int, int, char*, int, int, char*, int);
P9("unmount") extern int p9unmount(char*, int, char*, int);
P9("notify") extern int p9notify(int);
P9("exitmsg") extern int p9exitmsg(char*, int);
P9("rendezvous") extern void* p9rendezvous(void*, void*);
__attribute__((import_module("wasi_snapshot_preview1"), import_name("proc_exit"))) extern void procexit(int);

/* neg is a call's result: Plan 9's -1, for an import's -errno. */
static long
neg(long r)
{
	return r < 0 ? -1 : r;
}

int open(char *f, int m) { return neg(p9open(f, strlen(f), m)); }
int create(char *f, int m, ulong p) { return neg(p9create(f, strlen(f), m, p)); }
int close(int f) { return neg(p9close(f)); }
long pread(int f, void *a, long n, vlong o) { return neg(p9pread(f, a, n, o)); }
long pwrite(int f, void *a, long n, vlong o) { return neg(p9pwrite(f, a, n, o)); }
int stat(char *f, uchar *b, int n) { return neg(p9stat(f, strlen(f), b, n)); }
int fstat(int f, uchar *b, int n) { return neg(p9fstat(f, b, n)); }
int wstat(char *f, uchar *b, int n) { return neg(p9wstat(f, strlen(f), b, n)) < 0 ? -1 : n; }
int fwstat(int f, uchar *b, int n) { return neg(p9fwstat(f, b, n)) < 0 ? -1 : n; }
int remove(char *f) { return neg(p9remove(f, strlen(f))); }
int chdir(char *f) { return neg(p9cd(f, strlen(f))); }
int fd2path(int f, char *b, int n) { return neg(p9fd2path(f, b, n)); }
int dup(int o, int n) { return neg(n < 0 ? p9dup1(o) : p9dup2(o, n)); }
int pipe(int *p) { return neg(p9pipe(p)); }
int await(char *b, int n) { return neg(p9await(0, b, n)); }
int errstr(char *b, uint n) { return p9errswap(b, n); }
int sleep(long ms) { return neg(p9sleep(ms)); }
void* rendezvous(void *t, void *v) { return p9rendezvous(t, v); }
int bind(char *a, char *b, int f) { return neg(p9bind(a, strlen(a), b, strlen(b), f)); }
int unmount(char *a, char *b) { if(a == nil) a = ""; return neg(p9unmount(a, strlen(a), b, strlen(b))); }

int
mount(int fd, int afd, char *old, int flag, char *aname)
{
	if(aname == nil)
		aname = "";
	return neg(p9mount(fd, afd, old, strlen(old), flag, aname, strlen(aname)));
}

vlong
seek(int f, vlong o, int t)
{
	vlong v;

	if(p9seek(f, o, t, &v) < 0)
		return -1;
	return v;
}

/* rfork with RFPROC copies this process (asyncify unwinds and rewinds the stack: D93). */
int
rfork(int flags)
{
	int r;

	if(!(flags & RFPROC))
		return neg(p9rfork(flags));
	r = p9fork(flags);
	if(r == 0)
		_tos->pid = getpid();
	return neg(r);
}

int
exec(char *f, char *argv[])
{
	char *b, *p;
	int i, n;

	n = 0;
	for(i = 0; argv[i] != nil; i++)
		n += strlen(argv[i])+1;
	b = malloc(n+1);
	if(b == nil)
		return -1;
	p = b;
	for(i = 0; argv[i] != nil; i++)
		p = strecpy(p, b+n+1, argv[i])+1;
	p9exec(f, strlen(f), b, n, nil, 0);
	free(b);
	return -1;
}

void
_exits(char *s)
{
	if(s == nil)
		s = "";
	p9exitmsg(s, strlen(s));
	procexit(s[0] != 0);
	for(;;)
		;
}

/* A note: the handler runs in place at the end of a call (worker.ts), and noted leaves it. */
static void (*notef)(void*, char*);
static char notebuf[ERRMAX];
static jmp_buf notej;

int
notify(void (*f)(void*, char*))
{
	notef = f;
	return neg(p9notify(f != nil));
}

int
noted(int v)
{
	longjmp(notej, v == NCONT ? 2 : 1);
	return 0;
}

__attribute__((export_name("p9web_note_buf"))) char*
notebufp(void)
{
	return notebuf;
}

__attribute__((export_name("p9web_note"))) int
donote(void)
{
	switch(setjmp(notej)){
	case 0:
		if(notef != nil)
			(*notef)(nil, notebuf);
		return 0;
	case 2:
		return 1;
	}
	return 0;
}

/* Not yet on the kernel. */
long alarm(ulong ms) { USED(ms); return 0; }
int fauth(int fd, char *a) { USED(fd); USED(a); werrstr("authentication not required"); return -1; }
int fversion(int fd, int m, char *v, int n) { USED(fd); USED(m); USED(v); USED(n); werrstr("no fversion"); return -1; }
