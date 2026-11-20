#include <u.h>
#include <libc.h>
#include <patch.h>

void
splittext(Text *t, char *buf, long n)
{
	char *p, *e, *q;
	int m;

	m = 0;
	for(p = buf, e = buf+n; p < e; p = q+1){
		if((q = memchr(p, '\n', e-p)) == nil)
			q = e-1;
		m++;
	}
	t->l = emalloc((m+1)*sizeof(Line));
	t->n = m;
	m = 0;
	for(p = buf; p < e; p = q+1){
		if((q = memchr(p, '\n', e-p)) == nil)
			q = e-1;
		t->l[m].s = p;
		t->l[m].n = q+1-p;
		m++;
	}
}

static int
match(Text *t, int at, Text *o)
{
	int i;
	Line *a, *b;

	if(at < 0 || at+o->n > t->n)
		return 0;
	for(i = 0; i < o->n; i++){
		a = &t->l[at+i];
		b = &o->l[i];
		if(a->n != b->n || memcmp(a->s, b->s, a->n) != 0)
			return 0;
	}
	return 1;
}

static int
expect(Hunk *h)
{
	if(h->oldcnt == 0)
		return h->oldln;
	return h->oldln-1;
}

int
findhunk(Diff *d, Hunk *h, Text *t, int lo, int drift)
{
	int at, hi, f, b, begin, end;

	if(d->flags&Pzero){
		begin = h->oldln == 0 && h->oldcnt == 0;
		end = 0;
	}else{
		begin = h->oldln <= 1;
		end = h->trail == 0;
	}
	hi = t->n - h->old.n;
	if(begin && end)
		return lo == 0 && hi == 0 && match(t, 0, &h->old) ? 0 : -1;
	if(begin)
		return lo == 0 && match(t, 0, &h->old) ? 0 : -1;
	if(end)
		return hi >= lo && match(t, hi, &h->old) ? hi : -1;
	at = expect(h) + drift;
	if(at > hi)
		at = hi;
	if(at < lo)
		at = lo;
	for(f = at, b = at-1; f <= hi || b >= lo; f++, b--){
		if(f <= hi && match(t, f, &h->old))
			return f;
		if(b >= lo && match(t, b, &h->old))
			return b;
	}
	return -1;
}

int
placehunks(Diff *d, Text *t, int *at)
{
	int i, lo, drift;
	Hunk *h;

	lo = 0;
	drift = 0;
	for(i = 0; i < d->nh; i++){
		h = &d->h[i];
		if((at[i] = findhunk(d, h, t, lo, drift)) < 0){
			werrstr("hunk at line %d does not apply", h->lnum);
			return -1;
		}
		drift = at[i] - expect(h);
		lo = at[i] + h->old.n;
	}
	return 0;
}

static char*
put(char *p, Line *l, int n)
{
	int i;

	for(i = 0; i < n; i++){
		memmove(p, l[i].s, l[i].n);
		p += l[i].n;
	}
	return p;
}

static long
size(Line *l, int n)
{
	long m;
	int i;

	m = 0;
	for(i = 0; i < n; i++)
		m += l[i].n;
	return m;
}

char*
applydiff(Diff *d, Text *t, long *np)
{
	int i, *at, k;
	long n;
	char *b, *p, *e;
	Hunk *h;

	at = emalloc((d->nh+1)*sizeof(int));
	if(placehunks(d, t, at) < 0){
		free(at);
		return nil;
	}
	n = size(t->l, t->n);
	for(i = 0; i < d->nh; i++)
		n += size(d->h[i].new.l, d->h[i].new.n) - size(d->h[i].old.l, d->h[i].old.n);
	b = p = emalloc(n+1);
	k = 0;
	for(i = 0; i < d->nh; i++){
		h = &d->h[i];
		p = put(p, t->l+k, at[i]-k);
		e = put(p, h->new.l, h->new.n);
		k = at[i] + h->old.n;
		if(e > p && e[-1] != '\n' && (k < t->n || i < d->nh-1 && d->h[i+1].new.n > 0)){
			werrstr("hunk at line %d leaves a line without a newline inside the file", h->lnum);
			free(at);
			free(b);
			return nil;
		}
		p = e;
	}
	p = put(p, t->l+k, t->n-k);
	*p = 0;
	free(at);
	*np = p-b;
	return b;
}
