#include <u.h>
#include <libc.h>
#include <md.h>
#include "impl.h"

void
_mdincref(Mdp *p)
{
	if(p != nil)
		p->ref++;
}

void
_mddecref(Mdp *p)
{
	Mdp **st;
	int n, m, i;
	Mdbuf *b;
	Mdtree *t;

	if(p == nil || --p->ref > 0)
		return;
	m = 16;
	st = emalloc(m*sizeof st[0]);
	n = 0;
	st[n++] = p;
	while(n > 0){
		p = st[--n];
		if(p->kind == Pbuf){
			b = (Mdbuf*)p;
			free(b->n);
			free(b->seg);
			free(b->lab);
			free(b);
			continue;
		}
		t = (Mdtree*)p;
		for(i = 0; i < t->nk; i++){
			if(--t->k[i].p->ref > 0)
				continue;
			if(n >= m){
				m *= 2;
				st = erealloc(st, m*sizeof st[0]);
			}
			st[n++] = t->k[i].p;
		}
		free(t->k);
		free(t);
	}
	free(st);
}

Mdsum
_mdsumcat(Mdsum x, Mdsum y)
{
	Mdsum r;

	r.bloom = x.bloom | y.bloom;
	r.f = (x.f | y.f) & (Shb|Shi);
	if(y.f & Shb){
		r.f |= (x.f & Sea) | (y.f & Sea);
		if((x.f & Shb) && (x.f & Sel))
			r.f |= Sea;
		r.f |= y.f & Sel;
	}else
		r.f |= x.f & (Sea|Sel);
	if(y.f & Shi){
		r.f |= (x.f & Sia) | (y.f & Sia);
		if((x.f & Shi) && (x.f & Sila))
			r.f |= Sia;
		r.f |= y.f & (Sila|Silb);
	}else
		r.f |= x.f & (Sia|Sila|Silb);
	return r;
}

static uvlong
bloom(u32int h)
{
	return 1ULL<<(h & 63) | 1ULL<<((h>>6) & 63);
}

static Mdsum
unitsum(Tn *t)
{
	Mdsum s;
	Tn *k, *e;
	int i;

	if(t->a.p != nil)
		return t->a.s;
	s.f = 0;
	s.bloom = 0;
	if(!_mdisblock(t->type))
		return s;
	s.f = Shb;
	if(t->flags & Fends)
		s.f |= Sel;
	if(t->type == Mditem){
		s.f |= Shi;
		if(t->flags & Fla)
			s.f |= Sila;
		if(t->flags & Flb)
			s.f |= Silb;
	}
	k = t;
	e = t;
	for(;;){
		for(i = 0; i < k->nlab; i++)
			s.bloom |= bloom(k->lab[i]);
		if(k->a.p != nil)
			s.bloom |= k->a.s.bloom;
		else if(k->kid != nil){
			k = k->kid;
			continue;
		}
		while(k != e && k->next == nil)
			k = k->up;
		if(k == e)
			break;
		k = k->next;
	}
	return s;
}

Mdsum
_mdsumtn(Tn *t)
{
	return unitsum(t);
}

void
_mdcontflags(Tn *t, Mdsum s)
{
	int bl;

	bl = (t->flags & Fblank) != 0;
	t->flags &= ~(Fends|Fla|Flb|Mdtight);
	switch(t->type){
	case Mditem:
		if((s.f & Shb) ? (s.f & Sel) : bl)
			t->flags |= Fends;
		if(bl || (s.f & Sea) || (s.f & Shb) && (s.f & Sel))
			t->flags |= Fla;
		if(s.f & Sea)
			t->flags |= Flb;
		break;
	case Mdlist:
		if((s.f & Shb) ? (s.f & Sel) : bl)
			t->flags |= Fends;
		if(!((s.f & Sia) || (s.f & Silb)))
			t->flags |= Mdtight;
		break;
	default:
		if(bl)
			t->flags |= Fends;
	}
}

