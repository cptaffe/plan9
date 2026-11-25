#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <cursor.h>
#include <mouse.h>
#include <keyboard.h>
#include <frame.h>
#include <fcall.h>
#include <plumb.h>
#include <libsec.h>
#include "dat.h"
#include "fns.h"


typedef struct Colorent Colorent;
struct Colorent
{
	ulong	rgba;
	Image	*img;
};

static Colorent *imgcache;
static int nimgcache, mimgcache;

static Image*
colorimage(ulong rgba)
{
	int i;
	Image *img;

	for(i = 0; i < nimgcache; i++)
		if(imgcache[i].rgba == rgba)
			return imgcache[i].img;
	img = allocimage(display, Rect(0,0,1,1), screen->chan, 1, rgba);
	if(img == nil)
		return nil;
	if(nimgcache >= mimgcache){
		mimgcache = mimgcache ? mimgcache*2 : 32;
		imgcache = erealloc(imgcache, mimgcache*sizeof(Colorent));
	}
	imgcache[nimgcache].rgba = rgba;
	imgcache[nimgcache].img = img;
	nimgcache++;
	return img;
}

static void
freeentry(WinStyleEntry *e)
{
	free(e->name);
	e->name = nil;
}

void
winclearstyle(Window *w)
{
	int i;

	for(i = 0; i < w->nwpalette; i++)
		freeentry(&w->wpalette[i]);
	free(w->wpalette);
	w->wpalette = nil;
	w->nwpalette = 0;
	free(w->wruns);
	w->wruns = nil;
	w->nwruns = 0;
}

static int
findpalette(WinStyleEntry *pal, int npal, char *name)
{
	int i;

	for(i = 0; i < npal; i++)
		if(pal[i].name != nil && strcmp(pal[i].name, name) == 0)
			return i;
	return -1;
}

static void
resolvecolors(Window *w, int idx, Frame *f, Image **back, Image **text)
{
	WinStyleEntry *e, *base;

	e = &w->wpalette[idx];
	base = &w->wpalette[0];
	if(e->bg != Nocolor)
		*back = colorimage(e->bg);
	else if(idx > 0 && base->bg != Nocolor)
		*back = colorimage(base->bg);
	else
		*back = f->cols[BACK];
	if(*back == nil)
		*back = f->cols[BACK];

	if(e->fg != Nocolor)
		*text = colorimage(e->fg);
	else if(idx > 0 && base->fg != Nocolor)
		*text = colorimage(base->fg);
	else
		*text = f->cols[TEXT];
	if(*text == nil)
		*text = f->cols[TEXT];
}

void
winframesync(Window *w)
{
	Frame *f;
	Text *t;
	Image **sc, *back, *text;
	ulong *frstyles, org, frnchars, *composed, rs, re, from, to, cur, len;
	int nfrstyles, i;
	ulong k;
	WinStyleRun *r;

	t = &w->body;
	f = &t->fr;
	org = (ulong)t->org;
	frnchars = (ulong)f->nchars;

	if(w->nwpalette == 0){
		frsetstyles(f, 0, nil, 0, nil);
		frredraw(f);
		flushimage(display, 1);
		return;
	}

	sc = emalloc(w->nwpalette * NCOL * sizeof(Image*));
	for(i = 0; i < w->nwpalette; i++){
		resolvecolors(w, i, f, &back, &text);
		sc[i*NCOL + BACK] = back;
		sc[i*NCOL + HIGH] = f->cols[HIGH];
		sc[i*NCOL + BORD] = f->cols[BORD];
		sc[i*NCOL + TEXT] = text;
		sc[i*NCOL + HTEXT] = text;
	}

	if(frnchars == 0 || w->nwruns == 0){
		frsetstyles(f, w->nwpalette, sc, 0, nil);
		free(sc);
		frredraw(f);
		flushimage(display, 1);
		return;
	}

	composed = emalloc(frnchars * sizeof(ulong));
	memset(composed, 0, frnchars * sizeof(ulong));
	for(i = 0; i < w->nwruns; i++){
		r = &w->wruns[i];
		rs = r->start;
		re = r->start + r->len;
		from = rs < org ? org : rs;
		to = re > org+frnchars ? org+frnchars : re;
		if(from < to)
			for(k = from-org; k < to-org; k++)
				composed[k] = (ulong)r->paletteidx;
		if(rs > org+frnchars)
			break;
	}

	frstyles = emalloc(2*(frnchars+1)*sizeof(ulong));
	nfrstyles = 0;
	cur = composed[0];
	len = 1;
	for(k = 1; k < frnchars; k++){
		if(composed[k] == cur){
			len++;
			continue;
		}
		frstyles[nfrstyles*2] = cur;
		frstyles[nfrstyles*2+1] = len;
		nfrstyles++;
		cur = composed[k];
		len = 1;
	}
	frstyles[nfrstyles*2] = cur;
	frstyles[nfrstyles*2+1] = len;
	nfrstyles++;
	free(composed);

	frsetstyles(f, w->nwpalette, sc, nfrstyles, frstyles);
	free(sc);
	free(frstyles);
	frredraw(f);
	flushimage(display, 1);
}

