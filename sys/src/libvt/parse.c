#include <u.h>
#include <libc.h>
#include <vt.h>
#include "impl.h"

/* DEC's special graphics, for 0x60 to 0x7E */
static Rune graphics[] = {
	L'◆', L'▒', L'␉', L'␌', L'␍', L'␊', L'°', L'±', L'␤', L'␋', L'┘', L'┐', L'┌', L'└', L'┼', L'⎺',
	L'⎻', L'─', L'⎼', L'⎽', L'├', L'┤', L'┴', L'┬', L'│', L'≤', L'≥', L'π', L'≠', L'£', L'·',
};

static void
reply(Vt *vt, char *fmt, ...)
{
	char buf[64];
	va_list arg;
	int n;

	if(vt->reply == nil)
		return;
	va_start(arg, fmt);
	n = vsnprint(buf, sizeof buf, fmt, arg);
	va_end(arg);
	vt->reply(vt, buf, n);
}

static int
arg(Vt *vt, int i, int def)
{
	Priv *p;
	int j, n;

	p = P(vt);
	for(j = n = 0; j < p->nparam; j++){
		if(p->sub[j])
			continue;
		if(n++ == i)
			return p->param[j] > 0 ? p->param[j] : def;
	}
	return def;
}

static int
nargs(Vt *vt)
{
	Priv *p;
	int j, n;

	p = P(vt);
	for(j = n = 0; j < p->nparam; j++)
		if(!p->sub[j])
			n++;
	return n;
}

static void
clampx(Vt *vt)
{
	if(vt->x < 0)
		vt->x = 0;
	if(vt->x >= vt->cols)
		vt->x = vt->cols-1;
}

/* move puts the cursor at x, y, the screen's; y is kept in the region if it was there */
static void
move(Vt *vt, int x, int y)
{
	Priv *p;
	int top, bot;

	p = P(vt);
	top = 0;
	bot = vt->rows;
	if(vt->y >= p->top && vt->y < p->bot || (vt->mode & Morigin)){
		top = p->top;
		bot = p->bot;
	}
	if(y < top)
		y = top;
	if(y >= bot)
		y = bot-1;
	vt->x = x;
	vt->y = y;
	clampx(vt);
	p->pending = 0;
}

static void
moveto(Vt *vt, int x, int y)
{
	Priv *p;

	p = P(vt);
	if(vt->mode & Morigin)
		y += p->top;
	else{
		if(y < 0)
			y = 0;
		if(y >= vt->rows)
			y = vt->rows-1;
		vt->x = x;
		vt->y = y;
		clampx(vt);
		p->pending = 0;
		return;
	}
	move(vt, x, y);
}

static void
index(Vt *vt)
{
	Priv *p;

	p = P(vt);
	p->pending = 0;
	if(vt->y == p->bot-1)
		_vtscroll(vt, p->top, p->bot, 1, 1);
	else if(vt->y < vt->rows-1)
		vt->y++;
}

static void
revindex(Vt *vt)
{
	Priv *p;

	p = P(vt);
	p->pending = 0;
	if(vt->y == p->top)
		_vtscroll(vt, p->top, p->bot, -1, 0);
	else if(vt->y > 0)
		vt->y--;
}

static void
newline(Vt *vt)
{
	index(vt);
	if(vt->mode & Mnewline)
		vt->x = 0;
}

/* unwide clears the halves of wide runes that cells x to x+n-1 of row y would split */
static void
unwide(Vt *vt, int y, int x, int n)
{
	Vtcell *c;
	int e;

	c = vt->line[y].c;
	e = x + n;
	if(x > 0 && x < vt->cols && (c[x].attr & Vcont)){
		c[x-1].r = ' ';
		c[x-1].attr &= ~Vwide;
	}
	if(e > 0 && e < vt->cols && (c[e].attr & Vcont)){
		c[e].r = ' ';
		c[e].attr &= ~Vcont;
	}
}

