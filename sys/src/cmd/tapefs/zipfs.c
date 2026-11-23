#include <u.h>
#include <libc.h>
#include <flate.h>
#include "tapefs.h"
#include "zip.h"

/*
 * File system for zip archives (read-only)
 */

typedef struct End End;
typedef struct Member Member;
typedef struct Infl Infl;
typedef struct Node Node;

enum {
	Tail = 1024,
	Maxtail = 65*1024,
	Maxdir = 256*1024*1024,
	Maxname = 4096,
	Maxnode = 1<<19,
	Maxratio = 1032,
	Nclosed = 8,
	Maxclosed = 32*1024*1024,
	Freewait = 10000,
	Freetick = 1000,
};

struct End {
	vlong	end;
	uvlong	count;
	uvlong	size;
	uvlong	off;
};

struct Member {
	int	flags;
	int	meth;
	ulong	crc;
	vlong	csize;
	vlong	size;
	vlong	off;
	vlong	body;
	ulong	sum;
	vlong	summed;
	int	badsum;
	int	badsize;
	int	checked;
	int	fd;
	Ram	*ram;
	Member	*next;
};

struct Infl {
	Member	*m;
	vlong	off;
	vlong	left;
	uchar	in[Maxbuf];
	uchar	*p;
	uchar	*ep;
	int	fd;
	vlong	n;
	ulong	crc;
	int	tmperr;
	char	err[ERRMAX];
};

struct Node {
	Ram	*ram;
	long	pri;
	Node	*left;
	Node	*right;
};

static int zfd;
static vlong zsize;
static Dir *zdir;
static int changed;
static ulong *crctab;
static Member *cached;
static vlong freed;
static int nnode;

static int doempty(Ram*);

static Rune cp437[128] = {
	0x00c7, 0x00fc, 0x00e9, 0x00e2, 0x00e4, 0x00e0, 0x00e5, 0x00e7,
	0x00ea, 0x00eb, 0x00e8, 0x00ef, 0x00ee, 0x00ec, 0x00c4, 0x00c5,
	0x00c9, 0x00e6, 0x00c6, 0x00f4, 0x00f6, 0x00f2, 0x00fb, 0x00f9,
	0x00ff, 0x00d6, 0x00dc, 0x00a2, 0x00a3, 0x00a5, 0x20a7, 0x0192,
	0x00e1, 0x00ed, 0x00f3, 0x00fa, 0x00f1, 0x00d1, 0x00aa, 0x00ba,
	0x00bf, 0x2310, 0x00ac, 0x00bd, 0x00bc, 0x00a1, 0x00ab, 0x00bb,
	0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556,
	0x2555, 0x2563, 0x2551, 0x2557, 0x255d, 0x255c, 0x255b, 0x2510,
	0x2514, 0x2534, 0x252c, 0x251c, 0x2500, 0x253c, 0x255e, 0x255f,
	0x255a, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256c, 0x2567,
	0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256b,
	0x256a, 0x2518, 0x250c, 0x2588, 0x2584, 0x258c, 0x2590, 0x2580,
	0x03b1, 0x00df, 0x0393, 0x03c0, 0x03a3, 0x03c3, 0x00b5, 0x03c4,
	0x03a6, 0x0398, 0x03a9, 0x03b4, 0x221e, 0x03c6, 0x03b5, 0x2229,
	0x2261, 0x00b1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00f7, 0x2248,
	0x00b0, 0x2219, 0x00b7, 0x221a, 0x207f, 0x00b2, 0x25a0, 0x00a0,
};

static int
get2(uchar *p)
{
	return p[0] | p[1]<<8;
}

static ulong
get4(uchar *p)
{
	return get2(p) | (ulong)get2(p+2)<<16;
}

static uvlong
get8(uchar *p)
{
	return get4(p) | (uvlong)get4(p+4)<<32;
}

static int
readat(int fd, void *v, long n, vlong off)
{
	long i, m;

	for(i = 0; i < n; i += m){
		m = pread(fd, (uchar*)v+i, n-i, off+i);
		if(m == 0){
			werrstr("unexpected eof");
			return -1;
		}
		if(m < 0)
			return -1;
	}
	return 0;
}

static int
hassig(vlong off, ulong sig)
{
	uchar b[4];

	if(off < 0 || off > zsize-4 || readat(zfd, b, sizeof b, off) < 0)
		return 0;
	return get4(b) == sig;
}

