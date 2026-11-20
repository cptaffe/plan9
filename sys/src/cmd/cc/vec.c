#include	"cc.h"

enum
{
	NLD	= 8,
	NINV	= 8,
	NTMP	= 64,
};

typedef struct Vld Vld;
struct Vld
{
	Node*	n;
	Node*	base;
	Node*	idx;
	Type*	elem;
	int	d;
	int	rev;
	Node*	p;
};

static	Node*	fnbody;
static	Node*	iv;
static	Node*	acc;
static	int	step;
static	Vld	ld[NLD];
static	int	nld;
static	Node*	inv[NINV];
static	Node*	vinv[NINV];
static	int	ninv;
static	Node*	tmp[NTMP];
static	int	busy[NTMP];
static	int	ntmp;
static	Type*	vt;

static int
vsame(Node *a, Node *b)
{
	return a != Z && b != Z && a->op == ONAME && b->op == ONAME &&
		a->sym == b->sym && a->class == b->class && a->xoffset == b->xoffset;
}

static int
vuses(Node *n, Node *v)
{
	if(n == Z)
		return 0;
	if(vsame(n, v))
		return 1;
	return vuses(n->left, v) || vuses(n->right, v);
}

static int
vaddr(Node *n, Node *v)
{
	if(n == Z)
		return 0;
	if(n->op == OADDR && vsame(n->left, v))
		return 1;
	return vaddr(n->left, v) || vaddr(n->right, v);
}

static int
vvol(Type *t)
{
	return t == T || (t->garb & GVOLATILE) != 0;
}

static int
vint(Type *t)
{
	return !vvol(t) && typechl[t->etype] && t->width <= types[TUINT]->width;
}

static int
vword(Type *t)
{
	return vint(t) && t->width == types[TUINT]->width;
}

static int
vlocal(Node *n)
{
	if(n == Z || n->op != ONAME || !vword(n->type))
		return 0;
	if(n->class != CAUTO && n->class != CPARAM)
		return 0;
	return !vaddr(fnbody, n);
}

static Node*
vcopy(Node *n)
{
	Node *c;

	if(n == Z)
		return Z;
	c = new1(OXXX, Z, Z);
	*c = *n;
	c->left = vcopy(n->left);
	c->right = vcopy(n->right);
	return c;
}

static Type*
vptr(Type *t)
{
	if(t == T || vvol(t))
		return T;
	if(t->etype == TIND || t->etype == TARRAY)
		return t;
	return T;
}

static Type*
vtype(Node *n)
{
	Type *a, *b, *t;

	if(n == Z)
		return T;
	switch(n->op) {
	case ONAME:
		if(n->type == T || n->type->etype == TFUNC || n->type->etype == TENUM)
			return T;
		return n->type;
	case OCONST:
		return n->type;
	case OCAST:
		if(vtype(n->left) == T)
			return T;
		return n->type;
	case OPOS:
	case ONEG:
	case OCOM:
		a = vtype(n->left);
		if(a == T || !vint(a))
			return T;
		return types[TINT];
	case OADD:
		a = vtype(n->left);
		b = vtype(n->right);
		if(a == T || b == T)
			return T;
		if(vptr(a) != T && vint(b))
			return a;
		if(vptr(b) != T && vint(a))
			return b;
		if(!vint(a) || !vint(b))
			return T;
		return types[TINT];
	case OSUB:
	case OMUL:
	case ODIV:
	case OMOD:
	case OAND:
	case OOR:
	case OXOR:
	case OASHL:
	case OASHR:
		a = vtype(n->left);
		b = vtype(n->right);
		if(a == T || b == T || !vint(a) || !vint(b))
			return T;
		return types[TINT];
	case OIND:
		a = vptr(vtype(n->left));
		if(a == T)
			return T;
		return a->link;
	case ODOT:
		a = vtype(n->left);
		if(a == T || vvol(a) || (a->etype != TSTRUCT && a->etype != TUNION))
			return T;
		for(t = a->link; t != T; t = t->down)
			if(t->sym != S && t->sym == n->sym && t->nbits == 0)
				return t;
		return T;
	}
	return T;
}

