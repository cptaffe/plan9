#include <u.h>
#include <libc.h>
#include <authsrv.h>
#include <fcall.h>
#include "tapefs.h"

Fid	*fids;
Ram	*ram;
int	mfd[2];
char	*user;
uchar	mdata[Maxbuf+IOHDRSZ];
int	messagesize = Maxbuf+IOHDRSZ;
Fcall	rhdr;
Fcall	thdr;
ulong	path;
Idmap	*uidmap;
Idmap	*gidmap;
int	replete;
int	blocksize;		/* for 32v */
int	verbose;
int	newtap;		/* tap with time in sec */
int	blocksize;
void	(*lastclose)(Ram*);
int	(*checkempty)(Ram*);
Ram	*(*findchild)(Ram*, char*);

Fid *	newfid(int);
int	ramstat(Ram*, uchar*, int);
uint	ramsize(Ram*);
void	io(void);
void	usage(void);
int	perm(int);

char	*rflush(Fid*), *rversion(Fid*), *rauth(Fid*),
	*rattach(Fid*), *rwalk(Fid*),
	*ropen(Fid*), *rcreate(Fid*),
	*rread(Fid*), *rwrite(Fid*), *rclunk(Fid*),
	*rremove(Fid*), *rstat(Fid*), *rwstat(Fid*);

char 	*(*fcalls[])(Fid*) = {
	[Tflush]	rflush,
	[Tversion]		rversion,
	[Tauth]	rauth,
	[Tattach]	rattach,
	[Twalk]		rwalk,
	[Topen]		ropen,
	[Tcreate]	rcreate,
	[Tread]		rread,
	[Twrite]	rwrite,
	[Tclunk]	rclunk,
	[Tremove]	rremove,
	[Tstat]		rstat,
	[Twstat]	rwstat,
};

char	Eperm[] =	"permission denied";
char	Enotdir[] =	"not a directory";
char	Enoauth[] =	"tapefs: authentication not required";
char	Enotexist[] =	"file does not exist";
char	Einuse[] =	"file in use";
char	Eexist[] =	"file exists";
char	Enotowner[] =	"not owner";
char	Eisopen[] = 	"file already open for I/O";
char	Excl[] = 	"exclusive use file already open";
char	Ename[] = 	"illegal name";
char	Efid[] =	"unknown fid";
char	Ecount[] =	"read count too small for a directory entry";

void
notifyf(void *a, char *s)
{
	USED(a);
	if(strncmp(s, "interrupt", 9) == 0)
		noted(NCONT);
	noted(NDFLT);
}

void
main(int argc, char *argv[])
{
	Ram *r;
	char *defmnt;
	int p[2];
	char buf[2*ERRMAX];

	fmtinstall('F', fcallfmt);

	defmnt = "/n/tapefs";
	ARGBEGIN{
	case 'm':
		defmnt = EARGF(usage());
		break;
	case 'p':			/* password file */
		uidmap = getpass(EARGF(usage()));
		break;
	case 'g':			/* group file */
		gidmap = getpass(EARGF(usage()));
		break;
	case 'v':
		verbose++;
		break;
	case 'n':
		newtap++;
		break;
	case 'b':
		blocksize = atoi(EARGF(usage()));
		break;
	default:
		usage();
	}ARGEND

	if(argc==0)
		error("no file to mount");
	user = getuser();
	if(user == nil)
		user = "dmr";
	ram = r = (Ram *)emalloc(sizeof(Ram));
	r->busy = 1;
	r->data = 0;
	r->ndata = 0;
	r->perm = DMDIR | 0775;
	r->qid.path = 0;
	r->qid.vers = 0;
	r->qid.type = QTDIR;
	r->parent = 0;
	r->child = 0;
	r->next = 0;
	r->user = user;
	r->group = user;
	r->atime = time(0);
	r->mtime = r->atime;
	r->replete = 0;
	r->name = estrdup(".");
	populate(argv[0]);
	r->replete |= replete;
	if(pipe(p) < 0)
		error("pipe failed");
	mfd[0] = mfd[1] = p[0];
	notify(notifyf);

	switch(rfork(RFFDG|RFPROC|RFNAMEG|RFNOTEG)){
	case -1:
		error("fork");
	case 0:
		close(p[1]);
		notify(notifyf);
		io();
		break;
	default:
		close(p[0]);	/* don't deadlock if child fails */
		if(mount(p[1], -1, defmnt, MREPL|MCREATE, "") < 0) {
			snprint(buf, sizeof buf, "mount on `%s' failed", defmnt);
			error(buf);
		}
	}
	exits(0);
}

