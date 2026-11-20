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
};

File	*files;
int	nfiles;
char	**made;
int	nmade;
int	nflag;
int	rflag;
int	zflag;
int	strip = -1;
int	committing;
char	held[ERRMAX];

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
	if((d = dirstat(path)) != nil && d->qid.type&QTDIR){
		free(d);
		werrstr("%s: is a directory", path);
		return nil;
	}
	if(nfiles % 32 == 0)
		files = erealloc(files, (nfiles+32)*sizeof(File));
	f = &files[nfiles];
	memset(f, 0, sizeof *f);
	f->path = estrdup(path);
	f->perm = 0664;
	if(d != nil){
		f->perm = d->mode & 0777;
		free(d);
		if((fd = open(path, OREAD)) < 0 || (f->buf = readall(fd, &f->n)) == nil){
			werrstr("%s: %r", path);
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
filetype(Diff *d)
{
	int m;

	m = d->oldmode;
	if(m == 0 || (m&0170000) == 0100000)
		m = d->newmode;
	if(m != 0 && (m&0170000) != 0100000){
		werrstr("%s: mode %o: not a regular file", d->new ? d->new : d->old, m);
		return -1;
	}
	return 0;
}

char*
patched(Diff *d, File *f, long *np)
{
	Text t;
	char *b;

	splittext(&t, f->buf, f->exists ? f->n : 0);
	b = applydiff(d, &t, np);
	free(t.l);
	if(b == nil)
		werrstr("%s: %r", f->path);
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
apply(Diff *d)
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
			werrstr("%s: already exists", d->new);
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
			werrstr("%s: cannot reverse a copy", d->old);
			return -1;
		}
		if((s = getfile(d->old)) == nil)
			return -1;
		if(!s->exists){
			werrstr("%s: no such file", d->old);
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
			werrstr("%s: already exists", d->new);
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
		werrstr("%s: no such file", p);
		return -1;
	}
	if((b = patched(d, s, &n)) == nil)
		return -1;
	if(d->flags&Pdel){
		free(b);
		if(n != 0){
			werrstr("%s: not empty after the patch removes it", s->path);
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

int
load(char *name, int fd)
{
	char *b;
	long n;
	int i;
	Patch *pt;

	if((b = readall(fd, &n)) == nil){
		werrstr("%s: %r", name);
		return -1;
	}
	pt = parsepatch(b, n, strip);
	free(b);
	if(pt == nil){
		werrstr("%s:%r", name);
		return -1;
	}
	if(rflag)
		revpatch(pt);
	if(zflag)
		for(i = 0; i < pt->nd; i++)
			pt->d[i].flags |= Pzero;
	for(i = 0; i < pt->nd; i++)
		if(apply(&pt->d[i]) < 0){
			werrstr("%s:%d: %r", name, pt->d[i].lnum);
			freepatch(pt);
			return -1;
		}
	freepatch(pt);
	return 0;
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
		if((fd = create(p, OREAD, DMDIR|0775)) < 0){
			werrstr("mkdir %s: %r", p);
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

	p = estrdup(path);
	while((s = strrchr(p, '/')) != nil && s > p){
		*s = 0;
		if(remove(p) < 0)
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
			remove(files[i].tmp);
			files[i].tmp = nil;
		}
	while(nmade > 0)
		remove(made[--nmade]);
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
	if((fd = create(f->tmp, OWRITE|OEXCL, f->perm)) < 0){
		werrstr("create %s: %r", f->tmp);
		free(f->tmp);
		f->tmp = nil;
		return -1;
	}
	n = write(fd, f->buf, f->n);
	if(n != f->n){
		werrstr("write %s: %r", f->tmp);
		close(fd);
		return -1;
	}
	if(close(fd) < 0){
		werrstr("close %s: %r", f->tmp);
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
	if(dirwstat(from, &d) < 0){
		werrstr("rename %s to %s: %r", from, d.name);
		return -1;
	}
	return 0;
}

int
setperm(char *path, ulong perm)
{
	Dir d;

	nulldir(&d);
	d.mode = perm;
	if(dirwstat(path, &d) < 0){
		werrstr("chmod %s: %r", path);
		return -1;
	}
	return 0;
}

void
undo(void)
{
	int i;
	File *f;

	for(i = nfiles-1; i >= 0; i--){
		f = &files[i];
		switch(f->state){
		case Chmod:
			setperm(f->path, f->operm);
			break;
		case Placed:
			move(f->path, f->tmp);
			if(f->bak != nil)
				move(f->bak, f->path);
			break;
		case Saved:
			move(f->bak, f->path);
			break;
		}
		f->state = 0;
	}
}

int
commit(void)
{
	int i;
	File *f;

	for(i = 0; i < nfiles; i++){
		f = &files[i];
		if(!changed(f) || !f->had || f->exists && f->tmp == nil)
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
		if(!changed(f) || !f->exists)
			continue;
		if(f->tmp != nil){
			if(move(f->tmp, f->path) < 0)
				return -1;
			f->state = Placed;
		}else{
			if(setperm(f->path, f->perm) < 0)
				return -1;
			f->state = Chmod;
		}
	}
	return 0;
}

void
finish(void)
{
	int i;
	File *f;

	for(i = 0; i < nfiles; i++){
		f = &files[i];
		if(f->bak != nil && remove(f->bak) < 0)
			fprint(2, "%s: remove %s: %r\n", argv0, f->bak);
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

	if(argc == 0){
		if(load("stdin", 0) < 0)
			sysfatal("%r");
	}
	for(i = 0; i < argc; i++){
		if((fd = open(argv[i], OREAD)) < 0)
			sysfatal("open: %r");
		if(load(argv[i], fd) < 0)
			sysfatal("%r");
		close(fd);
	}
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
		if(writetmp(f) < 0){
			fprint(2, "%s: %r\n", argv0);
			cleanup();
			exits("write");
		}
	}
	committing = 1;
	if(commit() < 0){
		fprint(2, "%s: %r\n", argv0);
		undo();
		cleanup();
		exits("commit");
	}
	finish();
	committing = 0;
	for(i = 0; i < nfiles; i++)
		if(changed(&files[i]))
			print("%s\n", files[i].path);
	exits(held[0] ? held : nil);
}
