/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_COMPILER_H
#define __LINUX_COMPILER_H

#include "../compiler_types.h"

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#define likely_notrace(x) likely(x)
#define unlikely_notrace(x) unlikely(x)

#ifndef __ASSEMBLY__

/**
 * offset_to_ptr - convert a relative memory offset to an absolute pointer
 * @off:	the address of the 32-bit offset value
 */
static inline void *offset_to_ptr(const int *off)
{
	return (void *)((unsigned long)off + *off);
}

#endif /* __ASSEMBLY__ */

/*
 * Whether 'type' is a signed type or an unsigned type. Supports scalar types,
 * bool and also pointer types.
 */
#define is_signed_type(type) (((type)(-1)) < (__force type)1)
#define is_unsigned_type(type) (!is_signed_type(type))

#ifndef __UNIQUE_ID
#define __UNIQUE_ID(prefix) __PASTE(__PASTE(__UNIQUE_ID_, prefix), __LINE__)
#endif
#endif /* __LINUX_COMPILER_H */
