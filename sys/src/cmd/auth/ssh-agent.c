/*
 * ssh-agent: OpenSSH's agent protocol on factotum's ed25519 keys, after plan9port's
 * auth/ssh-agent. It serves ssh-agent/agent, mounted after /mnt, as factotum is, and posted as
 * /srv/ssh-agent; each open of /mnt/ssh-agent/agent is one conversation, and SSH_AUTH_SOCK
 * names it.
 */
#include <u.h>
#include <libc.h>
#include <auth.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

enum
{
	SSH_AGENT_FAILURE = 5,
	SSH2_AGENTC_REQUEST_IDENTITIES = 11,
	SSH2_AGENT_IDENTITIES_ANSWER,
	SSH2_AGENTC_SIGN_REQUEST,
	SSH2_AGENT_SIGN_RESPONSE,

	Maxmsg = 256*1024,
};

typedef struct Msg Msg;
struct Msg
{
	uchar	*bp;
	uchar	*p;
	uchar	*ep;
};

typedef struct Conv Conv;
struct Conv
{
	uchar	*in;
	int	nin;
	uchar	*out;
	int	nout;
};

static char *factotum = "/mnt/factotum";
static int chatty;

static void*
emalloc(ulong n)
{
	void *v;

	v = mallocz(n, 1);
	if(v == nil)
		sysfatal("out of memory");
	return v;
}

static void
ensure(Msg *m, int n)
{
	int o, len;

	if(m->p+n <= m->ep)
		return;
	o = m->p - m->bp;
	len = (m->ep - m->bp)*2 + n;
	m->bp = realloc(m->bp, len);
	if(m->bp == nil)
		sysfatal("out of memory");
	m->p = m->bp + o;
	m->ep = m->bp + len;
}

static void
put4(Msg *m, uint n)
{
	ensure(m, 4);
	m->p[0] = n>>24;
	m->p[1] = n>>16;
	m->p[2] = n>>8;
	m->p[3] = n;
	m->p += 4;
}

static void
put1(Msg *m, uint n)
{
	ensure(m, 1);
	*m->p++ = n;
}

static void
putn(Msg *m, void *a, uint n)
{
	ensure(m, n);
	memmove(m->p, a, n);
	m->p += n;
}

static void
putstr(Msg *m, void *a, uint n)
{
	put4(m, n);
	putn(m, a, n);
}

static int
get4(Msg *m, uint *n)
{
	if(m->p+4 > m->ep)
		return -1;
	*n = (m->p[0]<<24)|(m->p[1]<<16)|(m->p[2]<<8)|m->p[3];
	m->p += 4;
	return 0;
}

/* the next string, in s, as a Msg of its own */
static int
getstr(Msg *m, Msg *s)
{
	uint n;

	if(get4(m, &n) < 0 || n > m->ep - m->p)
		return -1;
	s->bp = s->p = m->p;
	s->ep = m->p + n;
	m->p += n;
	return 0;
}

static int
streq(Msg *s, char *t)
{
	return s->ep - s->p == strlen(t) && memcmp(s->p, t, s->ep - s->p) == 0;
}

/* the blob of an ed25519 key, pk 32 bytes */
static void
putblob(Msg *m, uchar *pk)
{
	Msg b;

	memset(&b, 0, sizeof b);
	putstr(&b, "ssh-ed25519", 11);
	putstr(&b, pk, 32);
	putstr(m, b.bp, b.p - b.bp);
	free(b.bp);
}

static char*
readall(char *file)
{
	int fd, n, tot;
	char *s;

	if((fd = open(file, OREAD)) < 0)
		return nil;
	s = nil;
	tot = 0;
	for(;;){
		s = realloc(s, tot+8192+1);
		if(s == nil)
			sysfatal("out of memory");
		if((n = read(fd, s+tot, 8192)) <= 0)
			break;
		tot += n;
	}
	close(fd);
	s[tot] = 0;
	return s;
}

/* factotum's ed25519 keys, from its ctl: their pk= and comment= */
static int
listkeys(Msg *m)
{
	char *s, *p, *nl, *f[64], *pk, *comment, file[128];
	uchar b[32];
	int i, n, nk, ed;
	Msg keys;

	snprint(file, sizeof file, "%s/ctl", factotum);
	if((s = readall(file)) == nil)
		return -1;
	memset(&keys, 0, sizeof keys);
	nk = 0;
	for(p = s; *p; p = nl){
		if((nl = strchr(p, '\n')) != nil)
			*nl++ = 0;
		else
			nl = p+strlen(p);
		n = tokenize(p, f, nelem(f));
		if(n < 2 || strcmp(f[0], "key") != 0)
			continue;
		pk = comment = nil;
		ed = 0;
		for(i = 1; i < n; i++)
			if(strcmp(f[i], "proto=ed25519") == 0)
				ed = 1;
			else if(strncmp(f[i], "pk=", 3) == 0)
				pk = f[i]+3;
			else if(strncmp(f[i], "comment=", 8) == 0)
				comment = f[i]+8;
		if(!ed || pk == nil || strlen(pk) != 64 || dec16(b, 32, pk, 64) != 32)
			continue;
		if(comment == nil)
			comment = "factotum";
		putblob(&keys, b);
		putstr(&keys, comment, strlen(comment));
		nk++;
	}
	free(s);
	put4(m, nk);
	putn(m, keys.bp, keys.p - keys.bp);
	free(keys.bp);
	return nk;
}