static void
erase(Vt *vt, int y, int x0, int x1)
{
	if(x0 < 0)
		x0 = 0;
	if(x1 > vt->cols)
		x1 = vt->cols;
	if(x0 >= x1)
		return;
	unwide(vt, y, x0, x1-x0);
	_vtblank(vt, vt->line[y].c+x0, x1-x0);
	if(x1 == vt->cols)
		vt->line[y].wrapped = 0;
	_vttouch(vt, y);
}

static void
flushdumb(Vt *vt)
{
	Priv *p;

	p = P(vt);
	if(p->ndbuf > 0)
		p->dumb(vt, p->dbuf, p->ndbuf);
	p->ndbuf = 0;
}

static void
dput(Vt *vt, Rune r)
{
	Priv *p;

	p = P(vt);
	if(p->ndbuf == Ndumb)
		flushdumb(vt);
	p->dbuf[p->ndbuf++] = r;
}

static void
put(Vt *vt, Rune r)
{
	Priv *p;
	Vtcell *c;
	int w, n;

	p = P(vt);
	if(r >= 0x60 && r <= 0x7E && p->g[p->shift])
		r = graphics[r-0x60];
	if(p->dumb){
		if(r < 0x80 || r > 0x9F){
			dput(vt, r);
			p->last = r;
		}
		return;
	}
	w = vtwidth(r);
	if(w == 0)
		return;
	if(w > vt->cols)
		return;
	if(p->pending || vt->x+w > vt->cols){
		if(vt->mode & Mwrap){
			vt->line[vt->y].wrapped = 1;
			index(vt);
			vt->x = 0;
		}else
			vt->x = vt->cols - w;
	}
	p->pending = 0;
	c = vt->line[vt->y].c;
	if(vt->mode & Minsert){
		n = vt->cols - vt->x - w;
		unwide(vt, vt->y, vt->x, 0);
		if(n > 0)
			memmove(c+vt->x+w, c+vt->x, n*sizeof c[0]);
		if(vt->cols > 0 && (c[vt->cols-1].attr & Vwide)){
			c[vt->cols-1].r = ' ';
			c[vt->cols-1].attr &= ~Vwide;
		}
	}
	unwide(vt, vt->y, vt->x, w);
	c += vt->x;
	c[0].r = r;
	c[0].fg = p->pen.fg;
	c[0].bg = p->pen.bg;
	c[0].attr = p->pen.attr;
	c[0].link = p->pen.link;
	if(w == 2){
		c[0].attr |= Vwide;
		c[1] = c[0];
		c[1].r = 0;
		c[1].attr = (c[0].attr & ~Vwide) | Vcont;
	}
	_vttouch(vt, vt->y);
	p->last = r;
	if(vt->x+w >= vt->cols){
		vt->x = vt->cols-1;
		if(vt->mode & Mwrap)
			p->pending = 1;
	}else
		vt->x += w;
}

static void
tab(Vt *vt, int n)
{
	Priv *p;

	p = P(vt);
	p->pending = 0;
	while(n > 0 && vt->x < vt->cols-1){
		vt->x++;
		if(p->tabs[vt->x])
			n--;
	}
	while(n < 0 && vt->x > 0){
		vt->x--;
		if(p->tabs[vt->x])
			n++;
	}
}

static void
control(Vt *vt, int c)
{
	Priv *p;

	p = P(vt);
	if(p->dumb){
		switch(c){
		case 0x08:
		case 0x09:
		case 0x0A:
		case 0x0D:
			dput(vt, c);
			break;
		case 0x0B:
		case 0x0C:
			dput(vt, '\n');
			break;
		case 0x0E:
			p->shift = 1;
			break;
		case 0x0F:
			p->shift = 0;
			break;
		}
		return;
	}
	switch(c){
	case 0x07:
		p->bell++;
		break;
	case 0x08:
		if(vt->x > 0)
			vt->x--;
		p->pending = 0;
		break;
	case 0x09:
		tab(vt, 1);
		break;
	case 0x0A:
	case 0x0B:
	case 0x0C:
		newline(vt);
		break;
	case 0x0D:
		vt->x = 0;
		p->pending = 0;
		break;
	case 0x0E:
		p->shift = 1;
		break;
	case 0x0F:
		p->shift = 0;
		break;
	}
}

