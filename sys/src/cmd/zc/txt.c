#include "gc.h"

void
ginit(void)
{
	int i;
	Type *t;

	thechar = 'z';
	thestring = "wasm";
	listinit();
	nstring = 0;
	mnstring = 0;
	nrathole = 0;
	pc = 0;
	breakpc = -1;
	continpc = -1;
	cases = C;
	firstp = P;
	lastp = P;
	tfield = types[TLONG];
	fieldsysv = 1;
	vlongreg = 1;

	typeword = typechlvp;
	typeswitch = typechlv;
	typecmplx = typesu;

	zprog.link = P;
	zprog.as = AGOK;
	zprog.reg = NOREG;
	zprog.from.type = D_NONE;
	zprog.from.name = D_NONE;
	zprog.from.reg = NOREG;
	zprog.to = zprog.from;

	regnode.op = OREGISTER;
	regnode.class = CEXREG;
	regnode.reg = REGTMP;
	regnode.complex = 0;
	regnode.addable = 11;
	regnode.type = types[TLONG];

	vregnode = regnode;
	vregnode.reg = VREGTMP;
	vregnode.type = types[TVLONG];

	fregnode = regnode;
	fregnode.reg = FREGTMP;
	fregnode.type = types[TDOUBLE];

	constnode.op = OCONST;
	constnode.class = CXXX;
	constnode.complex = 0;
	constnode.addable = 20;
	constnode.type = types[TLONG];

	vconstnode = constnode;
	vconstnode.type = types[TVLONG];

	fconstnode.op = OCONST;
	fconstnode.class = CXXX;
	fconstnode.complex = 0;
	fconstnode.addable = 20;
	fconstnode.type = types[TDOUBLE];

	nodsafe = new(ONAME, Z, Z);
	nodsafe->sym = slookup(".safe");
	nodsafe->type = types[TINT];
	nodsafe->etype = types[TINT]->etype;
	nodsafe->class = CAUTO;
	complex(nodsafe);

	t = typ(TARRAY, types[TCHAR]);
	symrathole = slookup(".rathole");
	symrathole->class = CGLOBL;
	symrathole->type = t;

	nodrat = new(ONAME, Z, Z);
	nodrat->sym = symrathole;
	nodrat->type = types[TIND];
	nodrat->etype = TVOID;
	nodrat->class = CGLOBL;
	complex(nodrat);
	nodrat->type = t;

	nodret = new(ONAME, Z, Z);
	nodret->sym = slookup(".ret");
	nodret->type = types[TIND];
	nodret->etype = TIND;
	nodret->class = CPARAM;
	nodret = new(OIND, nodret, Z);
	complex(nodret);

	for(i=0; i<nelem(reg); i++)
		reg[i] = 0;
}

void
gclean(void)
{
	int i;
	Sym *s;

	for(i=0; i<NALLREG; i++)
		if(reg[i])
			diag(Z, "reg %d left allocated", i);
	while(mnstring)
		outstring("", 1L);
	symstring->type->width = nstring;
	symrathole->type->width = nrathole;
	for(i=0; i<NHASH; i++)
	for(s = hash[i]; s != S; s = s->link) {
		if(s->type == T)
			continue;
		if(s->type->width == 0)
			continue;
		if(s->class != CGLOBL && s->class != CSTATIC)
			continue;
		if(s->type == types[TENUM])
			continue;
		gpseudo(AGLOBL, s, nodconst(s->type->width));
	}
	for(i=0; i<NHASH; i++)
	for(s = hash[i]; s != S; s = s->link)
		if(s->aused && (s->class == CEXTERN || s->class == CGLOBL) && s->type != T && s->type->etype == TFUNC)
			gdecl(s);
	nextpc();
	p->as = AEND;
	outcode();
}

void
nextpc(void)
{

	p = alloc(sizeof(*p));
	*p = zprog;
	p->lineno = nearln;
	pc++;
	if(firstp == P) {
		firstp = p;
		lastp = p;
		return;
	}
	lastp->link = p;
	lastp = p;
}

int
rclass(int r)
{
	if(r < VREG)
		return 'R';
	if(r < FREG)
		return 'V';
	return 'F';
}

int
tclass(Type *t)
{
	if(typev[t->etype])
		return 'V';
	if(typefd[t->etype])
		return 'F';
	return 'R';
}

