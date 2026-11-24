#include <u.h>
#include <libc.h>
#include <md.h>
#include "impl.h"

typedef struct Hd Hd;

struct Hd
{
	Mdtree	*t;
	Mdbuf	*b;
	int	i;
	u32int	q;
	u32int	l;
	Mdnode	n;
};

static void
handle(Mdcur *c, Hd *h)
{
	Mdframe *f;

	memset(h, 0, sizeof *h);
	f = &c->f[c->nf-1];
	memmove(&h->n, c, sizeof(Mdnode));
	switch(f->kind){
	case Rroot:
		h->t = (Mdtree*)f->p;
		break;
	case Rseq:
	case Rrun:
		h->t = (Mdtree*)((Mdtree*)f->p)->k[f->i].p;
		h->q = f->q;
		h->l = f->l;
		break;
	default:
		h->b = (Mdbuf*)f->p;
		h->i = f->i;
		h->q = f->q;
		h->l = f->l;
	}
	if(h->t != nil){
		h->q = c->q0;
		h->l = c->line;
	}
}

static int
iscont(int t)
{
	return t == Mddoc || t == Mdquote || t == Mdlist || t == Mditem;
}

static int
lastbefore(Mdcur *c, u32int x)
{
	int base, nb, found;
	Mdframe *best;

	if(!iscont(c->type))
		return 0;
	base = c->nf;
	if(!mddown(c))
		return 0;
	best = nil;
	nb = 0;
	found = 0;
	do{
		if(c->q0 >= x)
			break;
		if(_mdisblock(c->type)){
			nb = c->nf - base;
			best = erealloc(best, nb*sizeof best[0]);
			memmove(best, c->f+base, nb*sizeof best[0]);
			found = 1;
		}
	}while(mdnext(c));
	c->nf = base;
	if(found){
		memmove(c->f+base, best, nb*sizeof best[0]);
		c->nf = base+nb;
	}
	free(best);
	_mdcurfill(c);
	return found;
}

static int
lastnew(Md *m, u32int x, Mdcur *c)
{
	u32int q;

	memset(c, 0, sizeof *c);
	mdroot(m, c);
	while(lastbefore(c, x))
		;
	for(;;){
		if(c->type == Mddoc)
			return 0;
		if(c->flags & Fnew)
			return 1;
		q = c->q0;
		mdup(c);
		if(lastbefore(c, q))
			while(lastbefore(c, ~0U))
				;
	}
}

static void
shift(Tn *root, long dq, long dl)
{
	Tn *k;
	int i;

	for(k = root; k != nil; ){
		k->q0 += dq;
		k->q1 += dq;
		k->line += dl;
		for(i = 0; i < k->nseg; i++){
			k->seg[i].b += dq;
			k->seg[i].e += dq;
			k->seg[i].ln += dq;
			k->seg[i].line += dl;
		}
		if(k->a.p != nil){
			k->a.q += dq;
			k->a.l += dl;
		}
		if(k->kid != nil){
			k = k->kid;
			continue;
		}
		while(k != root && k->next == nil)
			k = k->up;
		if(k == root)
			break;
		k = k->next;
	}
}

typedef struct Ul Ul;
struct Ul
{
	Tn	*h;
	Tn	*t;
	long	dq;
	long	dl;
};

static void
unit(Ul *u, Tn *x)
{
	x->up = nil;
	x->next = nil;
	x->prev = u->t;
	if(u->t != nil)
		u->t->next = x;
	else
		u->h = x;
	u->t = x;
	if(u->dq != 0 || u->dl != 0){
		Tn *n;

		n = x->next;
		x->next = nil;
		shift(x, u->dq, u->dl);
		x->next = n;
	}
}

