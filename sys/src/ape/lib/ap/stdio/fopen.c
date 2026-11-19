/*
 * pANS stdio -- fopen
 */
#include "iolib.h"
FILE *fopen(const char *name, const char *mode){
	FILE *f, *r;
	for(f=_IO_stream;f!=&_IO_stream[FOPEN_MAX];f++)
		if(f->state==CLOSED){
			flockfile(f);
			if(f->state==CLOSED){
				r=freopen(name, mode, f);
				funlockfile(f);
				return r;
			}
			funlockfile(f);
		}
	return NULL;
}
