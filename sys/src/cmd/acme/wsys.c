#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <cursor.h>
#include <mouse.h>
#include <keyboard.h>
#include <frame.h>
#include <fcall.h>
#include <plumb.h>
#include <libsec.h>
#include "dat.h"
#include "fns.h"

extern char Einuse[], Ebadctl[], Eperm[];

enum
{
	Nmouse	= 32,
	Nkbd	= 256,
};

struct Wsys
{
	int	on;
	Fid	*fid;
	Image	*i;
	Rectangle	r;
	char	name[64];
	int	gen;
	int	resized;
	Mouse	mq[Nmouse];
	int	mh;
	int	mn;
	Xfid	*mx;
	int	mopen;
	char	kbd[Nkbd];
	int	nkbd;
	Xfid	*kx;
	int	rawing;
	int	ctlopen;
	Cursor	cur;
	int	hascur;
};

static Window	*grab;
static Window	*curwin;

static Wsys*
wsysof(Window *w)
{
	if(w->wsys == nil)
		w->wsys = emalloc(sizeof(Wsys));
	return w->wsys;
}

static Rectangle
bodyr(Window *w)
{
	Rectangle r;

	r = w->body.all;
	r.min.y = w->body.fr.r.min.y;
	if(r.max.y < r.min.y)
		r.max.y = r.min.y;
	return r;
}

static void
wake(Xfid **xp)
{
	Xfid *x;

	x = *xp;
	if(x != nil){
		*xp = nil;
		sendp(x->c, nil);
	}
}

static void
hide(Image *i)
{
	originwindow(i, i->r.min, _screen->image->r.max);
}

static void
letgo(Image *i, char *name)
{
	hide(i);
	nameimage(i, name, 0);
	freeimage(i);
}

static void
freelayer(Wsys *s)
{
	if(s->i == nil)
		return;
	letgo(s->i, s->name);
	s->i = nil;
}

static void
mpush(Wsys *s, Mouse *m)
{
	int i;

	if(s->mn == Nmouse){
		i = (s->mh+s->mn-1)%Nmouse;
		if(s->mq[i].buttons == m->buttons){
			s->mq[i] = *m;
			return;
		}
		s->mh = (s->mh+1)%Nmouse;
		s->mn--;
	}
	s->mq[(s->mh+s->mn)%Nmouse] = *m;
	s->mn++;
	wake(&s->mx);
}

static void
layer(Window *w)
{
	Wsys *s;
	Rectangle r;
	Image *old;
	char oname[64];

	s = w->wsys;
	r = bodyr(w);
	if(Dx(r)<=0 || Dy(r)<=0){
		s->r = r;
		if(s->i != nil){
			hide(s->i);
			return;
		}
		r = Rect(r.min.x, r.min.y, r.min.x+max(Dx(r), 1), r.min.y+1);
	}else
		s->r = r;
	old = s->i;
	strcpy(oname, s->name);
	s->i = allocwindow(_screen, r, Refbackup, DNofill);
	if(s->i == nil){
		warning(nil, "acme: can't allocate window %d's image: %r\n", w->id);
		if(old != nil)
			letgo(old, oname);
		return;
	}
	draw(s->i, s->i->r, textcols[BACK], nil, ZP);
	if(old != nil){
		draw(s->i, s->i->r, old, nil, old->r.min);
		letgo(old, oname);
	}
	if(Dy(s->r) <= 0)
		hide(s->i);
	snprint(s->name, sizeof s->name, "noborder.acme.%d.%d.%d", getpid(), w->id, ++s->gen);
	if(nameimage(s->i, s->name, 1) == 0)
		warning(nil, "acme: can't name window %d's image: %r\n", w->id);
}

static void
restorecursor(void)
{
	if(curwin != nil){
		curwin = nil;
		setcursor(mousectl, nil);
	}
}

static void
setcur(Window *w)
{
	Wsys *s;

	s = w->wsys;
	if(!s->hascur){
		restorecursor();
		return;
	}
	if(curwin != w){
		curwin = w;
		setcursor(mousectl, &s->cur);
	}
}

static void
off(Window *w)
{
	Wsys *s;

	s = w->wsys;
	s->fid = nil;
	if(!s->on)
		return;
	s->on = FALSE;
	freelayer(s);
	if(grab == w)
		grab = nil;
	if(curwin == w)
		restorecursor();
}

char*
wsysdraw(Window *w, Fid *f)
{
	Wsys *s;
	Mouse m;

	s = wsysof(w);
	if(s->on)
		return s->fid==f ? nil : Einuse;
	s->fid = f;
	s->on = TRUE;
	layer(w);
	s->resized = FALSE;
	if(s->mopen && s->i != nil){
		s->resized = TRUE;
		m = *mouse;
		mpush(s, &m);
	}
	return nil;
}

