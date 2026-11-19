#include	"l.h"

enum
{
	I32	= 0x7f,
	I64	= 0x7e,
	F32	= 0x7d,
	F64	= 0x7c,
};

static	Buf*	b;
static	Sym*	cur;
static	int	nlab;
static	int	loop;
static	int	seg;
static	int	pclocal;
static	int	sj;
static	int	nvar;
static	int*	varloc;
static	int*	varkind;
static	int	tmplocal;
static	int	loc[NALLREG];
static	long	frame;
static	long	total;
static	int	result;
static	int	usesp;

static int
cls(int r)
{
	if(r < VREG)
		return 'R';
	if(r < FREG)
		return 'V';
	return 'F';
}

static void
op(int o)
{
	bput(b, o);
}

static void
op2(int o, uvlong a)
{
	bput(b, o);
	buleb(b, a);
}

static void
lget(int r)
{
	if(r < 0 || r >= NALLREG || loc[r] < 0) {
		diag("%s: register %d has no local", cur->name, r);
		return;
	}
	op2(WLOCALGET, loc[r]);
}

static void
lset(int r)
{
	if(r < 0 || r >= NALLREG || loc[r] < 0) {
		diag("%s: register %d has no local", cur->name, r);
		return;
	}
	op2(WLOCALSET, loc[r]);
}

static void
i32c(vlong v)
{
	op(WI32CONST);
	bsleb(b, (int)v);
}

static void
i64c(vlong v)
{
	op(WI64CONST);
	bsleb(b, v);
}

static void
f64c(vlong bits)
{
	int i;

	op(WF64CONST);
	for(i=0; i<8; i++)
		bput(b, bits>>(8*i));
}

static void
rfield(int t, Sym *s, long idx, long add, vlong v)
{
	addrel(t, b->n, s, S, idx, add);
	bpad(b, v);
}

static void
raddr(Sym *s, long off)
{
	op(WI32CONST);
	if(isfunc(s)) {
		if(off != 0)
			diag("%s: offset from function %s", cur->name, s->name);
		rfield(RTABSLEB, s, 0, 0, 0);
	} else
		rfield(RMEMSLEB, s, 0, off, s->value + off);
}

static void
gsp(int o)
{
	op(o);
	if(rflag)
		rfield(RGLOBAL, lookup("__stack_pointer", 0), 0, 0, 0);
	else
		buleb(b, 0);
}

static void
gtag(int o)
{
	op(o);
	if(rflag)
		rfield(RTAG, lookup("__c_longjmp", 0), 0, 0, 0);
	else
		buleb(b, 0);
}

static void
dtobits(double d, vlong *v)
{
	memmove(v, &d, 8);
}

static long
spoff(Adr *a)
{
	return frame + a->offset;
}

static void
addrof(Adr *a)
{
	long v;

	if(a->sym != S && (a->name == D_EXTERN || a->name == D_STATIC)) {
		if(rflag) {
			raddr(a->sym, a->offset);
			return;
		}
		i32c(symaddr(a->sym) + a->offset);
		return;
	}
	if(a->name == D_AUTO || a->name == D_PARAM) {
		lget(REGSP);
		v = spoff(a);
		if(v != 0) {
			i32c(v);
			op(WI32ADD);
		}
		return;
	}
	if(a->reg != NOREG) {
		lget(a->reg);
		if(a->offset != 0) {
			i32c(a->offset);
			op(WI32ADD);
		}
		return;
	}
	i32c(a->offset);
}

static void
opnd(Adr *a, int c)
{
	vlong v;

	switch(a->type) {
	default:
		diag("%s: bad operand type %d", cur->name, a->type);
		return;
	case D_REG:
		lget(a->reg);
		if(cls(a->reg) == c)
			return;
		if(cls(a->reg) == 'R' && c == 'V') {
			op(WI64EXTENDU);
			return;
		}
		if(cls(a->reg) == 'V' && c == 'R') {
			op(WI32WRAP);
			return;
		}
		diag("%s: register class %c for %c", cur->name, cls(a->reg), c);
		return;
	case D_WPARAM:
		op2(WLOCALGET, a->offset);
		return;
	case D_LOCAL:
		op2(WLOCALGET, varloc[a->offset]);
		switch(varkind[a->offset]) {
		case KI64:
			if(c == 'R')
				op(WI32WRAP);
			break;
		default:
			if(c == 'V')
				op(WI64EXTENDU);
			break;
		}
		return;
	case D_CONST:
		if(a->sym != S || a->name != D_NONE || a->reg != NOREG) {
			addrof(a);
			if(c == 'V')
				op(WI64EXTENDU);
			return;
		}
	case D_VCONST:
		switch(c) {
		case 'R':
			i32c(a->offset);
			return;
		case 'V':
			i64c(a->offset);
			return;
		}
		dtobits((double)a->offset, &v);
		f64c(v);
		return;
	case D_FCONST:
		if(c != 'F') {
			diag("%s: float constant for %c", cur->name, c);
			return;
		}
		f64c(a->offset);
		return;
	}
}

