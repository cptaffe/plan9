#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>
#include <acme.h>
#include "dat.h"

Channel *fschan;
Channel *writechan;

static File *devcons, *devnew;

static void
fsread(Req *r)
{
	Fsevent e;

	if(r->fid->file == devnew){
		if(r->fid->aux==nil){
			respond(r, "phase error");
			return;
		}
		readstr(r, r->fid->aux);
		respond(r, nil);
		return;
	}

	assert(r->fid->file == devcons);
	e.type = 'r';
	e.r = r;
	send(fschan, &e);
}

static void
fsflush(Req *r)
{
	Fsevent e;

	e.type = 'f';
	e.r = r;
	send(fschan, &e);
}

static void
fswrite(Req *r)
{
	static Msg *e[4];
	static int n, partial;
	Msg *ep;
	int i, j, ei, nb, wid, pid;
	Rune rune;
	char *s;
	char tmp[UTFmax], *t;

	if(r->fid->file == devnew){
		if(r->fid->aux){
			respond(r, "already created a window");
			return;
		}
		s = emalloc(r->ifcall.count+1);
		memmove(s, r->ifcall.data, r->ifcall.count);
		s[r->ifcall.count] = 0;
		pid = strtol(s, &t, 0);
		if(*t==' ')
			t++;
		i = newpipewin(pid, t);
		free(s);
		s = emalloc(32);
		sprint(s, "%lud", (ulong)i);
		r->fid->aux = s;
		r->ofcall.count = r->ifcall.count;
		respond(r, nil);
		return;
	}

	assert(r->fid->file == devcons);

	if(e[0] == nil){
		for(i=0; i<nelem(e); i++){
			e[i] = emalloc(sizeof(Msg));
			e[i]->e.c1 = 'S';
			e[i]->b = emalloc(OUTSIZE*UTFmax+1);
		}
	}

	ep = e[n];
	n = (n+1)%nelem(e);
	/*
	 * lib9p ensures that r->ifcall.count is <= srv->msize - IOHDRSZ,
	 * where srv == &fs.
	 */
	assert((int)r->ifcall.count >= 0);
	nb = r->ifcall.count;
	if (partial + nb >= OUTSIZE*UTFmax+1)
		sysfatal("OUTSIZE < MAXRPC; increase OUTSIZE & recompile");
	memmove(ep->b+partial, r->ifcall.data, nb);
	nb += partial;
	ep->b[nb] = '\0';
	if(strlen(ep->b) < nb){	/* nulls in data */
		t = ep->b;
		for(i=j=0; i<nb; i++)
			if(ep->b[i] != '\0')
				t[j++] = ep->b[i];
		nb = j;
		t[j] = '\0';
	}
	ei = nb>OUTSIZE? OUTSIZE: nb;
	/* process bytes into runes, transferring terminal partial runes into next buffer */
	for(i=j=0; i<ei && fullrune(ep->b+i, ei-i); i+=wid,j++)
		wid = chartorune(&rune, ep->b+i);
	memmove(tmp, ep->b+i, nb-i);
//	partial = nb-i;			/* redundant */
	ep->nb = i;
	ep->nr = j;
	ep->b[i] = '\0';
	if(i != 0){
		sendp(win->cevent, ep);
		recvp(writechan);
	}
	partial = nb-i;
	memmove(e[n]->b, tmp, partial);
	r->ofcall.count = r->ifcall.count;
	respond(r, nil);
}

void
fsdestroyfid(Fid *fid)
{
	if(fid->aux)
		free(fid->aux);
}

Srv fs = {
.read=	fsread,
.write=	fswrite,
.flush=	fsflush,
.destroyfid=	fsdestroyfid,
.leavefdsopen=	1,
};

void
mountcons(void)
{
	fschan = chancreate(sizeof(Fsevent), 0);
	writechan = chancreate(sizeof(void*), 0);
	fs.tree = alloctree("win", "win", DMDIR|0555, nil);
	devcons = createfile(fs.tree->root, "cons", "win", 0666, nil);
	if(devcons == nil)
		sysfatal("creating /dev/cons: %r");
	devnew = createfile(fs.tree->root, "wnew", "win", 0666, nil);
	if(devnew == nil)
		sysfatal("creating /dev/wnew: %r");
	threadpostmountsrv(&fs, nil, "/dev", MBEFORE);
}
