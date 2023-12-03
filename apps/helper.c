#include "dragonstub/printk.h"
#include "efidef.h"
#include <efi.h>
#include <efilib.h>
#include <lib.h>
#include <dragonstub/dragonstub.h>

bool efi_nochunk;
bool efi_nokaslr = true;
// bool efi_nokaslr = !IS_ENABLED(CONFIG_RANDOMIZE_BASE);
bool efi_novamap = false;

static bool efi_noinitrd;
static bool efi_nosoftreserve;
static bool efi_disable_pci_dma = false;
// static bool efi_disable_pci_dma = IS_ENABLED(CONFIG_EFI_DISABLE_PCI_DMA);

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

/**
 *	parse_option_str - Parse a string and check an option is set or not
 *	@str: String to be parsed
 *	@option: option name
 *
 *	This function parses a string containing a comma-separated list of
 *	strings like a=b,c.
 *
 *	Return true if there's such option in the string, or return false.
 */
bool parse_option_str(const char *str, const char *option)
{
	while (*str) {
		if (!strncmp(str, option, strlen(option))) {
			str += strlen(option);
			if (!*str || *str == ',')
				return true;
		}

		while (*str && *str != ',')
			str++;

		if (*str == ',')
			str++;
	}

	return false;
}

/**
 * efi_parse_options() - Parse EFI command line options
 * @cmdline:	kernel command line
 *
 * Parse the ASCII string @cmdline for EFI options, denoted by the efi=
 * option, e.g. efi=nochunk.
 *
 * It should be noted that efi= is parsed in two very different
 * environments, first in the early boot environment of the EFI boot
 * stub, and subsequently during the kernel boot.
 *
 * Return:	status code
 */
efi_status_t efi_parse_options(char const *cmdline)
{
	size_t len;
	efi_status_t status;
	char *str, *buf;

	if (!cmdline)
		return EFI_SUCCESS;

	len = strnlen(cmdline, COMMAND_LINE_SIZE - 1) + 1;
	status = efi_bs_call(AllocatePool, EfiLoaderData, len, (void **)&buf);
	if (status != EFI_SUCCESS)
		return status;

	memcpy(buf, cmdline, len - 1);
	buf[len - 1] = '\0';
	str = skip_spaces(buf);

	while (*str) {
		char *param, *val;

		str = next_arg(str, &param, &val);
		if (!val && !strcmp(param, "--"))
			break;

		if (!strcmp(param, "nokaslr")) {
			efi_nokaslr = true;
		} else if (!strcmp(param, "quiet")) {
			// efi_loglevel = CONSOLE_LOGLEVEL_QUIET;
		} else if (!strcmp(param, "noinitrd")) {
			efi_noinitrd = true;
		}
#ifdef CONFIG_X86_64
		else if (IS_ENABLED(CONFIG_X86_64) &&
			 !strcmp(param, "no5lvl")) {
			efi_no5lvl = true;
		}
#endif
		else if (!strcmp(param, "efi") && val) {
			efi_nochunk = parse_option_str(val, "nochunk");
			efi_novamap |= parse_option_str(val, "novamap");

			// efi_nosoftreserve =
			// 	IS_ENABLED(CONFIG_EFI_SOFT_RESERVE) &&
			// 	parse_option_str(val, "nosoftreserve");
			efi_nosoftreserve = false;

			if (parse_option_str(val, "disable_early_pci_dma"))
				efi_disable_pci_dma = true;
			if (parse_option_str(val, "no_disable_early_pci_dma"))
				efi_disable_pci_dma = false;
			if (parse_option_str(val, "debug")) {
				// efi_loglevel = CONSOLE_LOGLEVEL_DEBUG;
			}
		} else if (!strcmp(param, "video") && val &&
			   strstarts(val, "efifb:")) {
			// efi_parse_option_graphics(val + strlen("efifb:"));
		}
	}
	efi_bs_call(FreePool, buf);
	return EFI_SUCCESS;
}