static void
take(P *p, Ul *u, Akid a, u32int lo, u32int hi)
{
	Tn *x, *n;
	Mdtree *t;
	Akid k;
	int i;
	u32int q, l;

	if(a.q >= hi || a.q + a.len <= lo && a.len > 0)
		return;
	if(a.q >= lo && a.q + a.len <= hi){
		x = _mdtn(p, Mdtext, a.q, a.q + a.len, a.l);
		x->a = a;
		_mdincref(a.p);
		unit(u, x);
		return;
	}
	switch(a.p->kind){
	case Pcont:
		if(a.q >= lo && a.q < hi){
			x = _mdtn(p, Mdtext, a.q, a.q + a.len, a.l);
			x->a = a;
			_mdincref(a.p);
			unit(u, x);
		}
		return;
	case Prun:
		t = (Mdtree*)a.p;
		q = a.q;
		l = a.l;
		for(i = 0; i < t->nk; i++){
			q += t->k[i].d;
			l += t->k[i].dl;
			k.p = t->k[i].p;
			k.q = q;
			k.l = l;
			k.len = t->k[i].len;
			k.s = t->k[i].s;
			take(p, u, k, lo, hi);
		}
		return;
	case Pbuf:
		for(x = _mddecode(p, (Mdbuf*)a.p, a.q, a.l); x != nil; x = n){
			n = x->next;
			if(x->q0 >= lo && x->q0 < hi)
				unit(u, x);
		}
		return;
	}
}

static Tn*
decnode(P *p, Hd *h)
{
	Tn *x, *f;
	Mdbuf *b;
	int j;

	b = h->b;
	f = _mddecode(p, b, h->q, h->l);
	j = 0;
	for(x = f; x != nil; ){
		if(j == h->i)
			return x;
		j++;
		if(x->kid != nil){
			x = x->kid;
			continue;
		}
		while(x != nil && x->next == nil)
			x = x->up;
		if(x == nil)
			break;
		x = x->next;
	}
	return nil;
}

static void
kids(P *p, Hd *h, u32int lo, u32int hi, Ul *u)
{
	Mdtree *t;
	Akid a;
	int i;
	u32int q, l;
	Tn *x, *k, *n;

	if(h->t != nil){
		t = h->t;
		q = h->q;
		l = h->l;
		for(i = 0; i < t->nk; i++){
			q += t->k[i].d;
			l += t->k[i].dl;
			a.p = t->k[i].p;
			a.q = q;
			a.l = l;
			a.len = t->k[i].len;
			a.s = t->k[i].s;
			take(p, u, a, lo, hi);
		}
		return;
	}
	x = decnode(p, h);
	if(x == nil)
		return;
	for(k = x->kid; k != nil; k = n){
		n = k->next;
		if(k->q0 >= lo && k->q0 < hi)
			unit(u, k);
	}
}

static Blk*
oldblk(P *p, Ul *u)
{
	Blk *b;
	Tn *k;

	if(u->h == nil)
		return nil;
	b = _mdalloc(p, sizeof *b);
	b->isold = 1;
	b->oldk = u->h;
	b->q0 = u->h->q0;
	b->q1 = u->h->q1;
	b->line = u->h->line;
	for(k = u->h; k != nil; k = k->next)
		if(k->q1 > b->q1)
			b->q1 = k->q1;
	return b;
}

static void
kidblk(Blk *up, Blk *b)
{
	b->up = up;
	b->prev = up->last;
	if(up->last != nil)
		up->last->next = b;
	else
		up->kid = b;
	up->last = b;
}

static Blk*
reopen(P *p, Hd *h)
{
	Blk *b;

	b = _mdalloc(p, sizeof *b);
	b->type = h->n.type;
	b->c = h->n.c;
	b->w = h->n.w;
	b->aux = h->n.aux;
	b->flags = h->n.flags & ~(Fends|Fla|Flb|Mdtight|Fblank);
	b->q0 = h->n.q0;
	b->q1 = h->n.q0;
	b->line = h->n.line;
	b->open = 1;
	b->moff = h->n.w;
	b->pad = 0;
	return b;
}

static int
optype(Mdnode *n)
{
	if(n->type == Mddef || n->type == Mdtable || n->type == Mdhead && (n->flags & Mdsetext))
		return Mdpara;
	return n->type;
}

static int
same(Blk *b, Mdnode *o)
{
	if(b->type != optype(o))
		return 0;
	switch(b->type){
	case Mdlist:
		return (b->flags & Mdordered) == (o->flags & Mdordered) && b->c == o->c;
	case Mditem:
		return (b->flags & Mdordered) == (o->flags & Mdordered) && b->c == o->c && b->w == o->w;
	case Mdcode:
		if((b->flags & Mdfenced) != (o->flags & Mdfenced))
			return 0;
		return !(b->flags & Mdfenced) || b->c == o->c && b->w == o->w && b->aux == o->aux;
	case Mdhtml:
	case Mdhead:
		return b->aux == o->aux;
	case Mdtable:
	case Mdrow:
	case Mdcell:
		return 0;
	}
	return 1;
}

