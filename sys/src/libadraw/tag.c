#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <mouse.h>
#include <cursor.h>
#include <keyboard.h>
#include <frame.h>
#include <plumb.h>
#include <complete.h>
#include <acme.h>
#include <adraw.h>
#include "impl.h"

enum
{
	Border = 2,
	Delay = 2,
	Minmove = 4,
};

static void	fill(Tagx*);
static void	setsel(Tagx*, uint, uint);
static void	layout(Tagx*, int);
static void	newbody(Tagx*);
static void	send1(Tagx*, Event*);
static int	give(Tagx*, Channel*, void*);


static Rune Lfile[] = { '.', '-', '+', '/', ':', '@', 0 };
static Rune Lhttp[] = { 'h', 't', 't', 'p', ':', '/', '/', 0 };
static Rune Lhttps[] = { 'h', 't', 't', 'p', 's', ':', '/', '/', 0 };
static Rune Lspacepipe[] = { ' ', '|', 0 };
static Rune Ltabpipe[] = { '\t', '|', 0 };
static Rune Ldelsnarf[] = { ' ', 'D', 'e', 'l', ' ', 'S', 'n', 'a', 'r', 'f', 0 };

static Image*
colimage(ulong v)
{
	return allocimage(display, Rect(0,0,1,1), screen->chan, 1, v);
}

static void
themecols(Image **cols)
{
	Themecol *c;
	ulong v;

	cols[BACK] = allocimagemix(display, DPalebluegreen, DWhite);
	cols[HIGH] = colimage(DPalegreygreen);
	cols[BORD] = colimage(DPurpleblue);
	cols[TEXT] = display->black;
	cols[HTEXT] = display->black;
	if((c = themelook("tag")) != nil){
		if((v = c->bg ? c->bg : c->col) != 0)
			cols[BACK] = colimage(v);
		if(c->fg)
			cols[TEXT] = colimage(c->fg);
	}
	if((c = themelook("tagsel")) != nil){
		if((v = c->bg ? c->bg : c->col) != 0)
			cols[HIGH] = colimage(v);
		if(c->fg)
			cols[HTEXT] = colimage(c->fg);
	}
	if((v = themecolor("tagborder", 0)) != 0)
		cols[BORD] = colimage(v);
}

static int
hexcol(char *s, ulong *v)
{
	char *e;

	if(*s == '#')
		s++;
	if(strlen(s) != 6)
		return 0;
	*v = strtoul(s, &e, 16)<<8 | 0xFF;
	return *e == 0;
}

static ulong
mix(ulong a, ulong b, int pct)
{
	ulong r, g, bl;

	r = (((a>>24)&0xFF)*(100-pct) + ((b>>24)&0xFF)*pct)/100;
	g = (((a>>16)&0xFF)*(100-pct) + ((b>>16)&0xFF)*pct)/100;
	bl = (((a>>8)&0xFF)*(100-pct) + ((b>>8)&0xFF)*pct)/100;
	return r<<24 | g<<16 | bl<<8 | 0xFF;
}

static int
colorargs(char *s, ulong *bg, ulong *fg)
{
	char *f[3], *b;
	int n, ok;

	b = _adrawstrdup(s);
	n = tokenize(b, f, nelem(f));
	ok = n == 0 || (n == 2 && hexcol(f[0], bg) && hexcol(f[1], fg));
	free(b);
	return ok ? n : -1;
}

static void
freecols(Image **cols)
{
	int i;

	for(i=0; i<NCOL; i++)
		if(cols[i] != display->black && cols[i] != display->white)
			freeimage(cols[i]);
}

static void
setcolors(Tagx *t, char *s)
{
	Image *cols[NCOL], *i2, *i3;
	ulong bg, fg, b2, b3;
	int i, bad;

	if(colorargs(s, &bg, &fg) == 2){
		cols[BACK] = colimage(bg);
		cols[HIGH] = colimage(mix(bg, fg, 11));
		cols[BORD] = colimage(mix(bg, fg, 24));
		cols[TEXT] = colimage(fg);
		cols[HTEXT] = colimage(fg);
		b2 = mix(bg, 0xAA0000FF, 45);
		b3 = mix(bg, 0x006600FF, 45);
	}else{
		themecols(cols);
		b2 = themecolor("but2", 0xAA0000FF);
		b3 = themecolor("but3", 0x006600FF);
	}
	i2 = colimage(b2);
	i3 = colimage(b3);
	bad = i2 == nil || i3 == nil;
	for(i=0; i<NCOL; i++)
		bad |= cols[i] == nil;
	if(bad){
		freecols(cols);
		freeimage(i2);
		freeimage(i3);
		return;
	}
	freecols(t->f->cols);
	memmove(t->f->cols, cols, sizeof cols);
	frinittick(t->f);
	freeimage(t->but2);
	freeimage(t->but3);
	t->but2 = i2;
	t->but3 = i3;
	layout(t, 1);
}

static int
isalnum(Rune c)
{
	if(c <= ' ')
		return 0;
	if(0x7F<=c && c<=0xA0)
		return 0;
	if(utfrune("!\"#$%&'()*+,-./:;<=>?@[\\]^`{|}~", c))
		return 0;
	return 1;
}

static int
isfilec(Rune r)
{
	return isalnum(r) || r && runestrchr(Lfile, r) != nil;
}

static int
isexecc(Rune r)
{
	return isfilec(r) || r=='<' || r=='|' || r=='>';
}

static int
isaddrc(Rune r)
{
	return r && utfrune("0123456789+-/$.#,;?", r) != nil;
}

static int
isregexc(Rune r)
{
	return r && (isalnum(r) || utfrune("^+-.*?#,;[]()$", r) != nil);
}

static Rune
readc(Tagx *t, uint q)
{
	if(q >= t->ns)
		return 0;
	return t->s[q];
}

static char*
text(Tagx *t, uint q0, uint q1)
{
	if(q1 > t->ns)
		q1 = t->ns;
	if(q0 > q1)
		q0 = q1;
	return _adrawsmprint("%.*S", q1-q0, t->s+q0);
}

static void
snap(Tagx *t)
{
	char *s;

	s = _adrawsmprint("%.*S", t->ns, t->s);
	lock(&t->sl);
	free(t->snap);
	t->snap = s;
	unlock(&t->sl);
}

static Rectangle
textr(Tagx *t, int nl)
{
	Rectangle r;

	r = t->all;
	r.min.x += scrollwid()+scrollgap();
	r.max.y = r.min.y+nl*t->font->height;
	if(r.max.y > t->all.max.y)
		r.max.y = t->all.max.y;
	return r;
}

static int
lines(Tagx *t)
{
	int n;

	if(!t->expand)
		return 1;
	frclear(t->g, 0);
	frinit(t->g, textr(t, Dy(t->all)/t->font->height), t->font, t->gb, t->f->cols);
	frinsert(t->g, t->s, t->s+t->ns, 0);
	if(t->g->nlines >= t->g->maxlines)
		return t->g->maxlines>0 ? t->g->maxlines : 1;
	n = t->g->nlines;
	if(t->ns>0 && t->s[t->ns-1]=='\n')
		n++;
	if(n == 0)
		n = 1;
	return n;
}

static void
button(Tagx *t)
{
	Rectangle r;

	r.min = t->r.min;
	r.max.x = r.min.x+scrollwid();
	r.max.y = r.min.y+t->font->height+1;
	if(r.max.y > t->r.max.y-1)
		r.max.y = t->r.max.y-1;
	draw(t->b, r, t->f->cols[BACK], nil, ZP);
	border(t->b, r, Border, t->f->cols[BORD], ZP);
	if(t->dirty)
		draw(t->b, insetrect(r, Border), t->mod, nil, ZP);
}

static void
layout(Tagx *t, int force)
{
	Rectangle r, o;
	int nl;

	nl = lines(t);
	r = textr(t, nl);
	o = t->r;
	if(!force && eqrect(r, t->f->r))
		return;
	frclear(t->f, 0);
	frinit(t->f, r, t->font, t->b, t->f->cols);
	t->r = t->all;
	t->r.max.y = r.max.y+1;
	if(t->r.max.y > t->all.max.y)
		t->r.max.y = t->all.max.y;
	draw(t->b, Rect(t->r.min.x, t->r.min.y, t->r.max.x, r.max.y), t->f->cols[BACK], nil, ZP);
	draw(t->b, Rect(t->r.min.x, r.max.y, t->r.max.x, t->r.max.y), t->f->cols[BORD], nil, ZP);
	button(t);
	fill(t);
	setsel(t, t->q0, t->q1);
	if(!force && !eqrect(o, t->r))
		newbody(t);
}

