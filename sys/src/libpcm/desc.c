#include <u.h>
#include <libc.h>
#include <pcm.h>
#include "impl.h"

Pcmdesc	pcmdescdef =
{
	.rate = 44100,
	.channels = 2,
	.framesz = 4,
	.abits = 16,
	.bits = 16,
	.fmt = L's',
};

int
pcmdescfmt(Fmt *f)
{
	Pcmdesc d;

	d = va_arg(f->args, Pcmdesc);
	return fmtprint(f, "%C%dc%dr%d", d.fmt, d.bits, d.channels, d.rate);
}

static int
num(char **pp)
{
	char *p;
	vlong v;

	p = *pp;
	if(*p < '0' || *p > '9')
		return -1;
	v = 0;
	while(*p >= '0' && *p <= '9'){
		v = v*10 + *p++ - '0';
		if(v > 0x7fffffff)
			return -1;
	}
	*pp = p;
	return v;
}

int
_pcmfields(Pcmdesc *d)
{
	int lo, hi, a;

	if(d->rate < 1){
		werrstr("invalid rate: %d", d->rate);
		return -1;
	}
	if(d->channels < 1 || d->channels > Maxchan){
		werrstr("invalid number of channels: %d", d->channels);
		return -1;
	}
	switch(d->fmt){
	case L's':
	case L'S':
	case L'u':
	case L'U':
		lo = 1;
		hi = 32;
		a = d->bits;
		break;
	case L'f':
		lo = 32;
		hi = 64;
		a = 32;
		break;
	case L'a':
	case L'µ':
		lo = hi = 8;
		a = 16;
		break;
	case 0:
		werrstr("no sample format");
		return -1;
	default:
		werrstr("unknown sample format %C", d->fmt);
		return -1;
	}
	if(d->bits < lo || d->bits > hi || d->fmt == L'f' && d->bits != 32 && d->bits != 64){
		werrstr("invalid bits for %C: %d", d->fmt, d->bits);
		return -1;
	}
	if(d->abits != a){
		werrstr("invalid bits after conversion for %C%d: %d", d->fmt, d->bits, d->abits);
		return -1;
	}
	return 0;
}

int
_pcmcheck(Pcmdesc *d)
{
	if(_pcmfields(d) < 0)
		return -1;
	if(d->framesz != (d->bits+7)/8 * d->channels){
		werrstr("frame size %d for %d channels of %d bits", d->framesz, d->channels, d->bits);
		return -1;
	}
	return 0;
}

int
mkpcmdesc(char *f, Pcmdesc *d)
{
	Rune r;
	char *p;
	int *v;

	memset(d, 0, sizeof(*d));
	p = f;
	while(*p != 0){
		p += chartorune(&r, p);
		switch(r){
		case L'r':
			v = &d->rate;
			break;
		case L'c':
			v = &d->channels;
			break;
		case L'm':
			r = L'µ';
		case L's':
		case L'S':
		case L'u':
		case L'U':
		case L'f':
		case L'a':
		case L'µ':
			d->fmt = r;
			v = &d->bits;
			break;
		default:
			werrstr("bad format %s: unknown letter %C", f, r);
			return -1;
		}
		if((*v = num(&p)) < 0){
			werrstr("bad format %s: %C needs a number", f, r);
			return -1;
		}
	}
	switch(d->fmt){
	case L'a':
	case L'µ':
		d->abits = 16;
		break;
	case L'f':
		d->abits = 32;
		break;
	default:
		d->abits = d->bits;
	}
	if(_pcmfields(d) < 0){
		werrstr("bad format %s: %r", f);
		return -1;
	}
	d->framesz = (d->bits+7)/8 * d->channels;
	return 0;
}