static vlong
findend(uchar *e)
{
	uchar *b;
	vlong off;
	long n, i;
	int try;

	for(try = 0; try < 2; try++){
		n = try == 0? Tail: Maxtail;
		if(n > zsize)
			n = zsize;
		off = zsize-n;
		b = emalloc(n+1);
		if(readat(zfd, b, n, off) < 0){
			free(b);
			return -1;
		}
		for(i = n-ZECHeadSize; i >= 0; i--)
			if(get4(b+i) == ZECHeader)
				break;
		if(i >= 0 && i+ZECHeadSize+get2(b+i+20) <= n){
			memmove(e, b+i, ZECHeadSize);
			free(b);
			return off+i;
		}
		free(b);
		if(n == zsize)
			break;
	}
	werrstr("not a zip file");
	return -1;
}

static int
readend64(End *d)
{
	uchar l[Z64LocSize], b[Z64EndSize];
	vlong at;
	uvlong off;

	at = d->end-Z64LocSize;
	if(!hassig(at, Z64Loc))
		return 0;
	if(readat(zfd, l, sizeof l, at) < 0)
		return -1;
	if(get4(l+4) != 0 || get4(l+16) > 1)
		return 0;
	off = get8(l+8);
	if(off <= zsize && hassig(off, Z64End))
		at = off;
	else if(hassig(at-Z64EndSize, Z64End))
		at -= Z64EndSize;
	else{
		werrstr("no zip64 end of directory");
		return -1;
	}
	if(readat(zfd, b, sizeof b, at) < 0)
		return -1;
	d->end = at;
	d->count = get8(b+32);
	d->size = get8(b+40);
	d->off = get8(b+48);
	return 0;
}

static vlong
finddir(End *d, vlong *base)
{
	uchar e[ZECHeadSize];
	vlong at;

	if((d->end = findend(e)) < 0)
		return -1;
	d->count = get2(e+10);
	d->size = get4(e+12);
	d->off = get4(e+16);
	if(d->count == ZMax16 || d->size == ZMax32 || d->off == ZMax32)
		if(readend64(d) < 0)
			return -1;
	if(d->size > zsize || (vlong)d->off < 0){
		werrstr("directory is outside the file");
		return -1;
	}
	*base = d->end - (vlong)d->size - (vlong)d->off;
	if(*base > 0 && hassig(d->off, ZCHeader))
		*base = 0;
	at = *base + (vlong)d->off;
	if(at < 0 || at > d->end){
		werrstr("directory is outside the file");
		return -1;
	}
	if(d->end-at > Maxdir){
		werrstr("directory is too big");
		return -1;
	}
	return at;
}

static char*
decodename(uchar *s, int n)
{
	char *t, *p;
	Rune r;
	int i, w, utf;

	t = emalloc(n*UTFmax+1);
	memmove(t, s, n);
	utf = 1;
	for(i = 0; i < n; i += w){
		if(s[i] < Runeself){
			w = 1;
			continue;
		}
		w = chartorune(&r, t+i);
		if(r == Runeerror && w == 1){
			utf = 0;
			break;
		}
	}
	p = t;
	for(i = 0; i < n; i += w){
		w = 1;
		if(s[i] == '\\')
			*p++ = '/';
		else if(s[i] < ' ' || s[i] == 0x7f)
			*p++ = '_';
		else if(s[i] < Runeself)
			*p++ = s[i];
		else if(utf){
			w = chartorune(&r, (char*)s+i);
			if(r >= 0x80 && r < 0xa0)
				*p++ = '_';
			else{
				memmove(p, s+i, w);
				p += w;
			}
		}else{
			r = cp437[s[i]-Runeself];
			p += runetochar(p, &r);
		}
	}
	*p = 0;
	return t;
}

static Ram*
findnode(Ram *dir, char *name)
{
	Node *nd;
	int c;

	if(!(dir->qid.type & QTDIR))
		return nil;
	for(nd = dir->data; nd != nil; nd = c < 0? nd->left: nd->right)
		if((c = strcmp(name, nd->ram->name)) == 0)
			return nd->ram;
	return nil;
}