static void
save(Vt *vt)
{
	Priv *p;
	Saved *s;

	p = P(vt);
	s = &p->saved[vt->line == p->alt];
	s->x = vt->x;
	s->y = vt->y;
	s->pen = p->pen;
	s->origin = (vt->mode & Morigin) != 0;
	s->wrap = (vt->mode & Mwrap) != 0;
	s->g[0] = p->g[0];
	s->g[1] = p->g[1];
	s->shift = p->shift;
}

static void
restore(Vt *vt)
{
	Priv *p;
	Saved *s;

	p = P(vt);
	s = &p->saved[vt->line == p->alt];
	vt->x = s->x;
	vt->y = s->y;
	clampx(vt);
	if(vt->y >= vt->rows)
		vt->y = vt->rows-1;
	p->pen = s->pen;
	vt->mode &= ~(Morigin|Mwrap);
	if(s->origin)
		vt->mode |= Morigin;
	if(s->wrap)
		vt->mode |= Mwrap;
	p->g[0] = s->g[0];
	p->g[1] = s->g[1];
	p->shift = s->shift;
	p->pending = 0;
}

static void
altscreen(Vt *vt, int on, int clear)
{
	Priv *p;
	int i;

	p = P(vt);
	if(on == (vt->line == p->alt))
		return;
	vt->line = on ? p->alt : p->main;
	if(on)
		vt->mode |= Malt;
	else
		vt->mode &= ~Malt;
	if(on && clear)
		for(i = 0; i < vt->rows; i++){
			_vtblank(vt, p->alt[i].c, vt->cols);
			p->alt[i].wrapped = 0;
		}
	_vttouchall(vt);
}

static void
setmode(Vt *vt, int on)
{
	Priv *p;
	int i, m, b;

	p = P(vt);
	for(i = 0; i < p->nparam; i++){
		if(p->sub[i])
			continue;
		m = p->param[i];
		b = 0;
		if(p->priv == '?')
			switch(m){
			case 1:	b = Mappcursor; break;
			case 6:
				b = Morigin;
				if(on)
					vt->mode |= Morigin;
				else
					vt->mode &= ~Morigin;
				moveto(vt, 0, 0);
				continue;
			case 7:	b = Mwrap; break;
			case 25:	b = Mcursor; break;
			case 66:	b = Mappkeypad; break;
			case 1000:	b = Mmouse; break;
			case 1002:	b = Mmousedrag; break;
			case 1003:	b = Mmouseall; break;
			case 1004:	b = Mfocus; break;
			case 1006:	b = Msgr; break;
			case 2004:	b = Mbracket; break;
			case 47:
			case 1047:
				altscreen(vt, on, m == 1047);
				continue;
			case 1048:
				if(on)
					save(vt);
				else
					restore(vt);
				continue;
			case 1049:
				if(on){
					save(vt);
					altscreen(vt, 1, 1);
				}else{
					altscreen(vt, 0, 0);
					restore(vt);
				}
				continue;
			}
		else if(p->priv == 0)
			switch(m){
			case 4:	b = Minsert; break;
			case 20:	b = Mnewline; break;
			}
		if(on)
			vt->mode |= b;
		else
			vt->mode &= ~b;
		if(b == Mwrap && !on)
			p->pending = 0;
	}
}

/* color reads SGR 38 or 48's color from param[*ip], leaving *ip at its last */
static int
color(Vt *vt, int *ip, ulong *c)
{
	Priv *p;
	int i, n, s, v[5];

	p = P(vt);
	i = *ip + 1;
	if(i >= p->nparam)
		return 0;
	s = p->sub[i];
	for(n = 0; n < 5 && i+n < p->nparam && (!s || p->sub[i+n]); n++)
		v[n] = p->param[i+n];
	if(n >= 2 && v[0] == 5){
		*c = Vindex | (v[1] & 0xFF);
		*ip = i + 1;
		return 1;
	}
	if(v[0] == 2){
		if(s && n == 5){
			*c = Vrgb | (v[2]&0xFF)<<16 | (v[3]&0xFF)<<8 | (v[4]&0xFF);
			*ip = i + 4;
			return 1;
		}
		if(n >= 4){
			*c = Vrgb | (v[1]&0xFF)<<16 | (v[2]&0xFF)<<8 | (v[3]&0xFF);
			*ip = i + 3;
			return 1;
		}
	}
	while(i+1 < p->nparam && p->sub[i+1])
		i++;
	*ip = i;
	return 0;
}