static void
fill(Tagx *t)
{
	if(t->f->nchars < t->ns && !t->f->lastlinefull)
		frinsert(t->f, t->s+t->f->nchars, t->s+t->ns, t->f->nchars);
}

static void
setsel(Tagx *t, uint q0, uint q1)
{
	Frame *f;
	int p0, p1, ticked;

	f = t->f;
	t->q0 = q0;
	t->q1 = q1;
	p0 = q0;
	p1 = q1;
	ticked = 1;
	if(p0 > f->nchars)
		p0 = f->nchars;
	if(p1 > f->nchars){
		ticked = 0;
		p1 = f->nchars;
	}
	if(p0==f->p0 && p1==f->p1){
		if(p0 == p1 && ticked != f->ticked)
			frtick(f, frptofchar(f, p0), ticked);
		return;
	}
	if(f->p1<=p0 || p1<=f->p0 || p0==p1 || f->p1==f->p0){
		frdrawsel(f, frptofchar(f, f->p0), f->p0, f->p1, 0);
		if(p0 != p1 || ticked)
			frdrawsel(f, frptofchar(f, p0), p0, p1, 1);
		goto Return;
	}
	if(p0 < f->p0)
		frdrawsel(f, frptofchar(f, p0), p0, f->p0, 1);
	else if(p0 > f->p0)
		frdrawsel(f, frptofchar(f, f->p0), f->p0, p0, 0);
	if(p1 > f->p1)
		frdrawsel(f, frptofchar(f, f->p1), f->p1, p1, 1);
	else if(p1 < f->p1)
		frdrawsel(f, frptofchar(f, p1), p1, f->p1, 0);
Return:
	f->p0 = p0;
	f->p1 = p1;
}

static void
insert(Tagx *t, uint q, Rune *r, int n)
{
	if(n <= 0)
		return;
	if(q > t->ns)
		q = t->ns;
	t->s = erealloc(t->s, (t->ns+n+1)*sizeof(Rune));
	memmove(t->s+q+n, t->s+q, (t->ns-q)*sizeof(Rune));
	memmove(t->s+q, r, n*sizeof(Rune));
	t->ns += n;
	if(q < t->q1)
		t->q1 += n;
	if(q < t->q0)
		t->q0 += n;
	if(q <= t->f->nchars)
		frinsert(t->f, r, r+n, q);
	snap(t);
	layout(t, 0);
}

static void
delete(Tagx *t, uint q0, uint q1)
{
	uint n, p1;

	if(q1 > t->ns)
		q1 = t->ns;
	if(q0 >= q1)
		return;
	n = q1-q0;
	memmove(t->s+q0, t->s+q1, (t->ns-q1)*sizeof(Rune));
	t->ns -= n;
	if(q0 < t->q0)
		t->q0 -= n < t->q0-q0 ? n : t->q0-q0;
	if(q0 < t->q1)
		t->q1 -= n < t->q1-q0 ? n : t->q1-q0;
	if(q0 < t->f->nchars){
		p1 = q1;
		if(p1 > t->f->nchars)
			p1 = t->f->nchars;
		frdelete(t->f, q0, p1);
	}
	fill(t);
	snap(t);
	layout(t, 0);
}

static char*
readfile(char *name)
{
	char *s;
	int fd, n, m;

	if((fd = open(name, OREAD)) < 0)
		return nil;
	s = nil;
	n = 0;
	for(;;){
		s = erealloc(s, n+8192+1);
		if((m = read(fd, s+n, 8192)) <= 0)
			break;
		n += m;
	}
	close(fd);
	s[n] = 0;
	return s;
}

static void
putsnarf(char *s)
{
	int fd;

	if(s == nil || s[0] == 0)
		return;
	if((fd = open("/dev/snarf", OWRITE|OTRUNC)) < 0)
		return;
	write(fd, s, strlen(s));
	close(fd);
}

static void
cut(Tagx *t, int dosnarf, int docut)
{
	char *s;

	if(t->q0 == t->q1)
		return;
	if(dosnarf){
		s = text(t, t->q0, t->q1);
		putsnarf(s);
		free(s);
	}
	if(docut){
		delete(t, t->q0, t->q1);
		setsel(t, t->q0, t->q0);
	}
}

static void
paste(Tagx *t, int selectall)
{
	char *s;
	Rune *r;
	int n;
	uint q0;

	s = readfile("/dev/snarf");
	if(s == nil || s[0] == 0){
		free(s);
		return;
	}
	cut(t, 0, 1);
	n = utflen(s);
	r = _adrawrunesmprint("%s", s);
	free(s);
	q0 = t->q0;
	insert(t, q0, r, n);
	free(r);
	if(selectall)
		setsel(t, q0, q0+n);
	else
		setsel(t, q0+n, q0+n);
}

static int
clickmatch(Tagx *t, int cl, int cr, int dir, uint *q)
{
	Rune c;
	int nest;

	nest = 1;
	for(;;){
		if(dir > 0){
			if(*q == t->ns)
				break;
			c = readc(t, *q);
			(*q)++;
		}else{
			if(*q == 0)
				break;
			(*q)--;
			c = readc(t, *q);
		}
		if(c == cr){
			if(--nest==0)
				return 1;
		}else if(c == cl)
			nest++;
	}
	return cl=='\n' && nest==1;
}

static Rune left1[] = { '{', '[', '(', '<', 0xab, 0 };
static Rune right1[] = { '}', ']', ')', '>', 0xbb, 0 };
static Rune left2[] = { '\n', 0 };
static Rune left3[] = { '\'', '"', '`', 0 };
static Rune *left[] = { left1, left2, left3, nil };
static Rune *right[] = { right1, left2, left3, nil };

static void
doubleclick(Tagx *t, uint *q0, uint *q1)
{
	int c, i;
	Rune *r, *l, *p;
	uint q;

	for(i=0; left[i]!=nil; i++){
		q = *q0;
		l = left[i];
		r = right[i];
		if(q == 0)
			c = '\n';
		else
			c = readc(t, q-1);
		p = runestrchr(l, c);
		if(p != nil){
			if(clickmatch(t, c, r[p-l], 1, &q))
				*q1 = q-(c!='\n');
			return;
		}
		if(q == t->ns)
			c = '\n';
		else
			c = readc(t, q);
		p = runestrchr(r, c);
		if(p != nil){
			if(clickmatch(t, c, l[p-r], -1, &q)){
				*q1 = *q0+(*q0<t->ns && c=='\n');
				*q0 = q;
				if(c!='\n' || q!=0 || readc(t, 0)=='\n')
					(*q0)++;
			}
			return;
		}
	}
	while(*q1<t->ns && isalnum(readc(t, *q1)))
		(*q1)++;
	while(*q0>0 && isalnum(readc(t, *q0-1)))
		(*q0)--;
}

static void
select1(Tagx *t, Mousectl *mc)
{
	uint q0, q1, selectq;
	int b, x, y, state;
	enum { None, Cut, Paste };

	b = mc->buttons;
	q0 = t->q0;
	q1 = t->q1;
	selectq = frcharofpt(t->f, mc->xy);
	if(t->clicked && mc->msec-t->clickmsec<500)
	if(q0==q1 && selectq==q0){
		doubleclick(t, &q0, &q1);
		setsel(t, q0, q1);
		flushimage(display, 1);
		x = mc->xy.x;
		y = mc->xy.y;
		do
			readmouse(mc);
		while(mc->buttons==b && abs(mc->xy.x-x)<3 && abs(mc->xy.y-y)<3);
		mc->xy.x = x;
		mc->xy.y = y;
		q0 = t->q0;
		q1 = t->q1;
		selectq = q0;
	}
	if(mc->buttons == b){
		t->f->scroll = nil;
		frselect(t->f, mc);
		if(selectq > t->ns)
			selectq = t->f->p0;
		q0 = t->f->p0;
		if(selectq > t->f->nchars)
			q1 = selectq;
		else
			q1 = t->f->p1;
	}
	if(q0 == q1){
		if(q0==t->q0 && t->clicked && mc->msec-t->clickmsec<500){
			doubleclick(t, &q0, &q1);
			t->clicked = 0;
		}else{
			t->clicked = 1;
			t->clickmsec = mc->msec;
		}
	}else
		t->clicked = 0;
	setsel(t, q0, q1);
	flushimage(display, 1);
	state = None;
	while(mc->buttons){
		mc->msec = 0;
		b = mc->buttons;
		if((b&1) && (b&6)){
			if(b & 2){
				if(state != Cut){
					cut(t, 1, 1);
					state = Cut;
				}
			}else{
				if(state != Paste){
					paste(t, 1);
					state = Paste;
				}
			}
		}
		flushimage(display, 1);
		while(mc->buttons == b)
			readmouse(mc);
		t->clicked = 0;
	}
}