static Node*
addnode(Node *at, Node *nd)
{
	Node *top;

	if(at == nil)
		return nd;
	if(strcmp(nd->ram->name, at->ram->name) < 0){
		at->left = addnode(at->left, nd);
		if(at->left->pri > at->pri){
			top = at->left;
			at->left = top->right;
			top->right = at;
			return top;
		}
	}else{
		at->right = addnode(at->right, nd);
		if(at->right->pri > at->pri){
			top = at->right;
			at->right = top->left;
			top->left = at;
			return top;
		}
	}
	return at;
}

static Ram*
child(Ram *dir, char *name, int isdir, long mtime)
{
	Fileinf f;
	Node *nd;
	Ram *r;

	if((r = findnode(dir, name)) != nil)
		return r;
	if(++nnode > Maxnode){
		werrstr("too many files");
		return nil;
	}
	memset(&f, 0, sizeof f);
	f.name = name;
	f.mode = isdir? DMDIR|0555: 0444;
	f.mdate = mtime;
	nd = emalloc(sizeof *nd);
	nd->ram = popfile(dir, f);
	nd->pri = lrand();
	dir->data = addnode(dir->data, nd);
	return nd->ram;
}

static void
makedir(Ram *r, char *name)
{
	if(r->qid.type & QTDIR)
		return;
	if(r->data != nil)
		fprint(2, "%s: %s: both a file and a directory; serving the directory\n", argv0, name);
	free(r->data);
	r->data = nil;
	r->ndata = 0;
	r->qid.type = QTDIR;
	r->perm = DMDIR|0555;
}

static int
addmember(char *name, Member *m, long mtime)
{
	char **elem, *p, *e;
	Ram *r;
	int i, n, isdir, rv;

	rv = 0;
	p = name+strlen(name);
	isdir = p > name && p[-1] == '/';
	elem = emalloc((strlen(name)/2+1) * sizeof *elem);
	n = 0;
	for(p = name; p != nil; p = e){
		if((e = strchr(p, '/')) != nil)
			*e++ = 0;
		if(*p == 0 || strcmp(p, ".") == 0)
			continue;
		if(strcmp(p, "..") == 0){
			if(n > 0)
				n--;
			continue;
		}
		if(strlen(p) > Maxname){
			fprint(2, "%s: %.40s...: name too long; skipped\n", argv0, p);
			goto Out;
		}
		elem[n++] = p;
	}
	if(n == 0)
		goto Out;
	r = ram;
	for(i = 0; i < n-1; i++){
		if((r = child(r, elem[i], 1, mtime)) == nil)
			goto Err;
		makedir(r, elem[i]);
	}
	if((r = child(r, elem[n-1], isdir, mtime)) == nil)
		goto Err;
	if(isdir){
		makedir(r, elem[n-1]);
		r->mtime = r->atime = mtime;
		goto Out;
	}
	if(r->qid.type & QTDIR){
		fprint(2, "%s: %s: both a file and a directory; serving the directory\n", argv0, elem[n-1]);
		goto Out;
	}
	if(r->data != nil){
		fprint(2, "%s: %s: duplicate name; serving the last\n", argv0, elem[n-1]);
		free(r->data);
	}
	r->data = m;
	r->ndata = m->size;
	r->mtime = r->atime = mtime;
	m->ram = r;
	m = nil;
	goto Out;
Err:
	rv = -1;
Out:
	free(m);
	free(elem);
	return rv;
}

static long
msdos2time(int time, int date)
{
	Tm tm;

	memset(&tm, 0, sizeof tm);
	tm.hour = time >> 11;
	tm.min = (time >> 5) & 63;
	tm.sec = (time & 31) << 1;
	tm.year = 80 + (date >> 9);
	tm.mon = ((date >> 5) & 15) - 1;
	tm.mday = date & 31;
	if(tm.mon < 0 || tm.mon > 11 || tm.mday == 0)
		return 0;
	return tm2sec(&tm);
}

static void
ntfstime(uchar *p, uchar *ep, long *mtime)
{
	uchar *a;
	int id, n;

	if(ep-p < 4)
		return;
	for(p += 4; ep-p >= 4; p = a+n){
		id = get2(p);
		n = get2(p+2);
		a = p+4;
		if(n > ep-a)
			break;
		if(id == 1 && n == 24){
			*mtime = (vlong)(get8(a)/10000000) - 11644473600LL;
			return;
		}
	}
}