static void
sgr(Vt *vt)
{
	Priv *p;
	Pen *pen;
	int i, m;
	ulong c, dummy;

	p = P(vt);
	pen = &p->pen;
	if(p->nparam == 0){
		pen->fg = pen->bg = Vdefault;
		pen->attr = 0;
		return;
	}
	for(i = 0; i < p->nparam; i++){
		if(p->sub[i])
			continue;
		m = p->param[i];
		if(m < 0)
			m = 0;
		switch(m){
		case 0:
			pen->fg = pen->bg = Vdefault;
			pen->attr = 0;
			break;
		case 1:	pen->attr |= Vbold; break;
		case 2:	pen->attr |= Vfaint; break;
		case 3:	pen->attr |= Vitalic; break;
		case 4:
			if(i+1 < p->nparam && p->sub[i+1] && p->param[i+1] == 0)
				pen->attr &= ~Vunder;
			else
				pen->attr |= Vunder;
			break;
		case 5:
		case 6:	pen->attr |= Vblink; break;
		case 7:	pen->attr |= Vreverse; break;
		case 8:	pen->attr |= Vinvisible; break;
		case 9:	pen->attr |= Vstrike; break;
		case 21:	pen->attr |= Vunder; break;
		case 22:	pen->attr &= ~(Vbold|Vfaint); break;
		case 23:	pen->attr &= ~Vitalic; break;
		case 24:	pen->attr &= ~Vunder; break;
		case 25:	pen->attr &= ~Vblink; break;
		case 27:	pen->attr &= ~Vreverse; break;
		case 28:	pen->attr &= ~Vinvisible; break;
		case 29:	pen->attr &= ~Vstrike; break;
		case 38:
			if(color(vt, &i, &c))
				pen->fg = c;
			break;
		case 39:	pen->fg = Vdefault; break;
		case 48:
			if(color(vt, &i, &c))
				pen->bg = c;
			break;
		case 49:	pen->bg = Vdefault; break;
		case 58:
			color(vt, &i, &dummy);
			break;
		default:
			if(m >= 30 && m <= 37)
				pen->fg = Vindex | (m-30);
			else if(m >= 40 && m <= 47)
				pen->bg = Vindex | (m-40);
			else if(m >= 90 && m <= 97)
				pen->fg = Vindex | (m-90+8);
			else if(m >= 100 && m <= 107)
				pen->bg = Vindex | (m-100+8);
		}
	}
}

static void
insertcells(Vt *vt, int n)
{
	Vtcell *c;
	int k;

	c = vt->line[vt->y].c;
	k = vt->cols - vt->x;
	if(n > k)
		n = k;
	unwide(vt, vt->y, vt->x, 0);
	memmove(c+vt->x+n, c+vt->x, (k-n)*sizeof c[0]);
	_vtblank(vt, c+vt->x, n);
	if(c[vt->cols-1].attr & Vwide){
		c[vt->cols-1].r = ' ';
		c[vt->cols-1].attr &= ~Vwide;
	}
	vt->line[vt->y].wrapped = 0;
	_vttouch(vt, vt->y);
}

static void
deletecells(Vt *vt, int n)
{
	Vtcell *c;
	int k;

	c = vt->line[vt->y].c;
	k = vt->cols - vt->x;
	if(n > k)
		n = k;
	unwide(vt, vt->y, vt->x, n);
	memmove(c+vt->x, c+vt->x+n, (k-n)*sizeof c[0]);
	_vtblank(vt, c+vt->cols-n, n);
	vt->line[vt->y].wrapped = 0;
	_vttouch(vt, vt->y);
}

