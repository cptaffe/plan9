#ifndef __STDATOMIC_H
#define __STDATOMIC_H
#pragma lib "/$M/lib/ape/libap.a"
/*
 * C11 atomics: the compiler's, by its __c11_atomic builtins.
 */
#include <stddef.h>
#include <stdint.h>

typedef enum memory_order {
	memory_order_relaxed = __ATOMIC_RELAXED,
	memory_order_consume = __ATOMIC_CONSUME,
	memory_order_acquire = __ATOMIC_ACQUIRE,
	memory_order_release = __ATOMIC_RELEASE,
	memory_order_acq_rel = __ATOMIC_ACQ_REL,
	memory_order_seq_cst = __ATOMIC_SEQ_CST,
} memory_order;

#define ATOMIC_BOOL_LOCK_FREE	__CLANG_ATOMIC_BOOL_LOCK_FREE
#define ATOMIC_CHAR_LOCK_FREE	__CLANG_ATOMIC_CHAR_LOCK_FREE
#define ATOMIC_CHAR16_T_LOCK_FREE	__CLANG_ATOMIC_CHAR16_T_LOCK_FREE
#define ATOMIC_CHAR32_T_LOCK_FREE	__CLANG_ATOMIC_CHAR32_T_LOCK_FREE
#define ATOMIC_WCHAR_T_LOCK_FREE	__CLANG_ATOMIC_WCHAR_T_LOCK_FREE
#define ATOMIC_SHORT_LOCK_FREE	__CLANG_ATOMIC_SHORT_LOCK_FREE
#define ATOMIC_INT_LOCK_FREE	__CLANG_ATOMIC_INT_LOCK_FREE
#define ATOMIC_LONG_LOCK_FREE	__CLANG_ATOMIC_LONG_LOCK_FREE
#define ATOMIC_LLONG_LOCK_FREE	__CLANG_ATOMIC_LLONG_LOCK_FREE
#define ATOMIC_POINTER_LOCK_FREE	__CLANG_ATOMIC_POINTER_LOCK_FREE

#define ATOMIC_VAR_INIT(v)	(v)
#define atomic_init(p, v)	__c11_atomic_init(p, v)
#define kill_dependency(y)	(y)

typedef _Atomic(_Bool) atomic_bool;
typedef _Atomic(char) atomic_char;
typedef _Atomic(signed char) atomic_schar;
typedef _Atomic(unsigned char) atomic_uchar;
typedef _Atomic(short) atomic_short;
typedef _Atomic(unsigned short) atomic_ushort;
typedef _Atomic(int) atomic_int;
typedef _Atomic(unsigned int) atomic_uint;
typedef _Atomic(long) atomic_long;
typedef _Atomic(unsigned long) atomic_ulong;
typedef _Atomic(long long) atomic_llong;
typedef _Atomic(unsigned long long) atomic_ullong;
typedef _Atomic(uint_least16_t) atomic_char16_t;
typedef _Atomic(uint_least32_t) atomic_char32_t;
typedef _Atomic(wchar_t) atomic_wchar_t;
typedef _Atomic(int_least8_t) atomic_int_least8_t;
typedef _Atomic(uint_least8_t) atomic_uint_least8_t;
typedef _Atomic(int_least16_t) atomic_int_least16_t;
typedef _Atomic(uint_least16_t) atomic_uint_least16_t;
typedef _Atomic(int_least32_t) atomic_int_least32_t;
typedef _Atomic(uint_least32_t) atomic_uint_least32_t;
typedef _Atomic(int_least64_t) atomic_int_least64_t;
typedef _Atomic(uint_least64_t) atomic_uint_least64_t;
typedef _Atomic(int_fast8_t) atomic_int_fast8_t;
typedef _Atomic(uint_fast8_t) atomic_uint_fast8_t;
typedef _Atomic(int_fast16_t) atomic_int_fast16_t;
typedef _Atomic(uint_fast16_t) atomic_uint_fast16_t;
typedef _Atomic(int_fast32_t) atomic_int_fast32_t;
typedef _Atomic(uint_fast32_t) atomic_uint_fast32_t;
typedef _Atomic(int_fast64_t) atomic_int_fast64_t;
typedef _Atomic(uint_fast64_t) atomic_uint_fast64_t;
typedef _Atomic(intptr_t) atomic_intptr_t;
typedef _Atomic(uintptr_t) atomic_uintptr_t;
typedef _Atomic(size_t) atomic_size_t;
typedef _Atomic(ptrdiff_t) atomic_ptrdiff_t;
typedef _Atomic(intmax_t) atomic_intmax_t;
typedef _Atomic(uintmax_t) atomic_uintmax_t;