static int
readextra(Member *m, uchar *p, uchar *ep, long *mtime)
{
	uchar *f;
	ulong size32, csize32;
	uvlong size, csize, off;
	int id, n;

	size32 = size = m->size;
	csize32 = csize = m->csize;
	off = m->off;
	while(ep-p >= 4){
		id = get2(p);
		n = get2(p+2);
		p += 4;
		if(n > ep-p)
			break;
		f = p;
		p += n;
		switch(id){
		case ZX64:
			if(size32 == ZMax32){
				if(p-f < 8)
					goto Short;
				size = get8(f);
				f += 8;
			}
			if(csize32 == ZMax32){
				if(p-f < 8)
					goto Short;
				csize = get8(f);
				f += 8;
			}
			if(off == ZMax32){
				if(p-f < 8)
					goto Short;
				off = get8(f);
			}
			break;
		case ZXNtfs:
			ntfstime(f, p, mtime);
			break;
		case ZXUnix:
		case ZXInfo:
			if(n >= 8)
				*mtime = get4(f+4);
			break;
		case ZXTime:
			if(n >= 5 && (f[0] & 1))
				*mtime = get4(f+1);
			break;
		}
	}
	if((vlong)size < 0 || (vlong)csize < 0 || (vlong)off < 0){
		m->badsize = 1;
		m->size = 0;
		m->off = 0;
		return 0;
	}
	m->size = size;
	m->csize = csize;
	m->off = off;
	return 0;
Short:
	werrstr("short zip64 field");
	return -1;
}

static int
readdir(void)
{
	End d;
	Member *m;
	uchar *b, *p, *ep, *x;
	char *name;
	vlong at, base;
	uvlong n;
	long len, mtime;
	int flen, xlen, clen;

	if((at = finddir(&d, &base)) < 0)
		return -1;
	len = d.end-at;
	if((b = malloc(len+1)) == nil){
		werrstr("no memory for the directory");
		return -1;
	}
	if(readat(zfd, b, len, at) < 0){
		free(b);
		werrstr("read directory: %r");
		return -1;
	}
	ep = b+len;
	n = 0;
	for(p = b; ep-p >= ZCHeadSize && get4(p) == ZCHeader; p = x+xlen+clen){
		flen = get2(p+28);
		xlen = get2(p+30);
		clen = get2(p+32);
		if(flen+xlen+clen > ep-p-ZCHeadSize)
			break;
		m = emalloc(sizeof *m);
		m->flags = get2(p+8);
		m->meth = get2(p+10);
		m->crc = get4(p+16);
		m->csize = get4(p+20);
		m->size = get4(p+24);
		m->off = get4(p+42);
		m->body = -1;
		m->fd = -1;
		mtime = msdos2time(get2(p+12), get2(p+14));
		x = p+ZCHeadSize+flen;
		n++;
		if(readextra(m, x, x+xlen, &mtime) < 0){
			werrstr("entry %llud: %r", n);
			free(m);
			free(b);
			return -1;
		}
		m->off += base;
		name = decodename(p+ZCHeadSize, flen);
		if(addmember(name, m, mtime) < 0){
			free(name);
			free(b);
			return -1;
		}
		free(name);
	}
	free(b);
	if((n & ZMax16) != (d.count & ZMax16)){
		werrstr("directory has %llud of %llud entries", n, d.count);
		return -1;
	}
	return 0;
}

static void
droptemp(Member *m)
{
	m->next = nil;
	close(m->fd);
	m->fd = -1;
	freed = nsec();
}

static void
trim(void)
{
	Member *m, **l;
	vlong size;
	int n;

	n = 0;
	size = 0;
	for(l = &cached; (m = *l) != nil; ){
		if(m->ram->open == 0){
			if(n == Nclosed || m->size > Maxclosed-size){
				*l = m->next;
				droptemp(m);
				continue;
			}
			n++;
			size += m->size;
		}
		l = &m->next;
	}
}

static void
dropclosed(void)
{
	Member *m, **l;

	for(l = &cached; (m = *l) != nil; ){
		if(m->ram->open == 0){
			*l = m->next;
			droptemp(m);
			continue;
		}
		l = &m->next;
	}
}

static void
uncache(Member *m)
{
	Member **l;

	for(l = &cached; *l != nil; l = &(*l)->next)
		if(*l == m){
			*l = m->next;
			break;
		}
	m->next = nil;
	close(m->fd);
	m->fd = -1;
}

