#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <sys/time.h>
#include <webgpu/webgpu.h>

extern int _ERRSTR(char*, unsigned int);

typedef unsigned char uchar;
typedef struct Gpu Gpu;
typedef struct Pend Pend;
typedef struct Ent Ent;
typedef struct Fut Fut;
typedef struct Refs Refs;

#define nelem(x)	(sizeof(x)/sizeof((x)[0]))
#define Map	(WGPUBufferUsage_MapRead|WGPUBufferUsage_MapWrite)

enum
{
	Chunk = 16<<20,
	Flush = 4<<20,
	Nfut = 64,
	Ngroup = 4,
};

enum
{
	Fmap = 1,
	Fdone,
};

struct Pend
{
	Pend	*next;
	WGPUBuffer	b;
	uint64_t	off;
	size_t	n;
	uint64_t	seq;
	uint64_t	fut;
	size_t	got;
	uchar	t[4];
};

struct Ent
{
	uint32_t	binding;
	WGPUBuffer	b;
	uint64_t	off;
	uint64_t	size;
};

struct Fut
{
	uint64_t	id;
	int	kind;
	int	sent;
	int	idle;
	char	why[256];
	uint64_t	want;
	uint64_t	nsub;
	WGPUBuffer	b;
	WGPUBufferMapCallbackInfo	map;
	WGPUQueueWorkDoneCallbackInfo	done;
};

struct Refs
{
	WGPUBuffer	*b;
	int	nb;
	WGPUComputePipeline	*p;
	int	np;
};

struct Gpu
{
	int	ref;
	int	fd;
	char	*ctl;
	uchar	*out;
	size_t	nout;
	size_t	aout;
	Pend	*ph;
	Pend	**pt;
	uint32_t	ids;
	uint32_t	tags;
	uint64_t	futs;
	uint64_t	seq;
	uint64_t	nsub;
	Fut	fut[Nfut];
	char	err[256];
	int	busy;
	WGPUDevice	dev;
};

struct WGPUInstanceImpl { int ref; Gpu *g; };
struct WGPUAdapterImpl { int ref; Gpu *g; };
struct WGPUQueueImpl { int ref; Gpu *g; };
struct WGPUDeviceImpl
{
	int	ref;
	Gpu	*g;
	WGPUQueue	q;
	WGPUUncapturedErrorCallbackInfo	uncap;
};
struct WGPUBufferImpl
{
	int	ref;
	Gpu	*g;
	uint32_t	id;
	uint64_t	size;
	WGPUBufferUsage	usage;
	uchar	*shadow;
	char	*why;
	uint64_t	last;
	int	gone;
};
struct WGPUShaderModuleImpl { int ref; Gpu *g; uint32_t id; };
struct WGPUComputePipelineImpl { int ref; Gpu *g; uint32_t id; };
struct WGPUBindGroupLayoutImpl { int ref; WGPUComputePipeline p; uint32_t group; };
struct WGPUBindGroupImpl
{
	int	ref;
	uint32_t	group;
	int	n;
	int	bad;
	Ent	*e;
};
struct WGPUCommandEncoderImpl
{
	int	ref;
	Gpu	*g;
	uchar	*m;
	size_t	nm;
	size_t	am;
	Pend	*ph;
	Pend	**pt;
	Refs	r;
};
struct WGPUComputePassEncoderImpl
{
	int	ref;
	WGPUCommandEncoder	e;
	WGPUComputePipeline	p;
	WGPUBindGroup	bg[Ngroup];
};
struct WGPUCommandBufferImpl
{
	int	ref;
	uchar	*m;
	size_t	nm;
	Pend	*ph;
	Pend	**pt;
	Refs	r;
};

static WGPUStringView
sv(char *s)
{
	WGPUStringView v;

	v.data = s;
	v.length = s != NULL ? strlen(s) : 0;
	return v;
}

static size_t
svlen(WGPUStringView v)
{
	if(v.data == NULL)
		return 0;
	return v.length == WGPU_STRLEN ? strlen(v.data) : v.length;
}

static void*
emalloc(size_t n)
{
	void *p;

	p = calloc(1, n > 0 ? n : 1);
	if(p == NULL){
		fprintf(stderr, "webgpu: out of memory\n");
		abort();
	}
	return p;
}

static void*
erealloc(void *p, size_t n)
{
	p = realloc(p, n > 0 ? n : 1);
	if(p == NULL){
		fprintf(stderr, "webgpu: out of memory\n");
		abort();
	}
	return p;
}

static void
grow(uchar **b, size_t *a, size_t need)
{
	size_t n;

	if(need <= *a)
		return;
	n = *a > 0 ? *a : 4096;
	while(n < need)
		n *= 2;
	*b = erealloc(*b, n);
	*a = n;
}

static void
put4(uchar *p, uint32_t v)
{
	p[0] = v;
	p[1] = v>>8;
	p[2] = v>>16;
	p[3] = v>>24;
}

static uchar*
room(uchar **b, size_t *n, size_t *a, size_t k)
{
	uchar *p;

	grow(b, a, *n+k);
	p = *b + *n;
	*n += k;
	return p;
}

static void
msg(uchar **b, size_t *n, size_t *a, int c, uint32_t *v, int nv, void *d, size_t nd)
{
	uchar *p;
	int i;

	p = room(b, n, a, 1+4*nv+nd);
	*p++ = c;
	for(i = 0; i < nv; i++, p += 4)
		put4(p, v[i]);
	if(nd > 0)
		memmove(p, d, nd);
}

static void
gmsg(Gpu *g, int c, uint32_t *v, int nv, void *d, size_t nd)
{
	msg(&g->out, &g->nout, &g->aout, c, v, nv, d, nd);
}

static void
emsg(WGPUCommandEncoder e, int c, uint32_t *v, int nv)
{
	msg(&e->m, &e->nm, &e->am, c, v, nv, NULL, 0);
}

