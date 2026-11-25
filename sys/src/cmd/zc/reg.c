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
	case AVMOV:
		*cls = 'X';
		return 16;
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
	case 'X':
		a->reg = KV128;
		break;
	default:
		a->reg = KI32;
		break;
	}
}

static void
regopt1(Prog *p0)
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

void
regopt(Prog *p0)
{
	regopt1(p0);
	nilopt(p0);
}

static	char	rchk[NREG];
static	short	rsrc[NREG];
static	char	lchk[NVARS];

static void
forget(void)
{
	int i;

	for(i=0; i<NREG; i++) {
		rchk[i] = 0;
		rsrc[i] = -1;
	}
	for(i=0; i<NVARS; i++)
		lchk[i] = 0;
}

static int
istarget(long pc, long *t, int nt)
{
	int lo, hi, m;

	lo = 0;
	hi = nt;
	while(lo < hi) {
		m = (lo+hi)/2;
		if(t[m] < pc)
			lo = m+1;
		else
			hi = m;
	}
	return lo < nt && t[lo] == pc;
}

static int
pccmp(void *a, void *b)
{
	long x, y;

	x = *(long*)a;
	y = *(long*)b;
	if(x < y)
		return -1;
	return x > y;
}

static int
isnamed(Adr *a)
{
	return a->type == D_CONST && a->sym != S && a->name != D_NONE;
}

static void
setreg(Prog *p)
{
	int r, k;

	r = p->to.reg;
	if(r < 0 || r >= NREG)
		return;
	rchk[r] = 0;
	rsrc[r] = -1;
	if(p->as != AMOVW)
		return;
	switch(p->from.type) {
	case D_REG:
		k = p->from.reg;
		if(k >= 0 && k < NREG && k != r) {
			rchk[r] = rchk[k];
			rsrc[r] = rsrc[k];
		}
		break;
	case D_LOCAL:
		k = p->from.offset;
		if(p->from.reg == KI32 && k >= 0 && k < NVARS) {
			rchk[r] = lchk[k];
			rsrc[r] = k;
		}
		break;
	case D_CONST:
		rchk[r] = isnamed(&p->from);
		break;
	}
}

static void
setlocal(Prog *p)
{
	int i, k, r;

	k = p->to.offset;
	if(k < 0 || k >= NVARS)
		return;
	lchk[k] = 0;
	for(i=0; i<NREG; i++)
		if(rsrc[i] == k)
			rsrc[i] = -1;
	if(p->as != AMOVW || p->to.reg != KI32)
		return;
	if(p->from.type == D_REG) {
		r = p->from.reg;
		if(r >= 0 && r < NREG) {
			lchk[k] = rchk[r];
			rsrc[r] = k;
		}
	} else if(isnamed(&p->from))
		lchk[k] = 1;
}

static	long*	brtab;
static	int	mbrtab;

void
nilopt(Prog *p0)
{
	Prog *p;
	long *t;
	int nt, r;
	vlong o;

	nt = 0;
	for(p = p0->link; p != P && p->as != ATEXT; p = p->link) {
		if(p->to.type != D_BRANCH)
			continue;
		if(nt >= mbrtab) {
			mbrtab = mbrtab ? 2*mbrtab : 1024;
			t = alloc(mbrtab*sizeof(long));
			if(nt > 0)
				memmove(t, brtab, nt*sizeof(long));
			brtab = t;
		}
		brtab[nt++] = p->to.offset;
	}
	t = brtab;
	qsort(t, nt, sizeof(long), pccmp);
	forget();
	for(p = p0->link; p != P && p->as != ATEXT; p = p->link) {
		if(istarget(p->pc, t, nt))
			forget();
		switch(p->as) {
		case ACHKRD:
		case ACHKWR:
			r = p->from.reg;
			if(r < 0 || r >= NREG)
				continue;
			if(rchk[r]) {
				p->as = ANOP;
				p->from = zprog.from;
				p->to = zprog.to;
				continue;
			}
			o = p->from.offset;
			if(o > -NILGUARD+1 && o < NILGUARD-1) {
				rchk[r] = 1;
				if(rsrc[r] >= 0)
					lchk[rsrc[r]] = 1;
			}
			continue;
		case ACALL:
			if(p->to.sym != S && strcmp(p->to.sym->name, "setjmp") == 0) {
				forget();
				continue;
			}
			rchk[REGRET] = 0;
			rsrc[REGRET] = -1;
			continue;
		case ACOPY:
			continue;
		}
		if(p->to.type == D_REG)
			setreg(p);
		else if(p->to.type == D_LOCAL)
			setlocal(p);
	}
}
