/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_GENERIC_BUG_H
#define _ASM_GENERIC_BUG_H
#include "printk.h"
#include "build_bug.h"
#include "linux/compiler.h"


#ifndef WARN
#define WARN(condition, format...)                               \
	({                                                       \
		int __ret_warn_on = !!(condition);               \
		if (unlikely(__ret_warn_on)) {                   \
			efi_warn("(%s:%d)", __FILE__, __LINE__); \
			efi_printk(format);                      \
		}                                                \
		unlikely(__ret_warn_on);                         \
	})
#endif

#ifndef WARN_ON
#define WARN_ON(condition)                                         \
	({                                                         \
		int __ret_warn_on = !!(condition);                 \
		if (unlikely(__ret_warn_on))                       \
			efi_warn("(%s:%d)\n", __FILE__, __LINE__); \
		unlikely(__ret_warn_on);                           \
	})
#endif

#define WARN_ON_ONCE(condition) WARN_ON(condition)
#define WARN_ONCE(condition, format...) WARN(condition, format)
#define WARN_TAINT(condition, taint, format...) WARN(condition, format)
#define WARN_TAINT_ONCE(condition, taint, format...) WARN(condition, format)

/* Optimization barrier */
#ifndef barrier
/* The "volatile" is due to gcc bugs */
#define barrier() __asm__ __volatile__("" : : : "memory")
#endif

#define BUILD_BUG_ON(condition)                            \
	do {                                               \
		efi_err("BUILD_BUG_ON(%s)\n", #condition); \
		while (1)                                  \
			;                                  \
	} while (1)

#ifdef __CHECKER__
#define BUILD_BUG_ON_ZERO(e) (0)
#else /* __CHECKER__ */
/*
 * Force a compilation error if condition is true, but also produce a
 * result (of value 0 and type int), so the expression can be used
 * e.g. in a structure initializer (or where-ever else comma expressions
 * aren't permitted).
 */
#define BUILD_BUG_ON_ZERO(e) ((int)(sizeof(struct { int : (-!!(e)); })))
#endif /* __CHECKER__ */


#endif
