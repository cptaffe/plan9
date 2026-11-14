/*
 * webauthn - a passkey's sign-in to the site that served the page,
 * bound to the session's X25519 keys (doc/auth.md).
 * The ceremony is the browser's, through /dev/webauthn.
 * Messages are text, each nul terminated; binary fields are
 * unpadded base64url.
 *
 * Client protocol:
 *	write user cpub [new]
 *	read get rpid nonce spub credid...	or	create rpid nonce spub userid
 *	write credid authdata clientdata signature	or	credid authdata clientdata spki alg mac
 *	read ok user	or	error text
 *
 * The passkey signs T = SHA-256("p9web webauthn" 0 dom 0 user 0 cpub spub nonce),
 * which factotum computes; the secret is HKDF-SHA256 of the X25519 secret, salt T.
 * new asks to make a passkey by the key's invite, which is never sent: the create's
 * mac is HMAC-SHA256 under SHA-256 of the invite of T, credid, authdata and clientdata.
 * The passkey's user id is factotum's, not the server's; once the passkey is made, the
 * key loses its invite.
 *
 * There is no server protocol: the site's is not factotum's.
 */

#include "dat.h"

enum
{
	Maxmsg = 4000,
	Nb64 = 45,
};

static char info[] = "p9web webauthn secret";

struct State
{
	Key *key;
	char *user;
	char *invite;
	char *dom;
	uchar x[32];
	uchar cpub[32];
	uchar secret[32];
	uchar t[SHA2_256dlen];
	int created;
	char ans[Maxmsg];
	int nans;
};

enum
{
	CHaveUser,
	CNeedAsk,
	CHaveAnswer,
	CNeedOK,

	Maxphase,
};

static char *phasenames[Maxphase] =
{
[CHaveUser]	"CHaveUser",
[CNeedAsk]	"CNeedAsk",
[CHaveAnswer]	"CHaveAnswer",
[CNeedOK]	"CNeedOK",
};

static int	b64uenc(char*, int, uchar*, int);

static int
webauthninit(Proto *p, Fsstate *fss)
{
	int iscli, ret;
	Key *k;
	Keyinfo ki;
	State *s;

	if((iscli = isclient(_strfindattr(fss->attr, "role"))) < 0)
		return failure(fss, nil);
	if(!iscli)
		return failure(fss, "webauthn has no server role");
	ret = findkey(&k, mkkeyinfo(&ki, fss, nil), "%s", p->keyprompt);
	if(ret != RpcOk)
		return ret;
	setattrs(fss->attr, k->attr);
	s = emalloc(sizeof *s);
	s->key = k;
	s->user = _strfindattr(k->attr, "user");
	s->invite = _strfindattr(k->privattr, "!invite");
	s->dom = _strfindattr(k->attr, "dom");
	if(s->user == nil || s->dom == nil){
		closekey(k);
		free(s);
		return failure(fss, "webauthn key has no user or dom");
	}
	curve25519_dh_new(s->x, s->cpub);
	fss->phasename = phasenames;
	fss->maxphase = Maxphase;
	fss->phase = CHaveUser;
	fss->ps = s;
	return RpcOk;
}

static void
webauthnclose(Fsstate *fss)
{
	State *s;

	s = fss->ps;
	if(s->key)
		closekey(s->key);
	memset(s->x, 0, sizeof s->x);
	memset(s->secret, 0, sizeof s->secret);
	free(s);
}

static int
webauthnread(Fsstate *fss, void *a, uint *n)
{
	int m;
	State *s;

	s = fss->ps;
	switch(fss->phase){
	default:
		return phaseerror(fss, "read");

	case CHaveUser:
		m = strlen(s->user)+1+Nb64;
		if(s->invite)
			m += 4;
		if(*n < m)
			return toosmall(fss, m);
		strcpy(a, s->user);
		strcat(a, " ");
		b64uenc((char*)a+strlen(a), Nb64, s->cpub, sizeof s->cpub);
		if(s->invite)
			strcat(a, " new");
		*n = strlen(a)+1;
		fss->phase = CNeedAsk;
		return RpcOk;

	case CHaveAnswer:
		m = s->nans+1;
		if(*n < m)
			return toosmall(fss, m);
		memmove(a, s->ans, s->nans);
		((char*)a)[s->nans] = '\0';
		*n = m;
		fss->phase = CNeedOK;
		return RpcOk;
	}
}

