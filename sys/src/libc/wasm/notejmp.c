#include <u.h>
#include <libc.h>

/* A handler runs in place on the program's stack (sys.c), so there is no Ureg to change. */
void
notejmp(void*, jmp_buf j, int ret)
{
	longjmp(j, ret == 0 ? 1 : ret);
}