Type*
sumember(Type *t, long *off)
{
	Type *l, *m;
	long o;

	o = 0;
	for(;;) {
		if(t->etype == TARRAY) {
			if(t->link == T || t->width != t->link->width)
				return T;
			t = t->link;
			continue;
		}
		if(t->etype != TSTRUCT && t->etype != TUNION)
			break;
		m = T;
		for(l = t->link; l != T; l = l->down) {
			if(l->nbits && l->sym == S)
				continue;
			if(m != T)
				return T;
			m = l;
		}
		if(m == T || m->width != t->width)
			return T;
		o += m->offset;
		t = m;
	}
	if(!typechlvp[t->etype] && !typefd[t->etype])
		return T;
	if(off != nil)
		*off = o;
	return t;
}

int
sutype(Type *t)
{
	t = sumember(t, nil);
	if(t == T)
		return -1;
	return t->etype;
}

int
kind(Type *t)
{
	int et;

	et = t->etype;
	if(typesu[et]) {
		et = sutype(t);
		if(et < 0)
			return KI32;
	}
	if(typev[et])
		return KI64;
	if(et == TFLOAT)
		return KF32;
	if(et == TDOUBLE)
		return KF64;
	if(et == TVOID)
		return KNONE;
	return KI32;
}

long
talign(Type *t)
{
	long a, m;
	Type *l;

	switch(t->etype) {
	case TCHAR:
	case TUCHAR:
		return 1;
	case TSHORT:
	case TUSHORT:
		return 2;
	case TVLONG:
	case TUVLONG:
	case TDOUBLE:
		return 8;
	case TARRAY:
		if(t->link == T)
			return 1;
		return talign(t->link);
	case TSTRUCT:
	case TUNION:
		m = 1;
		for(l = t->link; l != T; l = l->down) {
			if(l->nbits && l->sym == S)
				continue;
			a = talign(l);
			if(a > m)
				m = a;
		}
		return m;
	case TFUNC:
	case TVOID:
		return 1;
	}
	return 4;
}

Node*
nodgconst(vlong v, Type *t)
{
	if(!typev[t->etype])
		return nodconst((long)v);
	vconstnode.vconst = v;
	return &vconstnode;
}

Node*
nodconst(long v)
{
	constnode.vconst = convvtox(v, TLONG);
	return &constnode;
}

Node*
nod32const(vlong v)
{
	constnode.vconst = v & MASK(32);
	return &constnode;
}

Node*
nodfconst(double d)
{
	fconstnode.fconst = d;
	return &fconstnode;
}

void
nodreg(Node *n, Node *nn, int reg)
{
	*n = regnode;
	n->reg = reg;
	n->type = nn->type;
	n->lineno = nn->lineno;
}

void
regret(Node *n, Node *nn)
{
	int r;

	switch(tclass(nn->type)) {
	default:
		r = REGRET;
		break;
	case 'V':
		r = VREGRET;
		break;
	case 'F':
		r = FREGRET;
		break;
	}
	nodreg(n, nn, r);
	reg[r]++;
}

void
regalloc(Node *n, Node *tn, Node *o)
{
	int i, c, lo, hi;

	if(tn->type == T || typesu[tn->type->etype] || tn->type->etype == TVOID || tn->type->etype == TFUNC) {
		diag(tn, "unknown type in regalloc: %T", tn->type);
		nodreg(n, tn, REGMIN);
		reg[REGMIN]++;
		return;
	}
	c = tclass(tn->type);
	if(o != Z && o->op == OREGISTER && rclass(o->reg) == c) {
		i = o->reg;
		goto out;
	}
	switch(c) {
	default:
		lo = REGMIN;
		hi = REGMAX;
		break;
	case 'V':
		lo = VREGMIN;
		hi = VREGMAX;
		break;
	case 'F':
		lo = FREGMIN;
		hi = FREGMAX;
		break;
	}
	for(i=lo; i<=hi; i++)
		if(reg[i] == 0)
			goto out;
	diag(tn, "out of registers");
	i = lo;
out:
	reg[i]++;
	nodreg(n, tn, i);
}

void
regialloc(Node *n, Node *tn, Node *o)
{
	Node nod;

	nod = *tn;
	nod.type = types[TIND];
	regalloc(n, &nod, o);
}

