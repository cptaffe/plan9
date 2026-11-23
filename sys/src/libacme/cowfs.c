#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>
#include <libsec.h>
#include "cow.h"

typedef struct F F;
typedef struct Q Q;
typedef struct Msg Msg;
typedef struct Lock1 Lock1;
typedef struct Bdir Bdir;

enum
{
	Nhash = 4096,
	Nwork = 8,
	Stack = 32*1024,

	Mreq = 0,
	Mlook,
	Mqid,
	Mlock,
	Munlock,
	Madd,
	Mdel,
	Mnote,
	Mgone,
	Mungone,
	Mrecs,
	Mdrop,
	Midle,
	Mbatch,
	Mwhere,
	Mbase,
	Mbaseput,
	Mclunk,
	Mmove,
};

struct F
{
	char	*name;
	int	fd;
	Dir	*dl;
	long	nd;
	F	*next;
};

struct Q
{
	char	*name;
	uvlong	path;
	int	where;
	Dir	*base;
	int	bknown;
	Q	*next;
};

struct Lock1
{
	char	*name;
	Channel	*c;
	Lock1	*next;
};

struct Msg
{
	int	type;
	Req	*r;
	F	*f;
	char	*name;
	char	*to;
	Channel	*reply;
	int	v;
	uvlong	q;
	ulong	vers;
	char	*hash;
	Dir	*d;
	char	**names;
	int	n;
	uvlong	*qs;
	int	*gone;
};

char	*base;
char	*shadow;
char	*owner;
int	fixed;
Rec	*recs;
Q	*qtab[Nhash];
Lock1	*locks;
Channel	*ownc;
Channel	*savec;
Req	*queue;
Req	*qtail;
F	*clunks;

static char Enoexist[] = "file does not exist";
static char Eperm[] = "permission denied";
static char Ecow[] = "only notes is kept in .cow";

static ulong
hashname(char *p)
{
	ulong h;

	for(h = 0; *p; p++)
		h = h*31 + (uchar)*p;
	return h % Nhash;
}

static uvlong
pathq(char *p)
{
	uchar d[SHA2_256dlen];
	uvlong q;
	int i;

	sha2_256((uchar*)p, strlen(p), d, nil);
	q = 0;
	for(i = 0; i < 8; i++)
		q = q<<8 | d[i];
	return q;
}

static Q*
qof(char *name)
{
	Q *q;
	ulong h;

	h = hashname(name);
	for(q = qtab[h]; q != nil; q = q->next)
		if(strcmp(q->name, name) == 0)
			return q;
	q = emalloc9p(sizeof *q);
	q->name = estrdup9p(name);
	q->path = pathq(name);
	q->next = qtab[h];
	qtab[h] = q;
	return q;
}

static char*
jn(char *a, char *b)
{
	return *a == 0 ? estrdup9p(b) : smprint("%s/%s", a, b);
}

static char*
bpath(char *n)
{
	return *n == 0 ? estrdup9p(base) : smprint("%s/%s", base, n);
}

static char*
spath(char *n)
{
	return *n == 0 ? estrdup9p(shadow) : smprint("%s/%s", shadow, n);
}

static char*
parent(char *n)
{
	char *p, *s;

	s = estrdup9p(n);
	if((p = strrchr(s, '/')) != nil)
		*p = 0;
	else
		*s = 0;
	return s;
}

static char*
last(char *n)
{
	char *p;

	if(*n == 0)
		return "/";
	return (p = strrchr(n, '/')) != nil ? p+1 : n;
}

static int
iscow(char *n)
{
	return strcmp(n, ".cow") == 0 || strncmp(n, ".cow/", 5) == 0;
}

static Dir*
dupdir(Dir *d)
{
	Dir *n;
	uchar *b;
	uint m;

	m = sizeD2M(d);
	b = emalloc9p(m);
	convD2M(d, b, m);
	n = emalloc9p(sizeof(Dir) + m);
	convM2D(b, m, n, (char*)&n[1]);
	free(b);
	return n;
}

static void
markshadow(char *dir, char *rel)
{
	Dir *dl;
	long n, i;
	int fd;
	char *p, *s;

	if((fd = open(dir, OREAD)) < 0)
		return;
	n = dirreadall(fd, &dl);
	close(fd);
	for(i = 0; i < n; i++){
		if(*rel == 0 && strcmp(dl[i].name, ".cow") == 0)
			continue;
		p = jn(rel, dl[i].name);
		qof(p)->where = dl[i].mode & DMDIR ? 'S' : 's';
		if(dl[i].mode & DMDIR){
			s = smprint("%s/%s", dir, dl[i].name);
			markshadow(s, p);
			free(s);
		}
		free(p);
	}
	free(dl);
}

static void tell(Msg*);
static Msg *msg(int, char*);

