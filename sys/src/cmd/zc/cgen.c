#include "gc.h"

enum
{
	NARG	= 64,
};

void
cgen(Node *n, Node *nn)
{
	Node *l, *r;
	Prog *p1;
	Node nod, nod1, nod2, nod3, nod4;
	int o;
	long v, curs;

	if(debug['g']) {
		prtree(nn, "cgen lhs");
		prtree(n, "cgen");
	}
	if(n == Z || n->type == T)
		return;
	if(typecmplx[n->type->etype]) {
		sugen(n, nn, n->type->width);
		return;
	}
	l = n->left;
	r = n->right;
	o = n->op;
	if(n->addable >= INDEXED) {
		if(nn == Z) {
			switch(o) {
			default:
				nullwarn(Z, Z);
				break;
			case OINDEX:
				nullwarn(l, r);
				break;
			}
			return;
		}
		gmove(n, nn);
		return;
	}
	if(vecop(o) || n->type->etype == TVEC) {
		vcgen(n, nn);
		return;
	}
	curs = cursafe;

	if(n->complex >= FNX)
	if(l->complex >= FNX)
	if(r != Z && r->complex >= FNX)
	switch(o) {
	default:
		regret(&nod, r);
		cgen(r, &nod);

		regsalloc(&nod1, r);
		gopcode(OAS, &nod, Z, &nod1);

		regfree(&nod);
		nod = *n;
		nod.right = &nod1;
		cgen(&nod, nn);
		return;

	case OFUNC:
	case OCOMMA:
	case OANDAND:
	case OOROR:
	case OCOND:
	case ODOT:
		break;
	}

	switch(o) {
	default:
		diag(n, "unknown op in cgen: %O", o);
		break;

	case OAS:
		if(l->op == OBIT)
			goto bitas;
		if(l->addable >= INDEXED && l->complex < FNX) {
			if(nn != Z || r->addable < INDEXED) {
				if(r->complex >= FNX && nn == Z)
					regret(&nod, r);
				else
					regalloc(&nod, r, nn);
				cgen(r, &nod);
				gmove(&nod, l);
				if(nn != Z)
					gmove(&nod, nn);
				regfree(&nod);
			} else
				gmove(r, l);
			break;
		}
		if(l->complex >= r->complex) {
			reglcgen(&nod1, l, Z);
			if(r->addable >= INDEXED) {
				gmove(r, &nod1);
				if(nn != Z)
					gmove(r, nn);
				regfree(&nod1);
				break;
			}
			regalloc(&nod, r, nn);
			cgen(r, &nod);
		} else {
			regalloc(&nod, r, nn);
			cgen(r, &nod);
			reglcgen(&nod1, l, Z);
		}
		gmove(&nod, &nod1);
		if(nn != Z)
			gmove(&nod, nn);
		regfree(&nod);
		regfree(&nod1);
		break;

	bitas:
		n = l->left;
		regalloc(&nod, r, nn);
		if(l->complex >= r->complex) {
			reglcgen(&nod1, n, Z);
			cgen(r, &nod);
		} else {
			cgen(r, &nod);
			reglcgen(&nod1, n, Z);
		}
		regalloc(&nod2, n, Z);
		gopcode(OAS, &nod1, Z, &nod2);
		bitstore(l, &nod, &nod1, &nod2, nn);
		break;

	case OBIT:
		if(nn == Z) {
			nullwarn(l, Z);
			break;
		}
		bitload(n, &nod, Z, Z, nn);
		gopcode(OAS, &nod, Z, nn);
		regfree(&nod);
		break;

	case OADD:
	case OSUB:
	case OAND:
	case OOR:
	case OXOR:
	case OLSHR:
	case OASHL:
	case OASHR:
		if(nn != Z && nn->op == OREGISTER)
		if(sconst(r))
		if(!typefd[n->type->etype]) {
			cgen(l, nn);
			if(r->vconst == 0)
			if(o != OAND)
				break;
			gopcode(o, r, Z, nn);
			break;
		}

	case OLMUL:
	case OLDIV:
	case OLMOD:
	case OMUL:
	case ODIV:
	case OMOD:
		if(nn == Z) {
			nullwarn(l, r);
			break;
		}
		if(l->complex >= r->complex) {
			regalloc(&nod, l, nn);
			cgen(l, &nod);
			regalloc(&nod1, r, Z);
			cgen(r, &nod1);
			gopcode(o, &nod1, Z, &nod);
		} else {
			if(o == OLSHR || o == OASHL || o == OASHR){
				regalloc(&nod1, r, Z);
				cgen(r, &nod1);
				regalloc(&nod, l, nn);
				cgen(l, &nod);
				gopcode(o, &nod1, Z, &nod);
			}else{
				regalloc(&nod, r, nn);
				cgen(r, &nod);
				regalloc(&nod1, l, Z);
				cgen(l, &nod1);
				gopcode(o, &nod, &nod1, &nod);
			}
		}
		gopcode(OAS, &nod, Z, nn);
		regfree(&nod);
		regfree(&nod1);
		break;

	case OASLSHR:
	case OASASHL:
	case OASASHR:
	case OASAND:
	case OASADD:
	case OASSUB:
	case OASXOR:
	case OASOR:
		if(l->op == OBIT)
			goto asbitop;
		if(sconst(r))
		if(!typefd[n->type->etype])
		if(!typefd[l->type->etype]) {
			if(l->addable < INDEXED)
				reglcgen(&nod2, l, Z);
			else
				nod2 = *l;
			regalloc(&nod, l, nn);
			gopcode(OAS, &nod2, Z, &nod);
			gopcode(o, r, Z, &nod);
			gopcode(OAS, &nod, Z, &nod2);
			if(nn != Z)
				gopcode(OAS, &nod, Z, nn);

			regfree(&nod);
			if(l->addable < INDEXED)
				regfree(&nod2);
			break;
		}

	case OASLMUL:
	case OASLDIV:
	case OASLMOD:
	case OASMUL:
	case OASDIV:
	case OASMOD:
		if(l->op == OBIT)
			goto asbitop;
		if(l->complex >= r->complex) {
			if(l->addable < INDEXED)
				reglcgen(&nod2, l, Z);
			else
				nod2 = *l;
			regalloc(&nod1, r, Z);
			cgen(r, &nod1);
		} else {
			regalloc(&nod1, r, Z);
			cgen(r, &nod1);
			if(l->addable < INDEXED)
				reglcgen(&nod2, l, Z);
			else
				nod2 = *l;
		}

		regalloc(&nod, n, nn);
		gmove(&nod2, &nod);
		gopcode(o, &nod1, Z, &nod);
		gmove(&nod, &nod2);
		if(nn != Z)
			gopcode(OAS, &nod, Z, nn);
		regfree(&nod);
		regfree(&nod1);
		if(l->addable < INDEXED)
			regfree(&nod2);
		break;

	asbitop:
		regalloc(&nod4, n, nn);
		if(l->complex >= r->complex) {
			bitload(l, &nod, &nod1, &nod2, &nod4);
			regalloc(&nod3, r, Z);
			cgen(r, &nod3);
		} else {
			regalloc(&nod3, r, Z);
			cgen(r, &nod3);
			bitload(l, &nod, &nod1, &nod2, &nod4);
		}
		gmove(&nod, &nod4);
		gopcode(o, &nod3, Z, &nod4);
		regfree(&nod3);
		gmove(&nod4, &nod);
		regfree(&nod4);
		bitstore(l, &nod, &nod1, &nod2, nn);
		break;

	case OADDR:
		if(nn == Z) {
			nullwarn(l, Z);
			break;
		}
		lcgen(l, nn);
		break;

	case OFUNC:
		gcall(n, nn, Z);
		break;

	case ONEG:
		if(nn == Z) {
			nullwarn(l, Z);
			break;
		}
		regalloc(&nod, l, nn);
		cgen(l, &nod);
		gins(ANEG, &nod, &nod);
		gmove(&nod, nn);
		regfree(&nod);
		break;

	case OIND:
		if(nn == Z) {
			nullwarn(l, Z);
			break;
		}
		regialloc(&nod, n, nn);
		r = l;
		while(r->op == OADD)
			r = r->right;
		if(sconst(r)) {
			v = r->vconst;
			r->vconst = 0;
			cgen(l, &nod);
			nod.xoffset += v;
			r->vconst = v;
		} else
			cgen(l, &nod);
		regind(&nod, n);
		gopcode(OAS, &nod, Z, nn);
		regfree(&nod);
		break;

	case OEQ:
	case ONE:
	case OLE:
	case OLT:
	case OGE:
	case OGT:
	case OLO:
	case OLS:
	case OHI:
	case OHS:
		if(nn == Z) {
			nullwarn(l, r);
			break;
		}
		boolgen(n, 1, nn);
		break;

	case OANDAND:
	case OOROR:
		boolgen(n, 1, nn);
		if(nn == Z)
			patch(p, pc);
		break;

	case ONOT:
		if(nn == Z) {
			nullwarn(l, Z);
			break;
		}
		boolgen(n, 1, nn);
		break;

	case OCOMMA:
		cgen(l, Z);
		cgen(r, nn);
		break;

	case OCAST:
		if(nn == Z) {
			nullwarn(l, Z);
			break;
		}
		if(nocast(l->type, n->type)) {
			if(nocast(n->type, nn->type)) {
				cgen(l, nn);
				break;
			}
		}
		regalloc(&nod, l, nn);
		cgen(l, &nod);
		regalloc(&nod1, n, &nod);
		gopcode(OAS, &nod, Z, &nod1);
		gopcode(OAS, &nod1, Z, nn);
		regfree(&nod1);
		regfree(&nod);
		break;

	case ODOT:
		sugen(l, nodrat, l->type->width);
		if(nn != Z) {
			warn(n, "non-interruptable temporary");
			nod = *nodrat;
			if(!r || r->op != OCONST) {
				diag(n, "DOT and no offset");
				break;
			}
			nod.xoffset += (long)r->vconst;
			nod.type = n->type;
			cgen(&nod, nn);
		}
		break;

	case OCOND:
		bcgen(l, 1);
		p1 = p;
		cgen(r->left, nn);
		gbranch(OGOTO);
		patch(p1, pc);
		p1 = p;
		cgen(r->right, nn);
		patch(p1, pc);
		break;

	case OPOSTINC:
	case OPOSTDEC:
		v = 1;
		if(l->type->etype == TIND)
			v = l->type->link->width;
		if(o == OPOSTDEC)
			v = -v;
		if(l->op == OBIT)
			goto bitinc;
		if(nn == Z)
			goto pre;

		if(l->addable < INDEXED)
			reglcgen(&nod2, l, Z);
		else
			nod2 = *l;

		regalloc(&nod, l, nn);
		gopcode(OAS, &nod2, Z, &nod);
		regalloc(&nod1, l, Z);
		if(typefd[l->type->etype]) {
			regalloc(&nod3, l, Z);
			if(v < 0) {
				gopcode(OAS, nodfconst(-v), Z, &nod3);
				gopcode(OSUB, &nod3, &nod, &nod1);
			} else {
				gopcode(OAS, nodfconst(v), Z, &nod3);
				gopcode(OADD, &nod3, &nod, &nod1);
			}
			regfree(&nod3);
		} else
			gopcode(OADD, nodconst(v), &nod, &nod1);
		gopcode(OAS, &nod1, Z, &nod2);
		gopcode(OAS, &nod, Z, nn);

		regfree(&nod);
		regfree(&nod1);
		if(l->addable < INDEXED)
			regfree(&nod2);
		break;

	case OPREINC:
	case OPREDEC:
		v = 1;
		if(l->type->etype == TIND)
			v = l->type->link->width;
		if(o == OPREDEC)
			v = -v;
		if(l->op == OBIT)
			goto bitinc;

	pre:
		if(l->addable < INDEXED)
			reglcgen(&nod2, l, Z);
		else
			nod2 = *l;

		regalloc(&nod, l, nn);
		gopcode(OAS, &nod2, Z, &nod);
		if(typefd[l->type->etype]) {
			regalloc(&nod3, l, Z);
			if(v < 0) {
				gopcode(OAS, nodfconst(-v), Z, &nod3);
				gopcode(OSUB, &nod3, Z, &nod);
			} else {
				gopcode(OAS, nodfconst(v), Z, &nod3);
				gopcode(OADD, &nod3, Z, &nod);
			}
			regfree(&nod3);
		} else
			gopcode(OADD, nodconst(v), Z, &nod);
		gopcode(OAS, &nod, Z, &nod2);
		if(nn != Z)
			gopcode(OAS, &nod, Z, nn);
		if(nn && l->op == ONAME)
			gins(ANOP, l, Z);

		regfree(&nod);
		if(l->addable < INDEXED)
			regfree(&nod2);
		break;

	bitinc:
		if(nn != Z && (o == OPOSTINC || o == OPOSTDEC)) {
			bitload(l, &nod, &nod1, &nod2, Z);
			gopcode(OAS, &nod, Z, nn);
			gopcode(OADD, nodconst(v), Z, &nod);
			bitstore(l, &nod, &nod1, &nod2, Z);
			break;
		}
		bitload(l, &nod, &nod1, &nod2, nn);
		gopcode(OADD, nodconst(v), Z, &nod);
		bitstore(l, &nod, &nod1, &nod2, nn);
		break;
	}
	cursafe = curs;
}

