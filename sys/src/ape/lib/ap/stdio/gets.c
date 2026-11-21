/*
 * pANS stdio -- gets
 */
#include "iolib.h"
static char *ugets(char *as){
#ifdef secure
	stdin->flags|=ERR;
	return NULL;
#else
	char *s=as;
	int c;
	while((c=getchar())!='\n' && c!=EOF) *s++=c;
	if(c!=EOF || s!=as) *s='\0';
	else return NULL;
	return as;
#endif
}

char *
gets(char *as)
{
	char *r;

	flockfile(stdin);
	r = ugets(as);
	funlockfile(stdin);
	return r;
}
