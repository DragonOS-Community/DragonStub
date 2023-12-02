#include "dragonstub/printk.h"
#include "efidef.h"
#include <efi.h>
#include <efilib.h>
#include <elf.h>
#include <dragonstub/dragonstub.h>
#include <dragonstub/elfloader.h>
#include <dragonstub/linux/math.h>
#include <dragonstub/linux/align.h>

/*
 * This is the base address at which to start allocating virtual memory ranges
 * for UEFI Runtime Services.
 *
 * For ARM/ARM64:
 * This is in the low TTBR0 range so that we can use
 * any allocation we choose, and eliminate the risk of a conflict after kexec.
 * The value chosen is the largest non-zero power of 2 suitable for this purpose
 * both on 32-bit and 64-bit ARM CPUs, to maximize the likelihood that it can
 * be mapped efficiently.
 * Since 32-bit ARM could potentially execute with a 1G/3G user/kernel split,
 * map everything below 1 GB. (512 MB is a reasonable upper bound for the
 * entire footprint of the UEFI runtime services memory regions)
 *
 * For RISC-V:
 * There is no specific reason for which, this address (512MB) can't be used
 * EFI runtime virtual address for RISC-V. It also helps to use EFI runtime
 * services on both RV32/RV64. Keep the same runtime virtual address for RISC-V
 * as well to minimize the code churn.
 */
#define EFI_RT_VIRTUAL_BASE SZ_512M

/*
 * Some architectures map the EFI regions into the kernel's linear map using a
 * fixed offset.
 */
#ifndef EFI_RT_VIRTUAL_OFFSET
#define EFI_RT_VIRTUAL_OFFSET 0
#endif

extern void _image_end(void);

static u64 image_base = 0;
static u64 image_size = 0;
static u64 image_end = 0;