void
regfree(Node *n)
{
	int i;

	i = 0;
	if(n->op != OREGISTER && n->op != OINDREG)
		goto err;
	i = n->reg;
	if(i < 0 || i >= NALLREG)
		goto err;
	if(reg[i] <= 0)
		goto err;
	reg[i]--;
	return;
err:
	diag(n, "error in regfree: %d", i);
}

void
regsalloc(Node *n, Node *nn)
{
	cursafe = align(cursafe + stkoff, nn->type, Aaut3) - stkoff;
	maxargsafe = maxround(maxargsafe, cursafe+curarg);
	*n = *nodsafe;
	n->xoffset = -(stkoff + cursafe);
	n->type = nn->type;
	n->etype = nn->type->etype;
	n->lineno = nn->lineno;
}

void
regind(Node *n, Node *nn)
{

	if(n->op != OREGISTER) {
		diag(n, "regind not OREGISTER");
		return;
	}
	n->op = OINDREG;
	n->type = nn->type;
}

void
naddr(Node *n, Adr *a)
{
	vlong v;

	a->type = D_NONE;
	if(n == Z)
		return;
	switch(n->op) {
	default:
	bad:
		diag(n, "bad in naddr: %O", n->op);
		break;

	case OREGISTER:
		a->type = D_REG;
		a->sym = S;
		a->reg = n->reg;
		break;

	case OIND:
		naddr(n->left, a);
		if(a->type == D_REG) {
			a->type = D_OREG;
			a->offset = 0;
			break;
		}
		if(a->type == D_CONST) {
			a->type = D_OREG;
			break;
		}
		goto bad;

	case OINDREG:
		a->type = D_OREG;
		a->sym = S;
		a->offset = n->xoffset;
		a->reg = n->reg;
		break;

	case ONAME:
		a->etype = n->etype;
		a->type = D_OREG;
		a->name = D_STATIC;
		a->sym = n->sym;
		a->offset = n->xoffset;
		if(n->class == CSTATIC)
			break;
		if(n->class == CEXTERN || n->class == CGLOBL) {
			a->name = D_EXTERN;
			break;
		}
		if(n->class == CAUTO) {
			a->name = D_AUTO;
			a->width = n->type != T ? n->type->width : 0;
			break;
		}
		if(n->class == CPARAM) {
			a->name = D_PARAM;
			break;
		}
		goto bad;

	case OCONST:
		a->sym = S;
		a->reg = NOREG;
		if(typefd[n->type->etype]) {
			a->type = D_FCONST;
			a->dval = n->fconst;
		} else {
			v = n->vconst;
			a->offset = v;
			if((vlong)(int)v != v)
				a->type = D_VCONST;
			else
				a->type = D_CONST;
		}
		break;

	case OADDR:
		naddr(n->left, a);
		if(a->type == D_OREG) {
			a->type = D_CONST;
			break;
		}
		goto bad;

	case OADD:
		if(n->left->op == OCONST) {
			naddr(n->left, a);
			v = a->offset;
			naddr(n->right, a);
		} else {
			naddr(n->right, a);
			v = a->offset;
			naddr(n->left, a);
		}
		a->offset += v;
		break;
	}
}

static int
ismem(Node *n)
{
	return n->op == ONAME || n->op == OINDREG || n->op == OIND;
}

static int
loadop(int et)
{
	switch(et) {
	case TCHAR:
		return AMOVB;
	case TUCHAR:
		return AMOVBU;
	case TSHORT:
		return AMOVH;
	case TUSHORT:
		return AMOVHU;
	case TVLONG:
	case TUVLONG:
		return AMOVV;
	case TFLOAT:
		return AMOVF;
	case TDOUBLE:
		return AMOVD;
	}
	return AMOVW;
}

static int
storeop(int et)
{
	switch(et) {
	case TCHAR:
	case TUCHAR:
		return AMOVB;
	case TSHORT:
	case TUSHORT:
		return AMOVH;
	}
	return loadop(et);
}

static int
copyop(int c)
{
	switch(c) {
	case 'V':
		return AMOVV;
	case 'F':
		return AMOVD;
	}
	return AMOVW;
}

static int
issigned(int et)
{
	switch(et) {
	case TCHAR:
	case TSHORT:
	case TINT:
	case TLONG:
	case TVLONG:
	case TENUM:
		return 1;
	}
	return 0;
}

