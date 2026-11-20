#include <u.h>
#include <libc.h>
#include <regexp.h>
#include <tok.h>
#include "impl.h"

typedef struct List	List;
typedef struct Run	Run;

enum
{
	Left,
	All,

	Spc = 0,
	Sstart,
	Slast,
	Sfix,
};

struct List
{
	int	*v;
	int	n;
	int	m;
	uint	*hgen;
	int	*hidx;
	int	nh;
	uint	gen;
};

struct Run
{
	Tpat	*p;
	Toks	*t;
	int	t0;
	int	t1;
	int	w;
	int	ub;
	int	gb;
	int	mode;
	List	cur;
	List	nxt;
	List	seen;
	int	*work;
	int	*tmp;
	int	found;
	int	b0;
	int	b1;
	int	*best;
	int	cj;
};

static int*
put(Run *r, List *l, int *st)
{
	int *v;

	if(l->n == l->m){
		l->m = l->m ? 2*l->m : 64;
		l->v = erealloc(l->v, l->m*r->w*sizeof l->v[0]);
	}
	v = l->v + l->n++*r->w;
	memmove(v, st, r->w*sizeof v[0]);
	return v;
}

static uint
keyhash(Run *r, int *st)
{
	uint h;
	int i, c;

	h = st[Spc]*2654435761U ^ st[Slast];
	for(i = r->ub; i < r->gb; i++){
		c = st[i];
		if(i >= r->ub+r->p->nstar)
			c = c < 0 ? -1 : r->t->t[c].id;
		h = (h ^ c) * 16777619U;
	}
	return h;
}

static int
keyeq(Run *r, int *a, int *b)
{
	int i, x, y;

	if(a[Spc] != b[Spc] || a[Slast] != b[Slast])
		return 0;
	for(i = r->ub; i < r->ub+r->p->nstar; i++)
		if(a[i] != b[i])
			return 0;
	for(; i < r->gb; i++){
		x = a[i];
		y = b[i];
		if(x == y)
			continue;
		if(x < 0 || y < 0 || r->t->t[x].id != r->t->t[y].id)
			return 0;
	}
	return 1;
}

static void
rehash(Run *r, List *l)
{
	int i, j, nh;

	nh = l->nh ? 2*l->nh : 256;
	free(l->hgen);
	free(l->hidx);
	l->hgen = emalloc(nh*sizeof l->hgen[0]);
	l->hidx = emalloc(nh*sizeof l->hidx[0]);
	l->nh = nh;
	if(l->gen == 0)
		l->gen = 1;
	for(i = 0; i < l->n; i++){
		j = keyhash(r, l->v+i*r->w) & (nh-1);
		while(l->hgen[j] == l->gen)
			j = (j+1) & (nh-1);
		l->hgen[j] = l->gen;
		l->hidx[j] = i;
	}
}

static int
find(Run *r, List *l, int *st)
{
	int j;

	if(2*(l->n+1) > l->nh)
		rehash(r, l);
	j = keyhash(r, st) & (l->nh-1);
	for(; l->hgen[j] == l->gen; j = (j+1) & (l->nh-1))
		if(keyeq(r, l->v+l->hidx[j]*r->w, st))
			return l->hidx[j];
	l->hgen[j] = l->gen;
	l->hidx[j] = l->n;
	put(r, l, st);
	return -1;
}

static void
clear(List *l)
{
	l->n = 0;
	l->gen++;
	if(l->gen == 0){
		if(l->hgen != nil)
			memset(l->hgen, 0, l->nh*sizeof l->hgen[0]);
		l->gen = 1;
	}
}

static void
add(Run *r, List *l, int *st)
{
	int i, *v;

	if((i = find(r, l, st)) < 0)
		return;
	v = l->v+i*r->w;
	if(st[Sstart] >= v[Sstart])
		return;
	memmove(v, st, r->w*sizeof v[0]);
	if(l == &r->cur && i <= r->cj)
		put(r, l, st);
}

static void
record(Run *r, int *st)
{
	int s, e, better;

	s = st[Sstart];
	e = st[Slast];
	if(!r->found)
		better = 1;
	else if(r->mode == Left)
		better = s < r->b0 || s == r->b0 && e > r->b1;
	else
		better = e > r->b1 || e == r->b1 && s < r->b0;
	if(!better)
		return;
	r->found = 1;
	r->b0 = s;
	r->b1 = e;
	memmove(r->best, st, r->w*sizeof st[0]);
}

static int
test(Test *t, Tok *k)
{
	int i, ok;

	ok = 0;
	switch(t->type){
	case Tlit:
		ok = strcmp(k->s, t->s) == 0;
		break;
	case Tany:
		ok = 1;
		break;
	case Tname:
		ok = tokhas(k->name, t->s);
		break;
	case Tre:
		ok = regexec(t->re, k->s, nil, 0);
		break;
	case Tset:
		for(i = 0; i < t->nset && !ok; i++)
			ok = test(t->set[i], k);
		break;
	}
	ok ^= t->neg;
	return ok && (t->tag == nil || tokhas(k->tag, t->tag));
}

static int
step(Run *r, int *st, int pos)
{
	Inst *i;
	Tok *k, *tt;
	Test *t;
	int *u, *cap, j, s, c, ok;

	i = &r->p->i[st[Spc]];
	t = i->t;
	tt = r->t->t;
	k = &tt[pos];
	u = st+r->ub;
	cap = u+r->p->nstar;
	if(t->type == Tref){
		c = cap[t->ref];
		ok = (c >= 0 && tt[c].id == k->id) ^ t->neg;
		ok = ok && (t->tag == nil || tokhas(k->tag, t->tag));
	}else{
		ok = test(t, k);
		if(ok && i->bind >= 0){
			c = cap[i->bind];
			if(c < 0)
				cap[i->bind] = pos;
			else
				ok = tt[c].id == k->id;
		}
	}
	if(!ok)
		return 0;
	if(k->link >= 0 && i->nstars > 0){
		if(k->link < r->t0 || k->link >= r->t1)
			return 0;
		for(j = 0; j < i->nstars; j++){
			s = i->stars[j];
			if(k->link > pos){
				if(u[s] < 0)
					u[s] = k->link;
			}else if(u[s] == pos)
				u[s] = -1;
			else if(u[s] < 0)
				return 0;
		}
	}
	st[Spc]++;
	st[Slast] = pos+1;
	return 1;
}

