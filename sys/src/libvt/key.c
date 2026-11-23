#include <u.h>
#include <libc.h>
#include <keyboard.h>
#include <vt.h>
#include "impl.h"

static struct {
	Rune	k;
	char	c;
	int	n;
} keys[] = {
	{Kup,	'A'},
	{Kdown,	'B'},
	{Kright,	'C'},
	{Kleft,	'D'},
	{Khome,	'H'},
	{Kend,	'F'},
	{KF|1,	'P'},
	{KF|2,	'Q'},
	{KF|3,	'R'},
	{KF|4,	'S'},
	{Kins,	'~', 2},
	{Kdel,	'~', 3},
	{Kpgup,	'~', 5},
	{Kpgdown,	'~', 6},
	{KF|5,	'~', 15},
	{KF|6,	'~', 17},
	{KF|7,	'~', 18},
	{KF|8,	'~', 19},
	{KF|9,	'~', 20},
	{KF|10,	'~', 21},
	{KF|11,	'~', 23},
	{KF|12,	'~', 24},
};

static int
small(void)
{
	werrstr("buffer too small");
	return -1;
}

int
vtkey(Vt *vt, int key, char *buf, int n)
{
	char s[32];
	Rune r;
	int i, m, len, mods;

	r = key & ~(Vctl|Valt|Vshift);
	mods = 1 + ((key&Vshift) != 0) + 2*((key&Valt) != 0) + 4*((key&Vctl) != 0);
	len = 0;
	for(i = 0; i < nelem(keys); i++)
		if(keys[i].k == r)
			break;
	if(i < nelem(keys)){
		m = mods > 1;
		if(keys[i].n != 0)
			len = m ? snprint(s, sizeof s, "\033[%d;%d~", keys[i].n, mods) : snprint(s, sizeof s, "\033[%d~", keys[i].n);
		else if(m)
			len = snprint(s, sizeof s, "\033[1;%d%c", mods, keys[i].c);
		else if((vt->mode & Mappcursor) || r >= (KF|1) && r <= (KF|4))
			len = snprint(s, sizeof s, "\033O%c", keys[i].c);
		else
			len = snprint(s, sizeof s, "\033[%c", keys[i].c);
	}else if(r >= KF && r < KF+0x100 || r >= Spec && r < Spec+0x100 || r > Runemax)
		return 0;
	else{
		if(r == Kbs)
			r = 0x7F;
		if(key & Valt)
			s[len++] = 0x1B;
		if(key & Vctl){
			if(r >= 'a' && r <= 'z' || r >= '@' && r <= '_')
				r &= 0x1F;
			else if(r == ' ' || r == '2')
				r = 0;
			else if(r == '/')
				r = 0x1F;
			else if(r == '?')
				r = 0x7F;
		}
		len += runetochar(s+len, &r);
	}
	if(len > n)
		return small();
	memmove(buf, s, len);
	return len;
}

long
vtpaste(Vt *vt, char *s, long len, char *buf, long n)
{
	long i, o;
	int b;

	b = (vt->mode & Mbracket) != 0;
	o = 0;
	if(b){
		if(n < 6)
			return small();
		memmove(buf, "\033[200~", 6);
		o = 6;
	}
	for(i = 0; i < len; i++){
		if(b && s[i] == 0x1B)
			continue;
		if(o >= n)
			return small();
		buf[o++] = s[i] == '\n' ? '\r' : s[i];
	}
	if(b){
		if(o+6 > n)
			return small();
		memmove(buf+o, "\033[201~", 6);
		o += 6;
	}
	return o;
}

int
vtmouse(Vt *vt, int but, int x, int y, int press, char *buf, int n)
{
	char s[64];
	int b, len;

	if(!(vt->mode & (Mmouse|Mmousedrag|Mmouseall)))
		return 0;
	b = but & 7;
	if(press < 0){
		if(b == 0 && !(vt->mode & Mmouseall))
			return 0;
		if(b != 0 && !(vt->mode & (Mmousedrag|Mmouseall)))
			return 0;
	}
	if(b >= 4 && press == 0)
		return 0;
	if(b == 0)
		b = 3;
	else if(b >= 4)
		b = 64 + b-4;
	else
		b = b-1;
	if(press < 0)
		b += 32;
	if(but & Vshift)
		b += 4;
	if(but & Valt)
		b += 8;
	if(but & Vctl)
		b += 16;
	if(vt->mode & Msgr)
		len = snprint(s, sizeof s, "\033[<%d;%d;%d%c", b, x+1, y+1, press == 0 ? 'm' : 'M');
	else{
		if(press == 0)
			b = (b & ~3) | 3;
		if(x > 222)
			x = 222;
		if(y > 222)
			y = 222;
		memmove(s, "\033[M", 3);
		s[3] = 32+b;
		s[4] = 33+x;
		s[5] = 33+y;
		len = 6;
	}
	if(len > n)
		return small();
	memmove(buf, s, len);
	return len;
}

int
vtfocus(Vt *vt, int in, char *buf, int n)
{
	if(!(vt->mode & Mfocus))
		return 0;
	if(n < 3)
		return small();
	memmove(buf, in ? "\033[I" : "\033[O", 3);
	return 3;
}