static void
gconst(Node *f, Node *t, int mem)
{
	Node nod;
	int tt;
	vlong v;
	double d;

	tt = t->type->etype;
	nod = *f;
	nod.type = t->type;
	if(typefd[tt]) {
		d = f->fconst;
		if(!typefd[f->type->etype]) {
			if(typeu[f->type->etype])
				d = (uvlong)f->vconst;
			else
				d = f->vconst;
		}
		if(tt == TFLOAT)
			d = (float)d;
		nod.fconst = d;
	} else {
		if(!mem && extop(tt) != AXXX)
			tt = typeu[tt] ? TULONG : TLONG;
		if(typefd[f->type->etype])
			v = convftox(f->fconst, tt);
		else
			v = convvtox(f->vconst, tt);
		nod.vconst = v;
	}
	if(mem)
		gins(storeop(tt), &nod, t);
	else
		gins(copyop(tclass(t->type)), &nod, t);
}

int
extop(int et)
{
	switch(et) {
	case TCHAR:
		return AMOVB;
	case TUCHAR:
		return AMOVBU;
	case TSHORT:
		return AMOVH;
	case TUSHORT:
		return AMOVHU;
	}
	return AXXX;
}

void
gext(Node *n)
{
	int a;

	a = extop(n->type->etype);
	if(a != AXXX)
		gins(a, n, n);
}

static void
greg(Node *f, Node *t)
{
	int ft, tt, fc, tc, a;
	Node nod;

	ft = f->type->etype;
	tt = t->type->etype;
	fc = tclass(f->type);
	tc = tclass(t->type);
	if(fc == 'R' && extop(ft) != AXXX && !(tc == 'R' && tt == ft)) {
		if(tc == 'R')
			nod = *t;
		else
			regalloc(&nod, f, Z);
		gins(extop(ft), f, &nod);
		nod.type = types[issigned(ft) ? TLONG : TULONG];
		greg(&nod, t);
		if(tc != 'R')
			regfree(&nod);
		return;
	}
	a = AGOK;
	switch(fc) {
	case 'R':
		switch(tc) {
		case 'R':
			a = AMOVW;
			switch(tt) {
			case TCHAR:
				if(ft != TCHAR)
					a = AMOVB;
				break;
			case TUCHAR:
				if(ft != TUCHAR)
					a = AMOVBU;
				break;
			case TSHORT:
				if(ft != TCHAR && ft != TUCHAR && ft != TSHORT)
					a = AMOVH;
				break;
			case TUSHORT:
				if(ft != TUCHAR && ft != TUSHORT)
					a = AMOVHU;
				break;
			}
			break;
		case 'V':
			a = issigned(ft) ? AMOVWV : AMOVWUV;
			break;
		case 'F':
			a = issigned(ft) ? AMOVWD : AMOVWUD;
			gins(a, f, t);
			if(tt == TFLOAT)
				gins(AMOVDF, t, t);
			return;
		}
		break;
	case 'V':
		switch(tc) {
		case 'R':
			if(typechl[tt] && ewidth[tt] < SZ_LONG) {
				nodreg(&nod, t, t->reg);
				nod.type = types[TLONG];
				gins(AMOVVW, f, &nod);
				greg(&nod, t);
				return;
			}
			a = AMOVVW;
			break;
		case 'V':
			a = AMOVV;
			break;
		case 'F':
			if(tt == TFLOAT)
				a = issigned(ft) ? AMOVVF : AMOVVUF;
			else
				a = issigned(ft) ? AMOVVD : AMOVVUD;
			break;
		}
		break;
	case 'F':
		switch(tc) {
		case 'R':
			a = (typeu[tt] || tt == TIND) ? AMOVDWU : AMOVDW;
			if(ewidth[tt] < SZ_LONG) {
				nodreg(&nod, t, t->reg);
				nod.type = types[TLONG];
				gins(a, f, &nod);
				greg(&nod, t);
				return;
			}
			break;
		case 'V':
			a = tt == TUVLONG ? AMOVDVU : AMOVDV;
			break;
		case 'F':
			a = AMOVD;
			if(tt == TFLOAT && ft != TFLOAT)
				a = AMOVDF;
			break;
		}
		break;
	}
	if(a == AGOK)
		diag(Z, "bad opcode in gmove %T -> %T", f->type, t->type);
	if(a == copyop(tc) && samaddr(f, t))
		return;
	gins(a, f, t);
}