char*
rversion(Fid *unused)
{
	Fid *f;

	USED(unused);

	if(rhdr.msize < 256)
		return "version: message too small";
	if(rhdr.msize > messagesize)
		rhdr.msize = messagesize;
	else
		messagesize = rhdr.msize;
	thdr.msize = messagesize;
	if(strncmp(rhdr.version, "9P2000", 6) != 0)
		return "unrecognized 9P version";
	thdr.version = "9P2000";

	for(f = fids; f; f = f->next)
		if(f->busy)
			rclunk(f);
	return 0;
}

char*
rauth(Fid *unused)
{
	USED(unused);

	return Enoauth;
}

char*
rflush(Fid *f)
{
	USED(f);
	return 0;
}

char*
rattach(Fid *f)
{
	/* no authentication! */
	f->busy = 1;
	f->rclose = 0;
	f->ram = ram;
	thdr.qid = f->ram->qid;
	if(rhdr.uname[0])
		f->user = strdup(rhdr.uname);
	else
		f->user = "none";
	return 0;
}

char*
rwalk(Fid *f)
{
	Fid *nf;
	Ram *r;
	char *err;
	char *name;
	Ram *dir;
	int i;

	nf = nil;
	if(f->ram->busy == 0)
		return Enotexist;
	if(f->open)
		return Eisopen;
	if(rhdr.newfid != rhdr.fid){
		nf = newfid(rhdr.newfid);
		nf->busy = 1;
		nf->open = 0;
		nf->rclose = 0;
		nf->ram = f->ram;
		nf->user = f->user;	/* no ref count; the leakage is minor */
		f = nf;
	}

	thdr.nwqid = 0;
	err = nil;
	r = f->ram;

	if(rhdr.nwname > 0){
		for(i=0; i<rhdr.nwname; i++){
			if((r->qid.type & QTDIR) == 0){
				err = Enotdir;
				break;
			}
			if(r->busy == 0){
				err = Enotexist;
				break;
			}
			r->atime = time(0);
			name = rhdr.wname[i];
			dir = r;
			if(!perm(Pexec)){
				err = Eperm;
				break;
			}
			if(strcmp(name, "..") == 0){
				r = dir->parent? dir->parent: dir;
   Accept:
				if(i == MAXWELEM){
					err = "name too long";
					break;
				}
 				thdr.wqid[thdr.nwqid++] = r->qid;
				continue;
			}
			if(!dir->replete)
				popdir(dir);
			if(findchild){
				if((r = (*findchild)(dir, name)) != 0)
					goto Accept;
				break;
			}
			for(r=dir->child; r; r=r->next)
				if(r->busy && strcmp(name, r->name)==0)
					goto Accept;
			break;	/* file not found */
		}

		if(i==0 && err == nil)
			err = Enotexist;
	}

	if(err!=nil || thdr.nwqid<rhdr.nwname){
		if(nf){
			nf->busy = 0;
			nf->open = 0;
			nf->ram = 0;
		}
	}else if(thdr.nwqid  == rhdr.nwname){
		f->ram = r;
		f->diroff = 0;
	}

	return err;

}

