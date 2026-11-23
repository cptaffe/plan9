#include <u.h>
#include <libc.h>
#include <authsrv.h>
#include <libsec.h>

int
convA2Mform1(Authenticator *f, char *ap, char *key)
{
	static u32int counter;
	Chachastate s;
	uchar *p;
	char *sig;
	u32int c;
	int n;

	switch(f->num){
	case AuthAc:
		sig = "form1 Ac";
		break;
	case AuthAs:
		sig = "form1 As";
		break;
	default:
		werrstr("not a form1 authenticator");
		return -1;
	}
	p = (uchar*)ap;
	memmove(p, sig, FORM1SIGLEN);
	p += FORM1SIGLEN;
	c = counter++;
	p[0] = c;
	p[1] = c>>8;
	p[2] = c>>16;
	p[3] = c>>24;
	p += FORM1CNTLEN;
	memmove(p, f->chal, CHALLEN);
	p += CHALLEN;
	memmove(p, f->rand, NONCELEN);
	n = CHALLEN+NONCELEN;
	setupChachastate(&s, (uchar*)key, ChachaKeylen, (uchar*)ap, ChachaIVlen, 20);
	ccpoly_encrypt((uchar*)ap+FORM1NONCELEN, n, nil, 0, (uchar*)ap+FORM1NONCELEN+n, &s);
	return FORM1NONCELEN+n+FORM1AUTHTAGLEN;
}
