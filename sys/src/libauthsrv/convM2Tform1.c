#include <u.h>
#include <libc.h>
#include <authsrv.h>
#include <libsec.h>

int
convM2Tform1(char *ap, Ticket *f, char *key)
{
	uchar buf[TICKETLENFORM1], *p;
	Chachastate s;
	int n, num;

	memset(f, 0, sizeof *f);
	if(memcmp(ap, "form1 Ts", FORM1SIGLEN) == 0)
		num = AuthTs;
	else if(memcmp(ap, "form1 Tc", FORM1SIGLEN) == 0)
		num = AuthTc;
	else{
		werrstr("not a form1 ticket");
		return -1;
	}
	memmove(buf, ap, TICKETLENFORM1);
	n = TICKETLENFORM1-FORM1NONCELEN-FORM1AUTHTAGLEN;
	setupChachastate(&s, (uchar*)key, ChachaKeylen, buf, ChachaIVlen, 20);
	if(ccpoly_decrypt(buf+FORM1NONCELEN, n, nil, 0, buf+FORM1NONCELEN+n, &s) < 0){
		memset(buf, 0, sizeof buf);
		werrstr("ticket not authentic");
		return -1;
	}
	p = buf+FORM1NONCELEN;
	f->num = num;
	memmove(f->chal, p, CHALLEN);
	p += CHALLEN;
	memmove(f->cuid, p, ANAMELEN);
	f->cuid[ANAMELEN-1] = 0;
	p += ANAMELEN;
	memmove(f->suid, p, ANAMELEN);
	f->suid[ANAMELEN-1] = 0;
	p += ANAMELEN;
	memmove(f->key, p, NONCELEN);
	memset(buf, 0, sizeof buf);
	return 0;
}
