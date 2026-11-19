#include <u.h>
#include <libc.h>
#include <thread.h>
#include <acme.h>
#include "impl.h"

static int
getec(int fd, Winbuf *b)
{
	int c;

	if(b->n <= 0){
		b->n = read(fd, b->buf, sizeof b->buf);
		if(b->n <= 0){
			c = b->n < 0 ? -2 : -1;
			b->n = 0;
			return c;
		}
		b->p = b->buf;
	}
	b->n--;
	return *(uchar*)b->p++;
}

static int
geten(int fd, Winbuf *b, int *np)
{
	int c, n;

	n = 0;
	while((c = getec(fd, b)) >= '0' && c <= '9')
		n = n*10+c-'0';
	if(c < 0)
		return c == -1 ? 0 : -1;
	if(c != ' '){
		werrstr("event: bad number");
		return -1;
	}
	*np = n;
	return 1;
}

static int
gete(int fd, Winbuf *b, Event *e)
{
	int i, c, r;
	char *p, *s;

	if((c = getec(fd, b)) < 0)
		return c == -1 ? 0 : -1;
	e->c1 = c;
	if((c = getec(fd, b)) < 0)
		return c == -1 ? 0 : -1;
	e->c2 = c;
	if((r = geten(fd, b, &e->q0)) <= 0 || (r = geten(fd, b, &e->q1)) <= 0
	|| (r = geten(fd, b, &e->flag)) <= 0 || (r = geten(fd, b, &e->nr)) <= 0)
		return r;
	if(e->nr > EVENTSIZE){
		werrstr("event: text too long");
		return -1;
	}
	p = e->text;
	for(i=0; i<e->nr; i++){
		s = p;
		do{
			if((c = getec(fd, b)) < 0)
				return c == -1 ? 0 : -1;
			*p++ = c;
		}while(c >= Runeself && p-s < UTFmax && !fullrune(s, p-s));
	}
	*p = 0;
	e->nb = p-e->text;
	if((c = getec(fd, b)) < 0)
		return c == -1 ? 0 : -1;
	if(c != '\n'){
		werrstr("event: no newline");
		return -1;
	}
	return 1;
}

static int
readev(int fd, Winbuf *b, Event *e, Event *x)
{
	int r;

	if((r = gete(fd, b, e)) <= 0)
		return r;
	e->oq0 = e->q0;
	e->oq1 = e->q1;
	e->arg[0] = 0;
	e->loc[0] = 0;
	if(e->flag&2){
		if((r = gete(fd, b, x)) <= 0)
			return r;
		if(e->q0 == e->q1){
			x->oq0 = e->q0;
			x->oq1 = e->q1;
			x->flag = e->flag;
			memmove(e, x, offsetof(Event, arg));
		}
	}
	if(e->flag&8){
		if((r = gete(fd, b, x)) <= 0)
			return r;
		strcpy(e->arg, x->text);
		if((r = gete(fd, b, x)) <= 0)
			return r;
		strcpy(e->loc, x->text);
	}
	return 1;
}

int
winreadevent(Win *w, Event *e)
{
	int fd;

	if((fd = _winfid(w, "event")) < 0)
		return -1;
	return readev(fd, &w->eb, e, &w->e2);
}

int
winreadrdevent(Win *w, Event *e)
{
	int fd;

	if((fd = _winfid(w, "rdevent")) < 0)
		return -1;
	return readev(fd, &w->rb, e, &w->r2);
}

int
winopenrdevent(Win *w, int seq)
{
	int fd;

	if(seq >= 0)
		fd = winseqopen(w, "rdevent", seq, OREAD|OCEXEC);
	else
		fd = _winopen(w, "rdevent", OREAD|OCEXEC);
	if(fd < 0)
		return -1;
	if(w->rdevent >= 0)
		close(w->rdevent);
	w->rdevent = fd;
	w->rb.n = 0;
	return 0;
}

int
eventfmt(Fmt *fmt)
{
	Event *e;

	e = va_arg(fmt->args, Event*);
	return fmtprint(fmt, "%c%c %d %d %d %d %q", e->c1, e->c2, e->q0, e->q1, e->flag, e->nr, e->text);
}