static void
addtree(char *dir, char *rel)
{
	Dir *dl;
	long n, i;
	int fd;
	char *p, *s;
	Msg *m;

	if((fd = open(dir, OREAD)) < 0)
		return;
	n = dirreadall(fd, &dl);
	close(fd);
	for(i = 0; i < n; i++){
		p = jn(rel, dl[i].name);
		m = msg(Madd, p);
		m->v = (dl[i].mode & DMDIR) != 0;
		tell(m);
		if(dl[i].mode & DMDIR){
			s = smprint("%s/%s", dir, dl[i].name);
			addtree(s, p);
			free(s);
		}
		free(p);
	}
	free(dl);
}

static void
saverecs(void)
{
	Fmt f;
	Rec *r;

	fmtstrinit(&f);
	for(r = recs; r != nil; r = r->next)
		fmtprint(&f, "%c %lud %s %q\n", reckind(r), r->vers, r->hash, r->path);
	sendp(savec, fmtstrflush(&f));
}

static void
saveproc(void*)
{
	char *s, *f, *t, *n;
	int fd;
	Dir d;

	f = smprint("%s/.cow/files", shadow);
	t = smprint("%s/.cow/files.new", shadow);
	for(;;){
		if((s = recvp(savec)) == nil)
			continue;
		while(nbrecv(savec, &n) > 0){
			free(s);
			s = n;
		}
		if(access(f, AEXIST) < 0 && access(t, AEXIST) == 0){
			nulldir(&d);
			d.name = "files";
			if(dirwstat(t, &d) < 0){
				free(s);
				continue;
			}
		}
		if((fd = create(t, OWRITE, 0666)) >= 0){
			if(write(fd, s, strlen(s)) == strlen(s)){
				close(fd);
				remove(f);
				nulldir(&d);
				d.name = "files";
				dirwstat(t, &d);
			}else
				close(fd);
		}
		free(s);
	}
}

static int
gone(char *n)
{
	Rec *r;

	char *p;
	int o;

	r = findrec(recs, n);
	if(r != nil && r->gone)
		return 1;
	o = 0;
	for(p = n; !o && (p = strchr(p, '/')) != nil; p++){
		*p = 0;
		r = findrec(recs, n);
		o = r != nil && r->opaque;
		*p = '/';
	}
	return o;
}

static void
lockname(Msg *m)
{
	Lock1 *l;

	for(l = locks; l != nil; l = l->next)
		if(strcmp(l->name, m->name) == 0)
			break;
	if(l == nil){
		l = emalloc9p(sizeof *l);
		l->name = estrdup9p(m->name);
		l->c = chancreate(sizeof(Channel*), 64);
		l->next = locks;
		locks = l;
		sendul(m->reply, 1);
		return;
	}
	sendp(l->c, m->reply);
}

static void
unlockname(char *name)
{
	Lock1 *l, **ll;
	Channel *c;

	for(ll = &locks; (l = *ll) != nil; ll = &l->next)
		if(strcmp(l->name, name) == 0)
			break;
	if(l == nil)
		return;
	if(nbrecv(l->c, &c) > 0){
		sendul(c, 1);
		return;
	}
	*ll = l->next;
	chanfree(l->c);
	free(l->name);
	free(l);
}

static int
where(char *n)
{
	Q *q;

	if(*n == 0)
		return 'b';
	if(strcmp(n, ".cow") == 0)
		return 'c';
	if(strcmp(n, ".cow/ctl") == 0)
		return 'k';
	if(iscow(n))
		return strcmp(n, ".cow/notes") == 0 ? 's' : 'n';
	q = qof(n);
	if(q->where == 's' || q->where == 'S')
		return 's';
	if(gone(n))
		return 'n';
	return 'b';
}

static void
unmark(char *n)
{
	Q *q;
	int i, l;

	l = strlen(n);
	for(i = 0; i < Nhash; i++)
		for(q = qtab[i]; q != nil; q = q->next)
			if(strncmp(q->name, n, l) == 0 && (q->name[l] == 0 || q->name[l] == '/'))
				q->where = 0;
}

