/*
 * ../plan9/_buf.c for wasm: a buffered fd (select's, a non-blocking read's) is read by a copy
 * process, as there, but the processes share no segment: a process's memory on wasm is its own
 * (a fork copies it), and segattach's "shared" is not to be had. So what plan9's copy process
 * put in the shared buffer it writes, as a message, to one pipe the program reads (mux): a
 * header, Hdr, then the data. A pipe's write is one message, whole (pipe(3)), so copy
 * processes' messages do not mix. Having written one, a copy process waits for a byte on a pipe
 * of its own (ack) before it reads its fd again, as plan9's waits for room: the program has one
 * read's data of each fd at most. The timer process writes a message when select's time is up;
 * a select asks it for one by a note, "timer id ms", which interrupts the sleep it is in.
 */
#define _BSDTIME_EXTENSION
#define _BSD_EXTENSION	/* select, which plan9's mkfile gives */
#include "../plan9/lib.h"
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/select.h>
#include "../plan9/sys9.h"
#include "../plan9/dir.h"

typedef struct Hdr Hdr;
struct Hdr {
	int	fd;	/* the buffered fd's, or Timeout */
	int	n;	/* bytes of data that follow; 0 or less is the fd's end */
	int	id;	/* the copy process's (Muxbuf's), or the timer's request's */
};

enum {
	Timeout = -2,
};

/* _muxsid and _killmuxsid are known in libbsd's listen.c */
int _muxsid = -1;			/* group id of copy processes */
static int _mainpid = -1;
static int mux[2] = {-1, -1};		/* the copy processes' messages: they write [1], the program reads [0] */
static int ack[OPEN_MAX];		/* an fd's copy process's go-ahead, written by the program */
static int bufid[OPEN_MAX];		/* an fd's copy process's id, in its messages */
static int nextid;
static int timerpid = -1;		/* the timer process */
static int timernote = -1;		/* its /proc/n/note */
static int timerid;			/* the latest request's */

void _killmuxsid(void);
static void _copyproc(int, Muxbuf*);

/* A copy process's start (_RFORKFN's child): its fd, its buffer and its ack pipe */
typedef struct Copyarg Copyarg;
struct Copyarg {
	int	fd;
	Muxbuf	*b;
	int	a[2];
};
static void copystart(void*);
static int copynotehandler(void *, char *);

/* assume FD_SETSIZE is 96 */
#define FD_ANYSET(p)	((p)->fds_bits[0] || (p)->fds_bits[1] || (p)->fds_bits[2])

/* rawfds closes, in a new copy or timer process, the program's pipes it has no use for */
static void
rawfds(int keep)
{
	int i;

	for(i = 0; i < OPEN_MAX; i++)
		if(ack[i] >= 0 && ack[i] != keep){
			_CLOSE(ack[i]);
			ack[i] = -1;
		}
	_CLOSE(mux[0]);
	if(timernote >= 0)
		_CLOSE(timernote);
	timernote = -1;
}

/*
 * _bufpipe says whether fd is one of the program's pipes, whose number dup must not take, and
 * _bufmove gives that pipe another number, so that dup2 can take fd
 */
static int*
bufref(int fd)
{
	int i;

	if(fd < 0 || mux[0] < 0)
		return 0;
	if(fd == mux[0])
		return &mux[0];
	if(fd == mux[1])
		return &mux[1];
	if(fd == timernote)
		return &timernote;
	for(i = 0; i < OPEN_MAX; i++)
		if(ack[i] == fd)
			return &ack[i];
	return 0;
}

int
_bufpipe(int fd)
{
	return bufref(fd) != 0;
}

int
_bufmove(int fd)
{
	int *r, n;

	if((r = bufref(fd)) == 0)
		return 0;
	if((n = _DUP(fd, -1)) < 0){
		_syserrno();
		return -1;
	}
	*r = n;
	_CLOSE(fd);
	return 0;
}

/*
 * Start making fd read-buffered: make the pipe, if necessary, and fork a child to read the fd
 * and write what it reads into it. Return -1 if we can't do it.
 */