static void
fail(Gpu *g, char *s)
{
	WGPUDevice d;
	WGPUUncapturedErrorCallbackInfo *u;

	if(s != g->err)
		snprintf(g->err, sizeof g->err, "%s", s);
	d = g->dev;
	if(d == NULL)
		return;
	u = &d->uncap;
	if(u->callback != NULL)
		u->callback(&d, WGPUErrorType_Validation, sv(s), u->userdata1, u->userdata2);
}

static void
failf(Gpu *g, char *what, char *why)
{
	char e[256];

	snprintf(e, sizeof e, "webgpu: %s: %s", what, why);
	fail(g, e);
}

static int
fits(Gpu *g, char *what, uint64_t v)
{
	if(v <= UINT32_MAX)
		return 1;
	failf(g, what, "more than 4 GiB");
	return 0;
}

static int
inside(Gpu *g, char *what, uint64_t off, uint64_t n, uint64_t size)
{
	if(n <= size && off <= size-n)
		return 1;
	failf(g, what, "past the end");
	return 0;
}

static void
errtext(char *e, size_t n)
{
	char t[256];

	e[0] = 0;
	_ERRSTR(e, n);
	snprintf(t, sizeof t, "%s", e);
	_ERRSTR(t, sizeof t);
	if(e[0] == 0)
		snprintf(e, n, "i/o error");
}

static void
syserr(Gpu *g)
{
	char e[256];

	errtext(e, sizeof e);
	failf(g, "data", e);
}

static int
send(Gpu *g, void *p, size_t n)
{
	if(n == 0)
		return 0;
	if(write(g->fd, p, n) != (long)n){
		syserr(g);
		return -1;
	}
	return 0;
}

static int
flush(Gpu *g)
{
	int r;

	r = send(g, g->out, g->nout);
	g->nout = 0;
	return r;
}

static volatile sig_atomic_t rang;
static struct sigaction oldact;
static struct itimerval oldit;

static void
ring(int sig)
{
	(void)sig;
	rang = 1;
}

static void
arm(long ms)
{
	struct sigaction a;
	struct itimerval v;

	memset(&a, 0, sizeof a);
	a.sa_handler = ring;
	sigemptyset(&a.sa_mask);
	sigaction(SIGALRM, &a, &oldact);
	rang = 0;
	memset(&v, 0, sizeof v);
	v.it_value.tv_sec = ms/1000;
	v.it_value.tv_usec = ms%1000*1000;
	setitimer(ITIMER_REAL, &v, &oldit);
}

static void
disarm(void)
{
	struct itimerval z;

	memset(&z, 0, sizeof z);
	setitimer(ITIMER_REAL, &z, NULL);
	sigaction(SIGALRM, &oldact, NULL);
	if(oldit.it_value.tv_sec != 0 || oldit.it_value.tv_usec != 0)
		setitimer(ITIMER_REAL, &oldit, NULL);
}

static void
bufrelease(WGPUBuffer b);

static void
pop(Gpu *g)
{
	Pend *p;

	p = g->ph;
	g->ph = p->next;
	if(g->ph == NULL)
		g->pt = &g->ph;
	bufrelease(p->b);
	free(p);
}

static void
pendfail(Gpu *g, Pend *p, char *why)
{
	Fut *f;

	failf(g, "data", why);
	if(p->b != NULL){
		if(p->b->why == NULL){
			p->b->why = emalloc(strlen(g->err)+1);
			strcpy(p->b->why, g->err);
		}
		return;
	}
	f = &g->fut[p->fut % Nfut];
	if(f->id == p->fut && f->kind == Fdone && f->why[0] == 0)
		snprintf(f->why, sizeof f->why, "%s", g->err);
}

static int
drain(Gpu *g, uint64_t want, long ms)
{
	Pend *p;
	uchar *d;
	size_t n;
	long k;
	int r;
	char e[256];

	r = 0;
	rang = 0;
	if(ms >= 0)
		arm(ms > 0 ? ms : 1);
	while((p = g->ph) != NULL && p->seq <= want){
		d = p->b != NULL ? p->b->shadow+p->off : p->t;
		n = p->b != NULL ? p->n : 4;
		while(p->got < n){
			if(rang){
				r = 1;
				goto out;
			}
			k = read(g->fd, d+p->got, n-p->got);
			if(k < 0 && errno == EINTR)
				continue;
			if(k < 0){
				errtext(e, sizeof e);
				pendfail(g, p, e);
				break;
			}
			if(k == 0){
				while(g->ph != NULL){
					pendfail(g, g->ph, "no more to read");
					pop(g);
				}
				goto out;
			}
			p->got += k;
		}
		pop(g);
	}
out:
	if(ms >= 0)
		disarm();
	return r;
}

static uint32_t
newid(Gpu *g)
{
	return ++g->ids;
}

static char*
ctlline(Gpu *g, char *k)
{
	char *p;
	size_t n;

	n = strlen(k);
	for(p = g->ctl; p != NULL && *p; p = strchr(p, '\n'), p = p ? p+1 : NULL)
		if(strncmp(p, k, n) == 0 && (p[n] == ' ' || p[n] == '\n'))
			return p+n;
	return NULL;
}

static uint64_t
ctlnum(Gpu *g, char *k, uint64_t def)
{
	char *p;

	p = ctlline(g, k);
	return p != NULL ? strtoull(p, NULL, 10) : def;
}

static char*
ctlstr(Gpu *g, char *k)
{
	char *p, *e, *s;

	p = ctlline(g, k);
	if(p == NULL)
		return strdup("");
	if(*p == ' ')
		p++;
	e = strchr(p, '\n');
	if(e == NULL)
		e = p+strlen(p);
	s = emalloc(e-p+1);
	memmove(s, p, e-p);
	return s;
}

