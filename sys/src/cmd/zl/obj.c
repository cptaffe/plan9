#include	"l.h"

Sym*	hash[NHASH];
Sym*	textp;
Sym*	etextp;
Sym*	datap;
Sig*	sigs;
int	nsig;
int	nerrors;
char*	outfile = "z.out";
char*	entry = "_start";
long	stacksize = STACK;
int	debug[128];
char**	exports;
int	nexport;
int	rflag;
Rel*	rels;
int	nrel;
Sym*	cursym;
static	int	version;
static	char*	filen;
static	char*	libdir[16];
static	int	nlibdir;
static	char*	library[64];
static	int	nlibrary;
static	int	libloaded;
static	char*	frog[64];
static	int	nfrog;
static	Sym**	work;
static	int	nwork, mwork;
static	int	quiet;

void
usage(void)
{
	fprint(2, "usage: zl [-alrsv] [-o out] [-D stack] [-E entry] [-L dir] [-x name] file.z ...\n");
	exits("usage");
}

void
main(int argc, char *argv[])
{
	int c, i;
	char *a, name[256];

	ARGBEGIN {
	case 'o':
		outfile = EARGF(usage());
		break;
	case 'D':
		stacksize = strtol(EARGF(usage()), nil, 0);
		break;
	case 'E':
		entry = EARGF(usage());
		break;
	case 'L':
		if(nlibdir < nelem(libdir))
			libdir[nlibdir++] = EARGF(usage());
		break;
	case 'x':
		exports = realloc(exports, (nexport+1)*sizeof(char*));
		exports[nexport++] = EARGF(usage());
		break;
	case 'p':
		fprint(2, "zl: profiling is not supported\n");
		exits("usage");
	case 'r':
		rflag = 1;
		debug['l'] = 1;
		break;
	default:
		c = ARGC();
		if(c >= 0 && c < nelem(debug))
			debug[c]++;
		break;
	} ARGEND
	if(argc < 1)
		usage();
	a = getenv("ccroot");
	if(a == nil)
		a = "";
	snprint(name, sizeof name, "%s/wasm/lib", a);
	if(nlibdir < nelem(libdir))
		libdir[nlibdir++] = strdup(name);
	while(argc-- > 0)
		ldobj(*argv++);
	if(debug['v'])
		fprint(2, "%5.2f lib\n", cputime());
	do {
		i = libloaded;
		while(libloaded < nlibrary)
			ldobj(library[libloaded++]);
		loadlib();
	} while(i != libloaded || libloaded < nlibrary);
	if(nerrors)
		errorexit();
	if(debug['v'])
		fprint(2, "%5.2f reach\n", cputime());
	reach();
	if(nerrors)
		errorexit();
	if(debug['v'])
		fprint(2, "%5.2f layout\n", cputime());
	layout();
	if(nerrors)
		errorexit();
	if(debug['v'])
		fprint(2, "%5.2f asmb\n", cputime());
	if(rflag)
		asmr();
	else
		asmb();
	if(nerrors)
		errorexit();
	exits(0);
}

static void
addlib(void)
{
	char fn[512], comp[256], *p;
	int i, search, d;

	if(nfrog <= 0 || debug['l'])
		return;
	fn[0] = 0;
	search = 0;
	i = 0;
	if(frog[0][0] == '/')
		i = 1;
	else if(frog[0][0] == '.')
		strcpy(fn, ".");
	else
		search = 1;
	for(; i<nfrog; i++) {
		snprint(comp, sizeof comp, "%s", frog[i]);
		while((p = strstr(comp, "$O")) != nil) {
			memmove(p+1, p+2, strlen(p+2)+1);
			p[0] = 'z';
		}
		while((p = strstr(comp, "$M")) != nil && strlen(comp)+3 < sizeof comp) {
			memmove(p+4, p+2, strlen(p+2)+1);
			memmove(p, "wasm", 4);
		}
		if(i > 0 || !search)
			strncat(fn, "/", sizeof fn - strlen(fn) - 1);
		strncat(fn, comp, sizeof fn - strlen(fn) - 1);
	}
	cleanname(fn);
	p = strdup(fn);
	if(search)
		for(d=0; d<nlibdir; d++) {
			snprint(comp, sizeof comp, "%s/%s", libdir[d], fn);
			if(access(comp, AEXIST) >= 0) {
				p = strdup(comp);
				break;
			}
		}
	for(i=0; i<nlibrary; i++)
		if(strcmp(library[i], p) == 0)
			return;
	if(nlibrary >= nelem(library)) {
		diag("too many autolibs; skipping %s", p);
		return;
	}
	library[nlibrary++] = p;
}

