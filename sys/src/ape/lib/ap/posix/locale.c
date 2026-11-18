#include	<locale.h>
#include	<limits.h>
#include	<string.h>
#include	<errno.h>

static struct lconv Clocale = {
	".",		/* decimal_point */
	"",		/* thousands_sep */
	"",		/* grouping */
	"",		/* int_curr_symbol */
	"",		/* currency_symbol */
	"",		/* mon_decimal_point */
	"",		/* mon_thousands_sep */
	"",		/* mon_grouping */
	"",		/* positive_sign */
	"",		/* negative_sign */
	CHAR_MAX,	/* int_frac_digits */
	CHAR_MAX,	/* frac_digits */
	CHAR_MAX,	/* p_cs_precedes */
	CHAR_MAX,	/* p_sep_by_space */
	CHAR_MAX,	/* n_cs_precedes */
	CHAR_MAX,	/* n_sep_by_space */
	CHAR_MAX,	/* p_sign_posn */
	CHAR_MAX,	/* n_sign_posn */
	CHAR_MAX,	/* int_p_cs_precedes */
	CHAR_MAX,	/* int_p_sep_by_space */
	CHAR_MAX,	/* int_n_cs_precedes */
	CHAR_MAX,	/* int_n_sep_by_space */
	CHAR_MAX,	/* int_p_sign_posn */
	CHAR_MAX,	/* int_n_sign_posn */
};

static char *localename[2] = {"C", ""};
static short catlocale[7] = {0, 0, 0, 0, 0, 0, 0};
	/* indices into localename  for categories LC_ALL, LC_COLLATE, etc. */

#define ASIZE(a) (sizeof(a)/sizeof(a[0]))

struct __locale {
	int unused;
};

static struct __locale Cl;
static locale_t cur = LC_GLOBAL_LOCALE;

static int
name(const char *locale)
{
	int c;

	if(strcmp(locale, "POSIX") == 0 || strcmp(locale, "C.UTF-8") == 0)
		locale = "C";	/* the C locale's characters are UTF-8 */
	for(c=0; c<ASIZE(localename); c++)
		if(strcmp(locale, localename[c]) == 0)
			return c;
	return -1;
}

char *
setlocale(int category, const char *locale)
{
	int c, i;

	if(category < 0 || category >= ASIZE(catlocale))
		return 0;
	if(!locale)
		return localename[catlocale[category]];
	if((c = name(locale)) < 0)
		return 0;
	catlocale[category] = c;
	if(category == LC_ALL)
		for(i=0; i<ASIZE(catlocale); i++)
			catlocale[i] = c;
	return localename[c];
}

struct lconv *
localeconv(void)
{
	/* BUG: posix says look at environment variables
         * to set locale "", but we just make it the same
	 * as C, always.
	 */
	return &Clocale;
}

locale_t
newlocale(int mask, const char *locale, locale_t base)
{
	if((mask & ~LC_ALL_MASK) != 0 || locale == NULL){
		errno = EINVAL;
		return 0;
	}
	if(name(locale) < 0){
		errno = ENOENT;
		return 0;
	}
	return base != 0 && base != LC_GLOBAL_LOCALE? base: &Cl;
}

locale_t
duplocale(locale_t l)
{
	return &Cl;
}

void
freelocale(locale_t l)
{
}

locale_t
uselocale(locale_t l)
{
	locale_t old;

	old = cur;
	if(l != 0)
		cur = l;
	return old;
}