int
_startbuf(int fd)
{
	int i, pid, a[2];
	Fdinfo *f;
	Muxbuf *b;
	Copyarg c;

	if(mux[0] < 0){
		_RFORK(RFREND);
		for(i = 0; i < OPEN_MAX; i++)
			ack[i] = -1;
		if(_PIPE(mux) < 0){
			_syserrno();
			return -1;
		}
		atexit(_killmuxsid);
	}

	if(fd == -1)
		return 0;

	if((b = malloc(sizeof(Muxbuf))) == 0){
		errno = ENOMEM;
		return -1;
	}
	if(_PIPE(a) < 0){
		_syserrno();
		free(b);
		return -1;
	}
	f = &_fdinfo[fd];
	b->n = 0;
	b->putnext = b->data;
	b->getnext = b->data;
	b->eof = 0;
	b->fd = fd;
	bufid[fd] = ++nextid;
	if(_mainpid == -1)
		_mainpid = getpid();
	c.fd = fd;
	c.b = b;
	c.a[0] = a[0];
	c.a[1] = a[1];
	pid = _RFORKFN(RFFDG|RFNOWAIT, copystart, &c);

	/* parent process continues ... */
	_CLOSE(a[0]);
	if(pid < 0){
		_syserrno();
		_CLOSE(a[1]);
		free(b);
		return -1;
	}
	ack[fd] = a[1];
	b->copypid = pid;
	f->buf = b;
	f->flags |= FD_BUFFERED;
	_muxsid = _RENDEZVOUS(0, 0);
	/* leave fd open in parent so system doesn't reuse it */
	return 0;
}

/*
 * The given buffered fd is being closed: its copy process goes, with the pipe it waits on.
 */
void
_closebuf(int fd)
{
	Muxbuf *b;

	b = _fdinfo[fd].buf;
	if(!b)
		return;
	_fdinfo[fd].buf = 0;
	bufid[fd] = 0;
	if(ack[fd] >= 0){
		_CLOSE(ack[fd]);
		ack[fd] = -1;
	}
	kill(b->copypid, SIGKILL);
	free(b);
}

/* a copy process starts here */
static void
copystart(void *v)
{
	Copyarg *c;
	int i;

	c = v;
	_grouppid = 0;
	if(_muxsid == -1) {
		_RFORK(RFNOTEG);
		_muxsid = getpgrp();
	} else
		setpgid(getpid(), _muxsid);
	_NOTIFY(copynotehandler);
	for(i=0; i<OPEN_MAX; i++)
		if(i!=c->fd && (_fdinfo[i].flags&FD_ISOPEN))
			_CLOSE(i);
	_CLOSE(c->a[1]);
	ack[c->fd] = c->a[0];
	rawfds(c->a[0]);
	_RENDEZVOUS(0, _muxsid);
	_copyproc(c->fd, c->b);
}

/* child copy procs execute this until eof */
static void
_copyproc(int fd, Muxbuf *b)
{
	unsigned char m[sizeof(Hdr)+READMAX], c;
	Hdr h;
	int n, nzeros;

	h.fd = fd;
	h.id = bufid[fd];
	for(;;) {
		/*
		 * A Zero-length _READ might mean a zero-length write
		 * happened, or it might mean eof; try several times to
		 * disambiguate (posix read() discards 0-length messages)
		 */
		nzeros = 0;
		do {
			n = _READ(fd, m+sizeof h, READMAX);
		} while(n == 0 && ++nzeros < 3);
		h.n = n;
		memcpy(m, &h, sizeof h);
		if(_WRITE(mux[1], m, sizeof h + (n > 0? n: 0)) < 0 || n <= 0)
			_exit(0);
		/* wait until the program has taken it */
		if(_READ(ack[fd], &c, 1) != 1)
			_exit(0);		/* we've been closed */
	}
}

/*
 * take reads a message from the copy processes or the timer: the fd whose data it was, which is
 * then in its buffer, or Timeout and the timer's request id in *id; -1 if the read was
 * interrupted, or another fd's, whose buffer is gone, and 0 for an fd, which the caller ignores.
 */
static int
take(int *id)
{
	Hdr h;
	Muxbuf *b;
	int n, k;

	for(k = 0; k < sizeof h; k += n)
		if((n = _READ(mux[0], (char*)&h+k, sizeof h-k)) <= 0){
			if(k == 0){
				_syserrno();
				return -1;
			}
			n = 0;	/* a message is not cut short: its rest is coming */
		}
	*id = h.id;
	if(h.fd == Timeout)
		return Timeout;
	b = 0;
	if(h.fd >= 0 && h.fd < OPEN_MAX && bufid[h.fd] == h.id)
		b = _fdinfo[h.fd].buf;
	for(k = 0; k < h.n; k += n){
		static unsigned char junk[READMAX];	/* a closed fd's */

		if((n = _READ(mux[0], b? b->data+k: junk, h.n-k)) <= 0)
			n = 0;
	}
	if(b == 0)
		return 0;
	b->getnext = b->data;
	b->putnext = b->data + (h.n > 0? h.n: 0);
	b->n = h.n > 0? h.n: 0;
	if(h.n <= 0)
		b->eof = 1;
	return h.fd;
}