void
winstyleinsert(Window *w, uint q0, uint n)
{
	int i, idx;
	WinStyleRun *r;
	ulong left, right;

	if(n == 0 || w->nwruns == 0)
		return;

	for(i = 0; i < w->nwruns; i++){
		r = &w->wruns[i];
		if((ulong)q0 < r->start){
			r->start += n;
		}else if((ulong)q0 < r->start + r->len){
			left = (ulong)q0 - r->start;
			right = r->len - left;
			idx = r->paletteidx;
			if(left == 0){
				r->start += n;
			}else if(right == 0){
			}else{
				r->len = left;
				w->wruns = erealloc(w->wruns, (w->nwruns+1)*sizeof(WinStyleRun));
				r = &w->wruns[i];
				memmove(&w->wruns[i+2], &w->wruns[i+1],
					(w->nwruns-i-1)*sizeof(WinStyleRun));
				w->wruns[i+1].start = (ulong)q0 + n;
				w->wruns[i+1].len = right;
				w->wruns[i+1].paletteidx = idx;
				w->nwruns++;
				i++;
			}
		}
	}
}

void
winstyledelete(Window *w, uint q0, uint q1)
{
	WinStyleRun *out, *r;
	int nout, i;
	ulong del, dq0, dq1, rs, re, cs;

	if(q0 >= q1 || w->nwruns == 0)
		return;

	del = (ulong)(q1 - q0);
	dq0 = (ulong)q0;
	dq1 = (ulong)q1;
	out = emalloc(w->nwruns * 2 * sizeof(WinStyleRun));
	nout = 0;

	for(i = 0; i < w->nwruns; i++){
		r = &w->wruns[i];
		rs = r->start;
		re = r->start + r->len;
		if(re <= dq0){
			out[nout++] = *r;
		}else if(rs >= dq1){
			out[nout] = *r;
			out[nout].start -= del;
			nout++;
		}else{
			if(rs < dq0 && dq0 > rs){
				out[nout].start = rs;
				out[nout].len = dq0 - rs;
				out[nout].paletteidx = r->paletteidx;
				nout++;
			}
			if(re > dq1){
				cs = dq1 < rs ? rs : dq1;
				out[nout].start = cs - del;
				out[nout].len = re - cs;
				out[nout].paletteidx = r->paletteidx;
				nout++;
			}
		}
	}
	free(w->wruns);
	w->wruns = out;
	w->nwruns = nout;
}

char*
winstyleprint(Window *w)
{
	char *out, *name;
	int nout, mout, n, i;
	char line[512];
	WinStyleEntry *e;
	WinStyleRun *r;

	mout = 256;
	out = emalloc(mout);
	nout = 0;

	for(i = 0; i < w->nwpalette; i++){
		e = &w->wpalette[i];
		if(e->name == nil)
			continue;
		n = snprint(line, sizeof line, "%s", e->name);
		if(e->fg != Nocolor)
			n += snprint(line+n, sizeof line - n,
				"\tfg=%06lux", (e->fg >> 8) & 0xFFFFFFUL);
		if(e->bg != Nocolor)
			n += snprint(line+n, sizeof line - n,
				"%cbg=%06lux", e->fg != Nocolor ? ' ' : '\t', (e->bg >> 8) & 0xFFFFFFUL);
		line[n++] = '\n';
		while(nout + n + 1 > mout){
			mout *= 2;
			out = erealloc(out, mout);
		}
		memmove(out + nout, line, n);
		nout += n;
	}

	for(i = 0; i < w->nwruns; i++){
		r = &w->wruns[i];
		name = "base";
		if(r->paletteidx >= 0 && r->paletteidx < w->nwpalette
		&& w->wpalette[r->paletteidx].name != nil)
			name = w->wpalette[r->paletteidx].name;
		n = snprint(line, sizeof line, "%lud %lud %s\n", r->start, r->len, name);
		while(nout + n + 1 > mout){
			mout *= 2;
			out = erealloc(out, mout);
		}
		memmove(out + nout, line, n);
		nout += n;
	}

	out[nout] = '\0';
	return out;
}