static int
ctlhas(Gpu *g, char *k, char *w)
{
	char *p, *e;
	size_t n;

	p = ctlline(g, k);
	if(p == NULL)
		return 0;
	e = strchr(p, '\n');
	if(e == NULL)
		e = p+strlen(p);
	n = strlen(w);
	while(p < e){
		while(p < e && *p == ' ')
			p++;
		if((size_t)(e-p) >= n && strncmp(p, w, n) == 0 && (p+n == e || p[n] == ' '))
			return 1;
		while(p < e && *p != ' ')
			p++;
	}
	return 0;
}

static char*
slurp(char *path)
{
	char *s;
	int f;
	long k;
	size_t n, a;

	if((f = open(path, O_RDONLY)) < 0)
		return NULL;
	s = NULL;
	n = a = 0;
	for(;;){
		grow((uchar**)&s, &a, n+4097);
		k = read(f, s+n, 4096);
		if(k <= 0)
			break;
		n += k;
	}
	close(f);
	s[n] = 0;
	return s;
}

static Gpu*
gpuopen(void)
{
	Gpu *g;
	char d[64], *ctl;
	int c, n, fd;

	if((c = open("/dev/gpu/clone", O_RDONLY)) < 0)
		return NULL;
	n = read(c, d, sizeof d-1);
	if(n <= 0){
		close(c);
		return NULL;
	}
	d[n] = 0;
	n = atoi(d);
	snprintf(d, sizeof d, "/dev/gpu/%d/ctl", n);
	ctl = slurp(d);
	if(ctl == NULL || strncmp(ctl, "none", 4) == 0){
		close(c);
		free(ctl);
		return NULL;
	}
	snprintf(d, sizeof d, "/dev/gpu/%d/data", n);
	fd = open(d, O_RDWR);
	close(c);
	if(fd < 0){
		free(ctl);
		return NULL;
	}
	g = emalloc(sizeof *g);
	g->ref = 1;
	g->pt = &g->ph;
	g->fd = fd;
	g->ctl = ctl;
	return g;
}

static void
gpuclose(Gpu *g)
{
	if(--g->ref > 0)
		return;
	flush(g);
	drain(g, UINT64_MAX, -1);
	close(g->fd);
	free(g->ctl);
	free(g->out);
	free(g);
}

static void
refbuf(Refs *r, WGPUBuffer b)
{
	if(r->nb > 0 && r->b[r->nb-1] == b)
		return;
	if((r->nb & (r->nb-1)) == 0)
		r->b = erealloc(r->b, (r->nb ? 2*r->nb : 1)*sizeof r->b[0]);
	r->b[r->nb++] = b;
	b->ref++;
}

static void
refpipe(Refs *r, WGPUComputePipeline p)
{
	if(r->np > 0 && r->p[r->np-1] == p)
		return;
	if((r->np & (r->np-1)) == 0)
		r->p = erealloc(r->p, (r->np ? 2*r->np : 1)*sizeof r->p[0]);
	r->p[r->np++] = p;
	p->ref++;
}

static void
unrefs(Refs *r)
{
	int i;

	for(i = 0; i < r->nb; i++)
		bufrelease(r->b[i]);
	for(i = 0; i < r->np; i++)
		wgpuComputePipelineRelease(r->p[i]);
	free(r->b);
	free(r->p);
	memset(r, 0, sizeof *r);
}

static void
freepend(Pend *p)
{
	Pend *q;

	for(; p != NULL; p = q){
		q = p->next;
		bufrelease(p->b);
		free(p);
	}
}

static int
submit(Gpu *g, Pend **t)
{
	if(flush(g) == 0)
		return 0;
	freepend(*t);
	*t = NULL;
	g->pt = t;
	return -1;
}

static WGPUFuture
future(Gpu *g, int kind, Fut **fp)
{
	Fut *f;
	WGPUFuture r;

	r.id = ++g->futs;
	f = &g->fut[r.id % Nfut];
	memset(f, 0, sizeof *f);
	f->id = r.id;
	f->kind = kind;
	*fp = f;
	return r;
}

static void
send1(Gpu *g, Fut *f)
{
	uint32_t v;
	Pend **t;

	if(f->sent)
		return;
	f->sent = 1;
	if(f->why[0] != 0)
		return;
	if(f->kind == Fdone && !g->busy){
		f->idle = 1;
		return;
	}
	t = g->pt;
	if(f->kind == Fdone){
		v = ++g->tags;
		gmsg(g, 'F', &v, 1, NULL, 0);
		*g->pt = emalloc(sizeof(Pend));
		(*g->pt)->seq = ++g->seq;
		(*g->pt)->fut = f->id;
		g->pt = &(*g->pt)->next;
	}
	if(submit(g, t) < 0)
		snprintf(f->why, sizeof f->why, "%s", g->err);
	f->want = f->kind == Fmap ? f->b->last : g->seq;
	f->nsub = g->nsub;
}

static int
ready(Gpu *g, Fut *f)
{
	return f->why[0] != 0 || f->idle || g->ph == NULL || g->ph->seq > f->want;
}

static void
finish(Gpu *g, Fut *f)
{
	WGPUBuffer b;
	int ok;

	if(f->kind == Fmap){
		b = f->b;
		if(f->why[0] == 0 && b->why != NULL)
			snprintf(f->why, sizeof f->why, "%s", b->why);
		free(b->why);
		b->why = NULL;
		ok = f->why[0] == 0;
		if(f->map.callback != NULL)
			f->map.callback(ok ? WGPUMapAsyncStatus_Success : WGPUMapAsyncStatus_Error,
				sv(f->why), f->map.userdata1, f->map.userdata2);
		bufrelease(b);
	}else{
		if(!f->idle && g->nsub == f->nsub)
			g->busy = 0;
		ok = f->why[0] == 0;
		if(f->done.callback != NULL)
			f->done.callback(ok ? WGPUQueueWorkDoneStatus_Success
				: WGPUQueueWorkDoneStatus_Error,
				sv(f->why), f->done.userdata1, f->done.userdata2);
	}
	f->kind = 0;
}

