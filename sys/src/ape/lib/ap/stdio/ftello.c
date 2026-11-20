/*
 * pANS stdio -- ftello
 */
#include "iolib.h"
static off_t uftello(FILE *f){
	off_t seekp=lseek(f->fd, 0, SEEK_CUR);
	if(seekp<0) return -1;		/* enter error state? */
	switch(f->state){
	default:
		return seekp;
	case RD:
		return seekp-(f->wp-f->rp);
	case WR:
		return (f->flags&LINEBUF?f->lp:f->wp)-f->buf+seekp;
	}
}

off_t
ftello(FILE *f)
{
	off_t r;

	flockfile(f);
	r = uftello(f);
	funlockfile(f);
	return r;
}