void
gmove(Node *f, Node *t)
{
	int ft, tt;
	Node nod;

	ft = f->type->etype;
	tt = t->type->etype;
	if(debug['O'])
		print("gmove: %O[%T],%O[%T]\n", f->op, f->type, t->op, t->type);
	if(f->op == OCONST) {
		if(ismem(t)) {
			gconst(f, t, 1);
			return;
		}
		if(t->op == OREGISTER) {
			gconst(f, t, 0);
			return;
		}
	}
	if(ismem(f)) {
		if(t->op == OREGISTER && tclass(f->type) == tclass(t->type))
			regalloc(&nod, f, t);
		else
			regalloc(&nod, f, Z);
		gins(loadop(ft), f, &nod);
		gmove(&nod, t);
		regfree(&nod);
		return;
	}
	if(ismem(t)) {
		if(f->op == OREGISTER && tclass(f->type) == tclass(t->type)) {
			gins(storeop(tt), f, t);
			return;
		}
		regalloc(&nod, t, Z);
		gmove(f, &nod);
		gins(storeop(tt), &nod, t);
		regfree(&nod);
		return;
	}
	if(t->op != OREGISTER) {
		diag(Z, "bad gmove %O -> %O", f->op, t->op);
		return;
	}
	if(f->op != OREGISTER) {
		if(tclass(f->type) == tclass(t->type)) {
			gins(copyop(tclass(t->type)), f, t);
			return;
		}
		regalloc(&nod, f, Z);
		gins(copyop(tclass(f->type)), f, &nod);
		greg(&nod, t);
		regfree(&nod);
		return;
	}
	greg(f, t);
}

void
gins(int a, Node *f, Node *t)
{

	nextpc();
	p->as = a;
	if(f != Z)
		naddr(f, &p->from);
	if(t != Z)
		naddr(t, &p->to);
	if(debug['g'])
		print("%P\n", p);
}

static int
relop(int o, int br)
{
	int a;

	switch(o) {
	case OEQ:	a = ACMPEQ; break;
	case ONE:	a = ACMPNE; break;
	case OLT:	a = ACMPLT; break;
	case OLE:	a = ACMPLE; break;
	case OGT:	a = ACMPGT; break;
	case OGE:	a = ACMPGE; break;
	case OLO:	a = ACMPLO; break;
	case OLS:	a = ACMPLS; break;
	case OHI:	a = ACMPHI; break;
	case OHS:	a = ACMPHS; break;
	default:
		return AGOK;
	}
	if(br)
		a += ABEQ - ACMPEQ;
	return a;
}

void
gopcode(int o, Node *f1, Node *f2, Node *t)
{
	int a;
	Node nod;
	Adr ta;

	if(debug['O']) {
		if(f1 != Z && f1->type != T)
			print("gop: %O %O[%T],", o, f1->op, f1->type);
		else
			print("gop: %O Z,", o);
		if(f2 != Z && f2->type != T)
			print("%O[%T],", f2->op, f2->type);
		if(t != Z && t->type != T)
			print("%O[%T]\n", t->op, t->type);
		else
			print("Z\n");
	}
	a = AGOK;
	switch(o) {
	case OAS:
		gmove(f1, t);
		return;
	case OASADD:
	case OADD:
		a = AADD;
		break;
	case OASSUB:
	case OSUB:
		a = ASUB;
		break;
	case OASOR:
	case OOR:
		a = AOR;
		break;
	case OASAND:
	case OAND:
		a = AAND;
		break;
	case OASXOR:
	case OXOR:
		a = AXOR;
		break;
	case OASLSHR:
	case OLSHR:
		a = ASRL;
		break;
	case OASASHR:
	case OASHR:
		a = ASRA;
		break;
	case OASASHL:
	case OASHL:
		a = ASLL;
		break;
	case OASMUL:
	case OMUL:
	case OASLMUL:
	case OLMUL:
		a = AMUL;
		break;
	case OASDIV:
	case ODIV:
		a = ADIV;
		break;
	case OASMOD:
	case OMOD:
		a = AMOD;
		break;
	case OASLDIV:
	case OLDIV:
		a = ADIVU;
		break;
	case OASLMOD:
	case OLMOD:
		a = AMODU;
		break;
	case OEQ:
	case ONE:
	case OLT:
	case OLE:
	case OGT:
	case OGE:
	case OLO:
	case OLS:
	case OHI:
	case OHS:
		a = relop(o, t == Z);
		break;
	}
	if(a == AGOK) {
		diag(Z, "bad in gopcode %O", o);
		return;
	}
	if(f2 != Z && f2->op != OREGISTER) {
		regalloc(&nod, f2, Z);
		gmove(f2, &nod);
		gopcode(o, f1, &nod, t);
		regfree(&nod);
		return;
	}
	nextpc();
	p->as = a;
	if(f1 != Z)
		naddr(f1, &p->from);
	if(f2 != Z) {
		naddr(f2, &ta);
		p->reg = ta.reg;
	}
	if(t != Z)
		naddr(t, &p->to);
	if(debug['g'])
		print("%P\n", p);
	if(a >= AADD && a <= ASRL && t != Z && t->op == OREGISTER && t->type->etype == TFLOAT)
		gins(AMOVDF, t, t);
}

