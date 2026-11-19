#include <u.h>
#include <libc.h>
#include <md.h>
#include "impl.h"

void*
_mdalloc(P *p, ulong n)
{
	Arena *a;
	ulong m;
	void *v;

	n = (n+7) & ~7;
	a = p->a;
	if(a == nil || a->e - a->p < n){
		m = n > 65536 ? n : 65536;
		a = emalloc(sizeof *a + m);
		a->p = (uchar*)(a+1);
		a->e = a->p + m;
		a->next = p->a;
		p->a = a;
	}
	v = a->p;
	a->p += n;
	memset(v, 0, n);
	return v;
}

void
_mdfreearena(P *p)
{
	Arena *a, *n;

	for(a = p->a; a != nil; a = n){
		n = a->next;
		free(a);
	}
	p->a = nil;
}

Tn*
_mdtn(P *p, int type, u32int q0, u32int q1, u32int line)
{
	Tn *t;

	t = _mdalloc(p, sizeof *t);
	t->type = type;
	t->q0 = q0;
	t->q1 = q1;
	t->line = line;
	return t;
}

void
_mdadd(Tn *up, Tn *t)
{
	t->up = up;
	t->next = nil;
	t->prev = up->last;
	if(up->last != nil)
		up->last->next = t;
	else
		up->kid = t;
	up->last = t;
}

void
_mdunlink(Tn *t)
{
	if(t->prev != nil)
		t->prev->next = t->next;
	else if(t->up != nil)
		t->up->kid = t->next;
	if(t->next != nil)
		t->next->prev = t->prev;
	else if(t->up != nil)
		t->up->last = t->prev;
	t->up = t->next = t->prev = nil;
}

void
_mdbefore(Tn *at, Tn *t)
{
	t->up = at->up;
	t->next = at;
	t->prev = at->prev;
	if(at->prev != nil)
		at->prev->next = t;
	else if(at->up != nil)
		at->up->kid = t;
	at->prev = t;
}

int
_mdispunct(int c)
{
	return c >= 0x21 && c <= 0x2F || c >= 0x3A && c <= 0x40 || c >= 0x5B && c <= 0x60 || c >= 0x7B && c <= 0x7E;
}

