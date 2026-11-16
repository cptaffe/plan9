#include <u.h>
#include <libc.h>

/*
 * like fprint but be sure to deliver as a single write.
 * (fprint uses a small write buffer.)
 */
void
xfprint(int fd, char *fmt, ...)
{
	char *s;
	va_list arg;

	va_start(arg, fmt);
	s = vsmprint(fmt, arg);
	va_end(arg);
	if(s == nil)
		sysfatal("smprint: %r");
	write(fd, s, strlen(s));
	free(s);
}

/*
 * awd names the window of a program run in acme (win's, /dev/acme or $winid's) for the current
 * directory: dir/-name. A window whose name has no /- is a file's, not a shell's, and keeps it;
 * name is argv[1], or what follows the - now, or rc.
 */
void
main(int argc, char **argv)
{
	int fd, tfd, n;
	char dir[512], tag[512], *id, *p, *name, *t;

	tfd = open("/dev/acme/tag", OREAD);
	fd = open("/dev/acme/ctl", OWRITE);
	if(fd < 0 && (id = getenv("winid")) != nil){
		snprint(dir, sizeof dir, "/mnt/wsys/%s/ctl", id);
		fd = open(dir, OWRITE);
		snprint(dir, sizeof dir, "/mnt/wsys/%s/tag", id);
		tfd = open(dir, OREAD);
	}
	if(fd < 0)
		exits(0);
	name = "rc";
	if(tfd >= 0){
		n = read(tfd, tag, sizeof tag-1);
		close(tfd);
		if(n < 0)
			n = 0;
		tag[n] = 0;
		if((p = strpbrk(tag, " \t\n")) != nil)
			*p = 0;
		if((p = strstr(tag, "/-")) == nil)
			exits(0);
		if(p[2] != 0)
			name = p+2;
	}
	if(argc > 1)
		name = argv[1];
	if((t = strdup(name)) == nil)
		sysfatal("strdup: %r");
	getwd(dir, 512);
	if(dir[0]!=0 && dir[strlen(dir)-1]=='/')
		dir[strlen(dir)-1] = 0;
	xfprint(fd, "name %s/-%s\n",  dir, t);
	xfprint(fd, "dumpdir %s\n", dir);
	exits(0);
}
