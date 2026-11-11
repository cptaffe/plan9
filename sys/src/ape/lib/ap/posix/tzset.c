#include <time.h>

/*
 * tzset reads again the zone localtime uses (plan9/ctime.c's, where tzname,
 * timezone and daylight are), Plan 9's /env/timezone; $TZ is not read: the
 * zone is Plan 9's.
 */
extern void _tzone(int);

void
tzset(void)
{
	_tzone(1);
}