void
errorexit(void)
{
	if(outfile)
		remove(outfile);
	exits("error");
}

void
diag(char *fmt, ...)
{
	char buf[1024];
	va_list arg;

	va_start(arg, fmt);
	vseprint(buf, buf+sizeof(buf), fmt, arg);
	va_end(arg);
	if(cursym != S)
		fprint(2, "%s: %s\n", cursym->name, buf);
	else if(filen != nil)
		fprint(2, "%s: %s\n", filen, buf);
	else
		fprint(2, "%s\n", buf);
	if(++nerrors > 20 && !debug['A']) {
		fprint(2, "too many errors\n");
		errorexit();
	}
}

Sym*
lookup(char *name, int v)
{
	Sym *s;
	u32int h;
	char *p;

	h = v;
	for(p = name; *p; p++)
		h = h*31 + (uchar)*p;
	h %= NHASH;
	for(s = hash[h]; s != S; s = s->hash)
		if(s->version == v && strcmp(s->name, name) == 0)
			return s;
	s = mallocz(sizeof(Sym), 1);
	s->name = strdup(name);
	s->version = v;
	s->fidx = -1;
	s->tidx = -1;
	s->sig = -1;
	s->hash = hash[h];
	hash[h] = s;
	return s;
}

static long
g4(uchar *p)
{
	return (long)(int)(p[0] | p[1]<<8 | p[2]<<16 | (ulong)p[3]<<24);
}

static uchar*
rdaddr(uchar *bp, uchar *ep, Adr *a, Sym **h)
{
	int i;
	uvlong l, hi;

	if(ep - bp < 4)
		return nil;
	a->type = bp[0];
	a->reg = bp[1];
	i = bp[2];
	a->name = bp[3];
	a->sym = S;
	if(i > 0 && i < NSYM)
		a->sym = h[i];
	bp += 4;
	a->offset = 0;
	switch(a->type) {
	default:
		diag("unknown address type %d", a->type);
		return nil;
	case D_NONE:
	case D_REG:
		break;
	case D_OREG:
	case D_CONST:
	case D_BRANCH:
	case D_WPARAM:
	case D_LOCAL:
		a->offset = g4(bp);
		bp += 4;
		break;
	case D_VCONST:
	case D_FCONST:
		l = (u32int)g4(bp);
		hi = (u32int)g4(bp+4);
		a->offset = (vlong)(l | hi<<32);
		bp += 8;
		break;
	case D_SCONST:
		memmove(a->sval, bp, NSNAME);
		bp += NSNAME;
		break;
	}
	if(bp > ep)
		return nil;
	return bp;
}

typedef	struct	Member	Member;
typedef	struct	Def	Def;
struct	Member
{
	char*	name;
	uchar*	p;
	long	n;
	int	loaded;
};
struct	Def
{
	char*	sym;
	Member*	m;
};
static	Def*	defs;
static	int	ndef;
static	int	inlib;

static int
isobj(uchar *p, long n)
{
	return n >= 4 && p[0] == ANAME && p[1] == D_FILE && p[2] == 1 && p[3] == '<';
}

static long
asize(int t)
{
	switch(t) {
	case D_OREG:
	case D_CONST:
	case D_BRANCH:
	case D_WPARAM:
	case D_LOCAL:
		return 4;
	case D_VCONST:
	case D_FCONST:
	case D_SCONST:
		return 8;
	}
	return 0;
}