static Fut*
waited(Gpu *g, WGPUFuture w)
{
	Fut *f;

	if(g == NULL || w.id == 0)
		return NULL;
	f = &g->fut[w.id % Nfut];
	return f->id == w.id && f->kind != 0 ? f : NULL;
}

WGPUInstance
wgpuCreateInstance(WGPUInstanceDescriptor const *d)
{
	WGPUInstance i;

	(void)d;
	i = emalloc(sizeof *i);
	i->ref = 1;
	return i;
}

void
wgpuInstanceAddRef(WGPUInstance i)
{
	i->ref++;
}

void
wgpuInstanceRelease(WGPUInstance i)
{
	if(--i->ref > 0)
		return;
	if(i->g != NULL)
		gpuclose(i->g);
	free(i);
}

WGPUFuture
wgpuInstanceRequestAdapter(WGPUInstance i, WGPURequestAdapterOptions const *o,
	WGPURequestAdapterCallbackInfo cb)
{
	WGPUAdapter a;
	WGPUFuture f;

	(void)o;
	if(i->g == NULL)
		i->g = gpuopen();
	f.id = 0;
	if(i->g == NULL){
		if(cb.callback != NULL)
			cb.callback(WGPURequestAdapterStatus_Unavailable, NULL, sv("no GPU"),
				cb.userdata1, cb.userdata2);
		return f;
	}
	a = emalloc(sizeof *a);
	a->ref = 1;
	a->g = i->g;
	a->g->ref++;
	if(cb.callback != NULL)
		cb.callback(WGPURequestAdapterStatus_Success, a, sv(""),
			cb.userdata1, cb.userdata2);
	return f;
}

static char *wgslnames[] = {
	[WGPUWGSLLanguageFeatureName_ReadonlyAndReadwriteStorageTextures] =
		"readonly_and_readwrite_storage_textures",
	[WGPUWGSLLanguageFeatureName_Packed4x8IntegerDotProduct] = "packed_4x8_integer_dot_product",
	[WGPUWGSLLanguageFeatureName_UnrestrictedPointerParameters] =
		"unrestricted_pointer_parameters",
	[WGPUWGSLLanguageFeatureName_PointerCompositeAccess] = "pointer_composite_access",
	[WGPUWGSLLanguageFeatureName_UniformBufferStandardLayout] =
		"uniform_buffer_standard_layout",
	[WGPUWGSLLanguageFeatureName_SubgroupId] = "subgroup_id",
	[WGPUWGSLLanguageFeatureName_TextureAndSamplerLet] = "texture_and_sampler_let",
	[WGPUWGSLLanguageFeatureName_SubgroupUniformity] = "subgroup_uniformity",
	[WGPUWGSLLanguageFeatureName_TextureFormatsTier1] = "texture_formats_tier1",
	[WGPUWGSLLanguageFeatureName_LinearIndexing] = "linear_indexing",
};

WGPUBool
wgpuInstanceHasWGSLLanguageFeature(WGPUInstance i, WGPUWGSLLanguageFeatureName f)
{
	if(i->g == NULL)
		i->g = gpuopen();
	if(i->g == NULL || f <= 0 || f >= nelem(wgslnames) || wgslnames[f] == NULL)
		return 0;
	return ctlhas(i->g, "wgsl", wgslnames[f]);
}

WGPUWaitStatus
wgpuInstanceWaitAny(WGPUInstance i, size_t n, WGPUFutureWaitInfo *w, uint64_t timeout)
{
	Gpu *g;
	Fut *f, *first;
	size_t k;
	long ms;
	int done;

	g = i->g;
	ms = timeout == UINT64_MAX ? -1 : timeout/1000000 >= LONG_MAX/2 ? LONG_MAX/2
		: (long)(timeout/1000000 + (timeout%1000000 != 0));
	done = 0;
	first = NULL;
	for(k = 0; k < n; k++){
		w[k].completed = 0;
		if((f = waited(g, w[k].future)) == NULL){
			w[k].completed = 1;
			done++;
			continue;
		}
		send1(g, f);
		if(!ready(g, f) && (first == NULL || f->want < first->want))
			first = f;
	}
	if(first != NULL && done == 0){
		for(k = 0; k < n; k++)
			if((f = waited(g, w[k].future)) != NULL && ready(g, f))
				break;
		if(k == n && drain(g, first->want, ms) == 1)
			return WGPUWaitStatus_TimedOut;
	}
	for(k = 0; k < n; k++)
		if((f = waited(g, w[k].future)) != NULL && ready(g, f)){
			finish(g, f);
			w[k].completed = 1;
			done++;
		}
	return done > 0 || n == 0 ? WGPUWaitStatus_Success : WGPUWaitStatus_TimedOut;
}

void
wgpuAdapterAddRef(WGPUAdapter a)
{
	a->ref++;
}

void
wgpuAdapterRelease(WGPUAdapter a)
{
	if(--a->ref > 0)
		return;
	gpuclose(a->g);
	free(a);
}

WGPUStatus
wgpuAdapterGetLimits(WGPUAdapter a, WGPULimits *l)
{
	Gpu *g;

	g = a->g;
	l->maxBindGroups = ctlnum(g, "maxBindGroups", 4);
	l->maxBindingsPerBindGroup = ctlnum(g, "maxBindingsPerBindGroup", 1000);
	l->maxStorageBuffersPerShaderStage = ctlnum(g, "maxStorageBuffersPerShaderStage", 8);
	l->maxUniformBuffersPerShaderStage = ctlnum(g, "maxUniformBuffersPerShaderStage", 12);
	l->maxUniformBufferBindingSize = ctlnum(g, "maxUniformBufferBindingSize", 65536);
	l->maxStorageBufferBindingSize = ctlnum(g, "maxStorageBufferBindingSize", 134217728);
	l->minUniformBufferOffsetAlignment = ctlnum(g, "minUniformBufferOffsetAlignment", 256);
	l->minStorageBufferOffsetAlignment = ctlnum(g, "minStorageBufferOffsetAlignment", 256);
	l->maxBufferSize = ctlnum(g, "maxBufferSize", 268435456);
	l->maxComputeWorkgroupStorageSize = ctlnum(g, "maxComputeWorkgroupStorageSize", 16384);
	l->maxComputeInvocationsPerWorkgroup = ctlnum(g, "maxComputeInvocationsPerWorkgroup", 256);
	l->maxComputeWorkgroupSizeX = ctlnum(g, "maxComputeWorkgroupSizeX", 256);
	l->maxComputeWorkgroupSizeY = ctlnum(g, "maxComputeWorkgroupSizeY", 256);
	l->maxComputeWorkgroupSizeZ = ctlnum(g, "maxComputeWorkgroupSizeZ", 64);
	l->maxComputeWorkgroupsPerDimension = ctlnum(g, "maxComputeWorkgroupsPerDimension", 65535);
	return WGPUStatus_Success;
}

