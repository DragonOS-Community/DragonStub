#include <efi.h>
#include <efilib.h>
#include <dragonstub/dragonstub.h>

EFI_STATUS efi_handle_cmdline(EFI_LOADED_IMAGE *image, char **cmdline_ptr)
{
	int cmdline_size = 0;
	EFI_STATUS status;
	char *cmdline;

	/*
	 * Get the command line from EFI, using the LOADED_IMAGE
	 * protocol. We are going to copy the command line into the
	 * device tree, so this can be allocated anywhere.
	 */
	cmdline = efi_convert_cmdline(image, &cmdline_size);
	if (!cmdline) {
		efi_err("getting command line via LOADED_IMAGE_PROTOCOL\n");
		return EFI_OUT_OF_RESOURCES;
	}

	// 	if (IS_ENABLED(CONFIG_CMDLINE_EXTEND) ||
	// 	    IS_ENABLED(CONFIG_CMDLINE_FORCE) ||
	// 	    cmdline_size == 0) {
	// 		status = efi_parse_options(CONFIG_CMDLINE);
	// 		if (status != EFI_SUCCESS) {
	// 			efi_err("Failed to parse options\n");
	// 			goto fail_free_cmdline;
	// 		}
	// 	}

	// if (!IS_ENABLED(CONFIG_CMDLINE_FORCE) && cmdline_size > 0) {
	if (cmdline_size > 0) {
		status = efi_parse_options(cmdline);
		if (status != EFI_SUCCESS) {
			efi_err("Failed to parse options\n");
			goto fail_free_cmdline;
		}
	}

	*cmdline_ptr = cmdline;
	return EFI_SUCCESS;

fail_free_cmdline:
	efi_bs_call(FreePool, cmdline_ptr);
	return status;
}