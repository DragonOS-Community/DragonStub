#include "dragonstub/riscv64.h"
#include <efi.h>
#include <efilib.h>
#include <dragonstub/printk.h>
#include <dragonstub/dragonstub.h>

void print_dragonstub_banner(void);

EFI_STATUS
efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *systab)
{
	EFI_STATUS status;
	EFI_LOADED_IMAGE *loaded_image = NULL;
	/* addr/point and size pairs for memory management*/
	char *cmdline_ptr = NULL;

	print_dragonstub_banner();

	efi_info("EFI env initialized\n");

	/*
	 * Get a handle to the loaded image protocol.  This is used to get
	 * information about the running image, such as size and the command
	 * line.
	 */
	status = efi_bs_call(HandleProtocol, image_handle, &LoadedImageProtocol,
			     (void *)&loaded_image);

	if (EFI_ERROR(status)) {
		efi_err("Could not open loaded image protocol: %d\n", status);
		return status;
	}

	efi_info("Loaded image protocol opened\n");

	status = efi_handle_cmdline(loaded_image, &cmdline_ptr);

	if (EFI_ERROR(status)) {
		efi_err("Could not get command line: %d\n", status);
		return status;
	}
	if (cmdline_ptr == NULL)
		efi_warn("Command line is NULL\n");
	else
		efi_info("Command line: %s\n", cmdline_ptr);

	struct payload_info payload;
	status = find_payload(image_handle, loaded_image, &payload);
	if (EFI_ERROR(status)) {
		efi_err("Could not find payload, efi error code: %d\n", status);
		return status;
	}
	efi_info("Booting DragonOS kernel...\n");
	efi_stub_common(image_handle, loaded_image, &payload, cmdline_ptr);
	efi_todo("Boot DragonOS kernel");

	return EFI_SUCCESS;
}

/// @brief Print thr DragonStub banner
void print_dragonstub_banner(void)
{
	efi_printk(
		" ____                              ____  _         _     \n");
	efi_printk(
		"|  _ \\ _ __ __ _  __ _  ___  _ __ / ___|| |_ _   _| |__  \n");
	efi_printk(
		"| | | | '__/ _` |/ _` |/ _ \\| '_ \\\\___ \\| __| | | | '_ \\ \n");
	efi_printk(
		"| |_| | | | (_| | (_| | (_) | | | |___) | |_| |_| | |_) |\n");
	efi_printk(
		"|____/|_|  \\__,_|\\__, |\\___/|_| |_|____/ \\__|\\__,_|_.__/ \n");
	efi_printk(
		"                 |___/                                   \n");

	efi_printk("\n@Copyright 2022-2023 DragonOS Community.\n");
	efi_printk(
		"\nDragonStub official repo: https://github.com/DragonOS-Community/DragonStub\n");
	efi_printk("\nDragonStub is licensed under GPLv2\n\n");
}
