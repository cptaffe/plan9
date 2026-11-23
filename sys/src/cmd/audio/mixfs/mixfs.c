#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>
#include <pcm.h>
#pragma varargck type "!" Pcmdesc

enum {
	NBUF = 8*1024,
	NDELAY = 1764,	/* 40.0ms */
	NQUANTA = 64,	/* ~1.45ms */
	NCHAN = 2,
	ABUF = NBUF*NCHAN*2,
	Nsec = 1000000000ULL,
	STACK = 16*1024,
};

enum {
	Dopen,
	Didle,
	Dwrite,
	Dvolread,
	Dvolwrite,
};

#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))

typedef struct Stream Stream;
typedef struct Dmsg Dmsg;

struct Stream
{
	int	used;
	int	mode;
	int	run;
	ulong	rp;
	ulong	wp;
	Req	*req;
	Req	**ereq;
	uchar	*p;
	int	n;
	int	drain;
};

struct Dmsg
{
	int	op;
	int	sweep;
	uchar	*buf;
	int	n;
	Req	*r;
	int	vol[2];
	int	ok;
	int	wasopen;
	int	ms;
	int	open;
	int	null;
	int	delay;
	char	*err;
	char	errbuf[ERRMAX];
};

ulong	mixrp;
int	lbbuf[NBUF][NCHAN];
int	mixbuf[NBUF][NCHAN];
Stream	streams[64];
int	volume[2] = {100, 100};
int	vol64k[2] = {65536, 65536};
int	mixdelay = NDELAY;
int	sweep;
int	devopen;
int	devnull;
int	devbusy;
int	step;
int	ctllast;
Req	*ctlq;
Req	**ectlq = &ctlq;
Dmsg	dm;
int	mixtid;
uvlong	t0;
uvlong	ns;

char	*devaudio;
int	audiofd = -1;
int	volfd = -1;
Pcmdesc	fmt;
int	fmtchanged;
int	isnulldev;
int	delay;
int	rate;

Channel	*creq;
Channel	*cclunk;
Channel	*cdev;
Channel	*cdevdone;
Channel	*ctimer;
Channel	*ctimerdone;

void*
emalloc(ulong n)
{
	void *p;

	p = mallocz(n, 1);
	if(p == nil)
		sysfatal("out of memory");
	return p;
}

void*
erealloc(void *p, ulong n)
{
	p = realloc(p, n);
	if(p == nil)
		sysfatal("out of memory");
	return p;
}

int
s16(uchar *p)
{
	int v;

	v = p[0]<<(sizeof(int)-2)*8 | p[1]<<(sizeof(int)-1)*8;
	v >>= (sizeof(int)-2)*8;
	return v;
}

int
clip16(int v)
{
	if(v > 0x7fff)
		return 0x7fff;
	if(v < -0x8000)
		return -0x8000;
	return v;
}

void
closeaudiodev(void)
{
	if(audiofd >= 0){
		close(audiofd);
		audiofd = -1;
	}
}

void
updfmt(void)
{
	static char svol[4*1024];
	char *s, *e;
	int n, ok;

	ok = 0;
	if(volfd >= 0 && (n = pread(volfd, svol+1, sizeof(svol)-2, 0)) > 8){
		svol[0] = '\n';
		svol[1+n] = 0;
		if((s = strstr(svol, "\nfmtout ")) != nil && (e = strchr(s+12, '\n')) != nil){
			*e = 0;
			ok = fmtchanged = mkpcmdesc(s+8, &fmt) == 0;
		}
		if(!ok && (s = strstr(svol, "\nspeed ")) != nil && (n = strtol(s+6, &s, 10)) > 0){
			fmt = pcmdescdef;
			fmt.rate = n;
			ok = fmtchanged = fmt.rate > 0;
		}
	}
	if(!ok){
		fmt = pcmdescdef;
		fmtchanged = 1;
	}
}

