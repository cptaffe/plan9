#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <err.h>

/* BSD's err.h: the program's name (what _start keeps of argv[0]), the message, and an error's */

extern char *__progname;

static void
msg(int code, const char *fmt, va_list ap)
{
	char *p;

	p = __progname ? strrchr(__progname, '/') : NULL;
	fprintf(stderr, "%s: ", p ? p+1 : __progname ? __progname : "?");
	if(fmt != NULL){
		vfprintf(stderr, fmt, ap);
		if(code >= 0)
			fprintf(stderr, ": ");
	}
	if(code >= 0)
		fprintf(stderr, "%s", strerror(code));
	fprintf(stderr, "\n");
}

void vwarn(const char *fmt, va_list ap) { msg(errno, fmt, ap); }
void vwarnx(const char *fmt, va_list ap) { msg(-1, fmt, ap); }
void vwarnc(int code, const char *fmt, va_list ap) { msg(code, fmt, ap); }
void verr(int r, const char *fmt, va_list ap) { msg(errno, fmt, ap); exit(r); }
void verrx(int r, const char *fmt, va_list ap) { msg(-1, fmt, ap); exit(r); }
void verrc(int r, int code, const char *fmt, va_list ap) { msg(code, fmt, ap); exit(r); }

void
warn(const char *fmt, ...)
{
	va_list ap;
	int e;

	e = errno;
	va_start(ap, fmt);
	msg(e, fmt, ap);
	va_end(ap);
}

void
warnx(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	msg(-1, fmt, ap);
	va_end(ap);
}

void
warnc(int code, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	msg(code, fmt, ap);
	va_end(ap);
}

void
err(int r, const char *fmt, ...)
{
	va_list ap;
	int e;

	e = errno;
	va_start(ap, fmt);
	msg(e, fmt, ap);
	va_end(ap);
	exit(r);
}

void
errx(int r, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	msg(-1, fmt, ap);
	va_end(ap);
	exit(r);
}

void
errc(int r, int code, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	msg(code, fmt, ap);
	va_end(ap);
	exit(r);
}
