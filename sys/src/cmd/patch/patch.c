#include <u.h>
#include <libc.h>
#include <patch.h>

typedef struct File	File;

struct File
{
	char	*path;
	char	*buf;
	long	n;
	int	exists;
	int	had;
	ulong	perm;
	ulong	operm;
	int	dirty;
	int	edit;
	char	*tmp;
	char	*bak;
	int	state;
	int	isdir;
	int	later;
	char	*olddir;
	char	*src;
	int	lnum;
};

int	pcreate(char*, int, ulong);
int	premove(char*);
int	pwstat(char*, Dir*);

File	*files;
int	nfiles;
char	**made;
int	nmade;
int	nflag;
int	rflag;
int	zflag;
int	strip = -1;
int	committing;
char	*errfile;
char	*curname;
int	curlnum;
char	held[ERRMAX];
int	left;

enum
{
	Saved = 1,
	Placed,
	Chmod,
};

void*
emalloc(ulong n)
{
	void *v;

	if((v = mallocz(n, 1)) == nil)
		sysfatal("out of memory");
	setmalloctag(v, getcallerpc(&n));
	return v;
}

void*
erealloc(void *v, ulong n)
{
	if((v = realloc(v, n)) == nil && n != 0)
		sysfatal("out of memory");
	setrealloctag(v, getcallerpc(&v));
	return v;
}

char*
estrdup(char *s)
{
	char *t;

	t = emalloc(strlen(s)+1);
	strcpy(t, s);
	return t;
}

char*
readall(int fd, long *np)
{
	char *b;
	long n, m, k;

	n = 0;
	m = 8192;
	b = emalloc(m);
	while((k = read(fd, b+n, m-n)) > 0){
		n += k;
		if(n == m){
			m *= 2;
			b = erealloc(b, m);
		}
	}
	if(k < 0){
		free(b);
		return nil;
	}
	*np = n;
	return b;
}

File*
getfile(char *path)
{
	File *f;
	Dir *d;
	int i, fd;

	for(i = 0; i < nfiles; i++)
		if(strcmp(files[i].path, path) == 0)
			return &files[i];
	if(nfiles % 32 == 0)
		files = erealloc(files, (nfiles+32)*sizeof(File));
	f = &files[nfiles];
	memset(f, 0, sizeof *f);
	f->path = estrdup(path);
	f->perm = 0664;
	d = dirstat(path);
	if(d != nil && d->qid.type&QTDIR){
		free(d);
		d = nil;
		f->isdir = 1;
	}
	if(d != nil){
		f->perm = d->mode & 0777;
		free(d);
		if((fd = open(path, OREAD)) < 0 || (f->buf = readall(fd, &f->n)) == nil){
			errfile = path;
			if(fd >= 0)
				close(fd);
			free(f->path);
			return nil;
		}
		close(fd);
		f->exists = f->had = 1;
	}
	f->operm = f->perm;
	nfiles++;
	return f;
}

int
gitperm(int mode, ulong perm)
{
	if(mode == 0)
		return perm;
	if(mode&0111)
		return perm | (perm&0444)>>2;
	return perm & ~0111;
}

int
filetype(Pdiff *d)
{
	int m;

	m = d->oldmode;
	if(m == 0 || (m&0170000) == 0100000)
		m = d->newmode;
	if(m != 0 && (m&0170000) != 0100000){
		errfile = d->new ? d->new : d->old;
		werrstr("mode %o: not a regular file", m);
		return -1;
	}
	return 0;
}

char*
patched(Pdiff *d, File *f, long *np)
{
	Ptext t;
	char *b;

	splittext(&t, f->buf, f->exists ? f->n : 0);
	b = applydiff(d, &t, np);
	free(t.l);
	if(b == nil)
		errfile = f->path;
	return b;
}