static void
csi(Vt *vt, int f)
{
	Priv *p;
	int i, n, x, y;

	p = P(vt);
	n = arg(vt, 0, 1);
	if(p->dumb && (f != 'b' || p->priv != 0 || p->inter != 0))
		return;
	if(p->priv == '>' || p->priv == '='){
		if(f == 'c' && p->priv == '>')
			reply(vt, "\033[>1;10;0c");
		return;
	}
	if(p->priv == '?' && f != 'h' && f != 'l' && f != 'J' && f != 'K')
		return;
	if(p->inter != 0){
		if(p->inter == '!' && f == 'p'){
			_vtsoftreset(vt);
			vt->x = vt->y = 0;
		}
		return;
	}
	switch(f){
	case '@':
		insertcells(vt, n);
		break;
	case 'A':
		move(vt, vt->x, vt->y-n);
		break;
	case 'B':
	case 'e':
		move(vt, vt->x, vt->y+n);
		break;
	case 'C':
	case 'a':
		move(vt, vt->x+n, vt->y);
		break;
	case 'D':
		move(vt, vt->x-n, vt->y);
		break;
	case 'E':
		move(vt, 0, vt->y+n);
		break;
	case 'F':
		move(vt, 0, vt->y-n);
		break;
	case 'G':
	case '`':
		move(vt, n-1, vt->y);
		break;
	case 'H':
	case 'f':
		moveto(vt, arg(vt, 1, 1)-1, n-1);
		break;
	case 'I':
		tab(vt, n);
		break;
	case 'Z':
		tab(vt, -n);
		break;
	case 'J':
		switch(arg(vt, 0, 0)){
		case 0:
			erase(vt, vt->y, vt->x, vt->cols);
			for(i = vt->y+1; i < vt->rows; i++)
				erase(vt, i, 0, vt->cols);
			break;
		case 1:
			for(i = 0; i < vt->y; i++)
				erase(vt, i, 0, vt->cols);
			erase(vt, vt->y, 0, vt->x+1);
			break;
		case 2:
			for(i = 0; i < vt->rows; i++)
				erase(vt, i, 0, vt->cols);
			break;
		}
		break;
	case 'K':
		switch(arg(vt, 0, 0)){
		case 0:
			erase(vt, vt->y, vt->x, vt->cols);
			break;
		case 1:
			erase(vt, vt->y, 0, vt->x+1);
			break;
		case 2:
			erase(vt, vt->y, 0, vt->cols);
			break;
		}
		break;
	case 'L':
		if(vt->y >= p->top && vt->y < p->bot){
			_vtscroll(vt, vt->y, p->bot, -n, 0);
			vt->x = 0;
			p->pending = 0;
		}
		break;
	case 'M':
		if(vt->y >= p->top && vt->y < p->bot){
			_vtscroll(vt, vt->y, p->bot, n, 0);
			vt->x = 0;
			p->pending = 0;
		}
		break;
	case 'P':
		deletecells(vt, n);
		break;
	case 'S':
		_vtscroll(vt, p->top, p->bot, n, 1);
		break;
	case 'T':
		if(nargs(vt) <= 1)
			_vtscroll(vt, p->top, p->bot, -n, 0);
		break;
	case 'X':
		erase(vt, vt->y, vt->x, vt->x+n);
		break;
	case 'b':
		if(p->last != 0)
			for(i = 0; i < n && i < 65536; i++)
				put(vt, p->last);
		break;
	case 'c':
		if(arg(vt, 0, 0) == 0)
			reply(vt, "\033[?62;22c");
		break;
	case 'd':
		moveto(vt, vt->x, n-1);
		break;
	case 'g':
		switch(arg(vt, 0, 0)){
		case 0:
			p->tabs[vt->x] = 0;
			break;
		case 3:
			memset(p->tabs, 0, vt->cols);
			break;
		}
		break;
	case 'h':
		setmode(vt, 1);
		break;
	case 'l':
		setmode(vt, 0);
		break;
	case 'm':
		sgr(vt);
		break;
	case 'n':
		switch(arg(vt, 0, 0)){
		case 5:
			reply(vt, "\033[0n");
			break;
		case 6:
			y = vt->y;
			if(vt->mode & Morigin)
				y -= p->top;
			reply(vt, "\033[%d;%dR", y+1, vt->x+1);
			break;
		}
		break;
	case 'r':
		x = arg(vt, 0, 1);
		y = arg(vt, 1, vt->rows);
		if(y > vt->rows)
			y = vt->rows;
		if(x < y){
			p->top = x-1;
			p->bot = y;
			moveto(vt, 0, 0);
		}
		break;
	case 's':
		if(p->nparam == 0)
			save(vt);
		break;
	case 'u':
		restore(vt);
		break;
	}
}