static long
maddr(Adr *a)
{
	long v;

	if(a->type != D_OREG) {
		diag("%s: not memory: type %d", cur->name, a->type);
		i32c(0);
		return 0;
	}
	switch(a->name) {
	case D_EXTERN:
	case D_STATIC:
		if(rflag) {
			raddr(a->sym, a->offset);
			return 0;
		}
		i32c(0);
		return symaddr(a->sym) + a->offset;
	case D_AUTO:
	case D_PARAM:
		lget(REGSP);
		v = spoff(a);
		if(v < 0) {
			i32c(v);
			op(WI32ADD);
			return 0;
		}
		return v;
	}
	lget(a->reg);
	if(a->offset < 0) {
		i32c(a->offset);
		op(WI32ADD);
		return 0;
	}
	return a->offset;
}

static void
memop(int o, int al, long off)
{
	op(o);
	buleb(b, al);
	buleb(b, (ulong)off);
}

static int
vcls(int as)
{
	switch(as) {
	case AMOVV:
		return 'V';
	case AMOVF:
	case AMOVD:
		return 'F';
	}
	return 'R';
}

static void
mov(Prog *p)
{
	long off;
	int c;

	c = vcls(p->as);
	if(p->to.type == D_LOCAL) {
		if(p->as == AMOVF && p->from.type == D_WPARAM) {
			op2(WLOCALGET, p->from.offset);
			op(WF64PROMOTE);
		} else {
			opnd(&p->from, c);
			if(p->as == AMOVF) {
				op(WF32DEMOTE);
				op(WF64PROMOTE);
			}
		}
		op2(WLOCALSET, varloc[p->to.offset]);
		return;
	}
	if(p->from.type == D_LOCAL) {
		op2(WLOCALGET, varloc[p->from.offset]);
		switch(p->as) {
		case AMOVB:
			op(WI32EXTEND8);
			break;
		case AMOVBU:
			i32c(0xff);
			op(WI32AND);
			break;
		case AMOVH:
			op(WI32EXTEND16);
			break;
		case AMOVHU:
			i32c(0xffff);
			op(WI32AND);
			break;
		}
		lset(p->to.reg);
		return;
	}
	if(p->to.type == D_OREG) {
		off = maddr(&p->to);
		if(p->as == AMOVF && p->from.type == D_WPARAM)
			op2(WLOCALGET, p->from.offset);
		else {
			opnd(&p->from, c);
			if(p->as == AMOVF)
				op(WF32DEMOTE);
		}
		switch(p->as) {
		case AMOVB:
		case AMOVBU:
			memop(WI32STORE8, 0, off);
			break;
		case AMOVH:
		case AMOVHU:
			memop(WI32STORE16, 1, off);
			break;
		case AMOVW:
			memop(WI32STORE, 2, off);
			break;
		case AMOVV:
			memop(WI64STORE, 3, off);
			break;
		case AMOVF:
			memop(WF32STORE, 2, off);
			break;
		case AMOVD:
			memop(WF64STORE, 3, off);
			break;
		}
		return;
	}
	if(p->to.type != D_REG) {
		diag("%s: bad move destination", cur->name);
		return;
	}
	if(p->from.type == D_OREG) {
		off = maddr(&p->from);
		switch(p->as) {
		case AMOVB:
			memop(WI32LOAD8S, 0, off);
			break;
		case AMOVBU:
			memop(WI32LOAD8U, 0, off);
			break;
		case AMOVH:
			memop(WI32LOAD16S, 1, off);
			break;
		case AMOVHU:
			memop(WI32LOAD16U, 1, off);
			break;
		case AMOVW:
			memop(WI32LOAD, 2, off);
			break;
		case AMOVV:
			memop(WI64LOAD, 3, off);
			break;
		case AMOVF:
			memop(WF32LOAD, 2, off);
			op(WF64PROMOTE);
			break;
		case AMOVD:
			memop(WF64LOAD, 3, off);
			break;
		}
		lset(p->to.reg);
		return;
	}
	opnd(&p->from, cls(p->to.reg));
	switch(p->as) {
	case AMOVB:
		op(WI32EXTEND8);
		break;
	case AMOVBU:
		i32c(0xff);
		op(WI32AND);
		break;
	case AMOVH:
		op(WI32EXTEND16);
		break;
	case AMOVHU:
		i32c(0xffff);
		op(WI32AND);
		break;
	}
	lset(p->to.reg);
}