static int
region(int a, int b)
{
	if(a < b)
		return -1;
	if(a == b)
		return 0;
	return 1;
}

static uint
xselect(Frame *f, Mousectl *mc, Image *col, uint *p1p)
{
	uint p0, p1, q, tmp;
	ulong msec;
	Point mp, pt0, pt1, qt;
	int reg, b;

	mp = mc->xy;
	b = mc->buttons;
	msec = mc->msec;
	if(f->p0 == f->p1)
		frtick(f, frptofchar(f, f->p0), 0);
	p0 = p1 = frcharofpt(f, mp);
	pt0 = frptofchar(f, p0);
	pt1 = frptofchar(f, p1);
	reg = 0;
	frtick(f, pt0, 1);
	do{
		q = frcharofpt(f, mc->xy);
		if(p1 != q){
			if(p0 == p1)
				frtick(f, pt0, 0);
			if(reg != region(q, p0)){
				if(reg > 0)
					frdrawrange(f, pt0, p0, p1);
				else if(reg < 0)
					frdrawrange(f, pt1, p1, p0);
				p1 = p0;
				pt1 = pt0;
				reg = region(q, p0);
				if(reg == 0)
					frdrawsel0(f, pt0, p0, p1, col, display->white);
			}
			qt = frptofchar(f, q);
			if(reg > 0){
				if(q > p1)
					frdrawsel0(f, pt1, p1, q, col, display->white);
				else if(q < p1)
					frdrawrange(f, qt, q, p1);
			}else if(reg < 0){
				if(q > p1)
					frdrawrange(f, pt1, p1, q);
				else
					frdrawsel0(f, qt, q, p1, col, display->white);
			}
			p1 = q;
			pt1 = qt;
		}
		if(p0 == p1)
			frtick(f, pt0, 1);
		flushimage(f->display, 1);
		readmouse(mc);
	}while(mc->buttons == b);
	if(mc->msec-msec < Delay && p0!=p1
	&& abs(mp.x-mc->xy.x)<Minmove
	&& abs(mp.y-mc->xy.y)<Minmove){
		if(reg > 0)
			frdrawrange(f, pt0, p0, p1);
		else if(reg < 0)
			frdrawrange(f, pt1, p1, p0);
		p1 = p0;
	}
	if(p1 < p0){
		tmp = p0;
		p0 = p1;
		p1 = tmp;
	}
	pt0 = frptofchar(f, p0);
	if(p0 == p1)
		frtick(f, pt0, 0);
	frdrawrange(f, pt0, p0, p1);
	if(f->p0 == f->p1)
		frtick(f, frptofchar(f, f->p0), 1);
	flushimage(f->display, 1);
	*p1p = p1;
	return p0;
}

static int
select23(Tagx *t, Mousectl *mc, uint *q0, uint *q1, Image *high, int mask)
{
	uint p0, p1;
	int buts;

	p0 = xselect(t->f, mc, high, &p1);
	buts = mc->buttons;
	if((buts & mask) == 0){
		*q0 = p0;
		*q1 = p1;
	}
	while(mc->buttons)
		readmouse(mc);
	return buts;
}

static Rune*
parsetag(Tagx *t, int *len)
{
	Rune *r, *p, *pipe;
	int i, n;

	lock(&t->sl);
	r = _adrawrunesmprint("%s", t->snap ? t->snap : "");
	unlock(&t->sl);
	n = runestrlen(r);
	pipe = runestrstr(r, Lspacepipe);
	if((p = runestrstr(r, Ltabpipe)) != nil && (pipe == nil || p < pipe))
		pipe = p;
	if((p = runestrstr(r, Ldelsnarf)) != nil && (pipe == nil || p < pipe))
		i = p - r;
	else
		for(i=0; i<n; i++)
			if(r[i]==' ' || r[i]=='\t')
				break;
	*len = i;
	return r;
}

static char*
dirname(Tagx *t, char *name)
{
	Rune *b;
	char *s, *d;
	int i, slash;

	if(name != nil && name[0] == '/')
		return cleanname(_adrawstrdup(name));
	b = parsetag(t, &i);
	slash = -1;
	for(i--; i >= 0; i--)
		if(b[i] == '/'){
			slash = i;
			break;
		}
	if(slash >= 0)
		d = _adrawsmprint("%.*S", slash+1, b);
	else
		d = nil;
	free(b);
	if(d == nil)
		return name != nil ? cleanname(_adrawstrdup(name)) : nil;
	s = _adrawsmprint("%s%s", d, name != nil ? name : "");
	free(d);
	return cleanname(s);
}

static char*
wdir(Tagx *t)
{
	char *d, buf[1024];

	d = dirname(t, nil);
	if(d == nil || strcmp(d, ".") == 0){
		free(d);
		if(getwd(buf, sizeof buf) == nil)
			return nil;
		d = _adrawstrdup(buf);
	}
	return d;
}

static int
has(Tagx *t, uint q0, Rune *r)
{
	int i;

	if((int)q0 < 0)
		return 0;
	for(i=0; r[i]; i++)
		if(q0+i >= t->ns || readc(t, q0+i) != r[i])
			return 0;
	return 1;
}

static int
expandfile(Tagx *t, uint q0, uint q1, Expand *e)
{
	int i, n, nname, colon;
	uint amin, amax;
	Rune *r, c;
	char *b, *s;

	amax = q1;
	if(q1 == q0){
		colon = -1;
		while(q1<t->ns && isfilec(c=readc(t, q1))){
			if(c == ':' && !has(t, q1-4, Lhttp) && !has(t, q1-5, Lhttps)){
				colon = q1;
				break;
			}
			q1++;
		}
		while(q0>0 && (isfilec(c=readc(t, q0-1)) || isaddrc(c) || isregexc(c))){
			q0--;
			if(colon<0 && c==':' && !has(t, q0-4, Lhttp) && !has(t, q0-5, Lhttps))
				colon = q0;
		}
		if(colon >= 0){
			q1 = colon;
			if(colon<t->ns-1 && isaddrc(readc(t, colon+1))){
				q1 = colon+1;
				while(q1<t->ns && isaddrc(readc(t, q1)))
					q1++;
			}
		}
		if(q1 > q0)
			if(colon >= 0){
				for(amax=colon+1; amax<t->ns; amax++)
					if((c=readc(t, amax))==' ' || c=='\t' || c=='\n')
						break;
			}else
				amax = t->ns;
	}
	amin = amax;
	e->q0 = q0;
	e->q1 = q1;
	e->a0 = -1;
	n = q1-q0;
	if(n == 0)
		return 0;
	r = t->s+q0;
	if(n >= 7 && runestrncmp(r, Lhttp, 7) == 0 || n >= 8 && runestrncmp(r, Lhttps, 8) == 0){
		if(r[n-1] == '.'){
			e->q1--;
			n--;
		}
		e->name = _adrawrunesmprint("%.*S", n, r);
		e->nname = n;
		return 1;
	}
	nname = -1;
	for(i=0; i<n; i++){
		c = r[i];
		if(c==':' && nname<0){
			if(q0+i+1<t->ns && (i==n-1 || isaddrc(readc(t, q0+i+1))))
				amin = q0+i;
			else
				return 0;
			nname = i;
		}
	}
	if(nname == -1)
		nname = n;
	for(i=0; i<nname; i++)
		if(!isfilec(r[i]) && r[i] != ' ')
			return 0;
	s = _adrawsmprint("%.*S", nname, r);
	if(amin == q0){
		b = s;
		s = nil;
	}else{
		b = dirname(t, s);
		free(s);
		if(b == nil || access(b, 0) < 0){
			free(b);
			return 0;
		}
	}
	e->name = _adrawrunesmprint("%s", b);
	e->nname = runestrlen(e->name);
	free(b);
	if(amin < e->q1)
		e->a0 = amin;
	return 1;
}

static int
expand(Tagx *t, uint q0, uint q1, Expand *e)
{
	memset(e, 0, sizeof *e);
	if(q1==q0 && t->q1>t->q0 && t->q0<=q0 && q0<=t->q1){
		q0 = t->q0;
		q1 = t->q1;
	}
	if(expandfile(t, q0, q1, e))
		return 1;
	if(q0 == q1){
		while(q1<t->ns && isalnum(readc(t, q1)))
			q1++;
		while(q0>0 && isalnum(readc(t, q0-1)))
			q0--;
	}
	e->q0 = q0;
	e->q1 = q1;
	e->a0 = -1;
	return q1 > q0;
}

