#include "gc.h"

void
swit1(C1 *q, int nc, long def, Node *n)
{
	Node tn;

	if(typev[n->type->etype])
		regalloc(&tn, &vregnode, Z);
	else
		regalloc(&tn, &regnode, Z);
	swit2(q, nc, def, n, &tn);
	regfree(&tn);
}

void
swit2(C1 *q, int nc, long def, Node *n, Node *tn)
{
	C1 *r;
	int i;
	Prog *sp;

	USED(tn);
	if(nc < 5) {
		for(i=0; i<nc; i++) {
			gopcode(OEQ, nodgconst(q->val, n->type), n, Z);
			patch(p, q->label);
			q++;
		}
		gbranch(OGOTO);
		patch(p, def);
		return;
	}
	i = nc / 2;
	r = q+i;
	gopcode(OGT, nodgconst(r->val, n->type), n, Z);
	sp = p;
	gopcode(OEQ, nodgconst(r->val, n->type), n, Z);
	patch(p, r->label);
	swit2(q, i, def, n, tn);

	patch(sp, pc);
	swit2(r+1, nc-i-1, def, n, tn);
}

void
bitload(Node *b, Node *n1, Node *n2, Node *n3, Node *nn)
{
	int sh;
	long v;
	Node *l;

	l = b->left;
	if(typev[l->type->etype])
		diag(b, "vlong bit field not supported");
	if(n2 != Z) {
		regalloc(n1, l, nn);
		reglcgen(n2, l, Z);
		regalloc(n3, l, Z);
		gopcode(OAS, n2, Z, n3);
		gopcode(OAS, n3, Z, n1);
	} else {
		regalloc(n1, l, nn);
		cgen(l, n1);
	}
	if(b->type->shift == 0 && typeu[b->type->etype]) {
		v = convvtox(((vlong)1 << b->type->nbits) - 1, TLONG);
		gopcode(OAND, nodconst(v), Z, n1);
	} else {
		sh = 32 - b->type->shift - b->type->nbits;
		if(sh > 0)
			gopcode(OASHL, nodconst(sh), Z, n1);
		sh += b->type->shift;
		if(sh > 0)
			if(typeu[b->type->etype])
				gopcode(OLSHR, nodconst(sh), Z, n1);
			else
				gopcode(OASHR, nodconst(sh), Z, n1);
	}
}

void
bitstore(Node *b, Node *n1, Node *n2, Node *n3, Node *nn)
{
	long v;
	Node nod, *l;
	int sh;

	l = b->left;
	regalloc(&nod, l, Z);
	v = convvtox(((vlong)1 << b->type->nbits) - 1, TLONG);
	gopcode(OAND, nodconst(v), Z, n1);
	gopcode(OAS, n1, Z, &nod);
	if(nn != Z) {
		if(!typeu[b->type->etype] && b->type->nbits < 32) {
			gopcode(OASHL, nodconst(32 - b->type->nbits), Z, n1);
			gopcode(OASHR, nodconst(32 - b->type->nbits), Z, n1);
		}
		gopcode(OAS, n1, Z, nn);
	}
	sh = b->type->shift;
	if(sh > 0)
		gopcode(OASHL, nodconst(sh), Z, &nod);
	v = convvtox((vlong)v << sh, TLONG);
	gopcode(OAND, nodconst(convvtox(~(vlong)v, TLONG)), Z, n3);
	gopcode(OOR, n3, Z, &nod);
	gopcode(OAS, &nod, Z, n2);

	regfree(&nod);
	regfree(n1);
	regfree(n2);
	regfree(n3);
}

long
outstring(char *s, long n)
{
	long r;

	if(suppress)
		return nstring;
	r = nstring;
	while(n) {
		string[mnstring] = *s++;
		mnstring++;
		nstring++;
		if(mnstring >= NSNAME) {
			gpseudo(ADATA, symstring, nodconst(0L));
			p->from.offset += nstring - NSNAME;
			p->reg = NSNAME;
			p->to.type = D_SCONST;
			memmove(p->to.sval, string, NSNAME);
			mnstring = 0;
		}
		n--;
	}
	return r;
}

