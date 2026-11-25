#include <u.h>
#include <libc.h>
#include <tos.h>
#include "wasm.h"

P9(open) extern int p9web·open(char*, int, int);
P9(create) extern int p9web·create(char*, int, int, ulong);
P9(close) extern int p9web·close(int);
P9(pread) extern long p9web·pread(int, void*, long, vlong);
P9(pwrite) extern long p9web·pwrite(int, void*, long, vlong);
P9(seek) extern int p9web·seek(int, vlong, int, vlong*);
P9(stat) extern int p9web·stat(char*, int, uchar*, int);
P9(fstat) extern int p9web·fstat(int, uchar*, int);
P9(wstat) extern int p9web·wstat(char*, int, uchar*, int);
P9(fwstat) extern int p9web·fwstat(int, uchar*, int);
P9(remove) extern int p9web·remove(char*, int);
P9(chdir) extern int p9web·chdir(char*, int);
P9(fd2path) extern int p9web·fd2path(int, char*, int);
P9(dup) extern int p9web·dup(int, int);
P9(pipe) extern int p9web·pipe(int*);
P9(rfork) extern int p9web·rfork(int);
P9(exec) extern int p9web·exec(char*, int, char*, int);
P9(await) extern int p9web·await(int, char*, int);
P9(errstr) extern int p9web·errstr(char*, uint);
P9(sleep) extern int p9web·sleep(long);
P9(alarm) extern long p9web·alarm(ulong);
P9(bind) extern int p9web·bind(char*, int, char*, int, int);
P9(mount) extern int p9web·mount(int, int, char*, int, int, char*, int);
P9(unmount) extern int p9web·unmount(char*, int, char*, int);
P9(notify) extern int p9web·notify(void(*)(void*, char*));
P9(exits) extern void p9web·exits(char*, int);
P9(rendezvous) extern void* p9web·rendezvous(void*, void*);

typedef struct Pdata Pdata;
struct Pdata
{
	Tos	tos;
	struct {
		char	buf[ERRMAX];
		jmp_buf	j;
		uvlong	ureg[8];
		long	*jump;
		int	jret;
	} note;
	void	(*notify)(void*, char*);
	int	noting;
	int	trapping;
	int	trapfd;
	ulong	trappid;
};

P9(proc_getdata) extern Pdata* p9web·proc_getdata(void);
P9(proc_setdata) extern void p9web·proc_setdata(Pdata*);

static Pdata first;

static Pdata*
pdata(void)
{
	Pdata *p;

	if((p = p9web·proc_getdata()) == nil)
		p = &first;
	return p;
}

Tos*
_gettos(void)
{
	return &pdata()->tos;
}

void
_procstart(void (*f)(void*), void *a)
{
	Pdata d;

	memset(&d, 0, sizeof d);
	d.tos.pid = getpid();
	d.notify = pdata()->notify;
	p9web·proc_setdata(&d);
	(*f)(a);
	p9web·proc_setdata(nil);
}

static long
neg(long r)
{
	return r < 0 ? -1 : r;
}

int open(char *f, int m) { return neg(p9web·open(f, strlen(f), m)); }
int create(char *f, int m, ulong p) { return neg(p9web·create(f, strlen(f), m, p)); }
int close(int f) { return neg(p9web·close(f)); }
long pread(int f, void *a, long n, vlong o) { return neg(p9web·pread(f, a, n, o)); }
long pwrite(int f, void *a, long n, vlong o) { return neg(p9web·pwrite(f, a, n, o)); }
int stat(char *f, uchar *b, int n) { return neg(p9web·stat(f, strlen(f), b, n)); }
int fstat(int f, uchar *b, int n) { return neg(p9web·fstat(f, b, n)); }
int wstat(char *f, uchar *b, int n) { return neg(p9web·wstat(f, strlen(f), b, n)) < 0 ? -1 : n; }
int fwstat(int f, uchar *b, int n) { return neg(p9web·fwstat(f, b, n)) < 0 ? -1 : n; }
int remove(char *f) { return neg(p9web·remove(f, strlen(f))); }
int chdir(char *f) { return neg(p9web·chdir(f, strlen(f))); }
int fd2path(int f, char *b, int n) { return neg(p9web·fd2path(f, b, n)); }
int dup(int o, int n) { return neg(p9web·dup(o, n)); }
int pipe(int *p) { return neg(p9web·pipe(p)); }
int await(char *b, int n) { return neg(p9web·await(0, b, n)); }
int errstr(char *b, uint n) { return p9web·errstr(b, n); }
int sleep(long ms) { return neg(p9web·sleep(ms)); }
void* rendezvous(void *t, void *v) { return p9web·rendezvous(t, v); }
int bind(char *a, char *b, int f) { return neg(p9web·bind(a, strlen(a), b, strlen(b), f)); }
int unmount(char *a, char *b) { if(a == nil) a = ""; return neg(p9web·unmount(a, strlen(a), b, strlen(b))); }

