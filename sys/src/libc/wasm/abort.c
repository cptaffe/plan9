#include <u.h>
#include <libc.h>
#include "wasm.h"

void
abort(void)
{
	wasm·unreachable();
}
