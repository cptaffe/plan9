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
	Anchor,

	Spc = 0,
	Sstart,
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
	int	*vis;
	int	*rank;
	int	j0;
	int	j1;
	int	back;
	int	mode;
	int	w;
	int	ub;
	int	fb;
	int	bb;
	int	cb;
	List	l[2];
	int	*work;
	int	*tmp;
	int	found;
	int	b0;
	int	b1;
	int	o0;
	int	o1;
	int	t1;
	int	*best;
};

static Tok*
vtok(Run *r, int j)
{
	return &r->t->t[r->vis != nil ? r->vis[j] : j];
}

static int
vlink(Run *r, int j)
{
	int l;

	if((l = vtok(r, j)->link) < 0)
		return -1;
	if(r->vis != nil){
		if(r->t->t[l].comment)
			return -1;
		l = r->rank[l];
	}
	if(l < r->j0 || l >= r->j1)
		return -2;
	return l;
}

static void
put(Run *r, List *l, int *st)
{
	if(l->n == l->m){
		l->m = l->m ? 2*l->m : 64;
		l->v = erealloc(l->v, l->m*r->w*sizeof l->v[0]);
	}
	memmove(l->v + l->n++*r->w, st, r->w*sizeof st[0]);
}

static uint
keyhash(Run *r, int *st)
{
	uint h;
	int i, c;

	h = st[Spc]*2654435761U;
	for(i = r->ub; i < r->cb; i++){
		c = st[i];
		if(i >= r->bb)
			c = c < 0 ? -1 : vtok(r, c)->id;
		h = (h ^ c) * 16777619U;
	}
	return h;
}