static void
owned(Msg *m)
{
	Q *q;
	Rec *r;
	int i;

	switch(m->type){
	case Mlook:
		m->v = where(m->name);
		q = qof(m->name);
		m->q = q->path;
		if(fixed && m->v == 'b' && q->bknown){
			m->d = q->base != nil ? dupdir(q->base) : nil;
			m->v = q->base != nil ? 'B' : 'n';
		}
		sendp(m->reply, m);
		return;
	case Mqid:
		m->q = qof(m->name)->path;
		sendp(m->reply, m);
		return;
	case Mbaseput:
		q = qof(m->name);
		if(fixed && !q->bknown){
			q->bknown = 1;
			q->base = m->d;
			m->d = nil;
		}
		free(m->d);
		break;
	case Mlock:
		lockname(m);
		return;
	case Munlock:
		unlockname(m->name);
		break;
	case Madd:
		qof(m->name)->where = m->v ? 'S' : 's';
		break;
	case Mdel:
		qof(m->name)->where = 0;
		break;
	case Mmove:
		unmark(m->name);
		qof(m->to)->where = m->v ? 'S' : 's';
		break;
	case Mnote:
		if(findrec(recs, m->name) == nil){
			r = addrec(&recs, m->name, m->vers, m->hash);
			r->gone = 0;
		}
		saverecs();
		break;
	case Mgone:
		if((r = findrec(recs, m->name)) == nil)
			r = addrec(&recs, m->name, m->vers, m->hash);
		r->gone = 1;
		saverecs();
		break;
	case Mungone:
		if((r = findrec(recs, m->name)) != nil && r->gone){
			r->gone = 0;
			r->opaque = m->v;
			saverecs();
		}
		break;
	case Mrecs:
		m->v = gone(m->name);
		sendp(m->reply, m);
		return;
	case Mbase:
		q = qof(m->name);
		m->v = fixed && q->bknown;
		m->d = q->base != nil && m->v ? dupdir(q->base) : nil;
		sendp(m->reply, m);
		return;
	case Mwhere:
		m->v = qof(m->name)->where;
		sendp(m->reply, m);
		return;
	case Mbatch:
		for(i = 0; i < m->n; i++){
			m->qs[i] = qof(m->names[i])->path;
			m->gone[i] = gone(m->names[i]);
		}
		sendp(m->reply, m);
		return;
	case Mdrop:
		unmark(m->name);
		delrec(&recs, m->name);
		saverecs();
		break;
	}
	free(m->name);
	free(m->to);
	free(m->hash);
	free(m);
}

static void
give(Channel *c)
{
	Msg *w;

	w = emalloc9p(sizeof *w);
	if(queue != nil){
		w->type = Mreq;
		w->r = queue;
		queue = queue->aux;
		if(queue == nil)
			qtail = nil;
	}else{
		w->type = Mclunk;
		w->f = clunks;
		clunks = clunks->next;
	}
	sendp(c, w);
}

static void
ownerproc(void*)
{
	Msg *m;
	Channel *idle[Nwork];
	int nidle;

	nidle = 0;
	for(;;){
		if((m = recvp(ownc)) == nil)
			threadexitsall(nil);
		switch(m->type){
		case Mreq:
			m->r->aux = nil;
			if(qtail != nil)
				qtail->aux = m->r;
			else
				queue = m->r;
			qtail = m->r;
			free(m);
			break;
		case Mclunk:
			m->f->next = clunks;
			clunks = m->f;
			free(m);
			break;
		case Midle:
			idle[nidle++] = m->reply;
			free(m);
			break;
		default:
			owned(m);
		}
		while(nidle > 0 && (queue != nil || clunks != nil))
			give(idle[--nidle]);
	}
}

static Msg*
msg(int type, char *name)
{
	Msg *m;

	m = emalloc9p(sizeof *m);
	m->type = type;
	m->name = name != nil ? estrdup9p(name) : nil;
	return m;
}

static Msg*
ask(Msg *m, Channel *c)
{
	m->reply = c;
	sendp(ownc, m);
	return recvp(c);
}

static void
tell(Msg *m)
{
	sendp(ownc, m);
}

static void
lock1(char *n, Channel *c)
{
	Msg *m;

	m = msg(Mlock, n);
	m->reply = c;
	sendp(ownc, m);
	recvul(c);
	free(m->name);
	free(m);
}

static void
unlock1(char *n)
{
	tell(msg(Munlock, n));
}

static Dir*
cowdir(void)
{
	Dir *d;

	d = emalloc9p(sizeof *d);
	nulldir(d);
	d->name = ".cow";
	d->uid = d->gid = d->muid = owner;
	d->mode = DMDIR|0777;
	d->qid.type = QTDIR;
	d->qid.vers = 0;
	d->length = 0;
	d->atime = d->mtime = time(0);
	return d;
}

static Dir*
look(char *n, int *w, uvlong *qp, Channel *c)
{
	Msg *m;
	Dir *d;
	char *p;

	m = ask(msg(Mlook, n), c);
	*w = m->v;
	*qp = m->q;
	d = m->d;
	free(m->name);
	free(m);
	switch(*w){
	case 'c':
		return cowdir();
	case 'k':
		d = cowdir();
		d->name = "ctl";
		d->mode = 0222;
		d->qid.type = QTFILE;
		return d;
	case 'n':
		return nil;
	case 'B':
		*w = 'b';
		return d;
	case 's':
		p = spath(n);
		d = dirstat(p);
		free(p);
		if(d != nil || iscow(n))
			return d;
		*w = 'b';
	}
	p = bpath(n);
	d = dirstat(p);
	free(p);
	if(fixed && *w == 'b'){
		m = msg(Mbaseput, n);
		m->d = d != nil ? dupdir(d) : nil;
		tell(m);
	}
	return d;
}

