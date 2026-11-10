#include <string.h>

char*
strchr(const char *s, int c)
{
	char c1;

	c = (char)c;	/* ANSI: c is converted to char */
	if(c == 0) {
		while(*s++)
			;
		return s-1;
	}

	while(c1 = *s++)
		if(c1 == c)
			return s-1;
	return 0;
}
