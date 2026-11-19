#include <u.h>
#include <libc.h>
#include <prop.h>
#include "impl.h"

Src*
_propnew(int mode)
{
	Src *s;

	s = emalloc(sizeof *s);
	s->mode = mode;
	return s;
}

void
srcfree(Src *s)
{
	if(s == nil)
		return;
	if(s->nnotes)
		free(fmtstrflush(&s->notes));
	free(s->v);
	free(s->n);
	free(s->sp);
	free(s->open);
	free(s->err);
	free(s->ob);
	free(s);
}

Src*
srcbytes(uchar *b, long n)
{
	Src *s;

	s = _propnew(Bytes);
	s->b = b;
	s->nb = n;
	return s;
}

void
_propreset(Src *s)
{
	s->nv = 0;
	s->nsp = 0;
	s->nopen = 0;
	s->i = 0;
	s->ib = 0;
	free(s->err);
	s->err = nil;
	if(s->nnotes)
		free(fmtstrflush(&s->notes));
	fmtstrinit(&s->notes);
	s->nnotes = 1;
}

char*
_propnotes(Src *s)
{
	char *t;

	if(!s->nnotes)
		return nil;
	s->nnotes = 0;
	t = fmtstrflush(&s->notes);
	if(t != nil && *t == 0){
		free(t);
		t = nil;
	}
	return t;
}

uvlong
_proprnd(Src *s)
{
	uvlong z;

	z = s->x += 0x9E3779B97F4A7C15ULL;
	z = (z ^ z>>30) * 0xBF58476D1CE4E5B9ULL;
	z = (z ^ z>>27) * 0x94D049BB133111EBULL;
	return z ^ z>>31;
}

char*
_propdup(char *t)
{
	char *u;

	if(t == nil)
		return nil;
	u = emalloc(strlen(t)+1);
	strcpy(u, t);
	return u;
}

char*
_propintern(char *t, int n)
{
	static char **tab;
	static int ntab;
	char *u;
	int i;

	for(i = 0; i < ntab; i++)
		if(strncmp(tab[i], t, n) == 0 && tab[i][n] == 0)
			return tab[i];
	if(ntab % 32 == 0)
		tab = erealloc(tab, (ntab+32)*sizeof tab[0]);
	u = emalloc(n+1);
	memmove(u, t, n);
	tab[ntab++] = u;
	return u;
}

void
_propflush(Src *s)
{
	if(s->nob > 0)
		write(s->ofd, s->ob, s->nob);
	s->nob = 0;
}

void
_propput(Src *s, int op, uchar *b, int n)
{
	if(s->nob+n+1 > s->mob){
		s->mob = s->nob+n+1 + 8192;
		s->ob = erealloc(s->ob, s->mob);
	}
	s->ob[s->nob++] = op;
	memmove(s->ob+s->nob, b, n);
	s->nob += n;
	if(s->oall || s->nob >= 8192)
		_propflush(s);
}

static int
putnum(uchar *b, uvlong v, int n)
{
	int i;

	for(i = 0; i < n; i++)
		b[i] = v >> 8*i;
	return n;
}

static void
puttext(Src *s, int op, int pre, char *t)
{
	uchar *b;
	int n, k;

	n = t == nil ? 0 : strlen(t);
	if(n > 0xFFFE)
		n = 0xFFFE;
	b = emalloc(n+3);
	k = 0;
	if(pre >= 0)
		b[k++] = pre;
	k += putnum(b+k, t == nil ? 0xFFFF : n, 2);
	memmove(b+k, t, n);
	_propput(s, op, b, k+n);
	free(b);
}

void
_proprec(Src *s, uvlong v, uvlong n)
{
	uchar b[16];

	if(s->nv == s->mv){
		s->mv = s->mv*2 + 64;
		s->v = erealloc(s->v, s->mv*sizeof s->v[0]);
		s->n = erealloc(s->n, s->mv*sizeof s->n[0]);
	}
	s->v[s->nv] = v;
	s->n[s->nv] = n;
	s->nv++;
	if(s->child){
		putnum(b, v, 8);
		putnum(b+8, n, 8);
		_propput(s, 'c', b, 16);
	}
}

uvlong
_propnext(Src *s, uvlong n, uvlong r)
{
	uvlong v;
	int k;

	if(s->nv >= Maxchoice)
		discard(s);
	v = 0;
	switch(s->mode){
	case Rand:
		v = r;
		break;
	case Replay:
		if(s->i < s->nin)
			v = s->in[s->i++];
		if(n != 0 && v >= n)
			v = 0;
		break;
	case Bytes:
		for(k = 0; k < 8 && (n == 0 || (n-1)>>(8*k) != 0); k++){
			v <<= 8;
			if(s->ib < s->nb)
				v |= s->b[s->ib++];
		}
		if(n != 0)
			v %= n;
		break;
	}
	_proprec(s, v, n);
	return v;
}

