#pragma once

#include <efi.h>
#include <efilib.h>
#include "printk.h"
#include "linux-efi.h"
#include "compiler_attributes.h"
#include "linux/ctype.h"
#include <lib.h>
#include <dragonstub/linux/hex.h>
#include <dragonstub/minmax.h>
#include "types.h"
#include "linux/div64.h"
#include "limits.h"
#include "linux/sizes.h"

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

typedef EFI_LOADED_IMAGE efi_loaded_image_t;

/* The macro below handles dispatching via the thunk if needed */

#define efi_fn_call(inst, func, ...) ((inst)->func(__VA_ARGS__))

#define efi_call_proto(inst, func, ...)                           \
	({                                                        \
		__typeof__(inst) __inst = (inst);                 \
		efi_fn_call(__inst, func, __inst, ##__VA_ARGS__); \
	})

#define efi_rt_call(func, ...) \
	efi_fn_call(efi_table_attr(ST, RuntimeServices), func, ##__VA_ARGS__)

#define get_efi_var(name, vendor, ...)                   \
	efi_rt_call(GetVariable, (efi_char16_t *)(name), \
		    (efi_guid_t *)(vendor), __VA_ARGS__)

#define set_efi_var(name, vendor, ...)                   \
	efi_rt_call(SetVariable, (efi_char16_t *)(name), \
		    (efi_guid_t *)(vendor), __VA_ARGS__)

char *skip_spaces(const char *str);
long simple_strtol(const char *cp, char **endp, unsigned int base);
unsigned int atou(const char *s);
/**
 * simple_strtoull - convert a string to an unsigned long long
 * @cp: The start of the string
 * @endp: A pointer to the end of the parsed string will be placed here
 * @base: The number base to use
 */
unsigned long long simple_strtoull(const char *cp, char **endp,
				   unsigned int base);
long simple_strtol(const char *cp, char **endp, unsigned int base);

#define strtoul(cp, endp, base) simple_strtoull(cp, endp, base)

size_t strnlen(const char *s, size_t maxlen);
/**
 * strlen - Find the length of a string
 * @s: The string to be sized
 */
size_t strlen(const char *s);
int strncmp(const char *cs, const char *ct, size_t count);
int strcmp(const char *str1, const char *str2);
char *strchr(const char *s, int c);
/**
 * strstr - Find the first substring in a %NUL terminated string
 * @s1: The string to be searched
 * @s2: The string to search for
 */
char *strstr(const char *s1, const char *s2);

char *next_arg(char *args, char **param, char **val);

/**
 * strstarts - does @str start with @prefix?
 * @str: string to examine
 * @prefix: prefix to look for.
 */
static inline bool strstarts(const char *str, const char *prefix)
{
	return strncmp(str, prefix, strlen(prefix)) == 0;
}

efi_status_t efi_parse_options(char const *cmdline);

/// @brief 要加载的内核负载信息
struct payload_info {
	/// @brief 负载起始地址
	u64 payload_addr;
	/// @brief 负载大小
	u64 payload_size;
	/// @brief 被加载到的物理地址
	u64 loaded_paddr;
	/// @brief 加载了多大
	u64 loaded_size;
	/// @brief 加载的内核的入口物理地址
	u64 kernel_entry;
};

/// @brief 寻找要加载的内核负载
/// @param handle efi_handle
/// @param image efi_loaded_image_t
/// @param ret_info 返回的负载信息
/// @return
efi_status_t find_payload(efi_handle_t handle, efi_loaded_image_t *loaded_image,
			  struct payload_info *ret_info);

/* shared entrypoint between the normal stub and the zboot stub */
efi_status_t efi_stub_common(efi_handle_t handle, efi_loaded_image_t *image,
			     struct payload_info *payload_info,
			     char *cmdline_ptr);

efi_status_t efi_boot_kernel(efi_handle_t handle, efi_loaded_image_t *image,
			     struct payload_info *payload_info,
			     char *cmdline_ptr);

efi_status_t check_platform_features(void);
void *get_efi_config_table(efi_guid_t guid);
typedef EFI_CONFIGURATION_TABLE efi_config_table_t;

static inline int efi_guidcmp(efi_guid_t left, efi_guid_t right)
{
	return memcmp(&left, &right, sizeof(efi_guid_t));
}

static inline char *efi_guid_to_str(efi_guid_t *guid, char *out)
{
	snprintf(out, 1024, "%pUl", &guid->Data1);
	return out;
}

static inline void print_efi_guid(efi_guid_t *guid)
{
	efi_info("GUID: data1: %p data2: %p data3: %p data4: %p\n", guid->Data1,
		 guid->Data2, guid->Data3, guid->Data4);
}

/*
 * efi_allocate_virtmap() - create a pool allocation for the virtmap
 *
 * Create an allocation that is of sufficient size to hold all the memory
 * descriptors that will be passed to SetVirtualAddressMap() to inform the
 * firmware about the virtual mapping that will be used under the OS to call
 * into the firmware.
 */
efi_status_t efi_alloc_virtmap(efi_memory_desc_t **virtmap,
			       unsigned long *desc_size, u32 *desc_ver);

/*
 * efi_get_virtmap() - create a virtual mapping for the EFI memory map
 *
 * This function populates the virt_addr fields of all memory region descriptors
 * in @memory_map whose EFI_MEMORY_RUNTIME attribute is set. Those descriptors
 * are also copied to @runtime_map, and their total count is returned in @count.
 */
void efi_get_virtmap(efi_memory_desc_t *memory_map, unsigned long map_size,
		     unsigned long desc_size, efi_memory_desc_t *runtime_map,
		     int *count);

extern bool efi_nochunk;
extern bool efi_nokaslr;
extern bool efi_novamap;

/*
 * Determine whether we're in secure boot mode.
 */
enum efi_secureboot_mode efi_get_secureboot(void);
void *get_fdt(unsigned long *fdt_size);

/*
 * Allow the platform to override the allocation granularity: this allows
 * systems that have the capability to run with a larger page size to deal
 * with the allocations for initrd and fdt more efficiently.
 */
#ifndef EFI_ALLOC_ALIGN
#define EFI_ALLOC_ALIGN EFI_PAGE_SIZE
#endif

#ifndef EFI_ALLOC_LIMIT
#define EFI_ALLOC_LIMIT ULONG_MAX
#endif

/*
 * Allocation types for calls to boottime->allocate_pages.
 */
#define EFI_ALLOCATE_ANY_PAGES 0
#define EFI_ALLOCATE_MAX_ADDRESS 1
#define EFI_ALLOCATE_ADDRESS 2
#define EFI_MAX_ALLOCATE_TYPE 3

/**
 * efi_allocate_pages_aligned() - Allocate memory pages
 * @size:	minimum number of bytes to allocate
 * @addr:	On return the address of the first allocated page. The first
 *		allocated page has alignment EFI_ALLOC_ALIGN which is an
 *		architecture dependent multiple of the page size.
 * @max:	the address that the last allocated memory page shall not
 *		exceed
 * @align:	minimum alignment of the base of the allocation
 *
 * Allocate pages as EFI_LOADER_DATA. The allocated pages are aligned according
 * to @align, which should be >= EFI_ALLOC_ALIGN. The last allocated page will
 * not exceed the address given by @max.
 *
 * Return:	status code
 */
efi_status_t efi_allocate_pages_aligned(unsigned long size, unsigned long *addr,
					unsigned long max, unsigned long align,
					int memory_type);

/**
 * efi_allocate_pages() - Allocate memory pages
 * @size:	minimum number of bytes to allocate
 * @addr:	On return the address of the first allocated page. The first
 *		allocated page has alignment EFI_ALLOC_ALIGN which is an
 *		architecture dependent multiple of the page size.
 * @max:	the address that the last allocated memory page shall not
 *		exceed
 *
 * Allocate pages as EFI_LOADER_DATA. The allocated pages are aligned according
 * to EFI_ALLOC_ALIGN. The last allocated page will not exceed the address
 * given by @max.
 *
 * Return:	status code
 */
efi_status_t efi_allocate_pages(unsigned long size, unsigned long *addr,
				unsigned long max);

/**
 * efi_allocate_pages_exact() - Allocate memory pages at a specific address
 * @size:	minimum number of bytes to allocate
 * @addr:	The address of the first allocated page.
 *
 * Allocate pages as EFI_LOADER_DATA. The allocated pages are aligned according
 * to EFI_ALLOC_ALIGN.
 *
 * Return:	status code
 */
efi_status_t efi_allocate_pages_exact(unsigned long size, unsigned long addr);

/**
 * efi_free() - free memory pages
 * @size:	size of the memory area to free in bytes
 * @addr:	start of the memory area to free (must be EFI_PAGE_SIZE
 *		aligned)
 *
 * @size is rounded up to a multiple of EFI_ALLOC_ALIGN which is an
 * architecture specific multiple of EFI_PAGE_SIZE. So this function should
 * only be used to return pages allocated with efi_allocate_pages() or
 * efi_low_alloc_above().
 */
void efi_free(unsigned long size, unsigned long addr);

/*
 * An efi_boot_memmap is used by efi_get_memory_map() to return the
 * EFI memory map in a dynamically allocated buffer.
 *
 * The buffer allocated for the EFI memory map includes extra room for
 * a minimum of EFI_MMAP_NR_SLACK_SLOTS additional EFI memory descriptors.
 * This facilitates the reuse of the EFI memory map buffer when a second
 * call to ExitBootServices() is needed because of intervening changes to
 * the EFI memory map. Other related structures, e.g. x86 e820ext, need
 * to factor in this headroom requirement as well.
 */
#define EFI_MMAP_NR_SLACK_SLOTS 8

/**
 * efi_get_memory_map() - get memory map
 * @map:		pointer to memory map pointer to which to assign the
 *			newly allocated memory map
 * @install_cfg_tbl:	whether or not to install the boot memory map as a
 *			configuration table
 *
 * Retrieve the UEFI memory map. The allocated memory leaves room for
 * up to EFI_MMAP_NR_SLACK_SLOTS additional memory map entries.
 *
 * Return:	status code
 */
efi_status_t efi_get_memory_map(struct efi_boot_memmap **map,
				bool install_cfg_tbl);

#ifdef CONFIG_64BIT
#define MAX_FDT_SIZE (1UL << 21)
#else
#error "MAX_FDT_SIZE not yet defined for 32-bit"
#endif

/* Helper macros for the usual case of using simple C variables: */
#ifndef fdt_setprop_inplace_var
#define fdt_setprop_inplace_var(fdt, node_offset, name, var) \
	fdt_setprop_inplace((fdt), (node_offset), (name), &(var), sizeof(var))
#endif

#ifndef fdt_setprop_var
#define fdt_setprop_var(fdt, node_offset, name, var) \
	fdt_setprop((fdt), (node_offset), (name), &(var), sizeof(var))
#endif

#define efi_get_handle_at(array, idx)     \
	(efi_is_native() ? (array)[idx] : \
			   (efi_handle_t)(unsigned long)((u32 *)(array))[idx])

#define efi_get_handle_num(size) \
	((size) / (efi_is_native() ? sizeof(efi_handle_t) : sizeof(u32)))

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
efi_status_t efi_get_random_bytes(unsigned long size, u8 *out);

typedef efi_status_t (*efi_exit_boot_map_processing)(
	struct efi_boot_memmap *map, void *priv);

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
				    efi_exit_boot_map_processing priv_func);

void __noreturn efi_enter_kernel(struct payload_info *payload_info,
				 unsigned long fdt, unsigned long fdt_size);

typedef union efi_memory_attribute_protocol efi_memory_attribute_protocol_t;

union efi_memory_attribute_protocol {
	struct {
		efi_status_t(__efiapi *get_memory_attributes)(
			efi_memory_attribute_protocol_t *, efi_physical_addr_t,
			u64, u64 *);

		efi_status_t(__efiapi *set_memory_attributes)(
			efi_memory_attribute_protocol_t *, efi_physical_addr_t,
			u64, u64);

		efi_status_t(__efiapi *clear_memory_attributes)(
			efi_memory_attribute_protocol_t *, efi_physical_addr_t,
			u64, u64);
	};
	struct {
		u32 get_memory_attributes;
		u32 set_memory_attributes;
		u32 clear_memory_attributes;
	} mixed_mode;
};

/**
 * 安装到efi config table的信息
 * 
 * 表示dragonstub把内核加载到的地址和大小
*/
struct dragonstub_payload_efi {
	u64 loaded_addr;
	u64 size;
};

#define DRAGONSTUB_EFI_PAYLOAD_EFI_GUID                               \
	MAKE_EFI_GUID(0xddf1d47c, 0x102c, 0xaaf9, 0xce, 0x34, 0xbc, 0xef, \
		      0x98, 0x12, 0x00, 0x31)