static void
conv(Prog *p)
{
	static struct { uchar as, fc; int o; int o2; } tab[] = {
		AMOVWV,	'R',	0xac,	-1,
		AMOVWUV,	'R',	0xad,	-1,
		AMOVVW,	'V',	0xa7,	-1,
		AMOVWD,	'R',	0xb7,	-1,
		AMOVWUD,	'R',	0xb8,	-1,
		AMOVVD,	'V',	0xb9,	-1,
		AMOVVUD,	'V',	0xba,	-1,
		AMOVVF,	'V',	0xb4,	0xbb,
		AMOVVUF,	'V',	0xb5,	0xbb,
		AMOVDW,	'F',	0xfc02,	-1,
		AMOVDWU,	'F',	0xfc03,	-1,
		AMOVDV,	'F',	0xfc06,	-1,
		AMOVDVU,	'F',	0xfc07,	-1,
		AMOVDF,	'F',	0xb6,	0xbb,
	};
	int i;

	for(i=0; i<nelem(tab); i++)
		if(tab[i].as == p->as)
			break;
	if(i == nelem(tab)) {
		diag("%s: bad conversion %d", cur->name, p->as);
		return;
	}
	opnd(&p->from, tab[i].fc);
	if(tab[i].o > 0xff) {
		op(WPREFIX);
		buleb(b, tab[i].o & 0xff);
	} else
		op(tab[i].o);
	if(tab[i].o2 >= 0)
		op(tab[i].o2);
	lset(p->to.reg);
}

static int
arithop(int as, int c)
{
	static int r[] = { 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76 };
	static int v[] = { 0x7c, 0x7d, 0x7e, 0x7f, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88 };
	int i;

	i = as - AADD;
	switch(c) {
	case 'R':
		return r[i];
	case 'V':
		return v[i];
	}
	switch(as) {
	case AADD:
		return 0xa0;
	case ASUB:
		return 0xa1;
	case AMUL:
		return 0xa2;
	case ADIV:
		return 0xa3;
	}
	diag("%s: bad float op %s", cur->name, anames[as]);
	return 0xa0;
}

static int
relop(int rel, int c)
{
	static int r[] = { 0x46, 0x47, 0x48, 0x4c, 0x4a, 0x4e, 0x49, 0x4d, 0x4b, 0x4f };
	static int v[] = { 0x51, 0x52, 0x53, 0x57, 0x55, 0x59, 0x54, 0x58, 0x56, 0x5a };
	static int f[] = { 0x61, 0x62, 0x63, 0x65, 0x64, 0x66, 0x63, 0x65, 0x64, 0x66 };

	switch(c) {
	case 'R':
		return r[rel];
	case 'V':
		return v[rel];
	}
	return f[rel];
}

static void
arith(Prog *p)
{
	int c;
	Adr ra;

	c = cls(p->to.reg);
	if(p->reg != NOREG) {
		memset(&ra, 0, sizeof ra);
		ra.type = D_REG;
		ra.reg = p->reg;
		opnd(&ra, c);
	} else
		opnd(&p->to, c);
	opnd(&p->from, c);
	op(arithop(p->as, c));
	lset(p->to.reg);
}

static void
compare(Prog *p, int rel)
{
	int c;
	Adr ra;

	if(p->reg == NOREG) {
		diag("%s: compare without register", cur->name);
		return;
	}
	c = cls(p->reg);
	memset(&ra, 0, sizeof ra);
	ra.type = D_REG;
	ra.reg = p->reg;
	opnd(&ra, c);
	opnd(&p->from, c);
	op(relop(rel, c));
}

static void
brto(Prog *t, int cond)
{
	int k;

	if(t == P || t->lab < 0) {
		diag("%s: branch to nowhere", cur->name);
		return;
	}
	k = t->lab;
	if(k >= seg) {
		op2(cond ? WBRIF : WBR, k - seg);
		return;
	}
	if(cond) {
		op(WIF);
		op(WVOID);
	}
	i32c(k+1);
	op2(WLOCALSET, pclocal);
	op2(WBR, nlab - seg + (cond ? 1 : 0) + sj);
	if(cond)
		op(WEND);
}