Mdbuf*
_mdencode(Tn *first, Tn *last, u32int base, u32int lbase)
{
	Mdbuf *b;
	Tn *k, *r;
	int n, i, ns, nl, j, *stk, sp, ms;
	Mdn *x;
	u32int *prevat;

	n = 0;
	ns = 0;
	nl = 0;
	for(r = first; ; r = r->next){
		for(k = r; ; ){
			n++;
			ns += k->nseg;
			nl += k->nlab;
			if(k->kid != nil){
				k = k->kid;
				continue;
			}
			while(k != r && k->next == nil)
				k = k->up;
			if(k == r)
				break;
			k = k->next;
		}
		if(r == last)
			break;
	}
	b = emalloc(sizeof *b);
	b->ref = 1;
	b->kind = Pbuf;
	b->n = emalloc(n*sizeof b->n[0]);
	b->nn = n;
	b->seg = ns ? emalloc(ns*sizeof b->seg[0]) : nil;
	b->lab = nl ? emalloc(nl*sizeof b->lab[0]) : nil;
	ms = 64;
	stk = emalloc(ms*sizeof stk[0]);
	prevat = emalloc(ms*sizeof prevat[0]);
	i = 0;
	sp = 0;
	prevat[0] = 0;
	{
		u32int lastroot;
		int haveroot;

		haveroot = 0;
		lastroot = 0;
		for(r = first; ; r = r->next){
			sp = 0;
			for(k = r; ; ){
				x = &b->n[i];
				x->type = k->type;
				x->c = k->c;
				x->w = k->w;
				x->flags = k->flags;
				x->aux = k->aux;
				x->off = k->q0 - base;
				x->len = k->q1 - k->q0;
				x->line = k->line - lbase;
				for(j = 0; j < k->nseg; j++){
					Mdseg *g;

					g = &b->seg[b->nseg++];
					g->node = i;
					g->b = k->seg[j].b - base;
					g->e = k->seg[j].e - base;
					g->ln = k->seg[j].ln - base;
					g->line = k->seg[j].line - lbase;
					g->pad = k->seg[j].pad;
					g->nl = k->seg[j].nl;
				}
				for(j = 0; j < k->nlab; j++){
					b->lab[b->nlab].hash = k->lab[j];
					b->lab[b->nlab].node = i;
					b->nlab++;
				}
				if(sp == 0){
					if(haveroot)
						b->n[lastroot].next = i;
					haveroot = 1;
					lastroot = i;
				}else{
					if(prevat[sp] == 0)
						b->n[stk[sp-1]].kid = i;
					else
						b->n[prevat[sp]].next = i;
					prevat[sp] = i;
				}
				i++;
				if(k->kid != nil){
					if(sp+2 >= ms){
						ms *= 2;
						stk = erealloc(stk, ms*sizeof stk[0]);
						prevat = erealloc(prevat, ms*sizeof prevat[0]);
					}
					stk[sp] = i-1;
					sp++;
					prevat[sp] = 0;
					k = k->kid;
					continue;
				}
				while(k != r && k->next == nil){
					k = k->up;
					sp--;
				}
				if(k == r)
					break;
				k = k->next;
			}
			if(r == last)
				break;
		}
	}
	free(stk);
	free(prevat);
	return b;
}

