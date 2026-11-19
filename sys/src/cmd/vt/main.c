#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <mouse.h>
#include <keyboard.h>
#include <vt.h>

enum {
	Xmargin	= 5,
	Ymargin	= 5,
	Inset	= 3,
	Nhist	= 2000,
	Bsize	= 8192,
	Ncolor	= 64,
	Stack	= 16*1024,
};

typedef struct Buf Buf;
typedef struct Hline Hline;
typedef struct Color Color;
typedef struct Ttystate Ttystate;

struct Buf {
	long	n;
	uchar	d[Bsize];
};

struct Hline {
	Vtcell	*c;
	int	n;
};

struct Color {
	ulong	rgb;
	Image	*i;
};

struct Ttystate {
	int	crnl;
	int	nlcr;
};

int	mainstacksize = 64*1024;

/* ptty turns a newline into a return and a newline; these add more, as the menu asks */
Ttystate	tty;

char *menutext2[] = {
	"backup",
	"forward",
	"reset",
	"clear",
	"send",
	"page",
	nil
};

char *menutext3[] = {
	"24x80",
	"crnl",
	"nl",
	"raw",
	"exit",
	nil
};

Menu	menu2 = { menutext2 };
Menu	menu3 = { menutext3 };

ulong ansi[16] = {
	0x000000, 0xAA0000, 0x00AA00, 0xFF5500, 0x0000FF, 0xAA00AA, 0x00AAAA, 0x7F7F7F,
	0x555555, 0xFF5555, 0x55FF55, 0xFFFF55, 0x5555FF, 0xFF55FF, 0x55FFFF, 0xFFFFFF,
};

Vt	*vt;
Mousectl	*mc;
Keyboardctl	*kc;
Channel	*hostc;
char	*term;
int	nocolor;
ulong	fgrgb, bgrgb;
int	CW, NS;
Image	*bordercol;
Image	*red;
Image	*cursback;
int	cursdrawn;
Point	curspt;
Color	colors[Ncolor];
int	ncolors;
Hline	hist[Nhist];
int	hhead, nhist;
int	back;
int	raw;	/* the menu asked ptty for raw */
int	pagemode, blocked, olines;
Buf	*cur;
uchar	*pend;
long	npend;
int	outfd = -1;
int	logfd = -1;
int	hostpid;	/* ptty's */
int	pttyfd = -1;

void
usage(void)
{
	fprint(2, "usage: %s [-2abcx] [-f font] [-l logfile] [cmd]\n", argv0);
	threadexitsall("usage");
}

void*
emalloc(ulong n)
{
	void *v;

	v = mallocz(n, 1);
	if(v == nil)
		sysfatal("out of memory");
	return v;
}

void*
erealloc(void *v, ulong n)
{
	v = realloc(v, n);
	if(v == nil)
		sysfatal("out of memory");
	return v;
}

Point
pt(int x, int y)
{
	return addpt(screen->r.min, Pt(x*CW+Xmargin, y*NS+Ymargin));
}

Image*
colimg(ulong rgb)
{
	int i;

	for(i = 0; i < ncolors; i++)
		if(colors[i].rgb == rgb)
			return colors[i].i;
	if(ncolors == Ncolor){
		for(i = 0; i < ncolors; i++)
			freeimage(colors[i].i);
		ncolors = 0;
	}
	colors[ncolors].rgb = rgb;
	colors[ncolors].i = allocimage(display, Rect(0,0,1,1), screen->chan, 1, rgb<<8 | 0xFF);
	if(colors[ncolors].i == nil)
		sysfatal("allocimage: %r");
	return colors[ncolors++].i;
}

int
cube(int n)
{
	return n ? 55+40*n : 0;
}

ulong
rgbof(ulong c, ulong def)
{
	int i, g;

	switch(c & 0xFF000000){
	case Vindex:
		i = c & 0xFF;
		if(i < 16)
			return ansi[i];
		if(i < 232){
			i -= 16;
			return cube(i/36)<<16 | cube(i/6%6)<<8 | cube(i%6);
		}
		g = 8+10*(i-232);
		return g<<16 | g<<8 | g;
	case Vrgb:
		return c & 0xFFFFFF;
	}
	return def;
}

