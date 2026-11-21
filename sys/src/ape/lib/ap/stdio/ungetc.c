/*
 * pANS stdio -- ungetc
 */
#include "iolib.h"
#include <string.h>
static int uungetc(int c, FILE *f){
	if(c==EOF) return EOF;
	switch(f->state){
	default:	/* WR */
		f->state=ERR;
		return EOF;
	case CLOSED:
	case ERR:
		return EOF;
	case OPEN:
		_IO_setvbuf(f);
	case RDWR:
	case END:
		f->rp=f->wp=f->buf+(f->bufl==0?1:f->bufl);
		f->state=RD;
	case RD:
		if(f->rp==f->buf){
			/* room after what is buffered: move it up, for c before it */
			if(f->flags&STRING || f->wp>=f->buf+f->bufl) return EOF;
			memmove(f->buf+1, f->buf, f->wp-f->buf);
			f->wp++;
			f->rp++;
		}
		if(f->flags&STRING)
			f->rp--;
		else
			*--f->rp=c;
		return (unsigned char)c;
	}
}

int
ungetc(int c, FILE *f)
{
	int r;

	flockfile(f);
	r = uungetc(c, f);
	funlockfile(f);
	return r;
}