static char*
readall(int fd)
{
	char *s;
	int n, m;

	s = nil;
	n = 0;
	for(;;){
		s = erealloc(s, n+8192+1);
		if((m = read(fd, s+n, 8192)) <= 0)
			break;
		n += m;
	}
	s[n] = 0;
	return s;
}
static char*
cmdname(char *s)
{
	char *e, *n, *p;

	while(*s==' ' || *s=='\t' || *s=='\n')
		s++;
	for(e=s; *e && *e!=' ' && *e!='\t' && *e!='\n'; e++)
		;
	n = _adrawsmprint("%.*s", utfnlen(s, e-s), s);
	if((p = utfrrune(n, '/')) != nil)
		memmove(n, p+1, strlen(p+1)+1);
	return n;
}
static Lock fdlk;
static int *fds;
static int nfds;
static int afds;

static void
addfd(int fd)
{
	if(nfds == afds){
		afds = afds ? 2*afds : 16;
		fds = erealloc(fds, afds*sizeof fds[0]);
	}
	fds[nfds++] = fd;
}

static void
closefd(int fd)
{
	int i;

	if(fd < 0)
		return;
	lock(&fdlk);
	for(i=0; i<nfds; i++)
		if(fds[i] == fd){
			fds[i] = fds[--nfds];
			break;
		}
	close(fd);
	unlock(&fdlk);
}

static int
mkpipe(int p[2])
{
	int r;

	lock(&fdlk);
	if((r = pipe(p)) >= 0){
		addfd(p[0]);
		addfd(p[1]);
	}
	unlock(&fdlk);
	if(r < 0)
		werrstr("pipe: %r");
	return r;
}

static int
cons(void)
{
	int fd;

	if((fd = open("/mnt/acme/cons", OWRITE)) >= 0)
		return fd;
	return dup(2, -1);
}

static void
feednote(void*, char *s)
{
	if(strstr(s, "write on closed pipe") != nil)
		noted(NCONT);
	noted(NDFLT);
}

static void
feedproc(void *v)
{
	Feed *f;

	f = v;
	notify(feednote);
	if(f->s != nil)
		write(f->fd, f->s, strlen(f->s));
	closefd(f->fd);
	free(f->s);
	free(f);
}

static void
xkill(Tagx *t, char *rest, char *arg)
{
	char *f[32], *s, *bad[32];
	int i, n, nbad, found, fd, pids[64], np, j;
	Run *r;

	s = _adrawstrdup(rest != nil && *rest ? rest : arg != nil ? arg : "");
	n = tokenize(s, f, nelem(f));
	np = 0;
	nbad = 0;
	qlock(&t->lk);
	for(r=t->run; r!=nil && np<nelem(pids); r=r->next)
		if(n == 0)
			pids[np++] = r->pid;
	for(i=0; i<n; i++){
		found = 0;
		for(r=t->run; r!=nil && np<nelem(pids); r=r->next)
			if(strcmp(r->name, f[i]) == 0){
				pids[np++] = r->pid;
				found = 1;
			}
		if(!found && nbad<nelem(bad))
			bad[nbad++] = f[i];
	}
	qunlock(&t->lk);
	for(j=0; j<np; j++)
		postnote(PNGROUP, pids[j], "kill");
	if(nbad > 0){
		fd = cons();
		for(i=0; i<nbad; i++)
			fprint(fd, "Kill: no process %s\n", bad[i]);
		close(fd);
	}
	free(s);
}

static Rectangle
rioscreen(void)
{
	char buf[128];
	int fd, n;
	Image *i;
	Rectangle r;

	r = display->image->r;
	if((fd = open("/dev/draw/winname", OREAD)) < 0)
		return r;
	n = read(fd, buf, sizeof buf-1);
	close(fd);
	if(n <= 0)
		return r;
	buf[n] = 0;
	if((i = namedimage(display, buf)) == nil)
		return r;
	r = i->r;
	freeimage(i);
	return r;
}

