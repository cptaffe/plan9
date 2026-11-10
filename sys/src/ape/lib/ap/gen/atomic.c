#include <stdatomic.h>

/*
 * C11's atomic functions, which stdatomic.h makes the compiler's
 */
#undef atomic_thread_fence
#undef atomic_signal_fence
#undef atomic_flag_test_and_set_explicit
#undef atomic_flag_test_and_set
#undef atomic_flag_clear_explicit
#undef atomic_flag_clear

void
atomic_thread_fence(memory_order o)
{
	__c11_atomic_thread_fence(o);
}

void
atomic_signal_fence(memory_order o)
{
	__c11_atomic_signal_fence(o);
}

_Bool
atomic_flag_test_and_set_explicit(volatile atomic_flag *p, memory_order o)
{
	return __c11_atomic_exchange(&p->_v, 1, o);
}

_Bool
atomic_flag_test_and_set(volatile atomic_flag *p)
{
	return __c11_atomic_exchange(&p->_v, 1, memory_order_seq_cst);
}

void
atomic_flag_clear_explicit(volatile atomic_flag *p, memory_order o)
{
	__c11_atomic_store(&p->_v, 0, o);
}

void
atomic_flag_clear(volatile atomic_flag *p)
{
	__c11_atomic_store(&p->_v, 0, memory_order_seq_cst);
}
