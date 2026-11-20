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
};

struct List
{
	int	*v;
	int	n;
	int	m;
};

struct Run
{
	Tpat	*p;
	Toks	*t;
	int	t0;
	int	t1;
	int	w;
	int	mode;
	int	end;
	List	cur;
	List	nxt;
	List	stk;
	List	seen;
	int	*work;
	uint	*hgen;
	int	*hidx;
	int	nh;
	uint	gen;
	int	found;
	int	b0;
	int	b1;
	int	bind[Tmaxbind];
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
	int i, c, n;

	h = st[0]*2654435761U;
	n = 2+r->p->nstar;
	for(i = 2; i < n; i++)
		h = (h ^ st[i]) * 16777619U;
	for(; i < r->w; i++){
		c = st[i];
		h = (h ^ (c < 0 ? -1 : r->t->t[c].id)) * 16777619U;
	}
	return h;
}

static int
keyeq(Run *r, int *a, int *b)
{
	int i, n, x, y;

	if(a[0] != b[0])
		return 0;
	n = 2+r->p->nstar;
	for(i = 2; i < n; i++)
		if(a[i] != b[i])
			return 0;
	for(; i < r->w; i++){
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
rehash(Run *r)
{
	int i, j, nh;

	nh = r->nh ? 2*r->nh : 256;
	free(r->hgen);
	free(r->hidx);
	r->hgen = emalloc(nh*sizeof r->hgen[0]);
	r->hidx = emalloc(nh*sizeof r->hidx[0]);
	r->nh = nh;
	for(i = 0; i < r->seen.n; i++){
		j = keyhash(r, r->seen.v+i*r->w) & (nh-1);
		while(r->hgen[j] == r->gen)
			j = (j+1) & (nh-1);
		r->hgen[j] = r->gen;
		r->hidx[j] = i;
	}
}

static int
seen(Run *r, int *st)
{
	int j;

	if(2*(r->seen.n+1) > r->nh)
		rehash(r);
	j = keyhash(r, st) & (r->nh-1);
	for(; r->hgen[j] == r->gen; j = (j+1) & (r->nh-1))
		if(keyeq(r, r->seen.v+r->hidx[j]*r->w, st))
			return 1;
	r->hgen[j] = r->gen;
	r->hidx[j] = r->seen.n;
	put(r, &r->seen, st);
	return 0;
}

static void
newgen(Run *r)
{
	r->gen++;
	if(r->gen == 0){
		memset(r->hgen, 0, r->nh*sizeof r->hgen[0]);
		r->gen = 1;
	}
	r->seen.n = 0;
}

static void
record(Run *r, int *st)
{
	int s, better;

	s = st[1];
	if(!r->found)
		better = 1;
	else if(r->mode == Left)
		better = s < r->b0 || s == r->b0 && r->end > r->b1;
	else
		better = r->end > r->b1 || r->end == r->b1 && s < r->b0;
	if(!better)
		return;
	r->found = 1;
	r->b0 = s;
	r->b1 = r->end;
	memmove(r->bind, st+2+r->p->nstar, r->p->nbind*sizeof r->bind[0]);
}

static void
closure(Run *r, List *l, int *st0)
{
	Inst *i;
	int *st, *u;

	r->stk.n = 0;
	put(r, &r->stk, st0);
	st = r->work;
	u = st+2;
	while(r->stk.n > 0){
		r->stk.n--;
		memmove(st, r->stk.v+r->stk.n*r->w, r->w*sizeof st[0]);
		if(seen(r, st))
			continue;
		i = &r->p->i[st[0]];
		switch(i->op){
		case Itok:
			put(r, l, st);
			break;
		case Ijmp:
			st[0] = i->x;
			put(r, &r->stk, st);
			break;
		case Isplit:
			st[0] = i->y;
			put(r, &r->stk, st);
			st[0] = i->x;
			put(r, &r->stk, st);
			break;
		case Isbeg:
			u[i->star] = -1;
			st[0]++;
			put(r, &r->stk, st);
			break;
		case Isend:
			if(u[i->star] == -1){
				st[0]++;
				put(r, &r->stk, st);
			}
			break;
		case Imatch:
			record(r, st);
			break;
		}
	}
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
		ok = tokhas(k->name, t->s) && (t->tag == nil || tokhas(k->tag, t->tag));
		break;
	case Tre:
		ok = regexec(t->re, k->s, nil, 0);
		break;
	case Tset:
		for(i = 0; i < t->nset && !ok; i++)
			ok = test(t->set[i], k);
		break;
	}
	return ok ^ t->neg;
}

static int
step(Run *r, int *st, int pos)
{
	Inst *i;
	Tok *k, *tt;
	Test *t;
	int *u, *cap, j, s, c, ok;

	i = &r->p->i[st[0]];
	t = i->t;
	tt = r->t->t;
	k = &tt[pos];
	u = st+2;
	cap = u+r->p->nstar;
	if(t->type == Tref){
		c = cap[t->ref];
		ok = c < 0 ? t->neg : (tt[c].id == k->id) ^ t->neg;
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
	st[0]++;
	return 1;
}

static int
run(Run *r, int from)
{
	List l;
	int *st, *v, pos, j, n, stale;

	st = emalloc(r->w*sizeof st[0]);
	newgen(r);
	stale = 0;
	for(pos = from; pos <= r->t1; pos++){
		if(pos < r->t1 && r->t->t[pos].comment && !r->p->comments){
			stale = 1;
			continue;
		}
		if(stale){
			newgen(r);
			for(j = 0; j < r->cur.n; j++)
				seen(r, r->cur.v+j*r->w);
			stale = 0;
		}
		if(r->mode == All || !r->found){
			memset(st, 0xff, r->w*sizeof st[0]);
			st[0] = 0;
			st[1] = pos;
			r->end = pos;
			closure(r, &r->cur, st);
		}
		if(pos == r->t1)
			break;
		if(r->cur.n == 0){
			if(r->found && r->mode == Left)
				break;
			continue;
		}
		newgen(r);
		r->nxt.n = 0;
		r->end = pos+1;
		n = r->cur.n;
		for(j = 0; j < n; j++){
			v = r->cur.v+j*r->w;
			if(r->mode == Left && r->found && v[1] > r->b0)
				continue;
			memmove(st, v, r->w*sizeof st[0]);
			if(step(r, st, pos))
				closure(r, &r->nxt, st);
		}
		l = r->cur;
		r->cur = r->nxt;
		r->nxt = l;
	}
	free(st);
	return r->found;
}

static int
texec(Tpat *p, Toks *t, int t0, int t1, int from, int mode, Tmatch *m)
{
	Run r;
	int i, ok;

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
	r.w = 2+p->nstar+p->nbind;
	r.work = emalloc(r.w*sizeof r.work[0]);
	ok = run(&r, from);
	if(ok && m != nil){
		m->t0 = r.b0;
		m->t1 = r.b1;
		for(i = 0; i < Tmaxbind; i++)
			m->bind[i] = i < p->nbind ? r.bind[i] : -1;
	}
	free(r.cur.v);
	free(r.nxt.v);
	free(r.stk.v);
	free(r.seen.v);
	free(r.work);
	free(r.hgen);
	free(r.hidx);
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