static int
oldat(Md *m, u32int o, Mdcur *c)
{
	Mdcur x;
	int found;

	memset(c, 0, sizeof *c);
	c->md = m;
	mdseek(c, o);
	found = 0;
	memset(&x, 0, sizeof x);
	for(;;){
		if(c->q0 == o && (c->flags & Fnew) && _mdisblock(c->type)){
			mdcurcopy(&x, c);
			found = 1;
		}
		if(c->q0 < o || !mdup(c))
			break;
	}
	mdcurfree(c);
	if(!found)
		return 0;
	*c = x;
	return 1;
}

static Hd*
path(Mdcur *c, int *np)
{
	Mdcur x;
	Hd *h;
	int n, i;
	Hd t;

	memset(&x, 0, sizeof x);
	mdcurcopy(&x, c);
	h = nil;
	n = 0;
	while(mdup(&x)){
		h = erealloc(h, (n+1)*sizeof h[0]);
		handle(&x, &h[n++]);
	}
	mdcurfree(&x);
	for(i = 0; i < n/2; i++){
		t = h[i];
		h[i] = h[n-1-i];
		h[n-1-i] = t;
	}
	*np = n;
	return h;
}

static int
converge(P *p, Md *m, long delta, long *dl, Hd **ohp, int *onp, Mdcur *oc)
{
	Blk *y, *a;
	Hd *h;
	int n, i, depth;

	y = p->opened;
	if(y == nil || !(y->flags & Fnew))
		return 0;
	if((long)y->q0 - delta < 0)
		return 0;
	if(!oldat(m, y->q0 - delta, oc))
		return 0;
	if(!same(y, (Mdnode*)oc)){
		mdcurfree(oc);
		return 0;
	}
	h = path(oc, &n);
	depth = 0;
	for(a = y->up; a != nil; a = a->up)
		depth++;
	if(depth != n){
		free(h);
		mdcurfree(oc);
		return 0;
	}
	a = y->up;
	for(i = n-1; i >= 0; i--, a = a->up){
		if(a->type != h[i].n.type || !same(a, &h[i].n)){
			free(h);
			mdcurfree(oc);
			return 0;
		}
		if(a->type == Mdtable){
			free(h);
			mdcurfree(oc);
			return 0;
		}
	}
	*dl = (long)y->line - (long)oc->line;
	*ohp = h;
	*onp = n;
	return 1;
}

static int
haskey(Mdref *r, int n, char *k)
{
	int lo, hi, m, c;

	lo = 0;
	hi = n;
	while(lo < hi){
		m = (lo+hi)/2;
		c = strcmp(k, r[m].key);
		if(c == 0)
			return 1;
		if(c < 0)
			hi = m;
		else
			lo = m+1;
	}
	return 0;
}

static int
hasit(u32int *h, int n, u32int x)
{
	int i;

	for(i = 0; i < n; i++)
		if(h[i] == x)
			return 1;
	return 0;
}

static void
inl1(P *p, Tn *t)
{
	Tn *k, **mk;
	int n, m, i;

	mk = nil;
	n = m = 0;
	for(k = t->kid; k != nil; ){
		if(k->type == Mdmark && _mdisblock(k->aux)){
			if(n >= m){
				m = m*2 + 8;
				mk = erealloc(mk, m*sizeof mk[0]);
			}
			mk[n++] = k;
		}
		if(k->kid != nil){
			k = k->kid;
			continue;
		}
		while(k != t && k->next == nil)
			k = k->up;
		if(k == t)
			break;
		k = k->next;
	}
	t->kid = t->last = nil;
	t->scan = nil;
	_mdinline(p, t, t->seg, t->nseg);
	for(i = 0; i < n; i++){
		k = mk[i];
		k->up = k->next = k->prev = k->kid = k->last = nil;
		k->scan = nil;
		_mdinsmark(t, k);
	}
	free(mk);
}

