#include <u.h>
#include <libc.h>
#include <md.h>
#include "impl.h"

static int
pk(P *p, int i)
{
	return i >= 0 && i < p->nb ? p->b[i] : 0;
}

static int
spt(int c)
{
	return c == ' ' || c == '\t';
}

static Blk*
newblk(P *p, int type, u32int q0, u32int line)
{
	Blk *b;

	b = _mdalloc(p, sizeof *b);
	b->type = type;
	b->q0 = q0;
	b->q1 = q0;
	b->line = line;
	b->open = 1;
	return b;
}

P*
_mdnewp(char *s, ulong n)
{
	P *p;

	p = emalloc(sizeof *p);
	p->s = s;
	p->n = n;
	p->root = newblk(p, Mddoc, 0, 0);
	p->cur = p->root;
	return p;
}

static void
pushmk(P *p, Blk *b, Mk m)
{
	Mk *n;

	if(b->nmk >= b->mmk){
		b->mmk = b->mmk*2 + 4;
		n = _mdalloc(p, b->mmk*sizeof b->mk[0]);
		memmove(n, b->mk, b->nmk*sizeof b->mk[0]);
		b->mk = n;
	}
	b->mk[b->nmk++] = m;
}

static void
bmark(P *p, Blk *b, int aux, int i0, int i1)
{
	Mk m;

	m.q0 = p->ls + i0;
	m.q1 = p->ls + i1;
	m.line = p->line;
	m.aux = aux;
	m.own = b;
	pushmk(p, b, m);
}

static void
omark(P *p, Blk *own, int aux, int i0, int i1)
{
	Mk m;

	m.q0 = p->ls + i0;
	m.q1 = p->ls + i1;
	m.line = p->line;
	m.aux = aux;
	m.own = own;
	pushmk(p, own, m);
}

static void
cmark(P *p, Blk *own, int aux, int i0)
{
	omark(p, own, aux, i0, i0+1);
}

static void
addseg(P *p, Blk *b, Seg g)
{
	Seg *n;

	if(b->nseg >= b->mseg){
		b->mseg = b->mseg*2 + 4;
		n = _mdalloc(p, b->mseg*sizeof b->seg[0]);
		memmove(n, b->seg, b->nseg*sizeof b->seg[0]);
		b->seg = n;
	}
	b->seg[b->nseg++] = g;
}

static void
addline(P *p, Blk *b, int end)
{
	Seg g;

	memset(&g, 0, sizeof g);
	if(p->ptab){
		p->off++;
		g.pad = 4 - p->col%4;
	}
	g.b = p->ls + p->off;
	g.e = p->ls + end;
	if(g.e < g.b)
		g.e = g.b;
	if(g.b > p->le)
		g.b = g.e = p->le;
	if(g.e > p->le)
		g.e = p->le;
	g.ln = p->ln;
	g.line = p->line;
	g.nl = 1;
	addseg(p, b, g);
}

static void
fnsp(P *p)
{
	int c, ct;

	ct = 4 - p->col%4;
	if(p->fns <= p->off){
		p->fns = p->off;
		p->fnscol = p->col;
		while(p->fns < p->nb && ((c = p->b[p->fns]) == ' ' || c == '\t')){
			if(c == ' '){
				p->fns++;
				p->fnscol++;
				if(--ct == 0)
					ct = 4;
			}else{
				p->fns++;
				p->fnscol += ct;
				ct = 4;
			}
		}
	}
	p->indent = p->fnscol - p->col;
	p->blank = pk(p, p->fns) == '\n' || pk(p, p->fns) == '\r';
}

static void
advance(P *p, int count, int cols)
{
	int c, ct, k;

	while(count > 0 && p->off < p->nb){
		c = p->b[p->off];
		if(c == '\t'){
			ct = 4 - p->col%4;
			if(cols){
				p->ptab = ct > count;
				k = count < ct ? count : ct;
				p->col += k;
				p->off += p->ptab ? 0 : 1;
				count -= k;
			}else{
				p->ptab = 0;
				p->col += ct;
				p->off++;
				count--;
			}
		}else{
			p->ptab = 0;
			p->off++;
			p->col++;
			count--;
		}
	}
}

