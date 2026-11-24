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

/* State for global log file. */
typedef struct Log Log;
struct Log
{
	QLock lk;
	Rendez r;

	vlong start; /* msg[0] corresponds to 'start' in the global sequence of events */

	/* queued events (nev=entries in ev, mev=capacity of p) */
	char **ev;
	int nev;
	int mev;

	/* open acme/put files that need to read events */
	Fid **f;
	int nf;
	int mf;

	/* active (blocked) reads waiting for events */
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

	if(strcmp(op, "focus") != 0)
		dumpseq++;
	qlock(&eventlog.lk);
	if(eventlog.nev >= eventlog.mev) {
		/* Remove and free any entries that all readers have read. */
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

		/* Otherwise grow. */
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

static int
isedit(Window *w, WinEditLog *l, char *s)
{
	USED(w);
	USED(l);
	return s[1] == 'I' || s[1] == 'D';
}

static WinEditLog*	logof(Xfid*, Window*);

void
xfidwinlogopen(Xfid *x, Window *w)
{
	WinEditLog *l;
	int wantseq, i;
	ulong lo;

	l = logof(x, w);
	wantseq = x->f->seq;
	x->f->logpart = 0;
	x->f->logmismatch = 0;
	qlock(&l->lk);
	if(!l->closed){
		if(l->nf >= l->mf){
			l->mf = l->mf ? l->mf*2 : 8;
			l->f = erealloc(l->f, l->mf*sizeof l->f[0]);
		}
		l->f[l->nf++] = x->f;
		w->dotage = 0;
		w->sentq0 = w->sentq1 = ~0;
		if(wantseq >= 0){
			for(i=0; i<l->nev && l->evseq[i]<=wantseq; i++)
				;
			lo = l->nev > 0 ? l->evseq[0] - isedit(w, l, l->ev[0]) : w->seq;
			if(wantseq < lo || wantseq > w->seq)
				x->f->logmismatch = 1;
			else
				x->f->logoff = l->start + i;
		}else
			x->f->logoff = l->start + l->nev;
	}
	qunlock(&l->lk);
}

void
xfidwinlogclose(Xfid *x, Window *w)
{
	int i;
	WinEditLog *l;

	l = logof(x, w);
	qlock(&l->lk);
	if(!l->closed){
		for(i=0; i<l->nf; i++){
			if(l->f[i] == x->f){
				l->f[i] = l->f[--l->nf];
				if(l->r.l == nil)
					l->r.l = &l->lk;
				rwakeupall(&l->r);
				break;
			}
		}
	}
	qunlock(&l->lk);
}

void
xfidwinlogread(Xfid *x, Window *w)
{
	Fcall fc;
	int i, n, m;
	WinEditLog *l;
	char *p, *s;
	vlong min;

	l = logof(x, w);
	memset(&fc, 0, sizeof fc);
	if(x->f->logmismatch){
		x->f->logmismatch = 0;
		respond(x, &fc, "seq mismatch");
		return;
	}

	incref(&w->ref);
	winunlock(w);

	qlock(&l->lk);
	if(l->r.l == nil)
		l->r.l = &l->lk;

	if(l->closed){
		qunlock(&l->lk);
		winlock(w, 'F');
		winclose(w);
		respond(x, &fc, nil);
		return;
	}

	if(l->nread >= l->mread){
		l->mread = l->mread ? l->mread*2 : 8;
		l->read = erealloc(l->read, l->mread*sizeof l->read[0]);
	}
	l->read[l->nread++] = x;

	x->flushed = FALSE;
	while(x->f->logoff >= l->start+l->nev && !x->flushed && !l->closed)
		rsleep(&l->r);

	if(!l->closed){
		for(i=0; i<l->nread; i++){
			if(l->read[i] == x){
				l->read[i] = l->read[--l->nread];
				break;
			}
		}
	}

	if(x->flushed || l->closed){
		qunlock(&l->lk);
		winlock(w, 'F');
		winclose(w);
		if(l->closed && !x->flushed)
			respond(x, &fc, nil);
		return;
	}

	i = x->f->logoff - l->start;
	if(i < 0 || i >= l->nev){
		qunlock(&l->lk);
		winlock(w, 'F');
		winclose(w);
		respond(x, &fc, nil);
		return;
	}
	s = l->ev[i] + x->f->logpart;
	m = strlen(s);
	if(m > x->fcall.count){
		m = x->fcall.count;
		x->f->logpart += m;
	}else{
		x->f->logpart = 0;
		x->f->logoff++;
	}
	p = emalloc(m);
	memmove(p, s, m);

	min = l->start + l->nev;
	for(i=0; i<l->nf; i++)
		if(l->f[i]->logoff < min)
			min = l->f[i]->logoff;
	if(min > l->start){
		n = (int)(min - l->start);
		for(i=0; i<n; i++)
			free(l->ev[i]);
		l->nev -= n;
		l->start += n;
		memmove(l->ev, l->ev+n, l->nev*sizeof l->ev[0]);
		memmove(l->evseq, l->evseq+n, l->nev*sizeof l->evseq[0]);
	}

	qunlock(&l->lk);
	winlock(w, 'F');
	winclose(w);

	fc.data = p;
	fc.count = m;
	respond(x, &fc, nil);
	free(p);
}

static void
logflush(Xfid *x, WinEditLog *l)
{
	int i;

	qlock(&l->lk);
	if(!l->closed){
		for(i=0; i<l->nread; i++){
			if(l->read[i]->fcall.tag == x->fcall.oldtag){
				l->read[i]->flushed = TRUE;
				rwakeupall(&l->r);
				break;
			}
		}
	}
	qunlock(&l->lk);
}

void
xfidwinlogflush(Xfid *x, Window *w)
{
	logflush(x, &w->rdlog);
}

static WinEditLog*
logof(Xfid *x, Window *w)
{
	USED(x);
	return &w->rdlog;
}

static void
logappend(Window *w, WinEditLog *l, char *s)
{
	qlock(&l->lk);
	if(l->nf == 0 || l->closed){
		qunlock(&l->lk);
		free(s);
		return;
	}
	if(l->nev >= l->mev){
		l->mev = l->mev ? l->mev*2 : 8;
		l->ev = erealloc(l->ev, l->mev*sizeof l->ev[0]);
		l->evseq = erealloc(l->evseq, l->mev*sizeof l->evseq[0]);
	}
	l->evseq[l->nev] = w->seq;
	l->ev[l->nev++] = s;
	if(l->r.l == nil)
		l->r.l = &l->lk;
	rwakeupall(&l->r);
	qunlock(&l->lk);
}

void
winrdevent(Window *w, int c1, char *s)
{
	logappend(w, &w->rdlog, smprint("%c%s", c1, s));
}

static void
logfree(WinEditLog *l)
{
	int i;

	qlock(&l->lk);
	l->closed = 1;
	if(l->r.l == nil)
		l->r.l = &l->lk;
	rwakeupall(&l->r);
	for(i=0; i<l->nev; i++)
		free(l->ev[i]);
	free(l->ev);
	free(l->f);
	free(l->read);
	free(l->evseq);
	l->evseq = nil;
	l->ev = nil;
	l->nev = l->mev = 0;
	l->f = nil;
	l->nf = l->mf = 0;
	l->read = nil;
	l->nread = l->mread = 0;
	qunlock(&l->lk);
}

void
winlogfree(Window *w)
{
	logfree(&w->rdlog);
}

void
winlogdot(Window *w)
{
	logappend(w, &w->rdlog, smprint("MS%ud %ud 0 0 \n", w->body.q0, w->body.q1));
	w->sentq0 = w->body.q0;
	w->sentq1 = w->body.q1;
}
