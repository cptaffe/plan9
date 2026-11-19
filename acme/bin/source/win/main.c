#include <u.h>
#include <libc.h>
#include <bio.h>
#include <thread.h>
#include <fcall.h>
#include <9p.h>
#include <ctype.h>
#include <acme.h>
#include "dat.h"

void	mainctl(void*);
void	startcmd(char *[], int*);
void	stdout2body(void*);

int	debug;
int	notepg;
int	eraseinput;
int	resume;
int	dirty = 0;

Window *win;		/* the main window */
ulong	resumept;	/* the end of the body win is resumed in */

void
usage(void)
{
	fprint(2, "usage: win [-r] [command]\n");
	threadexitsall("usage");
}

void
threadmain(int argc, char *argv[])
{
	int i, j;
	char *dir, *tag, *name, *id;
	char buf[1024], **av;

	quotefmtinstall();
	rfork(RFNAMEG);
	ARGBEGIN{
	case 'd':
		debug = 1;
		chatty9p++;
		break;
	case 'e':
		eraseinput = 1;
		break;
	case 'r':
		resume = 1;
		break;
	case 'D':
{extern int _threaddebuglevel;
		_threaddebuglevel = 1<<20;
}
	}ARGEND

	if(argc == 0){
		av = emalloc(3*sizeof(char*));
		av[0] = "rc";
		av[1] = "-i";
		name = getenv("sysname");
	}else{
		av = argv;
		name = utfrrune(av[0], '/');
		if(name)
			name++;
		else
			name = av[0];
	}

	if(getwd(buf, sizeof buf) == 0)
		dir = "/";
	else
		dir = buf;
	dir = estrdup(dir);
	tag = estrdup(dir);
	tag = eappend(estrdup(tag), "/-", name);
	id = nil;
	if(resume && (id = getenv("winid")) != nil && atoi(id) > 0)
		win = wold(id, &resumept);
	else{
		resume = 0;
		win = wnew();
	}
	free(id);
	snprint(buf, sizeof buf, "%d", win->id);
	putenv("winid", buf);
	wname(win, tag);
	if(!resume)
		wtag(win, "Send Noscroll");
	strcpy(buf, "win -r");
	j = 6;
	for(i=0; i<argc && j+1+strlen(argv[i])+1<sizeof buf; i++){
		strcpy(buf+j, " ");
		strcpy(buf+j+1, argv[i]);
		j += 1+strlen(argv[i]);
	}

	wdump(win, dir, buf);
	threadcreate(mainctl, win, STACK);
	mountcons();
	threadcreate(fsloop, nil, STACK);
	startpipe();
	startcmd(av, &notepg);
	wctl(win, "scroll");
}

int
EQUAL(char *s, char *t)
{
	while(tolower(*s) == tolower(*t++))
		if(*s++ == '\0')
			return 1;
	return 0;
}

int
command(Window *w, char *s)
{
	while(*s==' ' || *s=='\t' || *s=='\n')
		s++;
	if(strcmp(s, "Delete")==0 || strcmp(s, "Del")==0){
		write(notepg, "hangup", 6);
		wdel(w, 1);
		threadexitsall(nil);
		return 1;
	}
	if(EQUAL(s, "scroll")){
		wctl(w, "scroll\nshow");
		return 1;
	}
	if(EQUAL(s, "noscroll")){
		wctl(w, "noscroll");
		return 1;
	}
	return 0;
}

static long
utfncpy(char *to, char *from, int n)
{
	char *end, *e;

	e = to+n;
	if(to >= e)
		return 0;
	end = memccpy(to, from, '\0', e - to);
	if(end == nil){
		end = e;
		if(end[-1]&0x80){
			if(end-2>=to && (end[-2]&0xE0)==0xC0)
				return end-to;
			if(end-3>=to && (end[-3]&0xF0)==0xE0)
				return end-to;
			while(end>to && (*--end&0xC0)==0x80)
				;
		}
	}else
		end--;
	return end - to;
}