void
wsyscursorclose(Window *w)
{
	if(w->wsys == nil)
		return;
	w->wsys->hascur = FALSE;
	if(curwin == w)
		restorecursor();
}

void
wsysctlclose(Window *w, Fid *f)
{
	if(w->wsys!=nil && w->wsys->fid==f)
		off(w);
}

static Window*
drawnat(Point p)
{
	Window *w;
	int i, j;

	for(j=0; j<row.ncol; j++)
		for(i=0; i<row.col[j]->nw; i++){
			w = row.col[j]->w[i];
			if(w->wsys!=nil && w->wsys->on && ptinrect(p, w->wsys->r))
				return w;
		}
	return nil;
}

int
wsysmouse(Text *t, Mouse *m)
{
	Window *w;
	Wsys *s;

	w = grab;
	if(w == nil && t!=nil && t->w!=nil && t->what==Body && t->w->wsys!=nil
	&& t->w->wsys->on && ptinrect(m->xy, t->w->wsys->r))
		w = t->w;
	if(w == nil && t == nil)
		w = drawnat(m->xy);
	if(w == nil){
		restorecursor();
		return FALSE;
	}
	s = w->wsys;
	winlock(w, 'M');
	if(s->mopen)
		mpush(s, m);
	grab = nil;
	if(m->buttons)
		grab = w;
	setcur(w);
	winunlock(w);
	return TRUE;
}

int
wsyskey(Window *w, Rune r)
{
	Wsys *s;
	char b[UTFmax];
	int n;

	s = w->wsys;
	if(s==nil || s->rawing==0 && !s->on)
		return FALSE;
	if(r==Kscrolloneup || r==Kscrollonedown)
		return FALSE;
	if(s->rawing == 0)
		return TRUE;
	n = runetochar(b, &r);
	if(s->nkbd+n <= Nkbd){
		memmove(s->kbd+s->nkbd, b, n);
		s->nkbd += n;
	}
	wake(&s->kx);
	return TRUE;
}

void
wsysresize(Window *w)
{
	Wsys *s;
	Mouse m;

	s = w->wsys;
	if(s==nil || !s->on)
		return;
	if(s->i!=nil && eqrect(s->r, bodyr(w)))
		return;
	layer(w);
	if(Dy(s->r) <= 0)
		return;
	s->resized = TRUE;
	m = *mouse;
	mpush(s, &m);
}

void
wsysscreen(void)
{
	int i, j;
	Window *w;

	for(j=0; j<row.ncol; j++)
		for(i=0; i<row.col[j]->nw; i++){
			w = row.col[j]->w[i];
			if(w->wsys != nil)
				freelayer(w->wsys);
		}
}

void
wsysdelete(Window *w)
{
	Wsys *s;

	s = w->wsys;
	if(s == nil)
		return;
	s->on = FALSE;
	s->fid = nil;
	freelayer(s);
	if(grab == w)
		grab = nil;
	if(curwin == w)
		restorecursor();
	wake(&s->mx);
	wake(&s->kx);
}

int
wsysflush(Window *w, int tag)
{
	Wsys *s;
	Xfid **xp;

	s = w->wsys;
	if(s == nil)
		return FALSE;
	xp = nil;
	if(s->mx!=nil && s->mx->fcall.tag==tag)
		xp = &s->mx;
	else if(s->kx!=nil && s->kx->fcall.tag==tag)
		xp = &s->kx;
	if(xp == nil)
		return FALSE;
	(*xp)->flushed = TRUE;
	wake(xp);
	return TRUE;
}

char*
wsysopen(Window *w, int q)
{
	Wsys *s;

	s = wsysof(w);
	switch(q){
	case QWmouse:
		if(s->mopen)
			return Einuse;
		s->mopen = TRUE;
		s->mn = 0;
		break;
	case QWconsctl:
		if(s->ctlopen)
			return Einuse;
		s->ctlopen = TRUE;
		break;
	}
	return nil;
}

void
wsysclose(Window *w, int q)
{
	Wsys *s;

	s = w->wsys;
	if(s == nil)
		return;
	switch(q){
	case QWmouse:
		s->mopen = FALSE;
		break;
	case QWconsctl:
		s->ctlopen = FALSE;
		s->rawing = 0;
		break;
	}
}

static int
block(Xfid *x, Window *w, Xfid **xp)
{
	Fcall fc;

	x->flushed = FALSE;
	*xp = x;
	winunlock(w);
	recvp(x->c);
	winlock(w, 'F');
	if(x->flushed)
		return FALSE;
	if(w->col==nil || w->wsys==nil){
		respond(x, &fc, "window shut down");
		return FALSE;
	}
	return TRUE;
}