static void
chain(Run *r, int *st, int pos)
{
	Inst *i;
	int *u, *g, *v, k;

	u = st+r->ub;
	g = st+r->gb;
	for(;;){
		if((k = find(r, &r->seen, st)) >= 0){
			v = r->seen.v+k*r->w;
			if(st[Sstart] >= v[Sstart])
				return;
			v[Sstart] = st[Sstart];
		}
		if(r->mode == Left && r->found && st[Sstart] > r->b0)
			return;
		i = &r->p->i[st[Spc]];
		switch(i->op){
		case Itok:
			if(pos < r->t1 && step(r, st, pos))
				add(r, &r->nxt, st);
			return;
		case Ijmp:
			st[Spc] = i->x;
			break;
		case Isplit:
			memmove(r->tmp, st, r->w*sizeof st[0]);
			r->tmp[Spc] = i->y;
			add(r, &r->cur, r->tmp);
			st[Spc] = i->x;
			break;
		case Isbeg:
			u[i->star] = -1;
			st[Spc]++;
			break;
		case Isend:
			if(u[i->star] != -1)
				return;
			st[Spc]++;
			break;
		case Igopen:
			g[2*i->grp] = pos;
			g[2*i->grp+1] = -1;
			st[Spc]++;
			break;
		case Igclose:
			if(st[Slast] > g[2*i->grp])
				g[2*i->grp+1] = st[Slast];
			else
				g[2*i->grp] = g[2*i->grp+1] = st[Slast];
			st[Spc]++;
			break;
		case Imatch:
			record(r, st);
			return;
		}
	}
}

static int
run(Run *r, int from)
{
	List l;
	int *st, pos, j;

	st = r->work;
	clear(&r->cur);
	clear(&r->nxt);
	r->cj = -1;
	for(pos = from; pos <= r->t1; pos++){
		if(pos < r->t1 && r->t->t[pos].comment && !r->p->comments)
			continue;
		if(r->mode == All || !r->found){
			memset(st, 0xff, r->w*sizeof st[0]);
			st[Spc] = 0;
			st[Sstart] = pos;
			st[Slast] = pos;
			add(r, &r->cur, st);
		}
		if(r->cur.n == 0){
			if(r->found && r->mode == Left)
				break;
			continue;
		}
		clear(&r->seen);
		clear(&r->nxt);
		for(j = 0; j < r->cur.n; j++){
			r->cj = j;
			memmove(st, r->cur.v+j*r->w, r->w*sizeof st[0]);
			chain(r, st, pos);
		}
		r->cj = -1;
		l = r->cur;
		r->cur = r->nxt;
		r->nxt = l;
	}
	return r->found;
}

static void
fill(Run *r, Tmatch *m)
{
	Tpat *p;
	int i, b, g, *st;

	p = r->p;
	st = r->best;
	m->t0 = r->b0;
	m->t1 = r->b1;
	for(i = 0; i < Tmaxcap; i++){
		m->cap[i][0] = m->cap[i][1] = -1;
		if(i >= p->ncap)
			continue;
		if((b = p->capbind[i]) >= 0){
			if(st[r->ub+p->nstar+b] >= 0){
				m->cap[i][0] = st[r->ub+p->nstar+b];
				m->cap[i][1] = m->cap[i][0]+1;
			}
		}else if((g = p->capgrp[i]) >= 0 && st[r->gb+2*g+1] >= 0){
			m->cap[i][0] = st[r->gb+2*g];
			m->cap[i][1] = st[r->gb+2*g+1];
		}
	}
}

static void
freelist(List *l)
{
	free(l->v);
	free(l->hgen);
	free(l->hidx);
}

static int
texec(Tpat *p, Toks *t, int t0, int t1, int from, int mode, Tmatch *m)
{
	Run r;
	int ok;

	if(t0 < 0)
		t0 = 0;
	if(t1 > t->n)
		t1 = t->n;
	if(t1 < t0)
		t1 = t0;
	if(from < t0)
		from = t0;
	if(from > t1)
		from = t1;
	memset(&r, 0, sizeof r);
	r.p = p;
	r.t = t;
	r.t0 = t0;
	r.t1 = t1;
	r.mode = mode;
	r.ub = Sfix;
	r.gb = Sfix+p->nstar+p->nbind;
	r.w = r.gb+2*p->ngrp;
	r.work = emalloc(r.w*sizeof r.work[0]);
	r.tmp = emalloc(r.w*sizeof r.tmp[0]);
	r.best = emalloc(r.w*sizeof r.best[0]);
	ok = run(&r, from);
	if(ok && m != nil)
		fill(&r, m);
	freelist(&r.cur);
	freelist(&r.nxt);
	freelist(&r.seen);
	free(r.work);
	free(r.tmp);
	free(r.best);
	return ok;
}

int
tokexec(Tpat *p, Toks *t, int t0, int t1, int from, Tmatch *m)
{
	return texec(p, t, t0, t1, from, Left, m);
}

int
tokbexec(Tpat *p, Toks *t, int t0, int t1, Tmatch *m)
{
	return texec(p, t, t0, t1, t0, All, m);
}
