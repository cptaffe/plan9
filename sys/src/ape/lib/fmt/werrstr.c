#include <stdarg.h>
#include <errno.h>
#include "fmt.h"

#ifdef __wasm__
extern _Thread_local char _plan9err[128];
#else
extern char _plan9err[128];
#endif

void
werrstr(const char *fmt, ...)
{
	va_list arg;

	va_start(arg, fmt);
	snprint(_plan9err, sizeof _plan9err, fmt, arg);
	va_end(arg);
	errno = EPLAN9;
}