static int
b64uenc(char *out, int lim, uchar *in, int n)
{
	int i, m;

	m = enc64(out, lim, in, n);
	while(m > 0 && out[m-1] == '=')
		out[--m] = '\0';
	for(i = 0; i < m; i++)
		if(out[i] == '+')
			out[i] = '-';
		else if(out[i] == '/')
			out[i] = '_';
	return m;
}

static int
b64udec32(uchar out[32], char *in)
{
	char buf[Nb64];
	int i, n;

	n = strlen(in);
	if(n != 43)
		return -1;
	for(i = 0; i < n; i++){
		if(in[i] == '-')
			buf[i] = '+';
		else if(in[i] == '_')
			buf[i] = '/';
		else if(in[i] >= 'A' && in[i] <= 'Z' || in[i] >= 'a' && in[i] <= 'z' || in[i] >= '0' && in[i] <= '9')
			buf[i] = in[i];
		else
			return -1;
	}
	return dec64(out, 32, buf, n) == 32 ? 0 : -1;
}

static int
msg(Fsstate *fss, char *a, uint n)
{
	if(n == 0 || a[n-1] != '\0'){
		if(n >= Maxmsg)
			return failure(fss, Etoolarge);
		return toosmall(fss, Maxmsg);
	}
	if(strncmp(a, "error", 5) == 0 && (a[5] == ' ' || a[5] == '\0'))
		return failure(fss, "%s", a[5] ? a+6 : "the server refused");
	return RpcOk;
}

static int
ceremony(Fsstate *fss, State *s, char *verb, char *rpid, char *chal, char *rest)
{
	int fd, m;

	fd = open("/dev/webauthn", ORDWR);
	if(fd < 0)
		return failure(fss, nil);
	if(fprint(fd, "%s %s %s %s%s%s", verb, s->user, rpid, chal, rest[0] ? " " : "", rest) < 0){
		close(fd);
		return failure(fss, nil);
	}
	seek(fd, 0, 0);
	s->nans = 0;
	while(s->nans < sizeof s->ans && (m = read(fd, s->ans+s->nans, sizeof s->ans-s->nans)) > 0)
		s->nans += m;
	close(fd);
	if(m < 0)
		return failure(fss, nil);
	if(s->nans == sizeof s->ans)
		return failure(fss, Etoolarge);
	while(s->nans > 0 && s->ans[s->nans-1] == '\n')
		s->nans--;
	return RpcOk;
}

static int
b64udec(uchar *out, int lim, char *in, int n)
{
	char *buf;
	int i, m;

	buf = emalloc(n+1);
	for(i = 0; i < n; i++){
		if(in[i] == '-')
			buf[i] = '+';
		else if(in[i] == '_')
			buf[i] = '/';
		else if(in[i] >= 'A' && in[i] <= 'Z' || in[i] >= 'a' && in[i] <= 'z' || in[i] >= '0' && in[i] <= '9')
			buf[i] = in[i];
		else{
			free(buf);
			return -1;
		}
	}
	m = dec64(out, lim, buf, n);
	free(buf);
	return m;
}

static int
mac(Fsstate *fss, State *s)
{
	char *f[3], *p, *e;
	uchar k[SHA2_256dlen], d[SHA2_256dlen], *b;
	DigestState *ds;
	int i, n;

	p = s->ans;
	e = s->ans+s->nans;
	for(i = 0; i < 3; i++){
		f[i] = p;
		while(p < e && *p != ' ')
			p++;
		if(p == e)
			return failure(fss, "webauthn: the device's answer is short");
		p++;
	}
	if(s->nans+1+Nb64 > sizeof s->ans)
		return failure(fss, Etoolarge);
	sha2_256((uchar*)s->invite, strlen(s->invite), k, nil);
	b = emalloc(s->nans);
	ds = hmac_sha2_256(s->t, sizeof s->t, k, sizeof k, nil, nil);
	for(i = 0; i < 3; i++){
		n = b64udec(b, s->nans, f[i], strchr(f[i], ' ')-f[i]);
		if(n < 0){
			free(b);
			memset(k, 0, sizeof k);
			free(ds);
			return failure(fss, "webauthn: the device's answer is not base64url");
		}
		ds = hmac_sha2_256(b, n, k, sizeof k, i == 2 ? d : nil, ds);
	}
	free(b);
	memset(k, 0, sizeof k);
	s->ans[s->nans++] = ' ';
	s->nans += b64uenc(s->ans+s->nans, Nb64, d, sizeof d);
	return RpcOk;
}