typedef struct Rawrun Rawrun;
struct Rawrun
{
	ulong	start;
	ulong	len;
	char	*name;
};

static int
themeentry(Window *w, char *name)
{
	Themecol *c;
	WinStyleEntry *e;

	if((c = themelook(name)) == nil)
		return 0;
	if(w->nwpalette == 0){
		w->wpalette = erealloc(w->wpalette, sizeof(WinStyleEntry));
		memset(&w->wpalette[0], 0, sizeof(WinStyleEntry));
		w->wpalette[0].name = estrdup("base");
		w->nwpalette = 1;
	}
	w->wpalette = erealloc(w->wpalette, (w->nwpalette+1)*sizeof(WinStyleEntry));
	e = &w->wpalette[w->nwpalette++];
	e->name = estrdup(name);
	e->fg = c->fg ? c->fg : c->col;
	e->bg = c->bg;
	return w->nwpalette-1;
}

void
winparsestyle(Window *w, char *buf, int nbuf, int hasaddr, Range addr)
{
	char *p, *e, *nl, *lend, *l, *ep, *ns, *rname, save;
	int i, j, mpal, npal, mraw, nraw, idx, bidx, nnew, mout, nout;
	WinStyleEntry *pal, *ne, tmp;
	Rawrun *raw;
	WinStyleRun *new, *out, *r, key;
	Themecol c;
	ulong start, len, q0, q1, rs, re, ce, cs;

	pal = nil;
	mpal = 0;
	npal = 0;
	raw = nil;
	mraw = 0;
	nraw = 0;

	p = buf;
	e = buf + nbuf;
	while(p < e){
		nl = memchr(p, '\n', e - p);
		lend = nl ? nl : e;

		while(p < lend && (*p == ' ' || *p == '\t'))
			p++;
		if(p >= lend || *p == '#'){
			p = nl ? nl+1 : e;
			continue;
		}

		if(*p < '0' || *p > '9'){
			l = emalloc(lend-p+1);
			memmove(l, p, lend-p);
			if(themeline(l, &c) > 0){
				if(npal >= mpal){
					mpal = mpal ? mpal*2 : 8;
					pal = erealloc(pal, mpal * sizeof(WinStyleEntry));
				}
				ne = &pal[npal++];
				ne->name = estrdup(c.name);
				ne->fg = c.fg ? c.fg : c.col;
				ne->bg = c.bg;
			}
			free(l);
		}else{
			start = strtoul(p, &ep, 10);
			if(ep == p){
				p = nl ? nl+1 : e;
				continue;
			}
			p = ep;
			while(p < lend && (*p == ' ' || *p == '\t'))
				p++;
			len = strtoul(p, &ep, 10);
			if(ep == p){
				p = nl ? nl+1 : e;
				continue;
			}
			p = ep;
			while(p < lend && (*p == ' ' || *p == '\t'))
				p++;

			ns = p;
			while(p < lend && *p != ' ' && *p != '\t')
				p++;
			if(p == ns){
				p = nl ? nl+1 : e;
				continue;
			}
			save = *p;
			*p = '\0';
			rname = estrdup(ns);
			*p = save;

			if(len > 0){
				if(nraw >= mraw){
					mraw = mraw ? mraw*2 : 32;
					raw = erealloc(raw, mraw * sizeof(Rawrun));
				}
				raw[nraw].start = start;
				raw[nraw].len = len;
				raw[nraw].name = rname;
				nraw++;
			}else{
				free(rname);
			}
		}
		p = nl ? nl+1 : e;
	}

	if(!hasaddr){
		winclearstyle(w);
		w->wpalette = pal;
		w->nwpalette = npal;
		pal = nil;
		npal = 0;
	}else{
		for(i = 0; i < npal; i++){
			idx = findpalette(w->wpalette, w->nwpalette, pal[i].name);
			if(idx >= 0){
				freeentry(&w->wpalette[idx]);
				w->wpalette[idx] = pal[i];
			}else{
				w->wpalette = erealloc(w->wpalette,
					(w->nwpalette+1)*sizeof(WinStyleEntry));
				w->wpalette[w->nwpalette++] = pal[i];
			}
			pal[i].name = nil;
		}
		free(pal);
		pal = nil;
	}

	if(w->nwpalette > 0){
		bidx = findpalette(w->wpalette, w->nwpalette, "base");
		if(bidx < 0){
			w->wpalette = erealloc(w->wpalette,
				(w->nwpalette+1)*sizeof(WinStyleEntry));
			memmove(&w->wpalette[1], &w->wpalette[0],
				w->nwpalette*sizeof(WinStyleEntry));
			memset(&w->wpalette[0], 0, sizeof(WinStyleEntry));
			w->wpalette[0].name = estrdup("base");
			w->nwpalette++;
			for(j = 0; j < w->nwruns; j++)
				w->wruns[j].paletteidx++;
		}else if(bidx > 0){
			tmp = w->wpalette[0];
			w->wpalette[0] = w->wpalette[bidx];
			w->wpalette[bidx] = tmp;
			for(j = 0; j < w->nwruns; j++){
				if(w->wruns[j].paletteidx == 0)
					w->wruns[j].paletteidx = bidx;
				else if(w->wruns[j].paletteidx == bidx)
					w->wruns[j].paletteidx = 0;
			}
		}
	}

	new = nil;
	nnew = 0;
	if(nraw > 0){
		new = emalloc(nraw * sizeof(WinStyleRun));
		for(i = 0; i < nraw; i++){
			start = raw[i].start;
			if(hasaddr)
				start += (ulong)addr.q0;
			idx = findpalette(w->wpalette, w->nwpalette, raw[i].name);
			if(idx < 0)
				idx = themeentry(w, raw[i].name);
			new[nnew].start = start;
			new[nnew].len = raw[i].len;
			new[nnew].paletteidx = idx;
			nnew++;
			free(raw[i].name);
			raw[i].name = nil;
		}
	}
	for(i = 0; i < nraw; i++)
		free(raw[i].name);
	free(raw);

	if(!hasaddr){
		w->wruns = new;
		w->nwruns = nnew;
	}else{
		q0 = (ulong)addr.q0;
		q1 = (ulong)addr.q1;
		mout = w->nwruns * 2 + nnew + 4;
		out = emalloc(mout * sizeof(WinStyleRun));
		nout = 0;

		for(i = 0; i < w->nwruns; i++){
			r = &w->wruns[i];
			rs = r->start;
			re = r->start + r->len;
			if(rs < q0){
				ce = re < q0 ? re : q0;
				if(ce > rs){
					out[nout].start = rs;
					out[nout].len = ce - rs;
					out[nout].paletteidx = r->paletteidx;
					nout++;
				}
			}
			if(re > q1){
				cs = rs > q1 ? rs : q1;
				if(re > cs){
					out[nout].start = cs;
					out[nout].len = re - cs;
					out[nout].paletteidx = r->paletteidx;
					nout++;
				}
			}
		}
		for(i = 0; i < nnew; i++)
			out[nout++] = new[i];
		free(new);

		for(i = 1; i < nout; i++){
			key = out[i];
			j = i - 1;
			while(j >= 0 && out[j].start > key.start){
				out[j+1] = out[j];
				j--;
			}
			out[j+1] = key;
		}

		free(w->wruns);
		w->wruns = out;
		w->nwruns = nout;
	}
}

void
xfidstyleflush(Window *w, char *buf, int nbuf, int hasaddr, Range addr)
{
	if((buf == nil || nbuf == 0) && !hasaddr){
		winclearstyle(w);
		winframesync(w);
		return;
	}
	winparsestyle(w, buf, nbuf, hasaddr, addr);
	winframesync(w);
}
