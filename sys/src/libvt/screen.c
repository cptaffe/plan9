#include <u.h>
#include <libc.h>
#include <vt.h>
#include "impl.h"

static Vtline*
newlines(int cols, int rows)
{
	Vtline *l;
	int i;

	l = mallocz(rows*sizeof l[0], 1);
	for(i = 0; i < rows; i++)
		l[i].c = mallocz(cols*sizeof(Vtcell), 1);
	return l;
}

static void
freelines(Vtline *l, int rows)
{
	int i;

	if(l == nil)
		return;
	for(i = 0; i < rows; i++)
		free(l[i].c);
	free(l);
}

void
_vtblank(Vt *vt, Vtcell *c, int n)
{
	Priv *p;
	int i;

	p = P(vt);
	for(i = 0; i < n; i++){
		c[i].r = ' ';
		c[i].fg = Vdefault;
		c[i].bg = p->pen.bg;
		c[i].attr = 0;
		c[i].link = 0;
	}
}

static void
blanklines(Vt *vt, Vtline *l, int rows)
{
	int i;

	for(i = 0; i < rows; i++){
		_vtblank(vt, l[i].c, vt->cols);
		l[i].wrapped = 0;
	}
}

/* _vtbegin starts a change: the dirty rows the last frame gave are cleared */
void
_vtbegin(Vt *vt)
{
	Priv *p;

	p = P(vt);
	if(p->cleardirty){
		memset(p->dirty, 0, vt->rows);
		p->cleardirty = 0;
	}
}

void
_vttouch(Vt *vt, int y)
{
	if(y >= 0 && y < vt->rows)
		P(vt)->dirty[y] = 1;
}

void
_vttouchall(Vt *vt)
{
	P(vt)->all = 1;
}

static void
deftabs(Vt *vt, int from)
{
	int i;

	for(i = from; i < vt->cols; i++)
		P(vt)->tabs[i] = i%8 == 0 && i > 0;
}

/* gone keeps a line that left the top of the main screen, swapping its cells for a blank row */
static void
gone(Vt *vt, Vtline *l)
{
	Priv *p;
	Vtline *r;
	Vtcell *c;

	p = P(vt);
	if(p->ngone == Vgone){
		p->head = (p->head+1) % Vgone;
		p->ngone--;
		p->lost++;
	}
	r = &p->ring[(p->head+p->ngone) % Vgone];
	p->ngone++;
	if(r->c == nil)
		r->c = mallocz(vt->cols*sizeof(Vtcell), 1);
	c = r->c;
	r->c = l->c;
	r->wrapped = l->wrapped;
	l->c = c;
	l->wrapped = 0;
}

static void
damage(Vt *vt, int top, int bot, int n)
{
	Priv *p;
	Vtscroll *s;
	int i, h;

	p = P(vt);
	if(p->all)
		return;
	h = bot - top;
	if(n >= h || -n >= h){
		for(i = top; i < bot; i++)
			p->dirty[i] = 1;
		return;
	}
	if(n > 0){
		memmove(p->dirty+top, p->dirty+top+n, h-n);
		memset(p->dirty+bot-n, 1, n);
	}else{
		memmove(p->dirty+top-n, p->dirty+top, h+n);
		memset(p->dirty+top, 1, -n);
	}
	if(p->nscroll > 0){
		s = &p->scroll[p->nscroll-1];
		if(s->top == top && s->bot == bot && (s->n > 0) == (n > 0) && s->n+n <= h && -(s->n+n) <= h){
			s->n += n;
			return;
		}
	}
	if(p->nscroll == p->ascroll){
		p->ascroll = p->ascroll ? 2*p->ascroll : 16;
		p->scroll = realloc(p->scroll, p->ascroll*sizeof p->scroll[0]);
	}
	s = &p->scroll[p->nscroll++];
	s->top = top;
	s->bot = bot;
	s->n = n;
}

/* _vtscroll moves rows top to bot-1 up by n, or down by -n; keep says the lines that leave the top go to gone */
void
_vtscroll(Vt *vt, int top, int bot, int n, int keep)
{
	Vtline t;
	int i, h;

	h = bot - top;
	if(n == 0 || h <= 0)
		return;
	if(n > h)
		n = h;
	if(n < -h)
		n = -h;
	keep = keep && top == 0 && vt->line == P(vt)->main;
	if(n > 0)
		for(i = 0; i < n; i++){
			if(keep)
				gone(vt, &vt->line[top]);
			t = vt->line[top];
			memmove(vt->line+top, vt->line+top+1, (h-1)*sizeof t);
			vt->line[bot-1] = t;
			_vtblank(vt, t.c, vt->cols);
			vt->line[bot-1].wrapped = 0;
		}
	else
		for(i = 0; i < -n; i++){
			t = vt->line[bot-1];
			memmove(vt->line+top+1, vt->line+top, (h-1)*sizeof t);
			vt->line[top] = t;
			_vtblank(vt, t.c, vt->cols);
			vt->line[top].wrapped = 0;
		}
	damage(vt, top, bot, n);
}

Vt*
vtnew(int cols, int rows)
{
	Vt *vt;
	Priv *p;

	if(cols < 1)
		cols = 1;
	if(rows < 1)
		rows = 1;
	vt = mallocz(sizeof *vt, 1);
	p = mallocz(sizeof *p, 1);
	vt->priv = p;
	vt->cols = cols;
	vt->rows = rows;
	p->main = newlines(cols, rows);
	p->alt = newlines(cols, rows);
	p->dirty = mallocz(rows, 1);
	p->tabs = mallocz(cols, 1);
	vt->line = p->main;
	vt->title = strdup("");
	vtreset(vt);
	return vt;
}

