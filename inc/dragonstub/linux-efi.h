#pragma once
#include <efidef.h>
#include "compiler_attributes.h"
#include "types.h"
#include "linux/pfn.h"
#if defined(CONFIG_riscv64)
#include "riscv64.h"
#endif

#define efi_table_hdr_t EFI_TABLE_HEADER
#define efi_guid_t EFI_GUID
#define efi_runtime_services_t EFI_RUNTIME_SERVICES
#define efi_boot_services_t EFI_BOOT_SERVICES

#define MAKE_EFI_GUID(a, b, c, d...)       \
	(efi_guid_t)                       \
	{                                  \
		a, b & 0xffff, c & 0xffff, \
		{                          \
			d                  \
		}                          \
	}

/*
 * EFI Configuration Table and GUID definitions
 *
 * These are all defined in a single line to make them easier to
 * grep for and to see them at a glance - while still having a
 * similar structure to the definitions in the spec.
 *
 * Here's how they are structured:
 *
 * GUID: 12345678-1234-1234-1234-123456789012
 * Spec:
 *      #define EFI_SOME_PROTOCOL_GUID \
 *        {0x12345678,0x1234,0x1234,\
 *          {0x12,0x34,0x12,0x34,0x56,0x78,0x90,0x12}}
 * Here:
 *	#define SOME_PROTOCOL_GUID		EFI_GUID(0x12345678, 0x1234, 0x1234,  0x12, 0x34, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12)
 *					^ tabs					    ^extra space
 *
 * Note that the 'extra space' separates the values at the same place
 * where the UEFI SPEC breaks the line.
 */
#define NULL_GUID                                                         \
	MAKE_EFI_GUID(0x00000000, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, \
		      0x00, 0x00, 0x00, 0x00)
#ifndef MPS_TABLE_GUID
#define MPS_TABLE_GUID                                                    \
	MAKE_EFI_GUID(0xeb9d2d2f, 0x2d88, 0x11d3, 0x9a, 0x16, 0x00, 0x90, \
		      0x27, 0x3f, 0xc1, 0x4d)
#endif
#ifndef ACPI_TABLE_GUID
#define ACPI_TABLE_GUID                                                   \
	MAKE_EFI_GUID(0xeb9d2d30, 0x2d88, 0x11d3, 0x9a, 0x16, 0x00, 0x90, \
		      0x27, 0x3f, 0xc1, 0x4d)
#endif
#ifndef ACPI_20_TABLE_GUID
#define ACPI_20_TABLE_GUID                                                \
	MAKE_EFI_GUID(0x8868e871, 0xe4f1, 0x11d3, 0xbc, 0x22, 0x00, 0x80, \
		      0xc7, 0x3c, 0x88, 0x81)
#endif
#ifndef SMBIOS_TABLE_GUID
#define SMBIOS_TABLE_GUID                                                 \
	MAKE_EFI_GUID(0xeb9d2d31, 0x2d88, 0x11d3, 0x9a, 0x16, 0x00, 0x90, \
		      0x27, 0x3f, 0xc1, 0x4d)
#endif
#ifndef SMBIOS3_TABLE_GUID
#define SMBIOS3_TABLE_GUID                                                \
	MAKE_EFI_GUID(0xf2fd1544, 0x9794, 0x4a2c, 0x99, 0x2e, 0xe5, 0xbb, \
		      0xcf, 0x20, 0xe3, 0x94)
#endif
#ifndef SAL_SYSTEM_TABLE_GUID
#define SAL_SYSTEM_TABLE_GUID                                             \
	MAKE_EFI_GUID(0xeb9d2d32, 0x2d88, 0x11d3, 0x9a, 0x16, 0x00, 0x90, \
		      0x27, 0x3f, 0xc1, 0x4d)
#endif
#define HCDP_TABLE_GUID                                                   \
	MAKE_EFI_GUID(0xf951938d, 0x620b, 0x42ef, 0x82, 0x79, 0xa8, 0x4b, \
		      0x79, 0x61, 0x78, 0x98)
#define UGA_IO_PROTOCOL_GUID                                              \
	MAKE_EFI_GUID(0x61a4d49e, 0x6f68, 0x4f1b, 0xb9, 0x22, 0xa8, 0x6e, \
		      0xed, 0x0b, 0x07, 0xa2)
#define EFI_GLOBAL_VARIABLE_GUID                                          \
	MAKE_EFI_GUID(0x8be4df61, 0x93ca, 0x11d2, 0xaa, 0x0d, 0x00, 0xe0, \
		      0x98, 0x03, 0x2b, 0x8c)
#define UV_SYSTEM_TABLE_GUID                                              \
	MAKE_EFI_GUID(0x3b13a7d4, 0x633e, 0x11dd, 0x93, 0xec, 0xda, 0x25, \
		      0x56, 0xd8, 0x95, 0x93)
