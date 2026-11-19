#include <u.h>
#include <libc.h>
#include <md.h>
#include "impl.h"

void
_mdcurpush(Mdcur *c, int kind, Mdp *p, int i, u32int q, u32int l)
{
	Mdframe *f;

	if(c->nf >= c->mf){
		c->mf = c->mf*2 + 16;
		c->f = erealloc(c->f, c->mf*sizeof c->f[0]);
	}
	f = &c->f[c->nf++];
	f->kind = kind;
	f->p = p;
	f->i = i;
	f->q = q;
	f->l = l;
}

static Mdframe*
top(Mdcur *c)
{
	return &c->f[c->nf-1];
}

Mdn*
_mdcurn(Mdcur *c)
{
	Mdframe *f;

	f = top(c);
	if(f->kind == Rbuf || f->kind == Rnode)
		return &((Mdbuf*)f->p)->n[f->i];
	if(f->kind == Rroot)
		return &((Mdtree*)f->p)->n;
	return &((Mdtree*)((Mdtree*)f->p)->k[f->i].p)->n;
}

void
_mdcurfill(Mdcur *c)
{
	Mdframe *f;
	Mdn *n;

	f = top(c);
	n = _mdcurn(c);
	c->type = n->type;
	c->flags = n->flags;
	c->aux = n->aux;
	c->c = n->c;
	c->w = n->w;
	if(f->kind == Rbuf || f->kind == Rnode){
		c->q0 = f->q + n->off;
		c->line = f->l + n->line;
	}else{
		c->q0 = f->q;
		c->line = f->l;
	}
	c->q1 = c->q0 + n->len;
}

static void
norm(Mdcur *c)
{
	Mdframe *f;
	Mdp *k;
	Mdtree *r;

	for(;;){
		f = top(c);
		if(f->kind != Rseq && f->kind != Rrun)
			return;
		k = ((Mdtree*)f->p)->k[f->i].p;
		if(k->kind == Prun){
			r = (Mdtree*)k;
			_mdcurpush(c, Rrun, k, 0, f->q + r->k[0].d, f->l + r->k[0].dl);
			continue;
		}
		if(k->kind == Pbuf)
			_mdcurpush(c, Rbuf, k, 0, f->q, f->l);
		return;
	}
}

void
mdroot(Md *m, Mdcur *c)
{
	c->md = m;
	c->nf = 0;
	_mdcurpush(c, Rroot, (Mdp*)m->root, -1, 0, 0);
	_mdcurfill(c);
}

static Mdtree*
curtree(Mdcur *c)
{
	Mdframe *f;

	f = top(c);
	if(f->kind == Rroot)
		return (Mdtree*)f->p;
	if(f->kind == Rseq || f->kind == Rrun)
		return (Mdtree*)((Mdtree*)f->p)->k[f->i].p;
	return nil;
}

int
mddown(Mdcur *c)
{
	Mdframe *f;
	Mdtree *t;
	Mdbuf *b;
	u32int q, l;

	f = top(c);
	t = curtree(c);
	if(t != nil){
		if(t->nk == 0)
			return 0;
		q = f->q;
		l = f->l;
		_mdcurpush(c, Rseq, (Mdp*)t, 0, q + t->k[0].d, l + t->k[0].dl);
		norm(c);
		_mdcurfill(c);
		return 1;
	}
	b = (Mdbuf*)f->p;
	if(b->n[f->i].kid == 0)
		return 0;
	_mdcurpush(c, Rnode, (Mdp*)b, b->n[f->i].kid, f->q, f->l);
	_mdcurfill(c);
	return 1;
}

static int
advance(Mdcur *c)
{
	int j, k;
	Mdframe *f;
	Mdtree *t;

	j = c->nf-1;
	if(c->f[j].kind == Rbuf)
		j--;
	for(k = j; k >= 0; k--){
		f = &c->f[k];
		if(f->kind != Rseq && f->kind != Rrun)
			return 0;
		t = (Mdtree*)f->p;
		if(f->i+1 < t->nk)
			break;
		if(f->kind == Rseq)
			return 0;
	}
	if(k < 0)
		return 0;
	c->nf = k+1;
	f = &c->f[k];
	t = (Mdtree*)f->p;
	f->i++;
	f->q += t->k[f->i].d;
	f->l += t->k[f->i].dl;
	norm(c);
	return 1;
}

int
mdnext(Mdcur *c)
{
	Mdframe *f;
	Mdbuf *b;

	f = top(c);
	switch(f->kind){
	case Rroot:
		return 0;
	case Rnode:
	case Rbuf:
		b = (Mdbuf*)f->p;
		if(b->n[f->i].next != 0){
			f->i = b->n[f->i].next;
			_mdcurfill(c);
			return 1;
		}
		if(f->kind == Rnode)
			return 0;
	}
	if(!advance(c))
		return 0;
	_mdcurfill(c);
	return 1;
}

int
mdup(Mdcur *c)
{
	Mdframe *f;

	f = top(c);
	if(f->kind == Rroot)
		return 0;
	if(f->kind == Rnode){
		c->nf--;
		_mdcurfill(c);
		return 1;
	}
	while(c->nf > 1){
		f = top(c);
		c->nf--;
		if(f->kind == Rseq)
			break;
	}
	_mdcurfill(c);
	return 1;
}

