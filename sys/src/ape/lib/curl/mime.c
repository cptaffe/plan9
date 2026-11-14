#define _POSIX_C_SOURCE 200809L
#define CURL_DISABLE_TYPECHECK
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "web.h"

typedef struct Buf Buf;

struct Buf {
	char	*p;
	size_t	n;
	size_t	max;
	int	bad;
};

struct curl_mimepart {
	curl_mimepart	*next;
	curl_mime	*parent;
	char	*name;
	char	*filename;
	char	*type;
	char	*encoder;
	char	*data;
	size_t	ndata;
	char	*file;
	curl_read_callback	rd;
	curl_free_callback	fr;
	void	*arg;
	struct curl_slist	*headers;
	int	ownheaders;
	curl_mime	*sub;
};

struct curl_mime {
	curl_mimepart	*first;
	curl_mimepart	*last;
	curl_mimepart	*parent;
	char	boundary[48];
};

static unsigned long seq;

curl_mime*
curl_mime_init(CURL *e)
{
	curl_mime *m;

	(void)e;
	if((m = calloc(1, sizeof *m)) == NULL)
		return NULL;
	snprintf(m->boundary, sizeof m->boundary, "------------------------p9web%08lx%04lx", (unsigned long)m, ++seq);
	return m;
}

static void
clearpart(curl_mimepart *p)
{
	if(p->fr)
		p->fr(p->arg);
	p->fr = NULL;
	p->rd = NULL;
	free(p->data);
	p->data = NULL;
	p->ndata = 0;
	free(p->file);
	p->file = NULL;
	if(p->sub)
		curl_mime_free(p->sub);
	p->sub = NULL;
}

void
curl_mime_free(curl_mime *m)
{
	curl_mimepart *p, *n;

	if(m == NULL)
		return;
	for(p = m->first; p; p = n){
		n = p->next;
		clearpart(p);
		free(p->name);
		free(p->filename);
		free(p->type);
		free(p->encoder);
		if(p->ownheaders)
			curl_slist_free_all(p->headers);
		free(p);
	}
	free(m);
}

curl_mimepart*
curl_mime_addpart(curl_mime *m)
{
	curl_mimepart *p;

	if(m == NULL || (p = calloc(1, sizeof *p)) == NULL)
		return NULL;
	p->parent = m;
	if(m->last)
		m->last->next = p;
	else
		m->first = p;
	m->last = p;
	return p;
}

static CURLcode
setstr(char **d, const char *s)
{
	free(*d);
	*d = NULL;
	if(s && (*d = strdup(s)) == NULL)
		return CURLE_OUT_OF_MEMORY;
	return CURLE_OK;
}

CURLcode
curl_mime_name(curl_mimepart *p, const char *s)
{
	return p ? setstr(&p->name, s) : CURLE_BAD_FUNCTION_ARGUMENT;
}

CURLcode
curl_mime_filename(curl_mimepart *p, const char *s)
{
	return p ? setstr(&p->filename, s) : CURLE_BAD_FUNCTION_ARGUMENT;
}

CURLcode
curl_mime_type(curl_mimepart *p, const char *s)
{
	return p ? setstr(&p->type, s) : CURLE_BAD_FUNCTION_ARGUMENT;
}

CURLcode
curl_mime_encoder(curl_mimepart *p, const char *s)
{
	if(p == NULL)
		return CURLE_BAD_FUNCTION_ARGUMENT;
	if(s && strcmp(s, "binary") != 0 && strcmp(s, "8bit") != 0 && strcmp(s, "7bit") != 0 && strcmp(s, "base64") != 0)
		return CURLE_BAD_FUNCTION_ARGUMENT;
	return setstr(&p->encoder, s);
}

