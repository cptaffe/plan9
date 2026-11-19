#include <u.h>
#include <libc.h>
#include <prop.h>
#include "impl.h"

enum
{
	Most = 1<<20,
};

static char alpha[] = "\n\t !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";

static void
put(char **b, long *n, long at, char *t, long m)
{
	if(*n + m > Most)
		m = Most - *n;
	if(m <= 0)
		return;
	*b = erealloc(*b, *n+m+1);
	memmove(*b+at+m, *b+at, *n-at+1);
	memmove(*b+at, t, m);
	*n += m;
}

char*
mutate(Src *s, char **c, int nc)
{
	char *b, *t, *u;
	long n, a, k, m, at, from;
	uvlong r;
	int i;

	if(nc <= 0)
		sysfatal("prop: mutate of nothing");
	i = nc > 1 ? pick(s, nil, nc) : 0;
	t = c[i];
	n = strlen(t);
	r = n;
	if(s->mode == Rand && _proprnd(s)%4 == 0)
		r = _proprnd(s) % (n+1);
	k = _propnext(s, n+1, r);
	r = 0;
	if(s->mode == Rand && k < n && _proprnd(s)%2 == 0)
		r = _proprnd(s) % (n-k+1);
	a = _propnext(s, n-k+1, r);
	b = emalloc(k+1);
	memmove(b, t+a, k);
	n = k;
	while(more(s)){
		switch(choose(s, 4)){
		case 0:
			at = choose(s, n+1);
			m = choose(s, n-at+1);
			memmove(b+at, b+at+m, n-at-m+1);
			n -= m;
			break;
		case 1:
			at = choose(s, n+1);
			put(&b, &n, at, &alpha[choose(s, sizeof alpha-1)], 1);
			break;
		case 2:
			u = c[nc > 1 ? pick(s, nil, nc) : 0];
			k = strlen(u);
			from = choose(s, k+1);
			m = choose(s, (k-from < 64 ? k-from : 64) + 1);
			at = choose(s, n+1);
			put(&b, &n, at, u+from, m);
			break;
		case 3:
			from = choose(s, n+1);
			m = choose(s, (n-from < 64 ? n-from : 64) + 1);
			at = choose(s, n+1);
			u = emalloc(m+1);
			memmove(u, b+from, m);
			put(&b, &n, at, u, m);
			free(u);
			break;
		}
		endspan(s);
	}
	return b;
}