/* sendinput and fsloop run in the same proc (can't interrupt each other). */
static Req *q;
static Req **eq;
static int eofsent;
static int
__sendinput(Window *w, ulong q0, ulong q1)
{
	char *s, *t;
	int n, nb, eofchar;
	static int partial;
	static char tmp[UTFmax];
	Req *r;
	Rune rune;

	if(!q)
		return 0;

	r = q;
	n = 0;
	if(partial){
	Partial:
		nb = partial;
		if(nb > r->ifcall.count)
			nb = r->ifcall.count;
		memmove(r->ofcall.data, tmp, nb);
		if(nb!=partial)
			memmove(tmp, tmp+nb, partial-nb);
		partial -= nb;
		q = r->aux;
		if(q == nil)
			eq = &q;
		r->aux = nil;
		r->ofcall.count = nb;
		if(debug)
			fprint(2, "satisfy read with partial\n");
		respond(r, nil);
		return n;
	}
	if(q0==q1)
		return 0;
	s = emalloc((q1-q0)*UTFmax+1);
	n = wbody(w, q0, q1, s);
	s[n] = '\0';
	t = strpbrk(s, "\n\004");
	if(t == nil){
		free(s);
		return 0;
	}
	r = q;
	eofchar = 0;
	if(*t == '\004'){
		eofchar = 1;
		*t = '\0';
	}else
		*++t = '\0';
	nb = utfncpy((char*)r->ofcall.data, s, r->ifcall.count);
	if(nb==0 && s<t && r->ifcall.count > 0){
		partial = utfncpy(tmp, s, UTFmax);
		assert(partial > 0);
		chartorune(&rune, tmp);
		partial = runelen(rune);
		free(s);
		n = 1;
		goto Partial;
	}
	n = utfnlen(r->ofcall.data, nb);
	eofsent = nb==strlen(s) && eofchar;
	if(eofsent)
		n++;
	r->ofcall.count = nb;
	q = r->aux;
	if(q == nil)
		eq = &q;
	r->aux = nil;
	if(debug)
		fprint(2, "read returns %lud-%lud: %.*q\n", q0, q0+n, n, r->ofcall.data);
	respond(r, nil);
	return n;
}

static int
_sendinput(Window *w, ulong q0, ulong *q1)
{
	char buf[32];
	int n;

	eofsent = 0;
	n = __sendinput(w, q0, *q1);
	if(n && eofsent && !eraseinput){
		sprint(buf, "#%lud,#%lud", q0+n-1, q0+n);
		wzap(w, buf);
		*q1 -= 1;
		return n-1;
	}
	if(!n || !eraseinput)
		return n;
	/* erase q0 to q0+n */
	sprint(buf, "#%lud,#%lud", q0, q0+n);
	wzap(w, buf);
	*q1 -= n;
	return 0;
}

int
sendinput(Window *w, ulong q0, ulong *q1)
{
	ulong n;
	Req *oq;

	n = 0;
	do {
		oq = q;
		n += _sendinput(w, q0+n, q1);
	} while(q != oq);
	return n;
}

Msg esendinput;
void
fsloop(void*)
{
	Fsevent e;
	Req **l, *r;

	eq = &q;
	memset(&esendinput, 0, sizeof esendinput);
	esendinput.e.c1 = 'C';
	for(;;){
		while(recv(fschan, &e) == -1)
			;
		r = e.r;
		switch(e.type){
		case 'r':
			*eq = r;
			r->aux = nil;
			eq = &r->aux;
			/* call sendinput with hostpt and endpt */
			sendp(win->cevent, &esendinput);
			break;
		case 'f':
			for(l=&q; *l; l=&(*l)->aux){
				if(*l == r->oldreq){
					*l = (*l)->aux;
					if(*l == nil)
						eq = l;
					respond(r->oldreq, "interrupted");
					break;
				}
			}
			respond(r, nil);
			break;
		}
	}
}

void
sendit(char *s)
{
//	char tmp[32];

	winwrite(win->a, "body", s, strlen(s));
/*
 * RSC: The problem here is that other procs can call sendit,
 * so we lose our single-threadedness if we call sendinput.
 * In fact, we don't even have the right queue memory,
 * I think that we'll get a write event from the body write above,
 * and we can do the sendinput then, from our single thread.
 *
 * I still need to figure out how to test this assertion for
 * programs that use /srv/win*
 *
	winselect(win, "$", 0);
	seek(win->addr, 0UL, 0);
	if(read(win->addr, tmp, 2*12) == 2*12)
		hostpt += sendinput(win, hostpt, atol(tmp), );
 */
}