void
settext(File *f, char *b, long n)
{
	free(f->buf);
	f->buf = b;
	f->n = n;
	f->exists = 1;
	f->dirty = 1;
	f->edit = 1;
	f->src = curname;
	f->lnum = curlnum;
}

void
unset(File *f)
{
	free(f->buf);
	f->buf = nil;
	f->n = 0;
	f->exists = 0;
	f->dirty = 1;
}

int
apply(Pdiff *d)
{
	File *s, *t;
	char *b, *p;
	long n;

	if(filetype(d) < 0)
		return -1;
	if(d->flags&Pnew){
		if((t = getfile(d->new)) == nil)
			return -1;
		if(t->exists){
			errfile = d->new;
			werrstr("already exists");
			return -1;
		}
		if((b = patched(d, t, &n)) == nil)
			return -1;
		settext(t, b, n);
		t->perm = gitperm(d->newmode, 0664);
		return 0;
	}
	if(d->flags&(Pren|Pcopy)){
		if(rflag && d->flags&Pcopy){
			errfile = d->old;
			werrstr("cannot reverse a copy");
			return -1;
		}
		if((s = getfile(d->old)) == nil)
			return -1;
		if(!s->exists){
			errfile = d->old;
			werrstr("no such file");
			return -1;
		}
		if((b = patched(d, s, &n)) == nil)
			return -1;
		if((t = getfile(d->new)) == nil){
			free(b);
			return -1;
		}
		if(t->exists){
			free(b);
			errfile = d->new;
			werrstr("already exists");
			return -1;
		}
		settext(t, b, n);
		t->perm = gitperm(d->newmode, s->perm);
		if(d->flags&Pren)
			unset(s);
		return 0;
	}
	p = d->old;
	if(p == nil)
		p = d->new;
	if((s = getfile(p)) == nil)
		return -1;
	if(!s->exists && !(d->flags&Pgit) && d->new != nil && d->old != nil){
		if((s = getfile(d->new)) == nil)
			return -1;
	}
	if(!s->exists){
		errfile = p;
		werrstr(s->isdir ? "is a directory" : "no such file");
		return -1;
	}
	if((b = patched(d, s, &n)) == nil)
		return -1;
	if(d->flags&Pdel){
		free(b);
		if(n != 0){
			errfile = s->path;
			werrstr("not empty after the patch removes it");
			return -1;
		}
		unset(s);
		return 0;
	}
	if(n != s->n || memcmp(b, s->buf, n) != 0)
		settext(s, b, n);
	else
		free(b);
	if(d->newmode && gitperm(d->newmode, s->perm) != s->perm){
		s->perm = gitperm(d->newmode, s->perm);
		s->dirty = 1;
	}
	return 0;
}

void
fail(char *name, int lnum)
{
	char err[ERRMAX];

	rerrstr(err, sizeof err);
	if(errfile != nil)
		fprint(2, "%s: %s:%d: %s: %s\n", argv0, name, lnum, errfile, err);
	else if(lnum > 0)
		fprint(2, "%s: %s:%d: %s\n", argv0, name, lnum, err);
	else
		fprint(2, "%s: %s: %s\n", argv0, name, err);
	exits(err);
}

void
load(char *name, int fd)
{
	char *b;
	long n;
	int i;
	Patch *pt;

	if((b = readall(fd, &n)) == nil)
		fail(name, 0);
	pt = parsepatch(b, n, strip);
	free(b);
	if(pt == nil){
		errfile = nil;
		fprint(2, "%s: %s:%r\n", argv0, name);
		exits("parse");
	}
	if(rflag)
		revpatch(pt);
	if(zflag)
		for(i = 0; i < pt->nd; i++)
			pt->d[i].flags |= Pzero;
	for(i = 0; i < pt->nd; i++){
		errfile = nil;
		curname = name;
		curlnum = pt->d[i].lnum;
		if(apply(&pt->d[i]) < 0)
			fail(name, pt->d[i].lnum);
	}
	freepatch(pt);
}