CURLcode
curl_mime_data(curl_mimepart *p, const char *d, size_t n)
{
	if(p == NULL)
		return CURLE_BAD_FUNCTION_ARGUMENT;
	clearpart(p);
	if(d == NULL)
		return CURLE_OK;
	if(n == CURL_ZERO_TERMINATED)
		n = strlen(d);
	if((p->data = malloc(n+1)) == NULL)
		return CURLE_OUT_OF_MEMORY;
	memcpy(p->data, d, n);
	p->data[n] = 0;
	p->ndata = n;
	return CURLE_OK;
}

CURLcode
curl_mime_filedata(curl_mimepart *p, const char *file)
{
	const char *b;

	if(p == NULL)
		return CURLE_BAD_FUNCTION_ARGUMENT;
	clearpart(p);
	if(file == NULL)
		return CURLE_OK;
	if((p->file = strdup(file)) == NULL)
		return CURLE_OUT_OF_MEMORY;
	b = strrchr(file, '/');
	return setstr(&p->filename, b ? b+1 : file);
}

CURLcode
curl_mime_data_cb(curl_mimepart *p, curl_off_t size, curl_read_callback rd, curl_seek_callback seek, curl_free_callback fr, void *arg)
{
	(void)size;
	(void)seek;
	if(p == NULL)
		return CURLE_BAD_FUNCTION_ARGUMENT;
	clearpart(p);
	p->rd = rd;
	p->fr = fr;
	p->arg = arg;
	return CURLE_OK;
}

CURLcode
curl_mime_subparts(curl_mimepart *p, curl_mime *m)
{
	if(p == NULL)
		return CURLE_BAD_FUNCTION_ARGUMENT;
	clearpart(p);
	p->sub = m;
	if(m)
		m->parent = p;
	return CURLE_OK;
}

CURLcode
curl_mime_headers(curl_mimepart *p, struct curl_slist *h, int own)
{
	if(p == NULL)
		return CURLE_BAD_FUNCTION_ARGUMENT;
	if(p->ownheaders)
		curl_slist_free_all(p->headers);
	p->headers = h;
	p->ownheaders = own;
	return CURLE_OK;
}

static void
add(Buf *b, const char *s, size_t n)
{
	char *q;
	size_t m;

	if(b->bad)
		return;
	if(b->n + n + 1 > b->max){
		m = b->max ? b->max : 4096;
		while(m < b->n + n + 1)
			m *= 2;
		if((q = realloc(b->p, m)) == NULL){
			b->bad = 1;
			return;
		}
		b->p = q;
		b->max = m;
	}
	memcpy(b->p + b->n, s, n);
	b->n += n;
	b->p[b->n] = 0;
}

static void
adds(Buf *b, const char *s)
{
	add(b, s, strlen(s));
}

static void
quoted(Buf *b, const char *s)
{
	for(; *s; s++){
		if(*s == '"')
			adds(b, "%22");
		else if(*s == '\r')
			adds(b, "%0D");
		else if(*s == '\n')
			adds(b, "%0A");
		else
			add(b, s, 1);
	}
}

