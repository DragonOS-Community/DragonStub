#include <dragonstub/dragonstub.h>
#include <dragonstub/linux/unaligned.h>
#include "efilib.h"
#include <libfdt.h>
#include <asm/csr.h>

/// @brief 当前的hartid
static unsigned long hartid;

typedef void __noreturn (*jump_kernel_func)(unsigned long, unsigned long);

static efi_status_t get_boot_hartid_from_fdt(void)
{
	const void *fdt;
	int chosen_node, len;
	const void *prop;

	// efi_guid_t device_tree_guid = *(efi_guid_t *)&tmp;
	fdt = get_efi_config_table(DEVICE_TREE_GUID);
	if (!fdt) {
		efi_err("Failed to get FDT from EFI config table\n");
		return EFI_INVALID_PARAMETER;
	}

	chosen_node = fdt_path_offset(fdt, "/chosen");
	if (chosen_node < 0) {
		efi_err("Failed to find /chosen node in FDT\n");
		return EFI_INVALID_PARAMETER;
	}

	prop = fdt_getprop((void *)fdt, chosen_node, "boot-hartid", &len);
	if (!prop) {
		efi_err("Failed to find boot-hartid property in FDT\n");
		return EFI_INVALID_PARAMETER;
	}

	if (len == sizeof(u32))
		hartid = (unsigned long)fdt32_to_cpu(*(fdt32_t *)prop);
	else if (len == sizeof(u64))
		hartid = (unsigned long)fdt64_to_cpu(
			__get_unaligned_t(fdt64_t, prop));
	else {
		efi_err("Invalid boot-hartid property in FDT\n");
		return EFI_INVALID_PARAMETER;
	}

	return 0;
}

static efi_status_t get_boot_hartid_from_efi(void)
{
	efi_guid_t boot_protocol_guid = RISCV_EFI_BOOT_PROTOCOL_GUID;
	struct riscv_efi_boot_protocol *boot_protocol;
	efi_status_t status;

	status = efi_bs_call(LocateProtocol, &boot_protocol_guid, NULL,
			     (void **)&boot_protocol);
	if (status != EFI_SUCCESS)
		return status;
	return efi_call_proto(boot_protocol, get_boot_hartid, &hartid);
}

efi_status_t check_platform_features(void)
{
	efi_info("Checking platform features...\n");
	efi_status_t status = -1;
	int ret;
	efi_info("Try to get boot hartid from EFI\n");
	status = get_boot_hartid_from_efi();
	if (status != EFI_SUCCESS) {
		efi_info("Try to get boot hartid from FDT\n");
		ret = get_boot_hartid_from_fdt();
		if (ret) {
			efi_err("Failed to get boot hartid!\n");
			return EFI_UNSUPPORTED;
		}
	}

	efi_info("Boot hartid: %ld\n", hartid);
	return EFI_SUCCESS;
}

void __noreturn efi_enter_kernel(struct payload_info *payload_info,
				 unsigned long fdt, unsigned long fdt_size)
{
	unsigned long kernel_entry = payload_info->kernel_entry;
	jump_kernel_func jump_kernel = (jump_kernel_func)kernel_entry;

	/*
	 * Jump to real kernel here with following constraints.
	 * 1. MMU should be disabled.
	 * 2. a0 should contain hartid
	 * 3. a1 should DT address
	 */
	csr_write(CSR_SATP, 0);
	jump_kernel(hartid, fdt);
}