#include	"l.h"

static	Sym**	imports;
static	int	nimport;
static	Sym**	funcs;
static	int	nfunc;
static	Sym**	table;
static	int	ntable;
static	long	database;
static	long	dataend;
static	long	bssend;
static	long	heapbase;
static	int	tagsig = -1;

void
bput(Buf *b, int c)
{
	if(b->n >= b->cap) {
		b->cap = b->cap ? 2*b->cap : 1024;
		b->p = realloc(b->p, b->cap);
	}
	b->p[b->n++] = c;
}

void
bputs(Buf *b, void *v, long n)
{
	uchar *p;

	p = v;
	while(n-- > 0)
		bput(b, *p++);
}

void
buleb(Buf *b, uvlong v)
{
	int c;

	do {
		c = v & 0x7f;
		v >>= 7;
		if(v)
			c |= 0x80;
		bput(b, c);
	} while(v);
}

void
bsleb(Buf *b, vlong v)
{
	int c, more;

	more = 1;
	while(more) {
		c = v & 0x7f;
		v >>= 7;
		if((v == 0 && !(c & 0x40)) || (v == -1 && (c & 0x40)))
			more = 0;
		else
			c |= 0x80;
		bput(b, c);
	}
}

void
bname(Buf *b, char *s)
{
	buleb(b, strlen(s));
	bputs(b, s, strlen(s));
}

void
bvec(Buf *b, Buf *v)
{
	buleb(b, v->n);
	bputs(b, v->p, v->n);
}

void
bfree(Buf *b)
{
	free(b->p);
	b->p = nil;
	b->n = 0;
	b->cap = 0;
}

int
sigof(Sig *s)
{
	int i;

	for(i=0; i<nsig; i++)
		if(sigs[i].np == s->np && sigs[i].r == s->r && memcmp(sigs[i].p, s->p, s->np) == 0)
			return i;
	sigs = realloc(sigs, (nsig+1)*sizeof(Sig));
	sigs[nsig] = *s;
	return nsig++;
}

void
funcsig(Sym *s, Sig *sg)
{
	Prog *p;

	memset(sg, 0, sizeof *sg);
	for(p = s->text->link; p != P; p = p->link) {
		if(p->as == APARAM) {
			if(sg->np >= NARG) {
				diag("%s: too many parameters", s->name);
				break;
			}
			sg->p[sg->np++] = p->from.offset;
			continue;
		}
		if(p->as == ARESULT) {
			sg->r = p->from.offset;
			s->pframe = p->to.offset;
			break;
		}
	}
}

static int
kindof(Prog *p)
{
	int r;

	if(p->as == AARGF)
		return KF32;
	r = p->from.reg;
	if(r < VREG)
		return KI32;
	if(r < FREG)
		return KI64;
	return KF64;
}

static void
scancalls(Sym *s, void (*f)(Sym*, Prog*, Sig*))
{
	Prog *p;
	Sig sg;

	memset(&sg, 0, sizeof sg);
	for(p = s->text; p != P; p = p->link) {
		if(p->as == AARG || p->as == AARGF) {
			if(sg.np >= NARG) {
				diag("%s: too many arguments", s->name);
				continue;
			}
			sg.p[sg.np++] = kindof(p);
			continue;
		}
		if(p->as == ACALL) {
			sg.r = p->from.offset;
			(*f)(s, p, &sg);
		}
		sg.np = 0;
	}
}

static void
impsig(Sym *s, Prog *p, Sig *sg)
{
	Sym *t;
	int i;

	p->sig = sigof(sg);
	t = p->to.sym;
	if(t == S || t->type != SIMPORT)
		return;
	i = sigof(sg);
	if(t->sig < 0) {
		t->sig = i;
		imports = realloc(imports, (nimport+1)*sizeof(Sym*));
		t->fidx = nimport;
		imports[nimport++] = t;
	}
	else if(t->sig != i)
		diag("%s: call of %s: signature differs from an earlier call", s->name, t->name);
}

static void
callsigs(Sym *s, Prog *p, Sig *sg)
{
	USED(s);
	p->sig = sigof(sg);
}

static void
addtab(Sym *s)
{
	if(s == S || s->type != STEXT && s->type != SIMPORT || s->tidx >= 0)
		return;
	table = realloc(table, (ntable+1)*sizeof(Sym*));
	table[ntable] = s;
	s->tidx = ++ntable;
}

static long
dalign(long size)
{
	if(size >= 8)
		return 8;
	if(size >= 4)
		return 4;
	if(size >= 2)
		return 2;
	return 1;
}