Tn*
_mddecode(P *p, Mdbuf *b, u32int base, u32int lbase)
{
	Tn **t, *first, *x, *prev;
	int i, j, *up;
	Mdn *n;

	if(b->nn == 0)
		return nil;
	t = emalloc(b->nn*sizeof t[0]);
	up = emalloc(b->nn*sizeof up[0]);
	for(i = 0; i < b->nn; i++){
		n = &b->n[i];
		x = _mdtn(p, n->type, n->off + base, n->off + n->len + base, n->line + lbase);
		x->c = n->c;
		x->w = n->w;
		x->flags = n->flags;
		x->aux = n->aux;
		t[i] = x;
		up[i] = -1;
	}
	for(i = 0; i < b->nn; i++){
		n = &b->n[i];
		if(n->kid != 0){
			prev = nil;
			for(j = n->kid; ; j = b->n[j].next){
				up[j] = i;
				_mdadd(t[i], t[j]);
				USED(prev);
				if(b->n[j].next == 0)
					break;
			}
		}
	}
	for(i = 0; i < b->nseg; i++){
		x = t[b->seg[i].node];
		x->nseg++;
	}
	for(i = 0; i < b->nn; i++)
		if(t[i]->nseg > 0){
			t[i]->seg = _mdalloc(p, t[i]->nseg*sizeof(Seg));
			t[i]->nseg = 0;
		}
	for(i = 0; i < b->nseg; i++){
		Seg *g;
		Mdseg *s;

		s = &b->seg[i];
		x = t[s->node];
		g = &x->seg[x->nseg++];
		g->b = s->b + base;
		g->e = s->e + base;
		g->ln = s->ln + base;
		g->line = s->line + lbase;
		g->pad = s->pad;
		g->nl = s->nl;
	}
	for(i = 0; i < b->nlab; i++)
		t[b->lab[i].node]->nlab++;
	for(i = 0; i < b->nn; i++)
		if(t[i]->nlab > 0){
			t[i]->lab = _mdalloc(p, t[i]->nlab*sizeof(u32int));
			t[i]->nlab = 0;
		}
	for(i = 0; i < b->nlab; i++){
		x = t[b->lab[i].node];
		x->lab[x->nlab++] = b->lab[i].hash;
	}
	first = t[0];
	prev = first;
	for(i = b->n[0].next; i != 0; i = b->n[i].next){
		prev->next = t[i];
		t[i]->prev = prev;
		prev = t[i];
	}
	free(t);
	free(up);
	return first;
}

Akid
_mdakid(Mdp *pp, u32int q, u32int l)
{
	Akid a;

	memset(&a, 0, sizeof a);
	a.p = pp;
	a.q = q;
	a.l = l;
	return a;
}

static Akid
bufkid(Tn *first, Tn *last)
{
	Akid a;
	Tn *k;
	u32int e;
	Mdsum s;

	e = first->q1;
	s.f = 0;
	s.bloom = 0;
	for(k = first; ; k = k->next){
		if(k->q1 > e)
			e = k->q1;
		s = _mdsumcat(s, unitsum(k));
		if(k == last)
			break;
	}
	a.p = (Mdp*)_mdencode(first, last, first->q0, first->line);
	a.q = first->q0;
	a.l = first->line;
	a.len = e - first->q0;
	a.s = s;
	return a;
}

static Mdtree*
mkrun(Akid *k, int n, int h)
{
	Mdtree *t;
	int i;

	t = emalloc(sizeof *t);
	t->ref = 1;
	t->kind = Prun;
	t->h = h;
	t->nk = n;
	t->k = emalloc(n*sizeof t->k[0]);
	for(i = 0; i < n; i++){
		t->k[i].p = k[i].p;
		t->k[i].d = i == 0 ? 0 : k[i].q - k[i-1].q;
		t->k[i].dl = i == 0 ? 0 : k[i].l - k[i-1].l;
		t->k[i].len = k[i].len;
		t->k[i].s = k[i].s;
	}
	return t;
}

static Akid
runkid(Akid *k, int n, int h)
{
	Akid a;
	int i;
	u32int e;

	a.p = (Mdp*)mkrun(k, n, h);
	a.q = k[0].q;
	a.l = k[0].l;
	e = k[0].q + k[0].len;
	a.s = k[0].s;
	for(i = 1; i < n; i++){
		if(k[i].q + k[i].len > e)
			e = k[i].q + k[i].len;
		a.s = _mdsumcat(a.s, k[i].s);
	}
	a.len = e - a.q;
	return a;
}

