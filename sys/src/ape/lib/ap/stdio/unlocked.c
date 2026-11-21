#include "iolib.h"
#undef getc_unlocked
#undef getchar_unlocked
#undef putc_unlocked
#undef putchar_unlocked

int
getc_unlocked(FILE *f)
{
	return getc(f);
}

int
getchar_unlocked(void)
{
	return getc(stdin);
}

int
putc_unlocked(int c, FILE *f)
{
	return putc(c, f);
}

int
putchar_unlocked(int c)
{
	return putc(c, stdout);
}
