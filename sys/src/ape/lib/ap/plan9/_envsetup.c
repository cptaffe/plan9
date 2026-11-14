#include "lib.h"
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include "sys9.h"
#include "dir.h"

/*
 * Called before main to initialize environ.
 * Some plan9 environment variables
 * have 0 bytes in them (notably $path);
 * we change them to 1's (and execve changes back)
 *
 * Also, give it what a Unix program looks for (unixenv:
 * /sys/lib/ape/environ's lines),
 * and register the note handler.
 */

char **environ;
int errno;
unsigned long _clock;

static void fdsetup(char *, char *);
static void sigsetup(char *, char *);
static char *unixfile(int *);
static int unixenv(char **, int, char **, char *);

enum {
	Envhunk=7000,
};

void
_envsetup(void)
{
	int dfd, fdinited, n, nd, m, i, j, f, nohandle, psize, cnt, nux;
	char *ps, *p, *ux;
	char **pp;
	char name[NAME_MAX+5];
	Dir *d9, *d9a;
	static char **emptyenvp = 0;

	environ = emptyenvp;		/* pessimism */
	nohandle = 0;
	fdinited = 0;
	cnt = 0;
	strcpy(name, "#e");
	dfd = _OPEN(name, 0);
	if(dfd < 0)
		return;
	name[2] = '/';
	ps = p = malloc(Envhunk);
	if(p == 0)
		return;
	psize = Envhunk;
	nd = _dirreadall(dfd, &d9a);
	_CLOSE(dfd);
	for(j=0; j<nd; j++){
		d9 = &d9a[j];
		n = strlen(d9->name);
		if(n >= sizeof name - 4)
			continue;	/* shouldn't be possible */
		m = d9->length;
		i = p - ps;
		if(i+n+1+m+1 > psize) {
			psize += (n+m+2 < Envhunk)? Envhunk : n+m+2;
			ps = realloc(ps, psize);
			if (ps == 0) {
				free(d9a);
				return;
			}
			p = ps + i;
		}
		memcpy(p, d9->name, n);
		p[n] = '=';
		strcpy(name+3, d9->name);
		f = _OPEN(name, O_RDONLY);
		if(f < 0 || _READ(f, p+n+1, m) != m)
			m = 0;
		_CLOSE(f);
		if(p[n+m] == 0)
			m--;
		for(i=0; i<m; i++)
			if(p[n+1+i] == 0)
				p[n+1+i] = 1;
		p[n+1+m] = 0;
		if(strcmp(d9->name, "_fdinfo") == 0) {
			_fdinit(p+n+1, p+n+1+m);
			fdinited = 1;
		} else if(strcmp(d9->name, "_sighdlr") == 0)
			sigsetup(p+n+1, p+n+1+m);
		else if(strcmp(d9->name, "_umask") == 0)
			_umask = strtoul(p+n+1, 0, 8) & 0777;
		else if(strcmp(d9->name, "nohandle") == 0)
			nohandle = 1;
		p += n+m+2;
		cnt++;
	}
	free(d9a);
	if(!fdinited)
		_fdinit(0, 0);
	ux = unixfile(&nux);
	pp = malloc((1+cnt+nux)*sizeof(char *));
	if (pp == 0)
		return;
	environ = pp;
	p = ps;
	for(i = 0; i < cnt; i++) {
		*pp++ = p;
		p = memchr(p, 0, ps+psize-p);
		if (!p)
			break;
		p++;
	}
	if(ux != 0){
		pp += unixenv(environ, pp-environ, pp, ux);
		free(ux);
	}
	*pp = 0;
	if(!nohandle)
		_NOTIFY(_notehandler);
}

static void
sigsetup(char *s, char *se)
{
	int sig;
	char *e;

	while(s < se){
		sig = strtoul(s, &e, 10);
		if(s == e)
			break;
		s = e;
		if(sig <= MAXSIG)
			_sighdlr[sig] = SIG_IGN;
	}
}

/*
 * What a program written for Unix looks for in its environment, as
 * plan9port gives it: /sys/lib/ape/environ's NAME=value lines, each
 * when the environment has no NAME. It is the program's, in environ,
 * not /env's. In a value, $name is Plan 9's variable, a list's
 * elements joined by :; a line with a $name not there is left out.
 */
enum {
	Unixmax=16*1024,
};

static char*
unixfile(int *nl)
{
	int f, n;
	char *b, *s;

	*nl = 0;
	if((f = _OPEN("/sys/lib/ape/environ", OREAD|OCEXEC)) < 0)
		return 0;
	b = malloc(Unixmax+1);
	if(b == 0){
		_CLOSE(f);
		return 0;
	}
	n = _READ(f, b, Unixmax);
	_CLOSE(f);
	if(n <= 0){
		free(b);
		return 0;
	}
	b[n] = 0;
	for(s = b; *s; s++)
		if(*s == '\n')
			(*nl)++;
	(*nl)++;
	return b;
}

static char*
look(char **env, int n, char *name, int l)
{
	int i;

	for(i = 0; i < n; i++)
		if(strncmp(env[i], name, l) == 0 && env[i][l] == '=')
			return env[i]+l+1;
	return 0;
}

static int
isname(int c)
{
	return c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c >= '0' && c <= '9' || c == '_';
}

/* line's value, its $names Plan 9's, or 0 if one is not there; into s, if not 0 */
static int
expand(char **env, int n, char *v, char *e, char *s)
{
	int l, k;
	char *w, *x;

	k = 0;
	while(v < e){
		if(*v != '$' || v+1 < e && v[1] == '$'){	/* $$ is a $ */
			if(s)
				s[k] = *v;
			k++, v += 1 + (*v == '$');
			continue;
		}
		for(w = ++v; v < e && isname(*v); v++)
			;
		if((x = look(env, n, w, v-w)) == 0)
			return -1;
		for(; *x; x++, k++)
			if(s)
				s[k] = *x == 1? ':': *x;	/* a list's elements, as _envsetup has them */
	}
	return k;
}

static int
unixenv(char **env, int n, char **pp, char *b)
{
	int i, k, l, m;
	char *s, *e, *q, *ve;

	k = 0;
	for(; *b; b = e+(*e != 0)){
		if((e = strchr(b, '\n')) == 0)
			e = b+strlen(b);
		while(b < e && (*b == ' ' || *b == '\t'))
			b++;
		if(b == e || *b == '#' || (q = memchr(b, '=', e-b)) == 0 || q == b)
			continue;
		l = q-b;
		for(i = 0; i < l && isname(b[i]); i++)
			;
		if(i < l || b[0] >= '0' && b[0] <= '9')	/* not a name: HOME = x is not HOME */
			continue;
		if(look(env, n+k, b, l) != 0)
			continue;
		for(ve = e; ve > q+1 && (ve[-1] == ' ' || ve[-1] == '\t' || ve[-1] == '\r'); ve--)
			;
		if((m = expand(env, n, q+1, ve, 0)) < 0)
			continue;
		if((s = malloc(l+1+m+1)) == 0)
			break;
		memcpy(s, b, l+1);
		expand(env, n, q+1, ve, s+l+1);
		s[l+1+m] = 0;
		pp[k++] = s;
	}
	return k;
}