static void
wctl(Rectangle r, Rectangle s, char *verb)
{
	char err[ERRMAX];
	int fd, e;

	if((fd = open("/dev/wctl", OWRITE)) < 0 || fprint(fd, "%s -r %d %d %d %d", verb, r.min.x-s.min.x, r.min.y-s.min.y, r.max.x-s.min.x, r.max.y-s.min.y) < 0){
		rerrstr(err, sizeof err);
		e = cons();
		fprint(e, "wctl %s %R: %s\n", verb, r, err);
		close(e);
	}
	if(fd >= 0)
		close(fd);
}
static Rectangle
fit(Rectangle w, Rectangle s)
{
	if(Dx(w) > Dx(s))
		w.max.x = w.min.x+Dx(s);
	if(Dy(w) > Dy(s))
		w.max.y = w.min.y+Dy(s);
	if(Dx(w) >= Dx(s) && Dy(w) >= Dy(s))
		w.max.y--;
	if(w.max.y > s.max.y)
		w = rectaddpt(w, Pt(0, s.max.y-w.max.y));
	if(w.min.y < s.min.y)
		w = rectaddpt(w, Pt(0, s.min.y-w.min.y));
	if(w.max.x > s.max.x)
		w = rectaddpt(w, Pt(s.max.x-w.max.x, 0));
	if(w.min.x < s.min.x)
		w = rectaddpt(w, Pt(s.min.x-w.min.x, 0));
	return w;
}
static void
grow(Tagx *t, int but)
{
	Rectangle w, o, s;
	int fh, onl, tl, tot, nnl;

	s = rioscreen();
	o = w = _screen->image->r;
	if(but == 3){
		if(t->filled){
			t->filled = 0;
			w = fit(t->saved, s);
		}else{
			t->saved = w;
			t->filled = 1;
			w = fit(s, s);
		}
	}else if(but == 2){
		w.min.y = s.min.y;
		w.max.y = s.max.y;
		w = fit(w, s);
	}else{
		fh = t->font->height;
		onl = (screen->r.max.y - t->r.max.y)/fh;
		tl = Dy(t->f->r)/fh;
		tot = (Dy(s) - (Dy(w)-Dy(screen->r)) - Dy(t->r))/fh;
		nnl = onl + (tl-1+onl < 5 ? tl-1+onl : 5);
		if(onl/2 > nnl-onl)
			nnl = onl + onl/2;
		if(nnl > tot)
			nnl = tot;
		if(nnl < tl-1+onl)
			nnl = (tl-1+onl + nnl)/2;
		if(nnl <= onl)
			return;
		w.max.y += (nnl-onl)*fh;
		w = fit(w, s);
	}
	if(!eqrect(w, o))
		wctl(w, s, "resize");
}
static Cursor boxcursor = {
	{-7, -7},
	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	 0xFF, 0xFF, 0xF8, 0x1F, 0xF8, 0x1F, 0xF8, 0x1F,
	 0xF8, 0x1F, 0xF8, 0x1F, 0xF8, 0x1F, 0xFF, 0xFF,
	 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
	{0x00, 0x00, 0x7F, 0xFE, 0x7F, 0xFE, 0x7F, 0xFE,
	 0x70, 0x0E, 0x70, 0x0E, 0x70, 0x0E, 0x70, 0x0E,
	 0x70, 0x0E, 0x70, 0x0E, 0x70, 0x0E, 0x70, 0x0E,
	 0x7F, 0xFE, 0x7F, 0xFE, 0x7F, 0xFE, 0x00, 0x00}
};
static void
square(Tagx *t, Mousectl *mc)
{
	Point op, d;
	Rectangle s;
	int b, but;

	b = mc->buttons;
	but = b&1 ? 1 : b&2 ? 2 : 3;
	op = mc->xy;
	setcursor(mc, &boxcursor);
	while(mc->buttons == b)
		readmouse(mc);
	setcursor(mc, nil);
	if(mc->buttons){
		while(mc->buttons)
			readmouse(mc);
		return;
	}
	d = subpt(mc->xy, op);
	if(abs(d.x)<5 && abs(d.y)<5)
		grow(t, but);
	else{
		s = rioscreen();
		wctl(fit(rectaddpt(_screen->image->r, d), s), s, "move");
	}
}
static int
bswidth(Tagx *t, Rune c)
{
	uint q;
	Rune r;
	int skipping, eq;

	if(c == 0x08)
		return 1;
	q = t->q0;
	skipping = 1;
	while(q > 0){
		r = readc(t, q-1);
		if(r == '\n'){
			if(q == t->q0)
				--q;
			break;
		}
		if(c == 0x17){
			eq = isalnum(r);
			if(eq && skipping)
				skipping = 0;
			else if(!eq && !skipping)
				break;
		}
		--q;
	}
	return t->q0-q;
}
static int
filewidth(Tagx *t, uint q0, int one)
{
	uint q;
	Rune r;

	q = q0;
	while(q > 0){
		r = readc(t, q-1);
		if(r <= ' ')
			break;
		if(one && r=='/')
			break;
		--q;
	}
	return q0-q;
}
static Rune*
complete1(Tagx *t)
{
	int i, nstr, npath, fd;
	char *str, *path, *dir, *d, err[ERRMAX];
	Completion *c;
	Rune *rp;

	if(t->q0<t->ns && readc(t, t->q0)>' ')
		return nil;
	nstr = filewidth(t, t->q0, 1);
	npath = filewidth(t, t->q0-nstr, 0);
	str = text(t, t->q0-nstr, t->q0);
	path = text(t, t->q0-nstr-npath, t->q0-nstr);
	if(npath>0 && path[0]=='/')
		dir = _adrawstrdup(path);
	else{
		d = wdir(t);
		dir = cleanname(_adrawsmprint("%s/%s", d ? d : ".", path));
		free(d);
	}
	rp = nil;
	if((c = complete(dir, str)) == nil)
		rerrstr(err, sizeof err);
	fd = cons();
	if(c == nil)
		fprint(fd, "error attempting completion: %s\n", err);
	else{
		if(!c->advance){
			fprint(fd, "%s%s%s*%s\n", dir,
				dir[0] && dir[strlen(dir)-1]!='/' ? "/" : "",
				str, c->nmatch ? "" : ": no matches in:");
			for(i=0; i<c->nfile; i++)
				fprint(fd, " %s\n", c->filename[i]);
		}else
			rp = _adrawrunesmprint("%s", c->string);
		freecompletion(c);
	}
	close(fd);
	free(str);
	free(path);
	free(dir);
	return rp;
}
static void
tagup(Tagx *t, int up)
{
	if(up == !t->expand)
		return;
	t->expand = !up;
	layout(t, 0);
}
static void
newbody(Tagx *t)
{
	int one;

	t->tag->body = Rect(screen->r.min.x, t->r.max.y, screen->r.max.x, screen->r.max.y);
	one = 1;
	nbsend(t->tag->mc->resizec, &one);
}
static Event*
event(int c1, int c2, uint q0, uint q1, uint oq0, uint oq1, int flag, char *s, char *arg)
{
	Event *e;

	e = emalloc(sizeof *e);
	e->c1 = c1;
	e->c2 = c2;
	e->q0 = q0;
	e->q1 = q1;
	e->oq0 = oq0;
	e->oq1 = oq1;
	e->flag = flag;
	if(s != nil && utflen(s) <= EVENTSIZE){
		utfecpy(e->text, e->text+sizeof e->text, s);
		e->nr = utflen(e->text);
		e->nb = strlen(e->text);
	}
	if(arg != nil)
		utfecpy(e->arg, e->arg+sizeof e->arg, arg);
	return e;
}
static char *builtins[] = {
	"Cut", "Del", "Kill", "Paste", "Put", "Redo", "Snarf", "Undo", nil
};
static int
isbuiltin(char *s)
{
	char **b, *w;
	int n;

	while(*s==' ' || *s=='\t' || *s=='\n')
		s++;
	for(n=0; s[n] && s[n]!=' ' && s[n]!='\t' && s[n]!='\n'; n++)
		;
	w = _adrawsmprint("%.*s", utfnlen(s, n), s);
	for(b=builtins; *b; b++)
		if(strcmp(*b, w) == 0)
			break;
	free(w);
	return *b != nil;
}
static char*
getarg(Tagx *t)
{
	Expand e;
	char *a;

	if(expand(t, t->q0, t->q1, &e)){
		if(e.nname){
			a = _adrawsmprint("%S", e.name);
			free(e.name);
			return a;
		}
		free(e.name);
	}else{
		e.q0 = t->q0;
		e.q1 = t->q1;
	}
	return text(t, e.q0, e.q1);
}
static void
execute(Tagx *t, uint q0, uint q1, int chord)
{
	Rune c;
	char *s, *a;
	uint oq0, oq1;
	int f;

	oq0 = q0;
	oq1 = q1;
	if(q1 == q0){
		if(t->q1>t->q0 && t->q0<=q0 && q0<=t->q1){
			q0 = t->q0;
			q1 = t->q1;
		}else{
			while(q1<t->ns && isexecc(c=readc(t, q1)) && c!=':')
				q1++;
			while(q0>0 && isexecc(c=readc(t, q0-1)) && c!=':')
				q0--;
			if(q1 == q0)
				return;
		}
	}
	s = text(t, q0, q1);
	a = chord ? getarg(t) : nil;
	f = 0;
	if(isbuiltin(s))
		f |= 1;
	if(q0!=oq0 || q1!=oq1)
		f |= 2;
	if(chord)
		f |= 8;
	send1(t, event('M', 'x', q0, q1, oq0, oq1, f, s, a));
	free(s);
	free(a);
}
static void
look3(Tagx *t, uint q0, uint q1)
{
	Expand e;
	char *s;
	int f;

	if(!expand(t, q0, q1, &e))
		return;
	if(e.nname)
		s = _adrawsmprint("%S%.*S", e.name, e.a0>=0 ? e.q1-e.a0 : 0, t->s+(e.a0>=0 ? e.a0 : 0));
	else
		s = text(t, e.q0, e.q1);
	f = 0;
	if(q0!=e.q0 || q1!=e.q1)
		f |= 2;
	if(e.nname)
		f |= 4;
	send1(t, event('M', 'l', e.q0, e.q1, q0, q1, f, s, nil));
	free(s);
	free(e.name);
}
static void
plumb1(Tagx *t, uint q0, uint q1)
{
	Plumbmsg *m;
	int fd;
	uint p;
	Rune c;
	char buf[32];

	if(q1 > t->ns)
		q1 = t->ns;
	if(q0 > q1)
		q0 = q1;
	if((fd = plumbopen("send", OWRITE)) < 0)
		return;
	m = emalloc(sizeof *m);
	m->src = _adrawstrdup("acme");
	m->wdir = wdir(t);
	m->type = _adrawstrdup("text");
	if(q1 == q0){
		if(t->q1>t->q0 && t->q0<=q0 && q0<=t->q1){
			q0 = t->q0;
			q1 = t->q1;
		}else{
			p = q0;
			while(q0>0 && (c=readc(t, q0-1))!=' ' && c!='\t' && c!='\n')
				q0--;
			while(q1<t->ns && (c=readc(t, q1))!=' ' && c!='\t' && c!='\n')
				q1++;
			if(q1 == q0){
				plumbfree(m);
				close(fd);
				return;
			}
			snprint(buf, sizeof buf, "click=%d", p-q0);
			m->attr = plumbunpackattr(buf);
		}
	}
	m->data = text(t, q0, q1);
	m->ndata = strlen(m->data);
	plumbsend(fd, m);
	plumbfree(m);
	close(fd);
}
static char*
acmename(int id)
{
	char buf[64], *s, *e;
	int fd, n;

	snprint(buf, sizeof buf, "/mnt/acme/%d/tag", id);
	if((fd = open(buf, OREAD)) < 0)
		return nil;
	s = emalloc(1024);
	n = read(fd, s, 1023);
	close(fd);
	if(n < 0)
		n = 0;
	s[n] = 0;
	for(e=s; *e && *e!=' ' && *e!='\t' && *e!='\n'; e++)
		;
	*e = 0;
	if(s[0] == 0){
		free(s);
		return nil;
	}
	return s;
}
static int
key1(Tagx *t, Rune r)
{
	uint q0;
	int n;
	Rune *rp;

	if(!ptinrect(t->xy, t->r))
		return 0;
	switch(r){
	case Kleft:
		if(t->q0 > 0)
			setsel(t, t->q0-1, t->q0-1);
		goto Out;
	case Kright:
		if(t->q1 < t->ns)
			setsel(t, t->q1+1, t->q1+1);
		goto Out;
	case Kdown:
		tagup(t, 0);
		goto Out;
	case Kup:
		tagup(t, 1);
		goto Out;
	case Khome:
	case Kend:
	case Kpgup:
	case Kpgdown:
		goto Out;
	case 0x01:
		n = 0;
		if(t->q0>0 && readc(t, t->q0-1)!='\n')
			n = bswidth(t, 0x15);
		setsel(t, t->q0-n, t->q0-n);
		goto Out;
	case 0x05:
		q0 = t->q0;
		while(q0<t->ns && readc(t, q0)!='\n')
			q0++;
		setsel(t, q0, q0);
		goto Out;
	case 0x03:
		cut(t, 1, 0);
		goto Out;
	case 0x1A:
		send1(t, event('K', 'x', 0, 0, 0, 0, 1, "Undo", nil));
		goto Out;
	case 0x12:
		send1(t, event('K', 'x', 0, 0, 0, 0, 1, "Redo", nil));
		goto Out;
	case 0x18:
		cut(t, 1, 1);
		goto Out;
	case 0x16:
		paste(t, 1);
		goto Out;
	}
	if(r == 0x08 && t->q1 > t->q0){
		cut(t, 0, 1);
		t->eq0 = ~0;
		goto Out;
	}
	if(t->q1 > t->q0){
		cut(t, 1, 1);
		t->eq0 = ~0;
	}
	switch(r){
	case 0x1B:
		if(t->eq0 != ~0){
			if(t->eq0 <= t->q0)
				setsel(t, t->eq0, t->q0);
			else
				setsel(t, t->q0, t->eq0);
		}
		goto Out;
	case 0x06:
	case Kins:
		if((rp = complete1(t)) == nil)
			goto Out;
		n = runestrlen(rp);
		q0 = t->q0;
		if(t->eq0 == ~0)
			t->eq0 = q0;
		insert(t, q0, rp, n);
		setsel(t, q0+n, q0+n);
		free(rp);
		goto Out;
	case 0x08:
	case 0x15:
	case 0x17:
		if(t->q0 == 0)
			goto Out;
		n = bswidth(t, r);
		q0 = t->q0-n;
		if(n <= 0)
			goto Out;
		if(t->eq0 == t->q0 || t->eq0 == ~0)
			t->eq0 = q0;
		delete(t, q0, q0+n);
		setsel(t, q0, q0);
		goto Out;
	case '\n':
		t->expand = 1;
		break;
	}
	q0 = t->q0;
	if(t->eq0 == ~0)
		t->eq0 = q0;
	insert(t, q0, &r, 1);
	setsel(t, q0+1, q0+1);
Out:
	flushimage(display, 1);
	return 1;
}
static void
send1(Tagx *t, Event *e)
{
	if(t->closed || t->quitting || give(t, t->tag->c, &e) < 0)
		free(e);
}

