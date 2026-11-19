#include <u.h>
#include <libc.h>
#include <md.h>
#include "impl.h"

struct Dl
{
	Dl	*prev;
	Dl	*next;
	Tn	*t;
	int	pos;
	int	len;
	int	c;
	int	open;
	int	close;
};

struct Br
{
	Br	*prev;
	Tn	*t;
	int	pos;
	int	image;
	int	after;
	int	in0;
	int	in1;
};

enum
{
	Kcomment = 1,
	Kcdata = 2,
	Kdecl = 4,
	Kpi = 8,

	Xesc = 1,
	Xent = 2,
	Xnl = 4,
	Xspace = 8,
};

static int
peek(Sub *u)
{
	return u->pos < u->n ? u->s[u->pos] : 0;
}

static int
peekat(Sub *u, int i)
{
	return i < u->n && i >= 0 ? u->s[i] : 0;
}

static int
segof(Sub *u, int v)
{
	int lo, hi, m;

	lo = 0;
	hi = u->nseg-1;
	while(lo < hi){
		m = (lo+hi+1)/2;
		if(u->vs[m] <= v)
			lo = m;
		else
			hi = m-1;
	}
	return lo;
}

static u32int
vmap0(Sub *u, int v)
{
	int i;
	Seg *g;

	if(u->nseg == 0)
		return 0;
	i = segof(u, v);
	g = &u->seg[i];
	if(v - u->vs[i] < g->e - g->b)
		return g->b + v - u->vs[i];
	if(i+1 < u->nseg && !u->seg[i+1].nl)
		return u->seg[i+1].b;
	return g->e;
}

static u32int
vmap1(Sub *u, int v)
{
	int i, c;
	Seg *g;

	if(u->nseg == 0)
		return 0;
	if(v <= 0)
		return u->seg[0].b;
	c = v-1;
	i = segof(u, c);
	g = &u->seg[i];
	if(c - u->vs[i] < g->e - g->b)
		return g->b + c - u->vs[i] + 1;
	return g->ln;
}

static u32int
vline(Sub *u, int v)
{
	if(u->nseg == 0)
		return 0;
	return u->seg[segof(u, v)].line;
}