void
_mdseqbuild(Seq *s)
{
	Akid *k;
	int g, i, j, n, per, extra;

	while(s->nk > Kmax){
		g = (s->nk + Kmax - 1)/Kmax;
		per = s->nk/g;
		extra = s->nk%g;
		k = emalloc(g*sizeof k[0]);
		j = 0;
		for(i = 0; i < g; i++){
			n = per + (i < extra);
			k[i] = runkid(s->k+j, n, s->h+1);
			j += n;
		}
		free(s->k);
		s->k = k;
		s->nk = g;
		s->h++;
	}
}

Mdtree*
_mdmkcont(Tn *t, Seq s)
{
	Mdtree *c;
	int i;

	_mdseqbuild(&s);
	c = emalloc(sizeof *c);
	c->ref = 1;
	c->kind = Pcont;
	c->h = s.h;
	c->nk = s.nk;
	c->k = emalloc((s.nk > 0 ? s.nk : 1)*sizeof c->k[0]);
	for(i = 0; i < s.nk; i++){
		c->k[i].p = s.k[i].p;
		c->k[i].d = i == 0 ? s.k[i].q - t->q0 : s.k[i].q - s.k[i-1].q;
		c->k[i].dl = i == 0 ? s.k[i].l - t->line : s.k[i].l - s.k[i-1].l;
		c->k[i].len = s.k[i].len;
		c->k[i].s = s.k[i].s;
	}
	c->n.type = t->type;
	c->n.c = t->c;
	c->n.w = t->w;
	c->n.flags = t->flags;
	c->n.aux = t->aux;
	c->n.len = t->q1 - t->q0;
	free(s.k);
	return c;
}

static Tn*
mktn(P *p, Blk *b)
{
	Tn *t;

	t = _mdtn(p, b->type, b->q0, b->q1, b->line);
	t->c = b->c;
	t->w = b->w;
	t->aux = b->aux;
	t->flags = b->flags & ~(Fends|Fla|Flb|Mdtight);
	return t;
}

static void
srctext(P *p, Tn *up, u32int a, u32int e, u32int line, int esc)
{
	u32int i, j;
	int k;
	Rune r[2];

	i = a;
	for(j = a; j < e; ){
		if(esc && p->s[j] == '\\' && j+1 < e && _mdispunct(p->s[j+1])){
			if(j > i)
				_mdadd(up, _mdtn(p, Mdtext, i, j, line));
			_mdadd(up, _mdtn(p, Mdesc, j, j+2, line));
			i = j = j+2;
			continue;
		}
		if(esc && p->s[j] == '&' && (k = _mdent1((uchar*)p->s+j, e-j, r)) > 0){
			if(j > i)
				_mdadd(up, _mdtn(p, Mdtext, i, j, line));
			_mdadd(up, _mdtn(p, Mdent, j, j+k, line));
			i = j = j+k;
			continue;
		}
		j++;
	}
	if(j > i)
		_mdadd(up, _mdtn(p, Mdtext, i, j, line));
}

static int
descend(Tn *k, Tn *m)
{
	if(k->a.p != nil || k->type == Mdtext || k->type == Mdmark || k->type == Mdtask || k->q0 >= k->q1)
		return 0;
	return k->q0 <= m->q0 && m->q1 <= k->q1;
}

void
_mdinsmark(Tn *t, Tn *m)
{
	Tn *k;

	for(;;){
		k = t->scan != nil ? t->scan : t->kid;
		while(k != nil && k->q1 <= m->q0)
			k = k->next;
		if(k != nil && descend(k, m)){
			t->scan = k;
			t = k;
			continue;
		}
		if(k == nil)
			_mdadd(t, m);
		else
			_mdbefore(k, m);
		t->scan = m;
		return;
	}
}