static void
esc(Vt *vt, int f)
{
	Priv *p;
	int i, j;

	p = P(vt);
	if(p->inter == '(' || p->inter == ')'){
		p->g[p->inter == ')'] = f == '0';
		return;
	}
	if(p->dumb)
		return;
	if(p->inter == '#'){
		if(f == '8'){
			for(i = 0; i < vt->rows; i++){
				for(j = 0; j < vt->cols; j++){
					vt->line[i].c[j].r = 'E';
					vt->line[i].c[j].attr = 0;
				}
				vt->line[i].wrapped = 0;
			}
			p->top = 0;
			p->bot = vt->rows;
			vt->x = vt->y = 0;
			_vttouchall(vt);
		}
		return;
	}
	if(p->inter != 0)
		return;
	switch(f){
	case '7':
		save(vt);
		break;
	case '8':
		restore(vt);
		break;
	case 'D':
		index(vt);
		break;
	case 'E':
		index(vt);
		vt->x = 0;
		break;
	case 'M':
		revindex(vt);
		break;
	case 'H':
		p->tabs[vt->x] = 1;
		break;
	case 'c':
		vtreset(vt);
		break;
	case '=':
		vt->mode |= Mappkeypad;
		break;
	case '>':
		vt->mode &= ~Mappkeypad;
		break;
	}
}

static void
link(Vt *vt, char *uri)
{
	Priv *p;
	int i;

	p = P(vt);
	if(*uri == 0){
		p->pen.link = 0;
		return;
	}
	for(i = 1; i <= vt->nlinks; i++)
		if(strcmp(vt->links[i], uri) == 0){
			p->pen.link = i;
			return;
		}
	if(vt->nlinks >= 65535)
		return;
	vt->links = erealloc(vt->links, (vt->nlinks+2)*sizeof vt->links[0]);
	vt->links[0] = nil;
	vt->links[++vt->nlinks] = _vtstrdup(uri);
	p->pen.link = vt->nlinks;
}

static void
osc(Vt *vt)
{
	Priv *p;
	char *s, *t;
	int n;

	p = P(vt);
	if(p->dumb)
		return;
	p->osc[p->nosc] = 0;
	s = p->osc;
	n = strtol(s, &t, 10);
	if(t == s || *t != ';' && *t != 0)
		return;
	if(*t == ';')
		t++;
	switch(n){
	case 0:
	case 2:
		free(vt->title);
		vt->title = _vtstrdup(t);
		p->title = 1;
		break;
	case 8:
		t = strchr(t, ';');
		if(t != nil)
			link(vt, t+1);
		break;
	}
}

static void
clear(Priv *p)
{
	p->nparam = 0;
	p->priv = 0;
	p->inter = 0;
	memset(p->sub, 0, sizeof p->sub);
}

static void
param(Priv *p, int c)
{
	if(c >= '0' && c <= '9'){
		if(p->nparam == 0)
			p->nparam = 1;
		if(p->param[p->nparam-1] < 0)
			p->param[p->nparam-1] = 0;
		if(p->param[p->nparam-1] < 100000)
			p->param[p->nparam-1] = p->param[p->nparam-1]*10 + c-'0';
		return;
	}
	if(p->nparam == 0){
		p->param[0] = -1;
		p->nparam = 1;
	}
	if(p->nparam < Nparam){
		p->param[p->nparam] = -1;
		p->sub[p->nparam] = c == ':';
		p->nparam++;
	}
}

