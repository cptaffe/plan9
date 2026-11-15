#include "lib.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include "sys9.h"
#include "dir.h"

static int
same(Dir *a, Dir *b)
{
	return a->qid.path == b->qid.path && a->type == b->type && a->dev == b->dev;
}

static int
copy(char *from, char *to, Dir *d)
{
	int ffd, tfd, n;
	char buf[8192];

	if((ffd = _OPEN(from, 0)) < 0){
		_syserrno();
		return -1;
	}
	if((tfd = _CREATE(to, 1, d->mode)) < 0){
		_syserrno();
		_CLOSE(ffd);
		return -1;
	}
	while((n = _READ(ffd, buf, sizeof buf)) > 0)
		if(_WRITE(tfd, buf, n) != n){
			n = -1;
			break;
		}
	if(n < 0)
		_syserrno();
	_CLOSE(ffd);
	_CLOSE(tfd);
	if(n < 0){
		_REMOVE(to);
		return -1;
	}
	if(_REMOVE(from) < 0){
		_syserrno();
		return -1;
	}
	return 0;
}

static int
parent(const char *to, char *t)
{
	char *p;
	Dir *pd;
	int n;

	if(t == to)
		p = strdup(".");
	else if(t-to == 1)
		p = strdup("/");
	else if((p = malloc(t-to)) != nil){
		memmove(p, to, t-to-1);
		p[t-to-1] = 0;
	}
	if(p == nil){
		errno = ENOMEM;
		return -1;
	}
	n = 0;
	if((pd = _dirstat(p)) == nil){
		_syserrno();
		n = -1;
	}else if(!(pd->mode&DMDIR)){
		errno = ENOTDIR;
		n = -1;
	}
	free(pd);
	free(p);
	return n;
}

int
rename(const char *from, const char *to)
{
	int n, sd;
	char *f, *t;
	Dir *d, *td, nd;

	if((d = _dirstat(from)) == nil){
		_syserrno();
		return -1;
	}
	f = strrchr(from, '/');
	t = strrchr(to, '/');
	f = f? f+1 : (char*)from;
	t = t? t+1 : (char*)to;
	sd = f-from == t-to && strncmp(from, to, f-from) == 0;
	n = 0;
	if((td = _dirstat(to)) != nil){
		if(same(d, td))
			goto out;
		if((d->mode&DMDIR) && !(td->mode&DMDIR)){
			errno = ENOTDIR;
			n = -1;
			goto out;
		}
		if(!(d->mode&DMDIR) && (td->mode&DMDIR)){
			errno = EISDIR;
			n = -1;
			goto out;
		}
	}
	if(!sd && (d->mode&DMDIR)){
		n = strlen(from);
		if(parent(to, t) == 0)
			errno = strncmp(from, to, n) == 0 && to[n] == '/' ? EINVAL : EXDEV;
		n = -1;
		goto out;
	}
	if(td != nil && _REMOVE(to) < 0){
		_syserrno();
		n = -1;
		goto out;
	}
	if(sd){
		_nulldir(&nd);
		nd.name = t;
		if(_dirwstat(from, &nd) < 0){
			_syserrno();
			n = -1;
		}
	}else
		n = copy((char*)from, (char*)to, d);
out:
	free(td);
	free(d);
	return n;
}
