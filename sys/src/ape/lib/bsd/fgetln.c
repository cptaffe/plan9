#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>

char *
fgetln(FILE *f, size_t *len)
{
	static char *line;
	static size_t max;
	ssize_t n;

	if((n = getline(&line, &max, f)) <= 0)
		return NULL;
	*len = n;
	return line;
}
