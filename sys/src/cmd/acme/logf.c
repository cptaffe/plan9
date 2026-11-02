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

// State for global log file.
typedef struct Log Log;
struct Log
{
	QLock lk;
	Rendez r;

	vlong start; // msg[0] corresponds to 'start' in the global sequence of events

	// queued events (nev=entries in ev, mev=capacity of p)
	char **ev;
	int nev;
	int mev;

	// open acme/put files that need to read events
	Fid **f;
	int nf;
	int mf;

	// active (blocked) reads waiting for events
	Xfid **read;
	int nread;
	int mread;
};

static Log eventlog;

void
xfidlogopen(Xfid *x)
{
	qlock(&eventlog.lk);
	if(eventlog.nf >= eventlog.mf) {
		eventlog.mf = eventlog.mf*2;
		if(eventlog.mf == 0)
			eventlog.mf = 8;
		eventlog.f = erealloc(eventlog.f, eventlog.mf*sizeof eventlog.f[0]);
	}
	eventlog.f[eventlog.nf++] = x->f;
	x->f->logoff = eventlog.start + eventlog.nev;

	qunlock(&eventlog.lk);
}

void
xfidlogclose(Xfid *x)
{
	int i;

	qlock(&eventlog.lk);
	for(i=0; i<eventlog.nf; i++) {
		if(eventlog.f[i] == x->f) {
			eventlog.f[i] = eventlog.f[--eventlog.nf];
			break;
		}
	}
	qunlock(&eventlog.lk);
}

void
xfidlogread(Xfid *x)
{
	char *p;
	int i;
	Fcall fc;

	qlock(&eventlog.lk);
	if(eventlog.nread >= eventlog.mread) {
		eventlog.mread = eventlog.mread*2;
		if(eventlog.mread == 0)
			eventlog.mread = 8;
		eventlog.read = erealloc(eventlog.read, eventlog.mread*sizeof eventlog.read[0]);
	}
	eventlog.read[eventlog.nread++] = x;

	if(eventlog.r.l == nil)
		eventlog.r.l = &eventlog.lk;
	x->flushed = FALSE;
	while(x->f->logoff >= eventlog.start+eventlog.nev && !x->flushed)
		rsleep(&eventlog.r);

	for(i=0; i<eventlog.nread; i++) {
		if(eventlog.read[i] == x) {
			eventlog.read[i] = eventlog.read[--eventlog.nread];
			break;
		}
	}

	if(x->flushed) {
		qunlock(&eventlog.lk);
		return;
	}

	i = x->f->logoff - eventlog.start;
	if(i < 0 || i >= eventlog.nev){
		/*
		 * Our position was GC'd out from under us — the FID was
		 * closed (xfidlogclose) while we were sleeping, removing it
		 * from eventlog.f so the GC could advance eventlog.start past
		 * x->f->logoff.  Return empty (EOF) so the client re-opens.
		 */
		qunlock(&eventlog.lk);
		respond(x, &fc, nil);
		return;
	}
	p = estrdup(eventlog.ev[i]);
	x->f->logoff++;
	qunlock(&eventlog.lk);

	fc.data = p;
	fc.count = strlen(p);
	respond(x, &fc, nil);
	free(p);
}

void
xfidlogflush(Xfid *x)
{
	int i;
	Xfid *rx;

	qlock(&eventlog.lk);
	for(i=0; i<eventlog.nread; i++) {
		rx = eventlog.read[i];
		if(rx->fcall.tag == x->fcall.oldtag) {
			rx->flushed = TRUE;
			rwakeupall(&eventlog.r);
		}
	}
	qunlock(&eventlog.lk);
}

/*
 * add a log entry for op on w.
 * expected calls:
 *
 * op == "new" for each new window
 *	- caller of coladd or makenewwindow responsible for calling
 *		xfidlog after setting window name
 *	- exception: zerox
 *
 * op == "zerox" for new window created via zerox
 *	- called from zeroxx
 *
 * op == "get" for Get executed on window
 *	- called from get
 *
 * op == "put" for Put executed on window
 *	- called from put
 *
 * op == "del" for deleted window
 *	- called from winclose
 */