static struct {
	char	*s;
	WGPUFeatureName	f;
} feats[] = {
	{"shader-f16", WGPUFeatureName_ShaderF16},
	{"subgroups", WGPUFeatureName_Subgroups},
	{"chromium-experimental-subgroup-matrix",
		WGPUFeatureName_ChromiumExperimentalSubgroupMatrix},
};

WGPUBool
wgpuAdapterHasFeature(WGPUAdapter a, WGPUFeatureName f)
{
	int i;

	if(f == WGPUFeatureName_ImplicitDeviceSynchronization)
		return 1;
	for(i = 0; i < nelem(feats); i++)
		if(feats[i].f == f)
			return ctlhas(a->g, "features", feats[i].s);
	return 0;
}

static WGPUSubgroupMatrixComponentType
comptype(char *s)
{
	static char *t[] = {
		[WGPUSubgroupMatrixComponentType_F32] = "f32",
		[WGPUSubgroupMatrixComponentType_F16] = "f16",
		[WGPUSubgroupMatrixComponentType_U32] = "u32",
		[WGPUSubgroupMatrixComponentType_I32] = "i32",
		[WGPUSubgroupMatrixComponentType_U8] = "u8",
		[WGPUSubgroupMatrixComponentType_I8] = "i8",
	};
	int i;

	for(i = 1; i < nelem(t); i++)
		if(t[i] != NULL && strcmp(s, t[i]) == 0)
			return i;
	return 0;
}

static void
matrices(Gpu *g, WGPUAdapterPropertiesSubgroupMatrixConfigs *m)
{
	WGPUSubgroupMatrixConfig *c;
	char *p, a[16], b[16];
	unsigned mm, nn, kk;
	int n;

	n = 0;
	for(p = g->ctl; (p = strstr(p, "subgroupMatrix ")) != NULL; p++)
		n++;
	c = emalloc(n*sizeof *c);
	n = 0;
	for(p = g->ctl; (p = strstr(p, "subgroupMatrix ")) != NULL; p++)
		if(sscanf(p, "subgroupMatrix %15s %15s %u %u %u", a, b, &mm, &nn, &kk) == 5){
			c[n].componentType = comptype(a);
			c[n].resultComponentType = comptype(b);
			c[n].M = mm;
			c[n].N = nn;
			c[n].K = kk;
			n++;
		}
	m->configCount = n;
	m->configs = c;
}

WGPUStatus
wgpuAdapterGetInfo(WGPUAdapter a, WGPUAdapterInfo *info)
{
	WGPUChainedStruct *c;
	Gpu *g;

	g = a->g;
	for(c = info->nextInChain; c != NULL; c = c->next)
		if(c->sType == WGPUSType_AdapterPropertiesSubgroupMatrixConfigs)
			matrices(g, (WGPUAdapterPropertiesSubgroupMatrixConfigs*)c);
	info->vendor = sv(ctlstr(g, "vendor"));
	info->architecture = sv(ctlstr(g, "architecture"));
	info->device = sv(ctlstr(g, "device"));
	info->description = sv(ctlstr(g, "description"));
	info->backendType = WGPUBackendType_WebGPU;
	info->adapterType = WGPUAdapterType_Unknown;
	info->subgroupMinSize = ctlnum(g, "subgroupMinSize", 4);
	info->subgroupMaxSize = ctlnum(g, "subgroupMaxSize", 128);
	return WGPUStatus_Success;
}

void
wgpuAdapterInfoFreeMembers(WGPUAdapterInfo info)
{
	free((char*)info.vendor.data);
	free((char*)info.architecture.data);
	free((char*)info.device.data);
	free((char*)info.description.data);
}

void
wgpuAdapterPropertiesSubgroupMatrixConfigsFreeMembers(WGPUAdapterPropertiesSubgroupMatrixConfigs m)
{
	free((void*)m.configs);
}

WGPUFuture
wgpuAdapterRequestDevice(WGPUAdapter a, WGPUDeviceDescriptor const *d,
	WGPURequestDeviceCallbackInfo cb)
{
	WGPUDevice v;
	WGPUFuture f;

	v = emalloc(sizeof *v);
	v->ref = 1;
	v->g = a->g;
	v->g->ref++;
	v->q = emalloc(sizeof *v->q);
	v->q->ref = 1;
	v->q->g = v->g;
	if(d != NULL)
		v->uncap = d->uncapturedErrorCallbackInfo;
	if(v->g->dev == NULL)
		v->g->dev = v;
	f.id = 0;
	if(cb.callback != NULL)
		cb.callback(WGPURequestDeviceStatus_Success, v, sv(""), cb.userdata1, cb.userdata2);
	return f;
}

void
wgpuDeviceAddRef(WGPUDevice d)
{
	d->ref++;
}

void
wgpuDeviceRelease(WGPUDevice d)
{
	if(--d->ref > 0)
		return;
	if(d->g->dev == d)
		d->g->dev = NULL;
	flush(d->g);
	drain(d->g, UINT64_MAX, -1);
	gpuclose(d->g);
	free(d);
}

WGPUQueue
wgpuDeviceGetQueue(WGPUDevice d)
{
	d->q->ref++;
	return d->q;
}