static void
defscan(Member *m)
{
	char *names[NSYM];
	uchar *bp, *ep, *a;
	int as, i, t;

	memset(names, 0, sizeof names);
	bp = m->p;
	ep = m->p + m->n;
	while(bp < ep) {
		as = bp[0];
		if(as == ASIGNAME) {
			bp += 4;
			as = ANAME;
		}
		if(as == ANAME) {
			t = bp[1];
			i = bp[2];
			if(i > 0 && i < NSYM)
				names[i] = t == D_EXTERN ? (char*)bp+3 : nil;
			bp += 3;
			while(bp < ep && *bp)
				bp++;
			bp++;
			continue;
		}
		if(ep - bp < 14)
			return;
		a = bp+6;
		if((as == ATEXT || as == AGLOBL || as == ADATA) && a[3] == D_EXTERN && a[2] > 0 && a[2] < NSYM && names[a[2]] != nil) {
			defs = realloc(defs, (ndef+1)*sizeof(Def));
			defs[ndef].sym = names[a[2]];
			defs[ndef].m = m;
			ndef++;
		}
		bp = a + 4 + asize(a[0]);
		if(bp + 4 > ep)
			return;
		bp += 4 + asize(bp[0]);
		if(as == AEND)
			return;
	}
}

static void
ldar(char *file, uchar *buf, long n)
{
	uchar *bp, *ep, *d;
	char name[64];
	long sz;
	int nl, k;
	Member *m;

	bp = buf + 8;
	ep = buf + n;
	while(bp + 60 <= ep) {
		memmove(name, bp, 16);
		name[16] = 0;
		sz = strtol((char*)bp+48, nil, 10);
		d = bp + 60;
		bp = d + sz + (sz & 1);
		if(d + sz > ep) {
			diag("%s: truncated archive", file);
			return;
		}
		if(strncmp(name, "#1/", 3) == 0) {
			nl = atoi(name+3);
			if(nl < 0 || nl > sz)
				continue;
			k = nl < sizeof name ? nl : sizeof name - 1;
			memmove(name, d, k);
			name[k] = 0;
			d += nl;
			sz -= nl;
		}
		if(!isobj(d, sz))
			continue;
		m = mallocz(sizeof(Member), 1);
		m->name = smprint("%s(%s)", file, name);
		m->p = d;
		m->n = sz;
		defscan(m);
	}
}

static void	ldbuf(char*, uchar*, long);

void
ldobj(char *file)
{
	int fd;
	long n;
	uchar *buf;
	Dir *d;

	filen = file;
	fd = open(file, OREAD);
	if(fd < 0) {
		diag("cannot open: %r");
		return;
	}
	d = dirfstat(fd);
	if(d == nil) {
		diag("cannot stat: %r");
		close(fd);
		return;
	}
	n = d->length;
	free(d);
	buf = malloc(n+1);
	if(readn(fd, buf, n) != n) {
		diag("short read");
		close(fd);
		return;
	}
	close(fd);
	filen = nil;
	if(n >= 8 && memcmp(buf, "!<arch>\n", 8) == 0) {
		ldar(file, buf, n);
		return;
	}
	ldbuf(file, buf, n);
	free(buf);
}

static void
unreach(void)
{
	Sym *s;
	int i;

	for(i=0; i<NHASH; i++)
		for(s = hash[i]; s != S; s = s->hash)
			s->reach = 0;
	nwork = 0;
}

void
loadlib(void)
{
	int i, more;
	Sym *s;

	do {
		more = 0;
		quiet = 1;
		reach();
		quiet = 0;
		for(i=0; i<ndef; i++) {
			if(defs[i].m->loaded)
				continue;
			s = lookup(defs[i].sym, 0);
			if(!s->reach || s->type != SXREF)
				continue;
			defs[i].m->loaded = 1;
			inlib = 1;
			ldbuf(defs[i].m->name, defs[i].m->p, defs[i].m->n);
			inlib = 0;
			more = 1;
		}
		unreach();
	} while(more);
}