static void
flatargs(Node *n, Node **av, int *nav)
{
	if(n == Z)
		return;
	if(n->op == OLIST) {
		flatargs(n->left, av, nav);
		flatargs(n->right, av, nav);
		return;
	}
	if(*nav >= NARG) {
		diag(n, "too many arguments");
		return;
	}
	av[(*nav)++] = n;
}

static int
sufield(Node *f)
{
	long o;
	Type *t;

	t = sumember(f->type, &o);
	if(o != 0 && f->op != ONAME && f->op != OINDREG)
		return 0;
	f->xoffset += o;
	f->type = t;
	return 1;
}

static int
retkind(Type *t)
{
	if(typesu[t->etype] && sutype(t) < 0)
		return KNONE;
	return kind(t);
}

static void
argreg(Node *a, Node *r, int spill, Type *pt)
{
	Node nod;

	if(pt != T && extop(pt->etype) != AXXX && tclass(r->type) == 'R')
		r->type = pt;
	if(!spill) {
		gext(r);
		return;
	}
	regsalloc(&nod, r);
	gmove(r, &nod);
	regfree(r);
	*a = nod;
}

static void
gvaarg(Node *n, Node *nn)
{
	Node *av[NARG], pa, lv, res, ind;
	Type *mt;
	int nav;
	long a;

	nav = 0;
	flatargs(n->right, av, &nav);
	if(nav != 2 || av[1]->type->etype != TIND) {
		diag(n, "bad __va_arg");
		return;
	}
	mt = av[1]->type->link;
	regialloc(&pa, av[0], Z);
	cgen(av[0], &pa);
	ind = pa;
	ind.op = OINDREG;
	ind.xoffset = 0;
	ind.type = types[TIND];
	regialloc(&lv, av[0], Z);
	gmove(&ind, &lv);
	regialloc(&res, av[0], Z);
	if(typesu[mt->etype] && sutype(mt) < 0) {
		ind = lv;
		ind.op = OINDREG;
		ind.xoffset = 0;
		ind.type = types[TIND];
		gmove(&ind, &res);
		gopcode(OADD, nodconst(4), Z, &lv);
	} else {
		if(typesu[mt->etype])
			mt = sumember(mt, nil);
		a = talign(mt);
		if(a > 4) {
			gopcode(OADD, nodconst(a-1), Z, &lv);
			gopcode(OAND, nodconst(~(a-1)), Z, &lv);
		}
		gmove(&lv, &res);
		gopcode(OADD, nodconst(round(mt->width, 4)), Z, &lv);
	}
	ind = pa;
	ind.op = OINDREG;
	ind.xoffset = 0;
	ind.type = types[TIND];
	gmove(&lv, &ind);
	if(nn != Z)
		gmove(&res, nn);
	regfree(&res);
	regfree(&lv);
	regfree(&pa);
}