int
mulcon(Node *n, Node *nn)
{
	USED(n);
	USED(nn);
	return 0;
}

void
sextern(Sym *s, Node *a, long o, long w)
{
	long e, lw;

	for(e=0; e<w; e+=NSNAME) {
		lw = NSNAME;
		if(w-e < lw)
			lw = w-e;
		gpseudo(ADATA, s, nodconst(0));
		p->from.offset += o+e;
		p->reg = lw;
		p->to.type = D_SCONST;
		memmove(p->to.sval, a->cstring+e, lw);
	}
}

void
gextern(Sym *s, Node *a, long o, long w)
{
	gpseudo(ADATA, s, a);
	p->from.offset += o;
	p->reg = w;
	if(p->to.type == D_OREG)
		p->to.type = D_CONST;
}

void	zname(Biobuf*, Sym*, int);
char*	zaddr(char*, Adr*, int);
void	zwrite(Biobuf*, Prog*, int, int);
void	outhist(Biobuf*);

void
zwrite(Biobuf *b, Prog *p, int sf, int st)
{
	char bf[100], *bp;

	bf[0] = p->as;
	bf[1] = p->reg;
	bf[2] = p->lineno;
	bf[3] = p->lineno>>8;
	bf[4] = p->lineno>>16;
	bf[5] = p->lineno>>24;
	bp = zaddr(bf+6, &p->from, sf);
	bp = zaddr(bp, &p->to, st);
	Bwrite(b, bf, bp-bf);
}

void
outcode(void)
{
	struct { Sym *sym; short type; } h[NSYM];
	Prog *p;
	Sym *s;
	int sf, st, t, sym;

	if(debug['S']) {
		for(p = firstp; p != P; p = p->link)
			if(p->as != ADATA && p->as != AGLOBL)
				pc--;
		for(p = firstp; p != P; p = p->link) {
			print("%P\n", p);
			if(p->as != ADATA && p->as != AGLOBL)
				pc++;
		}
	}
	outhist(&outbuf);
	for(sym=0; sym<NSYM; sym++) {
		h[sym].sym = S;
		h[sym].type = 0;
	}
	sym = 1;
	for(p = firstp; p != P; p = p->link) {
	jackpot:
		sf = 0;
		s = p->from.sym;
		while(s != S) {
			sf = s->sym;
			if(sf < 0 || sf >= NSYM)
				sf = 0;
			t = p->from.name;
			if(h[sf].type == t)
			if(h[sf].sym == s)
				break;
			s->sym = sym;
			zname(&outbuf, s, t);
			h[sym].sym = s;
			h[sym].type = t;
			sf = sym;
			sym++;
			if(sym >= NSYM)
				sym = 1;
			break;
		}
		st = 0;
		s = p->to.sym;
		while(s != S) {
			st = s->sym;
			if(st < 0 || st >= NSYM)
				st = 0;
			t = p->to.name;
			if(h[st].type == t)
			if(h[st].sym == s)
				break;
			s->sym = sym;
			zname(&outbuf, s, t);
			h[sym].sym = s;
			h[sym].type = t;
			st = sym;
			sym++;
			if(sym >= NSYM)
				sym = 1;
			if(st == sf)
				goto jackpot;
			break;
		}
		zwrite(&outbuf, p, sf, st);
	}
	firstp = P;
	lastp = P;
}