static int
keyeq(Run *r, int *a, int *b)
{
	int i, x, y;

	if(a[Spc] != b[Spc])
		return 0;
	for(i = r->ub; i < r->bb; i++)
		if(a[i] != b[i])
			return 0;
	for(; i < r->cb; i++){
		x = a[i];
		y = b[i];
		if(x == y)
			continue;
		if(x < 0 || y < 0 || vtok(r, x)->id != vtok(r, y)->id)
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
		while(l->hgen[j] == l->gen && !keyeq(r, l->v+l->hidx[j]*r->w, l->v+i*r->w))
			j = (j+1) & (nh-1);
		l->hgen[j] = l->gen;
		l->hidx[j] = i;
	}
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

static int
earlier(Run *r, int a, int b)
{
	return r->back ? a > b : a < b;
}

static void
addinst(Run *r, List *l, int *st, int from)
{
	int j, *v;

	if(2*(l->n+1) > l->nh)
		rehash(r, l);
	j = keyhash(r, st) & (l->nh-1);
	for(; l->hgen[j] == l->gen; j = (j+1) & (l->nh-1)){
		v = l->v+l->hidx[j]*r->w;
		if(!keyeq(r, v, st))
			continue;
		if(!earlier(r, st[Sstart], v[Sstart]))
			return;
		if(l->hidx[j] >= from){
			memmove(v, st, r->w*sizeof v[0]);
			return;
		}
		break;
	}
	l->hgen[j] = l->gen;
	l->hidx[j] = l->n;
	put(r, l, st);
}

static int
epos(Run *r, int j)
{
	if(r->vis == nil)
		return j;
	return j < r->j1 ? r->vis[j] : r->t1;
}

static void
record(Run *r, int *st, int pos)
{
	int s, e, os, oe, better;

	s = r->back ? pos : st[Sstart];
	e = r->back ? st[Sstart] : pos;
	os = epos(r, s);
	oe = e > s ? epos(r, e-1)+1 : os;
	if(!r->found)
		better = 1;
	else if(r->mode == All || r->back)
		better = oe > r->o1 || oe == r->o1 && os < r->o0;
	else
		better = os < r->o0 || os == r->o0 && oe > r->o1;
	if(!better)
		return;
	r->found = 1;
	r->b0 = s;
	r->b1 = e;
	r->o0 = os;
	r->o1 = oe;
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

enum
{
	Fneed = 1,
	Fforbid = 2,
};

static int
same(Run *r, int c, Tok *k)
{
	return vtok(r, c)->id == k->id;
}

static int
bguard(Run *r, int *st, Inst *i, Tok *k, int j)
{
	Test *t;
	int *bd, *fl, *cap, c, x, cb;

	t = i->t;
	bd = st+r->bb;
	fl = st+r->fb;
	cap = st+r->cb;
	if(t->type == Tref){
		x = t->ref;
		c = bd[x];
		if(r->p->bunsafe){
			if(!t->neg){
				if(c >= 0 && !same(r, c, k))
					return 0;
				if(c < 0)
					bd[x] = j;
			}
		}else if(t->neg){
			if(c >= 0 && same(r, c, k))
				fl[x] |= Fforbid;
		}else{
			if(c >= 0 && !same(r, c, k))
				return 0;
			if(c < 0)
				bd[x] = j;
			fl[x] |= Fneed;
		}
		return t->tag == nil || tokhas(k->tag, t->tag);
	}
	if(!test(t, k))
		return 0;
	if((x = i->bind) < 0)
		return 1;
	c = bd[x];
	if(c >= 0 && !same(r, c, k))
		return 0;
	if(!r->p->bunsafe){
		if(fl[x] & Fforbid)
			return 0;
		fl[x] &= ~Fneed;
	}
	if(c < 0)
		bd[x] = j;
	cb = 2*r->p->bcap[x];
	cap[cb] = j;
	cap[cb+1] = j+1;
	return 1;
}

static int
guard(Run *r, int *st, Inst *i, Tok *k, int j)
{
	Test *t;
	int *bd, *cap, c, cb;

	if(r->back)
		return bguard(r, st, i, k, j);
	t = i->t;
	bd = st+r->bb;
	cap = st+r->cb;
	if(t->type == Tref){
		c = bd[t->ref];
		return ((c >= 0 && same(r, c, k)) ^ t->neg) && (t->tag == nil || tokhas(k->tag, t->tag));
	}
	if(!test(t, k))
		return 0;
	if(i->bind < 0)
		return 1;
	c = bd[i->bind];
	if(c >= 0)
		return same(r, c, k);
	bd[i->bind] = j;
	cb = 2*r->p->bcap[i->bind];
	cap[cb] = j;
	cap[cb+1] = j+1;
	return 1;
}

static int
needs(Run *r, int *st)
{
	int x;

	if(!r->back || r->p->bunsafe)
		return 0;
	for(x = 0; x < r->p->nbind; x++)
		if(st[r->fb+x] & Fneed)
			return 1;
	return 0;
}

static int
step(Run *r, int *st, int j)
{
	Inst *i;
	int *u, m, s, l, open;

	i = &r->p->i[st[Spc]];
	if(!guard(r, st, i, vtok(r, j), j))
		return 0;
	if(i->nstars > 0 && (l = vlink(r, j)) != -1){
		if(l == -2)
			return 0;
		u = st+r->ub;
		open = r->back ? l < j : l > j;
		for(m = 0; m < i->nstars; m++){
			s = i->stars[m];
			if(open){
				if(u[s] < 0)
					u[s] = l;
			}else if(u[s] == j)
				u[s] = -1;
			else if(u[s] < 0)
				return 0;
		}
	}
	st[Spc] = i->next;
	return 1;
}

static void
chain(Run *r, int *st, int pos, int cur)
{
	Inst *i;
	int *u, *cap;

	u = st+r->ub;
	cap = st+r->cb;
	for(;;){
		i = &r->p->i[st[Spc]];
		switch(i->op){
		case Itok:
			if(r->back){
				if(pos > r->j0 && step(r, st, pos-1))
					addinst(r, &r->l[1], st, 0);
			}else if(pos < r->j1 && step(r, st, pos))
				addinst(r, &r->l[1], st, 0);
			return;
		case Inop:
			st[Spc] = i->next;
			break;
		case Ior:
			memmove(r->tmp, st, r->w*sizeof st[0]);
			r->tmp[Spc] = i->right;
			addinst(r, &r->l[0], r->tmp, cur);
			st[Spc] = i->next;
			break;
		case Ilbra:
			cap[2*i->sub] = pos;
			st[Spc] = i->next;
			break;
		case Irbra:
			cap[2*i->sub+1] = pos;
			st[Spc] = i->next;
			break;
		case Isbeg:
			u[i->star] = -1;
			st[Spc] = i->next;
			break;
		case Isend:
			if(u[i->star] != -1)
				return;
			st[Spc] = i->next;
			break;
		case Iend:
			if(!needs(r, st))
				record(r, st, pos);
			return;
		}
	}
}

static int
pruned(Run *r, int *v)
{
	if(!r->found || r->mode == All)
		return 0;
	if(r->back)
		return v[Sstart] < r->b1;
	return v[Sstart] > r->b0;
}

static int
run(Run *r, int from)
{
	List l;
	int *st, *v, pos, j, d, end;

	st = r->work;
	clear(&r->l[0]);
	clear(&r->l[1]);
	d = r->back ? -1 : 1;
	end = r->back ? r->j0 : r->j1;
	for(pos = from; ; pos += d){
		if(r->mode == All || !r->found && (r->mode != Anchor || pos == from)){
			memset(st, 0xff, r->w*sizeof st[0]);
			st[Spc] = r->back ? r->p->bstart : r->p->start;
			st[Sstart] = pos;
			memset(st+r->fb, 0, r->p->nbind*sizeof st[0]);
			addinst(r, &r->l[0], st, 0);
		}
		clear(&r->l[1]);
		for(j = 0; j < r->l[0].n; j++){
			v = r->l[0].v+j*r->w;
			if(pruned(r, v))
				continue;
			memmove(st, v, r->w*sizeof st[0]);
			chain(r, st, pos, j);
		}
		l = r->l[0];
		r->l[0] = r->l[1];
		r->l[1] = l;
		if(pos == end)
			break;
		if(r->l[0].n == 0 && (r->found || r->mode == Anchor) && r->mode != All)
			break;
	}
	return r->found;
}

static int
orig(Run *r, int j, int t1)
{
	if(r->vis == nil)
		return j;
	return j < r->j1 ? r->vis[j] : t1;
}

static void
fill(Run *r, Tmatch *m, int t1)
{
	Tpat *p;
	int i, c0, c1, *cap;

	p = r->p;
	cap = r->best+r->cb;
	m->t0 = r->o0;
	m->t1 = r->o1;
	for(i = 0; i < Tmaxcap; i++){
		m->cap[i][0] = m->cap[i][1] = -1;
		if(i >= p->ncap)
			continue;
		c0 = cap[2*i];
		c1 = cap[2*i+1];
		if(c0 < 0 || c1 < 0)
			continue;
		if(r->back && p->capbind[i] < 0){
			c0 = cap[2*i+1];
			c1 = cap[2*i];
		}
		m->cap[i][0] = c0 == r->b1 ? m->t1 : c0 == r->b0 ? m->t0 : orig(r, c0, t1);
		m->cap[i][1] = c1 > c0 ? orig(r, c1-1, t1)+1 : m->cap[i][0];
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
texec(Tpat *p, Toks *t, int t0, int t1, int from, int mode, int back, Tmatch *m)
{
	Run r;
	int ok;

	if(t0 < 0)
		t0 = 0;
	if(t0 > t->n)
		t0 = t->n;
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
	r.j0 = t0;
	r.j1 = t1;
	r.t1 = t1;
	if(!p->comments){
		_tokview(t, &r.vis, &r.rank);
		r.j0 = r.rank[t0];
		r.j1 = r.rank[t1];
		from = r.rank[from];
	}
	r.mode = mode;
	r.back = back;
	r.ub = Sfix;
	r.fb = r.ub+p->nstar;
	r.bb = r.fb+p->nbind;
	r.cb = r.bb+p->nbind;
	r.w = r.cb+2*p->ncap;
	r.work = emalloc(r.w*sizeof r.work[0]);
	r.tmp = emalloc(r.w*sizeof r.tmp[0]);
	r.best = emalloc(r.w*sizeof r.best[0]);
	ok = run(&r, from);
	if(ok && m != nil)
		fill(&r, m, t1);
	freelist(&r.l[0]);
	freelist(&r.l[1]);
	free(r.work);
	free(r.tmp);
	free(r.best);
	return ok;
}

int
tokexec(Tpat *p, Toks *t, int t0, int t1, int from, Tmatch *m)
{
	return texec(p, t, t0, t1, from, Left, 0, m);
}

int
tokbexec(Tpat *p, Toks *t, int t0, int t1, Tmatch *m)
{
	Tmatch v;

	if(!texec(p, t, t0, t1, t1, Left, 1, m))
		return 0;
	if(!p->refs || !p->bunsafe)
		return 1;
	if(texec(p, t, m->t0, m->t1, m->t0, Anchor, 0, &v) && v.t0 == m->t0 && v.t1 == m->t1){
		*m = v;
		return 1;
	}
	return texec(p, t, t0, t1, t0, All, 0, m);
}
