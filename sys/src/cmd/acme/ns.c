#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <cursor.h>
#include <mouse.h>
#include <keyboard.h>
#include <frame.h>
#include <fcall.h>
#include <plumb.h>
#include <libsec.h>
#include <auth.h>
#include "dat.h"
#include "fns.h"

char*
nsline(char *s)
{
	char *f[8], **a, *p;
	int n, i, fl, fd, r;

	n = tokenize(s, f, nelem(f));
	if(n == 0 || f[0][0] == '#')
		return nil;
	if(n == nelem(f))
		return smprint("%s: too many arguments", f[0]);
	fl = 0;
	for(i=1; i<n && f[i][0]=='-' && f[i][1]; i++)
		for(p=f[i]+1; *p; p++)
			switch(*p){
			case 'a':
				fl |= MAFTER;
				break;
			case 'b':
				fl |= MBEFORE;
				break;
			case 'c':
				fl |= MCREATE;
				break;
			case 'C':
				fl |= MCACHE;
				break;
			default:
				return smprint("%s: bad flag -%c", f[0], *p);
			}
	a = f+i;
	n -= i;
	if(strcmp(f[0], "mount") == 0){
		if(n < 2 || n > 3)
			return smprint("usage: mount [-abcC] srvfile mtpt [spec]");
		fd = open(a[0], ORDWR);
		if(fd < 0)
			return smprint("mount: %r");
		r = amount(fd, a[1], fl, n==3 ? a[2] : "");
		close(fd);
	}else if(strcmp(f[0], "bind") == 0){
		if(n != 2)
			return smprint("usage: bind [-abcC] new old");
		r = bind(a[0], a[1], fl);
	}else if(strcmp(f[0], "unmount") == 0){
		if(i != 1 || n < 1 || n > 2)
			return smprint("usage: unmount [new] old");
		r = unmount(n==2 ? a[0] : nil, a[n-1]);
	}else
		return smprint("%s: not mount, bind or unmount", f[0]);
	if(r < 0)
		return smprint("%s: %r", f[0]);
	return nil;
}

char*
nswrite(char *s)
{
	char *p, *err;

	err = nil;
	while(err==nil && s!=nil && *s){
		p = strchr(s, '\n');
		if(p)
			*p++ = 0;
		err = nsline(s);
		s = p;
	}
	return err;
}