static void
readstr(Xfid *x, char *s)
{
	Fcall fc;
	int n;
	uint off;

	n = strlen(s);
	off = x->fcall.offset;
	if(off > n)
		off = n;
	fc.count = x->fcall.count;
	if(off+fc.count > n)
		fc.count = n-off;
	fc.data = s+off;
	respond(x, &fc, nil);
}

void
wsysread(Xfid *x, Window *w, int q)
{
	Fcall fc;
	Wsys *s;
	Mouse m;
	char buf[80];
	int n, c, nr;
	Rune r;
	Rectangle rr;

	s = wsysof(w);
	switch(q){
	case QWwinname:
		if(!s->on){
			respond(x, &fc, "window is not drawn");
			return;
		}
		if(s->i == nil){
			respond(x, &fc, "window has no image");
			return;
		}
		readstr(x, s->name);
		return;

	case QWmouse:
		while(s->mn == 0)
			if(!block(x, w, &s->mx))
				return;
		m = s->mq[s->mh];
		s->mh = (s->mh+1)%Nmouse;
		s->mn--;
		c = 'm';
		if(s->resized)
			c = 'r';
		s->resized = FALSE;
		n = sprint(buf, "%c%11d %11d %11d %11ld ", c, m.xy.x, m.xy.y, m.buttons, m.msec);
		fc.data = buf;
		fc.count = min(n, x->fcall.count);
		respond(x, &fc, nil);
		return;

	case QWcons:
		while(s->nkbd == 0)
			if(!block(x, w, &s->kx))
				return;
		c = min(x->fcall.count, sizeof buf);
		for(n=0; n<s->nkbd; n+=nr){
			nr = chartorune(&r, s->kbd+n);
			if(n+nr > c)
				break;
		}
		if(n == 0)
			n = min(s->nkbd, c);
		memmove(buf, s->kbd, n);
		memmove(s->kbd, s->kbd+n, s->nkbd-n);
		s->nkbd -= n;
		fc.data = buf;
		fc.count = n;
		respond(x, &fc, nil);
		return;

	case QWwctl:
		rr = s->on ? s->r : bodyr(w);
		snprint(buf, sizeof buf, "%11d %11d %11d %11d %s %s ",
			rr.min.x, rr.min.y, rr.max.x, rr.max.y,
			activewin==w ? "current" : "notcurrent",
			Dy(rr)>0 ? "visible" : "hidden");
		readstr(x, buf);
		return;

	case QWcursor:
		respond(x, &fc, "cursor read not implemented");
		return;

	case QWconsctl:
		respond(x, &fc, Eperm);
		return;
	}
}

void
wsyswrite(Xfid *x, Window *w, int q)
{
	Fcall fc;
	Wsys *s;
	char *p;
	uchar *d;
	int n;
	Point pt;

	s = wsysof(w);
	n = x->fcall.count;
	p = x->fcall.data;
	d = (uchar*)p;
	switch(q){
	case QWconsctl:
		if(strncmp(p, "rawon", 5) == 0)
			s->rawing++;
		else if(strncmp(p, "rawoff", 6) == 0){
			if(s->rawing > 0)
				s->rawing--;
		}else if(strncmp(p, "holdon", 6)!=0 && strncmp(p, "holdoff", 7)!=0){
			respond(x, &fc, Ebadctl);
			return;
		}
		break;

	case QWcursor:
		if(n < 2*4+2*2*16)
			s->hascur = FALSE;
		else{
			s->cur.offset.x = (int)(d[0]<<24|d[1]<<16|d[2]<<8|d[3]);
			s->cur.offset.y = (int)(d[4]<<24|d[5]<<16|d[6]<<8|d[7]);
			memmove(s->cur.clr, d+2*4, 2*2*16);
			s->hascur = TRUE;
		}
		if(curwin==w || grab==w || (s->on && ptinrect(mouse->xy, s->r))){
			curwin = nil;
			setcur(w);
		}
		break;

	case QWmouse:
		if(p[0] != 'm'){
			respond(x, &fc, "bad mouse message");
			return;
		}
		pt.x = strtol(p+1, &p, 0);
		pt.y = strtol(p, nil, 0);
		if(s->on && (grab==w || ptinrect(mouse->xy, s->r)))
			moveto(mousectl, pt);
		break;

	default:
		respond(x, &fc, Eperm);
		return;
	}
	fc.count = x->fcall.count;
	respond(x, &fc, nil);
}