char *
ropen(Fid *f)
{
	Ram *r;
	int mode, trunc;

	if(f->open)
		return Eisopen;
	r = f->ram;
	if(r->busy == 0)
		return Enotexist;
	if(r->perm & DMEXCL)
		if(r->open)
			return Excl;
	mode = rhdr.mode;
	if(r->qid.type & QTDIR){
		if(mode != OREAD)
			return Eperm;
		f->diroff = 0;
		thdr.qid = r->qid;
		return 0;
	}
	if(mode & ORCLOSE)
		return Eperm;
	trunc = mode & OTRUNC;
	mode &= OPERM;
	if(mode==OWRITE || mode==ORDWR || trunc)
		if(!perm(Pwrite))
			return Eperm;
	if(mode==OREAD || mode==ORDWR)
		if(!perm(Pread))
			return Eperm;
	if(mode==OEXEC)
		if(!perm(Pexec))
			return Eperm;
	if(trunc && (r->perm&DMAPPEND)==0){
		r->ndata = 0;
		dotrunc(r);
		r->qid.vers++;
	}
	thdr.qid = r->qid;
	thdr.iounit = messagesize-IOHDRSZ;
	f->open = 1;
	r->open++;
	return 0;
}

char *
rcreate(Fid *f)
{
	USED(f);

	return Eperm;
}

char*
rread(Fid *f)
{
	static char err[ERRMAX];
	int len;
	Ram *r;
	char *buf;
	uvlong off, pos;
	int n, cnt;

	if(f->ram->busy == 0)
		return Enotexist;
	n = 0;
	thdr.count = 0;
	off = rhdr.offset;
	if(rhdr.count > messagesize-IOHDRSZ)
		cnt = messagesize-IOHDRSZ;
	else
		cnt = rhdr.count;
	buf = thdr.data;
	if(f->ram->qid.type & QTDIR){
		if(!f->ram->replete)
			popdir(f->ram);
		if(off != 0 && off == f->diroff){
			pos = off;
			r = f->dirnext;
		}else{
			pos = 0;
			r = f->ram->child;
		}
		for(; r!=nil; r=r->next){
			if(!r->busy)
				continue;
			len = ramsize(r);
			if(pos < off){
				pos += len;
				continue;
			}
			if(len > cnt-n){
				if(n == 0)
					return Ecount;
				break;
			}
			n += ramstat(r, (uchar*)buf+n, cnt-n);
			pos += len;
		}
		f->diroff = pos;
		f->dirnext = r;
		thdr.count = n;
		return 0;
	}
	r = f->ram;
	if(off >= r->ndata){
		if(r->ndata == 0 && checkempty && (*checkempty)(r) < 0){
			rerrstr(err, sizeof err);
			return err;
		}
		return 0;
	}
	r->atime = time(0);
	n = cnt;
	if(off+n > r->ndata)
		n = r->ndata - off;
	if((thdr.data = doread(r, off, n)) == nil){
		rerrstr(err, sizeof err);
		return err;
	}
	thdr.count = n;
	return 0;
}

char*
rwrite(Fid *f)
{
	Ram *r;
	ulong off;
	int cnt;

	r = f->ram;
	if(dopermw(f->ram)==0)
		return Eperm;
	if(r->busy == 0)
		return Enotexist;
	off = rhdr.offset;
	if(r->perm & DMAPPEND)
		off = r->ndata;
	cnt = rhdr.count;
	if(r->qid.type & QTDIR)
		return "file is a directory";
	if(off > 100*1024*1024)		/* sanity check */
		return "write too big";
	dowrite(r, rhdr.data, off, cnt);
	r->qid.vers++;
	r->mtime = time(0);
	thdr.count = cnt;
	return 0;
}

char *
rclunk(Fid *f)
{
	if(f->open && --f->ram->open == 0 && lastclose)
		(*lastclose)(f->ram);
	f->busy = 0;
	f->open = 0;
	f->ram = 0;
	f->diroff = 0;
	return 0;
}

char *
rremove(Fid *f)
{
	rclunk(f);
	return Eperm;
}

char *
rstat(Fid *f)
{
	if(f->ram->busy == 0)
		return Enotexist;
	thdr.nstat = ramstat(f->ram, thdr.stat, messagesize-IOHDRSZ);
	return 0;
}

char *
rwstat(Fid *f)
{
	if(f->ram->busy == 0)
		return Enotexist;
	return Eperm;
}