static int
cancontain(int up, int t)
{
	return up == Mddoc || up == Mdquote || up == Mditem || up == Mdlist && t == Mditem || up == Mdtable && t == Mdrow;
}

static int
acceptslines(int t)
{
	return t == Mdpara || t == Mdhead || t == Mdcode;
}

static void
append(Blk *up, Blk *b)
{
	b->up = up;
	b->prev = up->last;
	if(up->last != nil)
		up->last->next = b;
	else
		up->kid = b;
	up->last = b;
}

static void
insbefore(Blk *at, Blk *b)
{
	b->up = at->up;
	b->next = at;
	b->prev = at->prev;
	if(at->prev != nil)
		at->prev->next = b;
	else
		at->up->kid = b;
	at->prev = b;
}

static void
rmblk(Blk *b)
{
	if(b->prev != nil)
		b->prev->next = b->next;
	else
		b->up->kid = b->next;
	if(b->next != nil)
		b->next->prev = b->prev;
	else
		b->up->last = b->prev;
}

static Blk*
addkid(P *p, Blk *up, int type, int pos)
{
	Blk *b;

	while(!cancontain(up->type, type))
		up = _mdfinal(p, up);
	b = newblk(p, type, p->ls + pos, p->line);
	append(up, b);
	if(p->opened == nil){
		p->opened = b;
		b->flags |= Fnew;
	}
	return b;
}

static int
blankseg(P *p, Seg *g)
{
	u32int i;

	for(i = g->b; i < g->e; i++)
		if(p->s[i] != ' ' && p->s[i] != '\t')
			return 0;
	return 1;
}

static int
refs(P *p, Blk *b)
{
	Tn *defs, *d, *n;
	Blk *x;
	int k, i, first;

	if(b->nseg == 0 || p->s[b->seg[0].b] != '[')
		return b->nseg > 0 && !(b->nseg == 1 && blankseg(p, &b->seg[0])) ? 1 : 0;
	k = _mdparseref(p, b->seg, b->nseg, b, &defs);
	first = 1;
	for(d = defs; d != nil; d = n){
		n = d->next;
		d->next = nil;
		x = newblk(p, Mddef, d->q0, d->line);
		x->q1 = d->q1;
		x->open = 0;
		x->tn = d;
		if(first && (b->flags & Fnew)){
			x->flags |= Fnew;
			b->flags &= ~Fnew;
		}
		first = 0;
		insbefore(b, x);
		if(p->ndef >= p->mdef){
			p->mdef = p->mdef*2 + 8;
			p->def = erealloc(p->def, p->mdef*sizeof p->def[0]);
		}
		p->def[p->ndef].key = d->key;
		p->def[p->ndef].hash = _mdhash(d->key);
		p->def[p->ndef].off = d->q0;
		p->ndef++;
	}
	if(k > 0){
		memmove(b->seg, b->seg+k, (b->nseg-k)*sizeof b->seg[0]);
		b->nseg -= k;
		if(b->nseg > 0)
			b->seg[0].nl = 1;
	}
	for(i = 0; i < b->nseg; i++)
		if(!blankseg(p, &b->seg[i]))
			return 1;
	return 0;
}

static u32int
trimend(P *p, Blk *b)
{
	u32int e;

	e = b->seg[b->nseg-1].e;
	while(e > b->seg[0].b && _mdisspace(p->s[e-1]))
		e--;
	return e;
}