static void
closed(Ram *r)
{
	USED(r);
	trim();
}

static int
unchanged(void)
{
	Dir *d;

	if(!changed){
		if((d = dirfstat(zfd)) == nil){
			werrstr("stat archive: %r");
			return -1;
		}
		changed = d->qid.path != zdir->qid.path || d->qid.vers != zdir->qid.vers
			|| d->mtime != zdir->mtime || d->length != zdir->length;
		free(d);
	}
	if(changed){
		werrstr("archive changed");
		return -1;
	}
	return 0;
}

void
populate(char *name)
{
	int ok;

	crctab = mkcrctab(ZCrcPoly);
	ok = inflateinit();
	if(ok != FlateOk)
		sysfatal("inflateinit failed: %s", flateerr(ok));
	srand(truerand());
	if((zfd = open(name, OREAD)) < 0)
		sysfatal("open %s: %r", name);
	if((zdir = dirfstat(zfd)) == nil)
		sysfatal("stat %s: %r", name);
	zsize = zdir->length;
	if(readdir() < 0)
		sysfatal("%s: %r", name);
	replete = 1;
	lastclose = closed;
	checkempty = doempty;
	findchild = findnode;
}

void
dotrunc(Ram *r)
{
	USED(r);
}

void
docreate(Ram *r)
{
	USED(r);
}

static int
findbody(Member *m)
{
	uchar h[ZHeadSize];
	vlong body;

	if(m->body >= 0)
		return 0;
	if(m->badsize){
		werrstr("size out of range");
		return -1;
	}
	if(m->flags & ZEncrypted){
		werrstr("encrypted");
		return -1;
	}
	if(m->meth != ZStored && m->meth != ZDeflate){
		werrstr("compression method %d not supported", m->meth);
		return -1;
	}
	if(m->meth == ZStored && m->csize != m->size){
		werrstr("stored sizes differ");
		return -1;
	}
	if(m->size/Maxratio > m->csize){
		werrstr("size is impossible for deflate");
		return -1;
	}
	if(m->off < 0 || m->off > zsize-ZHeadSize){
		werrstr("header is outside the file");
		return -1;
	}
	if(readat(zfd, h, sizeof h, m->off) < 0){
		werrstr("read header: %r");
		return -1;
	}
	if(get4(h) != ZHeader){
		werrstr("bad local header");
		return -1;
	}
	body = m->off + ZHeadSize + get2(h+26) + get2(h+28);
	if(body > zsize || m->csize > zsize-body){
		werrstr("data runs past the end of the file");
		return -1;
	}
	m->body = body;
	return 0;
}

static int
checkcrc(Member *m, ulong crc)
{
	uchar d[16];
	vlong at;
	ulong v;

	if(m->flags & ZTrailInfo){
		at = m->body+m->csize;
		if(readat(zfd, d, 4, at) < 0)
			goto Nodescr;
		v = get4(d);
		if(v == ZDescr){
			if(readat(zfd, d, 12, at+4) < 0)
				goto Nodescr;
			v = get4(d);
		}else if(readat(zfd, d, 12, at) < 0)
			goto Nodescr;
		if(v != m->crc){
			werrstr("descriptor crc mismatch");
			return -1;
		}
	}else if(m->crc == 0)
		return 0;
	if(crc == m->crc)
		return 0;
	werrstr("crc mismatch");
	return -1;
Nodescr:
	werrstr("read descriptor: %r");
	return -1;
}

static int
readstored(Member *m, char *buf, long n, vlong off)
{
	long skip;

	if(readat(zfd, buf, n, m->body+off) < 0)
		return -1;
	if(off <= m->summed && off+n > m->summed){
		skip = m->summed-off;
		m->sum = blockcrc(crctab, m->sum, buf+skip, n-skip);
		m->summed = off+n;
		if(m->summed == m->size)
			m->badsum = checkcrc(m, m->sum) < 0;
	}
	if(m->badsum){
		werrstr("crc mismatch");
		return -1;
	}
	return 0;
}

static int
infget(void *v)
{
	Infl *z;
	long n;

	z = v;
	if(z->p < z->ep)
		return *z->p++;
	if(z->left == 0)
		return -1;
	n = sizeof z->in;
	if(n > z->left)
		n = z->left;
	if(readat(zfd, z->in, n, z->off) < 0){
		rerrstr(z->err, sizeof z->err);
		return -1;
	}
	z->off += n;
	z->left -= n;
	z->p = z->in;
	z->ep = z->in+n;
	return *z->p++;
}

