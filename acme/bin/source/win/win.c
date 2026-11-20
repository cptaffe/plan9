#include <u.h>
#include <libc.h>
#include <bio.h>
#include <thread.h>
#include <acme.h>
#include "dat.h"

static Window*
wmake(Win *a)
{
	Window *w;

	w = emalloc(sizeof(Window));
	w->a = a;
	w->id = a->id;
	w->cevent = chancreate(sizeof(Msg*), 0);
	wctl(w, "noscroll\n");
	return w;
}

Window*
wnew(void)
{
	Win *a;

	if((a = newwin()) == nil)
		error("new window: %r");
	return wmake(a);
}

Window*
wold(char *id, ulong *nbody)
{
	char buf[5*12+1];
	Win *a;
	int fd, n;

	if((a = openwin(atoi(id), -1)) == nil)
		error("window %s: %r", id);
	if((fd = winopenfd(a, "ctl", OREAD|OCEXEC)) < 0)
		error("%r");
	n = read(fd, buf, 5*12);
	close(fd);
	if(n != 5*12)
		error("read window %s's ctl: %r", id);
	buf[n] = 0;
	*nbody = strtoul(buf+2*12, nil, 10);
	return wmake(a);
}

void
wdump(Window *w, char *dir, char *cmd)
{
	if(dir != nil)
		wctl(w, "dumpdir %s\n", dir);
	if(cmd != nil)
		wctl(w, "dumpbody %s\n", cmd);
}

int
wguard(Window *w)
{
	int seq;

	if((seq = winseq(w->a)) < 0)
		error("read seq: %r");
	return seq;
}

int
wput(Window *w, uint q, int seq, char *s, int n)
{
	char buf[ERRMAX];
	int fd;

	if(winaddr(w->a, "#%ud", q) < 0)
		return 0;
	if((fd = winseqopen(w->a, "data", seq, OWRITE|OCEXEC)) < 0)
		return -1;
	n = write(fd, s, n);
	close(fd);
	if(n >= 0)
		return 1;
	rerrstr(buf, sizeof buf);
	if(strstr(buf, "seq mismatch") != nil)
		return 0;
	return -1;
}

void
wevents(void *v)
{
	Window *w;
	Msg *m;
	int i;

	w = v;
	for(i=0; ; i++){
		if(i >= NEVENT)
			i = 0;
		m = &w->m[i];
		if(winreadevent(w->a, &m->e) <= 0)
			threadexits(nil);
		m->b = m->e.text;
		m->nb = m->e.nb;
		m->nr = m->e.nr;
		sendp(w->cevent, m);
	}
}

void
wtag(Window *w, char *s)
{
	if(winwrite(w->a, "tag", s, strlen(s)) != strlen(s))
		error("write tag: %r");
}

void
wname(Window *w, char *s)
{
	if(winname(w->a, "%s", s) < 0)
		error("name window: %r");
}

void
wwrite(Window *w, Msg *m)
{
	winwriteevent(w->a, &m->e);
}

int
wbody(Window *w, uint q0, uint q1, char *data)
{
	int m, n, nr, nb;
	char buf[256];

	m = q0;
	nb = 0;
	while(m < q1){
		if(winaddr(w->a, "#%d", m) < 0)
			error("write addr: %r");
		if((n = winread(w->a, "data", buf, sizeof buf)) < 0)
			error("read data: %r");
		nr = utfnlen(buf, n);
		while(m+nr > q1){
			do; while(n>0 && (buf[--n]&0xC0)==0x80);
			--nr;
		}
		if(n == 0)
			break;
		memmove(data, buf, n);
		nb += n;
		data += n;
		*data = 0;
		m += nr;
	}
	return nb;
}

int
wdel(Window *w, int sure)
{
	return windel(w->a, sure) >= 0;
}

int
waddr(Window *w, char *addr, int errok)
{
	if(winaddr(w->a, "%s", addr) < 0){
		if(!errok)
			error("write addr %s: %r", addr);
		return 0;
	}
	return 1;
}

int
wselect(Window *w, char *addr, int errok)
{
	if(waddr(w, addr, errok)){
		wctl(w, "dot=addr\n");
		return 1;
	}
	return 0;
}

void
wzap(Window *w, char *addr)
{
	waddr(w, addr, 0);
	winwrite(w->a, "data", addr, 0);
}