void
xfidlog(Window *w, char *op)
{
	int i, n;
	vlong min;
	File *f;
	char *name;

	qlock(&eventlog.lk);
	if(eventlog.nev >= eventlog.mev) {
		// Remove and free any entries that all readers have read.
		min = eventlog.start + eventlog.nev;
		for(i=0; i<eventlog.nf; i++) {
			if(min > eventlog.f[i]->logoff)
				min = eventlog.f[i]->logoff;
		}
		if(min > eventlog.start) {
			n = min - eventlog.start;
			for(i=0; i<n; i++)
				free(eventlog.ev[i]);
			eventlog.nev -= n;
			eventlog.start += n;
			memmove(eventlog.ev, eventlog.ev+n, eventlog.nev*sizeof eventlog.ev[0]);
		}

		// Otherwise grow.
		if(eventlog.nev >= eventlog.mev) {
			eventlog.mev = eventlog.mev*2;
			if(eventlog.mev == 0)
				eventlog.mev = 8;
			eventlog.ev = erealloc(eventlog.ev, eventlog.mev*sizeof eventlog.ev[0]);
		}
	}
	f = w->body.file;
	name = runetobyte(f->name, f->nname);
	if(name == nil)
		name = estrdup("");
	eventlog.ev[eventlog.nev++] = smprint("%d %s %s\n", w->id, op, name);
	free(name);
	if(eventlog.r.l == nil)
		eventlog.r.l = &eventlog.lk;
	rwakeupall(&eventlog.r);
	qunlock(&eventlog.lk);
}

/* ------------------------------------------------------------------ */
/* Per-window body-edit notification log (<winid>/log).                */
/* Readers block; no write-back protocol.                              */
/* ------------------------------------------------------------------ */

/*
 * xfidwinlogopen - register a fid as a reader of this window's edit log.
 * Called with the window locked.
 */
void
xfidwinlogopen(Xfid *x, Window *w)
{
	WinEditLog *l = &w->editlog;
	int wantseq = x->f->seq;	/* moved onto this fid by xfidopen */

	qlock(&l->lk);
	if(!l->closed){
		if(l->nf >= l->mf){
			l->mf = l->mf ? l->mf*2 : 8;
			l->f = erealloc(l->f, l->mf * sizeof l->f[0]);
		}
		l->f[l->nf++] = x->f;
		if(wantseq >= 0){
			if((vlong)wantseq < l->start || wantseq > w->seq)
				x->f->logmismatch = 1;
			else
				x->f->logoff = wantseq;
		} else
			x->f->logoff = l->start + l->nev;
	}
	qunlock(&l->lk);
}

/*
 * xfidwinlogclose - deregister a fid.  Called with the window locked.
 */
void
xfidwinlogclose(Xfid *x, Window *w)
{
	int i;
	WinEditLog *l = &w->editlog;

	qlock(&l->lk);
	if(!l->closed){
		for(i = 0; i < l->nf; i++){
			if(l->f[i] == x->f){
				l->f[i] = l->f[--l->nf];
				/*
				 * Wake any read blocked on this fid so it can
				 * detect the deregistration and return EOF
				 * promptly, rather than hanging until the next
				 * edit event or window close.
				 */
				if(l->r.l == nil)
					l->r.l = &l->lk;
				rwakeupall(&l->r);
				break;
			}
		}
	}
	qunlock(&l->lk);
}

/*
 * xfidwinlogread - blocking read.
 *
 * Called by xfidread with the window locked.  We release the window
 * lock before blocking (same pattern as xfideventread) and re-acquire
 * it before returning so the caller's winunlock() is correctly paired.
 *
 * Lock ordering: the window lock is always acquired before l->lk
 * (matching textinsert/textdelete -> winlogedit).  Therefore we must
 * release the window lock BEFORE taking l->lk and re-acquire it AFTER
 * releasing l->lk to avoid an inversion deadlock.
 */
