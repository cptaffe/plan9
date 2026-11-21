#include <dlfcn.h>
#include <stddef.h>

static char *err;

void*
dlopen(const char *path, int mode)
{
	(void)path;
	(void)mode;
	err = "dlopen: no dynamic loading";
	return NULL;
}

void*
dlsym(void *h, const char *name)
{
	(void)h;
	(void)name;
	err = "dlsym: no dynamic loading";
	return NULL;
}

int
dlclose(void *h)
{
	(void)h;
	err = "dlclose: no dynamic loading";
	return -1;
}

char*
dlerror(void)
{
	char *e;

	e = err;
	err = NULL;
	return e;
}