int
changed(File *f)
{
	if(!f->dirty || !f->exists && !f->had)
		return 0;
	if(f->exists != f->had || f->perm != f->operm)
		return 1;
	return f->exists;
}

char*
base(char *p)
{
	char *s;

	if((s = strrchr(p, '/')) != nil)
		return s+1;
	return p;
}

int
oops(char *op, char *path)
{
	char err[ERRMAX];

	rerrstr(err, sizeof err);
	fprint(2, "%s: %s %s: %s\n", argv0, op, path, err);
	return -1;
}

int
mkparent(char *path)
{
	char *p, *s;
	Dir *d;
	int fd;

	p = estrdup(path);
	for(s = strchr(p+1, '/'); s != nil; s = strchr(s+1, '/')){
		*s = 0;
		if((d = dirstat(p)) != nil){
			free(d);
			*s = '/';
			continue;
		}
		if((fd = pcreate(p, OREAD, DMDIR|0775)) < 0){
			oops("mkdir", p);
			free(p);
			return -1;
		}
		close(fd);
		if(nmade % 16 == 0)
			made = erealloc(made, (nmade+16)*sizeof(char*));
		made[nmade++] = estrdup(p);
		*s = '/';
	}
	free(p);
	return 0;
}

void
rmparent(char *path)
{
	char *p, *s;
	Dir *d;
	int dir;

	p = estrdup(path);
	while((s = strrchr(p, '/')) != nil && s > p){
		*s = 0;
		if((d = dirstat(p)) == nil)
			break;
		dir = d->qid.type&QTDIR;
		free(d);
		if(!dir || premove(p) < 0)
			break;
	}
	free(p);
}

void
cleanup(void)
{
	int i;

	for(i = 0; i < nfiles; i++)
		if(files[i].tmp != nil){
			premove(files[i].tmp);
			files[i].tmp = nil;
		}
	while(nmade > 0)
		premove(made[--nmade]);
}

int
catch(void*, char *msg)
{
	if(committing && strncmp(msg, "sys:", 4) != 0){
		if(held[0] == 0)
			strecpy(held, held+sizeof held, msg);
		return 1;
	}
	if(strcmp(msg, "interrupt") == 0 || strcmp(msg, "hangup") == 0 || strcmp(msg, "alarm") == 0){
		cleanup();
		exits(msg);
	}
	return 0;
}

File*
lookup(char *path)
{
	int i;

	for(i = 0; i < nfiles; i++)
		if(strcmp(files[i].path, path) == 0)
			return &files[i];
	return nil;
}

int
under(char *path, char *dir)
{
	int n;

	n = strlen(dir);
	return strncmp(path, dir, n) == 0 && path[n] == '/';
}

int
emptied(char *dir)
{
	Dir *d;
	int fd, i, n, ok;
	char *p;
	File *f;

	if((fd = open(dir, OREAD)) < 0)
		return 0;
	n = dirreadall(fd, &d);
	close(fd);
	ok = 1;
	for(i = 0; i < n && ok; i++){
		p = smprint("%s/%s", dir, d[i].name);
		if(p == nil)
			sysfatal("smprint: %r");
		if(d[i].qid.type&QTDIR)
			ok = emptied(p);
		else
			ok = (f = lookup(p)) != nil && f->had && !f->exists;
		free(p);
	}
	free(d);
	return ok;
}

void
checkdirs(void)
{
	int i, j;
	File *f, *g;

	for(i = 0; i < nfiles; i++){
		f = &files[i];
		if(f->isdir && f->exists && !emptied(f->path)){
			fprint(2, "%s: %s: a directory the patch does not empty\n", argv0, f->path);
			exits("directory");
		}
		for(j = 0; j < nfiles; j++){
			g = &files[j];
			if(f->exists && g->exists && under(g->path, f->path)){
				fprint(2, "%s: %s:%d: %s: %s is a file\n", argv0, g->src, g->lnum, g->path, f->path);
				exits("file");
			}
		}
	}
}

