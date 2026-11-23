#include <u.h>
#include <libc.h>
#include <thread.h>
#include <acme.h>
#include "impl.h"

int
_winguarded(char *name)
{
	return strcmp(name, "body") == 0 || strcmp(name, "data") == 0 || strcmp(name, "xdata") == 0
		|| strcmp(name, "style") == 0 || strcmp(name, "rdevent") == 0;
}

int
winseq(Win *w)
{
	char buf[32];
	int fd, n;

	if((fd = _winopen(w, "seq", OREAD)) < 0)
		return -1;
	n = read(fd, buf, sizeof buf-1);
	if(n <= 0){
		if(n == 0)
			werrstr("eof");
		werrstr("read seq: %r");
		close(fd);
		return -1;
	}
	close(fd);
	buf[n] = 0;
	return atoi(buf);
}

int
winseqopen(Win *w, char *file, int seq, int mode)
{
	char buf[100], s[16];
	int fd, sfd, n;

	if(_winpath(w, "seq", buf, sizeof buf) == nil)
		return -1;
	n = snprint(s, sizeof s, "%d", seq);
	fd = -1;
	qlock(&w->seqlk);
	if((sfd = open(buf, OWRITE)) < 0)
		werrstr("open %s: %r", buf);
	else if(write(sfd, s, n) != n)
		werrstr("write %s: %r", buf);
	else if(_winpath(w, file, buf, sizeof buf) != nil && (fd = open(buf, mode)) < 0)
		werrstr("open %s: %r", buf);
	if(sfd >= 0)
		close(sfd);
	qunlock(&w->seqlk);
	return fd;
}

int
winseqwrite(Win *w, int seq, char *addr, void *a, int n)
{
	char *s;
	int fd, m, k, j;

	if(addr != nil && winaddr(w, "%s", addr) < 0)
		return -1;
	if((fd = winseqopen(w, "data", seq, OWRITE|OCEXEC)) < 0)
		return -1;
	s = a;
	m = 0;
	do{
		k = n-m < 4096 ? n-m : 4096;
		for(j = 0; j < UTFmax-1 && m+k-j < n && (s[m+k-j]&0xC0) == 0x80; j++)
			;
		if(m+k-j >= n || (s[m+k-j]&0xC0) != 0x80)
			k -= j;
		if(write(fd, s+m, k) != k){
			werrstr("write data: %r");
			close(fd);
			return -1;
		}
		if(m == 0 && k < n && winctl(w, "nomark") < 0){
			werrstr("nomark: %r");
			close(fd);
			return -1;
		}
		m += k;
	}while(m < n);
	close(fd);
	return n;
}
