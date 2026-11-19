#include <u.h>
#include <libc.h>
#include <prop.h>
#include "impl.h"

typedef struct Shrink	Shrink;

enum
{
	Maxshrink = 20000,
	Ncache = 1<<15,
};

struct Shrink
{
	Src	*s;
	Prop	*p;
	void	*a;
	uvlong	*v;
	uvlong	*n;
	int	nv;
	Span	*sp;
	int	nsp;
	uvlong	*c;
	int	mc;
	char	*err;
	char	*first;
	char	*notes;
	int	runs;
	int	steps;
	uvlong	*cache;
	int	ncache;
};

static int nruns = 100;
static uvlong seed;
static int seeded;
static uvlong *replay;
static int nreplay = -1;
static uchar *bytes;
static long nbytes = -1;
static char **names;
static int nnames;
static int verbose;
static int nfail;
static int forking;
static int limit;
static char *regf;
static char **regs;
static int nregs;

static void
usage(void)
{
	fprint(2, "usage: %s [-fv] [-n runs] [-s seed] [-t ms] [-r choices] [-b file] [-R file] [prop ...]\n", argv0);
	exits("usage");
}

static char*
slurp(char *f, long *np)
{
	char *b;
	long n, m;
	int fd;

	fd = open(f, OREAD);
	if(fd < 0)
		return nil;
	b = nil;
	n = 0;
	for(;;){
		b = erealloc(b, n+8192+1);
		m = read(fd, b+n, 8192);
		if(m <= 0)
			break;
		n += m;
	}
	close(fd);
	if(m < 0){
		free(b);
		return nil;
	}
	b[n] = 0;
	*np = n;
	return b;
}

