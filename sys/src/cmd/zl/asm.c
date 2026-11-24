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
static	int	nsect;

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
	if(t == S || t->type != SIMPORT && t->type != SUNDEF)
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
	for(s = hash[i]; s != S; s = s->hash) {
		if(!s->reach || s->type != SUNDEF || s->sig >= 0 || s->decl == P)
			continue;
		memset(&sg, 0, sizeof sg);
		for(p = s->decl; p != P; p = p->link)
			if(p->as == APARAM && sg.np < NARG)
				sg.p[sg.np++] = p->from.offset;
			else if(p->as == ARESULT)
				sg.r = p->from.offset;
		s->sig = sigof(&sg);
		imports = realloc(imports, (nimport+1)*sizeof(Sym*));
		s->fidx = nimport;
		imports[nimport++] = s;
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
	for(s = textp; s != S && !rflag; s = s->next) {
		if(!s->reach)
			continue;
		for(p = s->text; p != P; p = p->link) {
			if(p->as == ACALL)
				continue;
			if(p->from.type == D_CONST && (p->from.name == D_EXTERN || p->from.name == D_STATIC))
				addtab(p->from.sym);
			if(p->to.type == D_CONST && (p->to.name == D_EXTERN || p->to.name == D_STATIC))
				addtab(p->to.sym);
		}
	}
	if(lookup("wasm·throw", 0)->reach || lookup("setjmp", 0)->reach) {
		memset(&sg, 0, sizeof sg);
		sg.np = 1;
		sg.p[0] = KI32;
		tagsig = sigof(&sg);
	}
	v = rflag ? 0 : stacksize;
	database = v;
	for(s = datap; s != S; s = s->next) {
		if(!s->reach || s->type != SDATA)
			continue;
		if(s->size == 0)
			diag("%s: data without size", s->name);
		v = (v + dalign(s->size) - 1) & ~(dalign(s->size) - 1);
		s->value = v;
		v += s->size;
		for(p = s->data; p != P && !rflag; p = p->link)
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
	nsect++;
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
			if(p->to.sym != S && rflag) {
				if(w != 4)
					diag("%s: address of %s in %ld bytes", s->name, p->to.sym->name, w);
				if(isfunc(p->to.sym))
					addrel(RTABI32, off, p->to.sym, s, 0, 0);
				else {
					addrel(RMEMI32, off, p->to.sym, s, 0, v);
					v += p->to.sym->value;
				}
			} else if(p->to.sym != S)
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
		"mutable-globals", "nontrapping-fptoint", "bulk-memory", "bulk-memory-opt", "sign-ext", "multivalue", "call-indirect-overlong", "exception-handling", "atomics", "simd128",
	};
	Buf b;
	int i, n;

	memset(&b, 0, sizeof b);
	bname(&b, "target_features");
	n = nelem(f) - !simd;
	buleb(&b, n);
	for(i=0; i<n; i++) {
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

int
isfunc(Sym *s)
{
	return s->type == STEXT || s->type == SIMPORT || s->type == SUNDEF && s->sig >= 0;
}

void
addrel(int t, long off, Sym *s, Sym *in, long idx, long add)
{
	Rel *r;

	if(nrel % 64 == 0)
		rels = realloc(rels, (nrel+64)*sizeof(Rel));
	r = &rels[nrel++];
	r->type = t;
	r->off = off;
	r->sym = s;
	r->in = in;
	r->idx = idx;
	r->add = add;
}

void
bpad(Buf *b, vlong v)
{
	int i;

	for(i=0; i<4; i++)
		bput(b, (v >> 7*i) & 0x7f | 0x80);
	bput(b, (v >> 28) & 0x7f);
}

static int
lebsize(uvlong v)
{
	int n;

	for(n = 1; v >= 0x80; n++)
		v >>= 7;
	return n;
}

static int
relcmp(void *a, void *b)
{
	return ((Rel*)a)->off - ((Rel*)b)->off;
}

static void
putrels(Buf *m, char *name, int sect, Rel *r, int n)
{
	Buf b;
	int i;

	if(n == 0)
		return;
	memset(&b, 0, sizeof b);
	qsort(r, n, sizeof(Rel), relcmp);
	bname(&b, name);
	buleb(&b, sect);
	buleb(&b, n);
	for(i=0; i<n; i++) {
		buleb(&b, r[i].type);
		buleb(&b, r[i].off);
		buleb(&b, r[i].sym != S ? r[i].sym->ridx : r[i].idx);
		if(r[i].type == RMEMSLEB || r[i].type == RMEMI32)
			bsleb(&b, r[i].add);
	}
	section(m, 0, &b);
}

static char*
symname(Sym *s)
{
	if(s->sig >= 0 && strcmp(s->name, "main") == 0)
		return sigs[s->sig].np == 0 ? "__main_void" : "__main_argc_argv";
	if(strcmp(s->name, "longjmp") == 0)
		return "__wasm_longjmp";
	if(s->type == SUNDEF && strcmp(s->name, "end") == 0)
		return "__heap_base";
	if(s->type == SUNDEF && strcmp(s->name, "edata") == 0)
		return "__data_end";
	return s->name;
}

static int
symflags(Sym *s)
{
	int f;

	f = 0;
	if(s->version != 0)
		f |= 0x02;
	else if(s->type == SBSS || s->type == STEXT && s->dupok)
		f |= 0x01;
	if(s->type == STEXT && isexport(s))
		f |= 0x20 | 0x80;
	return f;
}

static int
lg2(long v)
{
	int n;

	for(n = 0; v > 1; n++)
		v >>= 1;
	return n;
}

static void
import(Buf *b, char *mod, char *field, int kind)
{
	bname(b, mod);
	bname(b, field);
	bput(b, kind);
}

void
asmr(void)
{
	Buf m, b, c, f, t;
	Sym *s, *sp, *tag, **segs, **und;
	Rel *cr, *dr;
	int i, j, ncr, ndr, nseg, nund, start, codesect, fd, deftag;
	long pos, *segbase;
	char *mod, *field, *name;
	uchar *mem;
	Sig *sg;

	memset(&m, 0, sizeof m);
	memset(&b, 0, sizeof b);
	memset(&c, 0, sizeof c);
	memset(&f, 0, sizeof f);
	memset(&t, 0, sizeof t);
	bputs(&m, "\0asm\1\0\0\0", 8);

	for(i=0; i<nfunc; i++)
		scancalls(funcs[i], callsigs);
	pos = lebsize(nfunc);
	for(i=0; i<nfunc; i++) {
		start = nrel;
		gencode(funcs[i], &f);
		for(j=start; j<nrel; j++)
			rels[j].off += pos + lebsize(f.n);
		pos += lebsize(f.n) + f.n;
		bvec(&c, &f);
		bfree(&f);
	}
	if(nerrors)
		return;
	cr = rels;
	ncr = nrel;
	rels = nil;
	nrel = 0;

	nseg = 0;
	segs = nil;
	for(i=0; i<2; i++)
	for(s = datap; s != S; s = s->next) {
		if(!s->reach || s->type != (i == 0 ? SDATA : SBSS))
			continue;
		segs = realloc(segs, (nseg+1)*sizeof(Sym*));
		segs[nseg++] = s;
	}
	nund = 0;
	und = nil;
	for(i=0; i<NHASH; i++)
	for(s = hash[i]; s != S; s = s->hash)
		if(s->reach && s->type == SUNDEF && s->sig < 0) {
			und = realloc(und, (nund+1)*sizeof(Sym*));
			und[nund++] = s;
		}
	for(i=0; i<nimport; i++)
		imports[i]->ridx = imports[i]->fidx;
	for(i=0; i<nfunc; i++)
		funcs[i]->ridx = funcs[i]->fidx;
	for(i=0; i<nseg; i++)
		segs[i]->ridx = nimport + nfunc + i;
	for(i=0; i<nund; i++)
		und[i]->ridx = nimport + nfunc + nseg + i;
	sp = lookup("__stack_pointer", 0);
	sp->ridx = nimport + nfunc + nseg + nund;
	tag = lookup("__c_longjmp", 0);
	tag->ridx = sp->ridx + 1;

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

	deftag = tagsig >= 0 && lookup("longjmp", 0)->type == STEXT;
	buleb(&b, 3 + (tagsig >= 0 && !deftag) + nimport);
	import(&b, "env", "__linear_memory", 0x02);
	bput(&b, 0x00);
	buleb(&b, (bssend + PAGE - 1) / PAGE);
	import(&b, "env", "__indirect_function_table", 0x01);
	bput(&b, 0x70);
	bput(&b, 0x00);
	buleb(&b, 0);
	import(&b, "env", "__stack_pointer", 0x03);
	bput(&b, 0x7f);
	bput(&b, 0x01);
	if(tagsig >= 0 && !deftag) {
		import(&b, "env", "__c_longjmp", 0x04);
		bput(&b, 0x00);
		buleb(&b, tagsig);
	}
	for(i=0; i<nimport; i++) {
		s = imports[i];
		if(s->type == SIMPORT) {
			mod = impmod(s, &field);
			import(&b, mod, field, 0x00);
			free(mod);
		} else
			import(&b, "env", symname(s), 0x00);
		buleb(&b, s->sig);
	}
	section(&m, 2, &b);

	buleb(&b, nfunc);
	for(i=0; i<nfunc; i++)
		buleb(&b, funcs[i]->sig);
	section(&m, 3, &b);

	if(deftag) {
		buleb(&b, 1);
		bput(&b, 0x00);
		buleb(&b, tagsig);
		section(&m, 13, &b);
	}

	codesect = nsect;
	buleb(&b, nfunc);
	bputs(&b, c.p, c.n);
	bfree(&c);
	section(&m, 10, &b);

	dr = nil;
	ndr = 0;
	if(nseg > 0) {
		mem = mallocz(bssend + 1, 1);
		for(i=0; i<nseg; i++)
			if(segs[i]->type == SDATA)
				putdata(mem, segs[i]);
		segbase = malloc(nseg*sizeof(long));
		buleb(&b, nseg);
		for(i=0; i<nseg; i++) {
			s = segs[i];
			buleb(&b, 0);
			bput(&b, 0x41);
			bsleb(&b, s->value);
			bput(&b, 0x0b);
			buleb(&b, s->size);
			segbase[i] = b.n;
			bputs(&b, mem + s->value, s->size);
		}
		for(j=0; j<nrel; j++)
			for(i=0; i<nseg; i++)
				if(rels[j].in == segs[i]) {
					rels[j].off += segbase[i];
					break;
				}
		free(segbase);
		free(mem);
		dr = rels;
		ndr = nrel;
		rels = nil;
		nrel = 0;
		section(&m, 11, &b);
	}
	if(nerrors)
		return;

	bname(&b, "linking");
	buleb(&b, 2);
	buleb(&t, nimport + nfunc + nseg + nund + 1 + (tagsig >= 0));
	for(i=0; i<nimport; i++) {
		s = imports[i];
		bput(&t, 0);
		if(s->type == SIMPORT) {
			buleb(&t, 0x10 | 0x40);
			buleb(&t, s->fidx);
			bname(&t, s->name);
		} else {
			buleb(&t, 0x10);
			buleb(&t, s->fidx);
		}
	}
	for(i=0; i<nfunc; i++) {
		s = funcs[i];
		bput(&t, 0);
		buleb(&t, symflags(s));
		buleb(&t, s->fidx);
		bname(&t, symname(s));
	}
	for(i=0; i<nseg; i++) {
		s = segs[i];
		bput(&t, 1);
		buleb(&t, symflags(s));
		bname(&t, s->name);
		buleb(&t, i);
		buleb(&t, 0);
		buleb(&t, s->size);
	}
	for(i=0; i<nund; i++) {
		bput(&t, 1);
		buleb(&t, 0x10);
		bname(&t, symname(und[i]));
	}
	bput(&t, 2);
	buleb(&t, 0x10);
	buleb(&t, 0);
	if(deftag) {
		bput(&t, 4);
		buleb(&t, 0);
		buleb(&t, 0);
		bname(&t, "__c_longjmp");
	} else if(tagsig >= 0) {
		bput(&t, 4);
		buleb(&t, 0x10);
		buleb(&t, 0);
	}
	bput(&b, 8);
	bvec(&b, &t);
	bfree(&t);
	if(nseg > 0) {
		buleb(&t, nseg);
		for(i=0; i<nseg; i++) {
			s = segs[i];
			name = smprint("%s%s", s->type == SBSS ? ".bss." : ".data.", s->name);
			bname(&t, name);
			free(name);
			buleb(&t, lg2(dalign(s->size)));
			buleb(&t, 0);
		}
		bput(&b, 5);
		bvec(&b, &t);
		bfree(&t);
	}
	section(&m, 0, &b);
	putrels(&m, "reloc.CODE", codesect, cr, ncr);
	if(nseg > 0)
		putrels(&m, "reloc.DATA", codesect+1, dr, ndr);
	features(&m);
	free(cr);
	free(dr);
	free(segs);
	free(und);
	if(nerrors)
		return;

	fd = create(outfile, OWRITE, 0664);
	if(fd < 0) {
		diag("create %s: %r", outfile);
		return;
	}
	if(write(fd, m.p, m.n) != m.n)
		diag("write %s: %r", outfile);
	close(fd);
}
