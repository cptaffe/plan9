#include <u.h>
#include <libc.h>
#include <regexp.h>
#include <tok.h>
#include "impl.h"

typedef struct Lx	Lx;
typedef struct Frag	Frag;
typedef struct Ator	Ator;
typedef struct P	P;

enum
{
	Maxdepth = 100,

	Ostart = 0x100,
	Orbra,
	Olbra,
	Oor,
	Ocat,
	Ostar,
	Oplus,
	Oquest,
};

struct Lx
{
	int	type;
	Test	*t;
	int	bind;
	int	sub;
};

struct Frag
{
	int	first;
	int	last;
	int	lo;
};

struct Ator
{
	int	op;
	int	sub;
};

struct P
{
	Tpat	*p;
	char	**w;
	int	nw;
	int	i;
	Lx	*lx;
	int	nlx;
	int	mlx;
	Frag	*and;
	int	nand;
	int	mand;
	Ator	*ator;
	int	nator;
	int	mator;
	int	lastwasand;
	int	nbra;
	int	back;
	int	nstar;
	int	depth;
	char	err[ERRMAX];
};

static char*
sdup(char *s)
{
	char *t;

	t = emalloc(strlen(s)+1);
	strcpy(t, s);
	return t;
}

static void
perr(P *p, char *fmt, ...)
{
	va_list a;

	if(p->err[0] != 0)
		return;
	va_start(a, fmt);
	vsnprint(p->err, sizeof p->err, fmt, a);
	va_end(a);
}

static Test*
newtest(P *p, int type)
{
	Test *t;
	Tpat *tp;

	tp = p->p;
	t = emalloc(sizeof *t);
	t->type = type;
	tp->tests = erealloc(tp->tests, (tp->ntests+1)*sizeof tp->tests[0]);
	tp->tests[tp->ntests++] = t;
	return t;
}

static int
isname(char *s, int n)
{
	int i;

	if(n <= 0)
		return 0;
	for(i = 0; i < n; i++)
		if(!(s[i] == '_' || s[i] >= 'a' && s[i] <= 'z' || s[i] >= 'A' && s[i] <= 'Z' || i > 0 && s[i] >= '0' && s[i] <= '9'))
			return 0;
	return 1;
}

static int
bindof(P *p, char *s, int n)
{
	Tpat *tp;
	int i;

	tp = p->p;
	for(i = 0; i < tp->nbind; i++)
		if(strncmp(tp->bname[i], s, n) == 0 && tp->bname[i][n] == 0)
			return i;
	if(tp->nbind == Tmaxcap){
		perr(p, "more than %d names", Tmaxcap);
		return -1;
	}
	tp->bname[i] = emalloc(n+1);
	memmove(tp->bname[i], s, n);
	tp->nbind++;
	return i;
}

static char*
unescape(char *s)
{
	char *t, *q;

	t = q = emalloc(strlen(s)+1);
	for(; *s != 0; s++){
		if(*s == '\\' && s[1] != 0)
			s++;
		*q++ = *s;
	}
	*q = 0;
	return t;
}

static Test*
simple(P *p, char *w)
{
	Test *t;
	char err[ERRMAX];

	if(w[0] == '.' && w[1] == 0)
		return newtest(p, Tany);
	if(w[0] == '@' && w[1] != 0){
		t = newtest(p, Tname);
		t->s = sdup(w+1);
		if(strpbrk(t->s, "{}") != nil)
			perr(p, "%s: bad tag", w);
		if(t->s[0] == 0)
			perr(p, "%s: @ needs a name", w);
		if(_tokname(t->s, "comment"))
			p->p->comments = 1;
		return t;
	}
	if(w[0] == '/' && w[1] != 0){
		t = newtest(p, Tre);
		werrstr("bad regular expression");
		if((t->re = regcomp(w+1)) == nil){
			rerrstr(err, sizeof err);
			perr(p, "%s: %s", w, err);
		}
		return t;
	}
	if(w[0] == ':' && isname(w+1, strlen(w+1))){
		t = newtest(p, Tref);
		t->ref = bindof(p, w+1, strlen(w+1));
		return t;
	}
	t = newtest(p, Tlit);
	t->s = unescape(w);
	return t;
}

static int
quant(char *w, int *n)
{
	int l;

	l = strlen(w);
	if(l > 2 && w[l-2] == '\\' && (w[l-1] == '+' || w[l-1] == '?') && (l < 3 || w[l-3] != '\\')){
		*n = l-2;
		return w[l-1];
	}
	if(l > 1 && w[l-1] == '*' && strchr(":^\\", w[l-2]) == nil){
		*n = l-1;
		return '*';
	}
	*n = l;
	return 0;
}

