#include <u.h>
#include <libc.h>
#include <libsec.h>
#include "cow.h"

static char*
cowfile(char *shadow)
{
	return smprint("%s/.cow/files", shadow);
}

char*
readall(int fd, long *np)
{
	char *b;
	long n, m;

	b = nil;
	m = 0;
	for(;;){
		b = realloc(b, m+8192+1);
		if(b == nil)
			sysfatal("out of memory");
		if((n = read(fd, b+m, 8192)) <= 0)
			break;
		m += n;
	}
	if(n < 0){
		free(b);
		return nil;
	}
	b[m] = 0;
	if(np != nil)
		*np = m;
	return b;
}

Rec*
readrecs(char *shadow)
{
	char *f, *b, *p, *nl, *t[5];
	Rec *h, **l, *r;
	int fd;

	f = cowfile(shadow);
	fd = open(f, OREAD);
	free(f);
	if(fd < 0){
		f = smprint("%s/.cow/files.new", shadow);
		fd = open(f, OREAD);
		free(f);
	}
	if(fd < 0)
		return nil;
	b = readall(fd, nil);
	close(fd);
	if(b == nil)
		return nil;
	h = nil;
	l = &h;
	for(p = b; *p; p = nl){
		if((nl = strchr(p, '\n')) != nil)
			*nl++ = 0;
		else
			nl = p+strlen(p);
		if(tokenize(p, t, nelem(t)) != 4)
			continue;
		r = mallocz(sizeof *r, 1);
		r->gone = t[0][0] == 'w';
		r->opaque = t[0][0] == 'o';
		r->vers = strtoul(t[1], nil, 10);
		r->hash = strdup(t[2]);
		r->path = strdup(t[3]);
		*l = r;
		l = &r->next;
	}
	free(b);
	return h;
}

int
writerecs(char *shadow, Rec *r)
{
	char *f;
	int fd;

	f = cowfile(shadow);
	fd = create(f, OWRITE, 0666);
	free(f);
	if(fd < 0)
		return -1;
	for(; r != nil; r = r->next)
		fprint(fd, "%c %lud %s %q\n", reckind(r), r->vers, r->hash, r->path);
	close(fd);
	return 0;
}

int
reckind(Rec *r)
{
	return r->gone ? 'w' : r->opaque ? 'o' : 'c';
}

Rec*
findrec(Rec *r, char *path)
{
	for(; r != nil; r = r->next)
		if(strcmp(r->path, path) == 0)
			return r;
	return nil;
}

Rec*
addrec(Rec **h, char *path, ulong vers, char *hash)
{
	Rec *r;

	if((r = findrec(*h, path)) != nil)
		return r;
	r = mallocz(sizeof *r, 1);
	r->path = strdup(path);
	r->vers = vers;
	r->hash = strdup(hash);
	r->next = *h;
	*h = r;
	return r;
}

void
delrec(Rec **h, char *path)
{
	Rec *r;

	for(; (r = *h) != nil; h = &r->next)
		if(strcmp(r->path, path) == 0){
			*h = r->next;
			r->next = nil;
			freerecs(r);
			return;
		}
}

void
freerecs(Rec *r)
{
	Rec *o;

	for(; r != nil; r = o){
		o = r->next;
		free(r->path);
		free(r->hash);
		free(r);
	}
}

char*
hashtext(char *s, long n)
{
	uchar d[SHA1dlen];
	char *h;
	int i;

	sha1((uchar*)s, n, d, nil);
	h = malloc(2*SHA1dlen+1);
	for(i = 0; i < SHA1dlen; i++)
		sprint(h+2*i, "%.2ux", d[i]);
	return h;
}

char*
hashfile(char *file)
{
	char *b, *h;
	long n;
	int fd;

	if((fd = open(file, OREAD)) < 0)
		return nil;
	b = readall(fd, &n);
	close(fd);
	if(b == nil)
		return nil;
	h = hashtext(b, n);
	free(b);
	return h;
}
