#include <u.h>
#include <libc.h>
#include <draw.h>
#include <mouse.h>
#include <frame.h>

static int
nbytes(char *s0, int nr)
{
	char *s;
	Rune r;

	s = s0;
	while(--nr >= 0)
		s += chartorune(&r, s);
	return s-s0;
}

/*
 * stylecols_for — return the NCOL colour set for style index idx.
 * Falls back to f->cols for out-of-range indices (including 0 = default).
 */
static Image**
stylecols_for(Frame *f, int idx)
{
	if(f->stylecols == nil || idx < 0 || idx >= f->nstylecols)
		return f->cols;
	return f->stylecols + idx * NCOL;
}

/*
 * drawrange_impl — draw characters [p0, p1) in a single O(boxes+segments)
 * pass, advancing a box cursor and a style-segment cursor in lockstep.
 *
 * forcesel=0: selection state is computed from f->p0/f->p1 per character.
 * forcesel=1: the entire range is drawn with selection state issel_forced
 *             (used by frdrawsel to apply a uniform highlight/unhighlight).
 */
static Point
drawrange_impl(Frame *f, Point pt, ulong p0, ulong p1, int forcesel, int issel_forced)
{
	Frbox  *b;
	int     bi;       /* index into f->box */
	ulong   bp;       /* char position of f->box[bi] start */
	int     si;       /* index into f->styles */
	ulong   sp;       /* char position of f->styles[si] start */
	ulong   seg_end;  /* char position of f->styles[si] end */
	int     sidx;     /* current style index */
	Image **sc;       /* current colour set */
	Image  *back, *text;
	ulong   p;        /* current drawing position */
	ulong   next;     /* end of current atomic sub-span */
	int     nr;       /* rune count of current box */
	int     sub_nr;   /* rune count of current sub-span */
	int     w, x;
	char   *ptr;
	Point   qt;
	int     trimmed;
	int     issel;

	if(p0 >= p1 || f->b == nil)
		return pt;

	/* Locate the starting box: skip boxes entirely before p0. */
	bp = 0;
	for(bi = 0; bi < f->nbox; bi++){
		nr = (f->box[bi].nrune < 0) ? 1 : f->box[bi].nrune;
		if(bp + (ulong)nr > p0)
			break;
		bp += nr;
	}
	if(bi == f->nbox)
		return pt;

	/*
	 * Locate the starting style segment: skip segments entirely before p0.
	 * When there are no styles, sc stays f->cols and seg_end is computed
	 * as p1 each iteration (the whole range is one implicit segment).
	 */
	si   = 0;
	sp   = 0;
	sidx = 0;
	if(f->nstyles > 0 && f->styles != nil){
		for(; si < f->nstyles; si++){
			ulong slen = f->styles[si*2+1];
			if(sp + slen > p0)
				break;
			sp += slen;
		}
		sidx = (si < f->nstyles) ? (int)f->styles[si*2] : 0;
	}
	sc = stylecols_for(f, sidx);

	p       = p0;
	trimmed = 0;
	while(p < p1 && bi < f->nbox){
		b  = &f->box[bi];
		nr = (b->nrune < 0) ? 1 : b->nrune;

		/* End of current box and current style segment. */
		ulong box_end = bp + (ulong)nr;
		seg_end = (f->nstyles > 0 && f->styles != nil && si < f->nstyles)
		        ? sp + f->styles[si*2+1] : p1;

		/* Next selection boundary (only relevant when forcesel is off). */
		ulong sel_next = p1;
		if(!forcesel){
			if(f->p0 > p && f->p0 < sel_next) sel_next = f->p0;
			if(f->p1 > p && f->p1 < sel_next) sel_next = f->p1;
		}

		/* Atomic sub-span: advance to the first of these boundaries. */
		next = box_end;
		if(seg_end  < next) next = seg_end;
		if(sel_next < next) next = sel_next;
		if(p1       < next) next = p1;

		/*
		 * Colours for this sub-span.
		 * Newline boxes paint the empty space to the right margin — that
		 * space contains no content so it always uses base colours.
		 */
		Image **sc_eff = (b->nrune < 0) ? f->cols : sc;
		issel = forcesel ? issel_forced : (p >= f->p0 && p < f->p1);
		if(issel){ back = sc_eff[HIGH]; text = sc_eff[HTEXT]; }
		else      { back = sc_eff[BACK]; text = sc_eff[TEXT];  }

		/*
		 * Line-wrap fill: call _frcklinewrap only at the start of a box.
		 * Mid-box style/selection boundaries never cause wrapping because
		 * boxes are pre-split by bxscan to fit on a single line.
		 *
		 * The space to the right of a newline is empty — it contains no
		 * content characters, so it must use the base window colours
		 * regardless of what style the current span carries.
		 */
		if(p == bp){
			qt = pt;
			_frcklinewrap(f, &pt, b);
			if(pt.y > qt.y)
				draw(f->b, Rect(qt.x, qt.y, f->r.max.x, pt.y),
				     issel ? f->cols[HIGH] : f->cols[BACK], nil, qt);
		}

		/* Draw sub-span [p, next) of box bi. */
		ptr = (char*)b->ptr;
		if(p > bp)
			ptr += nbytes(ptr, (int)(p - bp));
		sub_nr  = (int)(next - p);
		trimmed = (next < box_end);

		if(b->nrune < 0 || sub_nr == b->nrune) w = b->wid;
		else                                    w = stringnwidth(f->font, ptr, sub_nr);
		x = pt.x + w;
		if(x > f->r.max.x) x = f->r.max.x;

		draw(f->b, Rect(pt.x, pt.y, x, pt.y + f->font->height), back, nil, pt);
		if(b->nrune >= 0)
			stringnbg(f->b, pt, text, ZP, f->font, ptr, sub_nr, back, ZP);
		pt.x += w;
		p = next;

		/* Advance box cursor when the box is fully consumed. */
		if(p == box_end){
			bi++;
			bp = box_end;
		}

		/* Advance style cursor when the segment is fully consumed. */
		if(f->nstyles > 0 && f->styles != nil && p >= seg_end && si < f->nstyles){
			sp = seg_end;
			si++;
			sidx = (si < f->nstyles) ? (int)f->styles[si*2] : 0;
			sc   = stylecols_for(f, sidx);
		}
	}

	/*
	 * Trailing fill: if the last box drawn was a complete text box and the
	 * NEXT box starts on a new line, fill the gap to the right margin.
	 * Same rule as the line-wrap fill above: empty space uses base colours.
	 */
	if(!trimmed && p > p0 && bi > 0 && bi < f->nbox && f->box[bi-1].nrune > 0){
		qt = pt;
		_frcklinewrap(f, &pt, &f->box[bi]);
		if(pt.y > qt.y)
			draw(f->b, Rect(qt.x, qt.y, f->r.max.x, pt.y),
			     issel ? f->cols[HIGH] : f->cols[BACK], nil, qt);
	}

	return pt;
}