static int
istag(int c)
{
	return c == '_' || c == '.' || c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c >= '0' && c <= '9';
}

static char*
tagof(char *s, int *lp)
{
	char *t;
	int l, b, i;

	l = *lp;
	if(l < 4 || s[l-1] != '}' || s[l-2] == '\\')
		return nil;
	for(b = l-2; b > 0 && s[b] != '{'; b--)
		if(!istag(s[b]))
			return nil;
	if(b <= 0 || b == l-2 || s[b-1] == '\\')
		return nil;
	t = emalloc(l-b-1);
	for(i = b+1; i < l-1; i++)
		t[i-b-1] = s[i];
	*lp = b;
	return t;
}

static void
addlx(P *p, int type, Test *t, int bind, int sub)
{
	Lx *l;

	if(p->nlx == p->mlx){
		p->mlx = p->mlx ? 2*p->mlx : 64;
		p->lx = erealloc(p->lx, p->mlx*sizeof p->lx[0]);
	}
	l = &p->lx[p->nlx++];
	l->type = type;
	l->t = t;
	l->bind = bind;
	l->sub = sub;
}

static int
newcap(P *p)
{
	if(p->p->ncap == Tmaxcap){
		perr(p, "more than %d groups and names", Tmaxcap);
		return 0;
	}
	return p->p->ncap++;
}

static void
item(P *p)
{
	Test *t, *m;
	char *w, *s, **f, *set, *tag;
	int q, l, bind, neg, i, nf;

	w = p->w[p->i++];
	bind = -1;
	neg = 0;
	s = w;
	if((l = strcspn(s, ":")) < strlen(s) && isname(s, l) && s[l+1] != 0){
		bind = bindof(p, s, l);
		s += l+1;
	}
	if(s[0] == '^' && s[1] != 0){
		neg = 1;
		s++;
	}
	set = nil;
	quant(s, &l);
	free(tagof(s, &l));
	if(s[0] == '[' && l > 1 && !(l == 2 && s[1] == ']')){
		set = sdup(s+1);
		for(;;){
			quant(set, &l);
			free(tagof(set, &l));
			if(l > 0 && set[l-1] == ']' && (l < 2 || set[l-2] != '\\'))
				break;
			if(p->i >= p->nw){
				perr(p, "no ] for [");
				free(set);
				return;
			}
			w = p->w[p->i++];
			s = emalloc(strlen(set)+strlen(w)+2);
			sprint(s, "%s %s", set, w);
			free(set);
			set = s;
		}
	}
	q = quant(set != nil ? set : s, &l);
	tag = tagof(set != nil ? set : s, &l);
	if(set != nil){
		set[l-1] = 0;
		t = newtest(p, Tset);
		f = emalloc((strlen(set)/2+2)*sizeof f[0]);
		nf = tokenize(set, f, strlen(set)/2+1);
		for(i = 0; i < nf; i++){
			m = simple(p, f[i]);
			if(m->type == Tref)
				perr(p, "%s: a name in a set", f[i]);
			t->set = erealloc(t->set, (t->nset+1)*sizeof t->set[0]);
			t->set[t->nset++] = m;
		}
		free(f);
		free(set);
		if(nf == 0)
			perr(p, "empty set");
	}else{
		s = sdup(s);
		s[l] = 0;
		if(tag != nil && strcmp(s, "@") == 0)
			perr(p, "@{%s}: write .{%s}", tag, tag);
		t = simple(p, s);
		free(s);
	}
	t->neg = neg;
	t->tag = tag;
	if(t->type == Tref){
		p->p->refs = 1;
		if(bind >= 0)
			perr(p, "%s: a name bound to a name", w);
	}
	if(bind >= 0 && p->p->bcap[bind] < 0){
		i = newcap(p);
		p->p->bcap[bind] = i;
		p->p->capbind[i] = bind;
	}
	addlx(p, Itok, t, bind, -1);
	if(q)
		addlx(p, q == '*' ? Ostar : q == '+' ? Oplus : Oquest, nil, -1, -1);
}

static void
lexall(P *p)
{
	char *w;
	int c;

	while(p->i < p->nw && p->err[0] == 0){
		w = p->w[p->i];
		if(strcmp(w, "\\(") == 0){
			p->i++;
			if(++p->depth > Maxdepth){
				perr(p, "groups nested too deep");
				return;
			}
			c = newcap(p);
			p->p->capbind[c] = -1;
			addlx(p, Olbra, nil, -1, c);
		}else if(strncmp(w, "\\)", 2) == 0){
			p->i++;
			p->depth--;
			addlx(p, Orbra, nil, -1, -1);
			if(strcmp(w, "\\)*") == 0)
				addlx(p, Ostar, nil, -1, -1);
			else if(strcmp(w, "\\)\\+") == 0)
				addlx(p, Oplus, nil, -1, -1);
			else if(strcmp(w, "\\)\\?") == 0)
				addlx(p, Oquest, nil, -1, -1);
			else if(strcmp(w, "\\)") != 0)
				perr(p, "%s: bad \\)", w);
		}else if(strcmp(w, "\\|") == 0){
			p->i++;
			addlx(p, Oor, nil, -1, -1);
		}else
			item(p);
	}
}

