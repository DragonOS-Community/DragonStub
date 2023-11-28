#pragma once

#include <efi.h>
#include "linux/stdarg.h"

#define efi_printk(__fmt, ...)                       \
	({                                           \
		static CHAR16 __mem[2048];           \
		int i;                               \
		for (i = 0; __fmt[i]; ++i)           \
			__mem[i] = (CHAR16)__fmt[i]; \
		__mem[i] = 0;                        \
		Print(__mem, ##__VA_ARGS__);         \
	})

#define efi_todo(__fmt)                                    \
	({                                                 \
		efi_printk("Not yet implemented: " __fmt); \
		while (1)                                  \
			;                                  \
	})

#define efi_info(fmt, ...) efi_printk("[INFO]: " fmt, ##__VA_ARGS__)
#define efi_warn(fmt, ...) efi_printk("[WARNING]: " fmt, ##__VA_ARGS__)
#define efi_err(fmt, ...) efi_printk("[ERROR]: " fmt, ##__VA_ARGS__)
#define efi_debug(fmt, ...) efi_printk("[DEBUG]: " fmt, ##__VA_ARGS__)

/**
 * snprintf - Format a string and place it in a buffer
 * @buf: The buffer to place the result into
 * @size: The size of the buffer, including the trailing null space
 * @fmt: The format string to use
 * @...: Arguments for the format string
 *
 * The return value is the number of characters which would be
 * generated for the given input, excluding the trailing null,
 * as per ISO C99.  If the return is greater than or equal to
 * @size, the resulting string is truncated.
 *
 * See the vsnprintf() documentation for format string extensions over C99.
 */
int snprintf(char *buf, size_t size, const char *fmt, ...);

/**
 * vsnprintf - Format a string and place it in a buffer
 * @buf: The buffer to place the result into
 * @size: The size of the buffer, including the trailing null space
 * @fmt: The format string to use
 * @args: Arguments for the format string
 *
 * This function generally follows C99 vsnprintf, but has some
 * extensions and a few limitations:
 *
 *  - ``%n`` is unsupported
 *  - ``%p*`` is handled by pointer()
 *
 * See pointer() or Documentation/core-api/printk-formats.rst for more
 * extensive description.
 *
 * **Please update the documentation in both places when making changes**
 *
 * The return value is the number of characters which would
 * be generated for the given input, excluding the trailing
 * '\0', as per ISO C99. If you want to have the exact
 * number of characters written into @buf as return value
 * (not including the trailing '\0'), use vscnprintf(). If the
 * return is greater than or equal to @size, the resulting
 * string is truncated.
 *
 * If you're not already dealing with a va_list consider using snprintf().
 */
int vsnprintf(char *buf, size_t size, const char *fmt, va_list args);