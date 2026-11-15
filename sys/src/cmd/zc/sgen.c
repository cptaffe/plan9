#include "gc.h"

void
noretval(int n)
{
	USED(n);
}

void
xcom(Node *n)
{
	Node *l, *r;
	int t;

	if(n == Z)
		return;
	l = n->left;
	r = n->right;
	n->addable = 0;
	n->complex = 0;
	switch(n->op) {
	case OCONST:
		n->addable = 20;
		return;

	case OREGISTER:
		n->addable = 11;
		return;

	case OINDREG:
		n->addable = 12;
		return;

	case ONAME:
		n->addable = 10;
		return;

	case OADDR:
		xcom(l);
		if(l->addable == 10)
			n->addable = 2;
		if(l->addable == 12)
			n->addable = 3;
		break;

	case OIND:
		xcom(l);
		if(l->addable == 11)
			n->addable = 12;
		if(l->addable == 3)
			n->addable = 12;
		if(l->addable == 2)
			n->addable = 10;
		break;

	case OADD:
		xcom(l);
		xcom(r);
		if(l->addable == 20) {
			if(r->addable == 2)
				n->addable = 2;
			if(r->addable == 3)
				n->addable = 3;
		}
		if(r->addable == 20) {
			if(l->addable == 2)
				n->addable = 2;
			if(l->addable == 3)
				n->addable = 3;
		}
		break;

	case OASLMUL:
	case OASMUL:
		xcom(l);
		xcom(r);
		t = vlog(r);
		if(t >= 0) {
			n->op = OASASHL;
			r->vconst = t;
			r->type = types[TINT];
			goto shiftassign;
		}
		break;

	case OMUL:
	case OLMUL:
		xcom(l);
		xcom(r);
		t = vlog(l);
		if(t >= 0) {
			n->left = r;
			n->right = l;
			l = r;
			r = n->right;
		}
		t = vlog(r);
		if(t >= 0) {
			n->op = OASHL;
			r->vconst = t;
			r->type = types[TINT];
			simplifyshift(n);
		}
		break;

	case OASLDIV:
		xcom(l);
		xcom(r);
		t = vlog(r);
		if(t >= 0) {
			n->op = OASLSHR;
			r->vconst = t;
			r->type = types[TINT];
			goto shiftassign;
		}
		break;

	case OLDIV:
		xcom(l);
		xcom(r);
		t = vlog(r);
		if(t >= 0) {
			n->op = OLSHR;
			r->vconst = t;
			r->type = types[TINT];
			simplifyshift(n);
		}
		break;

	case OASLMOD:
		xcom(l);
		xcom(r);
		t = vlog(r);
		if(t >= 0) {
			n->op = OASAND;
			r->vconst--;
		}
		break;

	case OLMOD:
		xcom(l);
		xcom(r);
		t = vlog(r);
		if(t >= 0) {
			n->op = OAND;
			r->vconst--;
		}
		break;

	case OLSHR:
	case OASHL:
	case OASHR:
		xcom(l);
		xcom(r);
		simplifyshift(n);
		break;

	case OASLSHR:
	case OASASHL:
	case OASASHR:
		xcom(l);
		xcom(r);
	shiftassign:
		if(typev[l->type->etype] && !machcap(n) && !typev[r->type->etype]){
			if(r->op == OCONST)
				r->type = types[TVLONG];
			else{
				n->right = r = new1(OCAST, r, Z);
				r->type = types[TVLONG];
				xcom(r);
			}
		}
		break;

	default:
		if(l != Z)
			xcom(l);
		if(r != Z)
			xcom(r);
		break;
	}
	if(n->addable >= 10)
		return;

	if(l != Z)
		n->complex = l->complex;
	if(r != Z) {
		if(r->complex == n->complex)
			n->complex = r->complex+1;
		else
		if(r->complex > n->complex)
			n->complex = r->complex;
	}
	if(n->complex == 0)
		n->complex++;

	switch(n->op) {
	case OFUNC:
		n->complex = FNX;
		break;

	case OADD:
	case OXOR:
	case OAND:
	case OOR:
	case OEQ:
	case ONE:
		if(l->op == OCONST) {
			n->left = r;
			n->right = l;
		}
		break;

	}
}
