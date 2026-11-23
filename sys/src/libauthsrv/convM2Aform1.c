#include <u.h>
#include <libc.h>
#include <authsrv.h>
#include <libsec.h>

int
convM2Aform1(char *ap, Authenticator *f, char *key)
{
	uchar buf[AUTHENTLENFORM1], *p;
	Chachastate s;
	int n, num;

	memset(f, 0, sizeof *f);
	if(memcmp(ap, "form1 Ac", FORM1SIGLEN) == 0)
		num = AuthAc;
	else if(memcmp(ap, "form1 As", FORM1SIGLEN) == 0)
		num = AuthAs;
	else{
		werrstr("not a form1 authenticator");
		return -1;
	}
	memmove(buf, ap, AUTHENTLENFORM1);
	n = AUTHENTLENFORM1-FORM1NONCELEN-FORM1AUTHTAGLEN;
	setupChachastate(&s, (uchar*)key, ChachaKeylen, buf, ChachaIVlen, 20);
	if(ccpoly_decrypt(buf+FORM1NONCELEN, n, nil, 0, buf+FORM1NONCELEN+n, &s) < 0){
		memset(buf, 0, sizeof buf);
		werrstr("authenticator not authentic");
		return -1;
	}
	p = buf+FORM1NONCELEN;
	f->num = num;
	memmove(f->chal, p, CHALLEN);
	p += CHALLEN;
	memmove(f->rand, p, NONCELEN);
	memset(buf, 0, sizeof buf);
	return 0;
}