/*
 * frdrawrange — draw [p0, p1) honouring per-character styles and the current
 * selection (f->p0, f->p1).  pt must be the screen point for p0.
 * Returns the screen point after p1.
 */
Point
frdrawrange(Frame *f, Point pt, ulong p0, ulong p1)
{
	return drawrange_impl(f, pt, p0, p1, 0, 0);
}

/*
 * frdrawsel — draw [p0, p1) with selection state forced to issel for the
 * entire range (used for mouse-drag selection highlighting).  Styles still
 * apply; only the selection component is overridden.
 */
void
frdrawsel(Frame *f, Point pt, ulong p0, ulong p1, int issel)
{
	if(f->ticked)
		frtick(f, frptofchar(f, f->p0), 0);
	if(p0 == p1){
		frtick(f, pt, issel);
		return;
	}
	drawrange_impl(f, pt, p0, p1, 1, issel);
}

/*
 * frdrawsel0 — draw [p0, p1) with explicit back/text colours, ignoring both
 * styles and selection state.  Used by acme's xselect() for mouse-drag
 * highlights that must use a caller-supplied colour regardless of style.
 */
Point
frdrawsel0(Frame *f, Point pt, ulong p0, ulong p1, Image *back, Image *text)
{
	Frbox *b;
	int    nb, nr, w, x, trim;
	Point  qt;
	uint   p;
	char  *ptr;

	if(p0 > p1)
		sysfatal("libframe: frdrawsel0 p0=%lud > p1=%lud", p0, p1);

	p    = 0;
	b    = f->box;
	trim = 0;
	for(nb = 0; nb < f->nbox && p < p1; nb++){
		nr = b->nrune;
		if(nr < 0)
			nr = 1;
		if(p + (uint)nr <= p0)
			goto Continue;
		if(p >= p0){
			qt = pt;
			_frcklinewrap(f, &pt, b);
			if(pt.y > qt.y)
				draw(f->b, Rect(qt.x, qt.y, f->r.max.x, pt.y), back, nil, qt);
		}
		ptr = (char*)b->ptr;
		if(p < p0){
			ptr += nbytes(ptr, p0 - p);
			nr  -= (p0 - p);
			p    = p0;
		}
		trim = 0;
		if(p + (uint)nr > p1){
			nr  -= (p + nr) - p1;
			trim = 1;
		}
		if(b->nrune < 0 || nr == b->nrune) w = b->wid;
		else                               w = stringnwidth(f->font, ptr, nr);
		x = pt.x + w;
		if(x > f->r.max.x) x = f->r.max.x;
		draw(f->b, Rect(pt.x, pt.y, x, pt.y + f->font->height), back, nil, pt);
		if(b->nrune >= 0)
			stringnbg(f->b, pt, text, ZP, f->font, ptr, nr, back, ZP);
		pt.x += w;
	    Continue:
		b++;
		p += nr;
	}
	if(p1 > p0 && b > f->box && b < f->box + f->nbox && b[-1].nrune > 0 && !trim){
		qt = pt;
		_frcklinewrap(f, &pt, b);
		if(pt.y > qt.y)
			draw(f->b, Rect(qt.x, qt.y, f->r.max.x, pt.y), back, nil, qt);
	}
	return pt;
}