void
wgpuQueueAddRef(WGPUQueue q)
{
	q->ref++;
}

void
wgpuQueueRelease(WGPUQueue q)
{
	q->ref--;
}

WGPUBuffer
wgpuDeviceCreateBuffer(WGPUDevice d, WGPUBufferDescriptor const *desc)
{
	WGPUBuffer b;
	uint32_t v[3];

	b = emalloc(sizeof *b);
	b->ref = 1;
	b->g = d->g;
	b->size = desc->size;
	b->usage = desc->usage;
	if(!fits(d->g, "buffer", desc->size)){
		b->gone = 1;
		return b;
	}
	if(desc->usage & Map || desc->mappedAtCreation)
		b->shadow = emalloc(desc->size);
	if(desc->usage & WGPUBufferUsage_MapRead)
		return b;
	b->id = newid(d->g);
	v[0] = b->id;
	v[1] = desc->size;
	v[2] = desc->usage & ~Map;
	gmsg(d->g, 'b', v, 3, NULL, 0);
	return b;
}

static void
bufwrite(Gpu *g, WGPUBuffer b, uint64_t off, void const *data, size_t n)
{
	uint32_t v[3];
	size_t k;
	uchar h[13];

	if(b->id == 0){
		failf(g, "write", "not a buffer on the GPU");
		return;
	}
	if(off%4 != 0){
		failf(g, "write", "offset not a multiple of 4");
		return;
	}
	if(!inside(g, "write", off, n, b->size))
		return;
	while(n > 0){
		k = n < Chunk ? n : Chunk;
		v[0] = b->id;
		v[1] = off;
		v[2] = (k+3) & ~3;
		if(k < Flush || k != v[2]){
			gmsg(g, 'w', v, 3, (void*)data, k);
			if(v[2] != k)
				memset(room(&g->out, &g->nout, &g->aout, v[2]-k), 0, v[2]-k);
			if(g->nout >= Flush)
				flush(g);
		}else{
			flush(g);
			h[0] = 'w';
			put4(h+1, v[0]);
			put4(h+5, v[1]);
			put4(h+9, v[2]);
			if(send(g, h, sizeof h) < 0 || send(g, (void*)data, k) < 0)
				return;
		}
		off += k;
		n -= k;
		data = (uchar*)data + k;
	}
}

void
wgpuBufferAddRef(WGPUBuffer b)
{
	b->ref++;
}

void
wgpuBufferDestroy(WGPUBuffer b)
{
	uint32_t v;

	if(b->gone)
		return;
	b->gone = 1;
	if(b->id != 0){
		v = b->id;
		gmsg(b->g, 'f', &v, 1, NULL, 0);
	}
}

static void
bufrelease(WGPUBuffer b)
{
	if(b == NULL || --b->ref > 0)
		return;
	wgpuBufferDestroy(b);
	free(b->shadow);
	free(b->why);
	free(b);
}

void
wgpuBufferRelease(WGPUBuffer b)
{
	bufrelease(b);
}

uint64_t
wgpuBufferGetSize(WGPUBuffer b)
{
	return b->size;
}

WGPUFuture
wgpuBufferMapAsync(WGPUBuffer b, WGPUMapMode mode, size_t off, size_t n,
	WGPUBufferMapCallbackInfo cb)
{
	WGPUFuture r;
	Fut *f;

	(void)mode;
	r = future(b->g, Fmap, &f);
	f->b = b;
	b->ref++;
	f->map = cb;
	if(n == WGPU_WHOLE_MAP_SIZE)
		n = off <= b->size ? b->size-off : 0;
	if(b->shadow == NULL)
		failf(b->g, "map", "not a mappable buffer");
	else if(inside(b->g, "map", off, n, b->size))
		return r;
	snprintf(f->why, sizeof f->why, "%s", b->g->err);
	return r;
}

void*
wgpuBufferGetMappedRange(WGPUBuffer b, size_t off, size_t n)
{
	if(n == WGPU_WHOLE_MAP_SIZE)
		n = off <= b->size ? b->size-off : 0;
	if(b->shadow == NULL || n > b->size || off > b->size-n)
		return NULL;
	return b->shadow+off;
}

void const*
wgpuBufferGetConstMappedRange(WGPUBuffer b, size_t off, size_t n)
{
	return wgpuBufferGetMappedRange(b, off, n);
}

void
wgpuBufferUnmap(WGPUBuffer b)
{
	if(b->id == 0 || b->shadow == NULL)
		return;
	bufwrite(b->g, b, 0, b->shadow, b->size);
	if(b->usage & Map)
		return;
	free(b->shadow);
	b->shadow = NULL;
}

WGPUShaderModule
wgpuDeviceCreateShaderModule(WGPUDevice d, WGPUShaderModuleDescriptor const *desc)
{
	WGPUShaderModule m;
	WGPUChainedStruct const *c;
	WGPUShaderSourceWGSL const *w;
	uint32_t v[2];

	m = emalloc(sizeof *m);
	m->ref = 1;
	m->g = d->g;
	m->id = newid(d->g);
	for(c = desc->nextInChain; c != NULL; c = c->next)
		if(c->sType == WGPUSType_ShaderSourceWGSL){
			w = (WGPUShaderSourceWGSL const*)c;
			v[0] = m->id;
			v[1] = svlen(w->code);
			gmsg(d->g, 's', v, 2, (void*)w->code.data, v[1]);
			return m;
		}
	fail(d->g, "webgpu: a shader module that is not WGSL");
	return m;
}

void
wgpuShaderModuleAddRef(WGPUShaderModule m)
{
	m->ref++;
}

void
wgpuShaderModuleRelease(WGPUShaderModule m)
{
	uint32_t v;

	if(--m->ref > 0)
		return;
	v = m->id;
	gmsg(m->g, 'f', &v, 1, NULL, 0);
	free(m);
}

