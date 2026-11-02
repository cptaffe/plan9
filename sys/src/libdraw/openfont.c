#include <u.h>
#include <libc.h>
#include <draw.h>

static Font*
openfont1(Display *d, char *name)
{
	Font *fnt;
	int fd, i, n;
	char *buf;
	Dir *dir;

	fd = open(name, OREAD);
	if(fd < 0)
		return 0;

	dir = dirfstat(fd);
	if(dir == nil){
    Err0:
		close(fd);
		return 0;
	}
	n = dir->length;
	free(dir);
	buf = malloc(n+1);
	if(buf == 0)
		goto Err0;
	buf[n] = 0;
	i = read(fd, buf, n);
	close(fd);
	if(i != n){
		free(buf);
		return 0;
	}
	fnt = buildfont(d, buf, name);
	free(buf);
	return fnt;
}

/* swapfont makes targ, *oldp, the font that *newp is, in place: a program keeps its Font*. */
void
swapfont(Font *targ, Font **oldp, Font **newp)
{
	Font f, *old, *new;
	char *ospec;
	int oon;
	Font *onext, *oprev, *olo, *ohi, *nnext, *nprev, *nlo, *nhi;
	char *nspec;
	int non;

	if(targ != *oldp)
		sysfatal("bad swapfont %p %p %p", targ, *oldp, *newp);
	old = *oldp;
	new = *newp;

	/* all but what says which is which */
	ospec = old->namespec; oon = old->ondisplaylist; onext = old->next; oprev = old->prev;
	olo = old->lodpi; ohi = old->hidpi;
	nspec = new->namespec; non = new->ondisplaylist; nnext = new->next; nprev = new->prev;
	nlo = new->lodpi; nhi = new->hidpi;
	f = *old;
	*old = *new;
	*new = f;
	old->namespec = ospec; old->ondisplaylist = oon; old->next = onext; old->prev = oprev;
	old->lodpi = olo; old->hidpi = ohi;
	new->namespec = nspec; new->ondisplaylist = non; new->next = nnext; new->prev = nprev;
	new->lodpi = nlo; new->hidpi = nhi;

	*oldp = new;
	*newp = old;
}

/* hidpiname is the font at twice the dpi: y of a name x,y; /mnt/font/Name/2*Size/font; else none */
static char*
hidpiname(Font *f)
{
	char *p, *q;
	int size;

	p = strchr(f->namespec, ',');
	if(p != nil)
		return strdup(p+1);
	if(strncmp(f->name, "/mnt/font/", 10) == 0){
		p = strchr(f->name+10, '/');
		if(p == nil || *++p < '0' || *p > '9')
			return nil;
		q = p;
		size = 0;
		while('0' <= *q && *q <= '9')
			size = size*10 + *q++ - '0';
		return smprint("%.*s%d%s", utfnlen(f->name, p-f->name), f->name, size*2, q);
	}
	return nil;
}

void
loadhidpi(Font *f)
{
	char *name;
	Font *fnew;

	if(f->hidpi == f)
		return;
	if(f->hidpi != nil){
		swapfont(f, &f->lodpi, &f->hidpi);
		return;
	}
	name = hidpiname(f);
	if(name == nil)
		return;
	fnew = openfont1(f->display, name);
	free(name);
	if(fnew == nil)
		return;
	f->hidpi = fnew;
	swapfont(f, &f->lodpi, &f->hidpi);
}

Font*
openfont(Display *d, char *name)
{
	Font *f;
	char *p, *namespec;

	/* a name x,y is x at low dpi and y at high */
	name = strdup(name);
	namespec = strdup(name);
	if((p = strchr(name, ',')) != nil)
		*p = '\0';
	f = openfont1(d, name);
	free(name);
	if(f == nil){
		free(namespec);
		return nil;
	}
	f->lodpi = f;
	free(f->namespec);
	f->namespec = namespec;
	if(d != nil){
		f->ondisplaylist = 1;
		f->prev = d->lastfont;
		f->next = nil;
		if(f->prev)
			f->prev->next = f;
		else
			d->firstfont = f;
		d->lastfont = f;
		if(d->dpi >= DefaultDPI*3/2)
			loadhidpi(f);
	}
	return f;
}
