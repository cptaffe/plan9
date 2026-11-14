#define _POSIX_C_SOURCE 200809L
#define CURL_DISABLE_TYPECHECK
#define _BSD_EXTENSION
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <sys/time.h>
#include <curl/curl.h>
#include "web.h"

typedef struct Easy Easy;
typedef struct Hdr Hdr;
typedef struct Multi Multi;

struct Hdr {
	char	*name;
	char	*value;
};

struct Easy {
	char	*url;
	CURLU	*curlu;
	char	*method;
	char	*useragent;
	char	*referer;
	char	*cookie;
	char	*userpwd;
	char	*user;
	char	*pass;
	char	*encoding;
	const char	*postfields;
	char	*copied;
	curl_off_t	postsize;
	curl_off_t	insize;
	long	nobody;
	long	post;
	long	upload;
	long	failonerror;
	long	header;
	long	httpauth;
	long	noprogress;
	struct curl_slist	*headers;
	curl_mime	*mime;
	char	*mimebody;
	size_t	nmime;
	char	*errbuf;
	curl_write_callback	writef;
	void	*writedata;
	curl_read_callback	readf;
	void	*readdata;
	curl_write_callback	headerf;
	void	*headerdata;
	curl_xferinfo_callback	xferf;
	void	*xferdata;
	curl_progress_callback	progf;
	void	*progdata;
	void	*priv;

	long	code;
	char	*ctype;
	char	*effurl;
	char	*scheme;
	long	authavail;
	curl_off_t	retryafter;
	curl_off_t	dlsize;
	curl_off_t	dltotal;
	curl_off_t	ulsize;
	double	total;
	Hdr	*hdrs;
	int	nhdrs;
	struct curl_header	hout;

	Multi	*multi;
	Easy	*next;
	int	done;
	CURLcode	result;
};

struct Multi {
	Easy	*easy;
	CURLMsg	*msgs;
	int	nmsgs;
	int	taken;
};

static const char *known[] = {
	"Accept-Ranges", "Age", "Cache-Control", "Connection", "Content-Disposition",
	"Content-Encoding", "Content-Language", "Content-Length", "Content-Location",
	"Content-Range", "Content-Security-Policy", "Content-Type", "Date", "ETag", "Expires",
	"Keep-Alive", "Last-Modified", "Link", "Location", "Pragma", "Proxy-Authenticate",
	"Retry-After", "Server", "Set-Cookie", "Strict-Transport-Security", "Trailer",
	"Transfer-Encoding", "Vary", "Via", "WWW-Authenticate", "X-Content-Type-Options",
	"X-Frame-Options", "X-GitHub-Request-Id", "X-Request-Id",
};

static int
same(const char *file, const char *name)
{
	for(; *name; name++){
		if(!(*name >= 'a' && *name <= 'z' || *name >= 'A' && *name <= 'Z' || *name >= '0' && *name <= '9'))
			continue;
		if(*file == 0 || (*file | 0x20) != (*name | 0x20))
			return 0;
		file++;
	}
	return *file == 0;
}

static const char*
hdrname(const char *file)
{
	size_t i;

	for(i = 0; i < sizeof known/sizeof known[0]; i++)
		if(same(file, known[i]))
			return known[i];
	return file;
}

static void
seterr(Easy *e, const char *fmt, ...)
{
	va_list ap;

	if(!e->errbuf)
		return;
	va_start(ap, fmt);
	vsnprintf(e->errbuf, CURL_ERROR_SIZE, fmt, ap);
	va_end(ap);
}

static char*
sdup(const char *s)
{
	return s ? strdup(s) : NULL;
}

static void
setstr(char **p, const char *s)
{
	free(*p);
	*p = sdup(s);
}

static void
clearresult(Easy *e)
{
	int i;

	for(i = 0; i < e->nhdrs; i++){
		free(e->hdrs[i].name);
		free(e->hdrs[i].value);
	}
	free(e->hdrs);
	e->hdrs = NULL;
	e->nhdrs = 0;
	setstr(&e->ctype, NULL);
	setstr(&e->effurl, NULL);
	setstr(&e->scheme, NULL);
	e->code = 0;
	e->authavail = 0;
	e->retryafter = 0;
	e->dlsize = e->dltotal = e->ulsize = 0;
	e->total = 0;
}

static void
defaults(Easy *e)
{
	e->postsize = -1;
	e->insize = -1;
	e->httpauth = CURLAUTH_BASIC;
	e->noprogress = 1;
}

static void
freeopts(Easy *e)
{
	free(e->url);
	free(e->method);
	free(e->useragent);
	free(e->referer);
	free(e->cookie);
	free(e->userpwd);
	free(e->user);
	free(e->pass);
	free(e->encoding);
	free(e->copied);
}

CURL*
curl_easy_init(void)
{
	Easy *e;

	if((e = calloc(1, sizeof *e)) == NULL)
		return NULL;
	defaults(e);
	return e;
}

void
curl_easy_cleanup(CURL *h)
{
	Easy *e;

	if((e = h) == NULL)
		return;
	if(e->multi)
		curl_multi_remove_handle(e->multi, e);
	clearresult(e);
	freeopts(e);
	free(e);
}