#define LINUX_EFI_CRASH_GUID                                              \
	MAKE_EFI_GUID(0xcfc8fc79, 0xbe2e, 0x4ddc, 0x97, 0xf0, 0x9f, 0x98, \
		      0xbf, 0xe2, 0x98, 0xa0)
#define LOADED_IMAGE_PROTOCOL_GUID                                        \
	MAKE_EFI_GUID(0x5b1b31a1, 0x9562, 0x11d2, 0x8e, 0x3f, 0x00, 0xa0, \
		      0xc9, 0x69, 0x72, 0x3b)
#define LOADED_IMAGE_DEVICE_PATH_PROTOCOL_GUID                            \
	MAKE_EFI_GUID(0xbc62157e, 0x3e33, 0x4fec, 0x99, 0x20, 0x2d, 0x3b, \
		      0x36, 0xd7, 0x50, 0xdf)
#ifndef EFI_DEVICE_PATH_PROTOCOL_GUID
#define EFI_DEVICE_PATH_PROTOCOL_GUID                                     \
	MAKE_EFI_GUID(0x09576e91, 0x6d3f, 0x11d2, 0x8e, 0x39, 0x00, 0xa0, \
		      0xc9, 0x69, 0x72, 0x3b)
#endif
#ifndef EFI_DEVICE_PATH_TO_TEXT_PROTOCOL_GUID
#define EFI_DEVICE_PATH_TO_TEXT_PROTOCOL_GUID                             \
	MAKE_EFI_GUID(0x8b843e20, 0x8132, 0x4852, 0x90, 0xcc, 0x55, 0x1a, \
		      0x4e, 0x4a, 0x7f, 0x1c)
#endif
#ifndef EFI_DEVICE_PATH_FROM_TEXT_PROTOCOL_GUID
#define EFI_DEVICE_PATH_FROM_TEXT_PROTOCOL_GUID                           \
	MAKE_EFI_GUID(0x05c99a21, 0xc70f, 0x4ad2, 0x8a, 0x5f, 0x35, 0xdf, \
		      0x33, 0x43, 0xf5, 0x1e)
#endif
#ifndef EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID
#define EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID                                 \
	MAKE_EFI_GUID(0x9042a9de, 0x23dc, 0x4a38, 0x96, 0xfb, 0x7a, 0xde, \
		      0xd0, 0x80, 0x51, 0x6a)
#endif
#define EFI_UGA_PROTOCOL_GUID                                             \
	MAKE_EFI_GUID(0x982c298b, 0xf4fa, 0x41cb, 0xb8, 0x38, 0x77, 0xaa, \
		      0x68, 0x8f, 0xb8, 0x39)
#ifndef EFI_PCI_IO_PROTOCOL_GUID
#define EFI_PCI_IO_PROTOCOL_GUID                                          \
	MAKE_EFI_GUID(0x4cf5b200, 0x68b8, 0x4ca5, 0x9e, 0xec, 0xb2, 0x3e, \
		      0x3f, 0x50, 0x02, 0x9a)
#endif
#ifndef EFI_FILE_INFO_ID
#define EFI_FILE_INFO_ID                                                  \
	MAKE_EFI_GUID(0x09576e92, 0x6d3f, 0x11d2, 0x8e, 0x39, 0x00, 0xa0, \
		      0xc9, 0x69, 0x72, 0x3b)
#endif
#define EFI_SYSTEM_RESOURCE_TABLE_GUID                                    \
	MAKE_EFI_GUID(0xb122a263, 0x3661, 0x4f68, 0x99, 0x29, 0x78, 0xf8, \
		      0xb0, 0xd6, 0x21, 0x80)
#define EFI_FILE_SYSTEM_GUID                                              \
	MAKE_EFI_GUID(0x964e5b22, 0x6459, 0x11d2, 0x8e, 0x39, 0x00, 0xa0, \
		      0xc9, 0x69, 0x72, 0x3b)
#define DEVICE_TREE_GUID                                                  \
	MAKE_EFI_GUID(0xb1b621d5, 0xf19c, 0x41a5, 0x83, 0x0b, 0xd9, 0x15, \
		      0x2c, 0x69, 0xaa, 0xe0)
#define EFI_PROPERTIES_TABLE_GUID                                         \
	MAKE_EFI_GUID(0x880aaca3, 0x4adc, 0x4a04, 0x90, 0x79, 0xb7, 0x47, \
		      0x34, 0x08, 0x25, 0xe5)