static Dir*
basestat(char *n, Channel *c)
{
	Msg *m;
	Dir *d;
	char *p;

	m = ask(msg(Mbase, n), c);
	if(m->v){
		d = m->d;
		free(m->name);
		free(m);
		return d;
	}
	free(m->name);
	free(m);
	p = bpath(n);
	d = dirstat(p);
	free(p);
	if(fixed){
		m = msg(Mbaseput, n);
		m->d = d != nil ? dupdir(d) : nil;
		tell(m);
	}
	return d;
}

static int
inshadow(char *n, Channel *c)
{
	Msg *m;
	int w;

	m = ask(msg(Mwhere, n), c);
	w = m->v;
	free(m->name);
	free(m);
	return w;
}

static void
fixqid(Dir *d, uvlong q, int w)
{
	ulong v;

	v = d->qid.vers;
	if(w == 's')
		v = (v ^ (ulong)d->qid.path*2654435761UL ^ (ulong)(d->qid.path>>32)) << 1 | 1;
	else
		v <<= 1;
	d->qid.vers = v;
	d->qid.path = q;
	d->qid.type = d->mode>>24;
}

static void
copydir(Dir *t, Dir *d, char *name)
{
	*t = *d;
	t->name = estrdup9p(name);
	t->uid = estrdup9p(d->uid);
	t->gid = estrdup9p(d->gid);
	t->muid = estrdup9p(d->muid);
}

static int
isdir(char *p)
{
	Dir *d;
	int r;

	if((d = dirstat(p)) == nil)
		return 0;
	r = (d->mode&DMDIR) != 0;
	free(d);
	return r;
}

static long
readdir(char *p, Dir **dl)
{
	Dir *d;
	long n;
	int fd;

	*dl = nil;
	if((fd = open(p, OREAD)) < 0)
		return 0;
	n = 0;
	if((d = dirfstat(fd)) != nil && d->mode&DMDIR)
		n = dirreadall(fd, dl);
	free(d);
	close(fd);
	return n < 0 ? 0 : n;
}

static int
mkparents(char *n, Channel *c)
{
	char *s, *p, *sp;
	Dir *d;
	ulong perm;
	int fd;
	Msg *m;

	s = estrdup9p(n);
	if((p = strrchr(s, '/')) != nil){
		*p = 0;
		if(inshadow(s, c) == 'S'){
			free(s);
			return 0;
		}
		*p = '/';
	}
	for(p = s; (p = strchr(p, '/')) != nil; p++){
		*p = 0;
		sp = spath(s);
		if(inshadow(s, c) != 'S' && access(sp, AEXIST) < 0){
			perm = 0777;
			if((d = basestat(s, c)) != nil){
				perm = d->mode & 0777;
				free(d);
			}
			if((fd = create(sp, OREAD, DMDIR|perm)) < 0 && !isdir(sp)){
				free(sp);
				free(s);
				return -1;
			}
			if(fd >= 0)
				close(fd);
			m = msg(Madd, s);
			m->v = 1;
			tell(m);
		}
		free(sp);
		*p = '/';
	}
	free(s);
	return 0;
}

static void
note(char *n, int g, int dir, Channel *c)
{
	Msg *m;
	Dir *d;
	char *p, *h;

	if((d = basestat(n, c)) == nil){
		if(!g){
			m = msg(Mungone, n);
			m->v = dir;
			tell(m);
		}
		return;
	}
	p = bpath(n);
	h = d->mode&DMDIR ? estrdup9p("-") : hashfile(p);
	m = msg(g ? Mgone : Mnote, n);
	m->vers = d->qid.vers;
	m->hash = h != nil ? h : estrdup9p("-");
	tell(m);
	if(!g){
		m = msg(Mungone, n);
		m->v = dir;
		tell(m);
	}
	free(d);
	free(p);
}

static char*
copyup(char *n, Channel *c)
{
	char *bp, *sp, *b;
	Dir *d;
	long len;
	int fd;
	Msg *m;

	if(mkparents(n, c) < 0)
		return "cannot make the shadow's directories";
	bp = bpath(n);
	sp = spath(n);
	d = dirstat(bp);
	if(d == nil){
		free(bp);
		free(sp);
		return Enoexist;
	}
	b = nil;
	len = 0;
	if(d->mode&DMDIR)
		fd = create(sp, OREAD, DMDIR|(d->mode&0777));
	else{
		if((fd = open(bp, OREAD)) < 0 || (b = readall(fd, &len)) == nil){
			if(fd >= 0)
				close(fd);
			free(d);
			free(bp);
			free(sp);
			return "cannot read base";
		}
		close(fd);
		fd = create(sp, OWRITE, d->mode&0777);
		if(fd >= 0 && write(fd, b, len) != len){
			close(fd);
			fd = -1;
		}
	}
	free(bp);
	free(sp);
	if(fd < 0){
		free(b);
		free(d);
		return "cannot copy into the shadow";
	}
	close(fd);
	m = msg(Madd, n);
	m->v = (d->mode&DMDIR) != 0;
	tell(m);
	m = msg(Mnote, n);
	m->vers = d->qid.vers;
	m->hash = d->mode&DMDIR ? estrdup9p("-") : hashtext(b, len);
	tell(m);
	tell(msg(Mungone, n));
	free(b);
	free(d);
	return nil;
}