void
xfidwinlogread(Xfid *x, Window *w)
{
	Fcall fc;
	int i, n;
	WinEditLog *l = &w->editlog;
	char *p;
	vlong min;

	memset(&fc, 0, sizeof fc);

	/* First read: error if seq guard was set but position was unavailable. */
	if(x->f->logmismatch){
		x->f->logmismatch = 0;
		respond(x, &fc, "seq mismatch");
		return;
	}

	/*
	 * Release the window lock before any potentially blocking
	 * operation.  Re-acquire it at every return path.
	 *
	 * Explicitly bump the reference count across the winunlock/winlock
	 * gap so that winclose cannot free w->body.file (via textclose) in
	 * the window between us releasing the lock and re-acquiring it.
	 * Without this, a concurrent winclose reaching decref==0 could null
	 * w->body.file before winlock reads it, crashing in winlock's
	 * "f = w->body.file; for(i=0; i<f->ntext; ...)".
	 * The matching winclose() is called at every re-acquire site below.
	 */
	incref(&w->ref);
	winunlock(w);

	qlock(&l->lk);
	if(l->r.l == nil)
		l->r.l = &l->lk;

	/* Return EOF immediately if the window is already gone. */
	if(l->closed){
		qunlock(&l->lk);
		winlock(w, 'F');
		winclose(w);	/* release the extra ref taken before winunlock */
		respond(x, &fc, nil);
		return;
	}

	if(l->nread >= l->mread){
		l->mread = l->mread ? l->mread*2 : 8;
		l->read = erealloc(l->read, l->mread * sizeof l->read[0]);
	}
	l->read[l->nread++] = x;

	x->flushed = FALSE;
	while(x->f->logoff >= l->start + l->nev && !x->flushed && !l->closed)
		rsleep(&l->r);

	/*
	 * winlogfree sets closed=1 and frees l->read under the same
	 * lock hold, so we must not touch l->read after seeing closed=1.
	 */
	if(!l->closed){
		for(i = 0; i < l->nread; i++){
			if(l->read[i] == x){
				l->read[i] = l->read[--l->nread];
				break;
			}
		}
	}

	if(x->flushed || l->closed){
		qunlock(&l->lk);
		winlock(w, 'F');
		winclose(w);	/* release the extra ref taken before winunlock */
		/*
		 * flush(9p): if the server has not responded to the request
		 * being flushed before sending Rflush, it must not do so
		 * afterward.  Only send EOF when the window was closed but
		 * the read was not flushed; in the flushed case Rflush has
		 * already been sent, so sending Rread here would violate the
		 * protocol and corrupt 9pserve's tag recycling.
		 */
		if(l->closed && !x->flushed)
			respond(x, &fc, nil);  /* 0 bytes = EOF */
		return;
	}

	i = x->f->logoff - l->start;
	if(i < 0 || i >= l->nev){
		/*
		 * Our position was GC'd out from under us: a concurrent
		 * xfidwinlogclose deregistered this fid from l->f, the GC
		 * advanced l->start past our logoff, and then we woke up.
		 * Respond with EOF so the client knows to reopen.
		 */
		qunlock(&l->lk);
		winlock(w, 'F');
		winclose(w);
		respond(x, &fc, nil);
		return;
	}
	p = estrdup(l->ev[i]);
	x->f->logoff++;

	/* GC: remove entries every open reader has consumed. */
	min = l->start + l->nev;
	for(i = 0; i < l->nf; i++)
		if(l->f[i]->logoff < min)
			min = l->f[i]->logoff;
	if(min > l->start){
		n = (int)(min - l->start);
		for(i = 0; i < n; i++)
			free(l->ev[i]);
		l->nev -= n;
		l->start += n;
		memmove(l->ev, l->ev + n, l->nev * sizeof l->ev[0]);
	}

	qunlock(&l->lk);
	winlock(w, 'F');  /* re-acquire window lock before returning */
	winclose(w);	/* release the extra ref taken before winunlock */

	fc.data = p;
	fc.count = strlen(p);
	respond(x, &fc, nil);
	free(p);
}

