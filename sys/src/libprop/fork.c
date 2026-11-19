#include <u.h>
#include <libc.h>
#include <prop.h>
#include "impl.h"

enum
{
	Died = 3,
	Ended,
};

static Src *kid;
static int kiddone;
static int armed;

int _propstop;

static void
kidend(void)
{
	if(kid == nil || kiddone)
		return;
	kiddone = 1;
	_propput(kid, 'x', nil, 0);
	_propflush(kid);
}

static void
kidnote(void*, char*)
{
	kidend();
	noted(NDFLT);
}

static int
parnote(void*, char *msg)
{
	if(strcmp(msg, "alarm") == 0)
		return armed;
	if(strcmp(msg, "interrupt") == 0 && !_propstop){
		_propstop = 1;
		return 1;
	}
	return 0;
}

void
_propnotify(void)
{
	atnotify(parnote, 1);
}

static void
kidrun(Src *s, Prop *p, void *a, int fd, int all)
{
	char *e, *t;
	uchar *b;
	int k, n;

	kid = s;
	s->child = 1;
	s->ofd = fd;
	s->oall = all;
	s->nob = 0;
	notify(kidnote);
	atexit(kidend);
	e = nil;
	k = _proprun(s, p, a, &e);
	t = k == Fail ? e : nil;
	n = t == nil ? 0 : strlen(t);
	if(n > 0xFFFE)
		n = 0xFFFE;
	b = emalloc(n+3);
	b[0] = k;
	b[1] = n;
	b[2] = n>>8;
	memmove(b+3, t, n);
	_propput(s, 'r', b, n+3);
	kiddone = 1;
	_propflush(s);
	exits(nil);
}

static uvlong
num(uchar *b, int n)
{
	uvlong v;

	v = 0;
	while(n-- > 0)
		v = v<<8 | b[n];
	return v;
}

static int
decode(Src *s, uchar *b, long n, char **err)
{
	uchar *p, *e;
	char *t;
	int k, m;

	k = Died;
	p = b;
	e = b+n;
	while(p < e){
		switch(*p++){
		case 'c':
			if(e-p < 16)
				return k;
			_proprec(s, num(p, 8), num(p+8, 8));
			p += 16;
			break;
		case 's':
			if(e-p < 3)
				return k;
			m = num(p+1, 2);
			if(m == 0xFFFF){
				_propopen(s, nil, p[0]);
				p += 3;
				break;
			}
			if(e-p < 3+m)
				return k;
			_propopen(s, _propintern((char*)p+3, m), p[0]);
			p += 3+m;
			break;
		case 'e':
			endspan(s);
			break;
		case 'n':
			if(e-p < 2 || e-p < 2+(m = num(p, 2)))
				return k;
			t = emalloc(m+1);
			memmove(t, p+2, m);
			_propnote(s, t);
			free(t);
			p += 2+m;
			break;
		case 'r':
			if(e-p < 3 || e-p < 3+(m = num(p+1, 2)))
				return k;
			k = p[0];
			if(k == Fail){
				s->err = emalloc(m+1);
				memmove(s->err, p+3, m);
				*err = s->err;
			}
			p += 3+m;
			break;
		case 'x':
			if(k == Died)
				k = Ended;
			break;
		default:
			return k;
		}
	}
	return k;
}

static void
killpid(int pid)
{
	char *f;
	int fd;

	f = smprint("/proc/%d/ctl", pid);
	fd = open(f, OWRITE);
	free(f);
	if(fd < 0)
		return;
	write(fd, "kill", 4);
	close(fd);
}

static Waitmsg*
waitfor(int pid)
{
	Waitmsg *w;
	char e[ERRMAX];
	int i;

	for(i = 0; i < 100; i++){
		w = wait();
		if(w != nil && w->pid == pid)
			return w;
		if(w != nil){
			free(w);
			continue;
		}
		rerrstr(e, sizeof e);
		if(strstr(e, "interrupted") == nil)
			break;
	}
	return nil;
}

static int
once(Src *s, Prop *p, void *a, char **err, int ms, int all)
{
	Waitmsg *w;
	uchar *b;
	char *m, e[ERRMAX];
	long n, nb, r;
	int fd[2], pid, k, late, tries, full;

	if(pipe(fd) < 0){
		werrstr("pipe: %r");
		return -1;
	}
	switch(pid = fork()){
	case -1:
		werrstr("fork: %r");
		close(fd[0]);
		close(fd[1]);
		return -1;
	case 0:
		close(fd[0]);
		kidrun(s, p, a, fd[1], all);
	}
	close(fd[1]);
	_propreset(s);
	b = nil;
	n = nb = 0;
	late = 0;
	tries = 0;
	if(ms > 0){
		armed = 1;
		alarm(ms);
	}
	for(;;){
		if(n+8192 > nb){
			nb = n+8192 + nb;
			b = erealloc(b, nb);
		}
		r = read(fd[0], b+n, nb-n);
		if(r > 0){
			n += r;
			continue;
		}
		if(r == 0)
			break;
		if(_propstop || (ms > 0 && tries > 0)){
			killpid(pid);
			if(!_propstop)
				late = 1;
			alarm(0);
			continue;
		}
		if(ms > 0 && tries == 0){
			late = 1;
			tries = 1;
			postnote(PNPROC, pid, "timeout");
			alarm(ms);
			continue;
		}
		werrstr("read: %r");
		goto Err;
	}
	alarm(0);
	close(fd[0]);
	if((w = waitfor(pid)) == nil){
		werrstr("wait: %r");
		free(b);
		return -1;
	}
	k = decode(s, b, n, err);
	free(b);
	while(s->nopen > 0)
		endspan(s);
	full = k != Died;
	if(_propstop)
		k = Discard;
	else if(late){
		free(s->err);
		s->err = smprint("timed out after %d ms", ms);
		*err = s->err;
		k = Fail;
	}else if(k == Died || k == Ended){
		m = strstr(w->msg, ": ");
		m = m != nil ? m+2 : w->msg;
		free(s->err);
		s->err = _propdup(*m != 0 ? m : "exits with no status");
		*err = s->err;
		k = Fail;
	}
	free(w);
	if(!full && !all && k == Fail)
		return Died;
	return k;
Err:
	rerrstr(e, sizeof e);
	alarm(0);
	close(fd[0]);
	free(b);
	killpid(pid);
	free(waitfor(pid));
	werrstr("%s", e);
	return -1;
}

int
_propfork(Src *s, Prop *p, void *a, char **err, int ms)
{
	uvlong x;
	int k;

	x = s->x;
	k = once(s, p, a, err, ms, 0);
	if(k == Died){
		s->x = x;
		k = once(s, p, a, err, ms, 1);
	}
	return k;
}