int
reopendevs(char *name)
{
	static char dir[] = "/dev/";
	int i, n, dfd, afd, len;
	char *p;
	Dir *d;

	if(name != nil){
		if(name != devaudio){
			/* hack: restrict to known audio device names */
			if((strncmp(name, "/dev/audio", 10) != 0 || strchr(name+10, '/') != nil)
			&& (strncmp(name, "#u/audio", 8) != 0 || strchr(name+8, '/') != nil)
			&& (strncmp(name, "#A/audio", 8) != 0 || strchr(name+8, '/') != nil)
			&& (strncmp(name, "/dev/null", 9) != 0 || strchr(name+9, '/') != nil)){
				werrstr("name doesnt look like an audio device");
				return -1;
			}
		}
		if((afd = open(name, OWRITE)) >= 0){
			name = strdup(name);
			goto found;
		}
		if(name != devaudio)
			return -1;
	}
	if((dfd = open(dir, OREAD)) >= 0){
		while((n = dirread(dfd, &d)) > 0){
			for(i = 0; i < n; i++){
				len = strlen(d[i].name);
				if((d[i].mode & DMDIR) != 0
				|| len < 5
				|| strncmp(d[i].name, "audio", 5) != 0
				|| strcmp(d[i].name+len-3, "ctl") == 0
				|| strcmp(d[i].name+len-4, "stat") == 0)
					continue;
				name = smprint("%s%s", dir, d[i].name);
				if((afd = open(name, OWRITE)) >= 0){
					close(dfd);
					free(d);
					goto found;
				}
				free(name);
			}
			free(d);
		}
		close(dfd);
		werrstr("no devices found");
	}
	return -1;
found:
	free(devaudio);
	devaudio = name;
	isnulldev = 0;
	audiofd = dup(afd, audiofd);

	close(afd);
	if(volfd >= 0){
		close(volfd);
		volfd = -1;
	}
	if((p = utfrrune(name, '/')) != nil)
		p++;
	else
		p = name;
	if(strncmp(p, "audio", 5) == 0){
		name = smprint("%.*svolume%s", (int)(p - name), name, p+5);
		volfd = open(name, ORDWR);
		free(name);
		if(volfd >= 0)
			fprint(volfd, "delay %d\n", delay);
		updfmt();
	}else if(strncmp(p, "null", 4) == 0){
		isnulldev = 1;
		updfmt();
	}
	return 0;
}

int
eqfmt(Pcmdesc *a, Pcmdesc *b)
{
	return
		a->rate == b->rate
		&& a->bits == b->bits
		&& a->channels == b->channels
		&& a->framesz == b->framesz
		&& a->fmt == b->fmt;
}

int
isaudio(Fid *f)
{
	return f != nil && f->file != nil && strcmp(f->file->name, "audio") == 0;
}

void
devwrite(uchar *p, int n)
{
	static uchar *bufconv;
	static Pcmconv *conv;
	int m;

	if(fmtchanged){
		fmtchanged = 0;
		free(bufconv);
		bufconv = nil;
		freepcmconv(conv);
		conv = nil;
		if(!eqfmt(&fmt, &pcmdescdef))
		if((conv = allocpcmconv(&pcmdescdef, &fmt)) == nil ||
		   (m = pcmratio(conv, ABUF)) < 0 ||
		   (bufconv = malloc(m)) == nil){
			fprint(2, "%s: %r\n", devaudio);
			freepcmconv(conv);
			conv = nil;
		}
		rate = fmt.rate;
	}
	if(conv != nil){
		n = pcmconv(conv, p, bufconv, n);
		p = bufconv;
	}
	if(write(audiofd, p, n) != n)
		closeaudiodev();
}

