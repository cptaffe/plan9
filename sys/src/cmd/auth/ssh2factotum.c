/*
 * ssh2factotum: an OpenSSH ed25519 private key (openssh-key-v1, not encrypted), as ssh-keygen
 * writes it, as a factotum key: key proto=ed25519 pk=… comment=… !sk=…
 */
#include <u.h>
#include <libc.h>
#include <bio.h>

typedef struct Msg Msg;
struct Msg
{
	uchar	*p;
	uchar	*ep;
};

static char magic[] = "openssh-key-v1";

static void
usage(void)
{
	fprint(2, "usage: auth/ssh2factotum [file]\n");
	exits("usage");
}

static uint
get4(Msg *m)
{
	uint n;

	if(m->p+4 > m->ep)
		sysfatal("key too short");
	n = (m->p[0]<<24)|(m->p[1]<<16)|(m->p[2]<<8)|m->p[3];
	m->p += 4;
	return n;
}

static Msg
getstr(Msg *m)
{
	Msg s;
	uint n;

	n = get4(m);
	if(n > m->ep - m->p)
		sysfatal("key too short");
	s.p = m->p;
	s.ep = m->p+n;
	m->p += n;
	return s;
}

static int
streq(Msg s, char *t)
{
	return s.ep - s.p == strlen(t) && memcmp(s.p, t, s.ep - s.p) == 0;
}

static char*
hex(uchar *b, int n)
{
	char *s;
	int i;

	s = malloc(2*n+1);
	if(s == nil)
		sysfatal("out of memory");
	for(i = 0; i < n; i++)
		sprint(s+2*i, "%.2ux", b[i]);
	return s;
}

void
main(int argc, char *argv[])
{
	Biobuf *b;
	char *l, *t;
	uchar *key, *pk;
	int inkey, n, nt;
	Msg m, s, c;
	char *comment;

	ARGBEGIN{
	default:
		usage();
	}ARGEND
	if(argc > 1)
		usage();
	if(argc == 1)
		b = Bopen(argv[0], OREAD);
	else
		b = Bfdopen(0, OREAD);
	if(b == nil)
		sysfatal("open: %r");
	t = nil;
	nt = 0;
	inkey = 0;
	while((l = Brdstr(b, '\n', 1)) != nil){
		if(strstr(l, "-----BEGIN OPENSSH PRIVATE KEY-----") != nil)
			inkey = 1;
		else if(strstr(l, "-----END OPENSSH PRIVATE KEY-----") != nil)
			inkey = 0;
		else if(inkey){
			n = strlen(l);
			t = realloc(t, nt+n+1);
			if(t == nil)
				sysfatal("out of memory");
			memmove(t+nt, l, n);
			nt += n;
		}
		free(l);
	}
	if(t == nil)
		sysfatal("no OPENSSH PRIVATE KEY");
	key = malloc(nt);
	if(key == nil)
		sysfatal("out of memory");
	n = dec64(key, nt, t, nt);
	if(n < sizeof magic || memcmp(key, magic, sizeof magic) != 0)
		sysfatal("not an openssh-key-v1 key");
	m.p = key + sizeof magic;
	m.ep = key + n;
	if(!streq(getstr(&m), "none") || !streq(getstr(&m), "none"))
		sysfatal("key is encrypted: ssh-keygen -p -N '' -f it first");
	getstr(&m);
	if(get4(&m) != 1)
		sysfatal("not one key");
	getstr(&m);
	s = getstr(&m);
	if(get4(&s) != get4(&s))
		sysfatal("bad check bytes");
	if(!streq(getstr(&s), "ssh-ed25519"))
		sysfatal("not an ed25519 key");
	c = getstr(&s);
	if(c.ep - c.p != 32)
		sysfatal("bad public key");
	pk = c.p;
	c = getstr(&s);
	if(c.ep - c.p != 64 || memcmp(c.p+32, pk, 32) != 0)
		sysfatal("bad secret key");
	key = c.p;
	c = getstr(&s);
	comment = smprint("%.*s", (int)(c.ep - c.p), (char*)c.p);
	quotefmtinstall();
	if(*comment)
		print("key proto=ed25519 pk=%s comment=%q !sk=%s\n", hex(pk, 32), comment, hex(key, 32));
	else
		print("key proto=ed25519 pk=%s !sk=%s\n", hex(pk, 32), hex(key, 32));
	exits(nil);
}