void
gcall(Node *n, Node *nn, Node *sret)
{
	Node *l, *av[NARG], tmp[NARG], ar[NARG], nod, nod1, nod2, fn, *sr;
	Type *t, *pt[NARG];
	int i, nav, nfix, va, nar, k;
	long regs, w;

	l = n->left;
	if(l->op == ONAME && l->sym != S && strcmp(l->sym->name, "__va_arg") == 0) {
		gvaarg(n, nn);
		return;
	}
	if(l->complex >= FNX) {
		if(l->op != OIND)
			diag(n, "bad function call");
		regret(&nod, l->left);
		cgen(l->left, &nod);
		regsalloc(&nod1, l->left);
		gopcode(OAS, &nod, Z, &nod1);
		regfree(&nod);
		nod = *n;
		nod.left = &nod2;
		nod2 = *l;
		nod2.left = &nod1;
		nod2.complex = 1;
		gcall(&nod, nn, sret);
		return;
	}
	sr = Z;
	if(sret != Z && sutype(n->type) >= 0) {
		sr = sret;
		sret = Z;
	}
	t = l->type;
	if(t != T && t->etype == TIND)
		t = t->link;
	nfix = NARG;
	va = 0;
	if(t != T && t->etype == TFUNC) {
		i = 0;
		for(t = t->down; t != T; t = t->down) {
			if(t->etype == TDOT) {
				nfix = i;
				va = 1;
				break;
			}
			i++;
		}
	}
	for(i = 0; i < NARG; i++)
		pt[i] = T;
	nav = 0;
	if(sret != Z) {
		av[nav++] = sret;
		if(va)
			nfix++;
	}
	flatargs(n->right, av, &nav);
	t = l->type;
	if(t != T && t->etype == TIND)
		t = t->link;
	if(t != T && t->etype == TFUNC) {
		i = sret != Z;
		for(t = t->down; t != T && i < NARG; t = t->down) {
			if(t->etype == TDOT || t->etype == TVOID || t->etype == TOLD)
				break;
			pt[i++] = t;
		}
	}

	regs = cursafe;
	for(i=0; i<nav; i++) {
		if(av[i]->complex < FNX)
			continue;
		regsalloc(&tmp[i], av[i]);
		if(typesu[av[i]->type->etype])
			sugen(av[i], &tmp[i], av[i]->type->width);
		else {
			nod = znode;
			nod.op = OAS;
			nod.left = &tmp[i];
			nod.right = av[i];
			nod.type = av[i]->type;
			cgen(&nod, Z);
		}
		av[i] = &tmp[i];
	}

	curarg = 0;
	for(i=nfix; i<nav; i++) {
		if(typesu[av[i]->type->etype] && sutype(av[i]->type) < 0) {
			regsalloc(&nod1, av[i]);
			sugen(av[i], &nod1, av[i]->type->width);
			regialloc(&nod, &nod1, Z);
			lcgen(&nod1, &nod);
			curarg = align(curarg, types[TIND], Aarg1);
			nod2 = nod;
			nod2.op = OINDREG;
			nod2.reg = REGSP;
			nod2.xoffset = curarg;
			nod2.type = types[TIND];
			gmove(&nod, &nod2);
			regfree(&nod);
			curarg = align(curarg, types[TIND], Aarg2);
		} else {
			if(typesu[av[i]->type->etype]) {
				fn = *av[i];
				if(fn.addable < INDEXED || !sufield(&fn)) {
					regsalloc(&nod1, av[i]);
					sugen(av[i], &nod1, av[i]->type->width);
					fn = nod1;
					sufield(&fn);
				}
				av[i] = &fn;
			}
			curarg = align(curarg, av[i]->type, Aarg1);
			nod2 = znode;
			nod2.op = OINDREG;
			nod2.reg = REGSP;
			nod2.xoffset = curarg;
			nod2.type = av[i]->type;
			nod2.complex = 0;
			nod2.addable = 12;
			if(av[i]->addable >= INDEXED)
				gmove(av[i], &nod2);
			else {
				regalloc(&nod, av[i], Z);
				cgen(av[i], &nod);
				gmove(&nod, &nod2);
				regfree(&nod);
			}
			curarg = align(curarg, av[i]->type, Aarg2);
		}
		maxargsafe = maxround(maxargsafe, cursafe+curarg);
	}

	nar = 0;
	for(i=0; i<nav && i<nfix; i++) {
		if(sret != Z && i == 0) {
			regialloc(&ar[nar], av[i], Z);
			if(av[i]->op == OADDR && av[i]->left->op != OIND && av[i]->left->addable >= INDEXED)
				lcgen(av[i]->left, &ar[nar]);
			else
				cgen(av[i], &ar[nar]);
			nar++;
			continue;
		}
		if(typesu[av[i]->type->etype]) {
			if(sutype(av[i]->type) < 0) {
				regsalloc(&nod1, av[i]);
				sugen(av[i], &nod1, av[i]->type->width);
				regialloc(&ar[nar], &nod1, Z);
				lcgen(&nod1, &ar[nar]);
				nar++;
				continue;
			}
			fn = *av[i];
			if(fn.addable < INDEXED || !sufield(&fn)) {
				regsalloc(&nod1, av[i]);
				sugen(av[i], &nod1, av[i]->type->width);
				fn = nod1;
				sufield(&fn);
			}
			regalloc(&ar[nar], &fn, Z);
			gmove(&fn, &ar[nar]);
			argreg(&ar[nar], &ar[nar], nar >= 16, pt[i]);
			nar++;
			continue;
		}
		regalloc(&ar[nar], av[i], Z);
		cgen(av[i], &ar[nar]);
		argreg(&ar[nar], &ar[nar], nar >= 16, pt[i]);
		nar++;
	}

	if(l->addable < INDEXED) {
		reglcgen(&fn, l, Z);
	} else
		fn = *l;

	for(i=0; i<nar; i++) {
		k = AARG;
		if(ar[i].type->etype == TFLOAT)
			k = AARGF;
		if(ar[i].op != OREGISTER) {
			regalloc(&nod, &ar[i], Z);
			gmove(&ar[i], &nod);
			gext(&nod);
			gins(k, &nod, Z);
			regfree(&nod);
		} else
			gins(k, &ar[i], Z);
	}
	if(va) {
		nodreg(&nod, &regnode, REGSP);
		nod.type = types[TIND];
		gins(AARG, &nod, Z);
	}
	nextpc();
	p->as = ACALL;
	p->from.type = D_CONST;
	p->from.offset = retkind(n->type);
	naddr(&fn, &p->to);
	if(debug['g'])
		print("%P\n", p);
	for(i=0; i<nar; i++)
		if(ar[i].op == OREGISTER)
			regfree(&ar[i]);
	if(l->addable < INDEXED)
		regfree(&fn);
	cursafe = regs;
	if(sr != Z) {
		nod1 = *n;
		nod1.type = sumember(n->type, &w);
		regret(&nod, &nod1);
		regialloc(&nod2, sr, Z);
		cgen(sr, &nod2);
		nod2.op = OINDREG;
		nod2.xoffset = w;
		nod2.type = nod1.type;
		gmove(&nod, &nod2);
		nod2.op = OREGISTER;
		regfree(&nod2);
		regfree(&nod);
	}
	if(nn != Z && !typesu[n->type->etype] && n->type->etype != TVOID) {
		regret(&nod, n);
		gopcode(OAS, &nod, Z, nn);
		regfree(&nod);
	}
}