void
style(Vtcell *c, Image **fg, Image **bg)
{
	ulong f, b, t;

	f = c->fg;
	b = c->bg;
	if(nocolor)
		f = b = Vdefault;
	if((c->attr & Vbold) && (f & 0xFF000000) == Vindex && (f & 0xFF) < 8)
		f += 8;
	f = rgbof(f, fgrgb);
	b = rgbof(b, bgrgb);
	if(c->attr & Vreverse){
		t = f;
		f = b;
		b = t;
	}
	*fg = colimg(f);
	*bg = colimg(b);
}

int
same(Vtcell *a, Vtcell *b)
{
	return a->fg == b->fg && a->bg == b->bg && (a->attr & ~(Vwide|Vcont)) == (b->attr & ~(Vwide|Vcont));
}

void
drawline(int y, Vtcell *c, int n)
{
	Image *fg, *bg;
	Point p;
	int x, e, k;

	if(n > vt->cols)
		n = vt->cols;
	draw(screen, Rpt(pt(0, y), pt(vt->cols, y+1)), colimg(bgrgb), nil, ZP);
	for(x = 0; x < n; x = e){
		for(e = x+1; e < n && same(&c[e], &c[x]); e++)
			;
		style(&c[x], &fg, &bg);
		draw(screen, Rpt(pt(x, y), pt(e, y+1)), bg, nil, ZP);
		if(!(c[x].attr & Vinvisible))
			for(k = x; k < e; k++)
				if(c[k].r != ' ' && c[k].r != 0 && !(c[k].attr & Vcont))
					runestringn(screen, pt(k, y), fg, ZP, font, &c[k].r, 1);
		if(c[x].attr & Vunder){
			p = pt(x, y);
			draw(screen, Rect(p.x, p.y+font->ascent+1, pt(e, y).x, p.y+font->ascent+2), fg, nil, ZP);
		}
		if(c[x].attr & Vstrike){
			p = pt(x, y);
			draw(screen, Rect(p.x, p.y+NS/2, pt(e, y).x, p.y+NS/2+1), fg, nil, ZP);
		}
	}
}

/* row y of the window, which shows the history's last back lines above the screen */
void
drawrow(int y)
{
	Hline *h;
	int i;

	i = nhist - back + y;
	if(i < nhist){
		h = &hist[(hhead+i) % Nhist];
		drawline(y, h->c, h->n);
	}else
		drawline(y, vt->line[i-nhist].c, vt->cols);
}

void
drawall(void)
{
	int y;

	draw(screen, screen->r, colimg(bgrgb), nil, ZP);
	for(y = 0; y < vt->rows; y++)
		drawrow(y);
}

void
cursoff(void)
{
	if(cursdrawn)
		draw(screen, Rpt(curspt, addpt(curspt, Pt(CW+1, NS+1))), cursback, nil, ZP);
	cursdrawn = 0;
}

void
curson(void)
{
	if(back > 0 || !(vt->mode & Mcursor) && !blocked)
		return;
	curspt = pt(vt->x, vt->y);
	draw(cursback, cursback->r, screen, nil, curspt);
	border(screen, Rpt(curspt, addpt(curspt, Pt(CW, NS))), 2, blocked ? red : bordercol, ZP);
	cursdrawn = 1;
}

void
keep(Vtline *l)
{
	Hline *h;

	if(nhist < Nhist)
		h = &hist[(hhead + nhist++) % Nhist];
	else{
		h = &hist[hhead];
		hhead = (hhead+1) % Nhist;
		free(h->c);
	}
	h->n = vt->cols;
	h->c = emalloc(h->n * sizeof h->c[0]);
	memmove(h->c, l->c, h->n * sizeof h->c[0]);
}

void
label(char *s)
{
	int fd;

	fd = open("/dev/label", OWRITE);
	if(fd >= 0){
		write(fd, s, strlen(s));
		close(fd);
	}
}