static int
inlist(Dir *dl, long n, char *name)
{
	long i;

	for(i = 0; i < n; i++)
		if(strcmp(dl[i].name, name) == 0)
			return 1;
	return 0;
}

static void
list(F *f, Channel *c)
{
	Dir *sd, *bd, *x, *dl;
	long ns, nb, i, j, n, *k;
	char *p;
	Msg *m, *b;

	ns = nb = 0;
	sd = bd = nil;
	if(strcmp(f->name, ".cow") == 0){
		dl = emalloc9p(sizeof dl[0]);
		n = 0;
		p = spath(".cow/notes");
		if((x = dirstat(p)) != nil){
			copydir(&dl[0], x, "notes");
			m = ask(msg(Mqid, ".cow/notes"), c);
			fixqid(&dl[0], m->q, 's');
			free(m->name);
			free(m);
			free(x);
			n = 1;
		}
		free(p);
		f->dl = dl;
		f->nd = n;
		return;
	}
	p = spath(f->name);
	ns = readdir(p, &sd);
	free(p);
	p = bpath(f->name);
	nb = readdir(p, &bd);
	free(p);
	dl = emalloc9p((ns+nb+1)*sizeof dl[0]);
	n = 0;
	if(*f->name == 0){
		x = cowdir();
		copydir(&dl[n], x, ".cow");
		m = ask(msg(Mqid, ".cow"), c);
		fixqid(&dl[n], m->q, 'c');
		free(m->name);
		free(m);
		n++;
		free(x);
	}
	b = msg(Mbatch, nil);
	b->names = emalloc9p((ns+nb+1)*sizeof b->names[0]);
	b->qs = emalloc9p((ns+nb+1)*sizeof b->qs[0]);
	b->gone = emalloc9p((ns+nb+1)*sizeof b->gone[0]);
	k = emalloc9p((ns+nb+1)*sizeof k[0]);
	for(i = 0; i < ns+nb; i++){
		x = i < ns ? &sd[i] : &bd[i-ns];
		if(*f->name == 0 && strcmp(x->name, ".cow") == 0)
			continue;
		if(i >= ns && inlist(sd, ns, x->name))
			continue;
		k[b->n] = i;
		b->names[b->n++] = jn(f->name, x->name);
	}
	ask(b, c);
	for(j = 0; j < b->n; j++){
		i = k[j];
		x = i < ns ? &sd[i] : &bd[i-ns];
		if(i < ns || !b->gone[j]){
			copydir(&dl[n], x, x->name);
			fixqid(&dl[n], b->qs[j], i < ns ? 's' : 'b');
			n++;
		}
		free(b->names[j]);
	}
	free(b->names);
	free(b->qs);
	free(b->gone);
	free(b);
	free(k);
	free(sd);
	free(bd);
	f->dl = dl;
	f->nd = n;
}

static void
freelist(F *f)
{
	long i;

	for(i = 0; i < f->nd; i++){
		free(f->dl[i].name);
		free(f->dl[i].uid);
		free(f->dl[i].gid);
		free(f->dl[i].muid);
	}
	free(f->dl);
	f->dl = nil;
	f->nd = 0;
}

static F*
newf(char *name)
{
	F *f;

	f = emalloc9p(sizeof *f);
	f->name = estrdup9p(name);
	f->fd = -1;
	return f;
}

static void
fsattach(Req *r, Channel *c)
{
	Dir *d;
	int w;
	uvlong q;

	if((d = look("", &w, &q, c)) == nil){
		respond(r, "no base");
		return;
	}
	r->fid->aux = newf("");
	fixqid(d, q, w);
	r->fid->qid = d->qid;
	r->ofcall.qid = r->fid->qid;
	free(d);
	respond(r, nil);
}

static void
fswalk(Req *r, Channel *c)
{
	F *f, *nf;
	Dir *d;
	char *n, *cur;
	int i, w;
	uvlong q;

	f = r->fid->aux;
	cur = estrdup9p(f->name);
	for(i = 0; i < r->ifcall.nwname; i++){
		n = strcmp(r->ifcall.wname[i], "..") == 0 ? parent(cur) : jn(cur, r->ifcall.wname[i]);
		if((d = look(n, &w, &q, c)) == nil){
			free(n);
			break;
		}
		fixqid(d, q, w);
		if(w == 'c')
			d->qid.type = QTDIR;
		r->ofcall.wqid[i] = d->qid;
		free(d);
		free(cur);
		cur = n;
	}
	r->ofcall.nwqid = i;
	if(i < r->ifcall.nwname){
		free(cur);
		respond(r, i == 0 ? Enoexist : nil);
		return;
	}
	if(r->fid == r->newfid){
		free(f->name);
		f->name = cur;
	}else{
		nf = newf(cur);
		free(cur);
		r->newfid->aux = nf;
	}
	if(i > 0)
		r->newfid->qid = r->ofcall.wqid[i-1];
	respond(r, nil);
}