/* like read(), for a buffered fd; extra arg noblock says don't wait for data if true */
int
_readbuf(int fd, void *addr, int nwant, int noblock)
{
	Muxbuf *b;
	int ngot, id;

	b = _fdinfo[fd].buf;
	if(b->eof && b->n == 0)
		return 0;
	if(b->n == 0 && noblock) {
		errno = EAGAIN;
		return -1;
	}
	/* make sure there's data */
	while(b->n == 0 && !b->eof)
		if(take(&id) == -1)
			return -1;
	if(b->n == 0)
		return 0;
	ngot = b->n;
	if(ngot > nwant)
		ngot = nwant;
	memcpy(addr, b->getnext, ngot);
	b->getnext += ngot;
	b->n -= ngot;
	/* all taken: the copy process may read again */
	if(b->n == 0 && !b->eof && _WRITE(ack[fd], "", 1) != 1)
		b->eof = 1;
	return ngot;
}

int
_peekbuf(int fd, void *addr, int nwant, int noblock)
{
	Muxbuf *b;
	int ngot, id;

	b = _fdinfo[fd].buf;
	if(b->n == 0 && !b->eof && noblock) {
		errno = EAGAIN;
		return -1;
	}
	while(b->n == 0 && !b->eof)
		if(take(&id) == -1)
			return -1;
	ngot = b->n;
	if(ngot > nwant)
		ngot = nwant;
	memcpy(addr, b->getnext, ngot);
	return ngot;
}

/* the timer's request, which its note handler sets */
static struct {
	int	have;
	int	id;
	long	ms;
} timerreq;

static int
timerhandler(void *, char *s)
{
	char *e;

	if(strncmp(s, "timer ", 6) != 0)
		return _NOTED(1);	/* not ours: as a copy process's note */
	timerreq.id = strtol(s+6, &e, 10);
	timerreq.ms = strtol(e, 0, 10);
	timerreq.have = 1;
	return _NOTED(0);
}

/*
 * the timer process: a request's time, then its message to the program, unless another came.
 * A note is taken at a call only, so one that comes before _SLEEP fails it at once (as Plan 9's
 * sleep does, with a note pending): a request is never slept through.
 */
static void
_timerproc(void)
{
	Hdr h;
	long ms;

	for(;;){
		if(!timerreq.have){
			_SLEEP(4000001);	/* a little over an hour */
			continue;
		}
		timerreq.have = 0;
		h.id = timerreq.id;
		ms = timerreq.ms;
		if(_SLEEP(ms) < 0 && timerreq.have)
			continue;	/* another request */
		h.fd = Timeout;
		h.n = 0;
		if(_WRITE(mux[1], &h, sizeof h) < 0)
			_exit(0);
	}
}

static void
_killtimerproc(void)
{
	if(timerpid > 0)
		kill(timerpid, SIGKILL);
}

/* the timer process starts here */
static void
timerstart(void *v)
{
	int i;

	(void)v;
	_grouppid = 0;
	setpgid(getpid(), _muxsid);
	_NOTIFY(timerhandler);
	for(i=0; i<OPEN_MAX; i++)
		if(_fdinfo[i].flags&FD_ISOPEN)
			_CLOSE(i);
	rawfds(-1);
	_RENDEZVOUS(1, 0);	/* its handler is in place: it can take a note */
	_timerproc();
}

/* settimer asks the timer process for a message in t ms; its request id is timerid */
static int
settimer(int t)
{
	char buf[64];
	int i, n;

	if(timerpid == -1){
		timerpid = _RFORKFN(RFFDG|RFNOWAIT, timerstart, NULL);
		if(timerpid < 0){
			_syserrno();
			return -1;
		}
		_RENDEZVOUS(1, 0);
		atexit(_killtimerproc);
	}
	if(timernote < 0){
		n = snprintf(buf, sizeof buf, "/proc/%d/note", timerpid);
		if(n <= 0 || (timernote = _OPEN(buf, 1)) < 0){
			_syserrno();
			return -1;
		}
	}
	n = snprintf(buf, sizeof buf, "timer %d %d", ++timerid, t);
	if(_WRITE(timernote, buf, n) != n){
		_syserrno();
		return -1;
	}
	return 0;
}