static void
setq1(P *p, Blk *b)
{
	Blk *k;
	int i;
	u32int e;

	if(b->type == Mddoc){
		b->q0 = 0;
		b->q1 = p->n;
		return;
	}
	e = b->q0;
	switch(b->type){
	case Mdpara:
		if(b->nseg > 0){
			b->q0 = b->seg[0].b;
			e = trimend(p, b);
		}
		break;
	case Mdhead:
		if(b->flags & Mdsetext){
			if(b->nseg > 0){
				b->q0 = b->seg[0].b;
				e = trimend(p, b);
			}
		}else if(b->nseg > 0)
			e = b->seg[0].e;
		break;
	case Mdcode:
	case Mdhtml:
		if(b->nseg > 0)
			e = b->seg[b->nseg-1].e;
		break;
	case Mdtable:
	case Mdrow:
	case Mddef:
	case Mdrule:
	case Mdcell:
		e = b->q1;
		break;
	}
	for(k = b->kid; k != nil; k = k->next)
		if(k->q1 > e)
			e = k->q1;
	for(i = 0; i < b->nmk; i++)
		if(b->mk[i].own == b && b->mk[i].q1 > e)
			e = b->mk[i].q1;
	if(e < b->q0)
		e = b->q0;
	b->q1 = e;
}

static Tn*
info(P *p, Seg *g)
{
	Tn *t;
	u32int a, e;

	a = g->b;
	e = g->e;
	while(a < e && _mdisspace(p->s[a]))
		a++;
	while(e > a && _mdisspace(p->s[e-1]))
		e--;
	if(a == e)
		return nil;
	t = _mdtn(p, Mdinfo, a, e, g->line);
	return t;
}

Blk*
_mdfinal(P *p, Blk *b)
{
	Blk *up;
	int i;

	up = b->up;
	b->open = 0;
	switch(b->type){
	case Mdpara:
		if(!refs(p, b)){
			rmblk(b);
			return up;
		}
		break;
	case Mdcode:
		if(!(b->flags & Mdfenced)){
			while(b->nseg > 0 && blankseg(p, &b->seg[b->nseg-1]))
				b->nseg--;
		}else if(b->nseg > 0){
			b->tn = info(p, &b->seg[0]);
			memmove(b->seg, b->seg+1, (b->nseg-1)*sizeof b->seg[0]);
			b->nseg--;
		}
		break;
	}
	if(b->zip){
		b->flags = b->flags & ~Fblank | b->zflags & Fblank;
		b->llb = (b->zflags & Fblank) != 0;
	}
	if(b->llb)
		b->flags |= Fblank;
	else
		b->flags &= ~Fblank;
	setq1(p, b);
	if(b->zip && b->zq1 > b->q1)
		b->q1 = b->zq1;
	return up;
}

static int
lastopen(Blk *b)
{
	return b->last != nil && b->last->open;
}

static int
quoteprefix(P *p, Blk *c)
{
	if(p->indent <= 3 && pk(p, p->fns) == '>'){
		cmark(p, c, Mdquote, p->fns);
		advance(p, p->indent+1, 1);
		if(spt(pk(p, p->off)))
			advance(p, 1, 1);
		return 1;
	}
	return 0;
}

static int
itemprefix(P *p, Blk *c)
{
	if(p->indent >= c->moff + c->pad){
		advance(p, c->moff + c->pad, 1);
		return 1;
	}
	if(p->blank && c->kid != nil){
		advance(p, p->fns - p->off, 0);
		return 1;
	}
	return 0;
}

static int
codeprefix(P *p, Blk *c, int *cont)
{
	int m, i;

	if(!(c->flags & Mdfenced)){
		if(p->indent >= 4){
			advance(p, 4, 1);
			return 1;
		}
		if(p->blank){
			advance(p, p->fns - p->off, 0);
			return 1;
		}
		return 0;
	}
	m = 0;
	if(p->indent <= 3 && pk(p, p->fns) == c->c)
		m = _mdscanclosefence(p->b, p->nb, p->fns);
	if(m >= c->w){
		*cont = 0;
		bmark(p, c, Mdcode, p->fns, p->fns+m);
		advance(p, m, 0);
		p->cur = _mdfinal(p, c);
		return 0;
	}
	i = c->foff;
	while(i > 0 && spt(pk(p, p->off))){
		advance(p, 1, 1);
		i--;
	}
	return 1;
}

