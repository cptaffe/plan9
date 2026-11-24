#include <u.h>
#include <libc.h>
#include <ctype.h>
#include <thread.h>
#include <acme.h>
#include "impl.h"

int
pipewinto(Win *w, char *name, int errto, char *cmd, ...)
{
	va_list arg;
	char *p;
	int fd[3], pid;

	if((fd[0] = winfd(w, name, OREAD)) < 0)
		return -1;
	va_start(arg, cmd);
	p = _winevsmprint(cmd, arg);
	va_end(arg);
	fd[1] = dup(errto, -1);
	fd[2] = dup(errto, -1);
	if((pid = threadspawnl(fd, "rc", "rc", "-c", p, nil)) < 0)
		werrstr("rc: %r");
	free(p);
	return pid;
}

int
pipetowin(Win *w, char *name, int errto, char *cmd, ...)
{
	va_list arg;
	char *p;
	int fd[3], pid;

	if((fd[1] = winfd(w, name, OWRITE)) < 0)
		return -1;
	va_start(arg, cmd);
	p = _winevsmprint(cmd, arg);
	va_end(arg);
	fd[0] = open("/dev/null", OREAD);
	if(errto == 0)
		fd[2] = dup(fd[1], -1);
	else
		fd[2] = dup(errto, -1);
	if((pid = threadspawnl(fd, "rc", "rc", "-c", p, nil)) < 0)
		werrstr("rc: %r");
	free(p);
	return pid;
}

char*
sysrun(char *fmt, ...)
{
	static char buf[1025];
	char *cmd;
	va_list arg;
	int n, fd[3], p[2], tot;

	if(pipe(p) < 0)
		return nil;
	fd[0] = open("/dev/null", OREAD);
	fd[1] = p[1];
	fd[2] = dup(p[1], -1);
	va_start(arg, fmt);
	cmd = _winevsmprint(fmt, arg);
	va_end(arg);
	if(threadspawnl(fd, "rc", "rc", "-Ic", cmd, nil) < 0){
		werrstr("rc: %r");
		free(cmd);
		close(p[0]);
		return nil;
	}
	free(cmd);
	tot = 0;
	while((n = read(p[0], buf+tot, sizeof buf-tot)) > 0)
		tot += n;
	close(p[0]);
	if(n < 0)
		return nil;
	if(tot == sizeof buf)
		tot--;
	buf[tot] = 0;
	while(tot > 0 && isspace(buf[tot-1]))
		tot--;
	buf[tot] = 0;
	if(tot == 0){
		werrstr("no output");
		return nil;
	}
	return buf;
}

void
_winevfree(Evreader *r)
{
	if(decref(&r->ref) > 0)
		return;
	chanfree(r->c);
	chanfree(r->quit);
	free(r);
}

static void
giveback(int fd, Event *e)
{
	char buf[64];
	int n;

	if(strchr("xXlL", e->c2) == nil)
		return;
	n = snprint(buf, sizeof buf, "%c%c%d %d \n", e->c1, e->c2, e->q0, e->q1);
	write(fd, buf, n);
}

static void
eventreader(void *v)
{
	Evreader *r;
	Event *e;
	Alt a[3];
	int i;

	r = v;
	threadsetname("wineventchan");
	a[0].c = r->c;
	a[0].v = &e;
	a[0].op = CHANSND;
	a[1].c = r->quit;
	a[1].v = nil;
	a[1].op = CHANRCV;
	a[2].op = CHANEND;
	for(i = 0;; i = 1-i){
		e = &r->e[i];
		if(_winreadev(r->fd, &r->b, e, &r->x) <= 0)
			e = nil;
		if(alt(a) == 1){
			if(e != nil)
				giveback(r->fd, e);
			break;
		}
		if(e == nil)
			break;
	}
	close(r->fd);
	_winevfree(r);
	threadexits(nil);
}

Channel*
wineventchan(Win *w)
{
	Evreader *r;
	int fd;

	if(w->c != nil)
		return w->c;
	if((fd = _winfid(w, "event")) < 0)
		return nil;
	r = emalloc(sizeof *r);
	if((r->fd = dup(fd, -1)) < 0){
		werrstr("dup event: %r");
		free(r);
		return nil;
	}
	r->ref.ref = 2;
	r->c = chancreate(sizeof(Event*), 0);
	r->quit = chancreate(sizeof(ulong), 1);
	w->c = r->c;
	w->ev = r;
	proccreate(eventreader, r, 32*1024);
	return w->c;
}