void
scrollrect(Vtscroll *s)
{
	int n;

	n = s->n;
	if(n > 0 && n < s->bot-s->top)
		draw(screen, Rpt(pt(0, s->top), pt(vt->cols, s->bot-n)), screen, nil, pt(0, s->top+n));
	else if(n < 0 && -n < s->bot-s->top)
		draw(screen, Rpt(pt(0, s->top-n), pt(vt->cols, s->bot)), screen, nil, pt(0, s->top));
}

/* apply what changed since the last frame */
void
frame(void)
{
	Vtframe f;
	int i, any;

	vtframe(vt, &f);
	for(i = 0; i < f.ngone; i++)
		keep(&vt->gone[i]);
	any = f.all || f.nscroll || f.cursor;
	for(i = 0; i < vt->rows; i++)
		any |= f.dirty[i];
	if(f.title)
		label(vt->title);
	if(!any)
		return;
	cursoff();
	if(back > 0){
		back += f.ngone;
		if(back > nhist)
			back = nhist;
		drawall();
	}else if(f.all)
		drawall();
	else{
		for(i = 0; i < f.nscroll; i++)
			scrollrect(&f.scroll[i]);
		for(i = 0; i < vt->rows; i++)
			if(f.dirty[i])
				drawrow(i);
	}
	curson();
	flushimage(display, 1);
}

void
view(int n)
{
	if(n > nhist)
		n = nhist;
	if(n < 0)
		n = 0;
	if(n == back)
		return;
	back = n;
	cursoff();
	drawall();
	curson();
	flushimage(display, 1);
}

void
tohost(char *s, int n)
{
	if(write(outfd, s, n) < 0)
		threadexitsall("write");
}

void
reply(Vt*, char *s, int n)
{
	tohost(s, n);
}

/* the host's bytes to the screen, with the returns and newlines the menu adds */
void
hostbytes(uchar *b, long n)
{
	Ttystate *t;
	uchar o[2*Bsize];
	long i, j, m, k;

	t = &tty;
	if(!t->crnl && !t->nlcr){
		vtwrite(vt, b, n);
		return;
	}
	for(i = 0; i < n; i += k){
		k = n-i < Bsize ? n-i : Bsize;
		m = 0;
		for(j = i; j < i+k; j++){
			o[m++] = b[j];
			if(b[j] == '\r' && t->crnl)
				o[m++] = '\n';
			else if(b[j] == '\n' && t->nlcr)
				o[m++] = '\r';
		}
		vtwrite(vt, o, m);
	}
}

/* the host's bytes waiting, until a page fills in page mode */
void
feed(void)
{
	uchar *p;
	long k;

	while(npend > 0 && !blocked){
		if(!pagemode){
			hostbytes(pend, npend);
			npend = 0;
			break;
		}
		p = memchr(pend, '\n', npend);
		k = p ? p-pend+1 : npend;
		if(p && olines >= vt->rows-1 && vt->y == vt->rows-1){
			blocked = 1;
			break;
		}
		hostbytes(pend, k);
		pend += k;
		npend -= k;
		if(p)
			olines++;
	}
	if(npend == 0){
		free(cur);
		cur = nil;
	}
}

void
host(Buf *b)
{
	cur = b;
	pend = b->d;
	npend = b->n;
	if(logfd >= 0)
		write(logfd, b->d, b->n);
	feed();
	while(!blocked && nbrecv(hostc, &b) > 0){
		if(b == nil){
			frame();
			threadexitsall(nil);
		}
		cur = b;
		pend = b->d;
		npend = b->n;
		if(logfd >= 0)
			write(logfd, b->d, b->n);
		feed();
	}
	frame();
}

/* ptty's consctl, mounted once ptty has posted it */
int
pttyopen(void)
{
	char buf[32];
	int fd;

	if(pttyfd < 0 && hostpid > 0){
		snprint(buf, sizeof buf, "/srv/ptty.%d", hostpid);
		fd = open(buf, ORDWR);
		if(fd < 0)
			return -1;
		if(mount(fd, -1, "/mnt/ptty", MREPL, "") < 0){
			close(fd);
			return -1;
		}
		pttyfd = open("/mnt/ptty/consctl", ORDWR|OCEXEC);
	}
	return pttyfd;
}