static void
base64(Buf *b, const char *s, size_t n)
{
	static const char t[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	char o[4];
	size_t i;
	unsigned v;
	int col;

	col = 0;
	for(i = 0; i < n; i += 3){
		v = (unsigned char)s[i] << 16;
		if(i+1 < n)
			v |= (unsigned char)s[i+1] << 8;
		if(i+2 < n)
			v |= (unsigned char)s[i+2];
		o[0] = t[v>>18&63];
		o[1] = t[v>>12&63];
		o[2] = i+1 < n ? t[v>>6&63] : '=';
		o[3] = i+2 < n ? t[v&63] : '=';
		add(b, o, 4);
		if((col += 4) >= 76){
			adds(b, "\r\n");
			col = 0;
		}
	}
	if(col)
		adds(b, "\r\n");
}

static int
content(curl_mimepart *p, Buf *b)
{
	char buf[8192];
	FILE *f;
	size_t n;

	if(p->data){
		add(b, p->data, p->ndata);
		return 0;
	}
	if(p->file){
		if((f = fopen(p->file, "rb")) == NULL)
			return -1;
		while((n = fread(buf, 1, sizeof buf, f)) > 0)
			add(b, buf, n);
		fclose(f);
		return 0;
	}
	if(p->rd){
		for(;;){
			n = p->rd(buf, 1, sizeof buf, p->arg);
			if(n == CURL_READFUNC_ABORT || n == CURL_READFUNC_PAUSE || n > sizeof buf)
				return -1;
			if(n == 0)
				break;
			add(b, buf, n);
		}
	}
	return 0;
}

static int body(curl_mime*, Buf*, int);

static int
part(curl_mimepart *p, Buf *b, int form)
{
	struct curl_slist *h;
	Buf c;

	memset(&c, 0, sizeof c);
	if(p->sub){
		if(body(p->sub, &c, 0) < 0)
			return -1;
	}else if(content(p, &c) < 0)
		return -1;
	if(form || p->name || p->filename){
		adds(b, "Content-Disposition: ");
		adds(b, form ? "form-data" : "attachment");
		if(p->name){
			adds(b, "; name=\"");
			quoted(b, p->name);
			adds(b, "\"");
		}
		if(p->filename){
			adds(b, "; filename=\"");
			quoted(b, p->filename);
			adds(b, "\"");
		}
		adds(b, "\r\n");
	}
	if(p->sub){
		adds(b, "Content-Type: multipart/mixed; boundary=");
		adds(b, p->sub->boundary);
		adds(b, "\r\n");
	}else if(p->type || p->filename){
		adds(b, "Content-Type: ");
		adds(b, p->type ? p->type : "application/octet-stream");
		adds(b, "\r\n");
	}
	if(p->encoder){
		adds(b, "Content-Transfer-Encoding: ");
		adds(b, p->encoder);
		adds(b, "\r\n");
	}
	for(h = p->headers; h; h = h->next){
		adds(b, h->data);
		adds(b, "\r\n");
	}
	adds(b, "\r\n");
	if(p->encoder && strcmp(p->encoder, "base64") == 0)
		base64(b, c.p ? c.p : "", c.n);
	else
		add(b, c.p ? c.p : "", c.n);
	free(c.p);
	return c.bad ? -1 : 0;
}

static int
body(curl_mime *m, Buf *b, int form)
{
	curl_mimepart *p;

	for(p = m->first; p; p = p->next){
		adds(b, "--");
		adds(b, m->boundary);
		adds(b, "\r\n");
		if(part(p, b, form) < 0)
			return -1;
		adds(b, "\r\n");
	}
	adds(b, "--");
	adds(b, m->boundary);
	adds(b, "--\r\n");
	return b->bad ? -1 : 0;
}

char*
_curl_mimebody(curl_mime *m, size_t *n, char *type, size_t ntype)
{
	Buf b;

	memset(&b, 0, sizeof b);
	if(body(m, &b, 1) < 0){
		free(b.p);
		return NULL;
	}
	snprintf(type, ntype, "multipart/form-data; boundary=%s", m->boundary);
	*n = b.n;
	return b.p ? b.p : strdup("");
}

CURLSH*
curl_share_init(void)
{
	return calloc(1, 1);
}

CURLSHcode
curl_share_setopt(CURLSH *s, CURLSHoption opt, ...)
{
	(void)opt;
	return s ? CURLSHE_OK : CURLSHE_INVALID;
}

CURLSHcode
curl_share_cleanup(CURLSH *s)
{
	free(s);
	return CURLSHE_OK;
}

CURLMcode
curl_multi_notify_enable(CURLM *m, unsigned int what)
{
	(void)what;
	return m ? CURLM_OK : CURLM_BAD_HANDLE;
}

CURLMcode
curl_multi_notify_disable(CURLM *m, unsigned int what)
{
	(void)what;
	return m ? CURLM_OK : CURLM_BAD_HANDLE;
}
