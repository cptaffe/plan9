#include <u.h>
#include <libc.h>
#include <regexp.h>
#include <tok.h>
#include "impl.h"

uint
_tokhash(char *s, int n)
{
	uint h;

	h = 2166136261U;
	while(n-- > 0)
		h = (h ^ (uchar)*s++) * 16777619U;
	return h;
}

Ient*
_tokintern(Intern *t, char *s, int n)
{
	Ient *e, **h, *nx;
	uint k;
	int i, nh;

	if(t->nh == 0){
		t->nh = 256;
		t->h = emalloc(t->nh*sizeof t->h[0]);
	}
	k = _tokhash(s, n) & (t->nh-1);
	for(e = t->h[k]; e != nil; e = e->next)
		if(e->n == n && memcmp(e->s, s, n) == 0)
			return e;
	e = emalloc(sizeof *e + n + 1);
	e->s = (char*)(e+1);
	memmove(e->s, s, n);
	e->s[n] = 0;
	e->n = n;
	e->id = t->n++;
	e->next = t->h[k];
	t->h[k] = e;
	if(t->n > 2*t->nh){
		nh = 4*t->nh;
		h = emalloc(nh*sizeof h[0]);
		for(i = 0; i < t->nh; i++)
			for(e = t->h[i]; e != nil; e = nx){
				nx = e->next;
				k = _tokhash(e->s, e->n) & (nh-1);
				e->next = h[k];
				h[k] = e;
			}
		free(t->h);
		t->h = h;
		t->nh = nh;
		return _tokintern(t, s, n);
	}
	return e;
}

void
_tokinternfree(Intern *t)
{
	Ient *e, *nx;
	Tnames *n;
	int i;

	for(i = 0; i < t->nh; i++)
		for(e = t->h[i]; e != nil; e = nx){
			nx = e->next;
			if((n = e->v) != nil){
				free(n->s);
				free(n);
			}
			free(e);
		}
	free(t->h);
	t->h = nil;
	t->nh = t->n = 0;
}

int
_tokname(char *have, char *want)
{
	int n;

	n = strlen(want);
	return strncmp(have, want, n) == 0 && (have[n] == 0 || have[n] == '.');
}

int
tokhas(Tnames *n, char *name)
{
	int i;

	for(i = 0; i < n->n; i++)
		if(_tokname(n->s[i], name))
			return 1;
	return 0;
}

static Tnames*
names(Tx *x, char *s, int n)
{
	Ient *e;
	Tnames *nm;
	char *p, *q;

	if(n == 1 && s[0] == '-')
		n = 0;
	e = _tokintern(&x->names, s, n);
	if(e->v != nil)
		return e->v;
	nm = emalloc(sizeof *nm + n + 1);
	p = (char*)(nm+1);
	memmove(p, s, n);
	p[n] = 0;
	for(; *p != 0; p = q){
		if((q = strchr(p, ',')) == nil)
			q = p+strlen(p);
		if(q > p){
			nm->s = erealloc(nm->s, (nm->n+1)*sizeof nm->s[0]);
			nm->s[nm->n++] = p;
		}
		if(*q == ',')
			*q++ = 0;
	}
	e->v = nm;
	return nm;
}

static int
tokcmp(void *a, void *b)
{
	Tok *x, *y;

	x = a;
	y = b;
	if(x->q0 != y->q0)
		return x->q0 < y->q0 ? -1 : 1;
	if(x->q1 != y->q1)
		return x->q1 < y->q1 ? -1 : 1;
	return 0;
}

static char*
field(char **pp, char *e, int *n)
{
	char *p, *s;

	p = *pp;
	while(p < e && (*p == ' ' || *p == '\t'))
		p++;
	s = p;
	while(p < e && *p != ' ' && *p != '\t')
		p++;
	*n = p-s;
	*pp = p;
	return s;
}

static int
links(Toks *t)
{
	long *q;
	int i, j, *st, ns, ok;

	q = emalloc((t->n+1)*sizeof q[0]);
	st = emalloc((t->n+1)*sizeof st[0]);
	for(i = 0; i < t->n; i++)
		q[i] = t->t[i].link;
	ok = 0;
	for(i = 0; i < t->n; i++){
		t->t[i].link = -1;
		if(q[i] < 0)
			continue;
		j = tokat(t, q[i]);
		if(j >= t->n || t->t[j].q0 != q[i])
			continue;
		if(j == i || q[j] != t->t[i].q0){
			werrstr("token at %ld: bad partner %ld", t->t[i].q0, q[i]);
			goto Out;
		}
		t->t[i].link = j;
	}
	ns = 0;
	for(i = 0; i < t->n; i++){
		if((j = t->t[i].link) < 0)
			continue;
		if(j > i)
			st[ns++] = i;
		else if(ns == 0 || st[--ns] != j){
			werrstr("token at %ld: partners cross", t->t[i].q0);
			goto Out;
		}
	}
	ok = 1;
Out:
	free(q);
	free(st);
	return ok ? 0 : -1;
}