void
execevent(Window *w, Msg *m, int (*command)(Window*, char*))
{
	Event *e;
	int n, len, needfree;
	char *s, *t;

	e = &m->e;
	needfree = 0;
	s = e->text;
	if(e->nb==0 && e->q0<e->q1){
		/* fetch data from window */
		s = emalloc((e->q1-e->q0)*UTFmax+2);
		n = wbody(w, e->q0, e->q1, s);
		s[n] = '\0';
		needfree = 1;
	}else
	if(e->arg[0] != '\0'){
		t = emalloc(strlen(s)+1+strlen(e->arg)+2);
		sprint(t, "%s %s", s, e->arg);
		s = t;
		needfree = 1;
	}

	/* if it's a known command, do it */
	/* if it's a long message, it can't be for us anyway */
	if(!command(w, s) && s[0]!='\0'){	/* send it as typed text */
		/* if it's a built-in from the tag, send it back */
		if(e->flag & 1)
			wwrite(w, m);
		else{	/* send text to main window */
			len = strlen(s);
			if(len>0 && s[len-1]!='\n' && s[len-1]!='\004'){
				if(!needfree){
					/* if(needfree), we left room for a newline before */
					t = emalloc(len+2);
					strcpy(t, s);
					s = t;
					needfree = 1;
				}
				s[len++] = '\n';
				s[len] = '\0';
			}
			sendit(s);
		}
	}
	if(needfree)
		free(s);
}

int
hasboundary(char *b, int nb)
{
	int i;

	for(i=0; i<nb; i++)
		if(b[i]=='\n' || b[i]=='\004')
			return 1;
	return 0;
}

void
mainctl(void *v)
{
	Window *w;
	Msg *m, *held;
	Event *e;
	int delta, pendingS, pendingK, seq, try;
	ulong hostpt, endpt;

	w = v;
	seq = wguard(w);
	proccreate(wevents, w, STACK);
	held = nil;

	hostpt = resumept;
	endpt = resumept;
	waddr(w, "0", 0);
	pendingS = 0;
	pendingK = 0;
	for(;;){
		if(debug)
			fprint(2, "input range %lud-%lud\n", hostpt, endpt);
		m = recvp(w->cevent);
		e = &m->e;
		if(debug)
			fprint(2, "msg: %C %C %d %d %d %d %q\n",
				e->c1 ? e->c1 : ' ', e->c2 ? e->c2 : ' ', e->q0, e->q1, e->flag, m->nb, m->b);
		try = e->c2=='I' || e->c2=='D';
		if(try)
			seq++;
		switch(e->c1){
		default:
		Unknown:
			fprint(2, "unknown message %c%c\n", e->c1, e->c2);
			break;

		case 'C':	/* input needed for /dev/cons */
			if(pendingS)
				pendingK = 1;
			else
				hostpt += sendinput(w, hostpt, &endpt);
			break;

		case 'S':	/* output to stdout */
			held = m;
			try = 1;
			break;

		case 'E':	/* write to tag or body; body happens due to sendit */
			delta = e->q1-e->q0;
			if(e->c2=='I'){
				endpt += delta;
				if(e->q0 < hostpt)
					hostpt += delta;
				else
					hostpt += sendinput(w, hostpt, &endpt);
				break;
			}
			if(!islower(e->c2))
				fprint(2, "win msg: %C %C %d %d %d %d %q\n",
					e->c1, e->c2, e->q0, e->q1, e->flag, m->nb, m->b);
			break;

		case 'F':	/* generated by our actions (specifically case 'S' above) */
			delta = e->q1-e->q0;
			if(e->c2=='D'){
				/* we know about the delete by _sendinput */
				break;
			}
			if(e->c2=='I'){
				pendingS -= e->q1 - e->q0;
				if(pendingS < 0)
					fprint(2, "win: pendingS = %d\n", pendingS);
				if(e->q0 != hostpt)
					fprint(2, "win: insert at %d expected %lud\n", e->q0, hostpt);
				endpt += delta;
				hostpt += delta;
				sendp(writechan, nil);
				if(pendingS == 0 && pendingK){
					pendingK = 0;
					hostpt += sendinput(w, hostpt, &endpt);
				}
				break;
			}
			if(!islower(e->c2))
				fprint(2, "win msg: %C %C %d %d %d %d %q\n",
					e->c1, e->c2, e->q0, e->q1, e->flag, m->nb, m->b);
			break;

		case 'K':
			delta = e->q1-e->q0;
			switch(e->c2){
			case 'x':
			case 'X':
			case 'l':
			case 'L':
				wwrite(w, m);
				break;
			case 'D':
				endpt -= delta;
				if(e->q1 < hostpt)
					hostpt -= delta;
				else if(e->q0 < hostpt)
					hostpt = e->q0;
				break;
			case 'I':
				delta = e->q1 - e->q0;
				endpt += delta;
				if(endpt < e->q1)	/* just in case */
					endpt = e->q1;
				if(e->q0 < hostpt)
					hostpt += delta;
				if(m->nb>0 && m->b[m->nb-1]==0x7F){
					write(notepg, "interrupt", 9);
					hostpt = endpt;
					break;
				}
				if(e->q0 >= hostpt
				&& hasboundary(m->b, m->nb)){
					/*
					 * If we are between the S message (which
					 * we processed by inserting text in the
					 * window) and the F message notifying us
					 * that the text has been inserted, then our
					 * impression of the hostpt and acme's
					 * may be different.  This could be seen if you
					 * hit enter a bunch of times in a con
					 * session.  To work around the unreliability,
					 * only send input if we don't have an S pending.
					 * The same race occurs between when a character
					 * is typed and when we get notice of it, but
					 * since characters tend to be typed at the end
					 * of the buffer, we don't run into it.  There's
					 * no workaround possible for this typing race,
					 * since we can't tell when the user has typed
					 * something but we just haven't been notified.
					 */
					if(pendingS)
						pendingK = 1;
					else
						hostpt += sendinput(w, hostpt, &endpt);
				}
				break;
			}
			break;

		case 'M':	/* mouse */
			delta = e->q1-e->q0;
			switch(e->c2){
			case 'x':
			case 'X':
				execevent(w, m, command);
				break;

			case 'l':	/* reflect all searches back to acme */
			case 'L':
				wwrite(w, m);
				break;

			case 'I':
				endpt += delta;
				if(e->q0 < hostpt)
					hostpt += delta;
				else
					hostpt += sendinput(w, hostpt, &endpt);
				break;

			case 'D':
				endpt -= delta;
				if(e->q1 < hostpt)
					hostpt -= delta;
				else if(e->q0 < hostpt)
					hostpt = e->q0;
				break;
			case 'd':	/* modify away; we don't care */
			case 'i':
				break;

			default:
				goto Unknown;
			}
		}
		if(held != nil && try)
			switch(wput(w, hostpt, seq, held->b, held->nb)){
			case 1:
				pendingS += held->nr;
				held = nil;
				break;
			case -1:
				held = nil;
				sendp(writechan, nil);
				break;
			}
	}
}

