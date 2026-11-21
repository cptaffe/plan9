#include <locale.h>
#include <ctype.h>
#include <wctype.h>
#include <wchar.h>
#include <string.h>
#include <time.h>
#include <langinfo.h>

int isalnum_l(int c, locale_t l) { return isalnum(c); }
int isalpha_l(int c, locale_t l) { return isalpha(c); }
int isblank_l(int c, locale_t l) { return isblank(c); }
int iscntrl_l(int c, locale_t l) { return iscntrl(c); }
int isdigit_l(int c, locale_t l) { return isdigit(c); }
int isgraph_l(int c, locale_t l) { return isgraph(c); }
int islower_l(int c, locale_t l) { return islower(c); }
int isprint_l(int c, locale_t l) { return isprint(c); }
int ispunct_l(int c, locale_t l) { return ispunct(c); }
int isspace_l(int c, locale_t l) { return isspace(c); }
int isupper_l(int c, locale_t l) { return isupper(c); }
int isxdigit_l(int c, locale_t l) { return isxdigit(c); }
int tolower_l(int c, locale_t l) { return tolower(c); }
int toupper_l(int c, locale_t l) { return toupper(c); }

int iswalnum_l(wint_t c, locale_t l) { return iswalnum(c); }
int iswalpha_l(wint_t c, locale_t l) { return iswalpha(c); }
int iswblank_l(wint_t c, locale_t l) { return iswblank(c); }
int iswcntrl_l(wint_t c, locale_t l) { return iswcntrl(c); }
int iswdigit_l(wint_t c, locale_t l) { return iswdigit(c); }
int iswgraph_l(wint_t c, locale_t l) { return iswgraph(c); }
int iswlower_l(wint_t c, locale_t l) { return iswlower(c); }
int iswprint_l(wint_t c, locale_t l) { return iswprint(c); }
int iswpunct_l(wint_t c, locale_t l) { return iswpunct(c); }
int iswspace_l(wint_t c, locale_t l) { return iswspace(c); }
int iswupper_l(wint_t c, locale_t l) { return iswupper(c); }
int iswxdigit_l(wint_t c, locale_t l) { return iswxdigit(c); }
int iswctype_l(wint_t c, wctype_t t, locale_t l) { return iswctype(c, t); }
wctype_t wctype_l(const char *s, locale_t l) { return wctype(s); }
wint_t towlower_l(wint_t c, locale_t l) { return towlower(c); }
wint_t towupper_l(wint_t c, locale_t l) { return towupper(c); }
wint_t towctrans_l(wint_t c, wctrans_t t, locale_t l) { return towctrans(c, t); }
wctrans_t wctrans_l(const char *s, locale_t l) { return wctrans(s); }

int strcoll_l(const char *a, const char *b, locale_t l) { return strcoll(a, b); }
size_t strxfrm_l(char *d, const char *s, size_t n, locale_t l) { return strxfrm(d, s, n); }
char *strerror_l(int e, locale_t l) { return strerror(e); }
int wcscoll_l(const wchar_t *a, const wchar_t *b, locale_t l) { return wcscoll(a, b); }
size_t wcsxfrm_l(wchar_t *d, const wchar_t *s, size_t n, locale_t l) { return wcsxfrm(d, s, n); }

size_t
strftime_l(char *s, size_t n, const char *f, const struct tm *t, locale_t l)
{
	return strftime(s, n, f, t);
}

char *nl_langinfo_l(nl_item i, locale_t l) { return nl_langinfo(i); }
