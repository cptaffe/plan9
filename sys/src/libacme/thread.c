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

	va_start(arg, cmd);
	p = _winevsmprint(cmd, arg);
	va_end(arg);
	fd[0] = winfd(w, name, OREAD);
	fd[1] = dup(errto, -1);
	fd[2] = dup(errto, -1);
	pid = threadspawnl(fd, "rc", "rc", "-c", p, nil);
	free(p);
	return pid;
}

int
pipetowin(Win *w, char *name, int errto, char *cmd, ...)
{
	va_list arg;
	char *p;
	int fd[3], pid;

	va_start(arg, cmd);
	p = _winevsmprint(cmd, arg);
	va_end(arg);
	fd[0] = open("/dev/null", OREAD);
	fd[1] = winfd(w, name, OWRITE);
	if(errto == 0)
		fd[2] = dup(fd[1], -1);
	else
		fd[2] = dup(errto, -1);
	pid = threadspawnl(fd, "rc", "rc", "-c", p, nil);
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
		sysfatal("pipe: %r");
	fd[0] = open("/dev/null", OREAD);
	fd[1] = p[1];
	fd[2] = dup(p[1], -1);
	va_start(arg, fmt);
	cmd = _winevsmprint(fmt, arg);
	va_end(arg);
	threadspawnl(fd, "rc", "rc", "-Ic", cmd, nil);
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

static void
eventreader(void *v)
{
	Event e[2];
	Win *w;
	int i;

	w = v;
	i = 0;
	for(;;){
		if(winreadevent(w, &e[i]) <= 0)
			break;
		sendp(w->c, &e[i]);
		i = 1-i;
	}
	sendp(w->c, nil);
	threadexits(nil);
}

Channel*
wineventchan(Win *w)
{
	if(w->c == nil){
		w->c = chancreate(sizeof(Event*), 0);
		proccreate(eventreader, w, 32*1024);
	}
	return w->c;
}