static void
branch(Prog *p, int cond)
{
	brto(p->cond, cond);
}

static void
gcase(Prog *p)
{
	int i;

	for(i=0; i<p->ntab; i++) {
		if(p->tab[i] == p->cond)
			continue;
		opnd(&p->from, 'R');
		i32c(i);
		op(WI32EQ);
		brto(p->tab[i], 1);
	}
	brto(p->cond, 0);
}

static void
epilogue(void)
{
	if(usesp && total > 0) {
		lget(REGSP);
		i32c(total);
		op(WI32ADD);
		gsp(WGLOBALSET);
	}
	switch(result) {
	case KI32:
		lget(REGRET);
		break;
	case KI64:
		lget(VREGRET);
		break;
	case KF32:
		lget(FREGRET);
		op(WF32DEMOTE);
		break;
	case KF64:
		lget(FREGRET);
		break;
	}
	op(WRETURN);
}

int
issetjmp(Prog *p)
{
	return p->as == ACALL && p->to.sym != S && p->to.name == D_EXTERN && strcmp(p->to.sym->name, "setjmp") == 0;
}

static void
gsetjmp(Prog *p)
{
	op2(WLOCALSET, tmplocal);
	op2(WLOCALGET, tmplocal);
	lget(REGSP);
	memop(WI32STORE, 2, 0);
	op2(WLOCALGET, tmplocal);
	i32c(p->link->lab + 1);
	memop(WI32STORE, 2, 4);
	i32c(0);
	lset(REGRET);
}

static void
catchjmp(void)
{
	gtag(WCATCH);
	op2(WLOCALSET, tmplocal);
	op2(WLOCALGET, tmplocal);
	memop(WI32LOAD, 2, 0);
	memop(WI32LOAD, 2, 0);
	lget(REGSP);
	op(WI32NE);
	op(WIF);
	op(WVOID);
	op2(WRETHROW, 1);
	op(WEND);
	lget(REGSP);
	gsp(WGLOBALSET);
	op2(WLOCALGET, tmplocal);
	memop(WI32LOAD, 2, 4);
	lset(REGRET);
	op2(WLOCALGET, tmplocal);
	memop(WI32LOAD, 2, 0);
	memop(WI32LOAD, 2, 4);
	op2(WLOCALSET, pclocal);
	op2(WBR, 1);
	op(WEND);
}

static void
call(Prog *p)
{
	Sym *s;
	int k;

	s = p->to.sym;
	if(s != S) {
		if(s->type == SWASM) {
			if(strcmp(s->name, "wasm·memory_size") == 0) {
				op(WMEMSIZE);
				op(0);
			} else if(strcmp(s->name, "wasm·memory_grow") == 0) {
				op(WMEMGROW);
				op(0);
			} else if(strcmp(s->name, "wasm·unreachable") == 0)
				op(WUNREACHABLE);
			else if(strcmp(s->name, "wasm·memory_copy") == 0) {
				op(WPREFIX);
				buleb(b, 10);
				op(0);
				op(0);
			} else if(strcmp(s->name, "wasm·memory_fill") == 0) {
				op(WPREFIX);
				buleb(b, 11);
				op(0);
			} else if(strcmp(s->name, "wasm·throw") == 0)
				gtag(WTHROW);
			else
				diag("%s: unknown wasm operator %s", cur->name, s->name);
		} else {
			if(s->fidx < 0) {
				diag("%s: call of %s, not a function", cur->name, s->name);
				return;
			}
			if(s->sig != p->sig) {
				if(strcmp(s->name, "main") == 0 && sigs[s->sig].np == 0 && sigs[s->sig].r == sigs[p->sig].r) {
					for(k = 0; k < sigs[p->sig].np; k++)
						op(WDROP);
				} else
					diag("%s: call of %s: signature mismatch", cur->name, s->name);
			}
			if(rflag) {
				op(WCALL);
				rfield(RFUNC, s, 0, 0, s->fidx);
			} else
				op2(WCALL, s->fidx);
		}
	} else {
		lget(p->to.reg);
		if(rflag) {
			op(WCALLINDIRECT);
			rfield(RTYPE, S, p->sig, 0, p->sig);
		} else
			op2(WCALLINDIRECT, p->sig);
		op(WUNREACHABLE);
	}
	switch(p->from.offset) {
	case KI32:
		lset(REGRET);
		break;
	case KI64:
		lset(VREGRET);
		break;
	case KF32:
		op(WF64PROMOTE);
		lset(FREGRET);
		break;
	case KF64:
		lset(FREGRET);
		break;
	}
}