static void
fsopen(Req *r, Channel *c)
{
	F *f;
	Dir *d;
	char *p, *e;
	int w, m;
	uvlong q;

	f = r->fid->aux;
	if((d = look(f->name, &w, &q, c)) == nil){
		respond(r, Enoexist);
		return;
	}
	m = r->ifcall.mode & ~ORCLOSE;
	if(w == 'k'){
		free(d);
		r->ofcall.qid = r->fid->qid;
		respond(r, (m&3) == OWRITE ? nil : Eperm);
		return;
	}
	if(d->mode&DMDIR || w == 'c'){
		free(d);
		if((m&3) != OREAD || m&OTRUNC){
			respond(r, "is a directory");
			return;
		}
		list(f, c);
		r->ofcall.qid = r->fid->qid;
		respond(r, nil);
		return;
	}
	free(d);
	if(w == 'b' && ((m&3) == OWRITE || (m&3) == ORDWR || m&OTRUNC)){
		lock1(f->name, c);
		look(f->name, &w, &q, c);
		e = w == 'b' ? copyup(f->name, c) : nil;
		unlock1(f->name);
		if(e != nil){
			respond(r, e);
			return;
		}
		w = 's';
	}
	p = w == 's' ? spath(f->name) : bpath(f->name);
	f->fd = open(p, m);
	free(p);
	if(f->fd < 0){
		responderror(r);
		return;
	}
	if(w == 's' && (d = dirfstat(f->fd)) != nil){
		fixqid(d, q, w);
		r->fid->qid = d->qid;
		free(d);
	}
	r->ofcall.qid = r->fid->qid;
	respond(r, nil);
}

static void
fscreate(Req *r, Channel *c)
{
	F *f;
	Dir *d;
	char *n, *p;
	int w, fd;
	uvlong q;
	Msg *m;

	f = r->fid->aux;
	n = jn(f->name, r->ifcall.name);
	if(iscow(n) && strcmp(n, ".cow/notes") != 0 || strcmp(n, ".cow") == 0){
		free(n);
		respond(r, Ecow);
		return;
	}
	lock1(n, c);
	if((d = look(n, &w, &q, c)) != nil){
		free(d);
		if(r->ifcall.perm&DMDIR || w == 'c'){
			unlock1(n);
			free(n);
			respond(r, "file already exists");
			return;
		}
	}
	if(mkparents(n, c) < 0){
		unlock1(n);
		free(n);
		respond(r, "cannot make the shadow's directories");
		return;
	}
	if(!iscow(n))
		note(n, 0, (r->ifcall.perm&DMDIR) != 0, c);
	p = spath(n);
	fd = create(p, r->ifcall.mode & ~ORCLOSE, r->ifcall.perm);
	free(p);
	if(fd < 0){
		unlock1(n);
		free(n);
		responderror(r);
		return;
	}
	if(!iscow(n)){
		m = msg(Madd, n);
		m->v = (r->ifcall.perm&DMDIR) != 0;
		tell(m);
	}
	unlock1(n);
	free(f->name);
	f->name = n;
	if(r->ifcall.perm&DMDIR){
		close(fd);
		list(f, c);
	}else
		f->fd = fd;
	if((d = look(n, &w, &q, c)) != nil){
		fixqid(d, q, w);
		r->fid->qid = d->qid;
		free(d);
	}
	r->ofcall.qid = r->fid->qid;
	respond(r, nil);
}

static int
dirgen(int i, Dir *d, void *a)
{
	F *f;

	f = a;
	if(i >= f->nd)
		return -1;
	copydir(d, &f->dl[i], f->dl[i].name);
	return 0;
}

static void
fsread(Req *r)
{
	F *f;
	long n;

	f = r->fid->aux;
	if(r->fid->qid.type&QTDIR){
		dirread9p(r, dirgen, f);
		respond(r, nil);
		return;
	}
	if((n = pread(f->fd, r->ofcall.data, r->ifcall.count, r->ifcall.offset)) < 0){
		responderror(r);
		return;
	}
	r->ofcall.count = n;
	respond(r, nil);
}

static int gonename(char*, Channel*);

static char*
drop(char *n, Channel *c)
{
	char *p, *e;
	int w;
	uvlong q;
	Dir *d;

	e = nil;
	lock1(n, c);
	if((d = look(n, &w, &q, c)) != nil && w == 's'){
		p = spath(n);
		if(d->mode&DMDIR || remove(p) < 0)
			e = "cannot drop it";
		free(p);
	}else if(!gonename(n, c))
		e = "not a local change";
	free(d);
	if(e == nil)
		tell(msg(Mdrop, n));
	unlock1(n);
	return e;
}