Vt*
vtdumb(void (*put)(Vt*, Rune*, int))
{
	Vt *vt;

	vt = vtnew(1, 1);
	P(vt)->dumb = put;
	return vt;
}

void
vtfree(Vt *vt)
{
	Priv *p;
	int i;

	if(vt == nil)
		return;
	p = P(vt);
	freelines(p->main, vt->rows);
	freelines(p->alt, vt->rows);
	for(i = 0; i < Vgone; i++)
		free(p->ring[i].c);
	for(i = 1; i <= vt->nlinks; i++)
		free(vt->links[i]);
	free(vt->links);
	free(vt->title);
	free(p->dirty);
	free(p->tabs);
	free(p->scroll);
	free(p);
	free(vt);
}

void
_vtsoftreset(Vt *vt)
{
	Priv *p;

	p = P(vt);
	vt->mode = (vt->mode & (Malt|Mmouse|Mmousedrag|Mmouseall|Msgr|Mfocus|Mbracket)) | Mcursor | Mwrap;
	p->top = 0;
	p->bot = vt->rows;
	memset(&p->pen, 0, sizeof p->pen);
	p->g[0] = p->g[1] = 0;
	p->shift = 0;
	p->pending = 0;
	memset(p->saved, 0, sizeof p->saved);
	p->saved[0].wrap = p->saved[1].wrap = 1;
}

void
vtreset(Vt *vt)
{
	Priv *p;

	p = P(vt);
	_vtbegin(vt);
	vt->mode = 0;
	_vtsoftreset(vt);
	vt->line = p->main;
	blanklines(vt, p->main, vt->rows);
	blanklines(vt, p->alt, vt->rows);
	vt->x = vt->y = 0;
	deftabs(vt, 0);
	p->state = Ground;
	p->nutf = 0;
	p->last = 0;
	_vttouchall(vt);
}

static void
resizelines(Vt *vt, Vtline **lp, int cols, int rows)
{
	Vtline *l, *o;
	int i, n, oc;

	o = *lp;
	oc = vt->cols;
	l = newlines(cols, rows);
	n = rows < vt->rows ? rows : vt->rows;
	for(i = 0; i < rows; i++){
		_vtblank(vt, l[i].c, cols);
		if(i < n){
			memmove(l[i].c, o[i].c, (cols < oc ? cols : oc)*sizeof(Vtcell));
			l[i].wrapped = o[i].wrapped;
			if(cols < oc && (l[i].c[cols-1].attr & Vwide)){
				l[i].c[cols-1].r = ' ';
				l[i].c[cols-1].attr &= ~Vwide;
			}
		}
	}
	freelines(o, vt->rows);
	*lp = l;
}

void
vtresize(Vt *vt, int cols, int rows)
{
	Priv *p;
	int i, n, oc, alt;

	p = P(vt);
	if(cols < 1)
		cols = 1;
	if(rows < 1)
		rows = 1;
	if(p->dumb || cols == vt->cols && rows == vt->rows)
		return;
	_vtbegin(vt);
	alt = vt->line == p->alt;
	if(vt->y >= rows){
		n = vt->y - rows + 1;
		_vtscroll(vt, 0, vt->rows, n, 1);
		vt->y -= n;
	}
	for(i = 0; i < Vgone; i++)
		if(p->ring[i].c != nil){
			p->ring[i].c = realloc(p->ring[i].c, cols*sizeof(Vtcell));
			if(cols > vt->cols)
				_vtblank(vt, p->ring[i].c+vt->cols, cols-vt->cols);
		}
	resizelines(vt, &p->main, cols, rows);
	resizelines(vt, &p->alt, cols, rows);
	vt->line = alt ? p->alt : p->main;
	oc = vt->cols;
	vt->cols = cols;
	vt->rows = rows;
	p->tabs = realloc(p->tabs, cols);
	if(cols > oc)
		deftabs(vt, oc);
	free(p->dirty);
	p->dirty = mallocz(rows, 1);
	p->nscroll = 0;
	p->top = 0;
	p->bot = rows;
	if(vt->x >= cols)
		vt->x = cols-1;
	if(vt->y >= rows)
		vt->y = rows-1;
	for(i = 0; i < 2; i++){
		if(p->saved[i].x >= cols)
			p->saved[i].x = cols-1;
		if(p->saved[i].y >= rows)
			p->saved[i].y = rows-1;
	}
	p->pending = 0;
	_vttouchall(vt);
}

void
vtframe(Vt *vt, Vtframe *f)
{
	Priv *p;
	int i;

	p = P(vt);
	memset(f, 0, sizeof *f);
	if(p->dumb){
		memset(p->dirty, 0, vt->rows);
		f->dirty = p->dirty;
		vt->gone = p->out;
		return;
	}
	f->all = p->all;
	f->ngone = p->ngone;
	f->lost = p->lost;
	for(i = 0; i < p->ngone; i++)
		p->out[i] = p->ring[(p->head+i) % Vgone];
	vt->gone = p->out;
	f->scroll = p->scroll;
	f->nscroll = p->all ? 0 : p->nscroll;
	f->dirty = p->dirty;
	if(p->all)
		memset(p->dirty, 1, vt->rows);
	f->cursor = p->ox != vt->x || p->oy != vt->y || ((p->omode ^ vt->mode) & Mcursor);
	f->modes = p->omode ^ vt->mode;
	f->title = p->title;
	f->bell = p->bell;

	p->all = 0;
	p->ngone = 0;
	p->head = 0;
	p->lost = 0;
	p->nscroll = 0;
	p->ox = vt->x;
	p->oy = vt->y;
	p->omode = vt->mode;
	p->title = 0;
	p->bell = 0;
	p->cleardirty = 1;
}