enum
{
	NARGS		= 100,
	NARGCHAR	= 8*1024,
	EXECSTACK 	= STACK+(NARGS+1)*sizeof(char*)+NARGCHAR
};

struct Exec
{
	char		**argv;
	Channel	*cpid;
};

int
lookinbin(char *s)
{
	if(s[0] == '/')
		return 0;
	if(s[0]=='.' && s[1]=='/')
		return 0;
	if(s[0]=='.' && s[1]=='.' && s[2]=='/')
		return 0;
	return 1;
}

static void
devacme(void)
{
	char buf[64], n[16];
	int fd, k;

	snprint(buf, sizeof buf, "/mnt/amux/%d/clone", win->id);
	if((fd = open(buf, ORDWR)) >= 0){
		if((k = read(fd, n, sizeof n-1)) > 0){
			n[k] = 0;
			snprint(buf, sizeof buf, "/mnt/amux/%d/%d", win->id, atoi(n));
			if(bind(buf, "/dev/acme", MREPL) >= 0)
				return;
		}
		close(fd);
	}
	snprint(buf, sizeof buf, "/mnt/wsys/%d", win->id);
	bind(buf, "/dev/acme", MREPL);
}

/* adapted from mail.  not entirely free of details from that environment */
void
execproc(void *v)
{
	struct Exec *e;
	char *cmd, **av;
	Channel *cpid;

	e = v;
	rfork(RFCFDG|RFNOTEG);
	av = e->argv;
	close(0);
	open("/dev/cons", OREAD);
	close(1);
	open("/dev/cons", OWRITE);
	dup(1, 2);
	devacme();
	cpid = e->cpid;
	free(e);
	procexec(cpid, av[0], av);
	if(lookinbin(av[0])){
		cmd = estrstrdup("/bin/", av[0]);
		procexec(cpid, cmd, av);
	}
	error("can't exec %s: %r", av[0]);
}

void
startcmd(char *argv[], int *notepg)
{
	struct Exec *e;
	Channel *cpid;
	char buf[64];
	int pid;

	e = emalloc(sizeof(struct Exec));
	e->argv = argv;
	cpid = chancreate(sizeof(ulong), 0);
	e->cpid = cpid;
	proccreate(execproc, e, EXECSTACK);
	do
		pid = recvul(cpid);
	while(pid == -1);
	sprint(buf, "/proc/%d/notepg", pid);
	*notepg = open(buf, OWRITE);
}