typedef struct Cell Cell;
struct Cell
{
	int	a;
	int	e;
	int	ca;
	int	ce;
	int	pipe;
};

static int
row(uchar *s, int len, Cell **cp, int *ncp)
{
	int off, cm, pm, more, re, n, m;
	Cell *c;

	c = nil;
	n = m = 0;
	off = _mdscantcellend(s, len, 0);
	more = 1;
	while(off < len && more){
		cm = _mdscantcell(s, len, off);
		pm = _mdscantcellend(s, len, off+cm);
		if(cm || pm){
			if(n >= m){
				m = m*2 + 8;
				c = erealloc(c, m*sizeof c[0]);
			}
			c[n].a = off;
			c[n].e = off+cm;
			c[n].pipe = pm ? off+cm : -1;
			n++;
		}
		off += cm + pm;
		if(pm)
			more = 1;
		else{
			re = _mdscantrowend(s, len, off);
			off += re;
			if(re && off != len){
				n = 0;
				off += _mdscantcellend(s, len, off);
				more = 1;
			}else
				more = 0;
		}
	}
	if(off != len || n == 0){
		free(c);
		*cp = nil;
		*ncp = 0;
		return 0;
	}
	for(m = 0; m < n; m++){
		c[m].ca = c[m].a;
		c[m].ce = c[m].e;
		while(c[m].ca < c[m].ce && _mdisspace(s[c[m].ca]))
			c[m].ca++;
		while(c[m].ce > c[m].ca && _mdisspace(s[c[m].ce-1]))
			c[m].ce--;
	}
	*cp = c;
	*ncp = n;
	return 1;
}

static void
tmark(P *p, Blk *b, u32int q0, u32int q1, u32int line)
{
	Mk m;

	m.q0 = q0;
	m.q1 = q1;
	m.line = line;
	m.aux = Mdtable;
	m.own = b;
	pushmk(p, b, m);
}

static Blk*
mkrow(P *p, Blk *t, uchar *s, int len, u32int base, u32int line, Cell *c, int nc, int header)
{
	Blk *r, *x;
	int i, j, last;
	Seg g;
	u32int e;

	r = newblk(p, Mdrow, base, line);
	r->open = 0;
	if(header)
		r->flags |= Mdheader;
	e = base;
	if(_mdscantcellend(s, len, 0) > 0){
		tmark(p, r, base, base+1, line);
		e = base+1;
	}
	for(i = 0; i < nc && i < t->ncol; i++){
		x = newblk(p, Mdcell, base + c[i].a, line);
		x->open = 0;
		x->aux = t->align[i];
		x->q1 = base + c[i].e;
		last = c[i].ca;
		memset(&g, 0, sizeof g);
		g.line = line;
		for(j = c[i].ca; j < c[i].ce; j++)
			if(s[j] == '\\' && j+1 < c[i].ce && s[j+1] == '|'){
				g.b = base + last;
				g.e = base + j;
				g.ln = g.e;
				g.nl = x->nseg == 0;
				addseg(p, x, g);
				last = j+1;
				j++;
			}
		g.b = base + last;
		g.e = base + c[i].ce;
		g.ln = g.e;
		g.nl = x->nseg == 0;
		addseg(p, x, g);
		append(r, x);
		e = x->q1;
		if(c[i].pipe >= 0){
			tmark(p, r, base + c[i].pipe, base + c[i].pipe + 1, line);
			e = base + c[i].pipe + 1;
		}
	}
	for(; i < t->ncol; i++){
		x = newblk(p, Mdcell, e, line);
		x->open = 0;
		x->aux = t->align[i];
		x->flags |= Mdfill;
		x->q1 = e;
		append(r, x);
	}
	r->q1 = e;
	return r;
}

