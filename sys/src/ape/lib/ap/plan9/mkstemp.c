#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

static char*
tmpl(char *t)
{
	char *x;

	x = t + strlen(t) - 6;
	if(x < t || strcmp(x, "XXXXXX") != 0){
		errno = EINVAL;
		return NULL;
	}
	return x;
}

static int
fill(char *x)
{
	static char c[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	unsigned char r[6];
	int i;

	if(getentropy(r, sizeof r) < 0)
		return -1;
	for(i = 0; i < 6; i++)
		x[i] = c[r[i] % (sizeof c - 1)];
	return 0;
}

int
mkstemp(char *t)
{
	int i, f;
	char *x;

	if((x = tmpl(t)) == NULL)
		return -1;
	for(i = 0; i < 100; i++){
		if(fill(x) < 0)
			return -1;
		if((f = open(t, O_RDWR|O_CREAT|O_EXCL, 0600)) >= 0 || errno != EEXIST)
			return f;
	}
	return -1;
}

char*
mkdtemp(char *t)
{
	int i;
	char *x;

	if((x = tmpl(t)) == NULL)
		return NULL;
	for(i = 0; i < 100; i++){
		if(fill(x) < 0)
			return NULL;
		if(mkdir(t, 0700) == 0)
			return t;
		if(errno != EEXIST)
			return NULL;
	}
	return NULL;
}