WGPUComputePipeline
wgpuDeviceCreateComputePipeline(WGPUDevice d, WGPUComputePipelineDescriptor const *desc)
{
	WGPUComputePipeline p;
	WGPUComputeState const *c;
	uint32_t v[5];
	size_t i, n;
	uchar *q;
	Gpu *g;

	g = d->g;
	c = &desc->compute;
	p = emalloc(sizeof *p);
	p->ref = 1;
	p->g = g;
	p->id = newid(g);
	v[0] = p->id;
	v[1] = c->module->id;
	v[2] = c->entryPoint.data != NULL ? svlen(c->entryPoint) : 4;
	v[3] = c->constantCount;
	gmsg(g, 'P', v, 4, c->entryPoint.data != NULL ? (void*)c->entryPoint.data : "main", v[2]);
	for(i = 0; i < c->constantCount; i++){
		n = svlen(c->constants[i].key);
		q = room(&g->out, &g->nout, &g->aout, 4+n+8);
		put4(q, n);
		memmove(q+4, c->constants[i].key.data, n);
		memmove(q+4+n, &c->constants[i].value, 8);
	}
	return p;
}

void
wgpuComputePipelineAddRef(WGPUComputePipeline p)
{
	p->ref++;
}

void
wgpuComputePipelineRelease(WGPUComputePipeline p)
{
	uint32_t v;

	if(--p->ref > 0)
		return;
	v = p->id;
	gmsg(p->g, 'f', &v, 1, NULL, 0);
	free(p);
}

WGPUBindGroupLayout
wgpuComputePipelineGetBindGroupLayout(WGPUComputePipeline p, uint32_t i)
{
	WGPUBindGroupLayout l;

	l = emalloc(sizeof *l);
	l->ref = 1;
	l->p = p;
	l->group = i;
	p->ref++;
	return l;
}

void
wgpuBindGroupLayoutAddRef(WGPUBindGroupLayout l)
{
	l->ref++;
}

void
wgpuBindGroupLayoutRelease(WGPUBindGroupLayout l)
{
	if(--l->ref > 0)
		return;
	wgpuComputePipelineRelease(l->p);
	free(l);
}

WGPUBindGroup
wgpuDeviceCreateBindGroup(WGPUDevice d, WGPUBindGroupDescriptor const *desc)
{
	WGPUBindGroup g;
	size_t i;

	g = emalloc(sizeof *g);
	g->ref = 1;
	g->group = desc->layout != NULL ? desc->layout->group : 0;
	g->n = desc->entryCount;
	g->e = emalloc(g->n*sizeof g->e[0]);
	for(i = 0; i < desc->entryCount; i++){
		g->e[i].binding = desc->entries[i].binding;
		g->e[i].b = desc->entries[i].buffer;
		g->e[i].off = desc->entries[i].offset;
		g->e[i].size = desc->entries[i].size;
		if(g->e[i].b == NULL){
			failf(d->g, "bind group", "an entry with no buffer");
			g->bad = 1;
			continue;
		}
		g->e[i].b->ref++;
		if(g->e[i].size != WGPU_WHOLE_SIZE){
			if(!inside(d->g, "bind group", g->e[i].off, g->e[i].size, g->e[i].b->size))
				g->bad = 1;
		}else if(g->e[i].off > g->e[i].b->size){
			failf(d->g, "bind group", "past the end");
			g->bad = 1;
		}
	}
	return g;
}

void
wgpuBindGroupAddRef(WGPUBindGroup g)
{
	g->ref++;
}

void
wgpuBindGroupRelease(WGPUBindGroup g)
{
	int i;

	if(--g->ref > 0)
		return;
	for(i = 0; i < g->n; i++)
		bufrelease(g->e[i].b);
	free(g->e);
	free(g);
}

WGPUCommandEncoder
wgpuDeviceCreateCommandEncoder(WGPUDevice d, WGPUCommandEncoderDescriptor const *desc)
{
	WGPUCommandEncoder e;

	(void)desc;
	e = emalloc(sizeof *e);
	e->ref = 1;
	e->g = d->g;
	e->pt = &e->ph;
	return e;
}

void
wgpuCommandEncoderAddRef(WGPUCommandEncoder e)
{
	e->ref++;
}

void
wgpuCommandEncoderRelease(WGPUCommandEncoder e)
{
	if(--e->ref > 0)
		return;
	freepend(e->ph);
	unrefs(&e->r);
	free(e->m);
	free(e);
}

void
wgpuCommandEncoderCopyBufferToBuffer(WGPUCommandEncoder e, WGPUBuffer a, uint64_t ao,
	WGPUBuffer b, uint64_t bo, uint64_t n)
{
	uint32_t v[5];
	Pend *p;

	if(a->id == 0){
		failf(e->g, "copy", "from a buffer not on the GPU");
		return;
	}
	if(ao%4 != 0 || bo%4 != 0 || n%4 != 0){
		failf(e->g, "copy", "offsets and count not multiples of 4");
		return;
	}
	if(!inside(e->g, "copy", ao, n, a->size) || !inside(e->g, "copy", bo, n, b->size))
		return;
	refbuf(&e->r, a);
	if(b->usage & WGPUBufferUsage_MapRead){
		v[0] = a->id;
		v[1] = ao;
		v[2] = n;
		emsg(e, 'r', v, 3);
		p = emalloc(sizeof *p);
		p->b = b;
		b->ref++;
		p->off = bo;
		p->n = n;
		*e->pt = p;
		e->pt = &p->next;
		return;
	}
	refbuf(&e->r, b);
	v[0] = a->id;
	v[1] = ao;
	v[2] = b->id;
	v[3] = bo;
	v[4] = n;
	emsg(e, 'c', v, 5);
}

WGPUComputePassEncoder
wgpuCommandEncoderBeginComputePass(WGPUCommandEncoder e, WGPUComputePassDescriptor const *desc)
{
	WGPUComputePassEncoder p;

	(void)desc;
	p = emalloc(sizeof *p);
	p->ref = 1;
	p->e = e;
	e->ref++;
	return p;
}