static Mdp*
fixbuf(P *p, Mdbuf *b, u32int q, u32int l, u32int *hs, int nh, uvlong *ob)
{
	Tn *f, *k, *last;
	int ch, i;
	Mdsum s;

	for(i = 0; i < b->nlab; i++)
		if(hasit(hs, nh, b->lab[i].hash))
			break;
	if(i == b->nlab)
		return nil;
	f = _mddecode(p, b, q, l);
	ch = 0;
	for(k = f; k != nil; ){
		if((k->type == Mdpara || k->type == Mdhead || k->type == Mdcell) && k->nseg > 0){
			for(i = 0; i < k->nlab; i++)
				if(hasit(hs, nh, k->lab[i]))
					break;
			if(i < k->nlab){
				inl1(p, k);
				ch = 1;
			}
		}else if(k->kid != nil && _mdisblock(k->type)){
			k = k->kid;
			continue;
		}
		while(k->up != nil && k->next == nil)
			k = k->up;
		k = k->next;
	}
	if(!ch)
		return nil;
	s.f = 0;
	s.bloom = 0;
	last = f;
	for(k = f; k != nil; k = k->next){
		s = _mdsumcat(s, _mdsumtn(k));
		last = k;
	}
	*ob = s.bloom;
	return (Mdp*)_mdencode(f, last, q, l);
}

static Mdp*
fixkid(P *p, Mdp *x, int shared, u32int q, u32int l, uvlong bl, u32int *hs, int nh, uvlong *ob)
{
	Mdtree *t, *c;
	int i, j;
	u32int kq, kl;
	Mdp *np;
	uvlong b, kb;

	if(x->kind == Pbuf)
		return fixbuf(p, (Mdbuf*)x, q, l, hs, nh, ob);
	t = (Mdtree*)x;
	shared |= t->ref > 1;
	c = nil;
	kq = q;
	kl = l;
	b = 0;
	for(i = 0; i < t->nk; i++){
		kq += t->k[i].d;
		kl += t->k[i].dl;
		kb = t->k[i].s.bloom;
		if(kb & bl){
			np = fixkid(p, t->k[i].p, shared, kq, kl, bl, hs, nh, &kb);
			if(np != nil){
				if(c == nil){
					if(!shared)
						c = t;
					else{
						c = emalloc(sizeof *c);
						*c = *t;
						c->ref = 1;
						c->k = emalloc((t->nk > 0 ? t->nk : 1)*sizeof c->k[0]);
						memmove(c->k, t->k, t->nk*sizeof c->k[0]);
						for(j = 0; j < c->nk; j++)
							_mdincref(c->k[j].p);
					}
				}
				if(np != c->k[i].p){
					_mddecref(c->k[i].p);
					c->k[i].p = np;
				}
				c->k[i].s.bloom = kb;
			}
		}
		b |= kb;
	}
	*ob = b;
	return (Mdp*)c;
}

static void
defsfrom(P *p, Md *m, u32int lo, u32int hi, long delta)
{
	int i;
	Mdref r;

	for(i = 0; i < m->nref; i++){
		r = m->ref[i];
		if(r.off >= lo && r.off < hi)
			continue;
		if(r.off >= hi)
			r.off += delta;
		r.key = _mdalloc(p, strlen(m->ref[i].key)+1);
		strcpy(r.key, m->ref[i].key);
		if(p->ndef >= p->mdef){
			p->mdef = p->mdef*2 + 8;
			p->def = erealloc(p->def, p->mdef*sizeof p->def[0]);
		}
		p->def[p->ndef++] = r;
	}
}

static ulong
linestart(char *s, ulong e)
{
	ulong j;

	for(j = e; j > 0; j--)
		if(s[j-1] == '\n' || s[j-1] == '\r' && s[j] != '\n')
			return j;
	return 0;
}