int
samaddr(Node *f, Node *t)
{

	if(f->op != t->op)
		return 0;
	switch(f->op) {
	case OREGISTER:
		if(f->reg != t->reg)
			break;
		return 1;
	}
	return 0;
}

void
gbranch(int o)
{
	int a;

	a = AGOK;
	switch(o) {
	case ORETURN:
		a = ARET;
		gretval();
		break;
	case OGOTO:
		a = AJMP;
		break;
	}
	nextpc();
	if(a == AGOK) {
		diag(Z, "bad in gbranch %O",  o);
		nextpc();
	}
	p->as = a;
}

void
patch(Prog *op, long pc)
{

	op->to.offset = pc;
	op->to.type = D_BRANCH;
}

void
gpseudo(int a, Sym *s, Node *n)
{
	Prog *tp;

	nextpc();
	p->as = a;
	p->from.type = D_OREG;
	p->from.sym = s;
	if(a == ATEXT)
		p->reg = (profileflg ? 0 : NOPROF);
	p->from.name = D_EXTERN;
	if(s->class == CSTATIC)
		p->from.name = D_STATIC;
	naddr(n, &p->to);
	if(a == ADATA || a == AGLOBL)
		pc--;
	if(a == ATEXT) {
		tp = p;
		gprologue(s);
		p = tp;
	}
}

static void
gpar(int k)
{
	nextpc();
	p->as = APARAM;
	p->from.type = D_CONST;
	p->from.offset = k;
}

void
gdecl(Sym *s)
{
	Type *t, *r;

	t = s->type->down;
	if(t == T || t->etype == TOLD)
		return;
	r = s->type->link;
	nextpc();
	p->as = ADECL;
	p->from.type = D_OREG;
	p->from.sym = s;
	p->from.name = D_EXTERN;
	if(typecmplx[r->etype] && sutype(r) < 0)
		gpar(KI32);
	for(; t != T; t = t->down) {
		if(t->etype == TVOID)
			break;
		if(t->etype == TDOT) {
			gpar(KI32);
			break;
		}
		gpar(kind(t));
	}
	nextpc();
	p->as = ARESULT;
	p->from.type = D_CONST;
	p->from.offset = typecmplx[r->etype] && sutype(r) < 0 ? KNONE : kind(r);
}

static void
gstpar(int a, int i, long off)
{
	nextpc();
	p->as = a;
	p->from.type = D_WPARAM;
	p->from.offset = i;
	p->to.type = D_OREG;
	p->to.name = D_PARAM;
	p->to.offset = off;
}

static void
gcppar(int i, long off, long w)
{
	nextpc();
	p->as = AMOVW;
	p->from.type = D_WPARAM;
	p->from.offset = i;
	p->to.type = D_REG;
	p->to.reg = REGTMP;

	nextpc();
	p->as = AMOVW;
	p->from.type = D_CONST;
	p->from.name = D_PARAM;
	p->from.offset = off;
	p->to.type = D_REG;
	p->to.reg = REGTMP2;

	nextpc();
	p->as = ACOPY;
	p->from.type = D_CONST;
	p->from.offset = w;
	p->reg = REGTMP;
	p->to.type = D_REG;
	p->to.reg = REGTMP2;
}

static int
stop(int k)
{
	switch(k) {
	case KI64:
		return AMOVV;
	case KF32:
		return AMOVF;
	case KF64:
		return AMOVD;
	}
	return AMOVW;
}

static	long	retoff = -1;
static	long	retsub;

