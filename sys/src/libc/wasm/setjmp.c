#include <u.h>
#include <libc.h>
#include "wasm.h"

typedef struct Jmp Jmp;
struct Jmp
{
	void	*invocation;
	uint	label;
	struct {
		void	*env;
		int	val;
	} arg;
};

#ifdef __clang__
__asm__(".globl __c_longjmp\n.tagtype __c_longjmp i32\n__c_longjmp:\n");

void
__wasm_setjmp(void *env, uint label, void *invocation)
{
	Jmp *j;

	if(label == 0 || invocation == nil)
		abort();
	j = env;
	j->invocation = invocation;
	j->label = label;
}

uint
__wasm_setjmp_test(void *env, void *invocation)
{
	Jmp *j;

	j = env;
	if(j->label == 0 || invocation == nil)
		abort();
	return j->invocation == invocation ? j->label : 0;
}

void
__wasm_longjmp(void *env, int val)
{
	Jmp *j;

	j = env;
	j->arg.env = env;
	j->arg.val = val ? val : 1;
	__builtin_wasm_throw(1, &j->arg);
}
#else
void
longjmp(jmp_buf env, int val)
{
	Jmp *j;

	j = (Jmp*)env;
	j->arg.env = env;
	j->arg.val = val ? val : 1;
	wasm·throw(&j->arg);
}
#endif