static void
ramdir(Ram *r, Dir *dir)
{
	memset(dir, 0, sizeof *dir);
	dir->name = r->name;
	dir->qid = r->qid;
	dir->mode = r->perm;
	dir->length = r->ndata;
	dir->uid = r->user;
	dir->gid = r->group;
	dir->muid = r->user;
	dir->atime = r->atime;
	dir->mtime = r->mtime;
}

int
ramstat(Ram *r, uchar *buf, int nbuf)
{
	Dir dir;

	ramdir(r, &dir);
	return convD2M(&dir, buf, nbuf);
}

uint
ramsize(Ram *r)
{
	Dir dir;

	ramdir(r, &dir);
	return sizeD2M(&dir);
}

Fid *
newfid(int fid)
{
	Fid *f, *ff;

	ff = 0;
	for(f = fids; f; f = f->next)
		if(f->fid == fid)
			return f;
		else if(!ff && !f->busy)
			ff = f;
	if(ff){
		ff->fid = fid;
		return ff;
	}
	f = emalloc(sizeof *f);
	f->ram = 0;
	f->fid = fid;
	f->open = 0;
	f->next = fids;
	fids = f;
	return f;
}

void
io(void)
{
	char *err;
	Fid *f;
	int n, nerr;
	char buf[ERRMAX];

	errstr(buf, sizeof buf);
	for(nerr=0, buf[0]='\0'; nerr<100; nerr++){
		/*
		 * reading from a pipe or a network device
		 * will give an error after a few eof reads
		 * however, we cannot tell the difference
		 * between a zero-length read and an interrupt
		 * on the processes writing to us,
		 * so we wait for the error
		 */
		n = read9pmsg(mfd[0], mdata, sizeof mdata);
		if(n==0)
			continue;
		if(n < 0){
			if(buf[0]=='\0')
				errstr(buf, sizeof buf);
			continue;
		}
		nerr = 0;
		buf[0] = '\0';
		if(convM2S(mdata, n, &rhdr) != n)
			error("convert error in convM2S");

		if(verbose)
			fprint(2, "tapefs: <=%F\n", &rhdr);/**/

		thdr.data = (char*)mdata + IOHDRSZ;
		thdr.stat = mdata + IOHDRSZ;
		if(rhdr.type >= nelem(fcalls) || !fcalls[rhdr.type])
			err = "bad fcall type";
		else{
			f = newfid(rhdr.fid);
			if(!f->busy && rhdr.type != Tversion && rhdr.type != Tauth
			&& rhdr.type != Tattach && rhdr.type != Tflush)
				err = Efid;
			else
				err = (*fcalls[rhdr.type])(f);
		}
		if(err){
			thdr.type = Rerror;
			thdr.ename = err;
		}else{
			thdr.type = rhdr.type + 1;
			thdr.fid = rhdr.fid;
		}
		thdr.tag = rhdr.tag;
		n = convS2M(&thdr, mdata, messagesize);
		if(n <= 0)
			error("convert error in convS2M");
		if(verbose)
			fprint(2, "tapefs: =>%F\n", &thdr);/**/
		if(write(mfd[1], mdata, n) != n)
			error("mount write");
	}
	if(buf[0]=='\0' || strstr(buf, "hungup"))
		exits("");
	fprint(2, "%s: mount read: %s\n", argv0, buf);
	exits(buf);
}

int
perm(int p)
{
	if(p==Pwrite)
		return 0;
	return 1;
}

void
error(char *s)
{
	fprint(2, "%s: %s: ", argv0, s);
	perror("");
	exits(s);
}

char*
estrdup(char *s)
{
	char *t;

	t = emalloc(strlen(s)+1);
	strcpy(t, s);
	return t;
}

void *
emalloc(ulong n)
{
	void *p;
	p = mallocz(n, 1);
	if(!p)
		error("out of memory");
	return p;
}

void *
erealloc(void *p, ulong n)
{
	p = realloc(p, n);
	if(!p)
		error("out of memory");
	return p;
}

void
usage(void)
{
	fprint(2, "usage: %s [-s] [-m mountpoint]\n", argv0);
	exits("usage");
}
