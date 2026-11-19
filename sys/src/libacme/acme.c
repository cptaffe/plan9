#include <u.h>
#include <libc.h>
#include <thread.h>
#include <acme.h>
#include "impl.h"

static Win *windows;
static Win *last;
QLock _winseqlk;

Win*
newwin(void)
{
	char buf[100], err[ERRMAX];
	int fd, id, n;
	Win *w;

	if((fd = open("/mnt/acme/new/ctl", ORDWR|OCEXEC)) < 0){
		werrstr("open /mnt/acme/new/ctl: %r");
		return nil;
	}
	if((n = read(fd, buf, sizeof buf-1)) <= 0){
		if(n == 0)
			werrstr("eof");
		werrstr("read /mnt/acme/new/ctl: %r");
		close(fd);
		return nil;
	}
	buf[n] = 0;
	if((id = atoi(buf)) == 0){
		close(fd);
		werrstr("new/ctl: bad id %.20q", buf);
		return nil;
	}
	if((w = openwin(id, fd)) == nil){
		rerrstr(err, sizeof err);
		write(fd, "delete", 6);
		close(fd);
		werrstr("%s", err);
	}
	return w;
}

static int
sess(Win *w)
{
	char buf[64], s[16];
	int n;

	if(w->dir[0] != 0)
		return 0;
	snprint(buf, sizeof buf, "/mnt/amux/%d", w->id);
	if(access(buf, AEXIST) < 0){
		snprint(w->dir, sizeof w->dir, "/mnt/acme/%d", w->id);
		return 0;
	}
	snprint(buf, sizeof buf, "/mnt/amux/%d/clone", w->id);
	if((w->clone = open(buf, ORDWR|OCEXEC)) < 0){
		werrstr("open %s: %r", buf);
		return -1;
	}
	if((n = read(w->clone, s, sizeof s-1)) <= 0){
		if(n == 0)
			werrstr("eof");
		werrstr("read %s: %r", buf);
		close(w->clone);
		w->clone = -1;
		return -1;
	}
	s[n] = 0;
	snprint(w->dir, sizeof w->dir, "/mnt/amux/%d/%d", w->id, atoi(s));
	return 0;
}

char*
_winpath(Win *w, char *name, char *buf, int n)
{
	if(sess(w) < 0)
		return nil;
	snprint(buf, n, "%s/%s", w->dir, name);
	return buf;
}

char*
winpath(Win *w)
{
	if(sess(w) < 0)
		return nil;
	return w->dir;
}

Win*
openwin(int id, int ctl)
{
	char buf[100];
	Win *w;

	w = emalloc(sizeof *w);
	memset(w, 0, sizeof *w);
	w->id = id;
	w->clone = w->actl = -1;
	if(sess(w) < 0){
		free(w);
		return nil;
	}
	w->ctl = -1;
	if(w->clone < 0)
		w->ctl = ctl;
	if(w->ctl < 0){
		snprint(buf, sizeof buf, "%s/ctl", w->dir);
		if((w->ctl = open(buf, ORDWR|OCEXEC)) < 0){
			werrstr("open %s: %r", buf);
			if(w->clone >= 0)
				close(w->clone);
			free(w);
			return nil;
		}
	}
	if(w->clone >= 0)
		w->actl = ctl;
	w->tag = w->body = w->addr = w->event = w->data = w->xdata = w->rdevent = -1;
	w->next = nil;
	w->prev = last;
	if(last)
		last->next = w;
	else
		windows = w;
	last = w;
	return w;
}

static void
fdclose(int *fd)
{
	if(*fd >= 0){
		close(*fd);
		*fd = -1;
	}
}

void
winclosefiles(Win *w)
{
	fdclose(&w->ctl);
	fdclose(&w->tag);
	fdclose(&w->body);
	fdclose(&w->addr);
	fdclose(&w->event);
	fdclose(&w->data);
	fdclose(&w->xdata);
	fdclose(&w->rdevent);
	fdclose(&w->actl);
	if(w->clone >= 0){
		fdclose(&w->clone);
		w->dir[0] = 0;
	}
	w->eb.n = w->rb.n = 0;
}

void
winfree(Win *w)
{
	winclosefiles(w);
	if(w->c){
		chanfree(w->c);
		w->c = nil;
	}
	if(w->next)
		w->next->prev = w->prev;
	else
		last = w->prev;
	if(w->prev)
		w->prev->next = w->next;
	else
		windows = w->next;
	free(w);
}

void
windeleteall(void)
{
	Win *w, *next;

	for(w=windows; w; w=next){
		next = w->next;
		winctl(w, "delete");
	}
}

int
_winopen(Win *w, char *name, int mode)
{
	char buf[100];
	int fd;

	if(_winpath(w, name, buf, sizeof buf) == nil)
		return -1;
	if(_winguarded(name))
		qlock(&_winseqlk);
	fd = open(buf, mode);
	if(fd < 0)
		werrstr("open %s: %r", buf);
	if(_winguarded(name))
		qunlock(&_winseqlk);
	return fd;
}

int*
_winfd(Win *w, char *name)
{
	if(strcmp(name, "ctl") == 0)
		return &w->ctl;
	if(strcmp(name, "body") == 0)
		return &w->body;
	if(strcmp(name, "addr") == 0)
		return &w->addr;
	if(strcmp(name, "tag") == 0)
		return &w->tag;
	if(strcmp(name, "event") == 0)
		return &w->event;
	if(strcmp(name, "data") == 0)
		return &w->data;
	if(strcmp(name, "xdata") == 0)
		return &w->xdata;
	if(strcmp(name, "rdevent") == 0)
		return &w->rdevent;
	werrstr("no window file %s", name);
	return nil;
}