static int
tablematch(P *p, Blk *t)
{
	Cell *c;
	int nc, r;

	USED(t);
	r = row(p->b+p->fns, p->nb-p->fns, &c, &nc);
	free(c);
	return r && nc > 0;
}

static Blk*
tablehead(P *p, Blk *para)
{
	Cell *dc, *hc;
	int nd, nh, i, l, r, k, e;
	Blk *t, *pp, *rw, *x;
	Seg *g;
	uchar *h;

	if(para->tv || para->nseg == 0)
		return para;
	if(!_mdscantstart(p->b, p->nb, p->fns))
		return para;
	if(!row(p->b+p->fns, p->nb-p->fns, &dc, &nd))
		return para;
	g = &para->seg[para->nseg-1];
	h = emalloc(g->e - g->b + 2);
	memmove(h, p->s+g->b, g->e - g->b);
	h[g->e - g->b] = '\n';
	if(!row(h, g->e - g->b + 1, &hc, &nh) || nh != nd){
		free(h);
		free(hc);
		free(dc);
		para->tv = 1;
		return para;
	}
	t = para;
	if(para->nseg > 1){
		pp = newblk(p, Mdpara, para->seg[0].b, para->seg[0].line);
		pp->flags |= para->flags & Fnew;
		para->flags &= ~Fnew;
		for(i = 0; i < para->nseg-1; i++)
			addseg(p, pp, para->seg[i]);
		insbefore(para, pp);
		_mdfinal(p, pp);
	}
	t->type = Mdtable;
	t->q0 = g->b;
	t->line = g->line;
	t->ncol = nh;
	t->align = _mdalloc(p, nh);
	for(i = 0; i < nd; i++){
		l = p->b[p->fns+dc[i].ca] == ':';
		r = p->b[p->fns+dc[i].ce-1] == ':';
		t->align[i] = l && r ? 'c' : l ? 'l' : r ? 'r' : 0;
	}
	rw = mkrow(p, t, h, g->e - g->b + 1, g->b, g->line, hc, nh, 1);
	append(t, rw);
	t->nseg = 0;
	e = p->nb-1;
	while(e > p->fns && _mdisspace(p->b[e-1]))
		e--;
	tmark(p, t, p->ls + p->fns, p->ls + e, p->line);
	t->q1 = p->ls + e;
	for(x = rw->kid; x != nil; x = x->next)
		;
	free(h);
	free(hc);
	free(dc);
	advance(p, p->nb - 1 - p->off, 0);
	return t;
}

static Blk*
tablerow(P *p, Blk *t)
{
	Cell *c;
	int nc;
	Blk *r;

	if(p->blank)
		return nil;
	if(!row(p->b+p->fns, p->nb-p->fns, &c, &nc))
		return nil;
	r = mkrow(p, t, p->b+p->fns, p->nb-p->fns, p->ls+p->fns, p->line, c, nc, 0);
	append(t, r);
	r->open = 1;
	if(p->opened == nil)
		p->opened = r;
	free(c);
	advance(p, p->nb - 1 - p->off, 0);
	if(r->q1 > t->q1)
		t->q1 = r->q1;
	return r;
}

static Blk*
match(P *p, int *all)
{
	Blk *c;
	int cont;

	*all = 0;
	cont = 1;
	c = p->root;
	while(lastopen(c)){
		c = c->last;
		fnsp(p);
		switch(c->type){
		case Mdquote:
			if(!quoteprefix(p, c))
				goto done;
			break;
		case Mdlist:
			if(p->blank){
				if(c->lllb && p->indent == 0){
					if(p->cur->type == Mdcode || p->cur->type == Mdhtml)
						addline(p, p->cur, p->nb-1);
					return nil;
				}
				c->lllb = 1;
			}else
				c->lllb = 0;
			break;
		case Mditem:
			if(!itemprefix(p, c))
				goto done;
			break;
		case Mdcode:
			if(!codeprefix(p, c, &cont))
				goto done;
			break;
		case Mdhead:
			goto done;
		case Mdhtml:
			if(c->aux >= 6 && p->blank)
				goto done;
			break;
		case Mdpara:
			if(p->blank)
				goto done;
			break;
		case Mdtable:
			if(!(p->ext & Mdxtable) || !tablematch(p, c))
				goto done;
			break;
		case Mdrow:
			goto done;
		}
	}
	*all = 1;
done:
	if(!*all)
		c = c->up;
	if(!cont)
		c = nil;
	return c;
}