static void
runproc(void *v)
{
	Job *j;
	char buf[32], err[ERRMAX];
	int i, n, *o;

	j = v;
	lock(&fdlk);
	rfork(RFFDG|RFENVG|RFNOTEG);
	n = nfds;
	o = emalloc((n+1)*sizeof o[0]);
	memmove(o, fds, n*sizeof o[0]);
	unlock(&fdlk);
	dup(j->fd[0], 0);
	dup(j->fd[1], 1);
	dup(j->fd[2], 2);
	for(i=0; i<3; i++)
		if(j->fd[i] > 2)
			close(j->fd[i]);
	for(i=0; i<n; i++)
		if(o[i] > 2)
			close(o[i]);
	free(o);
	if(j->winid > 0){
		snprint(buf, sizeof buf, "%d", j->winid);
		putenv("winid", buf);
	}
	if(j->file != nil){
		putenv("%", j->file);
		putenv("samfile", j->file);
	}
	if(j->dir != nil)
		chdir(j->dir);
	procexecl(j->c, "/bin/rc", "rc", "-c", j->cmd, nil);
	rerrstr(err, sizeof err);
	sendp(j->errc, _adrawstrdup(err));
	threadexits("exec");
}

typedef struct Watch Watch;
struct Watch
{
	Dead	*d;
	int	pid;
	char	*name;
};

static void	edit(Tagx*, int, char*);

static void
deadfree(Dead *d)
{
	Exit *x;

	if(decref(&d->ref) > 0)
		return;
	while((x = nbrecvp(d->c)) != nil){
		free(x->name);
		free(x);
	}
	chanfree(d->c);
	free(d);
}

static void
watchproc(void *v)
{
	Watch *w;
	Exit *x;
	char buf[32];

	w = v;
	snprint(buf, sizeof buf, "/proc/%d/status", w->pid);
	while(access(buf, AEXIST) >= 0)
		sleep(250);
	x = emalloc(sizeof *x);
	x->pid = w->pid;
	x->name = w->name;
	if(nbsendp(w->d->c, x) == 0){
		free(x->name);
		free(x);
	}
	deadfree(w->d);
	free(w);
}

static int
spawn(Tagx *t, char *cmd, int fd0, int fd1, int fd2)
{
	Job j;
	Run *r;
	Watch *w;
	char *err;
	int i, pid;

	memset(&j, 0, sizeof j);
	j.fd[0] = fd0;
	j.fd[1] = fd1;
	j.fd[2] = fd2;
	j.cmd = cmd;
	j.dir = wdir(t);
	lock(&t->sl);
	j.winid = t->winid;
	unlock(&t->sl);
	j.file = j.winid > 0 ? acmename(j.winid) : nil;
	j.c = chancreate(sizeof(ulong), 0);
	j.errc = chancreate(sizeof(char*), 0);
	proccreate(runproc, &j, 16384);
	pid = recvul(j.c);
	err = pid == ~0 ? recvp(j.errc) : nil;
	chanfree(j.c);
	chanfree(j.errc);
	for(i=0; i<3; i++)
		if(i == 0 || j.fd[i] != j.fd[i-1])
			closefd(j.fd[i]);
	free(j.dir);
	free(j.file);
	if(pid == ~0){
		i = cons();
		fprint(i, "exec rc: %s\n", err);
		close(i);
		werrstr("exec rc: %s", err);
		free(err);
		return -1;
	}
	r = emalloc(sizeof *r);
	r->pid = pid;
	r->name = cmdname(cmd);
	qlock(&t->lk);
	r->next = t->run;
	t->run = r;
	qunlock(&t->lk);
	incref(&t->dead->ref);
	w = emalloc(sizeof *w);
	w->d = t->dead;
	w->pid = pid;
	w->name = _adrawstrdup(r->name);
	edit(t, 'a', _adrawstrdup(r->name));
	proccreate(watchproc, w, 8192);
	return pid;
}

static void
run(Tagx *t, char *s, char *arg)
{
	char *q, *c;
	int fd;

	if(arg != nil){
		q = _adrawquote(arg);
		c = _adrawsmprint("%s %s", s, q);
		free(q);
	}else
		c = _adrawstrdup(s);
	fd = cons();
	spawn(t, c, open("/dev/null", OREAD), fd, dup(fd, -1));
	free(c);
}

static void
del(Tagx *t, int hangup)
{
	if(t->closed)
		return;
	lock(&t->sl);
	t->closed = 1;
	unlock(&t->sl);
	t->hangup = hangup;
	t->nilwant = 1;
}

static int
closed(Tagx *t)
{
	int c;

	lock(&t->sl);
	c = t->closed;
	unlock(&t->sl);
	return c;
}

static void
dflt(Tagx *t, Event *e)
{
	char *s, *w, *r, *a;
	int n, fd;

	if(e->c2 == 'l' || e->c2 == 'L'){
		plumb1(t, e->q0, e->q1);
		return;
	}
	if(e->c2 != 'x' && e->c2 != 'X')
		return;
	if(e->nr > 0)
		s = _adrawstrdup(e->text);
	else
		s = text(t, e->q0, e->q1);
	a = (e->flag&8) ? e->arg : nil;
	w = s;
	while(*w==' ' || *w=='\t' || *w=='\n')
		w++;
	for(n=0; w[n] && w[n]!=' ' && w[n]!='\t' && w[n]!='\n'; n++)
		;
	r = w+n;
	while(*r==' ' || *r=='\t' || *r=='\n')
		r++;
	w = _adrawsmprint("%.*s", utfnlen(w, n), w);
	if(strcmp(w, "Snarf") == 0)
		cut(t, 1, 0);
	else if(strcmp(w, "Cut") == 0)
		cut(t, 1, 1);
	else if(strcmp(w, "Paste") == 0)
		paste(t, 1);
	else if(strcmp(w, "Kill") == 0)
		xkill(t, r, a);
	else if(strcmp(w, "Del") == 0)
		del(t, 1);
	else if(strcmp(w, "Put") == 0 || strcmp(w, "Undo") == 0 || strcmp(w, "Redo") == 0)
		;
	else if(w[0]=='|' || w[0]=='<' || w[0]=='>'){
		free(w);
		w = cmdname(s+strspn(s, " \t\n|<>"));
		fd = cons();
		fprint(fd, "%s: no selection\n", w);
		close(fd);
	}else
		run(t, s, a);
	free(w);
	free(s);
	flushimage(display, 1);
}