char*
volread(Dmsg *d)
{
	static char svol[4096];
	Req *r;
	char *f, *e;
	int n, m;

	r = d->r;
	if(r->ifcall.offset == 0){
		m = snprint(svol, sizeof(svol), "dev %s\nmix %d %d\nfmtout %!\nspeed %d\n",
			devaudio?devaudio:"",
			d->vol[0], d->vol[1],
			pcmdescdef,
			pcmdescdef.rate);
		if(volfd >= 0 && (n = pread(volfd, svol+m, sizeof(svol)-m-1, 0)) > 0){
			svol[m+n] = 0;
			/* device's fmtout and speed are removed */
			if((f = strstr(svol+m-1, "\nfmtout ")) != nil && (e = strchr(f+8, '\n')) != nil){
				n -= e-f;
				memmove(f, e, n+1);
			}
			if((f = strstr(svol+m-1, "\nspeed ")) != nil && (e = strchr(f+7, '\n')) != nil){
				n -= e-f;
				memmove(f, e, n+1);
			}
		}
	}
	readstr(r, svol);
	return nil;
}

char*
volwrite(Dmsg *d)
{
	char msg[64], *f[5];
	Req *r;
	int n, nf;

	r = d->r;
	snprint(msg, sizeof(msg), "%.*s",
		utfnlen((char*)r->ifcall.data, r->ifcall.count), (char*)r->ifcall.data);
	nf = tokenize(msg, f, nelem(f));
	if(nf > 1 && strcmp(f[0], "dev") == 0){
		if(reopendevs(f[1]) < 0)
			goto err;
	}else if(volfd >= 0){
		if(nf > 1 && strcmp(f[0], "delay") == 0){
			n = atoi(f[1]);
			if(n < NQUANTA || n > NBUF)
				goto err;
			delay = n;
		}
		if(write(volfd, r->ifcall.data, r->ifcall.count) < 0)
			goto err;
	}else
		return "bad msg";
	return nil;
err:
	rerrstr(d->errbuf, sizeof d->errbuf);
	return d->errbuf;
}

void
devproc(void *)
{
	Dmsg *d;

	threadsetname("devproc");
	rate = fmt.rate;
	delay = NDELAY;
	for(;;){
		d = recvp(cdev);
		d->err = nil;
		switch(d->op){
		case Dopen:
			d->ok = audiofd >= 0 || reopendevs(devaudio) >= 0;
			if(!d->ok)
				fprint(2, "%s: reopendevs: %r\n", argv0);
			break;
		case Didle:
			d->ms = 100;
			d->wasopen = audiofd >= 0;
			if(audiofd >= 0){
				if(d->sweep){
					closeaudiodev();
				} else {
					/* attempt to sleep just shortly before buffer underrun */
					d->ms = seek(audiofd, 0, 2);
					d->ms *= 800;
					d->ms /= rate*NCHAN*2;
				}
			}
			break;
		case Dwrite:
			devwrite(d->buf, d->n);
			break;
		case Dvolread:
			d->err = volread(d);
			break;
		case Dvolwrite:
			d->err = volwrite(d);
			break;
		}
		d->open = audiofd >= 0;
		d->null = isnulldev;
		d->delay = delay;
		sendp(cdevdone, d);
	}
}

void
timerproc(void *)
{
	threadsetname("timerproc");
	for(;;){
		sleep(recvul(ctimer));
		sendul(ctimerdone, 0);
	}
}

void
senddev(int op)
{
	dm.op = op;
	devbusy = 1;
	sendp(cdev, &dm);
}

void
begin(Stream *s, Req *r)
{
	int n;

	s->drain = 0;
	if(r->ifcall.type == Tread){
		s->p = (uchar*)r->ofcall.data;
		n = r->ifcall.count;
		n &= ~(NCHAN*2 - 1);
		r->ofcall.count = n;
	}else{
		s->p = (uchar*)r->ifcall.data;
		n = r->ifcall.count;
		r->ofcall.count = n;
	}
	s->n = n / (NCHAN*2);
}