static int
vinval(Node *n)
{
	Type *t;

	if(vuses(n, iv) || vuses(n, acc))
		return 0;
	t = vtype(n);
	return t != T && vint(t);
}

static int
vlin(Node *n, int *k)
{
	int a, b;

	if(n == Z)
		return 0;
	if(!vuses(n, iv)) {
		*k = 0;
		return vinval(n);
	}
	switch(n->op) {
	case ONAME:
		*k = 1;
		return 1;
	case OPOS:
		return vlin(n->left, k);
	case OCAST:
		return vword(n->type) && vlin(n->left, k);
	case ONEG:
		if(!vlin(n->left, &a))
			return 0;
		*k = -a;
		return 1;
	case OADD:
	case OSUB:
		if(!vlin(n->left, &a) || !vlin(n->right, &b))
			return 0;
		*k = n->op == OADD ? a+b : a-b;
		return 1;
	}
	return 0;
}

static int
vload(Node *n)
{
	Node *c, *b, *x;
	Type *t;
	Vld *l;
	int k;

	c = n->left;
	if(c == Z || c->op != OADD || nld >= NLD)
		return 0;
	b = c->left;
	x = c->right;
	if(b->op != ONAME || vptr(b->type) == T) {
		b = c->right;
		x = c->left;
	}
	if(b->op != ONAME || vptr(b->type) == T || vuses(b, iv) || vuses(b, acc))
		return 0;
	t = b->type->link;
	if(t == T || !vint(t))
		return 0;
	if(!vlin(x, &k) || (k != 1 && k != -1))
		return 0;
	l = &ld[nld++];
	l->n = n;
	l->base = b;
	l->idx = x;
	l->elem = t;
	l->d = k*step;
	return 1;
}

static int
vexpr(Node *n)
{
	if(n == Z)
		return 0;
	if(!vuses(n, iv)) {
		if(ninv >= NINV || !vinval(n) || !veccap(OVSPLAT, TUINT))
			return 0;
		inv[ninv++] = n;
		return 1;
	}
	switch(n->op) {
	case OIND:
		return vload(n);
	case OPOS:
		return vexpr(n->left);
	case OCAST:
		return vword(n->type) && vexpr(n->left);
	case ONEG:
	case OCOM:
		return veccap(n->op, TUINT) && vexpr(n->left);
	case OADD:
	case OSUB:
	case OMUL:
	case OAND:
	case OOR:
	case OXOR:
		return veccap(n->op, TUINT) && vexpr(n->left) && vexpr(n->right);
	}
	return 0;
}

static int
vstep(Node *n)
{
	if(n == Z)
		return 0;
	iv = n->left;
	switch(n->op) {
	case OPOSTINC:
	case OPREINC:
		return 1;
	case OPOSTDEC:
	case OPREDEC:
		return -1;
	case OASADD:
	case OASSUB:
		if(n->right == Z || n->right->op != OCONST || n->right->vconst != 1 || !vint(n->right->type))
			return 0;
		return n->op == OASADD ? 1 : -1;
	}
	return 0;
}

static int
vsamet(Type *a, Type *b)
{
	if(a->etype != b->etype || a->width != b->width || a->garb != b->garb)
		return 0;
	if(a->etype == TIND)
		return vsamet(a->link, b->link);
	return 1;
}

static Node*
vtemp(Type *t)
{
	Node *n;
	long o;
	int i;
	char buf[16];

	for(i=0; i<ntmp; i++)
		if(!busy[i] && vsamet(tmp[i]->type, t)) {
			busy[i] = 1;
			return tmp[i];
		}
	if(ntmp >= NTMP)
		return Z;
	snprint(buf, sizeof buf, ".v%d", ntmp);
	n = new(ONAME, Z, Z);
	n->sym = slookup(buf);
	n->type = t;
	n->etype = t->etype;
	n->class = CAUTO;
	o = align(stkoff, t, Aaut3);
	stkoff = maxround(stkoff, o);
	n->xoffset = -o;
	busy[ntmp] = 1;
	tmp[ntmp++] = n;
	return n;
}