static void
mouse1(Tagx *t, Mousectl *mc)
{
	Rectangle br;
	uint q0, q1;
	int b, chord;

	b = mc->buttons;
	t->eq0 = ~0;
	if(b & (8|16)){
		tagup(t, (b&8) != 0);
		return;
	}
	br = t->r;
	br.max.x = br.min.x+scrollwid();
	if(ptinrect(mc->xy, br)){
		square(t, mc);
		return;
	}
	if(mc->xy.y >= t->f->r.max.y){
		while(mc->buttons)
			readmouse(mc);
		return;
	}
	if(b & 1)
		select1(t, mc);
	else if(b & 2){
		b = select23(t, mc, &q0, &q1, t->but2, 4);
		if((b & 4) == 0){
			chord = (b & 1) != 0;
			execute(t, q0, q1, chord);
		}
	}else if(b & 4){
		if(select23(t, mc, &q0, &q1, t->but3, 1|2) == 0)
			look3(t, q0, q1);
	}else
		while(mc->buttons)
			readmouse(mc);
	flushimage(display, 1);
}

static void
setname(Tagx *t, char *name)
{
	Rune *r, *o;
	int n;

	o = parsetag(t, &n);
	free(o);
	r = _adrawrunesmprint("%s", name);
	delete(t, 0, n);
	insert(t, 0, r, runestrlen(r));
	free(r);
}

static int
okctl(char *s)
{
	ulong bg, fg;

	return strncmp(s, "name ", 5) == 0 || strcmp(s, "clean") == 0 || strcmp(s, "dirty") == 0
		|| strcmp(s, "cleartag") == 0 || strcmp(s, "del") == 0 || strncmp(s, "winid ", 6) == 0
		|| (strncmp(s, "color", 5) == 0 && (s[5] == 0 || s[5] == ' ') && colorargs(s+5, &bg, &fg) >= 0);
}

static void
ctl1(Tagx *t, char *s)
{
	int i;

	if(strncmp(s, "name ", 5) == 0)
		setname(t, s+5);
	else if(strcmp(s, "clean") == 0 || strcmp(s, "dirty") == 0){
		t->dirty = s[0] == 'd';
		button(t);
	}else if(strcmp(s, "cleartag") == 0){
		for(i=0; i<t->ns; i++)
			if(t->s[i] == '|')
				break;
		if(i < t->ns)
			delete(t, i+1, t->ns);
	}else if(strcmp(s, "del") == 0)
		del(t, 0);
	else if(strncmp(s, "winid ", 6) == 0){
		lock(&t->sl);
		t->winid = atoi(s+6);
		unlock(&t->sl);
	}
	else if(strncmp(s, "color", 5) == 0)
		setcolors(t, s+5);
}

static uint
bar(Tagx *t)
{
	uint i;

	for(i=0; i<t->ns; i++)
		if(t->s[i] == '|'){
			i++;
			if(i < t->ns && t->s[i] == ' ')
				i++;
			return i;
		}
	return t->ns;
}

static void
tagcmd(Tagx *t, char *name)
{
	Rune *r;
	uint q;

	q = bar(t);
	if(q == t->ns && (q == 0 || t->s[q-1] != ' '))
		r = _adrawrunesmprint(" %s ", name);
	else
		r = _adrawrunesmprint("%s ", name);
	insert(t, q, r, runestrlen(r));
	free(r);
}

static void
untagcmd(Tagx *t, char *name)
{
	Rune *r;
	uint q, n;

	r = _adrawrunesmprint("%s", name);
	n = runestrlen(r);
	for(q=bar(t); q+n<=t->ns; q++)
		if((q==0 || t->s[q-1]==' ') && runestrncmp(t->s+q, r, n) == 0
		&& (q+n==t->ns || t->s[q+n]==' ')){
			delete(t, q, q+n<t->ns ? q+n+1 : q+n);
			break;
		}
	free(r);
}

static void
apply(Tagx *t, Edit *e)
{
	Rune *r;

	if(!t->closed){
		if(e->op == 'w'){
			r = _adrawrunesmprint("%s", e->s);
			insert(t, t->ns, r, runestrlen(r));
			free(r);
		}else if(e->op == 'a')
			tagcmd(t, e->s);
		else
			ctl1(t, e->s);
		flushimage(display, 1);
	}
	free(e->s);
	free(e);
}

static void
edit(Tagx *t, int op, char *s)
{
	Edit *e;

	e = emalloc(sizeof *e);
	e->op = op;
	e->s = s;
	if(t->pid == getpid() && t->busy == 0 && (op != 'c' || strcmp(s, "del") != 0))
		apply(t, e);
	else
		sendp(t->ec, e);
}

static void
drain(Tagx *t)
{
	Edit *e;

	while((e = nbrecvp(t->ec)) != nil)
		apply(t, e);
}

static void
resizer(void *v)
{
	Tagx *t;
	int one;
	Alt a[] = {
		{nil, nil, CHANRCV},
		{nil, nil, CHANRCV},
		{nil, nil, CHANEND},
	};

	t = v;
	threadsetname("tagresize");
	a[0].c = t->mc->resizec;
	a[1].c = t->rzq;
	while(alt(a) == 0){
		one = 1;
		nbsend(t->rz, &one);
	}
	sendul(t->rzdone, 1);
	threadexits(nil);
}

static int
give(Tagx *t, Channel *c, void *v)
{
	Alt a[] = {
		{nil, nil, CHANSND},
		{nil, nil, CHANRCV},
		{nil, nil, CHANEND},
	};

	a[0].c = c;
	a[0].v = v;
	a[1].c = t->quit;
	a[1].v = &t->donec;
	if(alt(a) == 1){
		t->quitting = 1;
		return -1;
	}
	return 0;
}

static void
exited(Tagx *t, Exit *x)
{
	Run *r, **l;

	qlock(&t->lk);
	for(l=&t->run; (r=*l) != nil; )
		if(r->pid == x->pid){
			*l = r->next;
			free(r->name);
			free(r);
		}else
			l = &r->next;
	qunlock(&t->lk);
	if(!t->closed){
		untagcmd(t, x->name);
		flushimage(display, 1);
	}
	free(x->name);
	free(x);
}

static void
shutdown(Tagx *t)
{
	Tag *tg;
	Channel *done;
	Event *e;
	Edit *ed;
	Run *r;

	tg = t->tag;
	done = t->donec;
	lock(&t->sl);
	t->closed = 1;
	unlock(&t->sl);
	sendul(t->rzq, 1);
	recvul(t->rzdone);
	while((e = nbrecvp(tg->c)) != nil)
		free(e);
	if(!t->nilsent)
		nbsendp(tg->c, nil);
	while((e = nbrecvp(t->wc)) != nil)
		free(e);
	while((ed = nbrecvp(t->ec)) != nil){
		free(ed->s);
		free(ed);
	}
	frclear(t->f, 1);
	frclear(t->g, 1);
	freecols(t->f->cols);
	freeimage(t->gb);
	freeimage(t->but2);
	freeimage(t->but3);
	freeimage(t->mod);
	while((r = t->run) != nil){
		t->run = r->next;
		free(r->name);
		free(r);
	}
	deadfree(t->dead);
	chanfree(tg->c);
	chanfree(tg->mc->c);
	chanfree(tg->mc->resizec);
	chanfree(tg->kc->c);
	chanfree(t->wc);
	chanfree(t->ec);
	chanfree(t->rz);
	chanfree(t->rzq);
	chanfree(t->rzdone);
	chanfree(t->quit);
	free(t->f);
	free(t->g);
	free(t->s);
	free(t->snap);
	free(tg->mc);
	free(tg->kc);
	free(tg);
	free(t);
	sendul(done, 1);
	threadexits(nil);
}

