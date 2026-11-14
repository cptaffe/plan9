#include "curl_setup.h"
#include "protocol.h"
#include "url.h"
#include "curlx/strdup.h"

curl_malloc_callback Curl_cmalloc = malloc;
curl_free_callback Curl_cfree = free;
curl_realloc_callback Curl_crealloc = realloc;
curl_strdup_callback Curl_cstrdup = strdup;
curl_calloc_callback Curl_ccalloc = calloc;

const struct Curl_scheme Curl_scheme_http = {
	"http", NULL, CURLPROTO_HTTP, CURLPROTO_HTTP, 0, 80,
};
const struct Curl_scheme Curl_scheme_https = {
	"https", NULL, CURLPROTO_HTTPS, CURLPROTO_HTTP, 0, 443,
};

static const struct Curl_scheme *schemes[] = {
	&Curl_scheme_http,
	&Curl_scheme_https,
};

const struct Curl_scheme *
Curl_getn_scheme(const char *s, size_t n)
{
	size_t i;

	for(i = 0; i < sizeof schemes/sizeof schemes[0]; i++)
		if(strlen(schemes[i]->name) == n && curl_strnequal(s, schemes[i]->name, n))
			return schemes[i];
	return NULL;
}

const struct Curl_scheme *
Curl_get_scheme(const char *s)
{
	return Curl_getn_scheme(s, strlen(s));
}

CURLcode
Curl_parse_login_details(const char *login, const size_t len, char **userp, char **passwdp, char **optionsp)
{
	const char *p, *o, *e;
	size_t ul;

	e = login + len;
	p = memchr(login, ':', len);
	o = optionsp ? memchr(login, ';', len) : NULL;
	ul = (p && (!o || p < o) ? p : o ? o : e) - login;
	*userp = curlx_memdup0(login, ul);
	*passwdp = NULL;
	if(optionsp)
		*optionsp = NULL;
	if(!*userp)
		return CURLE_OUT_OF_MEMORY;
	if(p){
		const char *pe = o && o > p ? o : e;
		if(!(*passwdp = curlx_memdup0(p+1, pe-p-1)))
			goto nomem;
	}
	if(o){
		const char *oe = p && p > o ? p : e;
		if(oe > o+1 && !(*optionsp = curlx_memdup0(o+1, oe-o-1)))
			goto nomem;
	}
	return CURLE_OK;
nomem:
	free(*userp);
	free(*passwdp);
	*userp = *passwdp = NULL;
	return CURLE_OUT_OF_MEMORY;
}
