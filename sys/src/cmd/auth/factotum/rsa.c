/*
 * RSA authentication.
 *
 * Old ssh client protocol:
 *	read public key
 *		if you don't like it, read another, repeat
 *	write challenge
 *	read response
 *
 * all numbers are hexadecimal biginits parsable with strtomp.
 *
 * Sign (PKCS #1 using hash=sha1, md5, sha256 or sha512,
 * from the start attributes, else the key's, else sha1)
 *	write hash(msg)
 *	read signature(hash(msg))
 *
 * Verify:
 *	write hash(msg)
 *	write signature(hash(msg))
 *	read ok or fail
 */

#include "dat.h"

enum {
	CHavePub,
	CHaveResp,
	VNeedHash,
	VNeedSig,
	VHaveResp,
	SNeedHash,
	SHaveResp,
	Maxphase,
};

static char *phasenames[] = {
[CHavePub]	"CHavePub",
[CHaveResp]	"CHaveResp",
[VNeedHash]	"VNeedHash",
[VNeedSig]	"VNeedSig",
[VHaveResp]	"VHaveResp",
[SNeedHash]	"SNeedHash",
[SHaveResp]	"SHaveResp",
};

struct State
{
	RSApriv *priv;
	mpint *resp;
	int off;
	Key *key;
	mpint *digest;
	uchar hashed[SHA2_512dlen];
	int nhashed;
	char *sigresp;
	char *hash;
};

typedef struct Hash Hash;
struct Hash
{
	char *name;
	DigestState *(*fun)(uchar*, ulong, uchar*, DigestState*);
	int dlen;
};

static Hash hashes[] = {
	"sha1",	sha1,	SHA1dlen,
	"md5",	md5,	MD5dlen,
	"sha256",	sha2_256,	SHA2_256dlen,
	"sha512",	sha2_512,	SHA2_512dlen,
};

static RSApriv*
readrsapriv(Key *k)
{
	char *a;
	RSApriv *priv;

	priv = rsaprivalloc();

	if((a=_strfindattr(k->attr, "ek"))==nil || (priv->pub.ek=strtomp(a, nil, 16, nil))==nil)
		goto Error;
	if((a=_strfindattr(k->attr, "n"))==nil || (priv->pub.n=strtomp(a, nil, 16, nil))==nil)
		goto Error;
	if(k->privattr == nil)		/* only public half */
		return priv;
	if((a=_strfindattr(k->privattr, "!p"))==nil || (priv->p=strtomp(a, nil, 16, nil))==nil)
		goto Error;
	if((a=_strfindattr(k->privattr, "!q"))==nil || (priv->q=strtomp(a, nil, 16, nil))==nil)
		goto Error;
	if((a=_strfindattr(k->privattr, "!kp"))==nil || (priv->kp=strtomp(a, nil, 16, nil))==nil)
		goto Error;
	if((a=_strfindattr(k->privattr, "!kq"))==nil || (priv->kq=strtomp(a, nil, 16, nil))==nil)
		goto Error;
	if((a=_strfindattr(k->privattr, "!c2"))==nil || (priv->c2=strtomp(a, nil, 16, nil))==nil)
		goto Error;
	if((a=_strfindattr(k->privattr, "!dk"))==nil || (priv->dk=strtomp(a, nil, 16, nil))==nil)
		goto Error;
	return priv;

Error:
	rsaprivfree(priv);
	return nil;
}

static int
rsainit(Proto*, Fsstate *fss)
{
	Keyinfo ki;
	State *s;
	char *role, *a;

	if((role = _strfindattr(fss->attr, "role")) == nil)
		return failure(fss, "rsa role not specified");
	if(strcmp(role, "client") == 0)
		fss->phase = CHavePub;
	else if(strcmp(role, "sign") == 0)
		fss->phase = SNeedHash;
	else if(strcmp(role, "verify") == 0)
		fss->phase = VNeedHash;
	else
		return failure(fss, "rsa role %s unimplemented", role);

	s = emalloc(sizeof *s);
	if((a = _strfindattr(fss->attr, "hash")) != nil){
		s->hash = estrdup(a);
		fss->attr = _delattr(fss->attr, "hash");
	}
	fss->phasename = phasenames;
	fss->maxphase = Maxphase;
	fss->ps = s;

	switch(fss->phase){
	case SNeedHash:
	case VNeedHash:
		mkkeyinfo(&ki, fss, nil);
		if(findkey(&s->key, &ki, nil) != RpcOk)
			return failure(fss, nil);
		/* signing needs private key */
		if(fss->phase == SNeedHash && s->key->privattr == nil)
			return failure(fss,
				"missing private half of key -- cannot sign");
	}
	return RpcOk;
}