static int
choices(char *t, uvlong **vp)
{
	uvlong *v;
	char *p, *q;
	int n, m;

	v = nil;
	n = m = 0;
	for(p = t;;){
		while(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
			p++;
		if(*p == 0)
			break;
		if(n == m){
			m = m*2 + 64;
			v = erealloc(v, m*sizeof v[0]);
		}
		v[n++] = strtoull(p, &q, 10);
		if(q == p){
			free(v);
			werrstr("bad choice at %.10s", p);
			return -1;
		}
		p = q;
	}
	*vp = v;
	return n;
}

static void
parsechoices(char *t)
{
	char *p;
	long m;

	p = t;
	while(*p != 0 && strchr("0123456789 \t\n", *p) != nil)
		p++;
	if(*p != 0 && (t = slurp(t, &m)) == nil)
		sysfatal("-r: %r");
	nreplay = choices(t, &replay);
	if(nreplay < 0)
		sysfatal("-r: %r");
}

static int
loadregs(void)
{
	char *b, *p, *q;
	long n;
	int ln;

	if(access(regf, AEXIST) < 0)
		return 0;
	b = slurp(regf, &n);
	if(b == nil){
		werrstr("%s: %r", regf);
		return -1;
	}
	ln = 0;
	for(p = b; *p != 0; p = q){
		ln++;
		q = strchr(p, '\n');
		if(q == nil)
			q = p + strlen(p);
		else
			*q++ = 0;
		if(*p == 0)
			continue;
		n = strcspn(p, " ");
		if(n == 0 || p[n] != ' ' || p[n+1+strspn(p+n+1, "0123456789 ")] != 0){
			werrstr("%s:%d: not a property's name and choices", regf, ln);
			free(b);
			return -1;
		}
		if(nregs % 16 == 0)
			regs = erealloc(regs, (nregs+16)*sizeof regs[0]);
		regs[nregs++] = _propdup(p);
	}
	free(b);
	return 0;
}

static void
savereg(char *name, uvlong *v, int nv)
{
	Fmt f;
	char *t;
	int fd, i;

	fmtstrinit(&f);
	fmtprint(&f, "%s", name);
	for(i = 0; i < nv; i++)
		fmtprint(&f, " %llud", v[i]);
	t = fmtstrflush(&f);
	for(i = 0; i < nregs; i++)
		if(strcmp(regs[i], t) == 0){
			free(t);
			return;
		}
	fd = open(regf, OWRITE);
	if(fd < 0)
		fd = create(regf, OWRITE, 0666);
	if(fd < 0 || seek(fd, 0, 2) < 0 || fprint(fd, "%s\n", t) < 0)
		fprint(2, "%s: -R %s: %r\n", argv0 != nil ? argv0 : "prop", regf);
	if(fd >= 0)
		close(fd);
	if(nregs % 16 == 0)
		regs = erealloc(regs, (nregs+16)*sizeof regs[0]);
	regs[nregs++] = t;
}

void
propargs(int argc, char **argv)
{
	ARGBEGIN{
	case 'n':
		nruns = atoi(EARGF(usage()));
		break;
	case 's':
		seed = strtoull(EARGF(usage()), nil, 0);
		seeded = 1;
		break;
	case 'r':
		parsechoices(EARGF(usage()));
		break;
	case 'b':
		bytes = (uchar*)slurp(EARGF(usage()), &nbytes);
		if(bytes == nil)
			sysfatal("-b: %r");
		break;
	case 'f':
		forking = 1;
		break;
	case 't':
		limit = atoi(EARGF(usage()));
		forking = 1;
		break;
	case 'R':
		regf = EARGF(usage());
		if(loadregs() < 0)
			sysfatal("-R: %r");
		break;
	case 'v':
		verbose++;
		break;
	default:
		usage();
	}ARGEND
	names = argv;
	nnames = argc;
	_propnotify();
}

void
propexits(void)
{
	exits(nfail ? "fail" : nil);
}

static int
named(char *name)
{
	int i;

	if(nnames == 0)
		return 1;
	for(i = 0; i < nnames; i++)
		if(strcmp(names[i], name) == 0)
			return 1;
	return 0;
}

static uvlong
hash(uvlong *v, int n)
{
	uvlong h;
	int i;

	h = 14695981039346656037ULL ^ n;
	for(i = 0; i < n; i++)
		h = (h ^ v[i]) * 1099511628211ULL;
	return h | 1;
}

static uvlong
strhash(char *s)
{
	uvlong h;

	h = 14695981039346656037ULL;
	while(*s)
		h = (h ^ (uchar)*s++) * 1099511628211ULL;
	return h;
}

static int
less(uvlong *a, int na, uvlong *b, int nb)
{
	int i;

	if(na != nb)
		return na < nb;
	for(i = 0; i < na; i++)
		if(a[i] != b[i])
			return a[i] < b[i];
	return 0;
}

static int
cached(Shrink *h, uvlong k, int add)
{
	int i;

	for(i = k % Ncache; h->cache[i] != 0; i = (i+1) % Ncache)
		if(h->cache[i] == k)
			return 1;
	if(add && h->ncache < Ncache/2){
		h->cache[i] = k;
		h->ncache++;
	}
	return 0;
}

static int
run(Src *s, Prop *p, void *a, char **err)
{
	int k;

	if(!forking)
		return _proprun(s, p, a, err);
	k = _propfork(s, p, a, err, limit);
	if(k < 0)
		sysfatal("%r");
	return k;
}

static void
adopt(Shrink *h, Src *s, char *e)
{
	int i;

	h->v = erealloc(h->v, (s->nv+1)*sizeof h->v[0]);
	h->n = erealloc(h->n, (s->nv+1)*sizeof h->n[0]);
	memmove(h->v, s->v, s->nv*sizeof h->v[0]);
	memmove(h->n, s->n, s->nv*sizeof h->n[0]);
	h->nv = s->nv;
	h->sp = erealloc(h->sp, (s->nsp+1)*sizeof h->sp[0]);
	h->nsp = 0;
	for(i = 0; i < s->nsp; i++)
		if(s->sp[i].e > s->sp[i].a)
			h->sp[h->nsp++] = s->sp[i];
	free(h->err);
	h->err = _propdup(e);
	free(h->notes);
	h->notes = _propnotes(s);
}

static uvlong*
cand(Shrink *h, int n)
{
	if(n+1 > h->mc){
		h->mc = n+1;
		h->c = erealloc(h->c, h->mc*sizeof h->c[0]);
	}
	return h->c;
}

static int
try(Shrink *h, uvlong *c, int nc)
{
	Src *s;
	uvlong k;
	char *e;

	if(_propstop || h->runs >= Maxshrink || !less(c, nc, h->v, h->nv))
		return 0;
	k = hash(c, nc);
	if(cached(h, k, 0))
		return 0;
	h->runs++;
	s = h->s;
	s->in = c;
	s->nin = nc;
	e = nil;
	if(run(s, h->p, h->a, &e) == Fail && !_propstop && less(s->v, s->nv, h->v, h->nv)){
		adopt(h, s, e);
		h->steps++;
		return 1;
	}
	if(_propstop)
		return 0;
	cached(h, k, 1);
	return 0;
}

static int
cut(Shrink *h, int a, int e)
{
	uvlong *c;
	int n;

	n = h->nv - (e-a);
	c = cand(h, h->nv);
	memmove(c, h->v, a*sizeof c[0]);
	memmove(c+a, h->v+e, (h->nv-e)*sizeof c[0]);
	return try(h, c, n);
}

static int
delspans(Shrink *h)
{
	int i, ch;

	ch = 0;
	for(i = 0; i < h->nsp; i++)
		if(cut(h, h->sp[i].a, h->sp[i].e)){
			ch = 1;
			i--;
		}
	return ch;
}

static int
delblocks(Shrink *h)
{
	int i, k, ch;

	ch = 0;
	for(k = 8; k >= 1; k /= 2)
		for(i = 0; i+k <= h->nv; i++)
			if(cut(h, i, i+k)){
				ch = 1;
				i--;
			}
	return ch;
}

static int
descend(Shrink *h)
{
	uvlong *c;
	int i, j, ch, n;
	Span x, y;

	ch = 0;
	for(i = 0; i < h->nsp; i++)
		for(j = i+1; j < h->nsp; j++){
			x = h->sp[i];
			y = h->sp[j];
			if(x.tag == nil || y.tag != x.tag || y.a < x.a || y.e > x.e || y.e-y.a == x.e-x.a)
				continue;
			n = h->nv - (x.e-x.a) + (y.e-y.a);
			c = cand(h, h->nv);
			memmove(c, h->v, x.a*sizeof c[0]);
			memmove(c+x.a, h->v+y.a, (y.e-y.a)*sizeof c[0]);
			memmove(c+x.a+(y.e-y.a), h->v+x.e, (h->nv-x.e)*sizeof c[0]);
			if(try(h, c, n)){
				ch = 1;
				j = i;
			}
		}
	return ch;
}

static int
setone(Shrink *h, int i, uvlong v)
{
	uvlong *c;

	c = cand(h, h->nv);
	memmove(c, h->v, h->nv*sizeof c[0]);
	c[i] = v;
	return try(h, c, h->nv);
}

static int
lower(Shrink *h)
{
	uvlong lo, hi, mid;
	int i, ch;

	ch = 0;
	for(i = 0; i < h->nv; i++){
		if(h->v[i] == 0)
			continue;
		if(setone(h, i, 0)){
			ch = 1;
			continue;
		}
		lo = 0;
		hi = h->v[i];
		while(lo+1 < hi && i < h->nv){
			mid = lo + (hi-lo)/2;
			if(setone(h, i, mid)){
				ch = 1;
				hi = i < h->nv ? h->v[i] : 0;
			}else
				lo = mid;
		}
	}
	return ch;
}

static int
setsame(Shrink *h, uvlong old, uvlong v)
{
	uvlong *c;
	int i;

	c = cand(h, h->nv);
	for(i = 0; i < h->nv; i++)
		c[i] = h->v[i] == old ? v : h->v[i];
	return try(h, c, h->nv);
}

static int
lowersame(Shrink *h)
{
	uvlong lo, hi, mid;
	int i, j, ch, k;

	ch = 0;
	for(i = 0; i < h->nv; i++){
		hi = h->v[i];
		if(hi == 0)
			continue;
		k = 0;
		for(j = 0; j < h->nv; j++)
			if(h->v[j] == hi)
				k++;
		if(k < 2 || setsame(h, hi, 0)){
			ch |= k >= 2;
			continue;
		}
		lo = 0;
		while(lo+1 < hi){
			mid = lo + (hi-lo)/2;
			if(setsame(h, hi, mid)){
				ch = 1;
				hi = mid;
			}else
				lo = mid;
		}
	}
	return ch;
}

static int
swaps(Shrink *h)
{
	uvlong *c;
	int i, j, ch, nx, ny;
	Span x, y;

	ch = 0;
	for(i = 0; i+1 < h->nv; i++)
		if(h->v[i] > h->v[i+1]){
			c = cand(h, h->nv);
			memmove(c, h->v, h->nv*sizeof c[0]);
			c[i] = h->v[i+1];
			c[i+1] = h->v[i];
			if(try(h, c, h->nv))
				ch = 1;
		}
	for(i = 0; i < h->nsp; i++)
		for(j = 0; j < h->nsp; j++){
			x = h->sp[i];
			y = h->sp[j];
			if(x.e != y.a || x.d != y.d)
				continue;
			nx = x.e - x.a;
			ny = y.e - y.a;
			c = cand(h, h->nv);
			memmove(c, h->v, h->nv*sizeof c[0]);
			memmove(c+x.a, h->v+y.a, ny*sizeof c[0]);
			memmove(c+x.a+ny, h->v+x.a, nx*sizeof c[0]);
			if(try(h, c, h->nv))
				ch = 1;
		}
	return ch;
}

static int
shift(Shrink *h)
{
	uvlong *c, d, b;
	int i, j, ch;

	ch = 0;
	for(i = 0; i < h->nv; i++)
		for(j = i+1; j < h->nv && j <= i+4; j++){
			for(d = h->v[i]; d > 0; d /= 2){
				b = h->n[j];
				if(h->v[i] == 0 || (b != 0 && h->v[j]+d >= b) || h->v[j]+d < h->v[j])
					continue;
				c = cand(h, h->nv);
				memmove(c, h->v, h->nv*sizeof c[0]);
				c[i] -= d;
				c[j] += d;
				if(try(h, c, h->nv)){
					ch = 1;
					break;
				}
			}
			if(i >= h->nv)
				break;
		}
	return ch;
}

static void
shrink(Shrink *h)
{
	int ch;

	h->cache = emalloc(Ncache*sizeof h->cache[0]);
	do{
		ch = delspans(h);
		ch |= delblocks(h);
		ch |= descend(h);
		ch |= lower(h);
		ch |= lowersame(h);
		ch |= swaps(h);
		ch |= shift(h);
	}while(ch && h->runs < Maxshrink && !_propstop);
	free(h->cache);
	h->cache = nil;
}

static void
report(char *name, char *err, char *first, char *notes, uvlong *v, int nv, char *how)
{
	char *p, *q;
	int i;

	print("%s: %s\n", name, err);
	if(notes != nil)
		for(p = notes; *p != 0; p = q){
			q = strchr(p, '\n');
			if(q == nil)
				q = p + strlen(p);
			print("\t%.*s\n", (int)(q-p), p);
			if(*q == '\n')
				q++;
		}
	if(first != nil && strcmp(first, err) != 0)
		print("\tfirst failure: %s\n", first);
	if(how != nil)
		print("\t%s\n", how);
	print("\t%s -r '", argv0 != nil ? argv0 : "prop");
	for(i = 0; i < nv; i++)
		print(i ? " %llud" : "%llud", v[i]);
	print("' %s\n", name);
}

static void
stopped(char *name, int runs)
{
	print("%s: interrupted after %d runs, with no failure\n", name, runs);
	exits("interrupted");
}

static void
hfree(Shrink *h)
{
	free(h->v);
	free(h->n);
	free(h->sp);
	free(h->c);
	free(h->err);
	free(h->first);
	free(h->notes);
}

static int
failed(char *name, Prop *p, void *a, Src *s, char *err, char *how)
{
	Shrink h;
	Src *r;
	char *e, *t, *n;
	int k;

	memset(&h, 0, sizeof h);
	r = _propnew(Replay);
	h.s = r;
	h.p = p;
	h.a = a;
	h.first = _propdup(err);
	adopt(&h, s, err);
	shrink(&h);
	nfail++;
	k = Discard;
	e = nil;
	if(!_propstop){
		r->in = h.v;
		r->nin = h.nv;
		k = run(r, p, a, &e);
	}
	if(_propstop){
		t = smprint("%s; interrupted after %d shrinks in %d runs", how, h.steps, h.runs);
		report(name, h.err, h.first, h.notes, h.v, h.nv, t);
		exits("interrupted");
	}
	if(k != Fail){
		print("%s: %s, and then passes on replay\n", name, h.err);
		hfree(&h);
		srcfree(r);
		return -1;
	}
	t = smprint("%s; %d shrinks in %d runs", how, h.steps, h.runs);
	n = _propnotes(r);
	report(name, e, h.first, n, r->v, r->nv, t);
	if(regf != nil)
		savereg(name, r->v, r->nv);
	free(n);
	free(t);
	hfree(&h);
	srcfree(r);
	return -1;
}

static int
replayregs(char *name, Prop *p, void *a)
{
	Src *s;
	uvlong *v;
	char *e, *how;
	int i, n, nr, nv, np, k, bad;

	bad = 0;
	np = 0;
	nr = nregs;
	for(i = 0; i < nr; i++){
		n = strcspn(regs[i], " ");
		if(strncmp(regs[i], name, n) != 0 || name[n] != 0)
			continue;
		nv = choices(regs[i]+n, &v);
		s = _propnew(Replay);
		s->in = v;
		s->nin = nv;
		e = nil;
		k = run(s, p, a, &e);
		if(_propstop)
			stopped(name, np);
		np++;
		if(k == Fail){
			how = smprint("from -R %s", regf);
			failed(name, p, a, s, e, how);
			free(how);
			bad = 1;
		}
		srcfree(s);
		free(v);
	}
	if(verbose && np > 0 && !bad)
		print("%s: %d cases from %s pass\n", name, np, regf);
	return bad ? -1 : 0;
}

int
forall(char *name, Prop *p, void *a)
{
	Src *s, *m;
	char *e, *how, *n;
	int i, nd, k;

	if(!named(name))
		return 0;
	if(!seeded){
		seed = (uvlong)truerand()<<32 | truerand();
		seeded = 1;
	}
	e = nil;
	if(nreplay >= 0){
		s = _propnew(Replay);
		s->in = replay;
		s->nin = nreplay;
		k = run(s, p, a, &e);
		if(_propstop)
			stopped(name, 0);
		if(k == Fail){
			n = _propnotes(s);
			report(name, e, nil, n, s->v, s->nv, nil);
			free(n);
			nfail++;
		}else if(verbose || k == Discard)
			print("%s: %s on replay\n", name, k == Pass ? "passes" : "discards its choices");
		srcfree(s);
		return k == Fail ? -1 : 0;
	}
	if(regf != nil && replayregs(name, p, a) < 0)
		return -1;
	if(nbytes >= 0){
		s = srcbytes(bytes, nbytes);
		k = run(s, p, a, &e);
		if(_propstop)
			stopped(name, 0);
		if(k == Fail){
			k = failed(name, p, a, s, e, "from -b");
			srcfree(s);
			return k;
		}
		if(verbose || k == Discard)
			print("%s: %s on -b\n", name, k == Pass ? "passes" : "discards its choices");
		srcfree(s);
		return 0;
	}
	m = _propnew(Rand);
	m->x = seed ^ strhash(name);
	s = _propnew(Rand);
	nd = 0;
	k = 0;
	for(i = 0; i < nruns; ){
		s->size = i*10/nruns + _proprnd(m)%3;
		s->x = _proprnd(m);
		k = run(s, p, a, &e);
		if(_propstop)
			stopped(name, i);
		if(k == Discard){
			if(++nd > 10*nruns + 100){
				print("%s: gave up after %d runs and %d discards\n", name, i, nd);
				nfail++;
				k = -1;
				break;
			}
			continue;
		}
		i++;
		if(k == Fail){
			how = smprint("found by run %d with -s %llud", i, seed);
			k = failed(name, p, a, s, e, how);
			free(how);
			break;
		}
	}
	if(k == Pass && verbose)
		print("%s: %d runs pass, %d discarded\n", name, nruns, nd);
	srcfree(s);
	srcfree(m);
	return k < 0 ? -1 : 0;
}