static int
newinst(P *p, int op)
{
	Tpat *tp;
	Inst *i;

	tp = p->p;
	if(tp->ni == tp->mi){
		tp->mi = tp->mi ? 2*tp->mi : 64;
		tp->i = erealloc(tp->i, tp->mi*sizeof tp->i[0]);
	}
	i = &tp->i[tp->ni];
	memset(i, 0, sizeof *i);
	i->op = op;
	i->next = -1;
	i->right = -1;
	i->bind = -1;
	return tp->ni++;
}

#define	I(n)	(p->p->i[n])

static void
pushand(P *p, int f, int l, int lo)
{
	if(p->nand == p->mand){
		p->mand = p->mand ? 2*p->mand : 32;
		p->and = erealloc(p->and, p->mand*sizeof p->and[0]);
	}
	p->and[p->nand].first = f;
	p->and[p->nand].last = l;
	p->and[p->nand].lo = lo;
	p->nand++;
}

static Frag
popand(P *p)
{
	Frag f;
	int i;

	if(p->nand == 0){
		perr(p, "an operator without its operand");
		i = newinst(p, Inop);
		f.first = f.last = f.lo = i;
		return f;
	}
	return p->and[--p->nand];
}

static void
pushator(P *p, int op, int sub)
{
	if(p->nator == p->mator){
		p->mator = p->mator ? 2*p->mator : 32;
		p->ator = erealloc(p->ator, p->mator*sizeof p->ator[0]);
	}
	p->ator[p->nator].op = op;
	p->ator[p->nator].sub = sub;
	p->nator++;
}

static void
instar(P *p, int lo, int s)
{
	Inst *i;
	int k;

	for(k = lo; k < p->p->ni; k++){
		i = &I(k);
		if(i->op != Itok)
			continue;
		i->stars = erealloc(i->stars, (i->nstars+1)*sizeof i->stars[0]);
		i->stars[i->nstars++] = s;
	}
}

static void
evaluntil(P *p, int pri)
{
	Frag a, b, t;
	Ator o;
	int i1, i2, ib, s;

	while(p->nator > 0 && (pri == Orbra || p->ator[p->nator-1].op >= pri)){
		o = p->ator[--p->nator];
		switch(o.op){
		case Olbra:
			a = popand(p);
			i2 = newinst(p, Irbra);
			I(i2).sub = o.sub;
			I(a.last).next = i2;
			i1 = newinst(p, Ilbra);
			I(i1).sub = o.sub;
			I(i1).next = a.first;
			pushand(p, i1, i2, a.lo);
			return;
		case Oor:
			b = popand(p);
			a = popand(p);
			i2 = newinst(p, Inop);
			I(b.last).next = i2;
			I(a.last).next = i2;
			i1 = newinst(p, Ior);
			I(i1).right = a.first;
			I(i1).next = b.first;
			pushand(p, i1, i2, a.lo < b.lo ? a.lo : b.lo);
			break;
		case Ocat:
			b = popand(p);
			a = popand(p);
			if(p->back && I(b.first).op != Iend){
				t = a;
				a = b;
				b = t;
			}
			I(a.last).next = b.first;
			pushand(p, a.first, b.last, a.lo < b.lo ? a.lo : b.lo);
			break;
		case Ostar:
		case Oplus:
		case Oquest:
			b = popand(p);
			s = p->nstar++;
			instar(p, b.lo, s);
			i1 = newinst(p, Ior);
			i2 = newinst(p, Isend);
			I(i2).star = s;
			ib = newinst(p, Isbeg);
			I(ib).star = s;
			I(i1).right = b.first;
			I(i1).next = i2;
			if(o.op == Oquest){
				I(b.last).next = i2;
				I(ib).next = i1;
			}else{
				I(b.last).next = i1;
				I(ib).next = o.op == Ostar ? i1 : b.first;
			}
			pushand(p, ib, i2, b.lo);
			break;
		default:
			perr(p, "unknown operator");
			return;
		}
	}
}

