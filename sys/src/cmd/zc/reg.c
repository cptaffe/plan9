#include "gc.h"

enum
{
	NVARS	= 512,
	BIG	= 0x7fffffff,
};

typedef struct Var Var;
struct Var
{
	long	off;
	int	size;
	int	cls;
	int	bad;
	int	idx;
	Sym*	sym;
};

static	Var	vars[NVARS];
static	int	nvars;
static	int	toomany;

static int
opsize(int as, int *cls)
{
	*cls = 'R';
	switch(as) {
	case AMOVB:
	case AMOVBU:
		return 1;
	case AMOVH:
	case AMOVHU:
		return 2;
	case AMOVW:
		return 4;
	case AMOVV:
		*cls = 'V';
		return 8;
	case AMOVF:
		*cls = 'f';
		return 4;
	case AMOVD:
		*cls = 'F';
		return 8;
	}
	return 0;
}

static int
isframe(Adr *a)
{
	return a->name == D_AUTO || a->name == D_PARAM;
}

static Var*
var(long off, int size, int cls, Sym *s)
{
	Var *v;
	int i;

	for(i=0; i<nvars; i++) {
		v = &vars[i];
		if(v->off == off && v->size == size && v->cls == cls) {
			if(v->sym == S)
				v->sym = s;
			else if(s != S && v->sym != s)
				v->bad = 1;
			return v;
		}
	}
	if(nvars >= NVARS) {
		toomany = 1;
		return nil;
	}
	v = &vars[nvars++];
	v->off = off;
	v->size = size;
	v->cls = cls;
	v->bad = 0;
	v->idx = -1;
	v->sym = s;
	return v;
}

static void
taint(long lo, long hi)
{
	int i;

	for(i=0; i<nvars; i++)
		if(vars[i].off < hi && vars[i].off + vars[i].size > lo)
			vars[i].bad = 1;
}

static void
frameuse(Prog *p, Adr *a)
{
	int size, cls;

	if(a->type != D_OREG || !isframe(a) || p->as == ANOP)
		return;
	size = opsize(p->as, &cls);
	if(size == 0) {
		diag(Z, "regopt: frame operand of %A", p->as);
		return;
	}
	var(a->offset, size, cls, a->sym);
}

static void
address(Adr *a)
{
	int i;

	if(a->type != D_CONST || !isframe(a))
		return;
	if(a->name != D_AUTO || a->sym == S || a->sym == nodsafe->sym || a->width <= 0) {
		taint(a->offset, BIG);
		return;
	}
	taint(a->offset, a->offset + a->width);
	for(i=0; i<nvars; i++)
		if(vars[i].sym == a->sym)
			vars[i].bad = 1;
}

static void
rewrite(Adr *a, int as)
{
	Var *v;
	int size, cls;

	if(a->type != D_OREG || !isframe(a))
		return;
	size = opsize(as, &cls);
	if(size == 0)
		return;
	v = var(a->offset, size, cls, a->sym);
	if(v == nil || v->bad)
		return;
	a->type = D_LOCAL;
	a->name = D_NONE;
	a->sym = S;
	a->offset = v->idx;
	switch(cls) {
	case 'V':
		a->reg = KI64;
		break;
	case 'F':
	case 'f':
		a->reg = KF64;
		break;
	default:
		a->reg = KI32;
		break;
	}
}

void
regopt(Prog *p0)
{
	Prog *p;
	int i, j, n;

	if(debug['R'] > 1)
		return;
	nvars = 0;
	toomany = 0;
	for(p = p0->link; p != P; p = p->link) {
		if(p->as == ATEXT)
			break;
		frameuse(p, &p->from);
		frameuse(p, &p->to);
	}
	if(toomany)
		return;
	for(p = p0->link; p != P; p = p->link) {
		if(p->as == ATEXT)
			break;
		address(&p->from);
		address(&p->to);
	}
	for(i=0; i<nvars; i++)
	for(j=i+1; j<nvars; j++)
		if(vars[i].off < vars[j].off + vars[j].size && vars[j].off < vars[i].off + vars[i].size) {
			vars[i].bad = 1;
			vars[j].bad = 1;
		}
	n = 0;
	for(i=0; i<nvars; i++)
		if(!vars[i].bad)
			vars[i].idx = n++;
	for(p = p0->link; p != P; p = p->link) {
		if(p->as == ATEXT)
			break;
		rewrite(&p->from, p->as);
		rewrite(&p->to, p->as);
	}
	for(p = p0->link; p != P; p = p->link) {
		if(p->as == ATEXT)
			break;
		if(p->as == ANOP)
			continue;
		if((p->from.type == D_OREG || p->from.type == D_CONST) && isframe(&p->from))
			return;
		if((p->to.type == D_OREG || p->to.type == D_CONST) && isframe(&p->to))
			return;
	}
	p0->to.offset = 0;
	for(p = p0->link; p != P; p = p->link)
		if(p->as == ARESULT) {
			p->to.offset = 0;
			break;
		}
}