void
pttysize(int cols, int rows)
{
	if(pttyopen() >= 0)
		fprint(pttyfd, "size %d %d", cols, rows);
}

/* ptty's interrupt character, while it takes one: IOR's lflag has ISIG */
int
intrchar(void)
{
	char buf[256], *f[20];
	int n;

	if(pttyopen() < 0 || (n = pread(pttyfd, buf, sizeof buf-1, 0)) <= 0)
		return -1;
	buf[n] = 0;
	if(tokenize(buf, f, nelem(f)) < 16 || strcmp(f[0], "IOR") != 0)
		return -1;
	if(!(strtoul(f[4], nil, 16) & 0x40))
		return -1;
	return strtoul(f[8], nil, 16);
}

void
key(Rune r)
{
	char b[64];
	int n, c;

	olines = 0;
	if(blocked){
		blocked = 0;
		feed();
		frame();
		if(r == Kdown)
			return;
	}
	view(0);
	if(r == Kdel && (c = intrchar()) >= 0){
		b[0] = c;
		tohost(b, 1);
		return;
	}
	if(r == '\n' && (vt->mode & Mnewline)){
		tohost("\r\n", 2);
		return;
	}
	if(r == '\n')
		r = '\r';
	else if(r == '\r')
		r = '\n';
	n = vtkey(vt, r, b, sizeof b);
	if(n > 0)
		tohost(b, n);
}

void
paste(void)
{
	char *s, *b;
	long n, m;
	int fd;

	fd = open("/dev/snarf", OREAD);
	if(fd < 0)
		return;
	s = nil;
	n = 0;
	for(;;){
		s = realloc(s, n+Bsize+1);
		m = read(fd, s+n, Bsize);
		if(m <= 0)
			break;
		n += m;
	}
	close(fd);
	if(n == 0){
		free(s);
		return;
	}
	view(0);
	b = emalloc(2*n+12);
	m = vtpaste(vt, s, n, b, 2*n+12);
	if(m > 0)
		tohost(b, m);
	free(b);
	free(s);
}

void
exportsize(void)
{
	char buf[20];

	snprint(buf, sizeof buf, "%d", Dx(screen->r)-2*Xmargin);
	putenv("XPIXELS", buf);
	snprint(buf, sizeof buf, "%d", Dy(screen->r)-2*Ymargin);
	putenv("YPIXELS", buf);
	snprint(buf, sizeof buf, "%d", vt->rows);
	putenv("LINES", buf);
	snprint(buf, sizeof buf, "%d", vt->cols);
	putenv("COLS", buf);
	putenv("COLUMNS", buf);
	putenv("TERM", term);
}

void
winsize(int *cols, int *rows)
{
	*cols = (Dx(screen->r)-2*Xmargin)/CW;
	*rows = (Dy(screen->r)-2*Ymargin)/NS;
	if(*cols < 1)
		*cols = 1;
	if(*rows < 1)
		*rows = 1;
}

void
resized(void)
{
	int cols, rows;

	if(getwindow(display, Refnone) < 0)
		sysfatal("can't reattach to window: %r");
	cursdrawn = 0;
	winsize(&cols, &rows);
	if(cols != vt->cols || rows != vt->rows){
		vtresize(vt, cols, rows);
		exportsize();
		pttysize(cols, rows);
	}
	frame();
	cursdrawn = 0;
	drawall();
	curson();
	flushimage(display, 1);
}

void
setdim(int ht, int wid)
{
	Rectangle r;
	int fd;

	r.min = screen->r.min;
	r.max = addpt(screen->r.min, Pt(wid*CW+2*Xmargin+2*Inset, ht*NS+2*Ymargin+2*Inset));
	fd = open("/dev/wctl", OWRITE);
	if(fd >= 0){
		fprint(fd, "resize -dx %d -dy %d\n", Dx(r)+2*Borderwidth, Dy(r)+2*Borderwidth);
		close(fd);
	}
}