int
readmore(Stream *s)
{
	int i, j, m, v;

	while(s->n > 0){
		if(s->run == 0){
			s->rp = mixrp;
			s->run = 1;
		}
		m = (long)(mixrp - s->rp);
		if(m <= 0){
			s->run = 1;
			return 0;
		}
		if(m > NBUF){
			m = NBUF;
			s->rp = mixrp - m;
		}
		if(m > s->n)
			m = s->n;

		for(i=0; i<m; i++){
			for(j=0; j<NCHAN; j++){
				v = lbbuf[s->rp % NBUF][j];
				*s->p++ = v & 0xFF;
				*s->p++ = v >> 8;
			}
			s->rp++;
		}

		s->n -= m;
	}
	return 1;
}

int
writemore(Stream *s)
{
	int i, j, m;

	if(s->drain)
		return 1;
	while(s->n > 0){
		if(s->run == 0){
			s->wp = mixrp;
			s->run = 1;
		}
		m = NBUF-1 - (long)(s->wp - mixrp);

		if(m <= 0){
			s->run = 1;
			return 0;
		}
		if(m > s->n)
			m = s->n;

		for(i=0; i<m; i++){
			for(j=0; j<NCHAN; j++){
				mixbuf[s->wp % NBUF][j] += s16(s->p);
				s->p += 2;
			}
			s->wp++;
		}

		s->n -= m;
	}
	s->drain = 1;
	if((long)(s->wp - mixrp) >= mixdelay){
		s->run = 1;
		return 0;
	}
	return 1;
}

Req*
dequeue(Req **l, Req ***e, Req *r)
{
	for(; *l != nil; l = (Req**)&(*l)->aux)
		if(*l == r){
			*l = r->aux;
			if(*l == nil)
				*e = l;
			r->aux = nil;
			return r;
		}
	return nil;
}

void
service(Stream *s)
{
	Req *r;
	int done;

	while((r = s->req) != nil){
		if(r->ifcall.type == Tread)
			done = readmore(s);
		else
			done = writemore(s);
		if(!done)
			return;
		dequeue(&s->req, &s->ereq, r);
		if(s->req != nil)
			begin(s, s->req);
		respond(r, nil);
	}
}

void
queue(Stream *s, Req *r)
{
	r->aux = nil;
	*s->ereq = r;
	s->ereq = (Req**)&r->aux;
	if(s->req == r){
		begin(s, r);
		service(s);
	}
}

void
ctl(void)
{
	Req *r;

	r = ctlq;
	dequeue(&ctlq, &ectlq, r);
	dm.r = r;
	dm.vol[0] = volume[0];
	dm.vol[1] = volume[1];
	senddev(r->ifcall.type == Tread ? Dvolread : Dvolwrite);
}

void
mix(int m)
{
	static uchar buf[ABUF];
	uvlong dt;
	int i, j, v;
	ulong rp;
	uchar *p;

	if(devnull){
		dt = nsec() - t0;
		m = (dt+ns-1)/ns;
		m = MAX(NQUANTA, m);
		m = MIN(m, NBUF);
		t0 += dt;
	}else
		m = MIN(m, NQUANTA);

	p = buf;
	rp = mixrp;
	for(i=0; i<m; i++){
		for(j=0; j<NCHAN; j++){
			v = clip16(mixbuf[rp % NBUF][j])*vol64k[j] / 65536;
			lbbuf[rp % NBUF][j] = v;
			mixbuf[rp % NBUF][j] = 0;
			*p++ = v & 0xFF;
			*p++ = v >> 8;
		}
		rp++;
	}
	mixrp = rp;

	if(devnull){
		sendul(ctimer, 5);
		return;
	}
	dm.buf = buf;
	dm.n = p - buf;
	senddev(Dwrite);
}

void
mixstep(void)
{
	int n, m;
	Stream *s;

	m = NBUF;
	for(s = streams; s < streams+nelem(streams); s++){
		if(s->run){
			if(s->mode & OWRITE){
				n = (long)(s->wp - mixrp);
				if(n <= 0 && (s->used == 0 || sweep))
					s->run = 0;
				else if(n < m)
					m = n;
				if(n < mixdelay)
					service(s);
			} else {
				n = (long)(mixrp - s->rp);
				if(n > NBUF && (s->used == 0 || sweep))
					s->run = 0;
				if(n > 0)
					service(s);
			}
		}
	}
	m %= NBUF;

	if(m == 0){
		dm.sweep = sweep;
		senddev(Didle);
		return;
	}
	sweep = 0;
	if(!devopen){
		senddev(Dopen);
		return;
	}
	mix(m);
}

