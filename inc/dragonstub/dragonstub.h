#pragma once

#include <efi.h>
#include <efilib.h>
#include "printk.h"
#include "linux-efi.h"
#include "compiler_attributes.h"
#include "linux/ctype.h"
#include <lib.h>
#include <dragonstub/linux/hex.h>
#include "types.h"
#include "linux/div64.h"

/// @brief
/// @param image
/// @param cmdline_ptr
/// @return
EFI_STATUS efi_handle_cmdline(EFI_LOADED_IMAGE *image, char **cmdline_ptr);

char *efi_convert_cmdline(EFI_LOADED_IMAGE *image, int *cmd_line_len);

#define efi_table_attr(inst, attr) (inst->attr)

typedef u32 efi_tcg2_event_log_format;
#define INITRD_EVENT_TAG_ID 0x8F3B22ECU
#define LOAD_OPTIONS_EVENT_TAG_ID 0x8F3B22EDU
#define EV_EVENT_TAG 0x00000006U
#define EFI_TCG2_EVENT_HEADER_VERSION 0x1

struct efi_tcg2_event {
	u32 event_size;
	struct {
		u32 header_size;
		u16 header_version;
		u32 pcr_index;
		u32 event_type;
	} __packed event_header;
	/* u8[] event follows here */
} __packed;

struct efi_tcg2_tagged_event {
	u32 tagged_event_id;
	u32 tagged_event_data_size;
	/* u8  tagged event data follows here */
} __packed;

typedef struct efi_tcg2_event efi_tcg2_event_t;
typedef struct efi_tcg2_tagged_event efi_tcg2_tagged_event_t;
typedef union efi_tcg2_protocol efi_tcg2_protocol_t;

union efi_tcg2_protocol {
	struct {
		void *get_capability;
		efi_status_t(__efiapi *get_event_log)(efi_tcg2_protocol_t *,
						      efi_tcg2_event_log_format,
						      efi_physical_addr_t *,
						      efi_physical_addr_t *,
						      efi_bool_t *);
		efi_status_t(__efiapi *hash_log_extend_event)(
			efi_tcg2_protocol_t *, u64, efi_physical_addr_t, u64,
			const efi_tcg2_event_t *);
		void *submit_command;
		void *get_active_pcr_banks;
		void *set_active_pcr_banks;
		void *get_result_of_set_active_pcr_banks;
	};
	struct {
		u32 get_capability;
		u32 get_event_log;
		u32 hash_log_extend_event;
		u32 submit_command;
		u32 get_active_pcr_banks;
		u32 set_active_pcr_banks;
		u32 get_result_of_set_active_pcr_banks;
	} mixed_mode;
};

struct riscv_efi_boot_protocol {
	u64 revision;

	efi_status_t(__efiapi *get_boot_hartid)(
		struct riscv_efi_boot_protocol *, unsigned long *boot_hartid);
};

typedef struct {
	u32 attributes;
	u16 file_path_list_length;
	u8 variable_data[];
	// efi_char16_t description[];
	// efi_device_path_protocol_t file_path_list[];
	// u8 optional_data[];
} __packed efi_load_option_t;

typedef struct efi_generic_dev_path efi_device_path_protocol_t;

#define EFI_LOAD_OPTION_ACTIVE 0x0001U
#define EFI_LOAD_OPTION_FORCE_RECONNECT 0x0002U
#define EFI_LOAD_OPTION_HIDDEN 0x0008U
#define EFI_LOAD_OPTION_CATEGORY 0x1f00U
#define EFI_LOAD_OPTION_CATEGORY_BOOT 0x0000U
#define EFI_LOAD_OPTION_CATEGORY_APP 0x0100U

#define EFI_LOAD_OPTION_BOOT_MASK                          \
	(EFI_LOAD_OPTION_ACTIVE | EFI_LOAD_OPTION_HIDDEN | \
	 EFI_LOAD_OPTION_CATEGORY)
#define EFI_LOAD_OPTION_MASK \
	(EFI_LOAD_OPTION_FORCE_RECONNECT | EFI_LOAD_OPTION_BOOT_MASK)

typedef struct {
	u32 attributes;
	u16 file_path_list_length;
	const efi_char16_t *description;
	const efi_device_path_protocol_t *file_path_list;
	u32 optional_data_size;
	const void *optional_data;
} efi_load_option_unpacked_t;

/* The macro below handles dispatching via the thunk if needed */

#define efi_fn_call(inst, func, ...) ((inst)->func(__VA_ARGS__))

#define efi_call_proto(inst, func, ...)                           \
	({                                                        \
		__typeof__(inst) __inst = (inst);                 \
		efi_fn_call(__inst, func, __inst, ##__VA_ARGS__); \
	})