static Node*
vcon(long v)
{
	Node *n;

	n = new(OCONST, Z, Z);
	n->type = types[TINT];
	n->vconst = v;
	return n;
}

static Node*
vcast(Node *n, int et)
{
	Node *c;

	c = new(OCAST, n, Z);
	c->type = types[et];
	return c;
}

static Node*
vnode(int o, Node *l, Node *r)
{
	Node *n;

	n = new(o, l, r);
	n->type = vt;
	return n;
}

static Node*
vtree(Node *n)
{
	int i;

	for(i=0; i<ninv; i++)
		if(inv[i] == n)
			return vcopy(vinv[i]);
	switch(n->op) {
	case OIND:
		for(i=0; i<nld; i++)
			if(ld[i].n == n)
				break;
		return vnode(ld[i].rev ? OVRLOAD : OVLOAD, vcopy(ld[i].p), Z);
	case OPOS:
	case OCAST:
		return vtree(n->left);
	case ONEG:
	case OCOM:
		return vnode(n->op, vtree(n->left), Z);
	}
	return vnode(n->op, vtree(n->left), vtree(n->right));
}

static Node*
vlist(Node *a, Node *b)
{
	if(a == Z)
		return b;
	if(b == Z)
		return a;
	return new(OLIST, a, b);
}

