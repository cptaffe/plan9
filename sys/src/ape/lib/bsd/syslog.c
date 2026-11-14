#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <syslog.h>

extern char *__progname;

static char *ident;
static int opt;
static int mask = 0xff;
static int fd = -1;

static char*
name(void)
{
	char *p;

	if(ident != NULL)
		return ident;
	if(__progname == NULL)
		return "syslog";
	p = strrchr(__progname, '/');
	return p ? p+1 : __progname;
}

void
openlog(const char *s, int o, int f)
{
	(void)f;
	free(ident);
	ident = s ? strdup(s) : NULL;
	opt = o;
	closelog();
	if(opt & LOG_NDELAY)
		syslog(-1, "");
}

void
closelog(void)
{
	if(fd >= 0)
		close(fd);
	fd = -1;
}

int
setlogmask(int m)
{
	int o;

	o = mask;
	if(m != 0)
		mask = m;
	return o;
}

void
vsyslog(int pri, const char *fmt, va_list ap)
{
	char buf[1024], sys[64], path[128], *e, *t;
	int n, s, en;
	time_t now;

	en = errno;
	if(pri >= 0 && !(mask & LOG_MASK(LOG_PRI(pri))))
		return;
	if(fd < 0){
		snprintf(path, sizeof path, "/sys/log/%s", name());
		if((fd = open(path, O_WRONLY|O_APPEND)) >= 0)
			fcntl(fd, F_SETFD, FD_CLOEXEC);
	}
	if(pri < 0)
		return;
	sys[0] = 0;
	if((s = open("/dev/sysname", O_RDONLY)) >= 0){
		n = read(s, sys, sizeof sys - 1);
		sys[n > 0 ? n : 0] = 0;
		close(s);
	}
	now = time(NULL);
	t = ctime(&now);
	e = buf + sizeof buf - 1;
	n = snprintf(buf, e-buf, "%s %.15s ", sys, t+4);
	if(opt & LOG_PID)
		n += snprintf(buf+n, e-buf-n, "%s[%d]: ", name(), getpid());
	errno = en;
	if(n < e-buf)
		n += vsnprintf(buf+n, e-buf-n, fmt, ap);
	if(n >= e-buf)
		n = e-buf-1;
	if(n == 0 || buf[n-1] != '\n')
		buf[n++] = '\n';
	s = fd >= 0 && write(fd, buf, n) == n;
	if((opt & LOG_PERROR) || (!s && (opt & LOG_CONS)))
		write(2, buf, n);
	errno = en;
}

void
syslog(int pri, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsyslog(pri, fmt, ap);
	va_end(ap);
}