uvlong
choose(Src *s, uvlong n)
{
	uvlong x, r;
	int w;

	r = 0;
	if(s->mode == Rand && n != 1){
		x = _proprnd(s);
		switch(x & 7){
		case 0:
			r = 0;
			break;
		case 1:
			r = n - 1;
			break;
		case 2:
		case 3:
			w = (x>>3) % 64 + 1;
			r = _proprnd(s);
			if(w < 64)
				r &= (1ULL<<w) - 1;
			break;
		default:
			r = _proprnd(s);
			break;
		}
		if(n != 0)
			r %= n;
	}
	return _propnext(s, n, r);
}

int
coin(Src *s, int pct)
{
	uvlong r;

	r = 0;
	if(s->mode == Rand)
		r = _proprnd(s) % 100 < pct;
	return _propnext(s, 2, r);
}

int
pick(Src *s, int *w, int nw)
{
	uvlong r, t;
	int i;

	if(nw <= 0)
		sysfatal("prop: pick of nothing");
	r = 0;
	if(s->mode == Rand){
		if(w == nil)
			r = _proprnd(s) % nw;
		else{
			t = 0;
			for(i = 0; i < nw; i++)
				t += w[i];
			if(t > 0){
				t = _proprnd(s) % t;
				for(r = 0; r < nw-1 && t >= w[r]; r++)
					t -= w[r];
			}
		}
	}
	return _propnext(s, nw, r);
}

void
_propopen(Src *s, char *tag, int back)
{
	Span *p;

	if(s->nsp == s->msp){
		s->msp = s->msp*2 + 16;
		s->sp = erealloc(s->sp, s->msp*sizeof s->sp[0]);
	}
	if(s->nopen == s->mopen){
		s->mopen = s->mopen*2 + 16;
		s->open = erealloc(s->open, s->mopen*sizeof s->open[0]);
	}
	p = &s->sp[s->nsp];
	p->a = s->nv - back;
	p->e = -1;
	p->d = s->nopen;
	p->tag = tag;
	s->open[s->nopen++] = s->nsp++;
	if(s->child)
		puttext(s, 's', back, tag);
}

void
span(Src *s, char *tag)
{
	_propopen(s, tag != nil ? _propintern(tag, strlen(tag)) : nil, 0);
}

void
endspan(Src *s)
{
	if(s->nopen == 0)
		return;
	s->sp[s->open[--s->nopen]].e = s->nv;
	if(s->child)
		_propput(s, 'e', nil, 0);
}

int
more(Src *s)
{
	static char *tag;
	uvlong r;

	r = 0;
	if(s->mode == Rand && s->nv < Maxchoice/4)
		r = _proprnd(s) % (s->size+1) < s->size;
	if(_propnext(s, 2, r) == 0)
		return 0;
	if(tag == nil)
		tag = _propintern("more", 4);
	_propopen(s, tag, 1);
	return 1;
}

void
discard(Src *s)
{
	if(!s->jok)
		sysfatal("prop: discard outside a run");
	longjmp(s->j, 1);
}

void
_propnote(Src *s, char *t)
{
	if(!s->nnotes)
		return;
	fmtprint(&s->notes, "%s\n", t);
	if(s->child)
		puttext(s, 'n', -1, t);
}

void
note(Src *s, char *fmt, ...)
{
	va_list arg;
	char *t;

	if(!s->nnotes)
		return;
	va_start(arg, fmt);
	if(s->child){
		t = vsmprint(fmt, arg);
		_propnote(s, t);
		free(t);
	}else{
		fmtvprint(&s->notes, fmt, arg);
		fmtrune(&s->notes, '\n');
	}
	va_end(arg);
}

int
_proprun(Src *s, Prop *p, void *a, char **err)
{
	char *e;

	_propreset(s);
	if(setjmp(s->j)){
		s->jok = 0;
		return Discard;
	}
	s->jok = 1;
	e = p(s, a);
	s->jok = 0;
	while(s->nopen > 0)
		endspan(s);
	if(e == nil)
		return Pass;
	if(err != nil)
		*err = e;
	return Fail;
}

char*
runprop(Src *s, Prop *p, void *a)
{
	char *e;

	e = nil;
	if(_proprun(s, p, a, &e) != Fail)
		return nil;
	return e;
}
