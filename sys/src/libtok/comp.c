#include <u.h>
#include <libc.h>
#include <regexp.h>
#include <tok.h>
#include "impl.h"

typedef struct Node	Node;
typedef struct P	P;

enum
{
	Ntok = 1,
	Ncat,
	Nalt,
	Nrep,
	Nempty,
};

struct Node
{
	int	type;
	int	op;
	Test	*t;
	int	bind;
	Node	*l;
	Node	*r;
};

struct P
{
	char	**w;
	int	nw;
	int	i;
	Tpat	*p;
	Node	**nodes;
	int	nnodes;
	int	stk[64];
	int	nstk;
	char	err[ERRMAX];
};

static Node*	alt(P*);

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

static Node*
node(P *p, int type, Node *l, Node *r)
{
	Node *n;

	n = emalloc(sizeof *n);
	n->type = type;
	n->l = l;
	n->r = r;
	n->bind = -1;
	p->nodes = erealloc(p->nodes, (p->nnodes+1)*sizeof p->nodes[0]);
	p->nodes[p->nnodes++] = n;
	return n;
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
	if(tp->nbind == Tmaxbind){
		perr(p, "more than %d names", Tmaxbind);
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
	char *b, *e, err[ERRMAX];

	if(w[0] == '.' && w[1] == 0)
		return newtest(p, Tany);
	if(w[0] == '@' && w[1] != 0){
		t = newtest(p, Tname);
		t->s = sdup(w+1);
		if((b = strchr(t->s, '{')) != nil){
			*b++ = 0;
			if((e = strchr(b, '}')) == nil || e[1] != 0){
				perr(p, "%s: no } after the tag", w);
				return t;
			}
			*e = 0;
			t->tag = b;
			if(*b == 0)
				perr(p, "%s: empty tag", w);
		}
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

static Node*
item(P *p)
{
	Node *n;
	Test *t, *m;
	char *w, *s, *f[64], *set;
	int q, l, bind, neg, i, nf;

	w = p->w[p->i++];
	if(strcmp(w, "\\(") == 0){
		n = alt(p);
		if(p->i >= p->nw || strncmp(p->w[p->i], "\\)", 2) != 0){
			perr(p, "no \\) for \\(");
			return n;
		}
		w = p->w[p->i++];
		q = 0;
		if(strcmp(w, "\\)*") == 0)
			q = '*';
		else if(strcmp(w, "\\)\\+") == 0)
			q = '+';
		else if(strcmp(w, "\\)\\?") == 0)
			q = '?';
		else if(strcmp(w, "\\)") != 0)
			perr(p, "%s: bad \\)", w);
		if(q){
			n = node(p, Nrep, n, nil);
			n->op = q;
		}
		return n;
	}
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
	if(s[0] == '[' && l > 1 && !(l == 2 && s[1] == ']')){
		set = sdup(s+1);
		for(;;){
			quant(set, &l);
			if(l > 0 && set[l-1] == ']' && (l < 2 || set[l-2] != '\\'))
				break;
			if(p->i >= p->nw){
				perr(p, "no ] for [");
				free(set);
				return node(p, Nempty, nil, nil);
			}
			w = p->w[p->i++];
			s = emalloc(strlen(set)+strlen(w)+2);
			sprint(s, "%s %s", set, w);
			free(set);
			set = s;
		}
	}
	q = quant(set != nil ? set : s, &l);
	if(set != nil){
		set[l-1] = 0;
		t = newtest(p, Tset);
		nf = tokenize(set, f, nelem(f));
		for(i = 0; i < nf; i++){
			m = simple(p, f[i]);
			if(m->type == Tref)
				perr(p, "%s: a name in a set", f[i]);
			t->set = erealloc(t->set, (t->nset+1)*sizeof t->set[0]);
			t->set[t->nset++] = m;
		}
		free(set);
		if(nf == 0)
			perr(p, "empty set");
	}else{
		s = sdup(s);
		s[l] = 0;
		t = simple(p, s);
		free(s);
	}
	t->neg = neg;
	if(t->type == Tref && bind >= 0)
		perr(p, "%s: a name bound to a name", w);
	n = node(p, Ntok, nil, nil);
	n->t = t;
	n->bind = bind;
	if(q){
		n = node(p, Nrep, n, nil);
		n->op = q;
	}
	return n;
}

static Node*
seq(P *p)
{
	Node *n;

	n = nil;
	while(p->i < p->nw && strcmp(p->w[p->i], "\\|") != 0 && strncmp(p->w[p->i], "\\)", 2) != 0)
		n = n == nil ? item(p) : node(p, Ncat, n, item(p));
	if(n == nil)
		n = node(p, Nempty, nil, nil);
	return n;
}

static Node*
alt(P *p)
{
	Node *n;

	n = seq(p);
	while(p->i < p->nw && strcmp(p->w[p->i], "\\|") == 0){
		p->i++;
		n = node(p, Nalt, n, seq(p));
	}
	return n;
}

static int
emit(P *p, int op)
{
	Tpat *tp;
	Inst *i;

	tp = p->p;
	tp->i = erealloc(tp->i, (tp->ni+1)*sizeof tp->i[0]);
	i = &tp->i[tp->ni];
	memset(i, 0, sizeof *i);
	i->op = op;
	i->bind = -1;
	return tp->ni++;
}

static void
gen(P *p, Node *n)
{
	Inst *i;
	int a, b, c, s;

	switch(n->type){
	case Nempty:
		break;
	case Ntok:
		a = emit(p, Itok);
		i = &p->p->i[a];
		i->t = n->t;
		i->bind = n->bind;
		i->nstars = p->nstk;
		if(p->nstk > 0){
			i->stars = emalloc(p->nstk*sizeof i->stars[0]);
			memmove(i->stars, p->stk, p->nstk*sizeof i->stars[0]);
		}
		break;
	case Ncat:
		gen(p, n->l);
		gen(p, n->r);
		break;
	case Nalt:
		a = emit(p, Isplit);
		p->p->i[a].x = a+1;
		gen(p, n->l);
		b = emit(p, Ijmp);
		p->p->i[a].y = p->p->ni;
		gen(p, n->r);
		p->p->i[b].x = p->p->ni;
		break;
	case Nrep:
		if(p->nstk == nelem(p->stk)){
			perr(p, "repetitions nested too deep");
			return;
		}
		s = p->p->nstar++;
		a = emit(p, Isbeg);
		p->p->i[a].star = s;
		p->stk[p->nstk++] = s;
		switch(n->op){
		case '*':
			b = emit(p, Isplit);
			p->p->i[b].x = b+1;
			gen(p, n->l);
			c = emit(p, Ijmp);
			p->p->i[c].x = b;
			p->p->i[b].y = p->p->ni;
			break;
		case '+':
			b = p->p->ni;
			gen(p, n->l);
			c = emit(p, Isplit);
			p->p->i[c].x = b;
			p->p->i[c].y = c+1;
			break;
		case '?':
			b = emit(p, Isplit);
			p->p->i[b].x = b+1;
			gen(p, n->l);
			p->p->i[b].y = p->p->ni;
			break;
		}
		p->nstk--;
		a = emit(p, Isend);
		p->p->i[a].star = s;
		break;
	}
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
	Node *n;
	Tpat *tp;
	char *s;
	int i;

	memset(&p, 0, sizeof p);
	p.p = tp = emalloc(sizeof *tp);
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
	n = alt(&p);
	if(p.i < p.nw)
		perr(&p, "%s: no \\( before it", p.w[p.i]);
	if(p.err[0] == 0){
		gen(&p, n);
		emit(&p, Imatch);
		check(&p);
	}
	for(i = 0; i < p.nnodes; i++)
		free(p.nodes[i]);
	free(p.nodes);
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
toknbind(Tpat *p)
{
	return p->nbind;
}

char*
tokbindname(Tpat *p, int i)
{
	if(i < 0 || i >= p->nbind)
		return nil;
	return p->bname[i];
}
