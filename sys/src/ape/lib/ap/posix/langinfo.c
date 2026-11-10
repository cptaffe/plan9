#include <langinfo.h>

/* the C locale's, in UTF-8 */
static char *info[] = {
	[CODESET] = "UTF-8",
	[D_T_FMT] = "%a %b %e %H:%M:%S %Y",
	[D_FMT] = "%m/%d/%y",
	[T_FMT] = "%H:%M:%S",
	[T_FMT_AMPM] = "%I:%M:%S %p",
	[AM_STR] = "AM",
	[PM_STR] = "PM",
	[DAY_1] = "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday",
	[ABDAY_1] = "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat",
	[MON_1] = "January", "February", "March", "April", "May", "June",
			"July", "August", "September", "October", "November", "December",
	[ABMON_1] = "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
	[RADIXCHAR] = ".",
	[YESEXPR] = "^[yY]",
	[NOEXPR] = "^[nN]",
	[CRNCYSTR] = "-",
};

char *
nl_langinfo(nl_item item)
{
	if(item < 0 || item >= sizeof info/sizeof info[0] || info[item] == 0)
		return "";
	return info[item];
}
