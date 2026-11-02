#include <u.h>
#include <libc.h>
#include <draw.h>
#include <mouse.h>
#include <frame.h>

/*
 * frsetstyles — install a new style table and run-length colour array into a
 * frame.  The caller owns the memory; Frame keeps a fresh copy.
 *
 *   nsc  — number of style colour sets
 *   sc   — flat array of nsc*NCOL Image* pointers (sc[0..NCOL-1] == cols)
 *   ns   — number of run-length style segments
 *   st   — flat array of 2*ns ulongs: {style_index, length, ...}
 *
 * Returns 1 on success, 0 on allocation failure.
 */
int
frsetstyles(Frame *f, int nsc, Image **sc, int ns, ulong *st)
{
	Image **newsc;
	ulong  *newst;

	newsc = nil;
	newst = nil;
	if(nsc > 0 && sc != nil){
		newsc = malloc(nsc * NCOL * sizeof(Image*));
		if(newsc == nil)
			return 0;
		memmove(newsc, sc, nsc * NCOL * sizeof(Image*));
	}
	if(ns > 0 && st != nil){
		newst = malloc(2 * ns * sizeof(ulong));
		if(newst == nil){
			free(newsc);
			return 0;
		}
		memmove(newst, st, 2 * ns * sizeof(ulong));
	}
	free(f->stylecols);
	free(f->styles);
	f->stylecols  = newsc;
	f->nstylecols = nsc;
	f->styles     = newst;
	f->nstyles    = ns;
	frstylesync(f);
	return 1;
}

/*
 * frstylesync — copy style 0's colours back into f->cols so that all
 * existing libframe drawing helpers see the correct default palette.
 */
void
frstylesync(Frame *f)
{
	if(f->nstylecols > 0 && f->stylecols != nil)
		memmove(f->cols, f->stylecols, NCOL * sizeof(Image*));
}

/*
 * frstyleinsert — widen the RLE style run that covers position p0 by n
 * characters.
 *
 * Characters inserted strictly inside a run inherit that run's style.
 * At an exact run boundary the insert falls into the right neighbour —
 * we never extend the left run — so boundaries stay sharp until the next
 * compositor flush.  The last run is extended when p0 is at or past the
 * end of all runs (i.e. appending to the frame).
 */
void
frstyleinsert(Frame *f, ulong p0, ulong n)
{
	ulong pos, segend;
	int i;

	if(f->nstyles == 0 || n == 0)
		return;
	pos = 0;
	for(i = 0; i < f->nstyles; i++){
		segend = pos + f->styles[2*i+1];
		if(p0 < segend){
			/* Interior: extend this run. */
			f->styles[2*i+1] += n;
			return;
		}
		if(p0 == segend && i < f->nstyles-1){
			/* Boundary: fall through to the right neighbour. */
			pos = segend;
			continue;
		}
		pos = segend;
	}
	/* At or past the end of all runs: extend the last run. */
	if(f->nstyles > 0)
		f->styles[2*(f->nstyles-1)+1] += n;
}

/*
 * frstyledelete — remove characters [p0, p1) from the RLE style array,
 * shrinking or eliminating the affected segments.
 */
void
frstyledelete(Frame *f, ulong p0, ulong p1)
{
	ulong pos, del_start, seglen, avail, del, rem;
	int i;

	if(f->nstyles == 0 || p0 >= p1)
		return;
	rem = p1 - p0;
	pos = 0;
	for(i = 0; i < f->nstyles; i++){
		if(pos + f->styles[2*i+1] > p0)
			break;
		pos += f->styles[2*i+1];
	}
	if(i == f->nstyles)
		return;
	del_start = p0 - pos;
	while(i < f->nstyles && rem > 0){
		seglen = f->styles[2*i+1];
		avail  = seglen - del_start;
		del    = rem < avail ? rem : avail;
		f->styles[2*i+1] -= del;
		rem      -= del;
		del_start = 0;
		if(f->styles[2*i+1] == 0){
			memmove(&f->styles[2*i], &f->styles[2*(i+1)],
				2*(f->nstyles-i-1)*sizeof(ulong));
			f->nstyles--;
		} else {
			i++;
		}
	}
}