static int
thematic(P *p, int i)
{
	int c, n, nc;

	c = pk(p, i);
	if(c != '*' && c != '_' && c != '-'){
		p->tbkill = i;
		return 0;
	}
	n = 1;
	nc = 0;
	while(++i < p->nb){
		nc = p->b[i];
		if(nc == c)
			n++;
		else if(nc != ' ' && nc != '\t')
			break;
	}
	if(n >= 3 && (nc == '\r' || nc == '\n'))
		return 1;
	p->tbkill = i;
	return 0;
}

static int
listmarker(P *p, int pos, int interrupt, int *type, int *ch, int *start)
{
	int c, i, st, digits, n;

	st = pos;
	c = pk(p, pos);
	if(c == '*' || c == '-' || c == '+'){
		pos++;
		if(!_mdisspace(pk(p, pos)))
			return 0;
		if(interrupt){
			for(i = pos; spt(pk(p, i)); i++)
				;
			if(pk(p, i) == '\n')
				return 0;
		}
		*type = 0;
		*ch = c;
		*start = 0;
		return pos-st;
	}
	if(c >= '0' && c <= '9'){
		n = 0;
		digits = 0;
		do{
			n = 10*n + pk(p, pos) - '0';
			pos++;
			digits++;
		}while(digits < 9 && pk(p, pos) >= '0' && pk(p, pos) <= '9');
		if(interrupt && n != 1)
			return 0;
		c = pk(p, pos);
		if(c == '.' || c == ')'){
			pos++;
			if(!_mdisspace(pk(p, pos)))
				return 0;
			if(interrupt){
				for(i = pos; spt(pk(p, i)); i++)
					;
				if(pk(p, i) == '\n' || pk(p, i) == '\r')
					return 0;
			}
			*type = Mdordered;
			*ch = c;
			*start = n;
			return pos-st;
		}
	}
	return 0;
}

