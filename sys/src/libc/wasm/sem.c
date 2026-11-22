#include <u.h>
#include <libc.h>

#pragma weak semacquire
int semacquire(long *p, int block) { USED(block); if(*p > 0){ --*p; return 1; } return 0; }
#pragma weak tsemacquire
int tsemacquire(long *p, ulong ms) { USED(ms); return semacquire(p, 0); }
#pragma weak semrelease
long semrelease(long *p, long n) { *p += n; return *p; }
