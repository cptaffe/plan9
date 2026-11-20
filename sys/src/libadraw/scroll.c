#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <mouse.h>
#include <adraw.h>

int
scrollwid(void)
{
	return scalesize(display, 12);
}

int
scrollgap(void)
{
	return scalesize(display, 4);
}

static Image*
col(ulong v)
{
	return allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, v);
}

int
scrollinit(Scroll *s)
{
	Themecol *c;
	ulong v;

	v = 0;
	if((c = themelook("body")) != nil)
		v = c->bg ? c->bg : c->col;
	if(v == 0)
		v = 0xFFFFEAFF;
	s->back = col(v);
	s->bord = col(themecolor("bodyborder", DYellowgreen));
	if(s->back == nil || s->bord == nil){
		freeimage(s->back);
		freeimage(s->bord);
		s->back = s->bord = nil;
		return -1;
	}
	return 0;
}

static Rectangle
thumb(Rectangle r, vlong p0, vlong p1, vlong tot)
{
	Rectangle q;
	int h;

	q = r;
	h = Dy(q);
	if(tot <= 0)
		return q;
	if(p0 > 0)
		q.min.y += h*p0/tot;
	if(p1 < tot)
		q.max.y -= h*(tot-p1)/tot;
	if(q.max.y < q.min.y+2){
		if(q.min.y+2 <= r.max.y)
			q.max.y = q.min.y+2;
		else
			q.min.y = q.max.y-2;
	}
	return q;
}

void
scrolldraw(Image *dst, Scroll *s)
{
	Rectangle t;

	draw(dst, s->r, s->bord, nil, ZP);
	t = thumb(s->r, s->p0, s->p1, s->tot);
	draw(dst, t, s->back, nil, ZP);
	t.min.x = t.max.x-1;
	draw(dst, t, s->bord, nil, ZP);
}

static vlong
clamp(Scroll *s, vlong p0)
{
	vlong max;

	max = s->tot - (s->p1 - s->p0);
	if(p0 > max)
		p0 = max;
	if(p0 < 0)
		p0 = 0;
	return p0;
}

void
scrollmouse(Mousectl *mc, Scroll *s, void (*set)(Scroll*, vlong, void*), void *a)
{
	Rectangle r;
	vlong p0, old;
	int but, x, my, h, first, n;

	for(but = 1; but <= 3; but++)
		if(mc->buttons & 1<<(but-1))
			break;
	if(but > 3)
		return;
	r = insetrect(s->r, 1);
	h = Dy(r);
	if(h <= 0)
		return;
	x = (r.min.x+r.max.x)/2;
	old = -1;
	first = 1;
	do{
		flushimage(display, 1);
		my = mc->xy.y;
		if(my < r.min.y)
			my = r.min.y;
		if(my >= r.max.y)
			my = r.max.y;
		if(!eqpt(mc->xy, Pt(x, my))){
			moveto(mc, Pt(x, my));
			readmouse(mc);
		}
		n = (s->p1 - s->p0)*(my - r.min.y)/h;
		if(but == 2)
			p0 = s->tot*(my - r.min.y)/h;
		else if(but == 1)
			p0 = s->p0 - n;
		else
			p0 = s->p0 + n;
		p0 = clamp(s, p0);
		if(p0 != old && p0 != s->p0){
			s->p1 += p0 - s->p0;
			s->p0 = p0;
			set(s, p0, a);
		}
		old = p0;
		if(but == 2){
			readmouse(mc);
			continue;
		}
		if(first){
			flushimage(display, 1);
			sleep(200);
			first = 0;
		}else
			sleep(80);
		while(nbrecv(mc->c, &mc->Mouse) > 0)
			;
	}while(mc->buttons & 1<<(but-1));
	while(mc->buttons)
		readmouse(mc);
}
