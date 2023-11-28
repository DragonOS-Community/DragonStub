#include "efidef.h"
#include <efi.h>
#include <efilib.h>
#include <lib.h>
#include <dragonstub/dragonstub.h>

enum efistub_event {
	EFISTUB_EVT_INITRD,
	EFISTUB_EVT_LOAD_OPTIONS,
	EFISTUB_EVT_COUNT,
};

#define STR_WITH_SIZE(s) sizeof(s), s

static const struct {
	u32 pcr_index;
	u32 event_id;
	u32 event_data_len;
	u8 event_data[52];
} events[] = {
	[EFISTUB_EVT_INITRD] = { 9, INITRD_EVENT_TAG_ID,
				 STR_WITH_SIZE("Linux initrd") },
	[EFISTUB_EVT_LOAD_OPTIONS] = { 9, LOAD_OPTIONS_EVENT_TAG_ID,
				       STR_WITH_SIZE(
					       "LOADED_IMAGE::LoadOptions") },
};

static efi_status_t efi_measure_tagged_event(unsigned long load_addr,
					     unsigned long load_size,
					     enum efistub_event event)
{
	efi_guid_t tcg2_guid = EFI_TCG2_PROTOCOL_GUID;
	efi_tcg2_protocol_t *tcg2 = NULL;
	efi_status_t status;

	efi_bs_call(LocateProtocol, &tcg2_guid, NULL, (void **)&tcg2);
	if (tcg2) {
		struct efi_measured_event {
			efi_tcg2_event_t event_data;
			efi_tcg2_tagged_event_t tagged_event;
			u8 tagged_event_data[];
		} * evt;
		int size = sizeof(*evt) + events[event].event_data_len;

		status = efi_bs_call(AllocatePool, EfiLoaderData, size,
				     (void **)&evt);
		if (status != EFI_SUCCESS)
			goto fail;

		evt->event_data = (struct efi_tcg2_event){
			.event_size = size,
			.event_header.header_size =
				sizeof(evt->event_data.event_header),
			.event_header.header_version =
				EFI_TCG2_EVENT_HEADER_VERSION,
			.event_header.pcr_index = events[event].pcr_index,
			.event_header.event_type = EV_EVENT_TAG,
		};

		evt->tagged_event = (struct efi_tcg2_tagged_event){
			.tagged_event_id = events[event].event_id,
			.tagged_event_data_size = events[event].event_data_len,
		};

		memcpy(evt->tagged_event_data, events[event].event_data,
		       events[event].event_data_len);

		status = efi_call_proto(tcg2, hash_log_extend_event, 0,
					load_addr, load_size, &evt->event_data);
		efi_bs_call(FreePool, evt);

		if (status != EFI_SUCCESS)
			goto fail;
		return EFI_SUCCESS;
	}

	return EFI_UNSUPPORTED;
fail:
	efi_warn("Failed to measure data for event %d: 0x%lx\n", event, status);
	return status;
}

/*
 * At least some versions of Dell firmware pass the entire contents of the
 * Boot#### variable, i.e. the EFI_LOAD_OPTION descriptor, rather than just the
 * OptionalData field.
 *
 * Detect this case and extract OptionalData.
 */
void efi_apply_loadoptions_quirk(const void **load_options,
				 u32 *load_options_size)
{
#ifndef CONFIG_X86
	return;
#else
	const efi_load_option_t *load_option = *load_options;
	efi_load_option_unpacked_t load_option_unpacked;
	if (!load_option)
		return;
	if (*load_options_size < sizeof(*load_option))
		return;
	if ((load_option->attributes & ~EFI_LOAD_OPTION_BOOT_MASK) != 0)
		return;

	if (!efi_load_option_unpack(&load_option_unpacked, load_option,
				    *load_options_size))
		return;

	efi_warn_once(FW_BUG "LoadOptions is an EFI_LOAD_OPTION descriptor\n");
	efi_warn_once(FW_BUG "Using OptionalData as a workaround\n");

	*load_options = load_option_unpacked.optional_data;
	*load_options_size = load_option_unpacked.optional_data_size;
#endif
}

/*
 * Convert the unicode UEFI command line to ASCII to pass to kernel.
 * Size of memory allocated return in *cmd_line_len.
 * Returns NULL on error.
 */
char *efi_convert_cmdline(EFI_LOADED_IMAGE *image, int *cmd_line_len)
{
	const efi_char16_t *options = efi_table_attr(image, LoadOptions);
	u32 options_size = efi_table_attr(image, LoadOptionsSize);
	int options_bytes = 0, safe_options_bytes = 0; /* UTF-8 bytes */
	unsigned long cmdline_addr = 0;
	const efi_char16_t *s2;
	bool in_quote = false;
	efi_status_t status;
	u32 options_chars;

	if (options_size > 0)
		efi_measure_tagged_event((unsigned long)options, options_size,
					 EFISTUB_EVT_LOAD_OPTIONS);

	efi_apply_loadoptions_quirk((const void **)&options, &options_size);
	options_chars = options_size / sizeof(efi_char16_t);

	if (options) {
		s2 = options;
		while (options_bytes < COMMAND_LINE_SIZE && options_chars--) {
			efi_char16_t c = *s2++;

			if (c < 0x80) {
				if (c == L'\0' || c == L'\n')
					break;
				if (c == L'"')
					in_quote = !in_quote;
				else if (!in_quote && isspace((char)c))
					safe_options_bytes = options_bytes;

				options_bytes++;
				continue;
			}

			/*
			 * Get the number of UTF-8 bytes corresponding to a
			 * UTF-16 character.
			 * The first part handles everything in the BMP.
			 */
			options_bytes += 2 + (c >= 0x800);
			/*
			 * Add one more byte for valid surrogate pairs. Invalid
			 * surrogates will be replaced with 0xfffd and take up
			 * only 3 bytes.
			 */
			if ((c & 0xfc00) == 0xd800) {
				/*
				 * If the very last word is a high surrogate,
				 * we must ignore it since we can't access the
				 * low surrogate.
				 */
				if (!options_chars) {
					options_bytes -= 3;
				} else if ((*s2 & 0xfc00) == 0xdc00) {
					options_bytes++;
					options_chars--;
					s2++;
				}
			}
		}
		if (options_bytes >= COMMAND_LINE_SIZE) {
			options_bytes = safe_options_bytes;
			efi_err("Command line is too long: truncated to %d bytes\n",
				options_bytes);
		}
	}

	options_bytes++; /* NUL termination */

	status = efi_bs_call(AllocatePool, EfiLoaderData, options_bytes,
			     (void **)&cmdline_addr);
	if (status != EFI_SUCCESS)
		return NULL;

	snprintf((char *)cmdline_addr, options_bytes, "%.*ls",
		 options_bytes - 1, options);

	*cmd_line_len = options_bytes;
	return (char *)cmdline_addr;
}