static void
use(int r)
{
	if(r >= 0 && r < NALLREG && r != NOREG)
		loc[r] = 0;
}

static void
usea(Adr *a)
{
	switch(a->type) {
	case D_REG:
		use(a->reg);
		break;
	case D_OREG:
	case D_CONST:
		if(a->name == D_AUTO || a->name == D_PARAM)
			use(REGSP);
		else if(a->name == D_NONE && a->sym == S)
			use(a->reg);
		break;
	}
}

static void
locals(Sig *sg)
{
	int i, n, t, ng;
	uchar *ty;
	Prog *p;

	for(i=0; i<NALLREG; i++)
		loc[i] = -1;
	nvar = 0;
	for(p = cur->text; p != P; p = p->link) {
		if(p->from.type == D_LOCAL && p->from.offset >= nvar)
			nvar = p->from.offset+1;
		if(p->to.type == D_LOCAL && p->to.offset >= nvar)
			nvar = p->to.offset+1;
	}
	varloc = realloc(varloc, (nvar+1)*sizeof(int));
	varkind = realloc(varkind, (nvar+1)*sizeof(int));
	for(i=0; i<nvar; i++)
		varkind[i] = KI32;
	for(p = cur->text; p != P; p = p->link) {
		if(p->from.type == D_LOCAL)
			varkind[p->from.offset] = p->from.reg;
		if(p->to.type == D_LOCAL)
			varkind[p->to.offset] = p->to.reg;
	}
	ty = malloc(NALLREG + nvar + 4);
	use(REGRET);
	use(VREGRET);
	use(FREGRET);
	for(p = cur->text; p != P; p = p->link) {
		usea(&p->from);
		usea(&p->to);
		if(p->reg != NOREG && p->as != ATEXT)
			use(p->reg);
	}
	if(total > 0 || sj)
		use(REGSP);
	n = sg->np;
	ng = 0;
	pclocal = -1;
	if(loop) {
		pclocal = n++;
		ty[ng++] = I32;
	}
	tmplocal = -1;
	if(sj) {
		tmplocal = n++;
		ty[ng++] = I32;
	}
	for(i=0; i<NALLREG; i++) {
		if(loc[i] < 0)
			continue;
		loc[i] = n++;
		switch(cls(i)) {
		case 'R':
			t = I32;
			break;
		case 'V':
			t = I64;
			break;
		default:
			t = F64;
			break;
		}
		ty[ng++] = t;
	}
	for(i=0; i<nvar; i++) {
		varloc[i] = n++;
		switch(varkind[i]) {
		case KI64:
			ty[ng++] = I64;
			break;
		case KF64:
			ty[ng++] = F64;
			break;
		default:
			ty[ng++] = I32;
			break;
		}
	}
	usesp = loc[REGSP] >= 0;
	buleb(b, ng);
	for(i=0; i<ng; i++) {
		buleb(b, 1);
		bput(b, ty[i]);
	}
	free(ty);
}

static void	ins(Prog*);
static void	prologue(void);

typedef struct Blk Blk;
struct Blk
{
	Prog*	first;
	Prog*	last;
	int*	succ;
	int	nsucc;
	int	s2[2];
	int	rpo;
	int	idom;
	int	nfwd;
	int	header;
	int	done;
};

enum
{
	CLOOP,
	CBLOCK,
	CIF,
};

static	Blk*	blk;
static	int*	spool;
static	int	nblk;
static	int*	order;
static	int	norder;
static	int*	ctx;
static	int*	ctxb;
static	int	nctx;

static int
isterm(Prog *p)
{
	switch(p->as) {
	case AJMP:
	case ARET:
	case AUNDEF:
	case ACASE:
	case ABEQ:
	case ABNE:
	case ABLT:
	case ABLE:
	case ABGT:
	case ABGE:
	case ABLO:
	case ABLS:
	case ABHI:
	case ABHS:
		return 1;
	}
	return 0;
}

static int
iscond(Prog *p)
{
	return p->as >= ABEQ && p->as <= ABHS;
}

