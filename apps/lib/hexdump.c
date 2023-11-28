const char hex_asc[] = "0123456789abcdef";
const char hex_asc_upper[] = "0123456789ABCDEF";

/**
 * hex_to_bin - convert a hex digit to its real value
 * @ch: ascii character represents hex digit
 *
 * hex_to_bin() converts one hex digit to its actual value or -1 in case of bad
 * input.
 *
 * This function is used to load cryptographic keys, so it is coded in such a
 * way that there are no conditions or memory accesses that depend on data.
 *
 * Explanation of the logic:
 * (ch - '9' - 1) is negative if ch <= '9'
 * ('0' - 1 - ch) is negative if ch >= '0'
 * we "and" these two values, so the result is negative if ch is in the range
 *	'0' ... '9'
 * we are only interested in the sign, so we do a shift ">> 8"; note that right
 *	shift of a negative value is implementation-defined, so we cast the
 *	value to (unsigned) before the shift --- we have 0xffffff if ch is in
 *	the range '0' ... '9', 0 otherwise
 * we "and" this value with (ch - '0' + 1) --- we have a value 1 ... 10 if ch is
 *	in the range '0' ... '9', 0 otherwise
 * we add this value to -1 --- we have a value 0 ... 9 if ch is in the range '0'
 *	... '9', -1 otherwise
 * the next line is similar to the previous one, but we need to decode both
 *	uppercase and lowercase letters, so we use (ch & 0xdf), which converts
 *	lowercase to uppercase
 */
int hex_to_bin(unsigned char ch)
{
	unsigned char cu = ch & 0xdf;
	return -1 +
	       ((ch - '0' + 1) &
		(unsigned)((ch - '9' - 1) & ('0' - 1 - ch)) >> 8) +
	       ((cu - 'A' + 11) &
		(unsigned)((cu - 'F' - 1) & ('A' - 1 - cu)) >> 8);
}