WGPUCommandBuffer
wgpuCommandEncoderFinish(WGPUCommandEncoder e, WGPUCommandBufferDescriptor const *desc)
{
	WGPUCommandBuffer c;

	(void)desc;
	c = emalloc(sizeof *c);
	c->ref = 1;
	c->m = e->m;
	c->nm = e->nm;
	c->ph = e->ph;
	c->pt = e->ph != NULL ? e->pt : &c->ph;
	c->r = e->r;
	e->m = NULL;
	e->nm = e->am = 0;
	e->ph = NULL;
	e->pt = &e->ph;
	memset(&e->r, 0, sizeof e->r);
	return c;
}

void
wgpuComputePassEncoderAddRef(WGPUComputePassEncoder p)
{
	p->ref++;
}

void
wgpuComputePassEncoderRelease(WGPUComputePassEncoder p)
{
	int i;

	if(--p->ref > 0)
		return;
	for(i = 0; i < Ngroup; i++)
		if(p->bg[i] != NULL)
			wgpuBindGroupRelease(p->bg[i]);
	if(p->p != NULL)
		wgpuComputePipelineRelease(p->p);
	wgpuCommandEncoderRelease(p->e);
	free(p);
}

void
wgpuComputePassEncoderSetPipeline(WGPUComputePassEncoder p, WGPUComputePipeline pl)
{
	pl->ref++;
	if(p->p != NULL)
		wgpuComputePipelineRelease(p->p);
	p->p = pl;
}

void
wgpuComputePassEncoderSetBindGroup(WGPUComputePassEncoder p, uint32_t i, WGPUBindGroup g,
	size_t nd, uint32_t const *d)
{
	(void)nd;
	(void)d;
	if(i >= Ngroup)
		return;
	if(g != NULL)
		g->ref++;
	if(p->bg[i] != NULL)
		wgpuBindGroupRelease(p->bg[i]);
	p->bg[i] = g;
}

void
wgpuComputePassEncoderDispatchWorkgroups(WGPUComputePassEncoder p, uint32_t x, uint32_t y,
	uint32_t z)
{
	WGPUCommandEncoder e;
	uchar *q;
	int i, j, n;
	Ent *t;

	e = p->e;
	if(p->p == NULL){
		fail(e->g, "webgpu: a dispatch with no pipeline");
		return;
	}
	for(i = 0; i < Ngroup; i++)
		if(p->bg[i] != NULL && p->bg[i]->bad){
			failf(e->g, "dispatch", "an invalid bind group");
			return;
		}
	refpipe(&e->r, p->p);
	n = 0;
	for(i = 0; i < Ngroup; i++)
		if(p->bg[i] != NULL)
			n += p->bg[i]->n;
	q = room(&e->m, &e->nm, &e->am, 9+20*n+12);
	*q++ = 'D';
	put4(q, p->p->id);
	put4(q+4, n);
	q += 8;
	for(i = 0; i < Ngroup; i++){
		if(p->bg[i] == NULL)
			continue;
		for(j = 0; j < p->bg[i]->n; j++, q += 20){
			t = &p->bg[i]->e[j];
			refbuf(&e->r, t->b);
			put4(q, i);
			put4(q+4, t->binding);
			put4(q+8, t->b->id);
			put4(q+12, t->off);
			put4(q+16, t->size == WGPU_WHOLE_SIZE ? 0 : t->size);
		}
	}
	put4(q, x);
	put4(q+4, y);
	put4(q+8, z);
}

void
wgpuComputePassEncoderEnd(WGPUComputePassEncoder p)
{
	(void)p;
}

void
wgpuCommandBufferAddRef(WGPUCommandBuffer c)
{
	c->ref++;
}

void
wgpuCommandBufferRelease(WGPUCommandBuffer c)
{
	if(--c->ref > 0)
		return;
	freepend(c->ph);
	unrefs(&c->r);
	free(c->m);
	free(c);
}

void
wgpuQueueSubmit(WGPUQueue q, size_t n, WGPUCommandBuffer const *cs)
{
	Gpu *g;
	WGPUCommandBuffer c;
	Pend **t, *p;
	size_t i;

	g = q->g;
	t = g->pt;
	g->nsub++;
	for(i = 0; i < n; i++){
		c = cs[i];
		memmove(room(&g->out, &g->nout, &g->aout, c->nm), c->m, c->nm);
		c->nm = 0;
		if(c->ph != NULL){
			*g->pt = c->ph;
			g->pt = c->pt;
			c->ph = NULL;
			c->pt = &c->ph;
		}
	}
	for(p = *t; p != NULL; p = p->next){
		p->seq = ++g->seq;
		if(p->b != NULL)
			p->b->last = p->seq;
	}
	g->busy = 1;
	submit(g, t);
}

void
wgpuQueueWriteBuffer(WGPUQueue q, WGPUBuffer b, uint64_t off, void const *data, size_t n)
{
	bufwrite(q->g, b, off, data, n);
}

WGPUFuture
wgpuQueueOnSubmittedWorkDone(WGPUQueue q, WGPUQueueWorkDoneCallbackInfo cb)
{
	WGPUFuture r;
	Fut *f;

	r = future(q->g, Fdone, &f);
	f->done = cb;
	return r;
}

void
wgpuPipelineLayoutAddRef(WGPUPipelineLayout l)
{
	(void)l;
}

void
wgpuPipelineLayoutRelease(WGPUPipelineLayout l)
{
	(void)l;
}

void
wgpuSamplerAddRef(WGPUSampler s)
{
	(void)s;
}

void
wgpuSamplerRelease(WGPUSampler s)
{
	(void)s;
}

void
wgpuTextureViewAddRef(WGPUTextureView t)
{
	(void)t;
}

void
wgpuTextureViewRelease(WGPUTextureView t)
{
	(void)t;
}

void
wgpuSurfaceRelease(WGPUSurface s)
{
	(void)s;
}