/*
 * xfidwinlogflush - cancel a pending read for the given tag.
 * Called from xfidflush with the window locked.
 */
void
xfidwinlogflush(Xfid *x, Window *w)
{
	int i;
	WinEditLog *l = &w->editlog;

	qlock(&l->lk);
	if(!l->closed){
		for(i = 0; i < l->nread; i++){
			if(l->read[i]->fcall.tag == x->fcall.oldtag){
				l->read[i]->flushed = TRUE;
				rwakeupall(&l->r);
				break;
			}
		}
	}
	qunlock(&l->lk);
}

/*
 * winlogedit - append one line to the window's edit log.
 * Called from textinsert/textdelete (tofile=1, Body only).
 *
 * Format matches the acme event file format (sans C1):
 *
 *   op 'I': I q0 q1 0 nr [text]\n
 *     q0     = rune position of insertion
 *     q1     = q0 + rune count
 *     nr     = rune count included in text field (0 if omitted)
 *     text   = nr runes as UTF-8, present only when nr > 0
 *              (nr == 0 when the insertion is larger than EVENTSIZE)
 *
 *   op 'D': D q0 q1 0 0 \n
 *     q0, q1 = [q0, q1) rune range deleted
 *
 * The window lock may or may not be held by the caller; this function
 * only takes l->lk, which is always acquired after the window lock
 * (never before), so there is no inversion.
 *
 * r/nr are the inserted runes (only used for op=='I').  Pass nil/0 for
 * op=='D' (the deleted text is gone by call time).
 */
void
winlogedit(Window *w, char op, ulong q0, ulong q1, Rune *r, int nr)
{
	WinEditLog *l = &w->editlog;
	char *entry;

	qlock(&l->lk);
	if(l->nf == 0 || l->closed){
		qunlock(&l->lk);
		return;
	}
	w->seq++;	/* only advance when a log entry is actually written */
	if(l->nev >= l->mev){
		l->mev = l->mev ? l->mev*2 : 8;
		l->ev = erealloc(l->ev, l->mev * sizeof l->ev[0]);
	}
	/*
	 * Limit inline text to EVENTSIZE runes, matching the event-file convention.
	 * Entries larger than EVENTSIZE would exceed the bufio read-buffer used by
	 * the Go log reader (4096 bytes), causing the 9P server to send more bytes
	 * than the client's Tread.count and corrupting the stream.  When text is
	 * omitted (nr==0 in the log line), the Go client falls back to ReadBody.
	 */
	if(op == 'I' && nr > 0 && nr <= EVENTSIZE && r != nil)
		entry = smprint("%c %lud %lud 0 %d %.*S\n", op, q0, q1, nr, nr, r);
	else
		entry = smprint("%c %lud %lud 0 0 \n", op, q0, q1);
	l->ev[l->nev++] = entry;
	if(l->r.l == nil)
		l->r.l = &l->lk;
	rwakeupall(&l->r);
	qunlock(&l->lk);
}

/*
 * winlogfree - called from winclose.
 *
 * Sets closed=1, wakes every blocked reader, and frees all heap
 * storage under a single lock hold.  Readers that re-acquire l->lk
 * after being woken will see closed=1 and will not touch l->read,
 * l->ev, or l->f, so this is free of use-after-free races.
 */
void
winlogfree(Window *w)
{
	int i;
	WinEditLog *l = &w->editlog;

	qlock(&l->lk);
	l->closed = 1;
	if(l->r.l == nil)
		l->r.l = &l->lk;
	rwakeupall(&l->r);

	/* Free under the lock so woken readers see nil, not stale pointers. */
	for(i = 0; i < l->nev; i++)
		free(l->ev[i]);
	free(l->ev);
	free(l->f);
	free(l->read);
	l->ev   = nil; l->nev = l->mev = 0;
	l->f    = nil; l->nf  = l->mf  = 0;
	l->read = nil; l->nread = l->mread = 0;
	qunlock(&l->lk);
}