int
_winfid(Win *w, char *name)
{
	int *fd;

	if((fd = _winfd(w, name)) == nil)
		return -1;
	if(*fd < 0)
		*fd = _winopen(w, name, (strcmp(name, "rdevent") == 0 ? OREAD : ORDWR)|OCEXEC);
	return *fd;
}

int
winopenfd(Win *w, char *name, int mode)
{
	return _winopen(w, name, mode);
}

int
winfd(Win *w, char *name, int mode)
{
	return _winopen(w, name, mode);
}

static int
islock(char *p)
{
	return strncmp(p, "lock", 4) == 0 || strncmp(p, "unlock", 6) == 0;
}

static int
lockfd(Win *w)
{
	char buf[64];

	if(w->actl < 0){
		snprint(buf, sizeof buf, "/mnt/acme/%d/ctl", w->id);
		if((w->actl = open(buf, ORDWR|OCEXEC)) < 0)
			werrstr("open %s: %r", buf);
	}
	return w->actl;
}

int
winctl(Win *w, char *fmt, ...)
{
	char *s, *p, *e, *q;
	va_list arg;
	int n, fd, k, lk;

	va_start(arg, fmt);
	s = _winevsmprint(fmt, arg);
	va_end(arg);
	if((fd = _winfid(w, "ctl")) < 0){
		free(s);
		return -1;
	}
	if(w->clone < 0){
		n = pwrite(fd, s, strlen(s), 0);
		free(s);
		return n;
	}
	n = 0;
	for(p = s; *p != 0; p = e){
		lk = islock(p);
		e = p;
		do{
			q = strchr(e, '\n');
			e = q != nil ? q+1 : e+strlen(e);
		}while(*e != 0 && islock(e) == lk);
		k = lk ? lockfd(w) : fd;
		if(k < 0 || pwrite(k, p, e-p, 0) != e-p){
			n = -1;
			break;
		}
		n += e-p;
	}
	free(s);
	return n;
}

int
winname(Win *w, char *fmt, ...)
{
	char *s;
	va_list arg;
	int n;

	va_start(arg, fmt);
	s = _winevsmprint(fmt, arg);
	va_end(arg);
	n = winctl(w, "name %s\n", s);
	free(s);
	return n;
}

int
winprint(Win *w, char *name, char *fmt, ...)
{
	char *s;
	va_list arg;
	int n, fd;

	if((fd = _winfid(w, name)) < 0)
		return -1;
	va_start(arg, fmt);
	s = _winevsmprint(fmt, arg);
	va_end(arg);
	n = write(fd, s, strlen(s));
	free(s);
	return n;
}

int
winaddr(Win *w, char *fmt, ...)
{
	char *s;
	va_list arg;
	int n, fd;

	if((fd = _winfid(w, "addr")) < 0)
		return -1;
	va_start(arg, fmt);
	s = _winevsmprint(fmt, arg);
	va_end(arg);
	n = write(fd, s, strlen(s));
	free(s);
	return n;
}

int
winreadaddr(Win *w, uint *q1)
{
	char buf[40], *p;
	uint q0;
	int n, fd;

	if((fd = _winfid(w, "addr")) < 0)
		return -1;
	if((n = pread(fd, buf, sizeof buf-1, 0)) <= 0){
		if(n == 0)
			werrstr("empty addr");
		return -1;
	}
	buf[n] = 0;
	q0 = strtoul(buf, &p, 10);
	if(q1)
		*q1 = strtoul(p, nil, 10);
	return q0;
}

int
winread(Win *w, char *file, void *a, int n)
{
	int fd;

	if((fd = _winfid(w, file)) < 0)
		return -1;
	return read(fd, a, n);
}

int
winwrite(Win *w, char *file, void *a, int n)
{
	int fd;

	if((fd = _winfid(w, file)) < 0)
		return -1;
	return write(fd, a, n);
}

char*
winmread(Win *w, char *file)
{
	char *buf;
	int n, tot, m, fd;

	if((fd = _winfid(w, file)) < 0)
		return nil;
	m = 128;
	buf = emalloc(m+1);
	tot = 0;
	while((n = read(fd, buf+tot, m-tot)) > 0){
		tot += n;
		if(tot >= m){
			m += 128;
			buf = erealloc(buf, m+1);
		}
	}
	if(n < 0){
		free(buf);
		return nil;
	}
	buf[tot] = 0;
	return buf;
}

vlong
winseek(Win *w, char *file, vlong n, int type)
{
	int fd;

	if((fd = _winfid(w, file)) < 0)
		return -1;
	if(strcmp(file, "event") == 0)
		w->eb.n = 0;
	else if(strcmp(file, "rdevent") == 0)
		w->rb.n = 0;
	return seek(fd, n, type);
}

int
winwriteevent(Win *w, Event *e)
{
	char buf[100];
	int fd;

	if((fd = _winfid(w, "event")) < 0)
		return -1;
	snprint(buf, sizeof buf, "%c%c%d %d \n", e->c1, e->c2, e->q0, e->q1);
	return write(fd, buf, strlen(buf));
}

int
windel(Win *w, int sure)
{
	return winctl(w, sure ? "delete" : "del");
}