/**
 * get_efi_config_table() - retrieve UEFI configuration table
 * @guid:	GUID of the configuration table to be retrieved
 * Return:	pointer to the configuration table or NULL
 */
void *get_efi_config_table(efi_guid_t guid)
{
	efi_config_table_t *tables = efi_table_attr(ST, ConfigurationTable);
	int nr_tables = efi_table_attr(ST, NumberOfTableEntries);
	int i;

	for (i = 0; i < nr_tables; i++) {
		efi_config_table_t *t = (void *)tables;
		// print_efi_guid(&t->VendorGuid);
		if (efi_guidcmp(t->VendorGuid, guid) == 0)
			return efi_table_attr(t, VendorTable);

		tables++;
	}
	return NULL;
}

/**
 * efi_exit_boot_services() - Exit boot services
 * @handle:	handle of the exiting image
 * @priv:	argument to be passed to @priv_func
 * @priv_func:	function to process the memory map before exiting boot services
 *
 * Handle calling ExitBootServices according to the requirements set out by the
 * spec.  Obtains the current memory map, and returns that info after calling
 * ExitBootServices.  The client must specify a function to perform any
 * processing of the memory map data prior to ExitBootServices.  A client
 * specific structure may be passed to the function via priv.  The client
 * function may be called multiple times.
 *
 * Return:	status code
 */
efi_status_t efi_exit_boot_services(void *handle, void *priv,
				    efi_exit_boot_map_processing priv_func)
{
	struct efi_boot_memmap *map;
	efi_status_t status;

	if (efi_disable_pci_dma) {
		efi_todo("efi_exit_boot_services:: efi_disable_pci_dma: efi_pci_disable_bridge_busmaster");
		// efi_pci_disable_bridge_busmaster();
	}

	status = efi_get_memory_map(&map, true);
	if (status != EFI_SUCCESS)
		return status;
	efi_debug("before priv_func\n");
	status = priv_func(map, priv);
	if (status != EFI_SUCCESS) {
		
		efi_bs_call(FreePool, map);
		return status;
	}

	efi_debug("before ExitBootServices, handle=%p, map_key=%p\n", handle, map->map_key);
	efi_debug("BS->ExitBootServices=%p\n", BS->ExitBootServices);
	efi_debug("ST->BS->ExitBootServices=%p\n", ST->BootServices->ExitBootServices);
	status = efi_bs_call(ExitBootServices, handle, map->map_key);
	// exit之后不能再使用打印函数，否则会出现错误

	if (status == EFI_INVALID_PARAMETER) {
		/*
		 * The memory map changed between efi_get_memory_map() and
		 * exit_boot_services().  Per the UEFI Spec v2.6, Section 6.4:
		 * EFI_BOOT_SERVICES.ExitBootServices we need to get the
		 * updated map, and try again.  The spec implies one retry
		 * should be sufficent, which is confirmed against the EDK2
		 * implementation.  Per the spec, we can only invoke
		 * get_memory_map() and exit_boot_services() - we cannot alloc
		 * so efi_get_memory_map() cannot be used, and we must reuse
		 * the buffer.  For all practical purposes, the headroom in the
		 * buffer should account for any changes in the map so the call
		 * to get_memory_map() is expected to succeed here.
		 */
		map->map_size = map->buff_size;
		status = efi_bs_call(GetMemoryMap, &map->map_size, &map->map,
				     &map->map_key, &map->desc_size,
				     &map->desc_ver);

		/* exit_boot_services() was called, thus cannot free */
		if (status != EFI_SUCCESS)
			return status;

		status = priv_func(map, priv);
		/* exit_boot_services() was called, thus cannot free */
		if (status != EFI_SUCCESS)
			return status;

		status = efi_bs_call(ExitBootServices, handle, map->map_key);
	}

	return status;
}