void
devdone(Dmsg *d)
{
	devbusy = 0;
	devopen = d->open;
	devnull = d->null;
	mixdelay = d->delay;
	switch(d->op){
	case Dopen:
		if(d->ok)
			step = 1;
		else
			sendul(ctimer, 1000);
		break;
	case Didle:
		if(d->wasopen)
			sweep = 1;
		sendul(ctimer, d->ms);
		break;
	case Dwrite:
		step = 1;
		break;
	case Dvolread:
	case Dvolwrite:
		respond(d->r, d->err);
		break;
	}
}

void
mixopen(Req *r)
{
	Stream *s;

	for(s = streams; s < streams+nelem(streams); s++){
		if(s->used == 0 && s->run == 0){
			s->used = 1;
			s->mode = r->ifcall.mode;
			r->fid->aux = s;
			respond(r, nil);
			return;
		}
	}
	respond(r, "all streams in use");
}

void
mixvol(Req *r)
{
	char msg[64], *f[5];
	int x[2], nf;

	if(r->ifcall.type == Twrite){
		r->ofcall.count = r->ifcall.count;
		snprint(msg, sizeof(msg), "%.*s",
			utfnlen((char*)r->ifcall.data, r->ifcall.count), (char*)r->ifcall.data);
		nf = tokenize(msg, f, nelem(f));
		if(nf > 1 && strcmp(f[0], "mix") == 0){
			x[0] = atoi(f[1]);
			x[1] = nf < 3 ? x[0] : atoi(f[2]);
			if(f[1][0] == '+' || f[1][0] == '-'){
				x[0] += volume[0];
				x[1] += volume[1];
			}
			volume[0] = MAX(0, x[0]);
			volume[0] = MIN(volume[0], 100);
			volume[1] = MAX(0, x[1]);
			volume[1] = MIN(volume[1], 100);
			/* ≈60dB dynamic range; [0-100] → [0-65536] */
			vol64k[0] = 65.536 * (exp(volume[0] * 0.0690876) - 1.0);
			vol64k[1] = 65.536 * (exp(volume[1] * 0.0690876) - 1.0);
			respond(r, nil);
			return;
		}
	}
	r->aux = nil;
	*ectlq = r;
	ectlq = (Req**)&r->aux;
}

void
mixflush(Req *r)
{
	Req *o;
	Stream *s;
	int head;

	o = r->oldreq;
	if(dequeue(&ctlq, &ectlq, o) != nil)
		respond(o, "interrupted");
	else if((o->ifcall.type == Tread || o->ifcall.type == Twrite)
	&& isaudio(o->fid) && (s = o->fid->aux) != nil){
		head = s->req == o;
		if(dequeue(&s->req, &s->ereq, o) != nil){
			respond(o, "interrupted");
			if(head && s->req != nil){
				begin(s, s->req);
				service(s);
			}
		}
	}
	respond(r, nil);
}

void
mixstat(Req *r)
{
	Stream *s;

	s = r->fid->aux;
	r->d.length = 0;
	if(s != nil && s->run){
		r->d.length = (long)(s->wp - mixrp);
		r->d.length *= NCHAN*2;
	}
	respond(r, nil);
}

void
mixreq(Req *r)
{
	switch(r->ifcall.type){
	case Topen:
		mixopen(r);
		break;
	case Tstat:
		mixstat(r);
		break;
	case Tflush:
		mixflush(r);
		break;
	case Tread:
	case Twrite:
		if(r->fid->file->aux == &volfd)
			mixvol(r);
		else
			queue(r->fid->aux, r);
		break;
	default:
		respond(r, "bug in mixproc");
		break;
	}
}

