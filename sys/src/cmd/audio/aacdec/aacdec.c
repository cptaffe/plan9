/*
 * aacdec: AAC to PCM, as audio(1)'s decoders: the file on standard input,
 * PCM on standard output by way of pcmconv. New for p9web; 9front's play
 * names audio/aacdec but its tree has none. The input is an MP4 or M4A
 * file, or ADTS: those are read here, and each access unit goes to
 * libfdkaac, the Third-Party Modified Version of the Fraunhofer FDK AAC
 * Codec Library for Android, as Fedora ships it (fetched by build.sh, not changed).
 *
 * Not here: fragmented MP4. Not in Fedora's library: SBR and parametric
 * stereo: HE-AAC plays its core, band-limited, at the core's rate.
 */
#include <u.h>
#include <libc.h>
#include <aacdecoder_lib.h>

enum {
	Maxout = 8*2048,	/* samples: what aacDecoder_DecodeFrame asks of its buffer */
};

typedef struct Track Track;

struct Track
{
	uchar	asc[64];
	int	nasc;
	ulong	nsample;
	ulong	*size;
	vlong	*off;
	vlong	skip;			/* samples of priming, in the media's time */
	vlong	keep;			/* samples to play, 0: all */
	int	found;
};

static HANDLE_AACDECODER dec;
static int	srate, nframe;		/* the core's */
static int	debug;
static int	saidsbr;
static int	ofd = -1;
static int	orate, ochans;
static vlong	toskip, tokeep = -1;

static void
convert(int rate, int chans)
{
	int pid, pfd[2];
	char fmt[32];

	if(rate == orate && chans == ochans)
		return;
	orate = rate;
	ochans = chans;
	snprint(fmt, sizeof fmt, "s16r%dc%d", rate, chans);
	if(ofd >= 0){
		close(ofd);
		waitpid();
	}
	if(pipe(pfd) < 0)
		sysfatal("pipe: %r");
	pid = fork();
	if(pid < 0)
		sysfatal("fork: %r");
	if(pid == 0){
		dup(pfd[1], 0);
		close(pfd[1]);
		close(pfd[0]);
		execl("/bin/audio/pcmconv", "pcmconv", "-i", fmt, nil);
		sysfatal("exec: %r");
	}
	close(pfd[1]);
	ofd = pfd[0];
}

static void
output(INT_PCM *pcm, CStreamInfo *si)
{
	long n, s, w;
	int oc;

	oc = si->numChannels;
	convert(si->sampleRate, oc);
	n = si->frameSize;
	s = 0;
	if(toskip > 0){
		s = toskip < n ? toskip : n;
		toskip -= s;
		n -= s;
	}
	if(tokeep >= 0){
		if(n > tokeep)
			n = tokeep;
		tokeep -= n;
	}
	if(n > 0){
		w = n*oc*2;
		if(write(ofd, pcm + s*oc, w) != w)
			sysfatal("write: %r");
	}
}

/* an access unit; a bad one's output is the library's concealment: time goes on */
static void
frame(uchar *p, long n)
{
	static INT_PCM pcm[Maxout];
	AAC_DECODER_ERROR e;
	CStreamInfo *si;
	UINT size, left;

	size = left = n;
	if(aacDecoder_Fill(dec, &p, &size, &left) != AAC_DEC_OK)
		return;
	e = aacDecoder_DecodeFrame(dec, pcm, nelem(pcm), 0);
	if(e != AAC_DEC_OK && debug)
		fprint(2, "aacdec: frame: error %#x\n", e);
	si = aacDecoder_GetStreamInfo(dec);
	if(IS_OUTPUT_VALID(e) && si != nil && si->frameSize > 0)
		output(pcm, si);
}

/* said once: the file has more than this program plays */
static void
nosbr(void)
{
	if(saidsbr++ == 0)
		fprint(2, "aacdec: no SBR: playing the core at %d Hz\n", srate);
}

/* AudioSpecificConfig */
static void
config(uchar *p, int n)
{
	AAC_DECODER_ERROR e;
	CStreamInfo *si;
	UINT size;

	size = n;
	e = aacDecoder_ConfigRaw(dec, &p, &size);
	if(e != AAC_DEC_OK)
		sysfatal("unsupported stream: error %#x", e);
	si = aacDecoder_GetStreamInfo(dec);
	if(si == nil)
		sysfatal("no stream information");
	srate = si->aacSampleRate;
	nframe = si->aacSamplesPerFrame;
	if(si->flags & AC_SBR_PRESENT)
		nosbr();
}

/*
 * mp4
 */
static ulong
be32(uchar *p)
{
	return p[0]<<24 | p[1]<<16 | p[2]<<8 | p[3];
}

static vlong
be64(uchar *p)
{
	return (vlong)be32(p)<<32 | be32(p+4);
}