static int
infput(void *v, void *buf, int n)
{
	Infl *z;
	int m;

	z = v;
	if(n > z->m->size - z->n){
		snprint(z->err, sizeof z->err, "more data than the directory says");
		return -1;
	}
	if((m = write(z->fd, buf, n)) != n){
		if(m >= 0)
			werrstr("short write");
		rerrstr(z->err, sizeof z->err);
		z->tmperr = 1;
		return -1;
	}
	z->crc = blockcrc(crctab, z->crc, buf, n);
	z->n += n;
	return n;
}

static int
tmpfile(void)
{
	char name[64];
	int fd, i;

	for(i = 0; i < 10; i++){
		snprint(name, sizeof name, "/tmp/zipfs.%.8lux%.8lux", truerand(), truerand());
		if((fd = create(name, ORDWR|OEXCL|ORCLOSE, 0600)) >= 0)
			return fd;
	}
	werrstr("create %s: %r", name);
	return -1;
}

static int
inflateto(Infl *z, Member *m, int fd)
{
	int err;

	memset(z, 0, sizeof *z);
	z->m = m;
	z->fd = fd;
	z->off = m->body;
	z->left = m->csize;
	err = inflate(z, infput, z, infget);
	if(z->err[0]){
		werrstr("inflate: %s", z->err);
		return -1;
	}
	if(err != FlateOk){
		werrstr("inflate: %s", flateerr(err));
		return -1;
	}
	if(z->n != m->size){
		werrstr("less data than the directory says");
		return -1;
	}
	return checkcrc(m, z->crc);
}

static int
cachemember(Member *m)
{
	static Infl z;
	char err[ERRMAX];
	int fd;

	trim();
	for(;;){
		if((fd = tmpfile()) >= 0){
			if(inflateto(&z, m, fd) == 0)
				break;
			rerrstr(err, sizeof err);
			close(fd);
			if(!z.tmperr){
				werrstr("%s", err);
				return -1;
			}
		}else
			rerrstr(err, sizeof err);
		dropclosed();
		if(freed == 0 || nsec()-freed > Freewait*1000000LL){
			werrstr("%s", err);
			return -1;
		}
		sleep(Freetick);
	}
	m->fd = fd;
	m->next = cached;
	cached = m;
	return 0;
}

static int
doempty(Ram *r)
{
	static Infl z;
	Member *m;

	if((m = r->data) == nil)
		return 0;
	if(!m->checked){
		if(findbody(m) < 0)
			return -1;
		if(m->meth == ZStored){
			if(checkcrc(m, 0) < 0)
				return -1;
		}else if(inflateto(&z, m, -1) < 0)
			return -1;
	}
	if(unchanged() < 0)
		return -1;
	m->checked = 1;
	return 0;
}

static void
touch(Member *m)
{
	Member **l;

	for(l = &cached; *l != nil; l = &(*l)->next)
		if(*l == m){
			*l = m->next;
			m->next = cached;
			cached = m;
			return;
		}
}

static int
readdeflated(Member *m, char *buf, long n, vlong off)
{
	if(m->fd >= 0){
		if(readat(m->fd, buf, n, off) == 0){
			touch(m);
			return 0;
		}
		uncache(m);
	}
	if(cachemember(m) < 0)
		return -1;
	if(readat(m->fd, buf, n, off) < 0){
		werrstr("read temporary file: %r");
		return -1;
	}
	return 0;
}

char *
doread(Ram *r, vlong off, long cnt)
{
	static char buf[Maxbuf];
	Member *m;
	int rv;

	m = r->data;
	if(cnt > Maxbuf)
		cnt = Maxbuf;
	if(findbody(m) < 0)
		return nil;
	if(m->meth == ZStored)
		rv = readstored(m, buf, cnt, off);
	else
		rv = readdeflated(m, buf, cnt, off);
	if(unchanged() < 0 || rv < 0)
		return nil;
	return buf;
}

void
popdir(Ram *r)
{
	USED(r);
}

void
dowrite(Ram *r, char *buf, long off, long cnt)
{
	USED(r); USED(buf); USED(off); USED(cnt);
}

int
dopermw(Ram *r)
{
	USED(r);
	return 0;
}