void
frredraw(Frame *f)
{
	int   ticked;
	Point pt;

	ticked = f->ticked;
	if(ticked)
		frtick(f, frptofchar(f, f->p0), 0);

	pt = frptofchar(f, 0);
	frdrawrange(f, pt, 0, f->nchars);

	if(ticked)
		frtick(f, frptofchar(f, f->p0), 1);
	else if(f->p0 == f->p1)
		frtick(f, frptofchar(f, f->p0), 1);
}

static void
_frtick(Frame *f, Point pt, int ticked)
{
	Rectangle r;

	if(f->ticked == ticked || f->tick == 0 || !ptinrect(pt, f->r))
		return;
	pt.x -= f->tickscale;
	r = Rect(pt.x, pt.y, pt.x + FRTICKW*f->tickscale, pt.y + f->font->height);
	if(r.max.x > f->r.max.x)
		r.max.x = f->r.max.x;
	if(ticked){
		draw(f->tickback, f->tickback->r, f->b, nil, pt);
		draw(f->b, r, f->tick, nil, ZP);
	}else
		draw(f->b, r, f->tickback, nil, ZP);
	f->ticked = ticked;
}

void
frtick(Frame *f, Point pt, int ticked)
{
	if(f->tickscale != scalesize(f->display, 1)){
		if(f->ticked)
			_frtick(f, pt, 0);
		frinittick(f);
	}
	_frtick(f, pt, ticked);
}

Point
_frdraw(Frame *f, Point pt)
{
	Frbox *b;
	int    nb, n;

	for(b = f->box, nb = 0; nb < f->nbox; nb++, b++){
		_frcklinewrap0(f, &pt, b);
		if(pt.y == f->r.max.y){
			f->nchars -= _frstrlen(f, nb);
			_frdelbox(f, nb, f->nbox-1);
			break;
		}
		if(b->nrune > 0){
			n = _frcanfit(f, pt, b);
			if(n == 0)
				break;
			if(n != b->nrune){
				_frsplitbox(f, nb, n);
				b = &f->box[nb];
			}
			pt.x += b->wid;
		}else{
			if(b->bc == '\n'){
				pt.x  = f->r.min.x;
				pt.y += f->font->height;
			}else
				pt.x += _frnewwid(f, pt, b);
		}
	}
	return pt;
}

int
_frstrlen(Frame *f, int nb)
{
	int n;

	for(n = 0; nb < f->nbox; nb++)
		n += NRUNE(&f->box[nb]);
	return n;
}