static long
waiting(int fd)
{
	Dir *d;
	long n;

	if((d = _dirfstat(fd)) == nil)
		return 0;
	n = d->length;
	free(d);
	return n;
}

static void
takeall(void)
{
	int id;

	while(waiting(mux[0]) > 0)
		if(take(&id) == -1)
			break;
}

int
select(int nfds, fd_set *rfds, fd_set *wfds, fd_set *efds, struct timeval *timeout)
{
	int n, i, t, fd, id;
	fd_set rwant, ewant;
	Muxbuf *b;

	if(timeout)
		t = timeout->tv_sec*1000 + (timeout->tv_usec+999)/1000;
	else
		t = -1;
	if(!((rfds && FD_ANYSET(rfds)) || (wfds && FD_ANYSET(wfds))
			|| (efds && FD_ANYSET(efds)))) {
		/* no requested fds */
		if(t > 0)
			_SLEEP(t);
		return 0;
	}

	if(_startbuf(-1) != 0)
		return -1;

	/* make sure all requested rfds and efds are buffered */
	if(nfds >= OPEN_MAX)
		nfds = OPEN_MAX;
	for(i = 0; i < nfds; i++)
		if((rfds && FD_ISSET(i, rfds)) || (efds && FD_ISSET(i, efds)))
			if(!(_fdinfo[i].flags&FD_BUFFERED))
				if(_startbuf(i) != 0)
					return -1;

	/* check wfds;  for now, we'll say they are all ready */
	n = 0;
	if(wfds && FD_ANYSET(wfds)){
		for(i = 0; i<nfds; i++)
			if(FD_ISSET(i, wfds))
				n++;
	}

	takeall();
	FD_ZERO(&rwant);
	FD_ZERO(&ewant);
	for(fd = 0; fd < nfds; fd++) {
		if((b = _fdinfo[fd].buf) == 0 || !(_fdinfo[fd].flags&FD_BUFFERED))
			continue;
		if(efds && FD_ISSET(fd, efds)) {
			if(b->eof && b->n == 0)
				n++;
			else{
				FD_CLR(fd, efds);
				FD_SET(fd, &ewant);
			}
		}
		if(rfds && FD_ISSET(fd, rfds)) {
			if(b->n > 0 || b->eof)
				n++;
			else{
				FD_CLR(fd, rfds);
				FD_SET(fd, &rwant);
			}
		}
	}
	if(n || !(FD_ANYSET(&rwant) || FD_ANYSET(&ewant)) || t == 0)
		return n;

	if(timeout && settimer(t) < 0)
		return -1;
	for(;;){
		fd = take(&id);
		if(fd == -1)
			return -1;
		if(fd == Timeout){
			if(id == timerid && timeout)
				return 0;
			continue;	/* an earlier select's */
		}
		b = _fdinfo[fd].buf;
		if(b == 0)
			continue;
		if(FD_ISSET(fd, &rwant)) {
			FD_SET(fd, rfds);
			return 1;
		}
		if(FD_ISSET(fd, &ewant) && b->eof && b->n == 0) {
			FD_SET(fd, efds);
			return 1;
		}
	}
}

void
_killmuxsid(void)
{
	if(_muxsid != -1 && (_mainpid == getpid() || _mainpid == -1))
		kill(-_muxsid,SIGTERM);
}

/* call this on fork(), because reading a BUFFERED fd won't work in child */
void
_detachbuf(void)
{
	int i;
	Fdinfo *f;

	if(mux[0] < 0)
		return;
	for(i = 0; i < OPEN_MAX; i++){
		f = &_fdinfo[i];
		if(f->flags&FD_BUFFERED)
			f->flags = (f->flags&~FD_BUFFERED) | FD_BUFFEREDX;
				/* mark 'poisoned' */
	}
	rawfds(-1);
	_CLOSE(mux[1]);
	mux[0] = mux[1] = -1;
	_muxsid = -1;
	_mainpid = -1;
	timerpid = -1;
}

static int
copynotehandler(void *, char *)
{
	if(_finishing)
		_finish(0, 0);
	_NOTED(1);
	return 0;
}