void
kick(void)
{
	for(;;){
		if(devbusy)
			return;
		if(ctlq != nil && (!step || !ctllast)){
			ctllast = 1;
			ctl();
		}else if(step){
			step = 0;
			ctllast = 0;
			mixstep();
		}else
			return;
	}
}

void
mixproc(void *)
{
	Alt a[5];
	Req *r;
	Stream *s;
	Dmsg *d;
	ulong u;

	threadsetname("mixproc");
	ns = Nsec/pcmdescdef.rate;
	t0 = nsec();
	for(s = streams; s < streams+nelem(streams); s++)
		s->ereq = &s->req;
	a[0].op = CHANRCV;
	a[0].c = creq;
	a[0].v = &r;
	a[1].op = CHANRCV;
	a[1].c = cclunk;
	a[1].v = &s;
	a[2].op = CHANRCV;
	a[2].c = cdevdone;
	a[2].v = &d;
	a[3].op = CHANRCV;
	a[3].c = ctimerdone;
	a[3].v = &u;
	a[4].op = CHANEND;

	step = 1;
	for(;;){
		kick();
		switch(alt(a)){
		case 0:
			mixreq(r);
			break;
		case 1:
			s->used = 0;
			break;
		case 2:
			devdone(d);
			break;
		case 3:
			step = 1;
			break;
		}
	}
}

void
fsopen(Req *r)
{
	if(!isaudio(r->fid)){
		respond(r, nil);
		return;
	}
	sendp(creq, r);
}

void
fsstat(Req *r)
{
	if(!isaudio(r->fid)){
		r->d.length = 0;
		respond(r, nil);
		return;
	}
	sendp(creq, r);
}

void
fssend(Req *r)
{
	sendp(creq, r);
}

void
fsclunk(Fid *f)
{
	Stream *s;

	if(isaudio(f) && (s = f->aux) != nil){
		if(threadid() == mixtid)
			s->used = 0;
		else
			sendp(cclunk, s);
	}
	f->aux = nil;
}

void
fsend(Srv *)
{
	threadexitsall(nil);
}

Srv fs = {
	.open=		fsopen,
	.read=		fssend,
	.write=		fssend,
	.stat=		fsstat,
	.destroyfid=	fsclunk,
	.flush=		fssend,
	.end=		fsend,
};

void
usage(void)
{
	fprint(2, "usage: %s [-D] [-v] [-s srvname] [-m mtpt]\n", argv0);
	exits("usage");
}

void
threadmain(int argc, char **argv)
{
	char *srv = nil;
	char *m, *mtpt = "/mnt/mix";

	ARGBEGIN{
	case 'D':
		chatty9p++;
		break;
	case 's':
		srv = EARGF(usage());
		break;
	case 'm':
		mtpt = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND;

	if(argc > 1)
		usage();

	fmtinstall('!', pcmdescfmt);
	reopendevs(argv[0]);
	closeaudiodev();

	creq = chancreate(sizeof(void*), 0);
	cclunk = chancreate(sizeof(void*), 0);
	cdev = chancreate(sizeof(void*), 0);
	cdevdone = chancreate(sizeof(void*), 0);
	ctimer = chancreate(sizeof(ulong), 1);
	ctimerdone = chancreate(sizeof(ulong), 0);
	procrfork(devproc, nil, STACK, RFNAMEG);
	proccreate(timerproc, nil, STACK);
	mixtid = proccreate(mixproc, nil, STACK);

	fs.tree = alloctree(nil, nil, DMDIR|0777, nil);
	createfile(fs.tree->root, "audio", nil, 0666, nil);
	createfile(fs.tree->root, "volume", nil, 0666, &volfd);
	threadpostmountsrv(&fs, srv, mtpt, MREPL);

	bind(mtpt, "/dev", MAFTER);
	m = smprint("%s/audio", mtpt);
	bind(m, "/dev/audio", MREPL);
	m = smprint("%s/volume", mtpt);
	bind(m, "/dev/volume", MREPL);

	threadexits(0);
}