void
outhist(Biobuf *b)
{
	Hist *h;
	char *p, *q, *op, c;
	Prog pg;
	int n;

	pg = zprog;
	pg.as = AHISTORY;
	c = pathchar();
	for(h = hist; h != H; h = h->link) {
		p = h->name;
		op = 0;
		if(p && p[0] != c && h->offset == 0 && pathname){
			if(pathname[0] == c){
				op = p;
				p = pathname;
			}
		}
		while(p) {
			q = utfrune(p, c);
			if(q) {
				n = q-p;
				if(n == 0){
					n = 1;
					*p = '/';
				}
				q++;
			} else {
				n = strlen(p);
				q = 0;
			}
			if(n) {
				Bputc(b, ANAME);
				Bputc(b, D_FILE);
				Bputc(b, 1);
				Bputc(b, '<');
				Bwrite(b, p, n);
				Bputc(b, 0);
			}
			p = q;
			if(p == 0 && op) {
				p = op;
				op = 0;
			}
		}
		pg.lineno = h->line;
		pg.to.type = zprog.to.type;
		pg.to.offset = h->offset;
		if(h->offset)
			pg.to.type = D_CONST;

		zwrite(b, &pg, 0, 0);
	}
}

void
zname(Biobuf *b, Sym *s, int t)
{
	char *n, bf[7];
	ulong sig;

	n = s->name;
	if(debug['T'] && t == D_EXTERN && s->sig != SIGDONE && s->type != types[TENUM] && s != symrathole){
		sig = sign(s);
		bf[0] = ASIGNAME;
		bf[1] = sig;
		bf[2] = sig>>8;
		bf[3] = sig>>16;
		bf[4] = sig>>24;
		bf[5] = t;
		bf[6] = s->sym;
		Bwrite(b, bf, 7);
		s->sig = SIGDONE;
	} else {
		bf[0] = ANAME;
		bf[1] = t;
		bf[2] = s->sym;
		Bwrite(b, bf, 3);
	}
	Bwrite(b, n, strlen(n)+1);
}

static char*
z4(char *bp, long l)
{
	bp[0] = l;
	bp[1] = l>>8;
	bp[2] = l>>16;
	bp[3] = l>>24;
	return bp+4;
}

char*
zaddr(char *bp, Adr *a, int s)
{
	union {
		double	d;
		uvlong	v;
	} u;

	bp[0] = a->type;
	bp[1] = a->reg;
	bp[2] = s;
	bp[3] = a->name;
	bp += 4;
	switch(a->type) {
	default:
		diag(Z, "unknown type %d in zaddr", a->type);

	case D_NONE:
	case D_REG:
		break;

	case D_OREG:
	case D_CONST:
	case D_BRANCH:
	case D_WPARAM:
	case D_LOCAL:
		bp = z4(bp, a->offset);
		break;

	case D_VCONST:
		bp = z4(bp, a->offset);
		bp = z4(bp, a->offset>>32);
		break;

	case D_SCONST:
		memmove(bp, a->sval, NSNAME);
		bp += NSNAME;
		break;

	case D_FCONST:
		u.d = a->dval;
		bp = z4(bp, u.v);
		bp = z4(bp, u.v>>32);
		break;
	}
	return bp;
}

long
align(long i, Type *t, int op)
{
	long o;
	int w;

	o = i;
	w = 1;
	switch(op) {
	default:
		diag(Z, "unknown align opcode %d", op);
		break;

	case Asu2:
		w = talign(t);
		if(packflg)
			w = packflg;
		break;

	case Ael1:
		w = talign(t);
		if(packflg)
			w = packflg;
		break;

	case Ael2:
		o += t->width;
		break;

	case Aarg0:
		if(typecmplx[t->etype]) {
			o = align(o, types[TIND], Aarg1);
			o = align(o, types[TIND], Aarg2);
		}
		break;

	case Aarg1:
		w = talign(t);
		if(w < SZ_LONG)
			w = SZ_LONG;
		break;

	case Aarg2:
		o += t->width;
		w = SZ_LONG;
		break;

	case Aaut3:
		o = align(o, t, Ael1);
		o = align(o, t, Ael2);
		break;
	}
	o = round(o, w);
	if(debug['A'])
		print("align %s %ld %T = %ld\n", bnames[op], i, t, o);
	return o;
}

long
maxround(long max, long v)
{
	v = round(v, SZ_VLONG);
	if(v > max)
		return v;
	return max;
}