void
curl_easy_reset(CURL *h)
{
	Easy *e;
	Multi *m;
	Easy *next;

	e = h;
	m = e->multi;
	next = e->next;
	clearresult(e);
	freeopts(e);
	memset(e, 0, sizeof *e);
	e->multi = m;
	e->next = next;
	defaults(e);
}

CURL*
curl_easy_duphandle(CURL *h)
{
	Easy *e, *d;

	e = h;
	if((d = malloc(sizeof *d)) == NULL)
		return NULL;
	*d = *e;
	d->url = sdup(e->url);
	d->method = sdup(e->method);
	d->useragent = sdup(e->useragent);
	d->referer = sdup(e->referer);
	d->cookie = sdup(e->cookie);
	d->userpwd = sdup(e->userpwd);
	d->user = sdup(e->user);
	d->pass = sdup(e->pass);
	d->encoding = sdup(e->encoding);
	if(e->copied){
		d->copied = malloc(e->postsize >= 0 ? e->postsize+1 : strlen(e->copied)+1);
		if(d->copied)
			memcpy(d->copied, e->copied, e->postsize >= 0 ? e->postsize+1 : strlen(e->copied)+1);
		d->postfields = d->copied;
	}
	d->hdrs = NULL;
	d->nhdrs = 0;
	d->ctype = d->effurl = d->scheme = NULL;
	d->multi = NULL;
	d->next = NULL;
	d->done = 0;
	d->mimebody = NULL;
	return d;
}

CURLcode
curl_easy_setopt(CURL *h, CURLoption opt, ...)
{
	Easy *e;
	va_list ap;
	long l;
	void *p;
	curl_off_t o;

	if((e = h) == NULL)
		return CURLE_BAD_FUNCTION_ARGUMENT;
	l = 0;
	p = NULL;
	o = 0;
	va_start(ap, opt);
	if(opt < CURLOPTTYPE_OBJECTPOINT)
		l = va_arg(ap, long);
	else if(opt < CURLOPTTYPE_FUNCTIONPOINT)
		p = va_arg(ap, void*);
	else if(opt < CURLOPTTYPE_OFF_T)
		p = (void*)va_arg(ap, void(*)(void));
	else if(opt < CURLOPTTYPE_BLOB)
		o = va_arg(ap, curl_off_t);
	else
		p = va_arg(ap, void*);
	va_end(ap);
	switch(opt){
	case CURLOPT_URL: setstr(&e->url, p); e->curlu = NULL; break;
	case CURLOPT_CURLU: e->curlu = p; break;
	case CURLOPT_CUSTOMREQUEST: setstr(&e->method, p); break;
	case CURLOPT_USERAGENT: setstr(&e->useragent, p); break;
	case CURLOPT_REFERER: setstr(&e->referer, p); break;
	case CURLOPT_COOKIE: setstr(&e->cookie, p); break;
	case CURLOPT_USERPWD: setstr(&e->userpwd, p); break;
	case CURLOPT_USERNAME: setstr(&e->user, p); break;
	case CURLOPT_PASSWORD: setstr(&e->pass, p); break;
	case CURLOPT_ACCEPT_ENCODING: setstr(&e->encoding, p); break;
	case CURLOPT_HTTPHEADER: e->headers = p; break;
	case CURLOPT_MIMEPOST: e->mime = p; e->post = p != NULL; if(p) e->upload = 0; break;
	case CURLOPT_ERRORBUFFER: e->errbuf = p; break;
	case CURLOPT_POSTFIELDS:
		e->postfields = p;
		e->post = 1;
		break;
	case CURLOPT_COPYPOSTFIELDS:
		free(e->copied);
		e->copied = NULL;
		if(p){
			size_t n = e->postsize >= 0 ? (size_t)e->postsize : strlen(p);
			if((e->copied = malloc(n+1)) == NULL)
				return CURLE_OUT_OF_MEMORY;
			memcpy(e->copied, p, n);
			e->copied[n] = 0;
		}
		e->postfields = e->copied;
		e->post = 1;
		break;
	case CURLOPT_POSTFIELDSIZE: e->postsize = l; break;
	case CURLOPT_POSTFIELDSIZE_LARGE: e->postsize = o; break;
	case CURLOPT_INFILESIZE: e->insize = l; break;
	case CURLOPT_INFILESIZE_LARGE: e->insize = o; break;
	case CURLOPT_POST: e->post = l != 0; if(l) e->upload = 0; break;
	case CURLOPT_HTTPGET: if(l){ e->post = e->upload = e->nobody = 0; e->postfields = NULL; } break;
	case CURLOPT_UPLOAD: e->upload = l != 0; if(l) e->post = 0; break;
	case CURLOPT_NOBODY: e->nobody = l != 0; break;
	case CURLOPT_FAILONERROR: e->failonerror = l != 0; break;
	case CURLOPT_HEADER: e->header = l != 0; break;
	case CURLOPT_HTTPAUTH: e->httpauth = l; break;
	case CURLOPT_NOPROGRESS: e->noprogress = l != 0; break;
	case CURLOPT_WRITEFUNCTION: e->writef = (curl_write_callback)p; break;
	case CURLOPT_WRITEDATA: e->writedata = p; break;
	case CURLOPT_READFUNCTION: e->readf = (curl_read_callback)p; break;
	case CURLOPT_READDATA: e->readdata = p; break;
	case CURLOPT_HEADERFUNCTION: e->headerf = (curl_write_callback)p; break;
	case CURLOPT_HEADERDATA: e->headerdata = p; break;
	case CURLOPT_XFERINFOFUNCTION: e->xferf = (curl_xferinfo_callback)p; break;
	case CURLOPT_XFERINFODATA: e->xferdata = p; e->progdata = p; break;
	case CURLOPT_PROGRESSFUNCTION: e->progf = (curl_progress_callback)p; break;
	case CURLOPT_PRIVATE: e->priv = p; break;
	default: break;
	}
	return CURLE_OK;
}

