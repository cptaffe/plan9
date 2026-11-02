#ifndef __STDARG
#define __STDARG

typedef __builtin_va_list va_list;

#define va_start(list, start)	__builtin_va_start(list, start)
#define va_end(list)		__builtin_va_end(list)
#define va_arg(list, mode)	__builtin_va_arg(list, mode)
#define va_copy(to, from)	__builtin_va_copy(to, from)

#endif /* __STDARG */