/* a descriptor's tag and length; the next byte after them */
static uchar*
desc(uchar *p, uchar *e, int *tag, ulong *len)
{
	int i;

	if(p >= e)
		return nil;
	*tag = *p++;
	*len = 0;
	for(i = 0; i < 4 && p < e; i++){
		*len = *len<<7 | (*p & 0x7f);
		if((*p++ & 0x80) == 0)
			break;
	}
	return p;
}

static void
esds(Track *t, uchar *p, uchar *e)
{
	int tag, fl;
	ulong n;

	p = desc(p+4, e, &tag, &n);	/* the full atom's version and flags */
	if(p == nil || tag != 3)
		return;
	fl = p[2];
	p += 3;
	if(fl & 0x80)
		p += 2;
	if(fl & 0x40)
		p += 1 + *p;
	if(fl & 0x20)
		p += 2;
	p = desc(p, e, &tag, &n);
	if(p == nil || tag != 4)
		return;
	if(p[0] != 0x40 && p[0] != 0x66 && p[0] != 0x67 && p[0] != 0x68)
		return;			/* not AAC */
	p += 13;
	p = desc(p, e, &tag, &n);
	if(p == nil || tag != 5 || n > sizeof t->asc || p+n > e)
		return;
	memmove(t->asc, p, n);
	t->nasc = n;
}

static void atoms(Track*, uchar*, uchar*, uchar*);

static uchar *stsz, *stco, *stsc, *co64, *elst;
static ulong mdscale, mvscale;

static void
samples(Track *t)
{
	ulong i, n, fix, nch, ns, c, per, s, next, cc;
	vlong off;
	uchar *e;

	if(stsz == nil || stsc == nil || (stco == nil && co64 == nil))
		return;
	fix = be32(stsz+4);
	t->nsample = be32(stsz+8);
	t->size = malloc(t->nsample * sizeof t->size[0] + 1);
	t->off = malloc(t->nsample * sizeof t->off[0] + 1);
	if(t->size == nil || t->off == nil)
		sysfatal("malloc: %r");
	for(i = 0; i < t->nsample; i++)
		t->size[i] = fix ? fix : be32(stsz+12+4*i);
	nch = be32((stco ? stco : co64) + 4);
	ns = be32(stsc+4);
	e = stsc+8;
	s = 0;
	for(i = 0, c = 0; i < ns && s < t->nsample; i++, e += 12){
		per = be32(e+4);
		next = i+1 < ns ? be32(e+12)-1 : nch;
		if(next > nch)
			next = nch;
		for(c = be32(e)-1; c < next && s < t->nsample; c++){
			off = stco ? be32(stco+8+4*c) : be64(co64+8+8*c);
			for(cc = 0; cc < per && s < t->nsample; cc++, s++){
				t->off[s] = off;
				off += t->size[s];
			}
		}
	}
	t->nsample = s;
}

static void
atoms(Track *t, uchar *all, uchar *p, uchar *e)
{
	vlong n;
	uchar *q, *x;
	int hdr, v;
	Track nt;

	while(p+8 <= e && t->found == 0){
		n = be32(p);
		hdr = 8;
		if(n == 1){
			n = be64(p+8);
			hdr = 16;
		}else if(n == 0)
			n = e-p;
		if(n < hdr || n > e-p)
			return;
		q = p+hdr;
		x = p+n;
		if(memcmp(p+4, "moof", 4) == 0)
			sysfatal("fragmented mp4: unsupported");
		if(memcmp(p+4, "trak", 4) == 0){
			memset(&nt, 0, sizeof nt);
			stsz = stco = stsc = co64 = elst = nil;
			atoms(&nt, all, q, x);
			if(nt.nasc && !t->found){
				*t = nt;
				samples(t);
				if(elst != nil){
					/* the first real edit: priming to skip, and the length */
					v = elst[0];
					if(be32(elst+4) > 0 && mdscale){
						vlong dur, mt;

						dur = v ? be64(elst+8) : be32(elst+8);
						mt = v ? be64(elst+16) : (long)be32(elst+12);
						if(mt > 0)
							t->skip = mt;
						if(mvscale)
							t->keep = dur*mdscale/mvscale;
					}
				}
				t->found = 1;
			}
		}else if(memcmp(p+4, "moov", 4) == 0 || memcmp(p+4, "mdia", 4) == 0 || memcmp(p+4, "minf", 4) == 0
		|| memcmp(p+4, "stbl", 4) == 0 || memcmp(p+4, "edts", 4) == 0 || memcmp(p+4, "wave", 4) == 0)
			atoms(t, all, q, x);
		else if(memcmp(p+4, "mvhd", 4) == 0)
			mvscale = be32(q + (q[0] ? 20 : 12));
		else if(memcmp(p+4, "mdhd", 4) == 0)
			mdscale = be32(q + (q[0] ? 20 : 12));
		else if(memcmp(p+4, "elst", 4) == 0)
			elst = q;
		else if(memcmp(p+4, "stsz", 4) == 0)
			stsz = q;
		else if(memcmp(p+4, "stsc", 4) == 0)
			stsc = q;
		else if(memcmp(p+4, "stco", 4) == 0)
			stco = q;
		else if(memcmp(p+4, "co64", 4) == 0)
			co64 = q;
		else if(memcmp(p+4, "stsd", 4) == 0){
			if(memcmp(q+12, "mp4a", 4) == 0){
				/* the entry: its version says how long its fields are */
				v = q[24]<<8 | q[25];
				atoms(t, all, q+8+8+28 + (v == 1 ? 16 : v == 2 ? 36 : 0), x);
			}
		}else if(memcmp(p+4, "esds", 4) == 0)
			esds(t, q, x);
		p = x;
	}
}