static void
opennew(P *p, Blk **cp, int all)
{
	int indented, m, lazy, ct, lev, type, ch, start, so, sc, sp, i, hp, hcount, mpos;
	Blk *c, *x;

	c = *cp;
	lazy = p->cur->type == Mdpara;
	ct = c->type;
	while(ct != Mdcode && ct != Mdhtml){
		fnsp(p);
		indented = p->indent >= 4;
		if(!indented && pk(p, p->fns) == '>'){
			mpos = p->fns;
			advance(p, p->fns + 1 - p->off, 0);
			if(spt(pk(p, p->off)))
				advance(p, 1, 1);
			c = addkid(p, c, Mdquote, mpos);
			omark(p, c, Mdquote, mpos, mpos+1);
		}else if(!indented && (m = _mdscanatx(p->b, p->nb, p->fns)) > 0){
			hp = p->fns;
			advance(p, p->fns + m - p->off, 0);
			c = addkid(p, c, Mdhead, hp);
			for(hcount = 0; pk(p, hp+hcount) == '#'; hcount++)
				;
			c->aux = hcount;
			bmark(p, c, Mdhead, hp, hp+hcount);
		}else if(!indented && (m = _mdscanopenfence(p->b, p->nb, p->fns)) > 0){
			c = addkid(p, c, Mdcode, p->fns);
			c->flags |= Mdfenced;
			c->c = pk(p, p->fns);
			c->w = m > 255 ? 255 : m;
			c->foff = p->fns - p->off;
			c->aux = c->foff;
			bmark(p, c, Mdcode, p->fns, p->fns+m);
			advance(p, p->fns + m - p->off, 0);
		}else if(!indented && ((m = _mdscanhstart(p->b, p->nb, p->fns)) > 0 ||
		   ct != Mdpara && !lazy && (m = _mdscanhstart7(p->b, p->nb, p->fns)) > 0)){
			c = addkid(p, c, Mdhtml, p->off);
			c->aux = m;
		}else if(!indented && ct == Mdpara && (lev = _mdscansetext(p->b, p->nb, p->fns)) > 0){
			if(refs(p, c)){
				c->type = Mdhead;
				c->aux = lev;
				c->flags |= Mdsetext;
				i = p->nb-1;
				while(i > p->fns && _mdisspace(p->b[i-1]))
					i--;
				bmark(p, c, Mdhead, p->fns, i);
				advance(p, p->nb - 1 - p->off, 0);
			}
		}else if(!indented && !(ct == Mdpara && !all) && p->tbkill <= p->fns && thematic(p, p->fns)){
			c = addkid(p, c, Mdrule, p->fns);
			i = p->nb-1;
			while(i > p->fns && _mdisspace(p->b[i-1]))
				i--;
			c->q1 = p->ls + i;
			advance(p, p->nb - 1 - p->off, 0);
		}else if((!indented || ct == Mdlist) && p->indent < 4 &&
		   (m = listmarker(p, p->fns, c->type == Mdpara, &type, &ch, &start)) > 0){
			mpos = p->fns;
			advance(p, p->fns + m - p->off, 0);
			sp = p->ptab;
			so = p->off;
			sc = p->col;
			while(p->col - sc <= 5 && spt(pk(p, p->off)))
				advance(p, 1, 1);
			i = p->col - sc;
			if(i >= 5 || i < 1 || pk(p, p->off) == '\n' || pk(p, p->off) == '\r'){
				lev = m + 1;
				p->off = so;
				p->col = sc;
				p->ptab = sp;
				if(i > 0)
					advance(p, 1, 1);
			}else
				lev = m + i;
			if(ct != Mdlist || (c->flags & Mdordered) != type || c->c != ch){
				c = addkid(p, c, Mdlist, mpos);
				c->flags |= type;
				c->c = ch;
				c->aux = start;
			}
			x = c;
			c = addkid(p, c, Mditem, mpos);
			c->flags |= type;
			c->c = ch;
			c->aux = start;
			c->moff = p->indent;
			c->pad = lev;
			c->w = c->moff + c->pad;
			omark(p, c, Mditem, mpos, mpos+m);
			USED(x);
		}else if(indented && !lazy && !p->blank){
			advance(p, 4, 1);
			c = addkid(p, c, Mdcode, p->off);
		}else{
			x = nil;
			if(!indented && ct == Mdpara && (p->ext & Mdxtable))
				x = tablehead(p, c);
			else if(!indented && ct == Mdtable && (p->ext & Mdxtable))
				x = tablerow(p, c);
			if(x != nil && (x != c || x->type == Mdtable && ct == Mdpara)){
				c = x;
			}else if(ct == Mditem && (p->ext & Mdxtask) && (m = _mdscantask(p->b, p->nb, 0)) > 0){
				c->flags |= Mdtodo;
				for(i = p->off; i < p->nb && p->b[i] != '['; i++)
					;
				omark(p, c, Mdtask, i, i+3);
				if(strstr((char*)p->b, "[x]") != nil || strstr((char*)p->b, "[X]") != nil)
					c->flags |= Mdchecked;
				advance(p, 3, 0);
				break;
			}else
				break;
		}
		if(acceptslines(c->type))
			break;
		ct = c->type;
		lazy = 0;
	}
	*cp = c;
}