static u64 virtmap_base = EFI_RT_VIRTUAL_BASE;
static bool flat_va_mapping = (EFI_RT_VIRTUAL_OFFSET != 0);

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
				     .payload_size = payload_size,
				     .loaded_paddr = 0,
				     .loaded_size = 0,
				     .kernel_entry = 0 };
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

	efi_info("Checking payload's ELF header...\n");
	bool found = elf_check((void *)payload_start, payload_size);

	if (found) {
		info->payload_addr = payload_start;
		info->payload_size = payload_size;
		efi_info("Found payload ELF header\n");
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

/*
 * efi_allocate_virtmap() - create a pool allocation for the virtmap
 *
 * Create an allocation that is of sufficient size to hold all the memory
 * descriptors that will be passed to SetVirtualAddressMap() to inform the
 * firmware about the virtual mapping that will be used under the OS to call
 * into the firmware.
 */
efi_status_t efi_alloc_virtmap(efi_memory_desc_t **virtmap,
			       unsigned long *desc_size, u32 *desc_ver)
{
	unsigned long size, mmap_key;
	efi_status_t status;

	/*
	 * Use the size of the current memory map as an upper bound for the
	 * size of the buffer we need to pass to SetVirtualAddressMap() to
	 * cover all EFI_MEMORY_RUNTIME regions.
	 */
	size = 0;
	status = efi_bs_call(GetMemoryMap, &size, NULL, &mmap_key, desc_size,
			     desc_ver);
	if (status != EFI_BUFFER_TOO_SMALL)
		return EFI_LOAD_ERROR;

	return efi_bs_call(AllocatePool, EfiLoaderData, size, (void **)virtmap);
}

/*
 * efi_get_virtmap() - create a virtual mapping for the EFI memory map
 *
 * This function populates the virt_addr fields of all memory region descriptors
 * in @memory_map whose EFI_MEMORY_RUNTIME attribute is set. Those descriptors
 * are also copied to @runtime_map, and their total count is returned in @count.
 */
void efi_get_virtmap(efi_memory_desc_t *memory_map, unsigned long map_size,
		     unsigned long desc_size, efi_memory_desc_t *runtime_map,
		     int *count)
{
	u64 efi_virt_base = virtmap_base;
	efi_memory_desc_t *in, *out = runtime_map;
	int l;

	*count = 0;

	for (l = 0; l < map_size; l += desc_size) {
		u64 paddr, size;

		in = (void *)memory_map + l;
		if (!(in->Attribute & EFI_MEMORY_RUNTIME))
			continue;

		paddr = in->PhysicalStart;
		size = in->NumberOfPages * EFI_PAGE_SIZE;

		in->VirtualStart = in->PhysicalStart + EFI_RT_VIRTUAL_OFFSET;
		if (efi_novamap) {
			continue;
		}

		/*
		 * Make the mapping compatible with 64k pages: this allows
		 * a 4k page size kernel to kexec a 64k page size kernel and
		 * vice versa.
		 */
		if (!flat_va_mapping) {
			paddr = round_down(in->PhysicalStart, SZ_64K);
			size += in->PhysicalStart - paddr;

			/*
			 * Avoid wasting memory on PTEs by choosing a virtual
			 * base that is compatible with section mappings if this
			 * region has the appropriate size and physical
			 * alignment. (Sections are 2 MB on 4k granule kernels)
			 */
			if (IS_ALIGNED(in->PhysicalStart, SZ_2M) &&
			    size >= SZ_2M)
				efi_virt_base = round_up(efi_virt_base, SZ_2M);
			else
				efi_virt_base = round_up(efi_virt_base, SZ_64K);

			in->VirtualStart += efi_virt_base - paddr;
			efi_virt_base += size;
		}

		memcpy(out, in, desc_size);
		out = (void *)out + desc_size;
		++*count;
	}
}

/// @brief 设置内存保留表
/// @param
static void install_memreserve_table(void)
{
	struct linux_efi_memreserve *rsv;
	efi_guid_t memreserve_table_guid = LINUX_EFI_MEMRESERVE_TABLE_GUID;
	efi_status_t status;

	status = efi_bs_call(AllocatePool, EfiLoaderData, sizeof(*rsv),
			     (void **)&rsv);
	if (status != EFI_SUCCESS) {
		efi_err("Failed to allocate memreserve entry!\n");
		return;
	}

	rsv->next = 0;
	rsv->size = 0;
	rsv->count = 0;

	status = efi_bs_call(InstallConfigurationTable, &memreserve_table_guid,
			     rsv);
	if (status != EFI_SUCCESS)
		efi_err("Failed to install memreserve config table!\n");
}

static u32 get_supported_rt_services(void)
{
	const efi_rt_properties_table_t *rt_prop_table;
	u32 supported = EFI_RT_SUPPORTED_ALL;

	rt_prop_table = get_efi_config_table(EFI_RT_PROPERTIES_TABLE_GUID);
	if (rt_prop_table)
		supported &= rt_prop_table->runtime_services_supported;

	return supported;
}

efi_status_t efi_stub_common(efi_handle_t handle,
			     efi_loaded_image_t *loaded_image,
			     struct payload_info *payload_info,
			     char *cmdline_ptr)
{
	struct screen_info *si;
	efi_status_t status;

	status = check_platform_features();
	if (status != EFI_SUCCESS)
		return status;

	// si = setup_graphics();

	// efi_retrieve_tpm2_eventlog();

	// /* Ask the firmware to clear memory on unclean shutdown */
	// efi_enable_reset_attack_mitigation();

	// efi_load_initrd(image, ULONG_MAX, efi_get_max_initrd_addr(image_addr),
	// 		NULL);

	// efi_random_get_seed();

	/* force efi_novamap if SetVirtualAddressMap() is unsupported */
	efi_novamap |= !(get_supported_rt_services() &
			 EFI_RT_SUPPORTED_SET_VIRTUAL_ADDRESS_MAP);

	install_memreserve_table();
	efi_info("Memreserve table installed\n");
	efi_info("Booting DragonOS kernel...\n");
	status = efi_boot_kernel(handle, loaded_image, payload_info,
				 cmdline_ptr);

	// free_screen_info(si);
	return status;
}