int
_mdisspace(int c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

int
_mdpunctr(Rune r)
{
	int lo, hi, m;

	if(r < 128)
		return _mdispunct(r);
	lo = 0;
	hi = _mdnpunct;
	while(lo < hi){
		m = (lo+hi)/2;
		if(r < _mdpunct[m][0])
			hi = m;
		else if(r > _mdpunct[m][1])
			lo = m+1;
		else
			return 1;
	}
	return 0;
}

int
_mdspacer(Rune r)
{
	return r == 9 || r == 10 || r == 12 || r == 13 || r == 32 || r == 0xA0 || r == 0x1680 ||
		r >= 0x2000 && r <= 0x200A || r == 0x202F || r == 0x205F || r == 0x3000;
}

int
_mdutf(uchar *s, int n, int i, Rune *r)
{
	int k, c, m;
	Rune v;

	c = s[i];
	if(c < 0x80){
		*r = c == 0 ? 0xFFFD : c;
		return 1;
	}
	if(c >= 0xC2 && c <= 0xDF){
		k = 1;
		v = c & 0x1F;
		m = 0x80;
	}else if(c >= 0xE0 && c <= 0xEF){
		k = 2;
		v = c & 0x0F;
		m = 0x800;
	}else if(c >= 0xF0 && c <= 0xF4){
		k = 3;
		v = c & 0x07;
		m = 0x10000;
	}else
		return -1;
	if(i+k >= n)
		return -1;
	for(c = 1; c <= k; c++){
		if((s[i+c] & 0xC0) != 0x80)
			return -1;
		v = v<<6 | s[i+c] & 0x3F;
	}
	if(v < m || v > 0x10FFFF || v >= 0xD800 && v <= 0xDFFF)
		return -1;
	*r = v;
	return k+1;
}

int
_mdfoldr(Rune r, Rune *out)
{
	int lo, hi, m, n;

	lo = 0;
	hi = _mdnfold;
	while(lo < hi){
		m = (lo+hi)/2;
		if(r < _mdfold[m][0])
			hi = m;
		else if(r > _mdfold[m][0])
			lo = m+1;
		else{
			for(n = 0; n < 3 && _mdfold[m][n+1] != 0; n++)
				out[n] = _mdfold[m][n+1];
			return n;
		}
	}
	out[0] = r;
	return 1;
}

static int
entcmp(uchar *s, int n, char *name)
{
	int i;

	for(i = 0; i < n; i++){
		if(name[i] == 0)
			return s[i] - ';';
		if(s[i] != (uchar)name[i])
			return s[i] - (uchar)name[i];
	}
	return name[i] == 0 ? 0 : ';' - (uchar)name[i];
}

int
_mdent1(uchar *s, int n, Rune *r)
{
	int lo, hi, m, c, i;
	vlong v;

	if(n < 3 || s[0] != '&')
		return 0;
	if(s[1] == '#'){
		v = 0;
		if(n > 2 && (s[2] == 'x' || s[2] == 'X')){
			for(i = 3; i < n && i < 3+6; i++){
				c = s[i];
				if(c >= '0' && c <= '9')
					v = v*16 + c-'0';
				else if(c >= 'a' && c <= 'f')
					v = v*16 + c-'a'+10;
				else if(c >= 'A' && c <= 'F')
					v = v*16 + c-'A'+10;
				else
					break;
			}
			if(i == 3)
				return 0;
		}else{
			for(i = 2; i < n && i < 2+7; i++){
				c = s[i];
				if(c < '0' || c > '9')
					break;
				v = v*10 + c-'0';
			}
			if(i == 2)
				return 0;
		}
		if(i >= n || s[i] != ';')
			return 0;
		if(v == 0 || v > 0x10FFFF || v >= 0xD800 && v <= 0xDFFF)
			v = 0xFFFD;
		r[0] = v;
		r[1] = 0;
		return i+1;
	}
	for(i = 1; i < n && i <= 32; i++){
		c = s[i];
		if(c == ';')
			break;
		if(!(c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c >= '0' && c <= '9'))
			return 0;
	}
	if(i >= n || s[i] != ';' || i == 1)
		return 0;
	lo = 0;
	hi = _mdnent;
	while(lo < hi){
		m = (lo+hi)/2;
		c = entcmp(s+1, i, _mdent[m].name);
		if(c < 0)
			hi = m;
		else if(c > 0)
			lo = m+1;
		else{
			r[0] = _mdent[m].r[0];
			r[1] = _mdent[m].r[1];
			return i+1;
		}
	}
	return 0;
}

int
mdentity(char *s, int n, Rune *r)
{
	return _mdent1((uchar*)s, n, r);
}

char*
_mdnorm(uchar *s, int n)
{
	char *b;
	int i, k, m, w, sp, j;
	Rune r, f[3];

	b = emalloc(n*3*3+1);
	m = 0;
	sp = 0;
	for(i = 0; i < n; i += k){
		k = _mdutf(s, n, i, &r);
		if(k < 0){
			k = 1;
			r = 0xFFFD;
		}
		if(r == ' ' || r == '\t' || r == '\n' || r == '\r' || r == '\v' || r == '\f'){
			sp = 1;
			continue;
		}
		if(sp && m > 0)
			b[m++] = ' ';
		sp = 0;
		w = _mdfoldr(r, f);
		for(j = 0; j < w; j++)
			m += runetochar(b+m, &f[j]);
	}
	b[m] = 0;
	return b;
}

u32int
_mdhash(char *s)
{
	u32int h;

	h = 2166136261U;
	for(; *s; s++){
		h ^= (uchar)*s;
		h *= 16777619;
	}
	return h;
}

int
_mdisblock(int t)
{
	return t >= Mddoc && t <= Mdcell;
}