int
blocked(char *path)
{
	int i;
	File *f;

	for(i = 0; i < nfiles; i++){
		f = &files[i];
		if(f->had && !f->exists && under(path, f->path))
			return 1;
	}
	return 0;
}

int
writetmp(File *f)
{
	int fd;
	long n;

	if(mkparent(f->path) < 0)
		return -1;
	f->tmp = smprint("%s.patch%d", f->path, getpid());
	if(f->tmp == nil)
		sysfatal("smprint: %r");
	if((fd = pcreate(f->tmp, OWRITE|OEXCL, f->perm)) < 0){
		oops("create", f->tmp);
		free(f->tmp);
		f->tmp = nil;
		return -1;
	}
	n = write(fd, f->buf, f->n);
	if(n != f->n){
		oops("write", f->tmp);
		close(fd);
		return -1;
	}
	if(close(fd) < 0){
		oops("close", f->tmp);
		return -1;
	}
	return 0;
}

int
move(char *from, char *to)
{
	Dir d;

	nulldir(&d);
	d.name = base(to);
	if(pwstat(from, &d) < 0)
		return oops("rename", from);
	return 0;
}

int
setperm(char *path, ulong perm)
{
	Dir d;

	nulldir(&d);
	d.mode = perm;
	if(pwstat(path, &d) < 0)
		return oops("chmod", path);
	return 0;
}

int
rmtree(char *dir)
{
	Dir *d;
	int fd, i, n, bad;
	char *p;

	if((fd = open(dir, OREAD)) < 0)
		return oops("open", dir);
	n = dirreadall(fd, &d);
	close(fd);
	bad = 0;
	for(i = 0; i < n; i++){
		p = smprint("%s/%s", dir, d[i].name);
		if(p == nil)
			sysfatal("smprint: %r");
		if(d[i].qid.type&QTDIR){
			if(rmtree(p) < 0)
				bad++;
		}else if(premove(p) < 0){
			oops("remove", p);
			bad++;
		}
		free(p);
	}
	free(d);
	if(bad)
		return -1;
	if(premove(dir) < 0)
		return oops("remove", dir);
	return 0;
}

int
undo(void)
{
	int i, bad;
	File *f;

	bad = 0;
	for(i = nfiles-1; i >= 0; i--){
		f = &files[i];
		if(f->state == Chmod && setperm(f->path, f->operm) < 0)
			bad++;
		if(f->state == Placed && move(f->path, f->tmp) < 0)
			bad++;
	}
	cleanup();
	for(i = nfiles-1; i >= 0; i--){
		f = &files[i];
		if(f->olddir != nil){
			if(move(f->olddir, f->path) < 0){
				fprint(2, "%s: %s is left in %s\n", argv0, f->path, f->olddir);
				bad++;
			}
			free(f->olddir);
			f->olddir = nil;
		}
	}
	for(i = nfiles-1; i >= 0; i--){
		f = &files[i];
		if(f->bak != nil){
			if(move(f->bak, f->path) < 0){
				fprint(2, "%s: %s is left in %s\n", argv0, f->path, f->bak);
				bad++;
			}
			free(f->bak);
			f->bak = nil;
		}
		f->state = 0;
	}
	return bad;
}

int
place(File *f)
{
	if(f->tmp != nil){
		if(move(f->tmp, f->path) < 0)
			return -1;
		f->state = Placed;
	}else{
		if(setperm(f->path, f->perm) < 0)
			return -1;
		f->state = Chmod;
	}
	return 0;
}

