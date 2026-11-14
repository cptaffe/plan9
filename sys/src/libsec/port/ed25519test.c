#include "ed25519.c"

/*
 * test vectors from RFC8032, 7.1, tests 1-3, and RFC7748, 5.2 and 6.1; then random keys,
 * messages and points, as many as argv[1] says
 */
typedef struct Sigtest Sigtest;
struct Sigtest {
	char	*sk;
	char	*pk;
	char	*msg;
	char	*sig;
};

Sigtest sigtests[] = {
	"9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60",
	"d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a",
	"",
	"e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e06522490155"
	"5fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b",

	"4ccd089b28ff96da9db6c346ec114e0f5b8a319f35aba624da8cf6ed4fb8a6fb",
	"3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c",
	"72",
	"92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da"
	"085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c00",

	"c5aa8df43f9f837bedb7442f31dcb7b166d38535076f094b85ce3a2e0b4458f7",
	"fc51cd8e6218a1a38da47ed00230f0580816ed13ba3303ac5deb911548908025",
	"af82",
	"6291d657deec24024827e69c3abe01a30ce548a284743a445e3680d7db5ac3ac"
	"18ff9b538d16f290ae67f760984dc6594a7c15e9716ed28dc027beceea1ec40a",
};

typedef struct Dhtest Dhtest;
struct Dhtest {
	char	*k;
	char	*u;
	char	*out;
};

Dhtest dhtests[] = {
	"a546e36bf0527c9d3b16154b82465edd62144c0ac1fc5a18506a2244ba449ac4",
	"e6db6867583030db3594c1a424b15f7c726624ec26b3353b10a903a6d0ab1c4c",
	"c3da55379de9c6908e94ea4df28d084f32eccf03491c71f754b4075577a28552",

	"4b66e9d4d1b4673c5ad22691957d6af5c11b6421e0ea01d42ca4169e7918ba0d",
	"e5210f12786811d3f4b7959d0538ae2c31dbe7106fc03c3efc4cd549c715a493",
	"95cbde9476e8907d7aade45cb4b873f88b595a68799fa152e6f8f7647aac7957",

	"77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a",
	"0900000000000000000000000000000000000000000000000000000000000000",
	"8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a",

	"5dab087e624a8a4b79e17f8b83800ee66f3bb1292618b6fd1c2f8b27ff88e0eb",
	"0900000000000000000000000000000000000000000000000000000000000000",
	"de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f",

	"77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a",
	"de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f",
	"4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742",
};

static int fails;

static int
hex(uchar *b, char *s)
{
	return dec16(b, strlen(s)/2, s, strlen(s));
}

static void
check(char *what, int i, uchar *got, char *want, int n)
{
	uchar w[64];

	hex(w, want);
	if(memcmp(got, w, n) != 0){
		print("%s %d: %.*H, not %s\n", what, i, n, got, want);
		fails++;
	}
}

static void
randoms(int nr)
{
	uchar sk[32], pk[32], m[64], sig[64], k[32], e[32], e2[32];
	int i, n, b;
	Pt p, q;

	for(i = 0; i < nr; i++){
		genrandom(sk, sizeof sk);
		n = nrand(sizeof m);
		genrandom(m, n);
		ed25519pubkey(pk, sk);
		ed25519sign(sig, m, n, sk, pk);
		if(!ed25519verify(sig, m, n, pk)){
			print("random %d: refused\n", i);
			fails++;
		}
		b = nrand(8*sizeof sig);
		sig[b/8] ^= 1<<(b%8);
		if(ed25519verify(sig, m, n, pk)){
			print("random %d: took bit %d changed\n", i, b);
			fails++;
		}
		genrandom(k, sizeof k);
		ptmul(&p, k, &base);
		ptencode(e, &p);
		if(ptdecode(&q, e) < 0){
			print("random %d: point %.*H not decoded\n", i, 32, e);
			fails++;
			continue;
		}
		ptencode(e2, &q);
		if(memcmp(e, e2, 32) != 0){
			print("random %d: point %.*H decoded as %.*H\n", i, 32, e, 32, e2);
			fails++;
		}
	}
}

void
main(int argc, char **argv)
{
	uchar sk[32], pk[32], m[8], sig[64], k[32], u[32], z[32], x[32], y[32], z2[32];
	int i, n;
	Sigtest *t;
	Dhtest *d;

	fmtinstall('H', encodefmt);
	for(i = 0; i < nelem(sigtests); i++){
		t = &sigtests[i];
		hex(sk, t->sk);
		n = hex(m, t->msg);
		ed25519pubkey(pk, sk);
		check("ed25519pubkey", i, pk, t->pk, 32);
		ed25519sign(sig, m, n, sk, pk);
		check("ed25519sign", i, sig, t->sig, 64);
		if(!ed25519verify(sig, m, n, pk)){
			print("ed25519verify %d: refused\n", i);
			fails++;
		}
		sig[i] ^= 1;
		if(ed25519verify(sig, m, n, pk)){
			print("ed25519verify %d: took a changed signature\n", i);
			fails++;
		}
	}
	for(i = 0; i < nelem(dhtests); i++){
		d = &dhtests[i];
		hex(k, d->k);
		hex(u, d->u);
		x25519(z, k, u);
		check("x25519", i, z, d->out, 32);
	}
	curve25519_dh_new(x, y);
	curve25519_dh_new(k, u);
	if(!curve25519_dh_finish(x, u, z) || !curve25519_dh_finish(k, y, z2) || memcmp(z, z2, 32) != 0){
		print("curve25519_dh: no shared secret\n");
		fails++;
	}
	memset(u, 0, 32);
	curve25519_dh_new(x, y);
	if(curve25519_dh_finish(x, u, z)){
		print("curve25519_dh: took a zero key\n");
		fails++;
	}
	randoms(argc > 1 ? atoi(argv[1]) : 100);
	if(fails)
		exits("fail");
	print("ok\n");
	exits(nil);
}