static int
vfor(Node *n)
{
	Node *l, *t, *hi, *e, *b, *c, *tn, *tc, *va, *s, *body, *rem;
	int i, o, sub, fwd, lanes, lg;
	long ln;

	l = n->left;
	t = l->left;
	step = vstep(l->right->right);
	if(step == 0 || !vlocal(iv) || t == Z)
		return 0;
	switch(t->op) {
	default:
		return 0;
	case OLT:
	case OLE:
	case OGT:
	case OGE:
		break;
	}
	o = t->op;
	hi = t->right;
	if(!vsame(t->left, iv)) {
		if(!vsame(t->right, iv))
			return 0;
		hi = t->left;
		switch(o) {
		case OLT: o = OGT; break;
		case OLE: o = OGE; break;
		case OGT: o = OLT; break;
		case OGE: o = OLE; break;
		}
	}
	if(step > 0 && o != OLT && o != OLE)
		return 0;
	if(step < 0 && o != OGT && o != OGE)
		return 0;
	b = n->right;
	while(b != Z && b->op == OLIST) {
		if(b->left == Z)
			b = b->right;
		else if(b->right == Z)
			b = b->left;
		else
			return 0;
	}
	if(b == Z)
		return 0;
	acc = b->left;
	e = b->right;
	sub = 0;
	switch(b->op) {
	default:
		return 0;
	case OASSUB:
		sub = 1;
	case OASADD:
		break;
	case OAS:
		if(e == Z)
			return 0;
		if(e->op == OADD && vsame(e->left, acc))
			e = e->right;
		else if(e->op == OADD && vsame(e->right, acc))
			e = e->left;
		else if(e->op == OSUB && vsame(e->left, acc)) {
			e = e->right;
			sub = 1;
		} else
			return 0;
		break;
	}
	if(!vlocal(acc) || vsame(acc, iv) || vuses(e, acc))
		return 0;
	lanes = vecwidth / types[TUINT]->width;
	for(lg = 0; (1<<lg) < lanes; lg++)
		;
	if(lanes < 2 || (1<<lg) != lanes)
		return 0;
	if(!veccap(OAS, TUINT) || !veccap(OVSUM, TUINT) || !veccap(OVSPLAT, TUINT) || !veccap(sub ? OSUB : OADD, TUINT))
		return 0;
	nld = 0;
	ninv = 0;
	if(!vinval(hi) || !vexpr(e) || nld == 0)
		return 0;
	fwd = 0;
	for(i=0; i<nld; i++)
		if(ld[i].d > 0)
			fwd = 1;
	for(i=0; i<nld; i++) {
		ld[i].rev = fwd && ld[i].d < 0;
		if(!veccap(ld[i].rev ? OVRLOAD : OVLOAD, ld[i].elem->etype))
			return 0;
	}

	if(ntmp + 3 + nld + ninv > NTMP)
		return 0;
	for(i=0; i<ntmp; i++)
		busy[i] = 0;
	ln = lineno;
	lineno = n->lineno;
	vt = typ(TVEC, types[TUINT]);
	vt->width = vecwidth;
	tn = vtemp(types[TUINT]);
	tc = vtemp(types[TUINT]);
	va = vtemp(vt);
	s = Z;
	for(i=0; i<nld; i++) {
		ld[i].p = vtemp(typ(TIND, ld[i].elem));
		c = new(OADDR, new(OIND, new(OADD, vcopy(ld[i].base), vcopy(ld[i].idx)), Z), Z);
		if(ld[i].d < 0)
			c = new(OSUB, c, vcon(lanes-1));
		s = vlist(s, new(OAS, vcopy(ld[i].p), c));
	}
	for(i=0; i<ninv; i++) {
		vinv[i] = vtemp(vt);
		s = vlist(s, vnode(OAS, vcopy(vinv[i]), vnode(OVSPLAT, vcast(vcopy(inv[i]), TUINT), Z)));
	}
	s = vlist(s, vnode(OAS, vcopy(va), vnode(OVSPLAT, vcast(vcon(0), TUINT), Z)));
	s = vlist(s, new(OAS, vcopy(tc), vcopy(tn)));
	body = vnode(OAS, vcopy(va), vnode(sub ? OSUB : OADD, vcopy(va), vtree(e)));
	for(i=0; i<nld; i++)
		body = vlist(body, new(ld[i].d > 0 ? OASADD : OASSUB, vcopy(ld[i].p), vcon(lanes)));
	s = vlist(s, new(ODWHILE, new(ONE, new(OPREDEC, vcopy(tc), Z), vcon(0)), body));
	s = vlist(s, new(OASADD, vcopy(acc), new(OVSUM, vcopy(va), Z)));
	s = vlist(s, new(step > 0 ? OASADD : OASSUB, vcopy(iv), new(OASHL, vcopy(tn), vcon(lg))));
	if(step > 0)
		c = new(OSUB, vcast(vcopy(hi), TUINT), vcast(vcopy(iv), TUINT));
	else
		c = new(OSUB, vcast(vcopy(iv), TUINT), vcast(vcopy(hi), TUINT));
	if(o == OLE || o == OGE)
		c = new(OADD, c, vcon(1));
	s = new(OIF, new(ONE, vcopy(tn), vcon(0)), new(OLIST, s, Z));
	s = vlist(new(OAS, vcopy(tn), new(OASHR, c, vcon(lg))), s);
	s = new(OIF, vcopy(t), new(OLIST, s, Z));
	rem = new(OFOR, new(OLIST, t, new(OLIST, Z, l->right->right)), n->right);
	rem->lineno = n->lineno;
	c = l->right->left;
	n->op = OLIST;
	n->left = c;
	n->right = new(OLIST, s, rem);
	if(n->left == Z) {
		n->left = s;
		n->right = rem;
	}
	lineno = ln;
	return 1;
}

static void
vwalk(Node *n)
{
	while(n != Z) {
		if(n->op == OFOR && vfor(n))
			return;
		switch(n->op) {
		case ONAME:
		case OCONST:
		case OSTRING:
		case OLSTRING:
			return;
		}
		vwalk(n->left);
		n = n->right;
	}
}

void
vecfn(Node *n)
{
	fnbody = n;
	ntmp = 0;
	vwalk(n);
}
