#include <u.h>
#include <libc.h>
#include <md.h>
#include "impl.h"

static int
refcmp(void *a, void *b)
{
	Mdref *x, *y;
	int r;

	x = a;
	y = b;
	r = strcmp(x->key, y->key);
	if(r != 0)
		return r;
	return x->off < y->off ? -1 : x->off > y->off;
}

static void
keys(P *p)
{
	int i;

	qsort(p->def, p->ndef, sizeof p->def[0], refcmp);
	p->nkey = 0;
	p->key = emalloc((p->ndef+1)*sizeof p->key[0]);
	for(i = 0; i < p->ndef; i++)
		if(p->nkey == 0 || strcmp(p->key[p->nkey-1], p->def[i].key) != 0)
			p->key[p->nkey++] = p->def[i].key;
}

int
_mdlookup(P *p, char *k)
{
	int lo, hi, m, r;

	lo = 0;
	hi = p->nkey;
	while(lo < hi){
		m = (lo+hi)/2;
		r = strcmp(k, p->key[m]);
		if(r == 0)
			return 1;
		if(r < 0)
			hi = m;
		else
			lo = m+1;
	}
	return 0;
}

static Md*
finish(P *p)
{
	Md *m;
	Tn *t;
	int i;

	keys(p);
	t = _mdpack(p, p->root);
	m = emalloc(sizeof *m);
	m->s = p->s;
	m->ext = p->ext;
	m->n = p->n;
	m->r0 = 0;
	m->r1 = p->n;
	m->root = (Mdtree*)t->a.p;
	m->nref = p->ndef;
	m->ref = emalloc((p->ndef+1)*sizeof m->ref[0]);
	for(i = 0; i < p->ndef; i++){
		m->ref[i] = p->def[i];
		m->ref[i].key = strdup(p->def[i].key);
	}
	return m;
}

static void
freep(P *p)
{
	_mdfreearena(p);
	free(p->b);
	free(p->def);
	free(p->key);
	free(p);
}

Md*
mdparse(char *s, ulong n, int ext)
{
	P *p;
	ulong ls;
	Md *m;

	p = _mdnewp(s, n);
	p->ext = ext;
	ls = 0;
	if(n >= 3 && (uchar)s[0] == 0xEF && (uchar)s[1] == 0xBB && (uchar)s[2] == 0xBF)
		ls = 3;
	while(ls < n){
		_mdline(p, ls);
		ls = p->ln;
	}
	_mdfinish(p);
	_mdfinal(p, p->root);
	m = finish(p);
	freep(p);
	return m;
}

void
mdfree(Md *m)
{
	int i;

	if(m == nil)
		return;
	_mddecref((Mdp*)m->root);
	for(i = 0; i < m->nref; i++)
		free(m->ref[i].key);
	free(m->ref);
	free(m);
}