void
reglcgen(Node *t, Node *n, Node *nn)
{
	Node *r;
	long v;

	regialloc(t, n, nn);
	if(n->op == OIND) {
		r = n->left;
		while(r->op == OADD)
			r = r->right;
		if(sconst(r)) {
			v = r->vconst;
			r->vconst = 0;
			lcgen(n, t);
			t->xoffset += v;
			r->vconst = v;
			regind(t, n);
			return;
		}
	}
	lcgen(n, t);
	regind(t, n);
}

void
lcgen(Node *n, Node *nn)
{
	Prog *p1;
	Node nod;

	if(debug['g']) {
		prtree(nn, "lcgen lhs");
		prtree(n, "lcgen");
	}
	if(n == Z || n->type == T)
		return;
	if(nn == Z) {
		nn = &nod;
		regalloc(&nod, n, Z);
	}
	switch(n->op) {
	default:
		if(n->addable < INDEXED) {
			diag(n, "unknown op in lcgen: %O", n->op);
			break;
		}
		nod = *n;
		nod.op = OADDR;
		nod.left = n;
		nod.right = Z;
		nod.type = types[TIND];
		gopcode(OAS, &nod, Z, nn);
		break;

	case OCOMMA:
		cgen(n->left, n->left);
		lcgen(n->right, nn);
		break;

	case OIND:
		cgen(n->left, nn);
		break;

	case OCOND:
		bcgen(n->left, 1);
		p1 = p;
		lcgen(n->right->left, nn);
		gbranch(OGOTO);
		patch(p1, pc);
		p1 = p;
		lcgen(n->right->right, nn);
		patch(p1, pc);
		break;
	}
}