#ifndef EFI_RNG_PROTOCOL_GUID
#define EFI_RNG_PROTOCOL_GUID                                             \
	MAKE_EFI_GUID(0x3152bca5, 0xeade, 0x433d, 0x86, 0x2e, 0xc0, 0x1c, \
		      0xdc, 0x29, 0x1f, 0x44)
#endif
#ifndef EFI_RNG_ALGORITHM_RAW
#define EFI_RNG_ALGORITHM_RAW                                             \
	MAKE_EFI_GUID(0xe43176d7, 0xb6e8, 0x4827, 0xb7, 0x84, 0x7f, 0xfd, \
		      0xc4, 0xb6, 0x85, 0x61)
#endif
#define EFI_MEMORY_ATTRIBUTES_TABLE_GUID                                  \
	MAKE_EFI_GUID(0xdcfa911d, 0x26eb, 0x469f, 0xa2, 0x20, 0x38, 0xb7, \
		      0xdc, 0x46, 0x12, 0x20)
#define EFI_CONSOLE_OUT_DEVICE_GUID                                       \
	MAKE_EFI_GUID(0xd3b36f2c, 0xd551, 0x11d4, 0x9a, 0x46, 0x00, 0x90, \
		      0x27, 0x3f, 0xc1, 0x4d)
#define APPLE_PROPERTIES_PROTOCOL_GUID                                    \
	MAKE_EFI_GUID(0x91bd12fe, 0xf6c3, 0x44fb, 0xa5, 0xb7, 0x51, 0x22, \
		      0xab, 0x30, 0x3a, 0xe0)
#define EFI_TCG2_PROTOCOL_GUID                                            \
	MAKE_EFI_GUID(0x607f766c, 0x7455, 0x42be, 0x93, 0x0b, 0xe4, 0xd7, \
		      0x6d, 0xb2, 0x72, 0x0f)
#ifndef EFI_LOAD_FILE_PROTOCOL_GUID
#define EFI_LOAD_FILE_PROTOCOL_GUID                                       \
	MAKE_EFI_GUID(0x56ec3091, 0x954c, 0x11d2, 0x8e, 0x3f, 0x00, 0xa0, \
		      0xc9, 0x69, 0x72, 0x3b)
#endif
#define EFI_LOAD_FILE2_PROTOCOL_GUID                                      \
	MAKE_EFI_GUID(0x4006c0c1, 0xfcb3, 0x403e, 0x99, 0x6d, 0x4a, 0x6c, \
		      0x87, 0x24, 0xe0, 0x6d)
#define EFI_RT_PROPERTIES_TABLE_GUID                                      \
	MAKE_EFI_GUID(0xeb66918a, 0x7eef, 0x402a, 0x84, 0x2e, 0x93, 0x1d, \
		      0x21, 0xc3, 0x8a, 0xe9)
#define EFI_DXE_SERVICES_TABLE_GUID                                       \
	MAKE_EFI_GUID(0x05ad34ba, 0x6f02, 0x4214, 0x95, 0x2e, 0x4d, 0xa0, \
		      0x39, 0x8e, 0x2b, 0xb9)
#define EFI_SMBIOS_PROTOCOL_GUID                                          \
	MAKE_EFI_GUID(0x03583ff6, 0xcb36, 0x4940, 0x94, 0x7e, 0xb9, 0xb3, \
		      0x9f, 0x4a, 0xfa, 0xf7)
#define EFI_MEMORY_ATTRIBUTE_PROTOCOL_GUID                                \
	MAKE_EFI_GUID(0xf4560cf6, 0x40ec, 0x4b4a, 0xa1, 0x92, 0xbf, 0x1d, \
		      0x57, 0xd0, 0xb1, 0x89)

#define EFI_IMAGE_SECURITY_DATABASE_GUID                                  \
	MAKE_EFI_GUID(0xd719b2cb, 0x3d3a, 0x4596, 0xa3, 0xbc, 0xda, 0xd0, \
		      0x0e, 0x67, 0x65, 0x6f)
#define EFI_SHIM_LOCK_GUID                                                \
	MAKE_EFI_GUID(0x605dab50, 0xe046, 0x4300, 0xab, 0xb6, 0x3d, 0xd8, \
		      0x10, 0xdd, 0x8b, 0x23)

#define EFI_CERT_SHA256_GUID                                              \
	MAKE_EFI_GUID(0xc1c41626, 0x504c, 0x4092, 0xac, 0xa9, 0x41, 0xf9, \
		      0x36, 0x93, 0x43, 0x28)
#define EFI_CERT_X509_GUID                                                \
	MAKE_EFI_GUID(0xa5c059a1, 0x94e4, 0x4aa7, 0x87, 0xb5, 0xab, 0x15, \
		      0x5c, 0x2b, 0xf0, 0x72)