static int
chop(P *p, int end)
{
	int n, orig;

	while(end > 0 && _mdisspace(p->b[end-1]))
		end--;
	orig = n = end-1;
	while(n >= 0 && p->b[n] == '#')
		n--;
	if(n != orig && n >= 0 && spt(p->b[n])){
		end = n;
		while(end > 0 && _mdisspace(p->b[end-1]))
			end--;
	}
	return end;
}

static void
addtext(P *p, Blk *c, Blk *lm)
{
	Blk *t;
	int llb, ct, end, e2, hend;

	fnsp(p);
	if(p->blank && c->last != nil)
		c->last->llb = 1;
	ct = c->type;
	llb = p->blank && ct != Mdquote && ct != Mdhead && ct != Mdrule && !(ct == Mdcode && (c->flags & Mdfenced)) &&
		!(ct == Mditem && c->kid == nil && c->line == p->line);
	c->llb = llb;
	for(t = c; t->up != nil; t = t->up)
		t->up->llb = 0;
	if(p->cur != lm && c == lm && !p->blank && p->cur->type == Mdpara){
		addline(p, p->cur, p->nb-1);
		return;
	}
	while(p->cur != lm)
		p->cur = _mdfinal(p, p->cur);
	if(c->type == Mdcode)
		addline(p, c, p->nb-1);
	else if(c->type == Mdhtml){
		addline(p, c, p->nb-1);
		if(c->aux <= 5 && _mdscanhend(p->b, p->nb, p->fns, c->aux))
			c = _mdfinal(p, c);
	}else if(p->blank){
	}else if(acceptslines(c->type)){
		end = p->nb-1;
		if(c->type == Mdhead && !(c->flags & Mdsetext)){
			e2 = end;
			while(e2 > 0 && _mdisspace(p->b[e2-1]))
				e2--;
			hend = chop(p, end);
			if(hend < p->fns)
				hend = p->fns;
			if(hend < e2){
				int hs;
				for(hs = e2; hs > hend && p->b[hs-1] == '#'; hs--)
					;
				if(hs < e2)
					bmark(p, c, Mdhead, hs, e2);
			}
			end = hend;
		}
		advance(p, p->fns - p->off, 0);
		if(c->type == Mdhead && (c->flags & Mdsetext) && p->off >= p->nb-1)
			;
		else
			addline(p, c, end);
	}else{
		c = addkid(p, c, Mdpara, p->fns);
		advance(p, p->fns - p->off, 0);
		addline(p, c, p->nb-1);
	}
	p->cur = c;
}

void
_mdline(P *p, ulong ls)
{
	ulong le, ln;
	int all, n;
	Blk *lm, *c;

	for(le = ls; le < p->n && p->s[le] != '\n' && p->s[le] != '\r'; le++)
		;
	ln = le;
	if(ln < p->n){
		if(p->s[ln] == '\r'){
			ln++;
			if(ln < p->n && p->s[ln] == '\n')
				ln++;
		}else
			ln++;
	}
	p->ls = ls;
	p->le = le;
	p->ln = ln;
	n = le - ls;
	if(n+2 > p->mb){
		p->mb = n+2 + 64;
		p->b = erealloc(p->b, p->mb);
	}
	memmove(p->b, p->s+ls, n);
	p->b[n] = '\n';
	p->b[n+1] = 0;
	p->nb = n+1;
	p->off = 0;
	p->col = 0;
	p->fns = 0;
	p->fnscol = 0;
	p->tbkill = 0;
	p->indent = 0;
	p->blank = 0;
	p->ptab = 0;
	p->opened = nil;
	lm = match(p, &all);
	if(lm == nil)
		goto out;
	c = lm;
	opennew(p, &c, all);
	addtext(p, c, lm);
out:
	p->line++;
}

void
_mdfinish(P *p)
{
	while(p->cur != p->root)
		p->cur = _mdfinal(p, p->cur);
}