static void
leaf(P *p, Blk *b, Tn *t)
{
	int i;
	Tn *x;

	switch(b->type){
	case Mdpara:
	case Mdhead:
	case Mdcell:
		if(b->nseg > 0){
			t->seg = _mdalloc(p, b->nseg*sizeof(Seg));
			memmove(t->seg, b->seg, b->nseg*sizeof(Seg));
			t->nseg = b->nseg;
			_mdinline(p, t, t->seg, t->nseg, 1);
		}
		break;
	case Mdcode:
	case Mdhtml:
		if(b->tn != nil){
			x = b->tn;
			srctext(p, x, x->q0, x->q1, x->line, 1);
			_mdadd(t, x);
		}
		for(i = 0; i < b->nseg; i++){
			x = _mdtn(p, Mdtext, b->seg[i].b, b->seg[i].e, b->seg[i].line);
			x->aux = b->seg[i].pad;
			_mdadd(t, x);
		}
		break;
	case Mddef:
		x = b->tn;
		t->kid = x->kid;
		t->last = x->last;
		for(x = t->kid; x != nil; x = x->next)
			x->up = t;
		break;
	}
}

typedef struct Pm Pm;
struct Pm
{
	Mk	m;
	Tn	*own;
};

static int
pmcmp(void *a, void *b)
{
	Pm *x, *y;

	x = a;
	y = b;
	if(x->m.q0 != y->m.q0)
		return x->m.q0 < y->m.q0 ? -1 : 1;
	return 0;
}

static void
addmarks(P *p, Blk **ord, int n)
{
	int i, j, k;
	Pm *pm;
	Tn *m;
	Blk *b;

	k = 0;
	for(i = 0; i < n; i++)
		if(!ord[i]->isold)
			k += ord[i]->nmk;
	if(k == 0)
		return;
	pm = emalloc(k*sizeof pm[0]);
	k = 0;
	for(i = 0; i < n; i++){
		b = ord[i];
		if(b->isold)
			continue;
		for(j = 0; j < b->nmk; j++){
			pm[k].m = b->mk[j];
			pm[k].own = b->tn;
			k++;
		}
	}
	qsort(pm, k, sizeof pm[0], pmcmp);
	for(i = 0; i < k; i++){
		m = _mdtn(p, pm[i].m.aux == Mdtask ? Mdtask : Mdmark, pm[i].m.q0, pm[i].m.q1, pm[i].m.line);
		if(m->type == Mdmark)
			m->aux = pm[i].m.aux;
		_mdinsmark(pm[i].own, m);
	}
	free(pm);
}

static int
small(Tn *t)
{
	Tn *k;

	if(t->q1 - t->q0 > Chunk)
		return 0;
	for(k = t->kid; k != nil; k = k->next)
		if(k->a.p != nil)
			return 0;
	return 1;
}

static void
oldunits(P *p, Blk *b, Tn *t)
{
	Tn *x, *n;

	USED(p);
	for(x = b->oldk; x != nil; x = n){
		n = x->next;
		x->next = x->prev = x->up = nil;
		_mdadd(t, x);
	}
}

static Tn*
container(P *p, Tn *t)
{
	Tn *k, *n, *g0, *g1, *x;
	Mdsum s;
	Akid *a;
	int na, ma;
	u32int gq;
	Seq q;

	s.f = 0;
	s.bloom = 0;
	for(k = t->kid; k != nil; k = k->next)
		s = _mdsumcat(s, unitsum(k));
	_mdcontflags(t, s);
	if(t->type != Mddoc && small(t))
		return t;
	a = nil;
	na = ma = 0;
	for(k = t->kid; k != nil; k = n){
		n = k->next;
		if(na+1 >= ma){
			ma = ma*2 + 8;
			a = erealloc(a, ma*sizeof a[0]);
		}
		if(k->a.p != nil){
			a[na++] = k->a;
			continue;
		}
		g0 = k;
		g1 = k;
		gq = k->q0;
		while(g1->next != nil && g1->next->a.p == nil && g1->next->q1 - gq <= Chunk)
			g1 = g1->next;
		n = g1->next;
		a[na++] = bufkid(g0, g1);
	}
	q.k = a;
	q.nk = na;
	q.h = 0;
	x = _mdtn(p, t->type, t->q0, t->q1, t->line);
	x->flags = t->flags;
	x->a.p = (Mdp*)_mdmkcont(t, q);
	x->a.q = t->q0;
	x->a.l = t->line;
	x->a.len = t->q1 - t->q0;
	x->a.s.f = Shb;
	if(t->flags & Fends)
		x->a.s.f |= Sel;
	if(t->type == Mditem){
		x->a.s.f |= Shi;
		if(t->flags & Fla)
			x->a.s.f |= Sila;
		if(t->flags & Flb)
			x->a.s.f |= Silb;
	}
	x->a.s.bloom = s.bloom;
	x->flags = t->flags;
	return x;
}