static int
rlen(char *p, char *e)
{
	Rune c;

	if(!fullrune(p, e-p))
		return 1;
	return chartorune(&c, p);
}

static int
num(char *s, int n, long lim, long *v)
{
	vlong x;
	int i;

	if(n <= 0 || n > 18)
		return -1;
	x = 0;
	for(i = 0; i < n; i++){
		if(s[i] < '0' || s[i] > '9')
			return -1;
		x = x*10 + s[i]-'0';
	}
	if(x > lim)
		return -1;
	*v = x;
	return 0;
}

Toks*
tokparse(char *text, long ntext, char *lines, long nlines)
{
	Toks *t;
	Tx *x;
	Tok *k;
	Ient *e;
	char *p, *le, *e1, *b0, *b1, *te, *f[5];
	long q0, n, r, r1, nr, lk;
	int ns, i, m, ln, nf, fl[5];

	t = emalloc(sizeof *t);
	x = emalloc(sizeof *x);
	t->aux = x;
	te = text+ntext;
	nr = 0;
	for(p = text; p < te; nr++)
		p += rlen(p, te);
	t->nr = nr;
	m = 0;
	ln = 0;
	for(p = lines, e1 = lines+nlines; p < e1; p = le+1){
		ln++;
		if((le = memchr(p, '\n', e1-p)) == nil)
			le = e1;
		for(nf = 0; nf < nelem(f); nf++){
			f[nf] = field(&p, le, &fl[nf]);
			if(fl[nf] == 0)
				break;
		}
		if(nf == 0)
			continue;
		field(&p, le, &ns);
		if(nf < 4 || ns > 0){
			werrstr("tokens line %d: not four or five fields", ln);
			goto Err;
		}
		if(num(f[0], fl[0], nr, &q0) < 0 || num(f[1], fl[1], nr, &n) < 0 || n == 0 || n > nr-q0){
			werrstr("tokens line %d: bad start or length", ln);
			goto Err;
		}
		lk = -1;
		if(nf == 5 && num(f[4], fl[4], nr, &lk) < 0){
			werrstr("tokens line %d: bad partner", ln);
			goto Err;
		}
		if(t->n == m){
			m = m ? 2*m : 1024;
			t->t = erealloc(t->t, m*sizeof t->t[0]);
		}
		k = &t->t[t->n++];
		memset(k, 0, sizeof *k);
		k->q0 = q0;
		k->q1 = q0+n;
		k->name = names(x, f[2], fl[2]);
		k->tag = names(x, f[3], fl[3]);
		k->link = lk;
	}
	qsort(t->t, t->n, sizeof t->t[0], tokcmp);
	for(i = 1; i < t->n; i++)
		if(t->t[i].q0 < t->t[i-1].q1){
			werrstr("token at %ld overlaps the one before", t->t[i].q0);
			goto Err;
		}
	r = 0;
	b0 = text;
	for(i = 0; i < t->n; i++){
		k = &t->t[i];
		if(k->q0 < r){
			r = 0;
			b0 = text;
		}
		for(; r < k->q0; r++)
			b0 += rlen(b0, te);
		b1 = b0;
		for(r1 = r; r1 < k->q1; r1++)
			b1 += rlen(b1, te);
		e = _tokintern(&x->text, b0, b1-b0);
		k->s = e->s;
		k->id = e->id;
		k->comment = tokhas(k->name, "comment");
	}
	if(links(t) < 0)
		goto Err;
	return t;
Err:
	tokfree(t);
	return nil;
}

void
tokfree(Toks *t)
{
	Tx *x;

	if(t == nil)
		return;
	x = t->aux;
	free(x->vis);
	free(x->rank);
	_tokinternfree(&x->text);
	_tokinternfree(&x->names);
	free(x);
	free(t->t);
	free(t);
}

int
tokat(Toks *t, long q)
{
	int lo, hi, m;

	lo = 0;
	hi = t->n;
	while(lo < hi){
		m = (lo+hi)/2;
		if(t->t[m].q0 < q)
			lo = m+1;
		else
			hi = m;
	}
	return lo;
}

int
tokbefore(Toks *t, long q)
{
	int lo, hi, m;

	lo = 0;
	hi = t->n;
	while(lo < hi){
		m = (lo+hi)/2;
		if(t->t[m].q1 <= q)
			lo = m+1;
		else
			hi = m;
	}
	return lo;
}

void
_tokview(Toks *t, int **vis, int **rank)
{
	Tx *x;
	int i, n;

	x = t->aux;
	if(x->rank == nil){
		x->vis = emalloc((t->n+1)*sizeof x->vis[0]);
		x->rank = emalloc((t->n+1)*sizeof x->rank[0]);
		n = 0;
		for(i = 0; i < t->n; i++){
			x->rank[i] = n;
			if(!t->t[i].comment)
				x->vis[n++] = i;
		}
		x->rank[t->n] = n;
	}
	*vis = x->vis;
	*rank = x->rank;
}