void
layout(void)
{
	Sym *s;
	Prog *p;
	Sig sg;
	long v;
	int i;

	for(s = textp; s != S; s = s->next) {
		if(!s->reach)
			continue;
		funcsig(s, &sg);
		s->sig = sigof(&sg);
		scancalls(s, impsig);
	}
	for(i=0; i<NHASH; i++)
	for(s = hash[i]; s != S; s = s->hash)
		if(s->reach && s->type == SIMPORT && s->sig < 0)
			diag("import %s is never called", s->name);
	for(s = textp; s != S; s = s->next) {
		if(!s->reach)
			continue;
		funcs = realloc(funcs, (nfunc+1)*sizeof(Sym*));
		s->fidx = nimport + nfunc;
		funcs[nfunc++] = s;
	}
	for(s = textp; s != S; s = s->next) {
		if(!s->reach)
			continue;
		for(p = s->text; p != P; p = p->link) {
			if(p->as == ACALL)
				continue;
			if(p->from.type == D_CONST)
				addtab(p->from.sym);
			if(p->to.type == D_CONST)
				addtab(p->to.sym);
		}
	}
	if(lookup("wasm·throw", 0)->reach || lookup("setjmp", 0)->reach) {
		memset(&sg, 0, sizeof sg);
		sg.np = 1;
		sg.p[0] = KI32;
		tagsig = sigof(&sg);
	}
	v = stacksize;
	database = v;
	for(s = datap; s != S; s = s->next) {
		if(!s->reach || s->type != SDATA)
			continue;
		if(s->size == 0)
			diag("%s: data without size", s->name);
		v = (v + dalign(s->size) - 1) & ~(dalign(s->size) - 1);
		s->value = v;
		v += s->size;
		for(p = s->data; p != P; p = p->link)
			if(p->to.type == D_CONST)
				addtab(p->to.sym);
	}
	dataend = v;
	for(s = datap; s != S; s = s->next) {
		if(!s->reach || s->type != SBSS)
			continue;
		v = (v + dalign(s->size) - 1) & ~(dalign(s->size) - 1);
		s->value = v;
		v += s->size;
	}
	bssend = v;
	heapbase = (v + 15) & ~15;
}

long
symaddr(Sym *s)
{
	switch(s->type) {
	case STEXT:
	case SIMPORT:
		if(s->tidx < 0)
			diag("%s: address of function not in table", s->name);
		return s->tidx;
	case SDATA:
	case SBSS:
		return s->value;
	case SLINK:
		if(strcmp(s->name, "__heap_base") == 0 || strcmp(s->name, "end") == 0)
			return heapbase;
		if(strcmp(s->name, "__stack_high") == 0)
			return stacksize;
		if(strcmp(s->name, "__stack_low") == 0)
			return 0;
		if(strcmp(s->name, "__data_end") == 0 || strcmp(s->name, "edata") == 0)
			return dataend;
		return 0;
	}
	diag("address of undefined %s", s->name);
	return 0;
}

static void
section(Buf *m, int id, Buf *b)
{
	bput(m, id);
	bvec(m, b);
	bfree(b);
}

static int
vt(int k)
{
	switch(k) {
	case KI64:
		return 0x7e;
	case KF32:
		return 0x7d;
	case KF64:
		return 0x7c;
	}
	return 0x7f;
}

static void
expfunc(Buf *b, char *name, int idx)
{
	bname(b, name);
	bput(b, 0x00);
	buleb(b, idx);
}

static void
putdata(uchar *mem, Sym *s)
{
	Prog *p;
	long off, w, i;
	vlong v;
	double d;
	float f;
	u32int u;
	uchar *q;

	for(p = s->data; p != P; p = p->link) {
		off = p->from.offset;
		w = p->reg;
		if(off < 0 || off + w > s->size) {
			diag("%s: data out of range", s->name);
			continue;
		}
		q = mem + (s->value - database) + off;
		switch(p->to.type) {
		case D_SCONST:
			memmove(q, p->to.sval, w);
			continue;
		case D_FCONST:
			v = p->to.offset;
			if(w == 4) {
				memmove(&d, &v, 8);
				f = d;
				memmove(&u, &f, 4);
				v = u;
			}
			break;
		case D_CONST:
			v = p->to.offset;
			if(p->to.sym != S)
				v += symaddr(p->to.sym);
			break;
		case D_VCONST:
			v = p->to.offset;
			break;
		default:
			diag("%s: bad data type %d", s->name, p->to.type);
			continue;
		}
		for(i=0; i<w; i++)
			q[i] = v >> (8*i);
	}
}

static void
names(Buf *m)
{
	Buf b, f;
	int i;

	memset(&b, 0, sizeof b);
	memset(&f, 0, sizeof f);
	bname(&b, "name");
	buleb(&f, nimport + nfunc);
	for(i=0; i<nimport; i++) {
		buleb(&f, imports[i]->fidx);
		bname(&f, imports[i]->name);
	}
	for(i=0; i<nfunc; i++) {
		buleb(&f, funcs[i]->fidx);
		bname(&f, funcs[i]->name);
	}
	bput(&b, 1);
	bvec(&b, &f);
	bfree(&f);
	section(m, 0, &b);
}

