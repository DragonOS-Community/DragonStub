#include <dragonstub/dragonstub.h>


typedef union efi_rng_protocol efi_rng_protocol_t;

union efi_rng_protocol {
	struct {
		efi_status_t (__efiapi *get_info)(efi_rng_protocol_t *,
						  unsigned long *,
						  efi_guid_t *);
		efi_status_t (__efiapi *get_rng)(efi_rng_protocol_t *,
						 efi_guid_t *, unsigned long,
						 u8 *out);
	};
	struct {
		u32 get_info;
		u32 get_rng;
	} mixed_mode;
};

/**
 * efi_get_random_bytes() - fill a buffer with random bytes
 * @size:	size of the buffer
 * @out:	caller allocated buffer to receive the random bytes
 *
 * The call will fail if either the firmware does not implement the
 * EFI_RNG_PROTOCOL or there are not enough random bytes available to fill
 * the buffer.
 *
 * Return:	status code
 */
efi_status_t efi_get_random_bytes(unsigned long size, u8 *out)
{
	efi_guid_t rng_proto = EFI_RNG_PROTOCOL_GUID;
	efi_status_t status;
	efi_rng_protocol_t *rng = NULL;

	status = efi_bs_call(LocateProtocol, &rng_proto, NULL, (void **)&rng);
	if (status != EFI_SUCCESS)
		return status;

	return efi_call_proto(rng, get_rng, NULL, size, out);
}