Tn*
_mdpack(P *p, Blk *root)
{
	Blk *b;
	Tn *t, *pt;
	Blk **ord;
	int n, m, i;
	Tn **res;

	m = 64;
	ord = emalloc(m*sizeof ord[0]);
	n = 0;
	for(b = root; ; ){
		if(n >= m){
			m *= 2;
			ord = erealloc(ord, m*sizeof ord[0]);
		}
		ord[n++] = b;
		if(b->kid != nil){
			b = b->kid;
			continue;
		}
		while(b != root && b->next == nil)
			b = b->up;
		if(b == root)
			break;
		b = b->next;
	}
	res = emalloc(n*sizeof res[0]);
	for(i = n-1; i >= 0; i--){
		b = ord[i];
		b->line = b->line;
		if(b->isold){
			res[i] = nil;
			continue;
		}
		t = mktn(p, b);
		if(b->llb || (b->flags & Fblank))
			t->flags |= Fblank;
		else
			t->flags &= ~Fblank;
		leaf(p, b, t);
		res[i] = t;
		b->tn = t;
	}
	for(i = 0; i < n; i++){
		Blk *k;

		b = ord[i];
		if(b->isold)
			continue;
		pt = b->tn;
		for(k = b->kid; k != nil; k = k->next){
			if(k->isold)
				oldunits(p, k, pt);
			else if(k->type != Mdcell || 1)
				_mdadd(pt, k->tn);
		}
	}
	for(i = n-1; i >= 0; i--){
		Blk *k;
		int j;
		u32int e;

		b = ord[i];
		if(b->isold || !(b->type == Mdquote || b->type == Mdlist || b->type == Mditem))
			continue;
		e = b->q0;
		for(k = b->kid; k != nil; k = k->next)
			if(k->q1 > e)
				e = k->q1;
		for(j = 0; j < b->nmk; j++)
			if(b->mk[j].q1 > e)
				e = b->mk[j].q1;
		if(b->zip && b->zq1 > e)
			e = b->zq1;
		b->q1 = e;
		b->tn->q1 = e;
	}
	addmarks(p, ord, n);
	for(i = n-1; i >= 0; i--){
		b = ord[i];
		if(b->isold)
			continue;
		t = b->tn;
		if(b->type == Mddoc || b->type == Mdquote || b->type == Mdlist || b->type == Mditem){
			Tn *up, *prev, *next, *c;

			up = t->up;
			prev = t->prev;
			next = t->next;
			c = container(p, t);
			if(c != t){
				c->up = up;
				c->prev = prev;
				c->next = next;
				if(prev != nil)
					prev->next = c;
				else if(up != nil)
					up->kid = c;
				if(next != nil)
					next->prev = c;
				else if(up != nil)
					up->last = c;
				b->tn = c;
			}
		}else{
			Mdsum s;
			s.f = 0;
			s.bloom = 0;
			_mdcontflags(t, s);
		}
	}
	t = root->tn;
	free(ord);
	free(res);
	return t;
}