static char*
geturl(Easy *e)
{
	char *u;

	if(e->curlu){
		if(curl_url_get(e->curlu, CURLUPART_URL, &u, 0) != CURLUE_OK)
			return NULL;
		return u;
	}
	return sdup(e->url);
}

static int
ctl(int fd, const char *fmt, ...)
{
	char buf[8192];
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	if(n < 0 || n >= sizeof buf)
		return -1;
	return write(fd, buf, n) == n ? 0 : -1;
}

static int
haveheader(Easy *e, const char *name)
{
	struct curl_slist *s;
	size_t n;

	n = strlen(name);
	for(s = e->headers; s; s = s->next)
		if(strncasecmp(s->data, name, n) == 0 && (s->data[n] == ':' || s->data[n] == ';'))
			return 1;
	return 0;
}

static char*
base64(const char *s)
{
	static const char t[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	size_t n, i;
	char *o, *p;
	unsigned v;

	n = strlen(s);
	if((p = o = malloc(4*((n+2)/3)+1)) == NULL)
		return NULL;
	for(i = 0; i+2 < n; i += 3){
		v = (unsigned char)s[i]<<16 | (unsigned char)s[i+1]<<8 | (unsigned char)s[i+2];
		*p++ = t[v>>18&63]; *p++ = t[v>>12&63]; *p++ = t[v>>6&63]; *p++ = t[v&63];
	}
	if(i < n){
		v = (unsigned char)s[i]<<16 | (i+1 < n ? (unsigned char)s[i+1]<<8 : 0);
		*p++ = t[v>>18&63];
		*p++ = t[v>>12&63];
		*p++ = i+1 < n ? t[v>>6&63] : '=';
		*p++ = '=';
	}
	*p = 0;
	return o;
}

static char*
readfile(const char *path)
{
	char buf[8192], *s;
	int fd, n, t;

	if((fd = open(path, O_RDONLY)) < 0)
		return NULL;
	t = 0;
	while(t < sizeof buf - 1 && (n = read(fd, buf+t, sizeof buf - 1 - t)) > 0)
		t += n;
	close(fd);
	buf[t] = 0;
	while(t > 0 && (buf[t-1] == '\n' || buf[t-1] == '\r'))
		buf[--t] = 0;
	s = strdup(buf);
	return s;
}

static int
addhdr(Easy *e, const char *name, char *value)
{
	Hdr *h;

	if((h = realloc(e->hdrs, (e->nhdrs+1)*sizeof *h)) == NULL)
		return -1;
	e->hdrs = h;
	h[e->nhdrs].name = strdup(name);
	h[e->nhdrs].value = value;
	e->nhdrs++;
	return 0;
}

static const char*
gethdr(Easy *e, const char *name)
{
	int i;

	for(i = 0; i < e->nhdrs; i++)
		if(strcasecmp(e->hdrs[i].name, name) == 0)
			return e->hdrs[i].value;
	return NULL;
}

static const char *fixed[] = { "body", "contenttype", "ctl", "postbody", "status", "parsed" };

static void
readheaders(Easy *e, const char *dir)
{
	char path[256];
	DIR *d;
	struct dirent *de;
	size_t i;
	char *v;

	if(e->ctype)
		addhdr(e, "Content-Type", strdup(e->ctype));
	if((d = opendir(dir)) == NULL)
		return;
	while((de = readdir(d)) != NULL){
		for(i = 0; i < sizeof fixed/sizeof fixed[0]; i++)
			if(strcmp(de->d_name, fixed[i]) == 0)
				break;
		if(i < sizeof fixed/sizeof fixed[0] || de->d_name[0] == '.')
			continue;
		snprintf(path, sizeof path, "%s/%s", dir, de->d_name);
		if((v = readfile(path)) != NULL)
			addhdr(e, hdrname(de->d_name), v);
	}
	closedir(d);
}

static size_t
fwritef(char *p, size_t s, size_t n, void *f)
{
	return fwrite(p, s, n, f ? f : stdout);
}

static int
giveheader(Easy *e, const char *s, size_t n)
{
	curl_write_callback f;
	void *d;

	if(e->headerf || e->headerdata){
		f = e->headerf ? e->headerf : e->writef ? e->writef : fwritef;
		d = e->headerdata;
		if(f((char*)s, 1, n, d) != n)
			return -1;
	}
	if(e->header){
		f = e->writef ? e->writef : fwritef;
		if(f((char*)s, 1, n, e->writedata) != n)
			return -1;
	}
	return 0;
}

static long
authof(const char *v)
{
	long a;

	a = 0;
	if(v == NULL)
		return 0;
	if(strcasestr(v, "basic"))
		a |= CURLAUTH_BASIC;
	if(strcasestr(v, "digest"))
		a |= CURLAUTH_DIGEST;
	if(strcasestr(v, "negotiate"))
		a |= CURLAUTH_NEGOTIATE;
	if(strcasestr(v, "bearer"))
		a |= CURLAUTH_BEARER;
	return a;
}

static int
progress(Easy *e)
{
	if(e->noprogress)
		return 0;
	if(e->xferf)
		return e->xferf(e->xferdata, e->dltotal, e->dlsize, e->insize > 0 ? e->insize : 0, e->ulsize);
	if(e->progf)
		return e->progf(e->progdata, (double)e->dltotal, (double)e->dlsize, (double)(e->insize > 0 ? e->insize : 0), (double)e->ulsize);
	return 0;
}

static double
now(void)
{
	struct timeval t;

	gettimeofday(&t, NULL);
	return t.tv_sec + t.tv_usec/1e6;
}

static CURLcode
post(Easy *e, const char *dir)
{
	char path[256], buf[65536];
	const char *p;
	curl_off_t n, left;
	size_t r;
	int fd;

	snprintf(path, sizeof path, "%s/postbody", dir);
	if((fd = open(path, O_WRONLY)) < 0){
		seterr(e, "webfs: %s: %s", path, strerror(errno));
		return CURLE_SEND_ERROR;
	}
	if(e->mimebody){
		p = e->mimebody;
		left = e->nmime;
		while(left > 0){
			n = left > 65536 ? 65536 : left;
			if(write(fd, p, n) != n){
				close(fd);
				seterr(e, "webfs: postbody: %s", strerror(errno));
				return CURLE_SEND_ERROR;
			}
			p += n;
			left -= n;
			e->ulsize += n;
		}
	}else if(e->postfields){
		p = e->postfields;
		left = e->postsize >= 0 ? e->postsize : (curl_off_t)strlen(p);
		while(left > 0){
			n = left > 65536 ? 65536 : left;
			if(write(fd, p, n) != n){
				close(fd);
				seterr(e, "webfs: postbody: %s", strerror(errno));
				return CURLE_SEND_ERROR;
			}
			p += n;
			left -= n;
			e->ulsize += n;
		}
	}else if(e->readf){
		for(;;){
			r = e->readf(buf, 1, sizeof buf, e->readdata);
			if(r == CURL_READFUNC_ABORT){
				close(fd);
				return CURLE_ABORTED_BY_CALLBACK;
			}
			if(r == CURL_READFUNC_PAUSE || r > sizeof buf){
				close(fd);
				return CURLE_READ_ERROR;
			}
			if(r == 0)
				break;
			if(write(fd, buf, r) != r){
				close(fd);
				seterr(e, "webfs: postbody: %s", strerror(errno));
				return CURLE_SEND_ERROR;
			}
			e->ulsize += r;
			if(progress(e)){
				close(fd);
				return CURLE_ABORTED_BY_CALLBACK;
			}
		}
	}
	close(fd);
	return CURLE_OK;
}

static CURLcode
perform(Easy *e)
{
	char dir[64], path[256], buf[65536], *url, *auth, *cred, *s, *reason;
	const char *method, *v;
	CURLU *u;
	char *scheme, *user, *pass;
	struct curl_slist *l;
	int cfd, bfd, n, i, sending;
	curl_write_callback wf;
	CURLcode r;
	double t0;

	clearresult(e);
	t0 = now();
	if(e->errbuf)
		e->errbuf[0] = 0;
	if((url = geturl(e)) == NULL){
		seterr(e, "no URL set");
		return CURLE_URL_MALFORMAT;
	}
	if((u = curl_url()) == NULL){
		free(url);
		return CURLE_OUT_OF_MEMORY;
	}
	if(curl_url_set(u, CURLUPART_URL, url, CURLU_GUESS_SCHEME|CURLU_NON_SUPPORT_SCHEME) != CURLUE_OK){
		seterr(e, "URL rejected: %s", url);
		free(url);
		curl_url_cleanup(u);
		return CURLE_URL_MALFORMAT;
	}
	free(url);
	scheme = user = pass = NULL;
	curl_url_get(u, CURLUPART_SCHEME, &scheme, 0);
	if(scheme == NULL || strcmp(scheme, "http") != 0 && strcmp(scheme, "https") != 0){
		seterr(e, "Protocol \"%s\" not supported", scheme ? scheme : "");
		curl_free(scheme);
		curl_url_cleanup(u);
		return CURLE_UNSUPPORTED_PROTOCOL;
	}
	e->scheme = sdup(scheme);
	curl_free(scheme);
	curl_url_get(u, CURLUPART_USER, &user, CURLU_URLDECODE);
	curl_url_get(u, CURLUPART_PASSWORD, &pass, CURLU_URLDECODE);
	curl_url_set(u, CURLUPART_USER, NULL, 0);
	curl_url_set(u, CURLUPART_PASSWORD, NULL, 0);
	curl_url_set(u, CURLUPART_FRAGMENT, NULL, 0);
	url = NULL;
	curl_url_get(u, CURLUPART_URL, &url, 0);
	curl_url_cleanup(u);
	cred = NULL;
	if(e->userpwd)
		cred = strdup(e->userpwd);
	else if(e->user || e->pass || user){
		const char *a = e->user ? e->user : user ? user : "";
		const char *b = e->pass ? e->pass : (!e->user && pass) ? pass : "";
		if((cred = malloc(strlen(a)+strlen(b)+2)) != NULL)
			sprintf(cred, "%s:%s", a, b);
	}
	curl_free(user);
	curl_free(pass);

	r = CURLE_OK;
	bfd = -1;
	if((cfd = open("/mnt/web/clone", O_RDWR)) < 0){
		seterr(e, "webfs: /mnt/web/clone: %s", strerror(errno));
		curl_free(url);
		free(cred);
		return CURLE_COULDNT_CONNECT;
	}
	n = read(cfd, buf, sizeof buf - 1);
	buf[n > 0 ? n : 0] = 0;
	snprintf(dir, sizeof dir, "/mnt/web/%d", atoi(buf));

	sending = e->post || e->upload || e->postfields;
	if(e->method)
		method = e->method;
	else if(e->nobody)
		method = "HEAD";
	else if(e->upload)
		method = "PUT";
	else if(sending)
		method = "POST";
	else
		method = "GET";
	if(ctl(cfd, "url %s", url) < 0 || ctl(cfd, "request %s", method) < 0){
		seterr(e, "webfs: %s: %s", url, strerror(errno));
		r = CURLE_URL_MALFORMAT;
		goto out;
	}
	if(e->useragent && ctl(cfd, "useragent %s", e->useragent) < 0)
		goto badhdr;
	if(e->referer && !haveheader(e, "Referer") && ctl(cfd, "headers Referer: %s", e->referer) < 0)
		goto badhdr;
	if(e->cookie && !haveheader(e, "Cookie") && ctl(cfd, "headers Cookie: %s", e->cookie) < 0)
		goto badhdr;
	if(cred && (e->httpauth & CURLAUTH_BASIC) && !haveheader(e, "Authorization")){
		if((auth = base64(cred)) == NULL){
			r = CURLE_OUT_OF_MEMORY;
			goto out;
		}
		i = ctl(cfd, "headers Authorization: Basic %s", auth);
		free(auth);
		if(i < 0)
			goto badhdr;
	}
	if(e->mime && sending){
		char mt[128];
		free(e->mimebody);
		if((e->mimebody = _curl_mimebody(e->mime, &e->nmime, mt, sizeof mt)) == NULL){
			seterr(e, "could not make the multipart body");
			r = CURLE_READ_ERROR;
			goto out;
		}
		if(!haveheader(e, "Content-Type") && ctl(cfd, "headers Content-Type: %s", mt) < 0)
			goto badhdr;
	}else if(sending && !e->upload && !haveheader(e, "Content-Type") && ctl(cfd, "headers Content-Type: application/x-www-form-urlencoded") < 0)
		goto badhdr;
	for(l = e->headers; l; l = l->next){
		s = l->data;
		if((v = strpbrk(s, ":;")) == NULL || v == s)
			continue;
		if(*v == ':'){
			const char *w = v+1;
			while(*w == ' ' || *w == '\t')
				w++;
			if(*w == 0)
				continue;
			if(ctl(cfd, "headers %s", s) < 0)
				goto badhdr;
		}else if(v[1] == 0 && ctl(cfd, "headers %.*s:", (int)(v-s), s) < 0)
			goto badhdr;
	}
	if(sending && (r = post(e, dir)) != CURLE_OK)
		goto out;

	snprintf(path, sizeof path, "%s/body", dir);
	if((bfd = open(path, O_RDONLY)) < 0){
		seterr(e, "%s", strerror(errno));
		r = CURLE_COULDNT_CONNECT;
		goto out;
	}
	snprintf(path, sizeof path, "%s/status", dir);
	if((s = readfile(path)) == NULL){
		seterr(e, "webfs: %s: %s", path, strerror(errno));
		r = CURLE_RECV_ERROR;
		goto out;
	}
	e->code = strtol(s, &reason, 10);
	while(*reason == ' ')
		reason++;
	snprintf(path, sizeof path, "%s/contenttype", dir);
	e->ctype = readfile(path);
	if(e->ctype && e->ctype[0] == 0)
		setstr(&e->ctype, NULL);
	snprintf(path, sizeof path, "%s/parsed/url", dir);
	e->effurl = readfile(path);
	if(e->effurl == NULL || e->effurl[0] == 0)
		setstr(&e->effurl, url);
	readheaders(e, dir);
	if(e->code == 401)
		e->authavail = authof(gethdr(e, "WWW-Authenticate"));
	if((v = gethdr(e, "Retry-After")) != NULL)
		e->retryafter = strtoll(v, NULL, 10);
	if((v = gethdr(e, "Content-Length")) != NULL)
		e->dltotal = strtoll(v, NULL, 10);

	n = snprintf(buf, sizeof buf, "HTTP/1.1 %ld %s\r\n", e->code, reason);
	free(s);
	if(giveheader(e, buf, n) < 0)
		goto writerr;
	for(i = 0; i < e->nhdrs; i++){
		n = snprintf(buf, sizeof buf, "%s: %s\r\n", e->hdrs[i].name, e->hdrs[i].value);
		if(n >= sizeof buf)
			n = sizeof buf - 1;
		if(giveheader(e, buf, n) < 0)
			goto writerr;
	}
	if(giveheader(e, "\r\n", 2) < 0)
		goto writerr;

	if(e->failonerror && e->code >= 400){
		seterr(e, "The requested URL returned error: %ld", e->code);
		r = CURLE_HTTP_RETURNED_ERROR;
		goto out;
	}
	if(e->nobody || strcmp(method, "HEAD") == 0)
		goto out;
	wf = e->writef ? e->writef : fwritef;
	while((n = read(bfd, buf, sizeof buf)) > 0){
		e->dlsize += n;
		if(wf(buf, 1, n, e->writedata) != n)
			goto writerr;
		if(progress(e)){
			r = CURLE_ABORTED_BY_CALLBACK;
			goto out;
		}
	}
	if(n < 0){
		seterr(e, "webfs: body: %s", strerror(errno));
		r = CURLE_RECV_ERROR;
	}
	goto out;

badhdr:
	seterr(e, "webfs: header refused: %s", strerror(errno));
	r = CURLE_BAD_FUNCTION_ARGUMENT;
	goto out;
writerr:
	seterr(e, "Failure writing output to destination");
	r = CURLE_WRITE_ERROR;
out:
	free(e->mimebody);
	e->mimebody = NULL;
	if(bfd >= 0)
		close(bfd);
	close(cfd);
	curl_free(url);
	free(cred);
	e->total = now() - t0;
	return r;
}

CURLcode
curl_easy_perform(CURL *h)
{
	Easy *e;

	if((e = h) == NULL)
		return CURLE_BAD_FUNCTION_ARGUMENT;
	return perform(e);
}

CURLcode
curl_easy_getinfo(CURL *h, CURLINFO info, ...)
{
	Easy *e;
	va_list ap;
	void *p;

	if((e = h) == NULL)
		return CURLE_BAD_FUNCTION_ARGUMENT;
	va_start(ap, info);
	p = va_arg(ap, void*);
	va_end(ap);
	if(p == NULL)
		return CURLE_BAD_FUNCTION_ARGUMENT;
	switch(info){
	case CURLINFO_RESPONSE_CODE: *(long*)p = e->code; break;
	case CURLINFO_HTTP_CONNECTCODE: *(long*)p = 0; break;
	case CURLINFO_HTTPAUTH_AVAIL: *(long*)p = e->authavail; break;
	case CURLINFO_PROXYAUTH_AVAIL: *(long*)p = 0; break;
	case CURLINFO_REDIRECT_COUNT: *(long*)p = 0; break;
	case CURLINFO_NUM_CONNECTS: *(long*)p = 1; break;
	case CURLINFO_OS_ERRNO: *(long*)p = 0; break;
	case CURLINFO_FILETIME: *(long*)p = -1; break;
	case CURLINFO_HTTP_VERSION: *(long*)p = CURL_HTTP_VERSION_1_1; break;
	case CURLINFO_CONTENT_TYPE: *(char**)p = e->ctype; break;
	case CURLINFO_EFFECTIVE_URL: *(char**)p = e->effurl ? e->effurl : e->url; break;
	case CURLINFO_SCHEME: *(char**)p = e->scheme; break;
	case CURLINFO_PRIVATE: *(char**)p = e->priv; break;
	case CURLINFO_PRIMARY_IP: *(char**)p = ""; break;
	case CURLINFO_REDIRECT_URL: *(char**)p = NULL; break;
	case CURLINFO_EFFECTIVE_METHOD: *(char**)p = e->method; break;
	case CURLINFO_RETRY_AFTER: *(curl_off_t*)p = e->retryafter; break;
	case CURLINFO_SIZE_DOWNLOAD_T: *(curl_off_t*)p = e->dlsize; break;
	case CURLINFO_SIZE_UPLOAD_T: *(curl_off_t*)p = e->ulsize; break;
	case CURLINFO_CONTENT_LENGTH_DOWNLOAD_T: *(curl_off_t*)p = e->dltotal ? e->dltotal : -1; break;
	case CURLINFO_TOTAL_TIME_T: *(curl_off_t*)p = (curl_off_t)(e->total*1e6); break;
	case CURLINFO_SPEED_DOWNLOAD_T: *(curl_off_t*)p = e->total > 0 ? (curl_off_t)(e->dlsize/e->total) : 0; break;
	case CURLINFO_TOTAL_TIME: *(double*)p = e->total; break;
	case CURLINFO_SIZE_DOWNLOAD: *(double*)p = (double)e->dlsize; break;
	case CURLINFO_SIZE_UPLOAD: *(double*)p = (double)e->ulsize; break;
	case CURLINFO_CONTENT_LENGTH_DOWNLOAD: *(double*)p = e->dltotal ? (double)e->dltotal : -1; break;
	case CURLINFO_SPEED_DOWNLOAD: *(double*)p = e->total > 0 ? e->dlsize/e->total : 0; break;
	default:
		return CURLE_UNKNOWN_OPTION;
	}
	return CURLE_OK;
}

static void
fillhdr(Easy *e, int at)
{
	int i, n, k;

	n = k = 0;
	for(i = 0; i < e->nhdrs; i++)
		if(strcasecmp(e->hdrs[i].name, e->hdrs[at].name) == 0){
			if(i < at)
				k++;
			n++;
		}
	e->hout.name = e->hdrs[at].name;
	e->hout.value = e->hdrs[at].value;
	e->hout.amount = n;
	e->hout.index = k;
	e->hout.origin = CURLH_HEADER;
	e->hout.anchor = (void*)(long)at;
}

CURLHcode
curl_easy_header(CURL *h, const char *name, size_t index, unsigned int origin, int request, struct curl_header **hout)
{
	Easy *e;
	int i;
	size_t k;

	if((e = h) == NULL || name == NULL || hout == NULL || request > 0)
		return request > 0 ? CURLHE_NOREQUEST : CURLHE_BAD_ARGUMENT;
	if(e->nhdrs == 0)
		return CURLHE_NOHEADERS;
	if(!(origin & CURLH_HEADER))
		return CURLHE_MISSING;
	k = 0;
	for(i = 0; i < e->nhdrs; i++)
		if(strcasecmp(e->hdrs[i].name, name) == 0 && k++ == index){
			fillhdr(e, i);
			*hout = &e->hout;
			return CURLHE_OK;
		}
	return k ? CURLHE_BADINDEX : CURLHE_MISSING;
}

struct curl_header*
curl_easy_nextheader(CURL *h, unsigned int origin, int request, struct curl_header *prev)
{
	Easy *e;
	int at;

	if((e = h) == NULL || request > 0 || !(origin & CURLH_HEADER))
		return NULL;
	at = prev ? (int)(long)prev->anchor + 1 : 0;
	if(at >= e->nhdrs)
		return NULL;
	fillhdr(e, at);
	return &e->hout;
}

CURLcode
curl_easy_ssls_import(CURL *h, const char *key, const unsigned char *mac, size_t nmac, const unsigned char *d, size_t nd)
{
	(void)h;
	(void)key;
	(void)mac;
	(void)nmac;
	(void)d;
	(void)nd;
	return CURLE_NOT_BUILT_IN;
}

CURLcode
curl_easy_ssls_export(CURL *h, curl_ssls_export_cb *f, void *arg)
{
	(void)h;
	(void)f;
	(void)arg;
	return CURLE_NOT_BUILT_IN;
}

CURLcode
curl_easy_pause(CURL *h, int bitmask)
{
	(void)h;
	(void)bitmask;
	return CURLE_OK;
}

CURLM*
curl_multi_init(void)
{
	return calloc(1, sizeof(Multi));
}

CURLMcode
curl_multi_add_handle(CURLM *mh, CURL *h)
{
	Multi *m;
	Easy *e, **p;

	m = mh;
	e = h;
	if(m == NULL)
		return CURLM_BAD_HANDLE;
	if(e == NULL)
		return CURLM_BAD_EASY_HANDLE;
	if(e->multi)
		return CURLM_ADDED_ALREADY;
	for(p = &m->easy; *p; p = &(*p)->next)
		;
	*p = e;
	e->next = NULL;
	e->multi = m;
	e->done = 0;
	return CURLM_OK;
}

CURLMcode
curl_multi_remove_handle(CURLM *mh, CURL *h)
{
	Multi *m;
	Easy *e, **p;
	int i, j;

	m = mh;
	e = h;
	if(m == NULL)
		return CURLM_BAD_HANDLE;
	if(e == NULL)
		return CURLM_BAD_EASY_HANDLE;
	for(p = &m->easy; *p; p = &(*p)->next)
		if(*p == e){
			*p = e->next;
			break;
		}
	for(i = j = m->taken; i < m->nmsgs; i++)
		if(m->msgs[i].easy_handle != e)
			m->msgs[j++] = m->msgs[i];
	m->nmsgs = j;
	e->multi = NULL;
	e->next = NULL;
	return CURLM_OK;
}

CURLMcode
curl_multi_perform(CURLM *mh, int *running)
{
	Multi *m;
	Easy *e;
	CURLMsg *g;

	if((m = mh) == NULL)
		return CURLM_BAD_HANDLE;
	for(e = m->easy; e; e = e->next){
		if(e->done)
			continue;
		e->result = perform(e);
		e->done = 1;
		if((g = realloc(m->msgs, (m->nmsgs+1)*sizeof *g)) == NULL)
			return CURLM_OUT_OF_MEMORY;
		m->msgs = g;
		memset(&g[m->nmsgs], 0, sizeof g[0]);
		g[m->nmsgs].msg = CURLMSG_DONE;
		g[m->nmsgs].easy_handle = e;
		g[m->nmsgs].data.result = e->result;
		m->nmsgs++;
	}
	if(running)
		*running = 0;
	return CURLM_OK;
}

CURLMsg*
curl_multi_info_read(CURLM *mh, int *left)
{
	Multi *m;

	m = mh;
	if(m == NULL || m->taken >= m->nmsgs){
		if(left)
			*left = 0;
		if(m){
			m->taken = m->nmsgs = 0;
		}
		return NULL;
	}
	if(left)
		*left = m->nmsgs - m->taken - 1;
	return &m->msgs[m->taken++];
}

CURLMcode
curl_multi_fdset(CURLM *mh, fd_set *r, fd_set *w, fd_set *x, int *max)
{
	(void)mh;
	(void)r;
	(void)w;
	(void)x;
	if(max)
		*max = -1;
	return CURLM_OK;
}

CURLMcode
curl_multi_timeout(CURLM *mh, long *ms)
{
	Multi *m;
	Easy *e;

	m = mh;
	*ms = -1;
	if(m)
		for(e = m->easy; e; e = e->next)
			if(!e->done)
				*ms = 0;
	return CURLM_OK;
}

CURLMcode
curl_multi_wait(CURLM *mh, struct curl_waitfd *fds, unsigned int n, int ms, int *ret)
{
	(void)mh;
	(void)fds;
	(void)n;
	(void)ms;
	if(ret)
		*ret = 0;
	return CURLM_OK;
}

CURLMcode
curl_multi_poll(CURLM *mh, struct curl_waitfd *fds, unsigned int n, int ms, int *ret)
{
	return curl_multi_wait(mh, fds, n, ms, ret);
}

CURLMcode
curl_multi_wakeup(CURLM *mh)
{
	(void)mh;
	return CURLM_OK;
}

CURLMcode
curl_multi_setopt(CURLM *mh, CURLMoption opt, ...)
{
	(void)opt;
	return mh ? CURLM_OK : CURLM_BAD_HANDLE;
}

CURLMcode
curl_multi_get_offt(CURLM *mh, CURLMinfo_offt info, curl_off_t *v)
{
	Multi *m;
	Easy *e;
	curl_off_t n, done;

	if((m = mh) == NULL)
		return CURLM_BAD_HANDLE;
	if(v == NULL)
		return CURLM_BAD_FUNCTION_ARGUMENT;
	n = done = 0;
	for(e = m->easy; e; e = e->next){
		n++;
		if(e->done)
			done++;
	}
	switch(info){
	case CURLMINFO_XFERS_CURRENT: *v = n; break;
	case CURLMINFO_XFERS_RUNNING: *v = 0; break;
	case CURLMINFO_XFERS_PENDING: *v = n - done; break;
	case CURLMINFO_XFERS_DONE: *v = done; break;
	case CURLMINFO_XFERS_ADDED: *v = n; break;
	default: return CURLM_UNKNOWN_OPTION;
	}
	return CURLM_OK;
}

CURLMcode
curl_multi_cleanup(CURLM *mh)
{
	Multi *m;
	Easy *e, *n;

	if((m = mh) == NULL)
		return CURLM_BAD_HANDLE;
	for(e = m->easy; e; e = n){
		n = e->next;
		e->multi = NULL;
		e->next = NULL;
	}
	free(m->msgs);
	free(m);
	return CURLM_OK;
}

CURLcode
curl_global_init(long flags)
{
	(void)flags;
	return CURLE_OK;
}

CURLcode
curl_global_init_mem(long flags, curl_malloc_callback m, curl_free_callback f, curl_realloc_callback r, curl_strdup_callback s, curl_calloc_callback c)
{
	(void)m;
	(void)f;
	(void)r;
	(void)s;
	(void)c;
	return curl_global_init(flags);
}

void
curl_global_cleanup(void)
{
}

CURLcode
curl_global_trace(const char *config)
{
	(void)config;
	return CURLE_OK;
}

CURLsslset
curl_global_sslset(curl_sslbackend id, const char *name, const curl_ssl_backend ***avail)
{
	static const curl_ssl_backend *none[] = { NULL };

	(void)id;
	(void)name;
	if(avail)
		*avail = none;
	return CURLSSLSET_UNKNOWN_BACKEND;
}

char*
curl_version(void)
{
	return "libcurl/" LIBCURL_VERSION " webfs";
}

static const char *const protocols[] = { "http", "https", NULL };
static const char *const featurenames[] = { "IPv6", "Largefile", "SSL", NULL };
static curl_version_info_data info = {
	CURLVERSION_NOW,
	LIBCURL_VERSION,
	LIBCURL_VERSION_NUM,
	"wasm32-unknown-plan9",
	CURL_VERSION_IPV6 | CURL_VERSION_LARGEFILE | CURL_VERSION_SSL,
	"webfs",
	NULL,
	protocols,
};

curl_version_info_data*
curl_version_info(CURLversion v)
{
	(void)v;
	info.feature_names = featurenames;
	return &info;
}
