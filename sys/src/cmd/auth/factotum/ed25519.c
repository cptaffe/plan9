/*
 * ed25519 signatures (RFC 8032), for ssh-agent.
 * A key is proto=ed25519 pk=hex !sk=hex: the 32-byte public key
 * and the 32-byte secret it is made from.
 *
 * Sign:
 *	write msg
 *	read signature(msg), 64 bytes
 */

#include "dat.h"

enum {
	SNeedMsg,
	SHaveSig,
	Maxphase,
};

static char *phasenames[] = {
[SNeedMsg]	"SNeedMsg",
[SHaveSig]	"SHaveSig",
};

typedef struct Edkey Edkey;
struct Edkey
{
	uchar	sk[32];
	uchar	pk[32];
};

struct State
{
	Key *key;
	uchar sig[64];
};

static int
hex32(uchar *b, char *s)
{
	return s != nil && strlen(s) == 64 && dec16(b, 32, s, 64) == 32;
}

static int
edinit(Proto*, Fsstate *fss)
{
	Keyinfo ki;
	State *s;
	char *role;

	if((role = _strfindattr(fss->attr, "role")) == nil)
		return failure(fss, "ed25519 role not specified");
	if(strcmp(role, "sign") != 0)
		return failure(fss, "ed25519 role %s unimplemented", role);
	s = emalloc(sizeof *s);
	fss->phasename = phasenames;
	fss->maxphase = Maxphase;
	fss->ps = s;
	fss->phase = SNeedMsg;
	mkkeyinfo(&ki, fss, nil);
	if(findkey(&s->key, &ki, nil) != RpcOk)
		return failure(fss, nil);
	return RpcOk;
}

static int
edwrite(Fsstate *fss, void *va, uint n)
{
	State *s;
	Edkey *k;

	s = fss->ps;
	switch(fss->phase){
	default:
		return phaseerror(fss, "write");
	case SNeedMsg:
		k = s->key->priv;
		ed25519sign(s->sig, va, n, k->sk, k->pk);
		fss->phase = SHaveSig;
		return RpcOk;
	}
}

static int
edread(Fsstate *fss, void *va, uint *n)
{
	State *s;

	s = fss->ps;
	switch(fss->phase){
	default:
		return phaseerror(fss, "read");
	case SHaveSig:
		if(*n < sizeof s->sig)
			return toosmall(fss, sizeof s->sig);
		memmove(va, s->sig, sizeof s->sig);
		*n = sizeof s->sig;
		fss->phase = Established;
		return RpcOk;
	}
}

static void
edclose(Fsstate *fss)
{
	State *s;

	s = fss->ps;
	if(s->key)
		closekey(s->key);
	memset(s->sig, 0, sizeof s->sig);
	free(s);
}

static int
edaddkey(Key *k, int before)
{
	Edkey *e;
	uchar pk[32];

	e = emalloc(sizeof *e);
	if(!hex32(e->pk, _strfindattr(k->attr, "pk"))
	|| !hex32(e->sk, _strfindattr(k->privattr, "!sk"))){
		memset(e, 0, sizeof *e);
		free(e);
		werrstr("malformed key data");
		return -1;
	}
	ed25519pubkey(pk, e->sk);
	if(memcmp(pk, e->pk, sizeof pk) != 0){
		memset(pk, 0, sizeof pk);
		memset(e, 0, sizeof *e);
		free(e);
		werrstr("pk is not !sk's public key");
		return -1;
	}
	k->priv = e;
	return replacekey(k, before);
}

static void
edclosekey(Key *k)
{
	memset(k->priv, 0, sizeof(Edkey));
	free(k->priv);
}

Proto ed25519 = {
.name=	"ed25519",
.init=		edinit,
.write=	edwrite,
.read=	edread,
.close=	edclose,
.addkey=	edaddkey,
.closekey=	edclosekey,
};
