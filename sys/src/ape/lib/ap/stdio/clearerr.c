/*
 * pANS stdio -- clearerr
 */
#include "iolib.h"
static void uclearerr(FILE *f){
	switch(f->state){
	case ERR:
		f->state=f->buf?RDWR:OPEN;
		break;
	case END:
		f->state=RDWR;
		break;
	}
}

void
clearerr(FILE *f)
{
	flockfile(f);
	uclearerr(f);
	funlockfile(f);
}