void
_mdtnmap(Sub *u, Tn *root)
{
	Tn *k;
	int v0, v1;

	for(k = root->kid; k != nil; ){
		v0 = k->q0;
		v1 = k->q1;
		k->q0 = vmap0(u, v0);
		k->q1 = v1 > v0 ? vmap1(u, v1) : k->q0;
		k->line = vline(u, v0);
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

static void
splitsegs(Sub *u, Tn *root)
{
	Tn *t, *x;
	int i;

	for(t = root->kid; t != nil; ){
		if(t->type == Mdtext)
			for(i = 1; i < u->nseg; i++)
				if(!u->seg[i].nl && t->q0 < u->vs[i] && u->vs[i] < t->q1){
					x = _mdtn(u->p, Mdtext, u->vs[i], t->q1, 0);
					t->q1 = u->vs[i];
					x->up = t->up;
					x->prev = t;
					x->next = t->next;
					if(t->next != nil)
						t->next->prev = x;
					else if(t->up != nil)
						t->up->last = x;
					t->next = x;
					break;
				}
		if(t->kid != nil){
			t = t->kid;
			continue;
		}
		while(t != root && t->next == nil)
			t = t->up;
		if(t == root)
			break;
		t = t->next;
	}
}

static Tn*
mk(Sub *u, int type, int v0, int v1)
{
	return _mdtn(u->p, type, v0, v1, 0);
}

static Tn*
mark(Sub *u, int aux, int v0, int v1)
{
	Tn *t;

	t = mk(u, Mdmark, v0, v1);
	t->aux = aux;
	return t;
}

static void
_mdtext(P *p, Sub *u, Tn *up, int v0, int v1, int x)
{
	int i, j, k;
	Rune r[2];

	USED(p);
	i = v0;
	j = v0;
	while(j < v1){
		if((x & Xnl) && (u->s[j] == '\n' || u->s[j] == '\r')){
			if(j > i)
				_mdadd(up, mk(u, Mdtext, i, j));
			k = j+1;
			if(u->s[j] == '\r' && k < v1 && u->s[k] == '\n')
				k++;
			_mdadd(up, mk(u, Mdsoft, j, k));
			i = j = k;
			continue;
		}
		if((x & Xesc) && u->s[j] == '\\' && j+1 < v1 && _mdispunct(u->s[j+1])){
			if(j > i)
				_mdadd(up, mk(u, Mdtext, i, j));
			_mdadd(up, mk(u, Mdesc, j, j+2));
			i = j = j+2;
			continue;
		}
		if((x & Xent) && u->s[j] == '&' && (k = _mdent1(u->s+j, v1-j, r)) > 0){
			if(j > i)
				_mdadd(up, mk(u, Mdtext, i, j));
			_mdadd(up, mk(u, Mdent, j, j+k));
			i = j = j+k;
			continue;
		}
		j++;
	}
	if(j > i)
		_mdadd(up, mk(u, Mdtext, i, j));
}

static void
skipsp(Sub *u)
{
	while(peek(u) == ' ' || peek(u) == '\t')
		u->pos++;
}

static int
skiplineend(Sub *u)
{
	int seen;

	seen = 0;
	if(peek(u) == '\r'){
		u->pos++;
		seen = 1;
	}
	if(peek(u) == '\n'){
		u->pos++;
		seen = 1;
	}
	return seen || u->pos >= u->n;
}

static void
spnl(Sub *u)
{
	skipsp(u);
	if(skiplineend(u))
		skipsp(u);
}

static int
linklabel(Sub *u, int *l0, int *l1)
{
	int start, len, c;

	start = u->pos;
	len = 0;
	if(peek(u) != '[')
		return 0;
	u->pos++;
	c = 0;
	while(u->pos < u->n && (c = u->s[u->pos]) != '[' && c != ']'){
		if(c == '\\'){
			u->pos++;
			len++;
			if(_mdispunct(peek(u))){
				u->pos++;
				len++;
			}
		}else{
			u->pos++;
			len++;
		}
		if(len > Maxlabel)
			goto no;
	}
	if(u->pos < u->n && c == ']'){
		*l0 = start+1;
		*l1 = u->pos;
		while(*l0 < *l1 && _mdisspace(u->s[*l0]))
			(*l0)++;
		while(*l1 > *l0 && _mdisspace(u->s[*l1-1]))
			(*l1)--;
		u->pos++;
		return 1;
	}
no:
	u->pos = start;
	return 0;
}

static int
linkurl2(Sub *u, int off, int *d0, int *d1)
{
	int i, np;

	i = off;
	np = 0;
	while(i < u->n){
		if(u->s[i] == '\\' && i+1 < u->n && _mdispunct(u->s[i+1]))
			i += 2;
		else if(u->s[i] == '('){
			np++;
			i++;
			if(np > 32)
				return -1;
		}else if(u->s[i] == ')'){
			if(np == 0)
				break;
			np--;
			i++;
		}else if(_mdisspace(u->s[i])){
			if(i == off)
				return -1;
			break;
		}else
			i++;
	}
	if(i >= u->n || np != 0)
		return -1;
	*d0 = off;
	*d1 = i;
	return i-off;
}

static int
linkurl(Sub *u, int off, int *d0, int *d1)
{
	int i;

	i = off;
	if(i < u->n && u->s[i] == '<'){
		i++;
		while(i < u->n){
			if(u->s[i] == '>'){
				i++;
				break;
			}else if(u->s[i] == '\\')
				i += 2;
			else if(u->s[i] == '\n' || u->s[i] == '<')
				return -1;
			else
				i++;
		}
	}else
		return linkurl2(u, off, d0, d1);
	if(i >= u->n)
		return -1;
	*d0 = off+1;
	*d1 = i-1;
	return i-off;
}

static Tn*
dest(Sub *u, int a, int b, int d0, int d1)
{
	Tn *t;

	t = mk(u, Mddest, a, b);
	if(d0 > a)
		_mdadd(t, mark(u, Mdlink, a, d0));
	_mdtext(u->p, u, t, d0, d1, Xesc|Xent);
	if(b > d1)
		_mdadd(t, mark(u, Mdlink, d1, b));
	return t;
}

static Tn*
title(Sub *u, int a, int b)
{
	Tn *t;

	t = mk(u, Mdtitle, a, b);
	_mdadd(t, mark(u, Mdlink, a, a+1));
	_mdtext(u->p, u, t, a+1, b-1, Xesc|Xent|Xnl);
	_mdadd(t, mark(u, Mdlink, b-1, b));
	return t;
}

static Tn*
label(Sub *u, int a, int b)
{
	Tn *t;

	t = mk(u, Mdlabel, a, b);
	_mdadd(t, mark(u, Mdlink, a, a+1));
	_mdtext(u->p, u, t, a+1, b-1, Xnl);
	_mdadd(t, mark(u, Mdlink, b-1, b));
	return t;
}

static int
defined(Sub *u, int a, int b)
{
	char *k;
	int r;
	u32int h;

	if(b-a < 1 || b-a > Maxlabel)
		return 0;
	k = _mdnorm(u->s+a, b-a);
	if(k[0] == 0){
		free(k);
		return 0;
	}
	h = _mdhash(k);
	if(u->nlab >= u->mlab){
		u->mlab = u->mlab*2 + 4;
		u->lab = erealloc(u->lab, u->mlab*sizeof u->lab[0]);
	}
	u->lab[u->nlab++] = h;
	r = _mdlookup(u->p, k);
	free(k);
	return r;
}

int
_mdparseref(P *p, Seg *seg, int nseg, Blk *b, Tn **out)
{
	Sub u;
	uchar *buf;
	int i, n, l0, l1, d0, d1, ml, pos, bt, t0, t1, end, start;
	u32int *vs;
	Tn *def, *lb;
	char *k;

	USED(b);
	n = 0;
	for(i = 0; i < nseg; i++)
		n += seg[i].e - seg[i].b + 1;
	buf = emalloc(n+2);
	vs = emalloc((nseg+1)*sizeof vs[0]);
	n = 0;
	for(i = 0; i < nseg; i++){
		if(i > 0)
			buf[n++] = '\n';
		vs[i] = n;
		memmove(buf+n, p->s+seg[i].b, seg[i].e-seg[i].b);
		n += seg[i].e - seg[i].b;
	}
	buf[n++] = '\n';
	memset(&u, 0, sizeof u);
	u.p = p;
	u.s = buf;
	u.n = n;
	u.seg = seg;
	u.nseg = nseg;
	u.vs = vs;
	pos = 0;
	*out = nil;
	for(;;){
		u.pos = pos;
		start = pos;
		if(peek(&u) != '[')
			break;
		if(!linklabel(&u, &l0, &l1) || l1 == l0)
			break;
		lb = label(&u, start, u.pos);
		if(peek(&u) != ':')
			break;
		u.pos++;
		t0 = u.pos;
		spnl(&u);
		if((ml = linkurl(&u, u.pos, &d0, &d1)) < 0)
			break;
		def = mk(&u, Mddef, start, 0);
		_mdadd(def, lb);
		_mdadd(def, mark(&u, Mddef, t0-1, t0));
		_mdadd(def, dest(&u, u.pos, u.pos+ml, d0, d1));
		u.pos += ml;
		bt = u.pos;
		spnl(&u);
		ml = u.pos == bt ? 0 : _mdscantitle(u.s, u.n, u.pos);
		t0 = t1 = 0;
		if(ml){
			t0 = u.pos;
			t1 = u.pos + ml;
			u.pos += ml;
		}else
			u.pos = bt;
		skipsp(&u);
		end = u.pos;
		if(!skiplineend(&u)){
			if(ml){
				u.pos = bt;
				skipsp(&u);
				end = u.pos;
				if(!skiplineend(&u))
					break;
				ml = 0;
			}else
				break;
		}
		if(ml){
			_mdadd(def, title(&u, t0, t1));
			end = t1;
		}else
			end = bt;
		def->q1 = end;
		k = _mdnorm(u.s+l0, l1-l0);
		def->key = _mdalloc(p, strlen(k)+1);
		strcpy(def->key, k);
		free(k);
		def->next = nil;
		if(*out == nil)
			*out = def;
		else{
			Tn *x;
			for(x = *out; x->next != nil; x = x->next)
				;
			x->next = def;
		}
		pos = u.pos;
	}
	for(def = *out; def != nil; def = def->next){
		u32int v0, v1;

		v0 = def->q0;
		v1 = def->q1;
		def->q0 = vmap0(&u, v0);
		def->q1 = vmap1(&u, v1);
		def->line = vline(&u, v0);
		_mdtnmap(&u, def);
	}
	i = pos >= n ? nseg : segof(&u, pos);
	free(buf);
	free(vs);
	return i;
}

static Tn*
text(Sub *u, int v0, int v1)
{
	return mk(u, Mdtext, v0, v1);
}

static int
scanbt(Sub *u, int len)
{
	int nt;

	if(len > 1000)
		return 0;
	if(u->btscan && u->bt[len] <= u->pos)
		return 0;
	for(;;){
		while(u->pos < u->n && u->s[u->pos] != '`')
			u->pos++;
		if(u->pos >= u->n)
			break;
		nt = 0;
		while(peek(u) == '`'){
			u->pos++;
			nt++;
		}
		if(nt <= 1000)
			u->bt[nt] = u->pos - nt;
		if(nt == len)
			return u->pos;
	}
	u->btscan = 1;
	return 0;
}

static Tn*
backticks(Sub *u)
{
	int init, len, start, end, a, b, i, ns;
	Tn *t;

	init = u->pos;
	while(peek(u) == '`')
		u->pos++;
	len = u->pos - init;
	start = u->pos;
	end = scanbt(u, len);
	if(end == 0){
		u->pos = start;
		return text(u, init, start);
	}
	t = mk(u, Mdcodespan, init, end);
	t->w = len;
	_mdadd(t, mark(u, Mdcodespan, init, start));
	a = start;
	b = end - len;
	ns = 0;
	for(i = a; i < b; i++)
		if(u->s[i] != ' ' && u->s[i] != '\n' && u->s[i] != '\r')
			ns = 1;
	if(ns && b-a >= 2 && (u->s[a] == ' ' || u->s[a] == '\n') && (u->s[b-1] == ' ' || u->s[b-1] == '\n')){
		a++;
		b--;
	}
	_mdtext(u->p, u, t, a, b, Xnl);
	_mdadd(t, mark(u, Mdcodespan, end-len, end));
	return t;
}

static int
flank(Sub *u, int c, int *open, int *close)
{
	int n, bp, k, lf, rf, bpun, apun, bsp, asp;
	Rune before, after;

	n = 0;
	if(u->pos == 0)
		before = '\n';
	else{
		bp = u->pos-1;
		while(bp > 0 && (u->s[bp] >> 6) == 2)
			bp--;
		if(_mdutf(u->s, u->pos, bp, &before) < 0)
			before = '\n';
	}
	while(peek(u) == c && u->pos < u->n){
		n++;
		u->pos++;
	}
	if(u->pos >= u->n)
		after = '\n';
	else if((k = _mdutf(u->s, u->n, u->pos, &after)) < 0)
		after = '\n';
	bsp = _mdspacer(before);
	asp = _mdspacer(after);
	bpun = _mdpunctr(before);
	apun = _mdpunctr(after);
	lf = n > 0 && !asp && (!apun || bsp || bpun);
	rf = n > 0 && !bsp && (!bpun || asp || apun);
	if(c == '_'){
		*open = lf && (!rf || bpun);
		*close = rf && (!lf || apun);
	}else{
		*open = lf;
		*close = rf;
	}
	return n;
}

static void
pushdelim(Sub *u, int c, int open, int close, Tn *t)
{
	Dl *d, *last;

	d = emalloc(sizeof *d);
	d->c = c;
	d->open = open;
	d->close = close;
	d->t = t;
	d->pos = u->pos;
	d->len = t->q1 - t->q0;
	last = u->delim;
	d->prev = last;
	if(last != nil)
		last->next = d;
	u->delim = d;
}

static void
rmdelim(Sub *u, Dl *d)
{
	if(d == nil)
		return;
	if(d->next == nil)
		u->delim = d->prev;
	else
		d->next->prev = d->prev;
	if(d->prev != nil)
		d->prev->next = d->next;
	free(d);
}

static void
popbr(Sub *u)
{
	Br *b;

	b = u->brack;
	if(b == nil)
		return;
	u->brack = b->prev;
	free(b);
}

static void
pushbr(Sub *u, int image, Tn *t)
{
	Br *b, *last;

	b = emalloc(sizeof *b);
	last = u->brack;
	if(last != nil){
		last->after = 1;
		b->in0 = last->in0;
		b->in1 = last->in1;
	}
	b->image = image;
	b->t = t;
	b->prev = last;
	b->pos = u->pos;
	if(image)
		b->in1 = 1;
	else
		b->in0 = 1;
	u->brack = b;
	if(!image)
		u->nolink = 0;
}

static Tn*
delim(Sub *u, int c)
{
	int n, open, close;
	Tn *t;

	n = flank(u, c, &open, &close);
	t = text(u, u->pos-n, u->pos);
	if(c == '~'){
		if((open || close) && (n == 1 || n == 2))
			pushdelim(u, c, open, close, t);
		return t;
	}
	if(open || close)
		pushdelim(u, c, open, close, t);
	return t;
}

static void
wrap(Tn *w, Tn *a, Tn *b)
{
	Tn *t, *n;

	for(t = a->next; t != nil && t != b; t = n){
		n = t->next;
		_mdunlink(t);
		_mdadd(w, t);
	}
}

static Dl*
emph(Sub *u, Dl *o, Dl *c)
{
	Dl *d, *x;
	int use;
	Tn *ot, *ct, *e;

	ot = o->t;
	ct = c->t;
	use = (ct->q1 - ct->q0 >= 2 && ot->q1 - ot->q0 >= 2) ? 2 : 1;
	for(d = c->prev; d != nil && d != o; d = x){
		x = d->prev;
		rmdelim(u, d);
	}
	e = mk(u, use == 1 ? Mdemph : Mdstrong, ot->q1 - use, ct->q0 + use);
	_mdadd(e, mark(u, e->type, ot->q1 - use, ot->q1));
	wrap(e, ot, ct);
	_mdadd(e, mark(u, e->type, ct->q0, ct->q0 + use));
	ot->q1 -= use;
	ct->q0 += use;
	e->up = ot->up;
	e->prev = ot;
	e->next = ct;
	ot->next = e;
	ct->prev = e;
	if(ot->q1 == ot->q0){
		_mdunlink(ot);
		rmdelim(u, o);
	}
	if(ct->q1 == ct->q0){
		_mdunlink(ct);
		x = c->next;
		rmdelim(u, c);
		c = x;
	}
	return c;
}

static Dl*
strike(Sub *u, Dl *o, Dl *c)
{
	Dl *res, *d, *x;
	Tn *ot, *ct, *e;

	res = c->next;
	ot = o->t;
	ct = c->t;
	if(ot->q1 - ot->q0 == ct->q1 - ct->q0){
		Tn *t, *n;

		e = mk(u, Mdstrike, ot->q0, ct->q1);
		_mdbefore(ot, e);
		for(t = ot; t != nil; t = n){
			n = t->next;
			_mdunlink(t);
			_mdadd(e, t);
			if(t == ct)
				break;
		}
		ot->type = Mdmark;
		ot->aux = Mdstrike;
		ct->type = Mdmark;
		ct->aux = Mdstrike;
	}
	for(d = c; d != nil && d != o; d = x){
		x = d->prev;
		rmdelim(u, d);
	}
	rmdelim(u, o);
	return res;
}

static void
procemph(Sub *u, int bottom)
{
	Dl *cand, *closer, *opener, *old;
	int found, idx, ob[15], tb[3], i;

	for(i = 0; i < 15; i++)
		ob[i] = bottom;
	for(i = 0; i < 3; i++)
		tb[i] = 0;
	closer = nil;
	cand = u->delim;
	while(cand != nil && cand->pos >= bottom){
		closer = cand;
		cand = cand->prev;
	}
	while(closer != nil){
		if(!closer->close){
			closer = closer->next;
			continue;
		}
		idx = 0;
		switch(closer->c){
		case '_':
			idx = 2 + (closer->open ? 3 : 0) + closer->len%3;
			break;
		case '*':
			idx = 8 + (closer->open ? 3 : 0) + closer->len%3;
			break;
		}
		opener = closer->prev;
		found = 0;
		while(opener != nil && opener->pos >= bottom &&
		      opener->pos >= (closer->c == '~' ? tb[closer->len%3] : ob[idx])){
			if(opener->open && opener->c == closer->c)
			if(!(closer->open || opener->close) || closer->len%3 == 0 || (opener->len + closer->len)%3 != 0){
				found = 1;
				break;
			}
			opener = opener->prev;
		}
		old = closer;
		if(closer->c == '~'){
			if(found)
				closer = strike(u, opener, closer);
			else
				closer = closer->next;
		}else if(found)
			closer = emph(u, opener, closer);
		else
			closer = closer->next;
		if(!found){
			if(old->c == '~')
				tb[old->len%3] = old->pos;
			else
				ob[idx] = old->pos;
			if(!old->open)
				rmdelim(u, old);
		}
	}
	while(u->delim != nil && u->delim->pos >= bottom)
		rmdelim(u, u->delim);
}

static Tn*
backslash(Sub *u)
{
	int c, st;
	Tn *t;

	st = u->pos;
	u->pos++;
	c = peek(u);
	if(u->pos < u->n && _mdispunct(c)){
		u->pos++;
		return mk(u, Mdesc, st, u->pos);
	}
	if(u->pos < u->n && skiplineend(u)){
		t = mk(u, Mdhard, st, u->pos);
		return t;
	}
	return text(u, st, st+1);
}

static Tn*
entity(Sub *u)
{
	int k;
	Rune r[2];

	k = _mdent1(u->s+u->pos, u->n-u->pos, r);
	if(k <= 0){
		u->pos++;
		return text(u, u->pos-1, u->pos);
	}
	u->pos += k;
	return mk(u, Mdent, u->pos-k, u->pos);
}

static Tn*
autolink(Sub *u, int a, int b, int email)
{
	Tn *t;

	t = mk(u, Mdauto, a, b);
	if(email)
		t->flags |= Mdemail;
	_mdadd(t, mark(u, Mdauto, a, a+1));
	_mdtext(u->p, u, t, a+1, b-1, Xent);
	_mdadd(t, mark(u, Mdauto, b-1, b));
	return t;
}

static Tn*
pointy(Sub *u)
{
	int ml, c, st;
	Tn *t;

	st = u->pos;
	u->pos++;
	ml = _mdscanauri(u->s, u->n, u->pos);
	if(ml > 0){
		u->pos += ml;
		return autolink(u, st, u->pos, 0);
	}
	ml = _mdscanamail(u->s, u->n, u->pos);
	if(ml > 0){
		u->pos += ml;
		return autolink(u, st, u->pos, 1);
	}
	ml = 0;
	if(u->pos + 2 <= u->n){
		c = u->s[u->pos];
		if(c == '!' && (u->flags & Kcomment) == 0){
			c = peekat(u, u->pos+1);
			if(c == '-' && peekat(u, u->pos+2) == '-'){
				if(peekat(u, u->pos+3) == '>')
					ml = 4;
				else if(peekat(u, u->pos+3) == '-' && peekat(u, u->pos+4) == '>')
					ml = 5;
				else{
					ml = _mdscancomment(u->s, u->n, u->pos+1);
					if(ml > 0)
						ml++;
					else
						u->flags |= Kcomment;
				}
			}else if(c == '['){
				if((u->flags & Kcdata) == 0){
					ml = _mdscancdata(u->s, u->n, u->pos+2);
					if(ml > 0){
						ml += 5;
						if(u->pos + ml > u->n){
							u->flags |= Kcdata;
							ml = 0;
						}
					}
				}
			}else if((u->flags & Kdecl) == 0){
				ml = 0;
				if(c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z'){
					for(ml = u->pos+1; ml < u->n && u->s[ml] != '>'; ml++)
						;
					ml = ml - (u->pos+1);
					ml += 2;
					if(u->pos + ml > u->n){
						u->flags |= Kdecl;
						ml = 0;
					}
				}
			}
		}else if(c == '?'){
			if((u->flags & Kpi) == 0){
				for(ml = u->pos+1; ml+1 < u->n && !(u->s[ml] == '?' && u->s[ml+1] == '>'); ml++)
					;
				if(ml+1 >= u->n){
					u->flags |= Kpi;
					ml = 0;
				}else
					ml = ml+2 - u->pos;
			}
		}else
			ml = _mdscanhtag(u->s, u->n, u->pos);
	}
	if(ml > 0){
		u->pos += ml;
		t = mk(u, Mdraw, st, u->pos);
		_mdtext(u->p, u, t, st, u->pos, Xnl);
		return t;
	}
	return text(u, st, st+1);
}

static void
unput(Tn *up, int n)
{
	Tn *t;

	for(t = up->last; n > 0 && t != nil && t->type == Mdtext; t = t->prev){
		if(t->q1 - t->q0 < n){
			n -= t->q1 - t->q0;
			t->q1 = t->q0;
		}else{
			t->q1 -= n;
			n = 0;
		}
	}
}

static int
isalphac(int c)
{
	return c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z';
}

static int
isalnumc(int c)
{
	return isalphac(c) || c >= '0' && c <= '9';
}

static int
hostchar(uchar *s, int n)
{
	Rune r;

	if(_mdutf(s, n, 0, &r) < 0)
		return 0;
	if(r < 128)
		return !_mdisspace(r) && !_mdispunct(r);
	return !_mdspacer(r) && !_mdpunctr(r);
}

static int
adelim(uchar *d, int end)
{
	int i, open, close, ne;

	open = close = 0;
	for(i = 0; i < end; i++){
		if(d[i] == '<'){
			end = i;
			break;
		}else if(d[i] == '(')
			open++;
		else if(d[i] == ')')
			close++;
	}
	while(end > 0){
		switch(d[end-1]){
		case ')':
			if(close <= open)
				return end;
			close--;
			end--;
			break;
		case '?': case '!': case '.': case ',': case ':': case '*': case '_': case '~': case '\'': case '"':
			end--;
			break;
		case ';':
			ne = end-2;
			while(ne > 0 && isalphac(d[ne]))
				ne--;
			if(ne < end-2 && d[ne] == '&')
				end = ne;
			else
				end--;
			break;
		default:
			return end;
		}
	}
	return end;
}

static int
domain(uchar *d, int n, int short_)
{
	int i, np, u1, u2;

	np = u1 = u2 = 0;
	for(i = 1; i < n-1; i++){
		if(d[i] == '\\' && i < n-2)
			i++;
		if(d[i] == '_')
			u2++;
		else if(d[i] == '.'){
			u1 = u2;
			u2 = 0;
			np++;
		}else if(!hostchar(d+i, n-i) && d[i] != '-')
			break;
	}
	if((u1 > 0 || u2 > 0) && np <= 10)
		return 0;
	if(short_)
		return i;
	return np ? i : 0;
}

static int
inbrack(Sub *u)
{
	Br *b;

	b = u->brack;
	return b != nil && (b->in0 || b->in1);
}

static Tn*
www(Sub *u, Tn *up)
{
	uchar *d;
	int n, end;
	Tn *t;

	USED(up);
	if(inbrack(u))
		return nil;
	d = u->s + u->pos;
	n = u->n - u->pos;
	if(u->pos > 0 && (d[-1] == 0 || strchr("*_~(", d[-1]) == nil) && !_mdisspace(d[-1]))
		return nil;
	if(n < 4 || memcmp(d, "www.", 4) != 0)
		return nil;
	end = domain(d, n, 0);
	if(end == 0)
		return nil;
	while(end < n && !_mdisspace(d[end]) && d[end] != '<')
		end++;
	end = adelim(d, end);
	if(end == 0)
		return nil;
	t = mk(u, Mdauto, u->pos, u->pos+end);
	t->flags |= Mdwww|Mdbare;
	_mdadd(t, text(u, u->pos, u->pos+end));
	u->pos += end;
	return t;
}

static Tn*
url(Sub *u, Tn *up)
{
	uchar *d;
	int n, rw, end, dl, k;
	static char *ok[] = {"http://", "https://", "ftp://", nil};
	char **o;
	Tn *t;

	if(inbrack(u))
		return nil;
	d = u->s + u->pos;
	n = u->n - u->pos;
	if(n < 4 || d[1] != '/' || d[2] != '/')
		return nil;
	rw = 0;
	while(rw < u->pos && isalphac(d[-rw-1]))
		rw++;
	for(o = ok; *o; o++){
		k = strlen(*o);
		if(n+rw > k && cistrncmp((char*)d-rw, *o, k) == 0 && hostchar(d-rw+k, n+rw-k))
			break;
	}
	if(*o == nil)
		return nil;
	end = 3;
	dl = domain(d+end, n-end, 1);
	if(dl == 0)
		return nil;
	end += dl;
	while(end < n && !_mdisspace(d[end]) && d[end] != '<')
		end++;
	end = adelim(d, end);
	if(end == 0)
		return nil;
	unput(up, rw);
	t = mk(u, Mdauto, u->pos-rw, u->pos+end);
	t->flags |= Mdbare;
	_mdadd(t, text(u, u->pos-rw, u->pos+end));
	u->pos += end;
	return t;
}

static Tn*
closebr(Sub *u)
{
	int init, after, sps, nn, endurl, st, et, ea, found, l0, l1, d0, d1, image, hasl;
	Br *o;
	Tn *l, *t, *x, *dt, *tt, *lt;
	int kind;

	u->pos++;
	init = u->pos;
	o = u->brack;
	if(o == nil)
		return text(u, init-1, init);
	image = o->image;
	if(!image && u->nolink){
		popbr(u);
		return text(u, init-1, init);
	}
	after = u->pos;
	dt = tt = lt = nil;
	kind = 0;
	if(peek(u) == '(' && (sps = _mdscanspace(u->s, u->n, u->pos+1), 1) &&
	   (nn = linkurl(u, u->pos+1+sps, &d0, &d1)) > -1){
		endurl = u->pos+1+sps+nn;
		st = endurl + _mdscanspace(u->s, u->n, endurl);
		et = st == endurl ? st : st + _mdscantitle(u->s, u->n, st);
		ea = et + _mdscanspace(u->s, u->n, et);
		if(peekat(u, ea) == ')'){
			u->pos = ea+1;
			if(nn > 0)
				dt = dest(u, endurl-nn, endurl, d0, d1);
			if(et > st)
				tt = title(u, st, et);
			kind = Mdinline;
			goto match;
		}
		u->pos = after;
	}
	l0 = l1 = 0;
	found = linklabel(u, &l0, &l1);
	hasl = found;
	if(!found)
		u->pos = init;
	if((!found || l1 == l0) && !o->after){
		l0 = o->pos;
		l1 = init-1;
		found = 1;
		kind = hasl ? Mdcollapsed : Mdshortcut;
	}else if(found)
		kind = Mdfull;
	if(found && defined(u, l0, l1)){
		if(kind == Mdfull)
			lt = label(u, after, u->pos);
		else if(kind == Mdcollapsed)
			lt = mark(u, image ? Mdimage : Mdlink, after, u->pos);
		goto match;
	}
	popbr(u);
	u->pos = init;
	return text(u, init-1, init);

match:
	l = mk(u, image ? Mdimage : Mdlink, o->t->q0, u->pos);
	l->flags |= kind;
	_mdbefore(o->t, l);
	t = o->t;
	_mdunlink(t);
	t->type = Mdmark;
	t->aux = l->type;
	_mdadd(l, t);
	while(l->next != nil){
		x = l->next;
		_mdunlink(x);
		_mdadd(l, x);
	}
	procemph(u, o->pos);
	_mdadd(l, mark(u, l->type, init-1, init));
	if(kind == Mdinline){
		_mdadd(l, mark(u, l->type, after, after+1));
		if(dt != nil)
			_mdadd(l, dt);
		if(tt != nil)
			_mdadd(l, tt);
		_mdadd(l, mark(u, l->type, u->pos-1, u->pos));
	}else if(lt != nil)
		_mdadd(l, lt);
	popbr(u);
	if(!image)
		u->nolink = 1;
	return nil;
}

static Tn*
newline(Sub *u)
{
	int nl, k, e;

	nl = u->pos;
	if(peekat(u, u->pos) == '\r')
		u->pos++;
	if(peekat(u, u->pos) == '\n')
		u->pos++;
	e = u->pos;
	k = nl;
	while(k > 0 && u->s[k-1] == ' ')
		k--;
	skipsp(u);
	if(nl > 1 && u->s[nl-1] == ' ' && u->s[nl-2] == ' ')
		return mk(u, Mdhard, k, e);
	return mk(u, Mdsoft, k, e);
}

static int
special(Sub *u, int c)
{
	switch(c){
	case '\r': case '\n': case '\\': case '`': case '&': case '_': case '*': case '[': case ']': case '<': case '!':
		return 1;
	case '~':
		return u->p->ext & Mdxstrike;
	case 'w': case ':':
		return u->p->ext & Mdxauto;
	}
	return 0;
}

static int
inline1(Sub *u, Tn *up)
{
	Tn *t;
	int c, e, st;

	c = peek(u);
	if(u->pos >= u->n)
		return 0;
	t = nil;
	if(!special(u, c))
		goto def;
	switch(c){
	case '\r':
	case '\n':
		t = newline(u);
		break;
	case '`':
		t = backticks(u);
		break;
	case '\\':
		t = backslash(u);
		break;
	case '&':
		t = entity(u);
		break;
	case '<':
		t = pointy(u);
		break;
	case '*':
	case '_':
	case '~':
		t = delim(u, c);
		break;
	case '[':
		u->pos++;
		t = text(u, u->pos-1, u->pos);
		pushbr(u, 0, t);
		break;
	case ']':
		t = closebr(u);
		break;
	case '!':
		u->pos++;
		if(peek(u) == '['){
			u->pos++;
			t = text(u, u->pos-2, u->pos);
			pushbr(u, 1, t);
		}else
			t = text(u, u->pos-1, u->pos);
		break;
	case 'w':
		t = www(u, up);
		if(t != nil)
			break;
		goto def;
	case ':':
		t = url(u, up);
		if(t != nil)
			break;
		goto def;
	default:
	def:
		st = u->pos;
		for(e = u->pos+1; e < u->n && !special(u, u->s[e]); e++)
			;
		u->pos = e;
		if(peek(u) == '\n' || peek(u) == '\r')
			while(e > st && (u->s[e-1] == ' ' || u->s[e-1] == '\t' || u->s[e-1] == '\v' || u->s[e-1] == '\f'))
				e--;
		t = text(u, st, e);
	}
	if(t != nil)
		_mdadd(up, t);
	return 1;
}

static int
mailproto(char *proto, uchar *d, int rewind, int maxrw)
{
	int len;

	len = strlen(proto);
	if(len > maxrw - rewind)
		return 0;
	if(memcmp(d - rewind - len, proto, len) != 0)
		return 0;
	if(len == maxrw - rewind)
		return 1;
	return !isalnumc(d[-(rewind+len+1)]);
}

static void
email(P *p, Tn *t)
{
	uchar *d;
	int start, off, rem, rw, maxrw, end, np, mailto, xmpp, c;
	uchar *at;
	Tn *l, *post, *x;

	d = (uchar*)p->s + t->q0;
	start = 0;
	off = 0;
	rem = t->q1 - t->q0;
	for(;;){
		if(off >= rem)
			break;
		at = memchr(d+start+off, '@', rem-off);
		if(at == nil)
			break;
		maxrw = at - (d+start+off);
	found:
		mailto = 1;
		xmpp = 0;
		for(rw = 0; rw < maxrw; rw++){
			c = d[start+off+maxrw-rw-1];
			if(isalnumc(c) || strchr(".+-_", c) != nil && c != 0)
				continue;
			if(c == ':'){
				if(mailproto("mailto:", d+start+off+maxrw, rw, maxrw)){
					mailto = 0;
					continue;
				}
				if(mailproto("xmpp:", d+start+off+maxrw, rw, maxrw)){
					mailto = 0;
					xmpp = 1;
					continue;
				}
			}
			break;
		}
		if(rw == 0){
			off += maxrw+1;
			continue;
		}
		np = 0;
		for(end = 1; end < rem-off-maxrw; end++){
			c = d[start+off+maxrw+end];
			if(isalnumc(c))
				continue;
			if(c == '@'){
				off += maxrw+1;
				maxrw = end-1;
				goto found;
			}else if(c == '.' && end < rem-off-maxrw-1 && isalnumc(d[start+off+maxrw+end+1]))
				np++;
			else if(c == '/' && xmpp)
				continue;
			else if(c != '-' && c != '_')
				break;
		}
		if(end < 2 || np == 0 || !isalphac(d[start+off+maxrw+end-1]) && d[start+off+maxrw+end-1] != '.'){
			off += maxrw+end;
			continue;
		}
		end = adelim(d+start+off+maxrw, end);
		if(end == 0){
			off += maxrw+1;
			continue;
		}
		l = _mdtn(p, Mdauto, t->q0+start+off+maxrw-rw, t->q0+start+off+maxrw+end, t->line);
		l->flags |= Mdbare | (mailto ? Mdemail : 0);
		x = _mdtn(p, Mdtext, l->q0, l->q1, t->line);
		_mdadd(l, x);
		post = _mdtn(p, Mdtext, l->q1, t->q1, t->line);
		t->q1 = l->q0;
		post->up = t->up;
		post->prev = l;
		post->next = t->next;
		if(t->next != nil)
			t->next->prev = post;
		else if(t->up != nil)
			t->up->last = post;
		l->up = t->up;
		l->prev = t;
		l->next = post;
		t->next = l;
		t = post;
		d = (uchar*)p->s + t->q0;
		start = 0;
		rem = t->q1 - t->q0;
		off = 0;
	}
}

static Tn*
after(Tn *root, Tn *k, int down)
{
	if(down && k->kid != nil)
		return k->kid;
	while(k != root && k->next == nil)
		k = k->up;
	if(k == root)
		return nil;
	return k->next;
}

static void
consolidate(P *p, Tn *root)
{
	Tn *t, *n;

	USED(p);
	for(t = root->kid; t != nil; ){
		n = t->next;
		if(t->type == Mdtext && n != nil && n->type == Mdtext && t->q1 == n->q0 && t->line == n->line){
			t->q1 = n->q1;
			_mdunlink(n);
			continue;
		}
		t = after(root, t, t->type != Mdcodespan && t->type != Mdraw);
	}
}

static void
emails(P *p, Tn *root)
{
	Tn *t;
	int down;

	for(t = root->kid; t != nil; ){
		down = 1;
		if(t->type == Mdlink || t->type == Mdauto || t->type == Mdcodespan || t->type == Mdraw ||
		   t->type == Mdmark || t->type == Mddest || t->type == Mdtitle || t->type == Mdlabel)
			down = 0;
		else if(t->type == Mdtext){
			email(p, t);
			while(t->next != nil && (t->next->type == Mdauto || t->next->type == Mdtext && t->next->prev->type == Mdauto))
				t = t->next;
			down = 0;
		}
		t = after(root, t, down);
	}
}

static void
dropempty(Tn *root)
{
	Tn *t, *n;

	for(t = root->kid; t != nil; ){
		if(t->type == Mdtext && t->q0 == t->q1){
			n = after(root, t, 0);
			_mdunlink(t);
			t = n;
			continue;
		}
		t = after(root, t, 1);
	}
}

void
_mdinline(P *p, Tn *leaf, Seg *seg, int nseg, int ext)
{
	Sub u;
	int i, n;
	uchar *buf;
	u32int *vs;

	USED(ext);
	n = 0;
	for(i = 0; i < nseg; i++)
		n += seg[i].e - seg[i].b + 1;
	buf = emalloc(n+1);
	vs = emalloc((nseg+1)*sizeof vs[0]);
	n = 0;
	for(i = 0; i < nseg; i++){
		if(i > 0 && seg[i].nl)
			buf[n++] = '\n';
		vs[i] = n;
		memmove(buf+n, p->s+seg[i].b, seg[i].e-seg[i].b);
		n += seg[i].e - seg[i].b;
	}
	while(n > 0 && _mdisspace(buf[n-1]))
		n--;
	memset(&u, 0, sizeof u);
	u.p = p;
	u.s = buf;
	u.n = n;
	u.seg = seg;
	u.nseg = nseg;
	u.vs = vs;
	u.nolink = 1;
	u.root = leaf;
	while(u.pos < u.n && inline1(&u, leaf))
		;
	procemph(&u, 0);
	while(u.delim != nil)
		rmdelim(&u, u.delim);
	while(u.brack != nil)
		popbr(&u);
	splitsegs(&u, leaf);
	_mdtnmap(&u, leaf);
	dropempty(leaf);
	consolidate(p, leaf);
	if(p->ext & Mdxauto)
		emails(p, leaf);
	dropempty(leaf);
	leaf->lab = nil;
	leaf->nlab = u.nlab;
	if(u.nlab > 0){
		leaf->lab = _mdalloc(p, u.nlab*sizeof leaf->lab[0]);
		memmove(leaf->lab, u.lab, u.nlab*sizeof leaf->lab[0]);
	}
	free(u.lab);
	free(buf);
	free(vs);
}
