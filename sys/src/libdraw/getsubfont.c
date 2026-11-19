#include <u.h>
#include <libc.h>
#include <draw.h>

static int	scalesubfont(Subfont*, int);

/*
 * Default version: treat as file name
 */

Subfont*
_getsubfont(Display *d, char *name)
{
	int fd, scale;
	char *fname;
	Subfont *f;

	scale = parsefontscale(name, &fname);
	if(strcmp(fname, "*default*") == 0){
		f = d != nil ? getdefont(d) : nil;
		if(f == nil){
			fprint(2, "getsubfont: can't read %s: %r\n", name);
			return nil;
		}
		free(f->name);
		f->name = strdup(name);
		installsubfont(name, f);
	}else{
		fd = open(fname, OREAD);
		if(fd < 0){
			fprint(2, "getsubfont: can't open %s: %r\n", fname);
			return 0;
		}
		/*
		 * unlock display so i/o happens with display released, unless
		 * user is doing his own locking, in which case this could break things.
		 * _getsubfont is called only from string.c and stringwidth.c,
		 * which are known to be safe to have this done.
		 */
		if(d && d->locking == 0)
			unlockdisplay(d);
		f = readsubfont(d, name, fd, d && d->locking==0);
		if(d && d->locking == 0)
			lockdisplay(d);
		if(f == 0)
			fprint(2, "getsubfont: can't read %s: %r\n", name);
		close(fd);
	}
	if(f != nil && scale > 1 && scalesubfont(f, scale) < 0){
		fprint(2, "getsubfont: can't scale %s: %r\n", name);
		freesubfont(f);
		return nil;
	}
	setmalloctag(f, getcallerpc(&d));
	return f;
}

static int
scalesubfont(Subfont *f, int scale)
{
	Image *i;
	Rectangle r, r2;
	int x, x2, y, j, d, v, m, ns, nd;
	uchar *src, *dst, *s, *t;
	Fontchar *fc;

	r = f->bits->r;
	d = f->bits->depth;
	r2 = Rect(r.min.x*scale, r.min.y*scale, r.max.x*scale, r.max.y*scale);
	ns = bytesperline(r, d);
	nd = bytesperline(r2, d);
	m = (1<<d) - 1;
	v = 0;
	i = nil;
	src = malloc(ns*Dy(r));
	dst = mallocz(nd*Dy(r2), 1);
	if(src == nil || dst == nil)
		goto Err;
	if(unloadimage(f->bits, r, src, ns*Dy(r)) != ns*Dy(r))
		goto Err;
	for(y=0; y<Dy(r); y++){
		s = src + y*ns;
		t = dst + y*scale*nd;
		for(x=r.min.x; x<r.max.x; x++){
			if(d < 8)
				v = s[(x*d>>3) - (r.min.x*d>>3)] >> (8-d-(x*d&7)) & m;
			for(j=0; j<scale; j++){
				x2 = x*scale + j;
				if(d < 8)
					t[(x2*d>>3) - (r2.min.x*d>>3)] |= v << (8-d-(x2*d&7));
				else
					memmove(t + (x2-r2.min.x)*(d>>3), s + (x-r.min.x)*(d>>3), d>>3);
			}
		}
		for(j=1; j<scale; j++)
			memmove(t + j*nd, t, nd);
	}
	i = allocimage(f->bits->display, r2, f->bits->chan, 0, DBlack);
	if(i == nil || loadimage(i, r2, dst, nd*Dy(r2)) < 0)
		goto Err;
	free(src);
	free(dst);
	freeimage(f->bits);
	f->bits = i;
	f->height *= scale;
	f->ascent *= scale;
	for(fc=f->info; fc<=f->info+f->n; fc++){
		fc->x *= scale;
		fc->top *= scale;
		fc->bottom *= scale;
		fc->left *= scale;
		fc->width *= scale;
	}
	return 0;

    Err:
	freeimage(i);
	free(src);
	free(dst);
	return -1;
}