/* the lines above the cursor's go to the history, as if the host had scrolled them */
void
clear(void)
{
	char buf[32];

	view(0);
	if(vt->y > 0){
		snprint(buf, sizeof buf, "\033[%dS\033[1;%dH", vt->y, vt->x+1);
		vtwrite(vt, (uchar*)buf, strlen(buf));
		frame();
	}
}

void
menu(int but)
{
	if(but == 3){
		menu3.item[1] = tty.crnl ? "cr" : "crnl";
		menu3.item[2] = tty.nlcr ? "nl" : "nlcr";
		menu3.item[3] = raw ? "cooked" : "raw";
		switch(menuhit(3, mc, &menu3, nil)){
		case 0:
			setdim(24, 80);
			break;
		case 1:
			tty.crnl = !tty.crnl;
			break;
		case 2:
			tty.nlcr = !tty.nlcr;
			break;
		case 3:
			if(pttyopen() >= 0 && fprint(pttyfd, raw ? "rawoff" : "rawon") > 0)
				raw = !raw;
			break;
		case 4:
			postnote(PNGROUP, hostpid, "hangup");
			threadexitsall(nil);
		}
		return;
	}
	menu2.item[5] = pagemode ? "scroll" : "page";
	switch(menuhit(2, mc, &menu2, nil)){
	case 0:
		view(back + vt->rows);
		break;
	case 1:
		view(back - vt->rows);
		break;
	case 2:
		view(0);
		break;
	case 3:
		clear();
		break;
	case 4:
		paste();
		break;
	case 5:
		pagemode = !pagemode;
		olines = 0;
		if(blocked && !pagemode){
			blocked = 0;
			feed();
			frame();
		}
		break;
	}
}

/* the mouse goes to the host if it asks for it; else the wheel moves through the history */
void
mouse(Mouse m)
{
	static int ob, ox, oy;
	char b[64];
	int x, y, n;

	if(m.buttons & 2){
		menu(2);
		return;
	}
	if(m.buttons & 4){
		menu(3);
		return;
	}
	if(!(vt->mode & (Mmouse|Mmousedrag|Mmouseall)) || back > 0){
		if(m.buttons & 8)
			view(back + 3);
		else if(m.buttons & 16)
			view(back - 3);
		ob = m.buttons;
		return;
	}
	x = (m.xy.x - screen->r.min.x - Xmargin) / CW;
	y = (m.xy.y - screen->r.min.y - Ymargin) / NS;
	if(x < 0)
		x = 0;
	if(x >= vt->cols)
		x = vt->cols-1;
	if(y < 0)
		y = 0;
	if(y >= vt->rows)
		y = vt->rows-1;
	n = 0;
	if((m.buttons & 1) && !(ob & 1))
		n = vtmouse(vt, 1, x, y, 1, b, sizeof b);
	else if(!(m.buttons & 1) && (ob & 1))
		n = vtmouse(vt, 1, x, y, 0, b, sizeof b);
	else if((m.buttons & 8) && !(ob & 8))
		n = vtmouse(vt, 4, x, y, 1, b, sizeof b);
	else if((m.buttons & 16) && !(ob & 16))
		n = vtmouse(vt, 5, x, y, 1, b, sizeof b);
	else if(x != ox || y != oy)
		n = vtmouse(vt, m.buttons & 1, x, y, -1, b, sizeof b);
	if(n > 0)
		tohost(b, n);
	ob = m.buttons;
	ox = x;
	oy = y;
}

void
hostproc(void*)
{
	Buf *b;

	threadsetname("vt host");
	for(;;){
		b = emalloc(sizeof *b);
		b->n = read(outfd, b->d, sizeof b->d);
		if(b->n <= 0){
			free(b);
			sendp(hostc, nil);
			return;
		}
		sendp(hostc, b);
	}
}

