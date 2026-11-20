#include "gc.h"

int
veccap(int o, int et)
{
	switch(o) {
	case OVLOAD:
	case OVRLOAD:
		switch(et) {
		case TCHAR:
		case TUCHAR:
		case TSHORT:
		case TUSHORT:
		case TINT:
		case TUINT:
		case TLONG:
		case TULONG:
			return 1;
		}
		return 0;
	case OVSPLAT:
	case OVSUM:
	case OAS:
	case OADD:
	case OSUB:
	case OMUL:
	case OAND:
	case OOR:
	case OXOR:
	case ONEG:
	case OCOM:
		switch(et) {
		case TINT:
		case TUINT:
		case TLONG:
		case TULONG:
			return 1;
		}
		return 0;
	}
	return 0;
}

static int
vldop(int o, int et)
{
	USED(o);
	switch(et) {
	case TCHAR:
		return AVLOADB;
	case TUCHAR:
		return AVLOADBU;
	case TSHORT:
		return AVLOADH;
	case TUSHORT:
		return AVLOADHU;
	}
	return AVLOADW;
}

static int
vbinop(int o)
{
	switch(o) {
	case OADD:
		return AVADD;
	case OSUB:
		return AVSUB;
	case OMUL:
		return AVMUL;
	case OAND:
		return AVAND;
	case OOR:
		return AVOR;
	case OXOR:
		return AVXOR;
	}
	diag(Z, "bad vector op %O", o);
	return AGOK;
}

void
vcgen(Node *n, Node *nn)
{
	Node *l, *r, nod, nod1;

	l = n->left;
	r = n->right;
	if(n->op == OAS) {
		regalloc(&nod, r, Z);
		cgen(r, &nod);
		gmove(&nod, l);
		if(nn != Z)
			gmove(&nod, nn);
		regfree(&nod);
		return;
	}
	if(nn == Z) {
		nullwarn(l, r);
		return;
	}
	if(nn->op != OREGISTER || tclass(nn->type) != tclass(n->type)) {
		regalloc(&nod, n, nn);
		vcgen(n, &nod);
		gmove(&nod, nn);
		regfree(&nod);
		return;
	}
	switch(n->op) {
	default:
		diag(n, "unknown vector op %O", n->op);
		break;
	case OVLOAD:
	case OVRLOAD:
		regalloc(&nod, l, Z);
		cgen(l, &nod);
		nod1 = nod;
		nod1.op = OINDREG;
		nod1.xoffset = 0;
		gins(vldop(n->op, l->type->link->etype), &nod1, nn);
		if(n->op == OVRLOAD)
			gins(AVREV, nn, nn);
		regfree(&nod);
		break;
	case OVSPLAT:
		regalloc(&nod, l, Z);
		cgen(l, &nod);
		gins(AVSPLAT, &nod, nn);
		regfree(&nod);
		break;
	case OVSUM:
		regalloc(&nod, l, Z);
		cgen(l, &nod);
		gins(AVSUM, &nod, nn);
		regfree(&nod);
		break;
	case ONEG:
	case OCOM:
		cgen(l, nn);
		gins(n->op == ONEG ? AVNEG : AVNOT, nn, nn);
		break;
	case OADD:
	case OSUB:
	case OMUL:
	case OAND:
	case OOR:
	case OXOR:
		if(l->complex >= r->complex) {
			cgen(l, nn);
			regalloc(&nod, r, Z);
			cgen(r, &nod);
		} else {
			regalloc(&nod, r, Z);
			cgen(r, &nod);
			cgen(l, nn);
		}
		gins(vbinop(n->op), &nod, nn);
		p->reg = nn->reg;
		regfree(&nod);
		break;
	}
}
