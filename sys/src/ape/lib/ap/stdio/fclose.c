/*
 * pANS stdio -- fclose
 */
#include "iolib.h"
static int ufclose(FILE *f){
	int error=0;
	if(!f) return EOF;
	if(f->state==CLOSED) return EOF;
	if(fflush(f)==EOF) error=EOF;
	f->rp=f->wp;
	if(f->flags&BALLOC) free(f->buf);
	if(!(f->flags&STRING) && close(f->fd)<0) error=EOF;
	f->state=CLOSED;
	f->flags=0;
	_IO_orient[f-_IO_stream]=0;
	return error;
}

int
fclose(FILE *f)
{
	int r;

	flockfile(f);
	r = ufclose(f);
	funlockfile(f);
	return r;
}