static void
ctlwrite(Req *r, Channel *c)
{
	char *s, *f[3], *e;
	int n;

	s = emalloc9p(r->ifcall.count+1);
	memmove(s, r->ifcall.data, r->ifcall.count);
	s[r->ifcall.count] = 0;
	if((n = tokenize(s, f, nelem(f))) == 2 && strcmp(f[0], "drop") == 0)
		e = drop(cleanname(f[1])[0] == '/' ? f[1]+1 : f[1], c);
	else
		e = n > 0 ? "unknown ctl message" : nil;
	free(s);
	if(e == nil)
		r->ofcall.count = r->ifcall.count;
	respond(r, e);
}

static int
gonename(char *n, Channel *c)
{
	Msg *m;
	int g;

	m = ask(msg(Mrecs, n), c);
	g = m->v;
	free(m->name);
	free(m);
	return g;
}

static void
fswrite(Req *r, Channel *c)
{
	F *f;
	long n;

	f = r->fid->aux;
	if(strcmp(f->name, ".cow/ctl") == 0){
		ctlwrite(r, c);
		return;
	}
	if((n = pwrite(f->fd, r->ifcall.data, r->ifcall.count, r->ifcall.offset)) < 0){
		responderror(r);
		return;
	}
	r->ofcall.count = n;
	respond(r, nil);
}

static int
inbase(char *n, Channel *c)
{
	Dir *d;

	d = basestat(n, c);
	free(d);
	return d != nil;
}

static void
fsremove(Req *r, Channel *c)
{
	F *f, t;
	Dir *d;
	char *p;
	int w, dir;
	uvlong q;

	f = r->fid->aux;
	if(*f->name == 0 || strcmp(f->name, ".cow") == 0){
		respond(r, Eperm);
		return;
	}
	lock1(f->name, c);
	if((d = look(f->name, &w, &q, c)) == nil){
		unlock1(f->name);
		respond(r, Enoexist);
		return;
	}
	dir = (d->mode&DMDIR) != 0;
	free(d);
	if(dir){
		memset(&t, 0, sizeof t);
		t.name = f->name;
		list(&t, c);
		if(t.nd > 0){
			freelist(&t);
			unlock1(f->name);
			respond(r, "directory not empty");
			return;
		}
		freelist(&t);
	}
	if(w == 's'){
		p = spath(f->name);
		if(remove(p) < 0){
			free(p);
			unlock1(f->name);
			responderror(r);
			return;
		}
		free(p);
		tell(msg(Mdel, f->name));
	}
	if(!iscow(f->name) && inbase(f->name, c))
		note(f->name, 1, 0, c);
	unlock1(f->name);
	respond(r, nil);
}

static void
fsstat(Req *r, Channel *c)
{
	F *f;
	Dir *d;
	int w;
	uvlong q;

	f = r->fid->aux;
	if((d = look(f->name, &w, &q, c)) == nil){
		respond(r, Enoexist);
		return;
	}
	fixqid(d, q, w);
	if(w == 'c')
		d->qid.type = QTDIR;
	copydir(&r->d, d, last(f->name));
	free(d);
	respond(r, nil);
}

static char *wstat1(Req*, F*, char*, Channel*);

static int
nochange(Dir *d)
{
	return d->name[0] == 0 && d->uid[0] == 0 && d->gid[0] == 0 && d->muid[0] == 0
		&& d->mode == ~0 && d->mtime == ~0 && d->atime == ~0 && d->length == ~0LL;
}

static void
fswstat(Req *r, Channel *c)
{
	F *f;
	char *e, *old, err[ERRMAX];

	f = r->fid->aux;
	if(*f->name == 0 || iscow(f->name) && strcmp(f->name, ".cow/notes") != 0){
		respond(r, Eperm);
		return;
	}
	if(nochange(&r->d)){
		respond(r, nil);
		return;
	}
	old = estrdup9p(f->name);
	lock1(old, c);
	e = wstat1(r, f, old, c);
	if(e != nil && *e == 0)
		rerrstr(err, sizeof err);
	unlock1(old);
	free(old);
	respond(r, e != nil && *e == 0 ? err : e);
}

static char*
wstat1(Req *r, F *f, char *old, Channel *c)
{
	Dir *d, nd;
	char *e, *p, *nn, *par;
	int w, w2, dir;
	uvlong q;
	Msg *m;

	if((d = look(old, &w, &q, c)) == nil)
		return Enoexist;
	dir = (d->mode&DMDIR) != 0;
	free(d);
	nn = nil;
	if(r->d.name[0] && strcmp(r->d.name, last(f->name)) != 0){
		if(iscow(old))
			return Ecow;
		if(dir && inbase(old, c))
			return "a directory of the base cannot be renamed";
		par = parent(f->name);
		nn = jn(par, r->d.name);
		free(par);
		if((d = look(nn, &w2, &q, c)) != nil){
			free(d);
			free(nn);
			return "file already exists";
		}
	}
	if(w == 'b' && (e = copyup(old, c)) != nil){
		free(nn);
		return e;
	}
	nd = r->d;
	p = spath(old);
	if(dirwstat(p, &nd) < 0){
		free(p);
		free(nn);
		return "";
	}
	free(p);
	if(nn != nil){
		m = msg(Mmove, old);
		m->to = estrdup9p(nn);
		m->v = dir;
		tell(m);
		if(dir){
			p = spath(nn);
			addtree(p, nn);
			free(p);
		}
		if(inbase(old, c))
			note(old, 1, 0, c);
		if(inbase(nn, c)){
			m = msg(Mungone, nn);
			m->v = dir;
			tell(m);
		}
		free(f->name);
		f->name = nn;
	}
	USED(r);
	return nil;
}

