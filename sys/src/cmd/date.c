#include <u.h>
#include <libc.h>

int uflg, nflg;
char *layout;

void
usage(void)
{
	fprint(2, "usage: date [-un] [-f layout] [seconds]\n");
	exits("usage");
}

void
main(int argc, char *argv[])
{
	ulong now;

	ARGBEGIN{
	case 'n':	nflg = 1; break;
	case 'u':	uflg = 1; break;
	case 'f':	layout = EARGF(usage()); break;
	default:	usage();
	}ARGEND

	if(argc == 1)
		now = strtoul(*argv, 0, 0);
	else
		now = time(0);

	if(nflg)
		print("%ld\n", now);
	else if(layout != nil)
		print("%s\n", tmfmt(uflg ? gmtime(now) : localtime(now), layout));
	else if(uflg)
		print("%s", asctime(gmtime(now)));
	else
		print("%s", ctime(now));

	exits(0);
}