static void
ldbuf(char *file, uchar *buf, long n)
{
	int as, i, j, k, t;
	long np, mp;
	uchar *bp, *ep;
	Sym *h[NSYM], *s, *curtext, *curdecl;
	Prog *p, **pcs, *lastp, *lastd;

	filen = file;
	if(!isobj(buf, n)) {
		diag("not a wasm object");
		return;
	}
	version++;
	memset(h, 0, sizeof h);
	bp = buf;
	ep = buf+n;
	np = 0;
	mp = 1024;
	pcs = malloc(mp*sizeof(Prog*));
	curtext = S;
	curdecl = S;
	lastp = P;
	lastd = P;
	while(bp < ep) {
		as = bp[0];
		if(as == ASIGNAME) {
			bp += 4;
			as = ANAME;
		}
		if(as == ANAME) {
			t = bp[1];
			i = bp[2];
			bp += 3;
			s = S;
			if(t != D_FILE)
				s = lookup((char*)bp, t == D_STATIC ? version : 0);
			else if(nfrog < nelem(frog))
				frog[nfrog++] = strdup((char*)bp+1);
			while(bp < ep && *bp)
				bp++;
			bp++;
			if(i > 0 && i < NSYM)
				h[i] = s;
			continue;
		}
		if(ep - bp < 6) {
			diag("truncated object");
			break;
		}
		p = mallocz(sizeof(Prog), 1);
		p->as = as;
		p->reg = bp[1];
		p->line = g4(bp+2);
		p->lab = -1;
		bp = rdaddr(bp+6, ep, &p->from, h);
		if(bp != nil)
			bp = rdaddr(bp, ep, &p->to, h);
		if(bp == nil) {
			diag("bad object at %s", anames[as]);
			break;
		}
		if(p->from.sym != S && (p->from.name == D_EXTERN || p->from.name == D_STATIC))
			p->from.sym->ref = 1;
		if(p->to.sym != S && (p->to.name == D_EXTERN || p->to.name == D_STATIC))
			p->to.sym->ref = 1;
		switch(as) {
		case AHISTORY:
			if(p->to.offset == -1)
				addlib();
			nfrog = 0;
			continue;
		case AEND:
			bp = ep;
			continue;
		case ADECL:
			curtext = S;
			curdecl = p->from.sym;
			if(curdecl != S && curdecl->decl != P)
				curdecl = S;
			lastd = P;
			continue;
		case APARAM:
		case ARESULT:
			if(curdecl == S)
				break;
			if(lastd == P)
				curdecl->decl = p;
			else
				lastd->link = p;
			lastd = p;
			if(as == ARESULT)
				curdecl = S;
			continue;
		case AGLOBL:
			s = p->from.sym;
			if(s == S)
				continue;
			if(s->type == SXREF) {
				s->type = SBSS;
				s->next = datap;
				datap = s;
			}
			if(p->to.offset > s->size)
				s->size = p->to.offset;
			if(p->reg & DUPOK)
				s->dupok = 1;
			continue;
		case ADATA:
			s = p->from.sym;
			if(s == S)
				continue;
			if(s->type == SXREF) {
				s->next = datap;
				datap = s;
			}
			if(s->type == SXREF || s->type == SBSS) {
				s->type = SDATA;
				s->dver = version;
			}
			if(s->dver != version) {
				if(!inlib)
					diag("multiple initialization for %s", s->name);
				continue;
			}
			if(s->data == P)
				s->data = p;
			else
				s->edata->link = p;
			s->edata = p;
			continue;
		}
		if(np >= mp) {
			mp *= 2;
			pcs = realloc(pcs, mp*sizeof(Prog*));
		}
		pcs[np++] = p;
		if(as == ATEXT) {
			s = p->from.sym;
			if(s->type == STEXT) {
				if(!(p->reg & DUPOK) && !s->dupok && !inlib)
					diag("duplicate definition of %s", s->name);
				curtext = S;
				continue;
			}
			if(s->type != SXREF)
				diag("%s: text and data", s->name);
			s->type = STEXT;
			s->text = p;
			s->frame = p->to.offset;
			s->file = strdup(file);
			s->dupok = (p->reg & DUPOK) != 0;
			if(textp == S)
				textp = s;
			else
				etextp->next = s;
			etextp = s;
			curtext = s;
			lastp = p;
			continue;
		}
		if(curtext == S)
			continue;
		lastp->link = p;
		lastp = p;
	}
	for(i=0; i<np; i++) {
		p = pcs[i];
		if(p->to.type == D_BRANCH) {
			if(p->to.offset < 0 || p->to.offset >= np) {
				diag("branch out of range");
				continue;
			}
			p->cond = pcs[p->to.offset];
		}
	}
	for(i=0; i<np; i++) {
		p = pcs[i];
		if(p->as != ACASE)
			continue;
		for(k=0; i+1+k < np && pcs[i+1+k]->as == ABCASE; k++)
			;
		p->ntab = k;
		p->tab = malloc(k*sizeof(Prog*));
		for(j=0; j<k; j++)
			p->tab[j] = pcs[i+1+j]->cond;
		if(k > 0)
			p->link = pcs[i+k]->link;
		i += k;
	}
	free(pcs);
	filen = nil;
}