static int
ask(Fsstate *fss, State *s, char *a)
{
	char *f[4], chal[Nb64], uid[Nb64], *p, *rest;
	uchar id[32];
	uchar nonce[32], spub[32], z[32], t[SHA2_256dlen];
	DigestState *ds;
	int i, ret;

	p = a;
	for(i = 0; i < 4; i++){
		if(p == nil)
			return failure(fss, Easproto);
		f[i] = p;
		if(p = strchr(p, ' '))
			*p++ = '\0';
	}
	rest = p ? p : "";
	if(strcmp(f[0], "get") != 0 && strcmp(f[0], "create") != 0)
		return failure(fss, Easproto);
	if(strcmp(f[0], "create") == 0){
		if(s->invite == nil)
			return failure(fss, "the server asks to make a passkey, and no invite was sent");
		if(rest[0] == '\0' || strchr(rest, ' '))
			return failure(fss, Easproto);
		genrandom(id, sizeof id);
		b64uenc(uid, sizeof uid, id, sizeof id);
		rest = uid;
	}
	if(strcmp(f[1], s->dom) != 0)
		return failure(fss, "the server's rpid %q is not the key's dom %q", f[1], s->dom);
	if(b64udec32(nonce, f[2]) < 0 || b64udec32(spub, f[3]) < 0)
		return failure(fss, Easproto);

	ds = sha2_256((uchar*)"p9web webauthn", 15, nil, nil);
	ds = sha2_256((uchar*)s->dom, strlen(s->dom)+1, nil, ds);
	ds = sha2_256((uchar*)s->user, strlen(s->user)+1, nil, ds);
	ds = sha2_256(s->cpub, sizeof s->cpub, nil, ds);
	ds = sha2_256(spub, sizeof spub, nil, ds);
	sha2_256(nonce, sizeof nonce, t, ds);
	memmove(s->t, t, sizeof t);
	if(curve25519_dh_finish(s->x, spub, z) == 0){
		memset(z, 0, sizeof z);
		return failure(fss, "the server's key gives no shared secret");
	}
	hkdf_x(t, sizeof t, (uchar*)info, sizeof info-1, z, sizeof z,
		s->secret, sizeof s->secret, hmac_sha2_256, SHA2_256dlen);
	memset(z, 0, sizeof z);

	b64uenc(chal, sizeof chal, t, sizeof t);
	if((ret = ceremony(fss, s, f[0], f[1], chal, rest)) != RpcOk)
		return ret;
	if(strcmp(f[0], "create") == 0){
		if((ret = mac(fss, s)) != RpcOk)
			return ret;
		s->created = 1;
	}
	fss->phase = CHaveAnswer;
	return RpcOk;
}

static int
webauthnwrite(Fsstate *fss, void *va, uint n)
{
	char *a;
	int ret;
	State *s;

	s = fss->ps;
	a = va;
	switch(fss->phase){
	default:
		return phaseerror(fss, "write");

	case CNeedAsk:
		if((ret = msg(fss, a, n)) != RpcOk)
			return ret;
		return ask(fss, s, a);

	case CNeedOK:
		if((ret = msg(fss, a, n)) != RpcOk)
			return ret;
		if(strncmp(a, "ok ", 3) != 0 || strcmp(a+3, s->user) != 0)
			return failure(fss, Easproto);
		if(s->created){
			s->key->privattr = _delattr(s->key->privattr, "!invite");
			s->invite = nil;
		}
		fss->ai.cuid = s->user;
		fss->ai.suid = s->user;
		fss->ai.secret = s->secret;
		fss->ai.nsecret = sizeof s->secret;
		fss->haveai = 1;
		fss->phase = Established;
		return RpcOk;
	}
}

Proto webauthn =
{
.name=	"webauthn",
.init=		webauthninit,
.write=	webauthnwrite,
.read=	webauthnread,
.close=	webauthnclose,
.addkey=	replacekey,
.keyprompt=	"user? dom?",
};