/* factotum's signature of data by the key whose blob is key */
static int
sign(Msg *m, Msg *key, Msg *data)
{
	Msg t, pk;
	AuthRpc *rpc;
	char buf[256], file[128], *s;
	int i, fd, ok;

	if(getstr(key, &t) < 0 || !streq(&t, "ssh-ed25519") || getstr(key, &pk) < 0 || pk.ep - pk.p != 32){
		werrstr("not an ed25519 key");
		return -1;
	}
	s = seprint(buf, buf+sizeof buf, "proto=ed25519 role=sign pk=");
	for(i = 0; i < 32; i++)
		s = seprint(s, buf+sizeof buf, "%.2ux", pk.p[i]);
	snprint(file, sizeof file, "%s/rpc", factotum);
	if((fd = open(file, ORDWR)) < 0)
		return -1;
	if((rpc = auth_allocrpc(fd)) == nil){
		close(fd);
		return -1;
	}
	ok = auth_rpc(rpc, "start", buf, strlen(buf)) == ARok
		&& auth_rpc(rpc, "write", data->p, data->ep - data->p) == ARok
		&& auth_rpc(rpc, "read", nil, 0) == ARok
		&& rpc->narg == 64;
	if(ok){
		memset(&t, 0, sizeof t);
		putstr(&t, "ssh-ed25519", 11);
		putstr(&t, rpc->arg, 64);
		putstr(m, t.bp, t.p - t.bp);
		free(t.bp);
	}
	auth_freerpc(rpc);
	close(fd);
	return ok ? 0 : -1;
}

/* a request, its type and body, answered into r */
static void
answer(Msg *r, uchar *req, int n)
{
	Msg m, key, data, body;
	uint flags;
	int type;

	m.bp = m.p = req;
	m.ep = req+n;
	type = n > 0 ? *m.p++ : -1;
	if(chatty)
		fprint(2, "ssh-agent: request %d, %d bytes\n", type, n);
	memset(&body, 0, sizeof body);
	switch(type){
	case SSH2_AGENTC_REQUEST_IDENTITIES:
		put1(&body, SSH2_AGENT_IDENTITIES_ANSWER);
		if(listkeys(&body) >= 0)
			break;
		goto Failure;
	case SSH2_AGENTC_SIGN_REQUEST:
		if(getstr(&m, &key) < 0 || getstr(&m, &data) < 0 || get4(&m, &flags) < 0)
			goto Failure;
		put1(&body, SSH2_AGENT_SIGN_RESPONSE);
		if(sign(&body, &key, &data) >= 0)
			break;
		if(chatty)
			fprint(2, "ssh-agent: sign: %r\n");
		goto Failure;
	default:
	Failure:
		body.p = body.bp;
		put1(&body, SSH_AGENT_FAILURE);
	}
	put4(r, body.p - body.bp);
	putn(r, body.bp, body.p - body.bp);
	free(body.bp);
}

static void
fsopen(Req *r)
{
	r->fid->aux = emalloc(sizeof(Conv));
	respond(r, nil);
}

static void
fswrite(Req *r)
{
	Conv *c;
	Msg out;
	uint len;
	uchar *p;

	c = r->fid->aux;
	if(c->nin + r->ifcall.count > Maxmsg){
		respond(r, "message too long");
		return;
	}
	c->in = realloc(c->in, c->nin + r->ifcall.count);
	if(c->in == nil)
		sysfatal("out of memory");
	memmove(c->in + c->nin, r->ifcall.data, r->ifcall.count);
	c->nin += r->ifcall.count;
	while(c->nin >= 4){
		p = c->in;
		len = (p[0]<<24)|(p[1]<<16)|(p[2]<<8)|p[3];
		if(len > Maxmsg){
			respond(r, "message too long");
			return;
		}
		if(c->nin < 4+len)
			break;
		memset(&out, 0, sizeof out);
		putn(&out, c->out, c->nout);
		answer(&out, p+4, len);
		free(c->out);
		c->out = out.bp;
		c->nout = out.p - out.bp;
		c->nin -= 4+len;
		memmove(c->in, c->in+4+len, c->nin);
	}
	r->ofcall.count = r->ifcall.count;
	respond(r, nil);
}

static void
fsread(Req *r)
{
	Conv *c;
	int n;

	c = r->fid->aux;
	n = r->ifcall.count;
	if(n > c->nout)
		n = c->nout;
	memmove(r->ofcall.data, c->out, n);
	c->nout -= n;
	memmove(c->out, c->out+n, c->nout);
	r->ofcall.count = n;
	respond(r, nil);
}

static void
fsdestroyfid(Fid *f)
{
	Conv *c;

	if((c = f->aux) == nil)
		return;
	free(c->in);
	free(c->out);
	free(c);
}

Srv fs = {
.open=	fsopen,
.read=	fsread,
.write=	fswrite,
.destroyfid=	fsdestroyfid,
};

static void
usage(void)
{
	fprint(2, "usage: auth/ssh-agent [-D] [-m mtpt] [-s srvname] [factotum]\n");
	exits("usage");
}

void
main(int argc, char *argv[])
{
	char *mtpt, *srvname;
	File *d;

	mtpt = "/mnt";
	srvname = "ssh-agent";
	ARGBEGIN{
	case 'D':
		chatty++;
		chatty9p++;
		break;
	case 'm':
		mtpt = EARGF(usage());
		break;
	case 's':
		srvname = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND
	if(argc > 1)
		usage();
	if(argc == 1)
		factotum = argv[0];
	fs.tree = alloctree(nil, nil, DMDIR|0700, nil);
	d = createfile(fs.tree->root, "ssh-agent", nil, DMDIR|0700, nil);
	closefile(createfile(d, "agent", nil, 0600, nil));
	closefile(d);
	postmountsrv(&fs, srvname, mtpt, MAFTER);
	exits(nil);
}