static void
dfs(int b)
{
	int i;

	blk[b].done = 1;
	for(i=0; i<blk[b].nsucc; i++)
		if(!blk[blk[b].succ[i]].done)
			dfs(blk[b].succ[i]);
	order[norder++] = b;
}

static int
hassucc(int b, int n)
{
	int i;

	for(i=0; i<blk[b].nsucc; i++)
		if(blk[b].succ[i] == n)
			return 1;
	return 0;
}

static void
addsucc(int b, int n)
{
	if(hassucc(b, n))
		return;
	blk[b].succ[blk[b].nsucc++] = n;
}

static int
intersect(int a, int c)
{
	while(a != c) {
		while(blk[a].rpo > blk[c].rpo)
			a = blk[a].idom;
		while(blk[c].rpo > blk[a].rpo)
			c = blk[c].idom;
	}
	return a;
}

static int
dominates(int a, int c)
{
	while(c != a && blk[c].idom != c)
		c = blk[c].idom;
	return c == a;
}

static int
mkblocks(Sym *s)
{
	Prog *p, *q;
	int i, j, b, n, changed, d;

	nblk = 0;
	q = P;
	for(p = s->text; p != P; p = p->link) {
		if(p == s->text || (p->mark & LABEL) || (q != P && isterm(q)))
			nblk++;
		p->blk = nblk-1;
		q = p;
	}
	blk = realloc(blk, nblk*sizeof(Blk));
	memset(blk, 0, nblk*sizeof(Blk));
	for(p = s->text; p != P; p = p->link) {
		if(blk[p->blk].first == P)
			blk[p->blk].first = p;
		blk[p->blk].last = p;
	}
	n = 0;
	for(b=0; b<nblk; b++)
		if(blk[b].last->as == ACASE)
			n += blk[b].last->ntab + 1;
	spool = realloc(spool, (n+1)*sizeof(int));
	n = 0;
	for(b=0; b<nblk; b++) {
		p = blk[b].last;
		blk[b].succ = blk[b].s2;
		blk[b].nsucc = 0;
		blk[b].rpo = -1;
		blk[b].idom = -1;
		if(p->as == ACASE) {
			blk[b].succ = spool + n;
			n += p->ntab + 1;
			for(i=0; i<p->ntab; i++) {
				if(p->tab[i] == P)
					return 0;
				addsucc(b, p->tab[i]->blk);
			}
		}
		if(p->as == AJMP || iscond(p) || p->as == ACASE) {
			if(p->cond == P)
				return 0;
			addsucc(b, p->cond->blk);
		}
		if(p->as != AJMP && p->as != ARET && p->as != AUNDEF && p->as != ACASE && p->link != P)
			blk[b].succ[blk[b].nsucc++] = p->link->blk;
	}
	order = realloc(order, nblk*sizeof(int));
	norder = 0;
	dfs(0);
	for(i=0; i<norder; i++)
		blk[order[i]].rpo = norder-1-i;
	for(i=0; i<norder/2; i++) {
		j = order[i];
		order[i] = order[norder-1-i];
		order[norder-1-i] = j;
	}
	blk[0].idom = 0;
	do {
		changed = 0;
		for(i=1; i<norder; i++) {
			b = order[i];
			d = -1;
			for(j=0; j<nblk; j++) {
				if(blk[j].rpo < 0 || blk[j].idom < 0)
					continue;
				if(!hassucc(j, b))
					continue;
				d = d < 0 ? j : intersect(j, d);
			}
			if(d != blk[b].idom) {
				blk[b].idom = d;
				changed = 1;
			}
		}
	} while(changed);
	for(i=0; i<norder; i++) {
		b = order[i];
		for(j=0; j<blk[b].nsucc; j++) {
			n = blk[b].succ[j];
			if(j == 1 && blk[b].succ[0] == n)
				continue;
			if(blk[n].rpo > blk[b].rpo)
				blk[n].nfwd++;
			else {
				if(!dominates(n, b))
					return 0;
				blk[n].header = 1;
			}
		}
	}
	for(i=0; i<norder; i++) {
		b = order[i];
		if(blk[b].last->as != ACASE)
			continue;
		for(j=0; j<blk[b].nsucc; j++) {
			n = blk[b].succ[j];
			if(blk[n].rpo > blk[b].rpo && blk[n].nfwd < 2)
				blk[n].nfwd = 2;
		}
	}
	ctx = realloc(ctx, (2*nblk+4)*sizeof(int));
	ctxb = realloc(ctxb, (2*nblk+4)*sizeof(int));
	nctx = 0;
	return 1;
}