static void
operator(P *p, int op, int sub)
{
	if(op == Orbra && --p->nbra < 0){
		perr(p, "\\): no \\( before it");
		p->nbra = 0;
		return;
	}
	if(op == Olbra){
		p->nbra++;
		if(p->lastwasand)
			operator(p, Ocat, -1);
	}else
		evaluntil(p, op);
	if(op != Orbra)
		pushator(p, op, sub);
	p->lastwasand = op == Ostar || op == Oquest || op == Oplus || op == Orbra;
}

static void
operand(P *p, Lx *l)
{
	int i;

	if(p->lastwasand)
		operator(p, Ocat, -1);
	i = newinst(p, l->type);
	I(i).t = l->t;
	I(i).bind = l->bind;
	pushand(p, i, i, i);
	p->lastwasand = 1;
}

static void
optimize(P *p, int lo, int hi)
{
	int k, n;

	for(k = lo; k < hi; k++){
		if(I(k).op == Iend)
			continue;
		n = I(k).next;
		while(n >= 0 && I(n).op == Inop)
			n = I(n).next;
		I(k).next = n;
	}
}

static int
compile1(P *p, int back)
{
	Lx end;
	int k, lo;

	lo = p->p->ni;
	p->back = back;
	p->nand = p->nator = 0;
	p->lastwasand = 0;
	p->nbra = 0;
	p->nstar = 0;
	pushator(p, Ostart-1, -1);
	for(k = 0; k < p->nlx && p->err[0] == 0; k++){
		if(p->lx[k].type >= Ostart)
			operator(p, p->lx[k].type, p->lx[k].sub);
		else
			operand(p, &p->lx[k]);
	}
	evaluntil(p, Ostart);
	memset(&end, 0, sizeof end);
	end.type = Iend;
	end.bind = -1;
	operand(p, &end);
	evaluntil(p, Ostart);
	if(p->nbra)
		perr(p, "no \\) for \\(");
	if(p->err[0] != 0 || p->nand != 1)
		return -1;
	optimize(p, lo, p->p->ni);
	if(p->nstar > p->p->nstar)
		p->p->nstar = p->nstar;
	return p->and[0].first;
}

static void
check(P *p)
{
	Tpat *tp;
	int i, *def;

	tp = p->p;
	def = emalloc((tp->nbind+1)*sizeof def[0]);
	for(i = 0; i < tp->ni; i++)
		if(tp->i[i].op == Itok && tp->i[i].bind >= 0)
			def[tp->i[i].bind] = 1;
	for(i = 0; i < tp->nbind; i++)
		if(!def[i])
			perr(p, ":%s is not bound", tp->bname[i]);
	free(def);
}

Tpat*
tokcomp(char *pat)
{
	P p;
	Tpat *tp;
	char *s;
	int i;

	memset(&p, 0, sizeof p);
	p.p = tp = emalloc(sizeof *tp);
	memset(tp->bcap, 0xff, sizeof tp->bcap);
	memset(tp->capbind, 0xff, sizeof tp->capbind);
	s = sdup(pat);
	for(i = 0; s[i] != 0; i++)
		if(s[i] == '\n')
			s[i] = ' ';
	p.w = emalloc((strlen(s)/2+2)*sizeof p.w[0]);
	p.nw = getfields(s, p.w, strlen(s)/2+1, 1, " \t\r");
	i = 0;
	for(p.i = 0; p.i < p.nw; p.i++)
		if(p.w[p.i][0] != 0)
			p.w[i++] = p.w[p.i];
	p.nw = i;
	p.i = 0;
	if(p.nw == 0)
		perr(&p, "empty pattern");
	lexall(&p);
	if(p.err[0] == 0)
		tp->start = compile1(&p, 0);
	if(p.err[0] == 0)
		tp->bstart = compile1(&p, 1);
	if(p.err[0] == 0)
		check(&p);
	free(p.lx);
	free(p.and);
	free(p.ator);
	free(p.w);
	free(s);
	if(p.err[0] != 0){
		tokpatfree(tp);
		werrstr("%s", p.err);
		return nil;
	}
	return tp;
}

void
tokpatfree(Tpat *tp)
{
	Test *t;
	int i;

	if(tp == nil)
		return;
	for(i = 0; i < tp->ntests; i++){
		t = tp->tests[i];
		if(t->type == Tname || t->type == Tlit)
			free(t->s);
		free(t->tag);
		free(t->re);
		free(t->set);
		free(t);
	}
	free(tp->tests);
	for(i = 0; i < tp->ni; i++)
		free(tp->i[i].stars);
	free(tp->i);
	for(i = 0; i < tp->nbind; i++)
		free(tp->bname[i]);
	free(tp);
}

int
tokncap(Tpat *p)
{
	return p->ncap;
}

char*
tokcapname(Tpat *p, int i)
{
	if(i < 0 || i >= p->ncap || p->capbind[i] < 0)
		return nil;
	return p->bname[p->capbind[i]];
}
