/*
 * pANS stdio -- fseek
 */
#include "iolib.h"
static int ufseek(FILE *f, long offs, int type){
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
fseek(FILE *f, long offs, int type)
{
	int r;

	flockfile(f);
	r = ufseek(f, offs, type);
	funlockfile(f);
	return r;
}