static void
playmp4(uchar *b, vlong n, double seekto)
{
	Track t;
	ulong i;

	memset(&t, 0, sizeof t);
	atoms(&t, b, b, b+n);
	if(!t.found || t.nsample == 0)
		sysfatal("no AAC track");
	config(t.asc, t.nasc);
	/* the media's time may be the full rate of a core that plays at half it */
	toskip = t.skip;
	if(t.keep)
		tokeep = t.keep;
	if(mdscale && mdscale != srate){
		toskip = toskip*srate/mdscale;
		if(t.keep)
			tokeep = t.keep*srate/mdscale;
	}
	i = seekto*srate/nframe;
	if(i > 1){
		/* two frames before, for the overlap */
		toskip = (i > 2 ? 2 : i-1)*nframe;
		i -= i > 2 ? 2 : 1;
		tokeep = -1;
	}else
		i = 0;
	for(; i < t.nsample; i++)
		if(t.off[i] >= 0 && t.off[i]+t.size[i] <= n)
			frame(b + t.off[i], t.size[i]);
}

/*
 * adts
 */
static void
playadts(uchar *b, vlong n, double seekto)
{
	vlong i;
	int len, hdr, ri, nraw, set, cf;
	long nfr;
	uchar asc[2];

	set = 0;
	nfr = 0;
	for(i = 0; i+7 <= n; ){
		if(b[i] != 0xff || (b[i+1]&0xf6) != 0xf0){
			i++;
			continue;
		}
		len = (b[i+3]&3)<<11 | b[i+4]<<3 | b[i+5]>>5;
		hdr = b[i+1]&1 ? 7 : 9;
		if(len < hdr || i+len > n){
			if(i+len > n)
				break;
			i++;
			continue;
		}
		if(!set){
			ri = b[i+2]>>2 & 15;
			cf = (b[i+2]&1)<<2 | b[i+3]>>6;
			/* the header's profile, rate and channels are a config's */
			asc[0] = ((b[i+2]>>6)+1)<<3 | ri>>1;
			asc[1] = (ri&1)<<7 | cf<<3;
			config(asc, 2);
			set = 1;
		}
		nraw = b[i+6]&3;
		if(nraw != 0)
			sysfatal("adts: several blocks in a frame: unsupported");
		if(++nfr*(double)nframe >= seekto*srate)
			frame(b+i+hdr, len-hdr);
		i += len;
	}
	if(!set)
		sysfatal("no AAC here");
}

static void
usage(void)
{
	fprint(2, "usage: %s [ -d ] [ -s seconds ]\n", argv0);
	exits("usage");
}

void
main(int argc, char **argv)
{
	double seekto;
	uchar *b;
	vlong n, cap;
	long m;

	seekto = 0;
	ARGBEGIN{
	case 'd':
		debug++;
		break;
	case 's':
		seekto = atof(EARGF(usage()));
		if(seekto < 0)
			usage();
		break;
	default:
		usage();
	}ARGEND

	/* an MP4's index may be at its end, and standard input is a pipe */
	n = 0;
	cap = 1<<20;
	b = malloc(cap);
	for(;;){
		if(b == nil)
			sysfatal("malloc: %r");
		m = read(0, b+n, cap-n);
		if(m < 0)
			sysfatal("read: %r");
		if(m == 0)
			break;
		n += m;
		if(n == cap){
			cap *= 2;
			b = realloc(b, cap);
		}
	}
	dec = aacDecoder_Open(TT_MP4_RAW, 1);
	if(dec == nil)
		sysfatal("aacDecoder_Open failed");
	/* stereo at most; no limiter or interpolating concealment, which delay the signal */
	aacDecoder_SetParam(dec, AAC_PCM_MAX_OUTPUT_CHANNELS, 2);
	aacDecoder_SetParam(dec, AAC_PCM_LIMITER_ENABLE, 0);
	aacDecoder_SetParam(dec, AAC_CONCEAL_METHOD, 1);

	if(n > 12 && memcmp(b+4, "ftyp", 4) == 0)
		playmp4(b, n, seekto);
	else
		playadts(b, n, seekto);
	aacDecoder_Close(dec);
	if(ofd >= 0){
		close(ofd);
		waitpid();
	}
	exits(nil);
}