static void	dotree(int);

static int
depth(int kind, int b)
{
	int i;

	for(i=nctx-1; i>=0; i--)
		if(ctx[i] == kind && ctxb[i] == b)
			return nctx-1-i;
	diag("%s: no branch target", cur->name);
	return 0;
}

static void
push(int kind, int b)
{
	ctx[nctx] = kind;
	ctxb[nctx] = b;
	nctx++;
}

static void
dobranch(int x, int y)
{
	if(blk[y].rpo <= blk[x].rpo)
		op2(WBR, depth(CLOOP, y));
	else if(blk[y].nfwd > 1)
		op2(WBR, depth(CBLOCK, y));
	else
		dotree(y);
}

static int
target(int x, int y)
{
	if(blk[y].rpo <= blk[x].rpo)
		return depth(CLOOP, y);
	return depth(CBLOCK, y);
}

static void
emitblk(int x)
{
	Prog *p, *l;
	int i;

	l = blk[x].last;
	for(p = blk[x].first; p != l; p = p->link)
		ins(p);
	if(l->as == ACASE) {
		opnd(&l->from, 'R');
		op(WBRTABLE);
		buleb(b, l->ntab);
		for(i=0; i<l->ntab; i++)
			buleb(b, target(x, l->tab[i]->blk));
		buleb(b, target(x, l->cond->blk));
		return;
	}
	if(iscond(l) && blk[x].nsucc > 1 && blk[x].succ[0] == blk[x].succ[1]) {
		compare(l, l->as - ABEQ);
		op(WDROP);
		dobranch(x, blk[x].succ[0]);
		return;
	}
	if(iscond(l)) {
		compare(l, l->as - ABEQ);
		op(WIF);
		op(WVOID);
		push(CIF, -1);
		dobranch(x, blk[x].succ[0]);
		op(WELSE);
		if(blk[x].nsucc > 1)
			dobranch(x, blk[x].succ[1]);
		nctx--;
		op(WEND);
		return;
	}
	if(l->as == AJMP) {
		dobranch(x, blk[x].succ[0]);
		return;
	}
	ins(l);
	if(l->as == ARET || l->as == AUNDEF)
		return;
	if(blk[x].nsucc > 0)
		dobranch(x, blk[x].succ[0]);
	else
		op(WUNREACHABLE);
}

static void
within(int x, int *ys, int n)
{
	if(n == 0) {
		emitblk(x);
		return;
	}
	op(WBLOCK);
	op(WVOID);
	push(CBLOCK, ys[0]);
	within(x, ys+1, n-1);
	nctx--;
	op(WEND);
	dotree(ys[0]);
}

static void
dotree(int x)
{
	int *ys, n, i, j, t;

	n = 0;
	ys = malloc((norder+1)*sizeof(int));
	for(i=0; i<norder; i++) {
		j = order[i];
		if(j != x && blk[j].idom == x && blk[j].nfwd > 1)
			ys[n++] = j;
	}
	for(i=0; i<n; i++)
	for(j=i+1; j<n; j++)
		if(blk[ys[j]].rpo > blk[ys[i]].rpo) {
			t = ys[i];
			ys[i] = ys[j];
			ys[j] = t;
		}
	if(blk[x].header) {
		op(WLOOP);
		op(WVOID);
		push(CLOOP, x);
		within(x, ys, n);
		nctx--;
		op(WEND);
	} else
		within(x, ys, n);
	free(ys);
}

static void
prologue(void)
{
	if(usesp) {
		gsp(WGLOBALGET);
		if(total > 0) {
			i32c(total);
			op(WI32SUB);
			op2(WLOCALTEE, loc[REGSP]);
			gsp(WGLOBALSET);
		} else
			lset(REGSP);
	}
}

