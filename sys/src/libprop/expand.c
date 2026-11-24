#include <u.h>
#include <libc.h>
#include <prop.h>
#include "impl.h"

enum
{
	Deepest = -64,
};

static void sym(Src*, char**, char*, int, int, Fmt*);

static void
class(Src *s, char *p, Fmt *f)
{
	Rune r[256], lo, hi;
	char *q;
	uvlong n, v;
	int nr, i;

	nr = 0;
	n = 0;
	for(q = p; *q != 0 && *q != ']'; ){
		if(*q == '\\' && q[1] != 0)
			q++;
		q += chartorune(&lo, q);
		hi = lo;
		if(*q == '-' && q[1] != 0 && q[1] != ']'){
			q++;
			if(*q == '\\' && q[1] != 0)
				q++;
			q += chartorune(&hi, q);
		}
		if(hi < lo || nr+2 > nelem(r))
			sysfatal("expand: bad class [%s", p);
		r[nr++] = lo;
		r[nr++] = hi;
		n += hi - lo + 1;
	}
	if(n == 0)
		sysfatal("expand: empty class [%s", p);
	v = choose(s, n);
	for(i = 0; i < nr; i += 2){
		if(v <= r[i+1] - r[i]){
			fmtrune(f, r[i] + v);
			return;
		}
		v -= r[i+1] - r[i] + 1;
	}
}

static char*
itemend(char *p)
{
	char *q;

	if(*p == '<'){
		q = strchr(p, '>');
		if(q == nil)
			sysfatal("expand: no > in %s", p);
		return q+1;
	}
	for(q = p+1; *q != 0 && *q != ']'; q++)
		if(*q == '\\' && q[1] != 0)
			q++;
	if(*q != ']')
		sysfatal("expand: no ] in %s", p);
	return q+1;
}

static void
one(Src *s, char **g, char *p, int depth, Fmt *f)
{
	if(*p == '<')
		sym(s, g, p+1, strchr(p, '>')-(p+1), depth, f);
	else
		class(s, p+1, f);
}

static void
body(Src *s, char **g, char *p, int depth, Fmt *f)
{
	char *e;
	Rune r;
	int rep, stop;

	while(*p != 0){
		if(*p != '<' && *p != '['){
			if(*p == '\\' && p[1] != 0)
				p++;
			p += chartorune(&r, p);
			fmtrune(f, r);
			continue;
		}
		e = itemend(p);
		rep = *e;
		if(rep == '*' || rep == '+' || rep == '?')
			e++;
		else
			rep = 0;
		stop = *p == '<' && depth <= 0;
		switch(rep){
		case 0:
			one(s, g, p, depth, f);
			break;
		case '?':
			if(stop)
				break;
			span(s, nil);
			if(coin(s, 50))
				one(s, g, p, depth, f);
			endspan(s);
			break;
		case '+':
			one(s, g, p, depth, f);
		case '*':
			if(stop)
				break;
			while(more(s)){
				one(s, g, p, depth, f);
				endspan(s);
			}
			break;
		}
		p = e;
	}
}

static void
sym(Src *s, char **g, char *name, int n, int depth, Fmt *f)
{
	int i, k, first, na;

	first = -1;
	na = 0;
	for(i = 0; g[i] != nil; i++)
		if(strncmp(g[i], name, n) == 0 && g[i][n] == '\t'){
			if(first < 0)
				first = i;
			na++;
		}
	if(na == 0)
		sysfatal("expand: no rule for %.*s", utfnlen(name, n), name);
	if(depth < Deepest)
		discard(s);
	span(s, _propintern(name, n));
	k = 0;
	if(na > 1 && depth > 0)
		k = pick(s, nil, na);
	for(i = first; ; i++)
		if(strncmp(g[i], name, n) == 0 && g[i][n] == '\t' && k-- == 0)
			break;
	body(s, g, g[i]+n+1, depth-1, f);
	endspan(s);
}

char*
expand(Src *s, char **g, char *start, int depth)
{
	Fmt f;
	char *t;

	fmtstrinit(&f);
	sym(s, g, start, strlen(start), depth, &f);
	if((t = fmtstrflush(&f)) == nil)
		sysfatal("expand: out of memory");
	return t;
}
