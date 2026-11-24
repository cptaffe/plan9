#include "lib.h"
#include <stddef.h>
#include <pwd.h>
#include <string.h>
#include <errno.h>

static int
fill(int num, char *nam, struct passwd *pw, char *buf, size_t n, struct passwd **r)
{
	size_t ln, ld;

	ln = strlen(nam)+1;
	ld = 5+ln;
	if(ln+ld+8 > n)
		return ERANGE;
	memmove(buf, nam, ln);
	pw->pw_name = buf;
	memmove(buf+ln, "/usr/", 5);
	memmove(buf+ln+5, nam, ln);
	pw->pw_dir = buf+ln;
	memmove(buf+ln+ld, "/bin/rc", 8);
	pw->pw_shell = buf+ln+ld;
	pw->pw_uid = num;
	pw->pw_gid = num;
	*r = pw;
	return 0;
}

int
getpwnam_r(const char *name, struct passwd *pw, char *buf, size_t n, struct passwd **r)
{
	int num;
	char *nam, *mem;

	*r = NULL;
	num = 0;
	nam = (char*)name;
	mem = 0;
	if(!_getpw(&num, &nam, &mem))
		return 0;
	return fill(num, nam, pw, buf, n, r);
}

int
getpwuid_r(uid_t uid, struct passwd *pw, char *buf, size_t n, struct passwd **r)
{
	int num;
	char *nam, *mem;

	*r = NULL;
	num = uid;
	nam = 0;
	mem = 0;
	if(!_getpw(&num, &nam, &mem))
		return 0;
	return fill(num, nam, pw, buf, n, r);
}