static void
byte(Vt *vt, int c)
{
	Priv *p;

	p = P(vt);
	if(c == 0x18 || c == 0x1A){
		p->state = Ground;
		return;
	}
	if(c == 0x1B && p->state != Osc && p->state != Oscesc && p->state != Str && p->state != Stresc){
		p->state = Esc;
		clear(p);
		return;
	}
	switch(p->state){
	case Ground:
		if(c < 0x20 || c == 0x7F)
			control(vt, c);
		else
			put(vt, c);
		return;
	case Esc:
	case Escint:
		if(c < 0x20){
			control(vt, c);
			return;
		}
		if(c >= 0x20 && c <= 0x2F){
			p->inter = c;
			p->state = Escint;
			return;
		}
		p->state = Ground;
		if(p->inter == 0)
			switch(c){
			case '[':
				clear(p);
				p->param[0] = -1;
				p->state = Csi;
				return;
			case ']':
				p->nosc = 0;
				p->state = Osc;
				return;
			case 'P':
			case 'X':
			case '^':
			case '_':
				p->state = Str;
				return;
			}
		esc(vt, c);
		return;
	case Csi:
		if(c < 0x20){
			control(vt, c);
			return;
		}
		if(c >= '0' && c <= ';'){
			if(p->inter != 0){
				p->state = Csiignore;
				return;
			}
			param(p, c);
			return;
		}
		if(c >= '<' && c <= '?'){
			if(p->nparam > 0 || p->priv != 0 || p->inter != 0)
				p->state = Csiignore;
			else
				p->priv = c;
			return;
		}
		if(c >= 0x20 && c <= 0x2F){
			p->inter = c;
			return;
		}
		p->state = Ground;
		if(c >= 0x40 && c <= 0x7E){
			if(p->nparam == 1 && p->param[0] < 0)
				p->nparam = 0;
			csi(vt, c);
		}
		return;
	case Csiignore:
		if(c < 0x20)
			control(vt, c);
		else if(c >= 0x40 && c <= 0x7E)
			p->state = Ground;
		return;
	case Osc:
		if(c == 0x07){
			osc(vt);
			p->state = Ground;
		}else if(c == 0x1B)
			p->state = Oscesc;
		else if(c >= 0x20 && p->nosc < Nosc-1)
			p->osc[p->nosc++] = c;
		return;
	case Oscesc:
		osc(vt);
		/* fall through */
	case Stresc:
		p->state = Ground;
		if(c != '\\'){
			p->state = Esc;
			clear(p);
			byte(vt, c);
		}
		return;
	case Str:
		if(c == 0x1B)
			p->state = Stresc;
		return;
	}
}

void
vtwrite(Vt *vt, uchar *buf, long n)
{
	Priv *p;
	Rune r;
	long i;

	p = P(vt);
	_vtbegin(vt);
	for(i = 0; i < n; i++){
		if(p->nutf > 0){
			if((buf[i] & 0xC0) == 0x80){
				p->utf[p->nutf++] = buf[i];
				if(fullrune((char*)p->utf, p->nutf)){
					chartorune(&r, (char*)p->utf);
					p->nutf = 0;
					if(p->state == Osc){
						if(p->nosc+UTFmax < Nosc)
							p->nosc += runetochar(p->osc+p->nosc, &r);
					}else if(p->state == Ground)
						put(vt, r);
				}
				continue;
			}
			p->nutf = 0;
			if(p->state == Ground)
				put(vt, Runeerror);
		}
		if(buf[i] >= 0xC0 && buf[i] < 0xF8 && (p->state == Ground || p->state == Osc)){
			p->utf[0] = buf[i];
			p->nutf = 1;
			continue;
		}
		if(buf[i] >= 0x80 && p->state == Ground){
			put(vt, Runeerror);
			continue;
		}
		byte(vt, buf[i]);
	}
	if(p->dumb)
		flushdumb(vt);
}