int
commit(void)
{
	int i;
	File *f;

	for(i = 0; i < nfiles; i++){
		f = &files[i];
		if(!changed(f) || !f->had || f->exists && f->tmp == nil && !f->later)
			continue;
		f->bak = smprint("%s.patchold%d", f->path, getpid());
		if(f->bak == nil)
			sysfatal("smprint: %r");
		if(move(f->path, f->bak) < 0){
			free(f->bak);
			f->bak = nil;
			return -1;
		}
		f->state = Saved;
	}
	for(i = 0; i < nfiles; i++){
		f = &files[i];
		if(changed(f) && f->exists && !f->later && !f->isdir && place(f) < 0)
			return -1;
	}
	for(i = 0; i < nfiles; i++){
		f = &files[i];
		if(!changed(f) || !f->later)
			continue;
		if(writetmp(f) < 0 || place(f) < 0)
			return -1;
	}
	for(i = 0; i < nfiles; i++){
		f = &files[i];
		if(!changed(f) || !f->isdir || !f->exists)
			continue;
		f->olddir = smprint("%s.patchold%d", f->path, getpid());
		if(f->olddir == nil)
			sysfatal("smprint: %r");
		if(move(f->path, f->olddir) < 0){
			free(f->olddir);
			f->olddir = nil;
			return -1;
		}
		if(place(f) < 0)
			return -1;
	}
	return 0;
}

int
inold(char *path)
{
	int i;

	for(i = 0; i < nfiles; i++)
		if(files[i].olddir != nil && under(path, files[i].path))
			return 1;
	return 0;
}

void
finish(void)
{
	int i;
	File *f;

	for(i = 0; i < nfiles; i++){
		f = &files[i];
		if(f->olddir != nil && rmtree(f->olddir) < 0){
			fprint(2, "%s: %s's old tree is left in %s\n", argv0, f->path, f->olddir);
			left++;
		}
	}
	for(i = 0; i < nfiles; i++){
		f = &files[i];
		if(f->bak != nil && !inold(f->path) && premove(f->bak) < 0){
			oops("remove", f->bak);
			fprint(2, "%s: %s's old text is left in %s\n", argv0, f->path, f->bak);
			left++;
		}
		if(changed(f) && !f->exists)
			rmparent(f->path);
		free(f->tmp);
		f->tmp = nil;
	}
	nmade = 0;
}

void
usage(void)
{
	fprint(2, "usage: %s [-nRz] [-p n] [patch ...]\n", argv0);
	exits("usage");
}

void
main(int argc, char **argv)
{
	int i, fd;
	File *f;

	ARGBEGIN{
	case 'n':
		nflag++;
		break;
	case 'R':
		rflag++;
		break;
	case 'z':
		zflag++;
		break;
	case 'p':
		strip = atoi(EARGF(usage()));
		if(strip < 0)
			usage();
		break;
	default:
		usage();
	}ARGEND

	if(argc == 0)
		load("stdin", 0);
	for(i = 0; i < argc; i++){
		if((fd = open(argv[i], OREAD)) < 0)
			sysfatal("open: %r");
		load(argv[i], fd);
		close(fd);
	}
	checkdirs();
	if(nflag){
		for(i = 0; i < nfiles; i++)
			if(changed(&files[i]))
				print("%s\n", files[i].path);
		exits(nil);
	}
	atnotify(catch, 1);
	for(i = 0; i < nfiles; i++){
		f = &files[i];
		if(!changed(f) || !f->exists || f->had && !f->edit)
			continue;
		if(blocked(f->path)){
			f->later = 1;
			continue;
		}
		if(writetmp(f) < 0){
			cleanup();
			exits("write");
		}
	}
	committing = 1;
	if(commit() < 0){
		if(undo() > 0){
			fprint(2, "%s: could not undo all of the patch; see patch(1)\n", argv0);
			exits("undo");
		}
		exits("commit");
	}
	finish();
	committing = 0;
	for(i = 0; i < nfiles; i++)
		if(changed(&files[i]))
			print("%s\n", files[i].path);
	if(held[0])
		exits(held);
	exits(left ? "left" : nil);
}