int
isexport(Sym *s)
{
	int i;

	if(strcmp(s->name, entry) == 0 || strncmp(s->name, "p9web_", 6) == 0)
		return 1;
	for(i=0; i<nexport; i++)
		if(strcmp(s->name, exports[i]) == 0)
			return 1;
	return 0;
}

int
iswasm(Sym *s)
{
	return strncmp(s->name, "wasm·", 6) == 0 || strcmp(s->name, "setjmp") == 0;
}

int
isimport(Sym *s)
{
	return utfrune(s->name, L'·') != nil && !iswasm(s);
}

char*
impmod(Sym *s, char **field)
{
	char *p, *m;

	p = utfrune(s->name, L'·');
	m = malloc(p - s->name + 1);
	memmove(m, s->name, p - s->name);
	m[p - s->name] = 0;
	*field = p + runelen(L'·');
	return m;
}

static int
linksym(Sym *s)
{
	static char *names[] = {
		"__heap_base", "__stack_high", "__stack_low", "__data_end", "end", "edata", "etext",
	};
	int i;

	for(i=0; i<nelem(names); i++)
		if(strcmp(s->name, names[i]) == 0)
			return 1;
	return 0;
}


static void
mark(Sym *s)
{
	if(s == S || s->reach)
		return;
	if(s->type == SXREF && !rflag && !quiet && !iswasm(s) && !isimport(s) && !linksym(s)) {
		diag("undefined: %s", s->name);
		s->reach = 1;
		return;
	}
	s->reach = 1;
	if(nwork >= mwork) {
		mwork = mwork ? 2*mwork : 1024;
		work = realloc(work, mwork*sizeof(Sym*));
	}
	work[nwork++] = s;
}

static void
marka(Adr *a)
{
	if(a->name == D_EXTERN || a->name == D_STATIC)
		mark(a->sym);
}

void
reach(void)
{
	Sym *s;
	Prog *p;

	int i;

	if(rflag) {
		for(s = textp; s != S; s = s->next)
			mark(s);
		for(s = datap; s != S; s = s->next)
			mark(s);
	} else {
		s = lookup(entry, 0);
		if(s->type != STEXT && !quiet)
			diag("entry %s not defined", entry);
		mark(s);
	}
	for(i=0; i<nexport; i++) {
		s = lookup(exports[i], 0);
		if(s->type != STEXT && !quiet)
			diag("export %s not defined", exports[i]);
		mark(s);
	}
	for(s = textp; s != S; s = s->next)
		if(isexport(s))
			mark(s);
	while(nwork > 0) {
		s = work[--nwork];
		switch(s->type) {
		case STEXT:
			for(p = s->text; p != P; p = p->link) {
				cursym = s;
				marka(&p->from);
				marka(&p->to);
			}
			cursym = S;
			break;
		case SDATA:
			for(p = s->data; p != P; p = p->link)
				marka(&p->to);
			break;
		case SBSS:
			break;
		case SXREF:
			if(iswasm(s))
				s->type = SWASM;
			else if(isimport(s))
				s->type = SIMPORT;
			else if(rflag)
				s->type = SUNDEF;
			else if(linksym(s))
				s->type = SLINK;
			break;
		}
	}
}