#define EFI_CERT_X509_SHA256_GUID                                         \
	MAKE_EFI_GUID(0x3bd2a492, 0x96c0, 0x4079, 0xb4, 0x20, 0xfc, 0xf9, \
		      0x8e, 0xf1, 0x03, 0xed)
#define EFI_CC_BLOB_GUID                                                  \
	MAKE_EFI_GUID(0x067b1f5f, 0xcf26, 0x44c5, 0x85, 0x54, 0x93, 0xd7, \
		      0x77, 0x91, 0x2d, 0x42)

#define RISCV_EFI_BOOT_PROTOCOL_GUID                                      \
	MAKE_EFI_GUID(0xccd15fec, 0x6f73, 0x4eec, 0x83, 0x95, 0x3e, 0x69, \
		      0xe4, 0xb9, 0x40, 0xbf)

#if defined(CONFIG_X86_64)
#define __efiapi __attribute__((ms_abi))
#elif defined(CONFIG_X86_32)
#define __efiapi __attribute__((regparm(0)))
#elif defined(CONFIG_riscv64)
#define __efiapi
#elif defined(CONFIG_aarch64)
#define __efiapi
#else
#error "Unsupported architecture"
#endif

/*
 * EFI Device Path information
 */
#define EFI_DEV_HW 0x01
#define EFI_DEV_PCI 1
#define EFI_DEV_PCCARD 2
#define EFI_DEV_MEM_MAPPED 3
#define EFI_DEV_VENDOR 4
#define EFI_DEV_CONTROLLER 5
#define EFI_DEV_ACPI 0x02
#define EFI_DEV_BASIC_ACPI 1
#define EFI_DEV_EXPANDED_ACPI 2
#define EFI_DEV_MSG 0x03
#define EFI_DEV_MSG_ATAPI 1
#define EFI_DEV_MSG_SCSI 2
#define EFI_DEV_MSG_FC 3
#define EFI_DEV_MSG_1394 4
#define EFI_DEV_MSG_USB 5
#define EFI_DEV_MSG_USB_CLASS 15
#define EFI_DEV_MSG_I20 6
#define EFI_DEV_MSG_MAC 11
#define EFI_DEV_MSG_IPV4 12
#define EFI_DEV_MSG_IPV6 13
#define EFI_DEV_MSG_INFINIBAND 9
#define EFI_DEV_MSG_UART 14
#define EFI_DEV_MSG_VENDOR 10
#define EFI_DEV_MEDIA 0x04
#define EFI_DEV_MEDIA_HARD_DRIVE 1
#define EFI_DEV_MEDIA_CDROM 2
#define EFI_DEV_MEDIA_VENDOR 3
#define EFI_DEV_MEDIA_FILE 4
#define EFI_DEV_MEDIA_PROTOCOL 5
#define EFI_DEV_MEDIA_REL_OFFSET 8
#define EFI_DEV_BIOS_BOOT 0x05
#define EFI_DEV_END_PATH 0x7F
#define EFI_DEV_END_PATH2 0xFF
#define EFI_DEV_END_INSTANCE 0x01
#define EFI_DEV_END_ENTIRE 0xFF

struct efi_generic_dev_path {
	u8 type;
	u8 sub_type;
	u16 length;
} __packed;

struct efi_acpi_dev_path {
	struct efi_generic_dev_path header;
	u32 hid;
	u32 uid;
} __packed;

struct efi_pci_dev_path {
	struct efi_generic_dev_path header;
	u8 fn;
	u8 dev;
} __packed;

struct efi_vendor_dev_path {
	struct efi_generic_dev_path header;
	efi_guid_t vendorguid;
	u8 vendordata[];
} __packed;

struct efi_rel_offset_dev_path {
	struct efi_generic_dev_path header;
	u32 reserved;
	u64 starting_offset;
	u64 ending_offset;
} __packed;

struct efi_mem_mapped_dev_path {
	struct efi_generic_dev_path header;
	u32 memory_type;
	u64 starting_addr;
	u64 ending_addr;
} __packed;

struct efi_file_path_dev_path {
	struct efi_generic_dev_path header;
	efi_char16_t filename[];
} __packed;

struct efi_dev_path {
	union {
		struct efi_generic_dev_path header;
		struct efi_acpi_dev_path acpi;
		struct efi_pci_dev_path pci;
		struct efi_vendor_dev_path vendor;
		struct efi_rel_offset_dev_path rel_offset;
	};
} __packed;

// struct device *efi_get_device_by_path(const struct efi_dev_path **node,
// 				      size_t *len);

static inline void memrange_efi_to_native(u64 *addr, u64 *npages)
{
	*npages = PFN_UP(*addr + (*npages << EFI_PAGE_SHIFT)) - PFN_DOWN(*addr);
	*addr &= PAGE_MASK;
}