static int
into(Mdcur *c, ulong q)
{
	Mdframe *f;
	Mdtree *t;
	Mdbuf *b;
	Mdp *k;
	int i, j, best, base, kind;
	u32int s, ls, bs, bl;
	Mdn *n;

	f = top(c);
	t = curtree(c);
	if(t != nil){
		base = c->nf;
		s = f->q;
		ls = f->l;
		kind = Rseq;
		for(;;){
			best = -1;
			bs = bl = 0;
			for(i = 0; i < t->nk; i++){
				s += t->k[i].d;
				ls += t->k[i].dl;
				if(s > q)
					break;
				if(q < s + t->k[i].len){
					best = i;
					bs = s;
					bl = ls;
				}
			}
			if(best < 0)
				break;
			_mdcurpush(c, kind, (Mdp*)t, best, bs, bl);
			k = t->k[best].p;
			if(k->kind == Prun){
				t = (Mdtree*)k;
				s = bs;
				ls = bl;
				kind = Rrun;
				continue;
			}
			if(k->kind == Pcont){
				_mdcurfill(c);
				return 1;
			}
			b = (Mdbuf*)k;
			for(j = 0; ; j = b->n[j].next){
				n = &b->n[j];
				if(bs + n->off <= q && q < bs + n->off + n->len){
					_mdcurpush(c, Rbuf, (Mdp*)b, j, bs, bl);
					_mdcurfill(c);
					return 1;
				}
				if(n->next == 0)
					break;
			}
			break;
		}
		c->nf = base;
		_mdcurfill(c);
		return 0;
	}
	b = (Mdbuf*)f->p;
	for(j = b->n[f->i].kid; j != 0; j = b->n[j].next){
		n = &b->n[j];
		if(f->q + n->off <= q && q < f->q + n->off + n->len){
			_mdcurpush(c, Rnode, (Mdp*)b, j, f->q, f->l);
			_mdcurfill(c);
			return 1;
		}
	}
	return 0;
}

int
mdseek(Mdcur *c, ulong q)
{
	int n;

	mdroot(c->md, c);
	n = 0;
	while(into(c, q))
		n++;
	return n;
}

void
mdcurfree(Mdcur *c)
{
	free(c->f);
	c->f = nil;
	c->nf = c->mf = 0;
}

void
mdcurcopy(Mdcur *x, Mdcur *c)
{
	memset(x, 0, sizeof *x);
	x->md = c->md;
	x->f = emalloc(c->mf*sizeof c->f[0]);
	x->mf = c->mf;
	x->nf = c->nf;
	memmove(x->f, c->f, c->nf*sizeof c->f[0]);
	_mdcurfill(x);
}

static char*
gather(Mdcur *c, ulong a, ulong b)
{
	Mdcur x;
	char *s, *k;
	int n, d;
	ulong q;

	s = emalloc(b-a+1);
	n = 0;
	mdcurcopy(&x, c);
	q = a;
	if(mddown(&x)){
		d = 1;
		for(;;){
			if(x.type == Mdmark && x.aux == Mdquote && x.q0 >= q && x.q1 <= b){
				memmove(s+n, c->md->s+q, x.q0-q);
				n += x.q0 - q;
				q = x.q1;
			}else if(x.q0 < b && x.q1 > q && mddown(&x)){
				d++;
				continue;
			}
			while(!mdnext(&x)){
				if(--d == 0)
					goto done;
				mdup(&x);
			}
		}
	}
done:
	if(q < b){
		memmove(s+n, c->md->s+q, b-q);
		n += b-q;
	}
	mdcurfree(&x);
	k = _mdnorm((uchar*)s, n);
	free(s);
	return k;
}

int
mddef(Mdcur *c, Mdcur *d)
{
	Mdcur x;
	ulong a, b, close;
	char *k;
	int lo, hi, m, typ, lab;
	Md *md;

	md = c->md;
	typ = c->type;
	if(typ != Mdlink && typ != Mdimage || (c->flags & (Mdfull|Mdcollapsed|Mdshortcut)) == 0)
		return 0;
	mdcurcopy(&x, c);
	if(!mddown(&x)){
		mdcurfree(&x);
		return 0;
	}
	a = x.q1;
	b = a;
	close = a;
	lab = 0;
	do{
		if((c->flags & Mdfull) && x.type == Mdlabel){
			a = x.q0+1;
			b = x.q1-1;
			lab = 1;
			break;
		}
		if(x.type == Mdmark && x.aux == typ && x.q1 - x.q0 == 1 && md->s[x.q0] == ']')
			close = x.q0;
	}while(mdnext(&x));
	if(!lab){
		b = close;
		mdup(&x);
	}
	k = gather(&x, a, b);
	mdcurfree(&x);
	lo = 0;
	hi = md->nref;
	while(lo < hi){
		m = (lo+hi)/2;
		if(strcmp(k, md->ref[m].key) <= 0)
			hi = m;
		else
			lo = m+1;
	}
	if(lo >= md->nref || strcmp(k, md->ref[lo].key) != 0){
		free(k);
		return 0;
	}
	free(k);
	d->md = md;
	mdseek(d, md->ref[lo].off);
	while(d->type != Mddef || d->q0 != md->ref[lo].off)
		if(!mdup(d))
			return 0;
	return 1;
}
