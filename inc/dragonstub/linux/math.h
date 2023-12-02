#pragma once

#include "../types.h"
#include "div64.h"

/*
 * This looks more complex than it should be. But we need to
 * get the type for the ~ right in round_down (it needs to be
 * as wide as the result!), and we want to evaluate the macro
 * arguments just once each.
 */
#define __round_mask(x, y) ((__typeof__(x))((y)-1))

/**
 * round_up - round up to next specified power of 2
 * @x: the value to round
 * @y: multiple to round up to (must be a power of 2)
 *
 * Rounds @x up to next multiple of @y (which must be a power of 2).
 * To perform arbitrary rounding up, use roundup() below.
 */
#define round_up(x, y) ((((x)-1) | __round_mask(x, y)) + 1)

/**
 * round_down - round down to next specified power of 2
 * @x: the value to round
 * @y: multiple to round down to (must be a power of 2)
 *
 * Rounds @x down to next multiple of @y (which must be a power of 2).
 * To perform arbitrary rounding down, use rounddown() below.
 */
#define round_down(x, y) ((x) & ~__round_mask(x, y))

#define DIV_ROUND_UP __KERNEL_DIV_ROUND_UP

#define DIV_ROUND_DOWN_ULL(ll, d)               \
	({                                      \
		unsigned long long _tmp = (ll); \
		do_div(_tmp, d);                \
		_tmp;                           \
	})

#define DIV_ROUND_UP_ULL(ll, d) \
	DIV_ROUND_DOWN_ULL((unsigned long long)(ll) + (d)-1, (d))

#if BITS_PER_LONG == 32
#define DIV_ROUND_UP_SECTOR_T(ll, d) DIV_ROUND_UP_ULL(ll, d)
#else
#define DIV_ROUND_UP_SECTOR_T(ll, d) DIV_ROUND_UP(ll, d)
#endif

/**
 * roundup - round up to the next specified multiple
 * @x: the value to up
 * @y: multiple to round up to
 *
 * Rounds @x up to next multiple of @y. If @y will always be a power
 * of 2, consider using the faster round_up().
 */
#define roundup(x, y)                            \
	({                                       \
		typeof(y) __y = y;               \
		(((x) + (__y - 1)) / __y) * __y; \
	})
/**
 * rounddown - round down to next specified multiple
 * @x: the value to round
 * @y: multiple to round down to
 *
 * Rounds @x down to next multiple of @y. If @y will always be a power
 * of 2, consider using the faster round_down().
 */
#define rounddown(x, y)              \
	({                           \
		typeof(x) __x = (x); \
		__x - (__x % (y));   \
	})

/*
 * Divide positive or negative dividend by positive or negative divisor
 * and round to closest integer. Result is undefined for negative
 * divisors if the dividend variable type is unsigned and for negative
 * dividends if the divisor variable type is unsigned.
 */
#define DIV_ROUND_CLOSEST(x, divisor)                                \
	({                                                           \
		typeof(x) __x = x;                                   \
		typeof(divisor) __d = divisor;                       \
		(((typeof(x))-1) > 0 || ((typeof(divisor))-1) > 0 || \
		 (((__x) > 0) == ((__d) > 0))) ?                     \
			(((__x) + ((__d) / 2)) / (__d)) :            \
			(((__x) - ((__d) / 2)) / (__d));             \
	})
/*
 * Same as above but for u64 dividends. divisor must be a 32-bit
 * number.
 */
#define DIV_ROUND_CLOSEST_ULL(x, divisor)                  \
	({                                                 \
		typeof(divisor) __d = divisor;             \
		unsigned long long _tmp = (x) + (__d) / 2; \
		do_div(_tmp, __d);                         \
		_tmp;                                      \
	})