int
mount(int fd, int afd, char *old, int flag, char *aname)
{
	if(aname == nil)
		aname = "";
	return neg(p9web·mount(fd, afd, old, strlen(old), flag, aname, strlen(aname)));
}

vlong
seek(int f, vlong o, int t)
{
	vlong v;

	if(p9web·seek(f, o, t, &v) < 0)
		return -1;
	return v;
}

#ifdef __clang__
extern int procfork(int) __attribute__((weak));
extern char _procforkmark[] __attribute__((weak));
#else
extern int procfork(int);
#endif

int
rfork(int flags)
{
	if(!(flags & RFPROC))
		return neg(p9web·rfork(flags));
#ifdef __clang__
	if(_procforkmark == nil){
		werrstr("rfork: the program was not built to fork");
		return -1;
	}
#endif
	return procfork(flags);
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
	for(i = 0; argv[i] != nil; i++){
		strcpy(p, argv[i]);
		p += strlen(p)+1;
	}
	p9web·exec(f, strlen(f), b, n);
	free(b);
	return -1;
}

void
_exits(char *s)
{
	if(s == nil)
		s = "";
	p9web·exits(s, strlen(s));
	for(;;)
		;
}

int
notify(void (*f)(void*, char*))
{
	if(p9web·notify(f) < 0)
		return -1;
	pdata()->notify = f;
	return 0;
}

static void
suicide(char *msg)
{
	char buf[64], *e;
	int fd, n;
	ulong pid;

	pid = pdata()->tos.pid;
	snprint(buf, sizeof buf, "/proc/%lud/status", pid);
	n = 0;
	if((fd = open(buf, OREAD)) >= 0){
		n = read(fd, buf, 27);
		close(fd);
	}
	if(n <= 0){
		e = argv0 != nil ? argv0 : "?";
		n = strlen(e);
		if(n > 27)
			n = 27;
		memmove(buf, e, n);
	}
	while(n > 0 && buf[n-1] == ' ')
		n--;
	buf[n] = 0;
	fprint(2, "%s %lud: suicide: %s\n", buf, pid, msg);
}

int
noted(int v)
{
	Pdata *p;

	p = pdata();
	if(v == NDFLT && p->trapping && strncmp(p->note.buf, "sys: trap: ", 11) == 0)
		suicide(p->note.buf);
	longjmp(p->note.j, v == NCONT ? 2 : 1);
	return 0;
}

void
_trap(int kind, ulong addr)
{
	Pdata *p;
	char msg[ERRMAX], f[32];
	int n, again;

	p = pdata();
	if(kind == 0)
		snprint(msg, sizeof msg, "sys: trap: divide error");
	else
		snprint(msg, sizeof msg, "sys: trap: fault %s addr=0x%lux", kind == 2 ? "write" : "read", addr);
	n = strlen(msg);
	again = 1;
	while(p->notify != nil && !p->noting){
		if(p->trappid != p->tos.pid){
			snprint(f, sizeof f, "/proc/%lud/note", p->tos.pid);
			if((p->trapfd = open(f, OWRITE|OCEXEC)) < 0)
				break;
			p->trappid = p->tos.pid;
			again = 0;
		}
		p->trapping = 1;
		if(write(p->trapfd, msg, n) != n){
			p->trapping = 0;
			p->trappid = 0;
			if(!again)
				break;
			again = 0;
			continue;
		}
		p->trapping = 0;
	}
	suicide(msg);
	_exits(msg);
}

EXPORT(p9web_note_buf) char*
p9web_note_buf(void)
{
	return pdata()->note.buf;
}

EXPORT(p9web_note_len) int
p9web_note_len(void)
{
	return sizeof first.note;
}

EXPORT(p9web_note) int
p9web_note(void (*f)(void*, char*))
{
	Pdata *p;

	p = pdata();
	switch(setjmp(p->note.j)){
	case 0:
		p->noting = 1;
		(*f)(p->note.ureg, p->note.buf);
		p->noting = 0;
		return 0;
	case 2:
		p->noting = 0;
		return p->note.jump != nil ? 2 : 1;
	}
	p->noting = 0;
	return 0;
}

void
notejmp(void*, jmp_buf j, int ret)
{
	Pdata *p;

	p = pdata();
	p->note.jump = j;
	p->note.jret = ret == 0 ? 1 : ret;
	noted(NCONT);
}

EXPORT(p9web_notejmp) void
p9web_notejmp(void)
{
	Pdata *p;
	long *j;

	p = pdata();
	j = p->note.jump;
	p->note.jump = nil;
	p->trapping = 0;
	longjmp(j, p->note.jret);
}

long alarm(ulong ms) { return neg(p9web·alarm(ms)); }

int fauth(int fd, char *a) { USED(fd); USED(a); werrstr("authentication not required"); return -1; }
int fversion(int fd, int m, char *v, int n) { USED(fd); USED(m); USED(v); USED(n); werrstr("no fversion"); return -1; }
