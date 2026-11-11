#include <time.h>

/*
 * tzset is the zone localtime uses (plan9/ctime.c's), Plan 9's /env/timezone,
 * read again; $TZ is not read: the zone is Plan 9's.
 */
extern void _tzone(char **, char **, long *, int *);

char *tzname[2] = {"GMT", ""};
long timezone;	/* XSI: seconds west of GMT, standard time's */
int daylight;	/* XSI: whether there is a summer time */

void
tzset(void)
{
	_tzone(&tzname[0], &tzname[1], &timezone, &daylight);
}