static void
loop(void *v)
{
	Tagx *t;
	Tag *tg;
	Mouse m;
	Rune r;
	Event *e, *nilp;
	Edit *ed;
	Exit *x;
	enum { Amouse, Aresize, Akey, Awrite, Aedit, Adead, Anil, Aquit };
	Alt a[] = {
		{nil, nil, CHANRCV},
		{nil, nil, CHANRCV},
		{nil, nil, CHANRCV},
		{nil, nil, CHANRCV},
		{nil, nil, CHANRCV},
		{nil, nil, CHANRCV},
		{nil, nil, CHANNOP},
		{nil, nil, CHANRCV},
		{nil, nil, CHANEND},
	};

	t = v;
	tg = t->tag;
	threadsetname("tag");
	nilp = nil;
	a[Amouse].c = t->mc->c;
	a[Amouse].v = &t->mc->Mouse;
	a[Aresize].c = t->rz;
	a[Akey].c = t->kc->c;
	a[Akey].v = &r;
	a[Awrite].c = t->wc;
	a[Awrite].v = &e;
	a[Aedit].c = t->ec;
	a[Aedit].v = &ed;
	a[Adead].c = t->dead->c;
	a[Adead].v = &x;
	a[Anil].c = tg->c;
	a[Anil].v = &nilp;
	a[Aquit].c = t->quit;
	a[Aquit].v = &t->donec;
	while(!t->quitting){
		a[Anil].op = t->nilwant && !t->nilsent ? CHANSND : CHANNOP;
		switch(alt(a)){
		case Amouse:
			m = t->mc->Mouse;
			t->xy = m.xy;
			if(t->owner == 0 && m.buttons)
				t->owner = !t->closed && ptinrect(m.xy, t->r) ? 1 : 2;
			if(t->owner == 1){
				if(m.buttons){
					t->busy++;
					mouse1(t, t->mc);
					t->busy--;
					drain(t);
				}
				if(t->mc->buttons == 0)
					t->owner = 0;
				break;
			}
			if(m.buttons == 0)
				t->owner = 0;
			tg->mc->Mouse = m;
			give(t, tg->mc->c, &m);
			break;
		case Aresize:
			if(getwindow(display, Refnone) < 0)
				sysfatal("getwindow: %r");
			t->all = screen->r;
			if(!t->closed)
				layout(t, 1);
			newbody(t);
			flushimage(display, 1);
			break;
		case Akey:
			if(!t->closed && key1(t, r))
				break;
			give(t, tg->kc->c, &r);
			break;
		case Awrite:
			if(!t->closed){
				t->busy++;
				dflt(t, e);
				t->busy--;
				drain(t);
			}
			free(e);
			break;
		case Aedit:
			apply(t, ed);
			break;
		case Adead:
			exited(t, x);
			break;
		case Anil:
			t->nilsent = 1;
			if(t->hangup){
				postnote(PNGROUP, getpid(), "hangup");
				postnote(PNPROC, getpid(), "hangup");
			}
			break;
		case Aquit:
			t->quitting = 1;
			break;
		}
	}
	shutdown(t);
}

Tag*
tagopen(Mousectl *mc, Keyboardctl *kc, char *name)
{
	Tag *tg;
	Tagx *t;
	Image *cols[NCOL], *im[4];
	int i, n;

	themecols(cols);
	im[0] = allocimage(display, Rect(0,0,1,1), screen->chan, 0, DWhite);
	im[1] = colimage(themecolor("but2", 0xAA0000FF));
	im[2] = colimage(themecolor("but3", 0x006600FF));
	im[3] = colimage(themecolor("modbutton", DMedblue));
	n = 0;
	for(i=0; i<NCOL; i++)
		n += cols[i] == nil;
	for(i=0; i<nelem(im); i++)
		n += im[i] == nil;
	if(n > 0){
		for(i=0; i<NCOL; i++)
			if(cols[i] != display->black)
				freeimage(cols[i]);
		for(i=0; i<nelem(im); i++)
			freeimage(im[i]);
		werrstr("tag colors: %r");
		return nil;
	}
	tg = emalloc(sizeof *tg);
	t = emalloc(sizeof *t);
	tg->x = t;
	t->tag = tg;
	t->mc = mc;
	t->kc = kc;
	t->pid = getpid();
	t->eq0 = ~0;
	tg->c = chancreate(sizeof(Event*), 32);
	tg->mc = emalloc(sizeof *tg->mc);
	*tg->mc = *mc;
	tg->mc->c = chancreate(sizeof(Mouse), 0);
	tg->mc->resizec = chancreate(sizeof(int), 2);
	tg->kc = emalloc(sizeof *tg->kc);
	*tg->kc = *kc;
	tg->kc->c = chancreate(sizeof(Rune), 20);
	t->wc = chancreate(sizeof(Event*), 32);
	t->ec = chancreate(sizeof(Edit*), 64);
	t->rz = chancreate(sizeof(int), 1);
	t->rzq = chancreate(sizeof(ulong), 1);
	t->rzdone = chancreate(sizeof(ulong), 0);
	t->quit = chancreate(sizeof(Channel*), 1);
	t->dead = emalloc(sizeof *t->dead);
	t->dead->ref.ref = 1;
	t->dead->c = chancreate(sizeof(Exit*), 256);
	t->b = screen;
	t->font = font;
	t->all = screen->r;
	t->expand = 1;
	t->f = emalloc(sizeof(Frame));
	t->g = emalloc(sizeof(Frame));
	memmove(t->f->cols, cols, sizeof cols);
	t->gb = im[0];
	t->but2 = im[1];
	t->but3 = im[2];
	t->mod = im[3];
	t->s = _adrawrunesmprint("%s Del Snarf |", name ? name : "");
	t->ns = runestrlen(t->s);
	t->q0 = t->q1 = t->ns;
	snap(t);
	t->r = t->all;
	frinit(t->f, textr(t, 1), t->font, t->b, cols);
	layout(t, 1);
	tg->body = Rect(screen->r.min.x, t->r.max.y, screen->r.max.x, screen->r.max.y);
	flushimage(display, 1);
	threadcreate(resizer, t, 8192);
	threadcreate(loop, t, 32768);
	return tg;
}

void
tagclose(Tag *tg)
{
	Channel *done;

	done = chancreate(sizeof(ulong), 0);
	sendp(tg->x->quit, done);
	recvul(done);
	chanfree(done);
}

int
tagreadevent(Tag *tg, Event *e)
{
	Event *p;

	if((p = recvp(tg->c)) == nil)
		return 0;
	*e = *p;
	free(p);
	return 1;
}

int
tagwriteevent(Tag *tg, Event *e)
{
	Event *p;

	if(closed(tg->x)){
		werrstr("tag closed");
		return -1;
	}
	p = emalloc(sizeof *p);
	*p = *e;
	sendp(tg->x->wc, p);
	return 1;
}

int
tagctl(Tag *tg, char *fmt, ...)
{
	va_list arg;
	char *s, *p, *e;
	int r;

	if(closed(tg->x)){
		werrstr("tag closed");
		return -1;
	}
	va_start(arg, fmt);
	s = _adrawvsmprint(fmt, arg);
	va_end(arg);
	r = 0;
	for(p=s; *p; p=e){
		if((e = strchr(p, '\n')) != nil)
			*e++ = 0;
		else
			e = p+strlen(p);
		if(*p == 0)
			continue;
		if(!okctl(p)){
			werrstr("unknown control message %q", p);
			r = -1;
			continue;
		}
		edit(tg->x, 'c', _adrawstrdup(p));
	}
	free(s);
	return r;
}

int
tagwrite(Tag *tg, char *file, void *buf, int n)
{
	char *s;

	if(closed(tg->x)){
		werrstr("tag closed");
		return -1;
	}
	if(strcmp(file, "tag") != 0){
		werrstr("no file %s", file);
		return -1;
	}
	s = emalloc(n+1);
	memmove(s, buf, n);
	edit(tg->x, 'w', s);
	return n;
}

char*
tagmread(Tag *tg, char *file)
{
	Tagx *t;
	char *s;

	t = tg->x;
	if(closed(t)){
		werrstr("tag closed");
		return nil;
	}
	if(strcmp(file, "tag") != 0){
		werrstr("no file %s", file);
		return nil;
	}
	lock(&t->sl);
	s = _adrawstrdup(t->snap ? t->snap : "");
	unlock(&t->sl);
	return s;
}

char*
tagpipe(Tag *tg, char *cmd, char *in)
{
	Feed *f;
	int p0[2], p1[2], fd;
	char *s;

	if(mkpipe(p1) < 0)
		return nil;
	if(in != nil){
		if(mkpipe(p0) < 0){
			closefd(p1[0]);
			closefd(p1[1]);
			return nil;
		}
	}else{
		p0[0] = open("/dev/null", OREAD);
		p0[1] = -1;
	}
	fd = cons();
	if(spawn(tg->x, cmd, p0[0], p1[1], fd) < 0){
		closefd(p0[1]);
		closefd(p1[0]);
		return nil;
	}
	if(p0[1] >= 0){
		f = emalloc(sizeof *f);
		f->fd = p0[1];
		f->s = _adrawstrdup(in);
		proccreate(feedproc, f, 16384);
	}
	s = readall(p1[0]);
	closefd(p1[0]);
	return s;
}