static int
rsaread(Fsstate *fss, void *va, uint *n)
{
	RSApriv *priv;
	State *s;
	mpint *m;
	Keyinfo ki;
	int len, r;

	s = fss->ps;
	switch(fss->phase){
	default:
		return phaseerror(fss, "read");
	case CHavePub:
		if(s->key){
			closekey(s->key);
			s->key = nil;
		}
		mkkeyinfo(&ki, fss, nil);
		ki.skip = s->off;
		ki.noconf = 1;
		if(findkey(&s->key, &ki, nil) != RpcOk)
			return failure(fss, nil);
		s->off++;
		priv = s->key->priv;
		*n = snprint(va, *n, "%B", priv->pub.n);
		return RpcOk;
	case CHaveResp:
		*n = snprint(va, *n, "%B", s->resp);
		fss->phase = Established;
		return RpcOk;
	case SHaveResp:
		priv = s->key->priv;
		len = (mpsignif(priv->pub.n)+7)/8;
		if(len > *n)
			return failure(fss, "signature buffer too short");
		m = rsadecrypt(priv, s->digest, nil);
		r = mptobe(m, (uchar*)va, len, nil);
		if(r < len){
			memmove((uchar*)va+len-r, va, r);
			memset(va, 0, len-r);
		}
		*n = len;
		mpfree(m);
		fss->phase = Established;
		return RpcOk;
	case VHaveResp:
		*n = snprint(va, *n, "%s", s->sigresp == nil? "ok":
			"signature does not verify");
		fss->phase = Established;
		return RpcOk;
	}
}

static int
rsawrite(Fsstate *fss, void *va, uint n)
{
	RSApriv *priv;
	mpint *m;
	State *s;
	char *hash;
	int dlen;
	Hash *h;
	uchar asn1[128];

	s = fss->ps;
	switch(fss->phase){
	default:
		return phaseerror(fss, "write");
	case CHavePub:
		if(s->key == nil)
			return failure(fss, "no current key");
		switch(canusekey(fss, s->key)){
		case -1:
			return RpcConfirm;
		case 0:
			return failure(fss, "confirmation denied");
		case 1:
			break;
		}
		m = strtomp(va, nil, 16, nil);
		if(m == nil)
			return failure(fss, "invalid challenge value");
		m = rsadecrypt(s->key->priv, m, m);
		s->resp = m;
		fss->phase = CHaveResp;
		return RpcOk;
	case SNeedHash:
	case VNeedHash:
		hash = s->hash;
		if(hash == nil)
			hash = _strfindattr(s->key->attr, "hash");
		if(hash == nil)
			hash = "sha1";
		for(h = hashes; h < hashes+nelem(hashes); h++)
			if(strcmp(hash, h->name) == 0)
				break;
		if(h == hashes+nelem(hashes))
			return failure(fss, "unknown hash function %s", hash);
		if(n != h->dlen)
			return failure(fss, "hash length %d should be %d",
				n, h->dlen);
		memmove(s->hashed, va, n);
		s->nhashed = n;
		if(fss->phase == VNeedHash){
			fss->phase = VNeedSig;
			return RpcOk;
		}
		dlen = asn1encodedigest(h->fun, va, asn1, sizeof asn1);
		if(dlen < 0)
			return failure(fss, nil);
		priv = s->key->priv;
		s->digest = pkcs1padbuf(asn1, dlen, priv->pub.n, 1);
		if(s->digest == nil)
			return failure(fss, nil);
		fss->phase = SHaveResp;
		return RpcOk;
	case VNeedSig:
		priv = s->key->priv;
		s->sigresp = X509rsaverifydigest(va, n, s->hashed, s->nhashed, &priv->pub);
		fss->phase = VHaveResp;
		return RpcOk;
	}
}

static void
rsaclose(Fsstate *fss)
{
	State *s;

	s = fss->ps;
	if(s->key)
		closekey(s->key);
	if(s->resp)
		mpfree(s->resp);
	if(s->digest)
		mpfree(s->digest);
	free(s->hash);
	free(s);
}

static int
rsaaddkey(Key *k, int before)
{
	fmtinstall('B', mpfmt);

	if((k->priv = readrsapriv(k)) == nil){
		werrstr("malformed key data");
		return -1;
	}
	return replacekey(k, before);
}

static void
rsaclosekey(Key *k)
{
	rsaprivfree(k->priv);
}

Proto rsa = {
.name=	"rsa",
.init=		rsainit,
.write=	rsawrite,
.read=	rsaread,
.close=	rsaclose,
.addkey=	rsaaddkey,
.closekey=	rsaclosekey,
};