static void
ins(Prog *p)
{
	switch(p->as) {
	default:
		diag("%s: unknown instruction %s", cur->name, p->as < ALAST ? anames[p->as] : "?");
		break;
	case ATEXT:
	case APARAM:
	case ARESULT:
	case ANOP:
		break;
	case AADD:
	case ASUB:
	case AMUL:
	case ADIV:
	case ADIVU:
	case AMOD:
	case AMODU:
	case AAND:
	case AOR:
	case AXOR:
	case ASLL:
	case ASRA:
	case ASRL:
		arith(p);
		break;
	case AMOVB:
	case AMOVBU:
	case AMOVH:
	case AMOVHU:
	case AMOVW:
	case AMOVV:
	case AMOVF:
	case AMOVD:
		mov(p);
		break;
	case AMOVWV:
	case AMOVWUV:
	case AMOVVW:
	case AMOVWD:
	case AMOVWUD:
	case AMOVVD:
	case AMOVVUD:
	case AMOVVF:
	case AMOVVUF:
	case AMOVDW:
	case AMOVDWU:
	case AMOVDV:
	case AMOVDVU:
	case AMOVDF:
		conv(p);
		break;
	case ACMPEQ:
	case ACMPNE:
	case ACMPLT:
	case ACMPLE:
	case ACMPGT:
	case ACMPGE:
	case ACMPLO:
	case ACMPLS:
	case ACMPHI:
	case ACMPHS:
		compare(p, p->as - ACMPEQ);
		lset(p->to.reg);
		break;
	case ABEQ:
	case ABNE:
	case ABLT:
	case ABLE:
	case ABGT:
	case ABGE:
	case ABLO:
	case ABLS:
	case ABHI:
	case ABHS:
		compare(p, p->as - ABEQ);
		branch(p, 1);
		break;
	case AJMP:
		branch(p, 0);
		break;
	case ACASE:
		gcase(p);
		break;
	case ACALL:
		if(issetjmp(p))
			gsetjmp(p);
		else
			call(p);
		break;
	case ARET:
		epilogue();
		break;
	case ANEG:
		opnd(&p->from, 'F');
		op(WF64NEG);
		lset(p->to.reg);
		break;
	case AARG:
		opnd(&p->from, cls(p->from.reg));
		break;
	case AARGF:
		opnd(&p->from, 'F');
		op(WF32DEMOTE);
		break;
	case ACOPY:
		lget(p->to.reg);
		lget(p->reg);
		i32c(p->from.offset);
		op(WPREFIX);
		buleb(b, 10);
		op(0);
		op(0);
		break;
	case AUNDEF:
		op(WUNREACHABLE);
		break;
	}

}

void
gencode(Sym *s, Buf *out)
{
	Prog *p;
	Sig sg;
	int i;

	b = out;
	cur = s;
	funcsig(s, &sg);
	result = sg.r;
	frame = (s->frame + 7) & ~7;
	total = (frame + s->pframe + 15) & ~15;
	nlab = 0;
	loop = 0;
	sj = 0;
	for(p = s->text; p != P; p = p->link) {
		if(p->cond != P && p->to.type == D_BRANCH)
			p->cond->mark |= LABEL;
		for(i=0; i<p->ntab; i++)
			if(p->tab[i] != P)
				p->tab[i]->mark |= LABEL;
		if(issetjmp(p) && p->link != P) {
			p->link->mark |= LABEL;
			sj = 1;
		}
	}
	if(sj) {
		loop = 1;
		if(total == 0)
			total = 16;
	}
	if(!sj && !debug['D'] && mkblocks(s)) {
		locals(&sg);
		prologue();
		dotree(0);
		if(result != KNONE)
			op(WUNREACHABLE);
		op(WEND);
		return;
	}
	for(p = s->text; p != P; p = p->link)
		if(p->mark & LABEL)
			p->lab = nlab++;
	seg = 0;
	for(p = s->text; p != P; p = p->link) {
		if(p->lab >= 0)
			seg = p->lab + 1;
		if(p->cond != P && p->to.type == D_BRANCH && p->cond->lab < seg)
			loop = 1;
		for(i=0; i<p->ntab; i++)
			if(p->tab[i] != P && p->tab[i]->lab < seg)
				loop = 1;
	}
	locals(&sg);
	prologue();
	if(loop)
		op(WLOOP), op(WVOID);
	if(sj)
		op(WTRY), op(WVOID);
	for(i=0; i<nlab; i++)
		op(WBLOCK), op(WVOID);
	if(loop) {
		op(WBLOCK);
		op(WVOID);
		op2(WLOCALGET, pclocal);
		op(WBRTABLE);
		buleb(b, nlab+1);
		buleb(b, 0);
		for(i=0; i<nlab; i++)
			buleb(b, i+1);
		buleb(b, 0);
		op(WEND);
	}
	seg = 0;
	for(p = s->text; p != P; p = p->link) {
		if(p->lab >= 0) {
			op(WEND);
			seg = p->lab + 1;
		}
		ins(p);
	}
	if(sj)
		catchjmp();
	if(loop)
		op(WEND);
	if(result != KNONE)
		op(WUNREACHABLE);
	op(WEND);
}