static void
features(Buf *m)
{
	static char *f[] = {
		"mutable-globals", "nontrapping-fptoint", "bulk-memory", "bulk-memory-opt", "sign-ext", "multivalue", "call-indirect-overlong", "exception-handling",
	};
	Buf b;
	int i;

	memset(&b, 0, sizeof b);
	bname(&b, "target_features");
	buleb(&b, nelem(f));
	for(i=0; i<nelem(f); i++) {
		bput(&b, '+');
		bname(&b, f[i]);
	}
	section(m, 0, &b);
}

void
asmb(void)
{
	Buf m, b, c, f;
	Sym *s;
	int i, j, fd;
	char *mod, *field;
	uchar *mem;
	Sig *sg;

	memset(&m, 0, sizeof m);
	memset(&b, 0, sizeof b);
	memset(&c, 0, sizeof c);
	memset(&f, 0, sizeof f);
	bputs(&m, "\0asm\1\0\0\0", 8);

	for(i=0; i<nfunc; i++)
		scancalls(funcs[i], callsigs);

	for(i=0; i<nfunc; i++) {
		gencode(funcs[i], &f);
		bvec(&c, &f);
		bfree(&f);
	}
	if(nerrors)
		return;

	buleb(&b, nsig);
	for(i=0; i<nsig; i++) {
		sg = &sigs[i];
		bput(&b, 0x60);
		buleb(&b, sg->np);
		for(j=0; j<sg->np; j++)
			bput(&b, vt(sg->p[j]));
		if(sg->r == KNONE)
			buleb(&b, 0);
		else {
			buleb(&b, 1);
			bput(&b, vt(sg->r));
		}
	}
	section(&m, 1, &b);

	buleb(&b, nimport);
	for(i=0; i<nimport; i++) {
		s = imports[i];
		mod = impmod(s, &field);
		bname(&b, mod);
		bname(&b, field);
		bput(&b, 0x00);
		buleb(&b, s->sig);
		free(mod);
	}
	section(&m, 2, &b);

	buleb(&b, nfunc);
	for(i=0; i<nfunc; i++)
		buleb(&b, funcs[i]->sig);
	section(&m, 3, &b);

	buleb(&b, 1);
	bput(&b, 0x70);
	bput(&b, 0x01);
	buleb(&b, ntable+1);
	buleb(&b, ntable+1);
	section(&m, 4, &b);

	buleb(&b, 1);
	bput(&b, 0x00);
	buleb(&b, (heapbase + PAGE - 1) / PAGE);
	section(&m, 5, &b);

	if(tagsig >= 0) {
		buleb(&b, 1);
		bput(&b, 0x00);
		buleb(&b, tagsig);
		section(&m, 13, &b);
	}

	buleb(&b, 2);
	bput(&b, 0x7f);
	bput(&b, 0x01);
	bput(&b, 0x41);
	bsleb(&b, stacksize);
	bput(&b, 0x0b);
	bput(&b, 0x7f);
	bput(&b, 0x00);
	bput(&b, 0x41);
	bsleb(&b, heapbase);
	bput(&b, 0x0b);
	section(&m, 6, &b);

	j = 3;
	for(i=0; i<nfunc; i++)
		if(isexport(funcs[i]))
			j++;
	buleb(&b, j);
	bname(&b, "memory");
	bput(&b, 0x02);
	buleb(&b, 0);
	bname(&b, "__stack_pointer");
	bput(&b, 0x03);
	buleb(&b, 0);
	bname(&b, "__heap_base");
	bput(&b, 0x03);
	buleb(&b, 1);
	for(i=0; i<nfunc; i++)
		if(isexport(funcs[i]))
			expfunc(&b, strcmp(funcs[i]->name, entry) == 0 ? "_start" : funcs[i]->name, funcs[i]->fidx);
	section(&m, 7, &b);

	if(ntable > 0) {
		buleb(&b, 1);
		buleb(&b, 0);
		bput(&b, 0x41);
		bsleb(&b, 1);
		bput(&b, 0x0b);
		buleb(&b, ntable);
		for(i=0; i<ntable; i++)
			buleb(&b, table[i]->fidx);
		section(&m, 9, &b);
	}

	buleb(&b, nfunc);
	bputs(&b, c.p, c.n);
	bfree(&c);
	section(&m, 10, &b);

	if(dataend > database) {
		mem = mallocz(dataend - database, 1);
		for(s = datap; s != S; s = s->next)
			if(s->reach && s->type == SDATA)
				putdata(mem, s);
		buleb(&b, 1);
		buleb(&b, 0);
		bput(&b, 0x41);
		bsleb(&b, database);
		bput(&b, 0x0b);
		buleb(&b, dataend - database);
		bputs(&b, mem, dataend - database);
		free(mem);
		section(&m, 11, &b);
	}
	if(!debug['s'])
		names(&m);
	features(&m);
	if(nerrors)
		return;

	fd = create(outfile, OWRITE, 0775);
	if(fd < 0) {
		diag("cannot create %s: %r", outfile);
		return;
	}
	if(write(fd, m.p, m.n) != m.n)
		diag("write error: %r");
	close(fd);
}