void
bcgen(Node *n, int true)
{

	if(n->type == T)
		gbranch(OGOTO);
	else
		boolgen(n, true, Z);
}

static Node*
zero(Node *n)
{
	if(typefd[n->type->etype])
		return nodfconst(0);
	if(typev[n->type->etype])
		return nodgconst(0, types[TVLONG]);
	return nodconst(0);
}

void
boolgen(Node *n, int true, Node *nn)
{
	int o;
	Prog *p1, *p2;
	Node *l, *r, nod, nod1, nod2;
	long curs;
	int fp;

	if(debug['g']) {
		prtree(nn, "boolgen lhs");
		prtree(n, true? "boolgen true" : "boolgen false");
	}
	curs = cursafe;
	l = n->left;
	r = n->right;
	switch(n->op) {

	default:
		regalloc(&nod, n, nn);
		cgen(n, &nod);
		gext(&nod);
		if(nn == Z) {
			o = true? OEQ : ONE;
			gopcode(o, zero(n), &nod, Z);
			regfree(&nod);
			break;
		}
		o = true? ONE : OEQ;
		nod1 = regnode;
		nod1.type = types[TINT];
		regalloc(&nod2, &nod1, nn);
		gopcode(o, zero(n), &nod, &nod2);
		regfree(&nod);
		gmove(&nod2, nn);
		regfree(&nod2);
		break;

	case OCONST:
		o = vconst(n);
		if(!true)
			o = !o;
		gbranch(OGOTO);
		if(o) {
			p1 = p;
			gbranch(OGOTO);
			patch(p1, pc);
		}
		goto com;

	case OCOMMA:
		cgen(l, Z);
		boolgen(r, true, nn);
		break;

	case ONOT:
		boolgen(l, !true, nn);
		break;

	case OCOND:
		bcgen(l, 1);
		p1 = p;
		bcgen(r->left, true);
		p2 = p;
		gbranch(OGOTO);
		patch(p1, pc);
		p1 = p;
		bcgen(r->right, !true);
		patch(p2, pc);
		p2 = p;
		gbranch(OGOTO);
		patch(p1, pc);
		patch(p2, pc);
		goto com;

	case OANDAND:
		if(!true)
			goto caseor;

	caseand:
		bcgen(l, true);
		p1 = p;
		bcgen(r, !true);
		p2 = p;
		patch(p1, pc);
		gbranch(OGOTO);
		patch(p2, pc);
		goto com;

	case OOROR:
		if(!true)
			goto caseand;

	caseor:
		bcgen(l, !true);
		p1 = p;
		bcgen(r, !true);
		p2 = p;
		gbranch(OGOTO);
		patch(p1, pc);
		patch(p2, pc);
		goto com;

	case OEQ:
	case ONE:
	case OLE:
	case OLT:
	case OGE:
	case OGT:
	case OHI:
	case OHS:
	case OLO:
	case OLS:
		o = n->op;
		if(l->complex >= FNX && r->complex >= FNX) {
			regret(&nod, r);
			cgen(r, &nod);
			regsalloc(&nod1, r);
			gopcode(OAS, &nod, Z, &nod1);
			regfree(&nod);
			nod = *n;
			nod.right = &nod1;
			boolgen(&nod, true, nn);
			break;
		}
		fp = typefd[l->type->etype];
		if(!fp) {
			if(nn != Z) {
				if(!true)
					o = comrel[relindex(o)];
			} else {
				if(true)
					o = comrel[relindex(o)];
			}
		}
		if(r->op == OCONST) {
			regalloc(&nod1, l, Z);
			cgen(l, &nod1);
			nod = *r;
		} else if(l->complex >= r->complex) {
			regalloc(&nod1, l, Z);
			cgen(l, &nod1);
			regalloc(&nod, r, Z);
			cgen(r, &nod);
		} else {
			regalloc(&nod, r, Z);
			cgen(r, &nod);
			regalloc(&nod1, l, Z);
			cgen(l, &nod1);
		}
		if(nn != Z || fp) {
			nod2 = regnode;
			nod2.type = types[TINT];
			regalloc(&nod2, &nod2, nn);
			gopcode(o, &nod, &nod1, &nod2);
			if(nn != Z) {
				if(fp && !true)
					gopcode(OXOR, nodconst(1), Z, &nod2);
				gmove(&nod2, nn);
			} else
				gopcode(true ? OEQ : ONE, nodconst(0), &nod2, Z);
			regfree(&nod2);
		} else
			gopcode(o, &nod, &nod1, Z);
		if(r->op != OCONST)
			regfree(&nod);
		regfree(&nod1);
		break;

	com:
		if(nn != Z) {
			p1 = p;
			gopcode(OAS, nodconst(1), Z, nn);
			gbranch(OGOTO);
			p2 = p;
			patch(p1, pc);
			gopcode(OAS, nodconst(0), Z, nn);
			patch(p2, pc);
		}
		break;
	}
	cursafe = curs;
}