void
gprologue(Sym *fs)
{
	Type *t, *r;
	long o, off, sub;
	int i, k;

	r = thisfn->link;
	o = align(0, r, Aarg0);
	i = 0;
	retoff = -1;
	if(typecmplx[r->etype] && sutype(r) < 0) {
		gpar(KI32);
		gstpar(AMOVW, i++, 0);
	}
	t = thisfn->down;
	if(t == T && fs != S && fs->type != T && fs->type->down != T && fs->type->down->etype == TOLD)
		t = fs->type->down;
	if(t != T && t->etype == TOLD)
		t = t->down;
	for(; t != T; t = t->down) {
		if(t->etype == TVOID)
			break;
		if(t->etype == TDOT) {
			o = align(o, types[TIND], Aarg1);
			gpar(KI32);
			gstpar(AMOVW, i++, o);
			o = align(o, types[TIND], Aarg2);
			break;
		}
		o = align(o, t, Aarg1);
		off = o;
		o = align(o, t, Aarg2);
		k = kind(t);
		gpar(k);
		if(typesu[t->etype] && sutype(t) < 0)
			gcppar(i++, off, t->width);
		else if(typesu[t->etype]) {
			sumember(t, &sub);
			gstpar(stop(k), i++, off + sub);
		} else
			gstpar(stop(k), i++, off);
	}
	if(typecmplx[r->etype] && sutype(r) >= 0) {
		retoff = round(o, talign(r));
		o = retoff + r->width;
		sumember(r, &retsub);
		nextpc();
		p->as = AMOVW;
		p->from.type = D_CONST;
		p->from.name = D_PARAM;
		p->from.offset = retoff;
		p->to.type = D_OREG;
		p->to.name = D_PARAM;
		p->to.offset = 0;
	}
	nextpc();
	p->as = ARESULT;
	p->from.type = D_CONST;
	p->from.offset = typecmplx[r->etype] && sutype(r) < 0 ? KNONE : kind(r);
	p->to.type = D_CONST;
	p->to.offset = round(o, 8);
}

void
gretval(void)
{
	Type *r;
	int et, rr;

	r = thisfn->link;
	if(retoff >= 0) {
		et = sutype(r);
		rr = REGRET;
		if(typev[et])
			rr = VREGRET;
		else if(typefd[et])
			rr = FREGRET;
		nextpc();
		p->as = loadop(et);
		p->from.type = D_OREG;
		p->from.name = D_PARAM;
		p->from.offset = retoff + retsub;
		p->to.type = D_REG;
		p->to.reg = rr;
		return;
	}
	if(extop(r->etype) != AXXX) {
		nextpc();
		p->as = extop(r->etype);
		p->from.type = D_REG;
		p->from.reg = REGRET;
		p->to.type = D_REG;
		p->to.reg = REGRET;
	}
}

int
sconst(Node *n)
{
	vlong vv;

	if(n->op == OCONST) {
		if(!typefd[n->type->etype]) {
			vv = n->vconst;
			if(vv >= -0x7fffffffLL && vv <= 0x7fffffffLL)
				return 1;
		}
	}
	return 0;
}

long
exreg(Type *t)
{
	USED(t);
	return 0;
}

schar	ewidth[NTYPE] =
{
	-1,
	SZ_CHAR,
	SZ_CHAR,
	SZ_SHORT,
	SZ_SHORT,
	SZ_INT,
	SZ_INT,
	SZ_LONG,
	SZ_LONG,
	SZ_VLONG,
	SZ_VLONG,
	SZ_FLOAT,
	SZ_DOUBLE,
	SZ_IND,
	0,
	-1,
	0,
	-1,
	-1,
	SZ_INT,
};

long	ncast[NTYPE] =
{
	0,
	BCHAR|BUCHAR,
	BCHAR|BUCHAR,
	BSHORT|BUSHORT,
	BSHORT|BUSHORT,
	BINT|BUINT|BLONG|BULONG|BIND,
	BINT|BUINT|BLONG|BULONG|BIND,
	BINT|BUINT|BLONG|BULONG|BIND,
	BINT|BUINT|BLONG|BULONG|BIND,
	BVLONG|BUVLONG,
	BVLONG|BUVLONG,
	BFLOAT,
	BDOUBLE,
	BLONG|BULONG|BIND,
	0,
	0,
	0,
	BSTRUCT,
	BUNION,
	0,
};