void
execproc(void *v)
{
	Channel *pidc;
	int fd;

	pidc = v;
	threadsetname("vt exec");
	rfork(RFFDG|RFNOTEG);
	fd = open("/dev/cons", OREAD);
	dup(fd, 0);
	if(fd != 0)
		close(fd);
	fd = open("/dev/cons", OWRITE);
	dup(fd, 1);
	dup(fd, 2);
	if(fd != 1 && fd != 2)
		close(fd);
	procexecl(pidc, "/bin/ptty", "ptty", nil);
	fprint(2, "vt: exec ptty: %r\n");
	threadexits("exec");
}

void
threadmain(int argc, char **argv)
{
	char *fontname, *p;
	int cols, rows, blkbg;
	Channel *pidc;
	Buf *b;
	Mouse m;
	Rune r;
	Alt a[5];

	rfork(RFNAMEG|RFNOTEG);
	fontname = nil;
	term = "xterm-256color";
	blkbg = 0;
	ARGBEGIN{
	case '2':
		term = "vt220";
		break;
	case 'a':
		term = "ansi";
		break;
	case 'b':
		blkbg = 1;
		break;
	case 'c':
		nocolor = 1;
		break;
	case 'f':
		fontname = EARGF(usage());
		break;
	case 'l':
		p = EARGF(usage());
		logfd = create(p, OWRITE|OCEXEC, 0666);
		if(logfd < 0)
			sysfatal("could not create log file: %s: %r", p);
		break;
	case 'x':
		term = "xterm";
		break;
	default:
		usage();
	}ARGEND

	if(initdraw(nil, fontname, "vt") < 0)
		sysfatal("initdraw: %r");
	mc = initmouse(nil, screen);
	if(mc == nil)
		sysfatal("initmouse: %r");
	kc = initkeyboard(nil);
	if(kc == nil)
		sysfatal("initkeyboard: %r");
	NS = font->height;
	CW = stringwidth(font, "m");
	fgrgb = blkbg ? 0xFFFFFF : 0x000000;
	bgrgb = blkbg ? 0x000000 : 0xFFFFFF;
	red = allocimage(display, Rect(0,0,1,1), screen->chan, 1, DRed);
	bordercol = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xCCCCCCFF);
	cursback = allocimage(display, Rect(0, 0, CW+1, NS+1), screen->chan, 0, DNofill);
	if(red == nil || bordercol == nil || cursback == nil)
		sysfatal("allocimage: %r");

	winsize(&cols, &rows);
	vt = vtnew(cols, rows);
	if(vt == nil)
		sysfatal("vtnew: %r");
	vt->reply = reply;

	/* ptty's terminal is a pipe, whose other end is vt's */
	if(bind("#|", "/dev", MBEFORE) < 0)
		sysfatal("bind #|: %r");
	outfd = open("/dev/data", ORDWR|OCEXEC);
	if(outfd < 0)
		sysfatal("open pipe: %r");
	if(bind("/dev/data1", "/dev/cons", MREPL) < 0)
		sysfatal("bind cons: %r");
	exportsize();

	hostc = chancreate(sizeof(Buf*), 8);
	pidc = chancreate(sizeof(ulong), 0);
	proccreate(execproc, pidc, Stack);
	hostpid = recvul(pidc);
	if(hostpid == ~0)
		sysfatal("exec ptty: %r");
	proccreate(hostproc, nil, Stack);

	drawall();
	curson();
	flushimage(display, 1);
	if(argc > 0){
		tohost(argv[0], strlen(argv[0]));
		tohost("\n", 1);
	}

	a[0] = (Alt){mc->c, &m, CHANRCV};
	a[1] = (Alt){mc->resizec, nil, CHANRCV};
	a[2] = (Alt){kc->c, &r, CHANRCV};
	a[3] = (Alt){hostc, &b, CHANRCV};
	a[4] = (Alt){nil, nil, CHANEND};
	for(;;){
		a[3].op = blocked || npend > 0 ? CHANNOP : CHANRCV;
		switch(alt(a)){
		case 0:
			mouse(m);
			break;
		case 1:
			resized();
			break;
		case 2:
			key(r);
			break;
		case 3:
			if(b == nil)
				threadexitsall(nil);
			host(b);
			break;
		}
	}
}