#define atomic_is_lock_free(p)	__c11_atomic_is_lock_free(sizeof(*(p)))

#define atomic_store_explicit(p, v, o)	__c11_atomic_store(p, v, o)
#define atomic_load_explicit(p, o)	__c11_atomic_load(p, o)
#define atomic_exchange_explicit(p, v, o)	__c11_atomic_exchange(p, v, o)
#define atomic_compare_exchange_strong_explicit(p, e, v, s, f)	__c11_atomic_compare_exchange_strong(p, e, v, s, f)
#define atomic_compare_exchange_weak_explicit(p, e, v, s, f)	__c11_atomic_compare_exchange_weak(p, e, v, s, f)
#define atomic_fetch_add_explicit(p, v, o)	__c11_atomic_fetch_add(p, v, o)
#define atomic_fetch_sub_explicit(p, v, o)	__c11_atomic_fetch_sub(p, v, o)
#define atomic_fetch_or_explicit(p, v, o)	__c11_atomic_fetch_or(p, v, o)
#define atomic_fetch_xor_explicit(p, v, o)	__c11_atomic_fetch_xor(p, v, o)
#define atomic_fetch_and_explicit(p, v, o)	__c11_atomic_fetch_and(p, v, o)

#define atomic_store(p, v)	atomic_store_explicit(p, v, memory_order_seq_cst)
#define atomic_load(p)	atomic_load_explicit(p, memory_order_seq_cst)
#define atomic_exchange(p, v)	atomic_exchange_explicit(p, v, memory_order_seq_cst)
#define atomic_compare_exchange_strong(p, e, v)	atomic_compare_exchange_strong_explicit(p, e, v, memory_order_seq_cst, memory_order_seq_cst)
#define atomic_compare_exchange_weak(p, e, v)	atomic_compare_exchange_weak_explicit(p, e, v, memory_order_seq_cst, memory_order_seq_cst)
#define atomic_fetch_add(p, v)	atomic_fetch_add_explicit(p, v, memory_order_seq_cst)
#define atomic_fetch_sub(p, v)	atomic_fetch_sub_explicit(p, v, memory_order_seq_cst)
#define atomic_fetch_or(p, v)	atomic_fetch_or_explicit(p, v, memory_order_seq_cst)
#define atomic_fetch_xor(p, v)	atomic_fetch_xor_explicit(p, v, memory_order_seq_cst)
#define atomic_fetch_and(p, v)	atomic_fetch_and_explicit(p, v, memory_order_seq_cst)

typedef struct atomic_flag { atomic_bool _v; } atomic_flag;
#define ATOMIC_FLAG_INIT	{ 0 }

#ifdef __cplusplus
extern "C" {
#endif

extern void atomic_thread_fence(memory_order);
extern void atomic_signal_fence(memory_order);
extern _Bool atomic_flag_test_and_set_explicit(volatile atomic_flag *, memory_order);
extern _Bool atomic_flag_test_and_set(volatile atomic_flag *);
extern void atomic_flag_clear_explicit(volatile atomic_flag *, memory_order);
extern void atomic_flag_clear(volatile atomic_flag *);

#ifdef __cplusplus
}
#endif

#define atomic_thread_fence(o)	__c11_atomic_thread_fence(o)
#define atomic_signal_fence(o)	__c11_atomic_signal_fence(o)
#define atomic_flag_test_and_set_explicit(p, o)	__c11_atomic_exchange(&(p)->_v, 1, o)
#define atomic_flag_test_and_set(p)	atomic_flag_test_and_set_explicit(p, memory_order_seq_cst)
#define atomic_flag_clear_explicit(p, o)	__c11_atomic_store(&(p)->_v, 0, o)
#define atomic_flag_clear(p)	atomic_flag_clear_explicit(p, memory_order_seq_cst)

#endif /* __STDATOMIC_H */
