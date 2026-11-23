#include <u.h>
#include <libc.h>
#include <draw.h>

int
parsefontscale(char *name, char **base)
{
	char *p;
	int scale;

	p = name;
	scale = 0;
	while('0' <= *p && *p <= '9'){
		scale = scale*10 + *p - '0';
		p++;
	}
	if(*p == '*' && scale > 0)
		*base = p+1;
	else{
		*base = name;
		scale = 1;
	}
	return scale;
}

static char*
readfile(char *name)
{
	enum { HUNK = 8*1024, };
	int f, n, r;
	char *s, *p;

	n = 0;
	r = -1;
	if((s = malloc(HUNK)) != nil){
		if((f = open(name, OREAD|OCEXEC)) >= 0){
			while((r = read(f, s+n, HUNK)) > 0){
				n += r;
				r = -1;
				if((p = realloc(s, n+HUNK)) == nil)
					break;
				s = p;
			}
			close(f);
		}
	}
	if(r < 0 || (p = realloc(s, n+1)) == nil){
		free(s);
		return nil;
	}
	p[n] = 0;
	return p;
}

static Font*
openfont1(Display *d, char *name)
{
	Font *fnt;
	Subfont *df;
	char *buf, *fname;
	int scale;

	scale = parsefontscale(name, &fname);
	if(strcmp(fname, "*default*") == 0){
		if(d == nil || (df = d->defaultsubfont) == nil){
			werrstr("openfont: no default subfont");
			return nil;
		}
		buf = smprint("%d %d\n0 %d\t%s\n", df->height, df->ascent, df->n-1, fname);
	}else
		buf = readfile(fname);
	if(buf == nil){
		werrstr("openfont: %r");
		return nil;
	}
	fnt = buildfont(d, buf, name);
	free(buf);
	if(fnt != nil && scale != 1){
		fnt->scale = scale;
		fnt->height *= scale;
		fnt->ascent *= scale;
	}
	return fnt;
}

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

static int
fontfssize(char *name, char **tail)
{
	char *p, *q;
	int i, cp, d;

	if(strncmp(name, "/mnt/font/", 10) != 0)
		return -1;
	p = name+10;
	for(i = 0; i < 3; i++){
		if((q = strchr(p, '/')) == nil || q == p)
			return -1;
		p = q+1;
	}
	*tail = p;
	cp = 0;
	while('0' <= *p && *p <= '9')
		cp = cp*10 + *p++ - '0';
	cp *= 100;
	if(*p == '.')
		for(p++, d = 10; d > 0 && '0' <= *p && *p <= '9'; d /= 10)
			cp += d*(*p++ - '0');
	if(cp <= 0 || strcmp(p, "a/font") != 0)
		return -1;
	return cp;
}

static char*
fontfsname(char *name, int dpi)
{
	char *t;
	int cp, px;

	cp = fontfssize(name, &t);
	if(cp < 0 || dpi <= 0)
		return nil;
	px = (cp*dpi + 3600)/7200;
	if(px < 1)
		px = 1;
	cp = px*75;
	if(cp%100 == 0)
		return smprint("%.*s%da/font", utfnlen(name, t-name), name, cp/100);
	if(cp%10 == 0)
		return smprint("%.*s%d.%da/font", utfnlen(name, t-name), name, cp/100, cp%100/10);
	return smprint("%.*s%d.%02da/font", utfnlen(name, t-name), name, cp/100, cp%100);
}

static char*
hidpiname(Font *f)
{
	char *p, *base, *s;
	int scale, dpi;

	dpi = f->display != nil ? f->display->dpi : 0;
	p = strchr(f->namespec, ',');
	if(p == nil && (s = fontfsname(f->namespec, dpi)) != nil){
		if(strcmp(s, f->namespec) != 0)
			return s;
		free(s);
		return nil;
	}
	if(dpi < DefaultDPI*3/2)
		return nil;
	if(p != nil)
		return strdup(p+1);
	scale = parsefontscale(f->namespec, &base);
	return smprint("%d*%s", scale*2, base);
}

void
loadhidpi(Font *f)
{
	char *name;
	Font *fnew;

	name = hidpiname(f);
	if(f->hidpi == f){
		if(name != nil && strcmp(name, f->name) == 0){
			free(name);
			return;
		}
		swapfont(f, &f->hidpi, &f->lodpi);
	}
	if(name == nil)
		return;
	if(f->hidpi != nil){
		if(strcmp(name, f->hidpi->name) == 0){
			free(name);
			swapfont(f, &f->lodpi, &f->hidpi);
			return;
		}
		freefont(f->hidpi);
		f->hidpi = nil;
	}
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
		loadhidpi(f);
	}
	return f;
}