void
sugen(Node *n, Node *nn, long w)
{
	Prog *p1;
	Node nod0, nod1, nod2, nod3, nod4, *l, *r;
	Type *t;

	if(n == Z || n->type == T)
		return;
	if(debug['g']) {
		prtree(nn, "sugen lhs");
		prtree(n, "sugen");
	}
	if(nn == nodrat)
		if(w > nrathole)
			nrathole = w;
	switch(n->op) {
	case OIND:
		if(nn == Z) {
			nullwarn(n->left, Z);
			break;
		}

	default:
		goto copy;

	case ODOT:
		l = n->left;
		sugen(l, nodrat, l->type->width);
		if(nn != Z) {
			warn(n, "non-interruptable temporary");
			nod1 = *nodrat;
			r = n->right;
			if(!r || r->op != OCONST) {
				diag(n, "DOT and no offset");
				break;
			}
			nod1.xoffset += (long)r->vconst;
			nod1.type = n->type;
			sugen(&nod1, nn, w);
		}
		break;

	case OSTRUCT:
		if(nn != Z && nn->complex >= FNX) {
			nod1 = *n;
			nod1.type = typ(TIND, n->type);
			regret(&nod2, &nod1);
			lcgen(nn, &nod2);
			regsalloc(&nod0, &nod1);
			gopcode(OAS, &nod2, Z, &nod0);
			regfree(&nod2);

			nod1 = *n;
			nod1.op = OIND;
			nod1.left = &nod0;
			nod1.right = Z;
			nod1.complex = 1;

			sugen(n, &nod1, w);
			return;
		}

		r = n->left;
		for(t = n->type->link; t != T; t = t->down) {
			if(t->nbits && t->sym == S)
				continue;
			l = r;
			if(r->op == OLIST) {
				l = r->left;
				r = r->right;
			}
			if(nn == Z) {
				cgen(l, nn);
				continue;
			}
			nod0 = znode;
			nod0.op = OAS;
			nod0.type = t;
			nod0.left = &nod1;
			nod0.right = l;

			nod1 = znode;
			nod1.op = OIND;
			nod1.type = t;
			nod1.left = &nod2;

			nod2 = znode;
			nod2.op = OADD;
			nod2.type = typ(TIND, t);
			nod2.left = &nod3;
			nod2.right = &nod4;

			nod3 = znode;
			nod3.op = OADDR;
			nod3.type = nod2.type;
			nod3.left = nn;

			nod4 = znode;
			nod4.op = OCONST;
			nod4.type = nod2.type;
			nod4.vconst = t->offset;

			ccom(&nod0);
			acom(&nod0);
			xcom(&nod0);
			nod0.addable = 0;

			cgen(&nod0, Z);
		}
		break;

	case OAS:
		if(nn == Z) {
			if(n->addable < INDEXED)
				sugen(n->right, n->left, w);
			break;
		}
		sugen(n->right, nodrat, w);
		warn(n, "non-interruptable temporary");
		sugen(nodrat, n->left, w);
		sugen(nodrat, nn, w);
		break;

	case OFUNC:
		if(nn == Z) {
			sugen(n, nodrat, w);
			break;
		}
		if(nn->op != OIND) {
			nn = new1(OADDR, nn, Z);
			nn->type = types[TIND];
			nn->addable = 0;
		} else
			nn = nn->left;
		gcall(n, Z, nn);
		break;

	case OCOND:
		bcgen(n->left, 1);
		p1 = p;
		sugen(n->right->left, nn, w);
		gbranch(OGOTO);
		patch(p1, pc);
		p1 = p;
		sugen(n->right->right, nn, w);
		patch(p1, pc);
		break;

	case OCOMMA:
		cgen(n->left, Z);
		sugen(n->right, nn, w);
		break;
	}
	return;

copy:
	if(nn == Z)
		return;
	if(n->complex >= FNX && nn->complex >= FNX) {
		t = nn->type;
		nn->type = types[TLONG];
		regialloc(&nod1, nn, Z);
		lcgen(nn, &nod1);
		regsalloc(&nod2, nn);
		nn->type = t;

		gopcode(OAS, &nod1, Z, &nod2);
		regfree(&nod1);

		nod2.type = typ(TIND, t);

		nod1 = nod2;
		nod1.op = OIND;
		nod1.left = &nod2;
		nod1.right = Z;
		nod1.complex = 1;
		nod1.type = t;

		sugen(n, &nod1, w);
		return;
	}

	if(n->complex > nn->complex) {
		regialloc(&nod1, n, Z);
		lcgen(n, &nod1);
		regialloc(&nod2, nn, Z);
		lcgen(nn, &nod2);
	} else {
		regialloc(&nod2, nn, Z);
		lcgen(nn, &nod2);
		regialloc(&nod1, n, Z);
		lcgen(n, &nod1);
	}
	gnilreg(ACHKRD, nod1.reg);
	gnilreg(ACHKWR, nod2.reg);
	nextpc();
	p->as = ACOPY;
	p->from.type = D_CONST;
	p->from.offset = w;
	p->reg = nod1.reg;
	p->to.type = D_REG;
	p->to.reg = nod2.reg;
	regfree(&nod1);
	regfree(&nod2);
}
