#pragma once

#define __get_unaligned_t(type, ptr)                        \
	({                                                  \
		const struct {                              \
			type x;                             \
		} __packed *__pptr = (typeof(__pptr))(ptr); \
		__pptr->x;                                  \
	})

#define __put_unaligned_t(type, val, ptr)                   \
	do {                                                \
		struct {                                    \
			type x;                             \
		} __packed *__pptr = (typeof(__pptr))(ptr); \
		__pptr->x = (val);                          \
	} while (0)
