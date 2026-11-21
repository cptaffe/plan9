/*
 * pANS stdio -- fseeko
 */
#include "iolib.h"
static int ufseeko(FILE *f, off_t offs, int type){
	unsigned char *rp;

	switch(f->state){
	case ERR:
	case CLOSED:
		return -1;
	case WR:
		fflush(f);
		break;
	case RD:
		if(type==1 && f->buf!=f->unbuf)
			offs-=f->wp-f->rp;
		break;
	}
	rp=f->rp;
	f->rp=f->wp;
	if(f->flags&STRING || lseek(f->fd, offs, type)==-1){
		f->rp=rp;
		return -1;
	}
	if(f->state==RD) f->rp=f->wp=f->buf;
	if(f->state!=OPEN)
		f->state=RDWR;
	return 0;
}

int
fseeko(FILE *f, off_t offs, int type)
{
	int r;

	flockfile(f);
	r = ufseeko(f, offs, type);
	funlockfile(f);
	return r;
}
