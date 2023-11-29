#include <efi.h>
#include <efilib.h>
#include <elf.h>
#include <dragonstub/dragonstub.h>

extern void _image_end(void);

static u64 image_base = 0;
static u64 image_size = 0;
static u64 image_end = 0;

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

static efi_status_t init_efi_program_info(efi_loaded_image_t *loaded_image)
{
	image_base = (u64)loaded_image->ImageBase;
	image_size = loaded_image->ImageSize;
	image_end = (u64)_image_end;
	efi_info("DragonStub loaded at 0x%p\n", image_base);
	efi_info("DragonStub + payload size: 0x%p\n", image_size);
	efi_info("DragonStub end addr: 0x%p\n", image_end);
	return EFI_SUCCESS;
}

/// @brief payload_info的构造函数
static struct payload_info payload_info_new(u64 payload_addr, u64 payload_size)
{
	struct payload_info info = { .payload_addr = payload_addr,
				     .payload_size = payload_size };
	return info;
}
static efi_status_t find_elf(struct payload_info *info)
{
	extern __weak void _binary_payload_start(void);
	extern __weak void _binary_payload_end(void);
	extern __weak void _binary_payload_size(void);
	u64 payload_start = (u64)_binary_payload_start;
	u64 payload_end = (u64)_binary_payload_end;

	u64 payload_size = payload_end - payload_start;

	efi_info("payload_addr: %p\n", payload_start);
	efi_info("payload_end: %p\n", payload_end);
	efi_info("payload_size: %p\n", payload_size);

	if (payload_start == 0 || payload_end <= payload_start + 4 ||
	    payload_size == 0) {
		return EFI_NOT_FOUND;
	}

	efi_debug("Checking payload's ELF header...\n");
	bool found = true;
	for (int i = 0; i < 4; i++) {
		u8 c = *(u8 *)(payload_start + i);
		if (c != ELFMAG[i]) {
			// 不是ELF magic number，跳过
			found = false;
			break;
		}
	}

	// 如果找到了ELF magic number，就认为找到了ELF header,稍后再验证
	if (found) {
		info->payload_addr = payload_start;
		info->payload_size = payload_size;
		return EFI_SUCCESS;
	}

	return EFI_NOT_FOUND;
}

/// @brief 寻找要加载的内核负载
/// @param handle efi_handle
/// @param image efi_loaded_image_t
/// @param ret_info 返回的负载信息
/// @return
efi_status_t find_payload(efi_handle_t handle, efi_loaded_image_t *loaded_image,
			  struct payload_info *ret_info)
{
	efi_info("Try to find payload to boot\n");
	efi_status_t status = init_efi_program_info(loaded_image);
	if (status != EFI_SUCCESS) {
		efi_err("Failed to init efi program info\n");
		return status;
	}

	struct payload_info info = payload_info_new(0, 0);

	status = find_elf(&info);
	if (status != EFI_SUCCESS) {
		efi_err("Payload not found: Did you forget to add the payload by setting PAYLOAD_ELF at compile time?\n"
			"Or the payload is not an ELF file?\n");
		return status;
	}

    *ret_info = info;
    return EFI_SUCCESS;
}