Md*
mdedit(Md *m, char *s, ulong n, ulong q0, ulong q1, ulong nq1)
{
	P *p;
	Mdcur yr, oc;
	Hd *h, *oh;
	int nh, onh, j, conv, nchg, i;
	long delta, dl;
	ulong ls, cut, e, L, ohi;
	Blk *b, *a, *ob, **chain;
	Ul u;
	Md *nm;
	Mdtree *root;
	Mdp *np;
	Tn *t;
	u32int *hs;
	uvlong bl, rb;

	if(q0 > q1 || q1 > m->n || q0 > nq1 || nq1 > n || n - nq1 != m->n - q1){
		werrstr("edit %lud-%lud to %lud-%lud does not fit lengths %lud and %lud", q0, q1, q0, nq1, m->n, n);
		return nil;
	}
	delta = (long)nq1 - (long)q1;
	p = _mdnewp(s, n);
	p->ext = m->ext;
	e = q0 > 0 ? q0-1 : 0;
	L = linestart(s, e);
	h = nil;
	nh = 0;
	cut = 0;
	ls = 0;
	memset(&yr, 0, sizeof yr);
	if(q0 > 0 && L > 0 && lastnew(m, L, &yr)){
		cut = linestart(s, yr.q0);
		h = path(&yr, &nh);
		chain = emalloc((nh+1)*sizeof chain[0]);
		for(j = 0; j < nh; j++){
			if(j == 0){
				b = p->root;
				b->flags = h[0].n.flags & ~(Fends|Fla|Flb|Mdtight|Fblank);
			}else{
				b = reopen(p, &h[j]);
				kidblk(chain[j-1], b);
			}
			chain[j] = b;
			memset(&u, 0, sizeof u);
			kids(p, &h[j], 0, j+1 < nh ? h[j+1].n.q0 : cut, &u);
			ob = oldblk(p, &u);
			if(ob != nil)
				kidblk(b, ob);
		}
		p->cur = chain[nh-1];
		p->line = yr.line;
		ls = cut;
		free(chain);
	}else if(n >= 3 && (uchar)s[0] == 0xEF && (uchar)s[1] == 0xBB && (uchar)s[2] == 0xBF)
		ls = 3;
	conv = 0;
	oh = nil;
	onh = 0;
	ohi = ~0UL;
	dl = 0;
	while(ls < n){
		_mdline(p, ls);
		if(ls > nq1 && converge(p, m, delta, &dl, &oh, &onh, &oc)){
			conv = 1;
			break;
		}
		ls = p->ln;
	}
	if(conv){
		Blk *y;

		y = p->opened;
		ohi = oc.q0;
		a = y->up;
		if(y->prev != nil)
			y->prev->next = nil;
		else
			a->kid = nil;
		a->last = y->prev;
		for(j = onh-1; j >= 0; j--, a = a->up){
			memset(&u, 0, sizeof u);
			u.dq = delta;
			u.dl = dl;
			kids(p, &oh[j], j == onh-1 ? oc.q0 : oh[j+1].n.q0 + 1, ~0U, &u);
			ob = oldblk(p, &u);
			if(ob != nil)
				kidblk(a, ob);
			a->zip = 1;
			a->zflags = oh[j].n.flags;
			a->zq1 = oh[j].n.q1 + delta;
		}
		p->cur = y->up;
		mdcurfree(&oc);
	}
	_mdfinish(p);
	_mdfinal(p, p->root);
	defsfrom(p, m, cut, conv ? ohi : ~0U, delta);
	_mdkeys(p);
	hs = emalloc((p->nkey + m->nref + 1)*sizeof hs[0]);
	nchg = 0;
	bl = 0;
	for(i = 0; i < p->nkey; i++)
		if(!haskey(m->ref, m->nref, p->key[i])){
			hs[nchg] = _mdhash(p->key[i]);
			bl |= _mdbloom(hs[nchg]);
			nchg++;
		}
	for(i = 0; i < m->nref; i++)
		if((i == 0 || strcmp(m->ref[i].key, m->ref[i-1].key) != 0) && !_mdlookup(p, m->ref[i].key)){
			hs[nchg] = _mdhash(m->ref[i].key);
			bl |= _mdbloom(hs[nchg]);
			nchg++;
		}
	t = _mdpack(p, p->root);
	root = (Mdtree*)t->a.p;
	if(nchg > 0){
		np = fixkid(p, (Mdp*)root, 0, 0, 0, bl, hs, nchg, &rb);
		if(np != nil && np != (Mdp*)root){
			_mddecref((Mdp*)root);
			root = (Mdtree*)np;
		}
	}
	nm = _mdsnap(p, root);
	nm->seq = m->seq + 1;
	nm->r0 = cut;
	nm->r1 = conv ? ohi + delta : n;
	free(hs);
	free(h);
	free(oh);
	mdcurfree(&yr);
	_mdfreep(p);
	return nm;
}