static void
fsclunk(F *f)
{
	if(f->fd >= 0)
		close(f->fd);
	freelist(f);
	free(f->name);
	free(f);
}

static void
workproc(void*)
{
	Msg *m;
	Req *r;
	Channel *c, *w;

	c = chancreate(sizeof(void*), 1);
	w = chancreate(sizeof(void*), 1);
	for(;;){
		m = msg(Midle, nil);
		m->reply = w;
		sendp(ownc, m);
		if((m = recvp(w)) == nil)
			continue;
		if(m->type == Mclunk){
			fsclunk(m->f);
			free(m);
			continue;
		}
		r = m->r;
		free(m);
		switch(r->ifcall.type){
		case Tattach:
			fsattach(r, c);
			break;
		case Twalk:
			fswalk(r, c);
			break;
		case Topen:
			fsopen(r, c);
			break;
		case Tcreate:
			fscreate(r, c);
			break;
		case Tread:
			fsread(r);
			break;
		case Twrite:
			fswrite(r, c);
			break;
		case Tremove:
			fsremove(r, c);
			break;
		case Tstat:
			fsstat(r, c);
			break;
		case Twstat:
			fswstat(r, c);
			break;
		default:
			respond(r, "bad request");
		}
	}
}

static void
serve(Req *r)
{
	Msg *m;

	m = msg(Mreq, nil);
	m->r = r;
	sendp(ownc, m);
}

static void
fsflush(Req *r)
{
	respond(r, nil);
}

static void
destroyfid(Fid *fid)
{
	Msg *m;

	if(fid->aux == nil)
		return;
	m = msg(Mclunk, nil);
	m->f = fid->aux;
	fid->aux = nil;
	sendp(ownc, m);
}

static void
fsend(Srv*)
{
	sendp(ownc, nil);
}

Srv fs = {
	.attach=	serve,
	.walk=	serve,
	.open=	serve,
	.create=	serve,
	.read=	serve,
	.write=	serve,
	.remove=	serve,
	.stat=	serve,
	.wstat=	serve,
	.flush=	fsflush,
	.destroyfid=	destroyfid,
	.end=	fsend,
};

static char*
absname(char *s)
{
	char buf[1024];

	if(s[0] == '/' || s[0] == '#')
		return s;
	if(getwd(buf, sizeof buf) == nil)
		sysfatal("getwd: %r");
	return smprint("%s/%s", buf, s);
}

void
usage(void)
{
	fprint(2, "usage: cowfs [-Di] [-s srvname] [-m mtpt] base shadow\n");
	threadexitsall("usage");
}

void
threadmain(int argc, char **argv)
{
	char *srvname, *mtpt, *p;
	Dir *d;
	int k;

	srvname = nil;
	mtpt = nil;
	ARGBEGIN{
	case 's':
		srvname = EARGF(usage());
		break;
	case 'm':
		mtpt = EARGF(usage());
		break;
	case 'i':
		fixed = 1;
		break;
	case 'D':
		chatty9p++;
		break;
	default:
		usage();
	}ARGEND
	if(argc != 2)
		usage();
	quotefmtinstall();
	base = cleanname(absname(argv[0]));
	shadow = cleanname(absname(argv[1]));
	if((d = dirstat(base)) == nil || !(d->mode&DMDIR))
		sysfatal("%s: not a directory", base);
	owner = estrdup9p(d->uid);
	free(d);
	if(access(shadow, AEXIST) < 0)
		close(create(shadow, OREAD, DMDIR|0777));
	p = smprint("%s/.cow", shadow);
	if(access(p, AEXIST) < 0)
		close(create(p, OREAD, DMDIR|0777));
	free(p);
	recs = readrecs(shadow);
	markshadow(shadow, "");
	if(srvname == nil && mtpt == nil)
		mtpt = base;
	ownc = chancreate(sizeof(Msg*), 0);
	savec = chancreate(sizeof(char*), 64);
	for(k = 0; k < Nwork; k++)
		procrfork(workproc, nil, Stack, RFNAMEG);
	procrfork(saveproc, nil, Stack, RFNAMEG);
	proccreate(ownerproc, nil, Stack);
	threadpostmountsrv(&fs, srvname, mtpt, MREPL|MCREATE);
	threadexits(nil);
}
