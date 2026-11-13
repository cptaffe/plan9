#include <u.h>
#include <libc.h>
#include <draw.h>
#include <ctype.h>

static Themecol *cols;
static int ncols;
static int loaded;

static int
hexcol(char *s, ulong *v)
{
	char *e;
	ulong x;

	if(*s == '#')
		s++;
	if(strlen(s) != 6)
		return 0;
	x = strtoul(s, &e, 16);
	if(*e != 0)
		return 0;
	*v = (x<<8) | 0xFF;
	return 1;
}

int
themeline(char *line, Themecol *c)
{
	char *f[16];
	int i, n, c0;
	ulong v;

	memset(c, 0, sizeof *c);
	n = tokenize(line, f, nelem(f));
	if(n == 0)
		return 0;
	c0 = f[0][0];
	if(c0 == '#')
		return 0;
	if(!isalpha(c0))
		return -1;
	c->name = f[0];
	for(i = 1; i < n; i++){
		if(strncmp(f[i], "fg=", 3) == 0 && hexcol(f[i]+3, &v))
			c->fg = v;
		else if(strncmp(f[i], "bg=", 3) == 0 && hexcol(f[i]+3, &v))
			c->bg = v;
		else if(hexcol(f[i], &v))
			c->col = v;
	}
	return 1;
}

static char*
slurp(char *file)
{
	char *b;
	int fd, n, m;

	if((fd = open(file, OREAD)) < 0)
		return nil;
	m = 0;
	b = nil;
	for(;;){
		b = realloc(b, m+8192+1);
		if(b == nil)
			break;
		if((n = read(fd, b+m, 8192)) <= 0)
			break;
		m += n;
	}
	close(fd);
	if(b != nil)
		b[m] = 0;
	return b;
}

static void
load(void)
{
	char *b, *p, *nl, *home, *file;
	Themecol c;
	int i;

	loaded = 1;
	b = nil;
	if((home = getenv("home")) != nil){
		file = smprint("%s/lib/theme", home);
		free(home);
		if(file != nil)
			b = slurp(file);
		free(file);
	}
	if(b == nil)
		b = slurp("/lib/theme");
	if(b == nil)
		return;
	for(p = b; *p != 0; p = nl){
		if((nl = strchr(p, '\n')) != nil)
			*nl++ = 0;
		else
			nl = p+strlen(p);
		if(themeline(p, &c) <= 0)
			continue;
		for(i = 0; i < ncols; i++)
			if(strcmp(cols[i].name, c.name) == 0)
				break;
		if(i == ncols){
			cols = realloc(cols, (ncols+1)*sizeof cols[0]);
			if(cols == nil){
				ncols = 0;
				return;
			}
			ncols++;
		}
		cols[i] = c;
	}
}

Themecol*
themelook(char *name)
{
	int i;

	if(!loaded)
		load();
	for(i = 0; i < ncols; i++)
		if(strcmp(cols[i].name, name) == 0)
			return &cols[i];
	return nil;
}

ulong
themecolor(char *name, ulong def)
{
	Themecol *c;

	if((c = themelook(name)) == nil)
		return def;
	if(c->col)
		return c->col;
	if(c->bg)
		return c->bg;
	if(c->fg)
		return c->fg;
	return def;
}

Image*
themeimage(Display *d, char *name)
{
	ulong v;

	if((v = themecolor(name, 0)) == 0)
		return nil;
	return allocimage(d, Rect(0,0,1,1), RGB24, 1, v);
}
